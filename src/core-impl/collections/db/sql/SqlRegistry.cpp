/****************************************************************************************
 * Copyright (c) 2007 Maximilian Kossick <maximilian.kossick@googlemail.com>            *
 * Copyright (c) 2010 Ralf Engels <ralf-engels@gmx.de>                                  *
 *                                                                                      *
 * This program is free software; you can redistribute it and/or modify it under        *
 * the terms of the GNU General Public License as published by the Free Software        *
 * Foundation; either version 2 of the License, or (at your option) any later           *
 * version.                                                                             *
 *                                                                                      *
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY      *
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A      *
 * PARTICULAR PURPOSE. See the GNU General Public License for more details.             *
 *                                                                                      *
 * You should have received a copy of the GNU General Public License along with         *
 * this program.  If not, see <http://www.gnu.org/licenses/>.                           *
 ****************************************************************************************/

#define DEBUG_PREFIX "SqlRegistry"
#include "core/support/Debug.h"

#include "SqlRegistry.h"
#include "SqlRegistry_p.h"
#include "SqlCollection.h"
#include "../ScanManager.h"

#include <QMutableHashIterator>
#include <QMutexLocker>

SqlRegistry::SqlRegistry( Collections::SqlCollection* collection )
    : QObject( 0 )
    , m_collection( collection )
    , m_blockDatabaseUpdateCount( 0 )
    , m_collectionChanged( false )
{
    setObjectName( "SqlRegistry" );

    // -- remove unneeded entries from the database.
    // we have to do this now before anyone can hold references
    // to those objects.
    DatabaseUpdater databaseUpdater( m_collection );
    databaseUpdater.deleteAllRedundant( "album" ); // what about cover images in database and disk cache?
    databaseUpdater.deleteAllRedundant( "artist" );
    databaseUpdater.deleteAllRedundant( "genre" );
    databaseUpdater.deleteAllRedundant( "composer" );
    databaseUpdater.deleteAllRedundant( "url" );
    databaseUpdater.deleteAllRedundant( "year" );

    m_timer = new QTimer( this );
    m_timer->setInterval( 30 * 1000 );  //try to clean up every 30 seconds, change if necessary
    m_timer->setSingleShot( false );
    connect( m_timer, SIGNAL( timeout() ), this, SLOT( emptyCache() ) );
    m_timer->start();
}

SqlRegistry::~SqlRegistry()
{
    //don't delete m_collection
}

// ------ directory
int
SqlRegistry::getDirectory( const QString &path, uint mtime )
{
    int dirId;
    int deviceId = m_collection->mountPointManager()->getIdForUrl( path );
    QString rdir = m_collection->mountPointManager()->getRelativePath( deviceId, path );

    SqlStorage *storage = m_collection->sqlStorage();

    // - find existing entry
    QString query = QString( "SELECT id, changedate FROM directories "
                             "WHERE  deviceid = %1 AND dir = '%2';" )
                        .arg( QString::number( deviceId ), storage->escape( rdir ) );
    QStringList res = storage->query( query );

    // - create new entry
    if( res.isEmpty() )
    {
        debug() << "SqlRegistry::getDirectory new Directory" << path;
        QString insert = QString( "INSERT INTO directories(deviceid,changedate,dir) "
                                  "VALUES (%1,%2,'%3');" )
                        .arg( QString::number( deviceId ), QString::number( mtime ),
                                storage->escape( rdir ) );
        dirId = storage->insert( insert, "directories" );
        m_collectionChanged = true;
    }
    else
    {
        // update old one
        dirId = res[0].toUInt();
        uint oldMtime = res[1].toUInt();
        if( oldMtime != mtime )
        {
            QString update = QString( "UPDATE directories SET changedate = %1 "
                                      "WHERE id = %2;" )
                .arg( QString::number( mtime ), res[0] );
        debug() << "SqlRegistry::getDirectory update Directory"<<res[0]<<"from" << oldMtime << "to" << mtime;
        debug() << update;
            storage->query( update );
        }
    }
    return dirId;
}

// ------ track

Meta::TrackPtr
SqlRegistry::getTrack( int urlId )
{
    QMutexLocker locker( &m_trackMutex );

    QString query = "SELECT %1 FROM urls %2 "
        "WHERE urls.id = %3";
    query = query.arg( Meta::SqlTrack::getTrackReturnValues(),
                       Meta::SqlTrack::getTrackJoinConditions(),
                       QString::number( urlId ) );
    QStringList rowData = m_collection->sqlStorage()->query( query );
    if( rowData.isEmpty() )
        return Meta::TrackPtr();

    TrackPath id( rowData[Meta::SqlTrack::returnIndex_urlDeviceId].toInt(),
                rowData[Meta::SqlTrack::returnIndex_urlRPath] );
    QString uid = rowData[Meta::SqlTrack::returnIndex_urlUid];

    if( m_trackMap.contains( id ) )
        return m_trackMap.value( id );
    else if( m_uidMap.contains( uid ) )
        return m_uidMap.value( uid );
    else
    {
        Meta::SqlTrack *sqlTrack = new Meta::SqlTrack( m_collection, rowData );
        Meta::TrackPtr trackPtr( sqlTrack );

        m_trackMap.insert( id, trackPtr );
        m_uidMap.insert( sqlTrack->uidUrl(), trackPtr );
        return trackPtr;
    }
}

Meta::TrackPtr
SqlRegistry::getTrack( const QString &path )
{
    int deviceId = m_collection->mountPointManager()->getIdForUrl( path );
    QString rpath = m_collection->mountPointManager()->getRelativePath( deviceId, path );

    TrackPath id(deviceId, rpath);
    if( m_trackMap.contains( id ) )
        return m_trackMap.value( id );
    else
    {
        QString query;
        QStringList result;

        query = "SELECT %1 FROM urls %2 "
            "WHERE urls.deviceid = %3 AND urls.rpath = '%4';";
        query = query.arg( Meta::SqlTrack::getTrackReturnValues(),
                           Meta::SqlTrack::getTrackJoinConditions(),
                           QString::number( deviceId ),
                           m_collection->sqlStorage()->escape( rpath ) );
        result = m_collection->sqlStorage()->query( query );
        if( result.isEmpty() )
            return Meta::TrackPtr();

        Meta::SqlTrack *sqlTrack = new Meta::SqlTrack( m_collection, result );

        Meta::TrackPtr trackPtr( sqlTrack );
        m_trackMap.insert( id, trackPtr );
        m_uidMap.insert( sqlTrack->uidUrl(), trackPtr );
        return trackPtr;
    }
}


Meta::TrackPtr
SqlRegistry::getTrack( int deviceId, const QString &rpath, int directoryId, const QString &uidUrl )
{
    TrackPath id(deviceId, rpath);
    QMutexLocker locker( &m_trackMutex );
    if( m_trackMap.contains( id ) )
        return m_trackMap.value( id );
    else
    {
        QString query;
        QStringList result;
        Meta::SqlTrack *sqlTrack = 0;

        // -- get it from the database
        query = "SELECT %1 FROM urls %2 "
            "WHERE urls.deviceid = %3 AND urls.rpath = '%4';";
        query = query.arg( Meta::SqlTrack::getTrackReturnValues(),
                           Meta::SqlTrack::getTrackJoinConditions(),
                           QString::number( deviceId ),
                           m_collection->sqlStorage()->escape( rpath ) );
        result = m_collection->sqlStorage()->query( query );

        if( !result.isEmpty() )
            sqlTrack = new Meta::SqlTrack( m_collection, result );

        // -- we have to create a new track
        if( !sqlTrack )
            sqlTrack = new Meta::SqlTrack( m_collection, deviceId, rpath, directoryId, uidUrl );

        Meta::TrackPtr trackPtr( sqlTrack );
        m_trackMap.insert( id, trackPtr );
        m_uidMap.insert( sqlTrack->uidUrl(), trackPtr );
        return trackPtr;
    }
}

Meta::TrackPtr
SqlRegistry::getTrack( int trackId, const QStringList &rowData )
{
    Q_ASSERT( trackId == rowData[Meta::SqlTrack::returnIndex_trackId].toInt() );
    Q_UNUSED( trackId );

    TrackPath path( rowData[Meta::SqlTrack::returnIndex_urlDeviceId].toInt(),
                    rowData[Meta::SqlTrack::returnIndex_urlRPath] );
    QString uid = rowData[Meta::SqlTrack::returnIndex_urlUid];

    QMutexLocker locker( &m_trackMutex );
    if( m_trackMap.contains( path ) )
        return m_trackMap.value( path );
    else if( m_uidMap.contains( uid ) )
        return m_uidMap.value( uid );
    else
    {
        Meta::SqlTrack *sqlTrack =  new Meta::SqlTrack( m_collection, rowData );
        Meta::TrackPtr track( sqlTrack );

        m_trackMap.insert( path, track );
        m_uidMap.insert( KSharedPtr<Meta::SqlTrack>::staticCast( track )->uidUrl(), track );
        return track;
    }
}

bool
SqlRegistry::updateCachedUrl( const QString &oldUrl, const QString &newUrl )
{
    QMutexLocker locker( &m_trackMutex );
    int deviceId = m_collection->mountPointManager()->getIdForUrl( oldUrl );
    QString rpath = m_collection->mountPointManager()->getRelativePath( deviceId, oldUrl );
    TrackPath oldId(deviceId, rpath);

    int newdeviceId = m_collection->mountPointManager()->getIdForUrl( newUrl );
    QString newRpath = m_collection->mountPointManager()->getRelativePath( newdeviceId, newUrl );
    TrackPath newId( newdeviceId, newRpath );

    if( m_trackMap.contains( newId ) )
        warning() << "updating path to an already existing path.";
    else if( !m_trackMap.contains( oldId ) )
        warning() << "updating path from a non existing path.";
    else
    {
        Meta::TrackPtr track = m_trackMap.take( oldId );
        m_trackMap.insert( newId, track );
        return true;
    }
    return false;
}

bool
SqlRegistry::updateCachedUid( const QString &oldUid, const QString &newUid )
{
    QMutexLocker locker( &m_trackMutex );
    // TODO: improve uid handling
    if( m_uidMap.contains( newUid ) )
        warning() << "updating uid to an already existing uid.";
    else if( !oldUid.isEmpty() && !m_uidMap.contains( oldUid ) )
        warning() << "updating uid from a non existing uid.";
    else
    {
        Meta::TrackPtr track = m_uidMap.take(oldUid);
        m_uidMap.insert( newUid, track );
        return true;
    }
    return false;
}

Meta::TrackPtr
SqlRegistry::getTrackFromUid( const QString &uid )
{
    QMutexLocker locker( &m_trackMutex );
    if( m_uidMap.contains( uid ) )
        return m_uidMap.value( uid );
    {
        QString query;
        QStringList result;

        // -- get all the track info
        query = "SELECT %1 FROM urls %2 "
            "WHERE urls.uniqueid = '%3';";
        query = query.arg( Meta::SqlTrack::getTrackReturnValues(),
                           Meta::SqlTrack::getTrackJoinConditions(),
                           m_collection->sqlStorage()->escape( uid ) );
        result = m_collection->sqlStorage()->query( query );
        if( result.isEmpty() )
            return Meta::TrackPtr();

        Meta::SqlTrack *sqlTrack = new Meta::SqlTrack( m_collection, result );
        Meta::TrackPtr trackPtr( sqlTrack );

        int deviceid = m_collection->mountPointManager()->getIdForUrl( trackPtr->playableUrl().path() );
        QString rpath = m_collection->mountPointManager()->getRelativePath( deviceid, trackPtr->playableUrl().path() );
        TrackPath id(deviceid, rpath);
        m_trackMap.insert( id, trackPtr );
        m_uidMap.insert( uid, trackPtr );
        return trackPtr;
    }
}

void
SqlRegistry::removeTrack( int urlId, const QString uid )
{
    QMutexLocker locker( &m_trackMutex );

    // --- delete the track from database
    QString query = QString( "DELETE FROM tracks where url=%1;" ).arg( urlId );
    m_collection->sqlStorage()->query( query );
    // keep the urls and statistics entry in case a deleted track is restored later.
    // however we need to change rpath so that it does not block new tracks
    // (deviceid,rpath is a unique key)
    // so we just write the uid into the path
    query = QString( "UPDATE urls SET deviceid=0, rpath='%1', directory=NULL "
                     "WHERE id=%2;")
        .arg( m_collection->sqlStorage()->escape(uid) )
        .arg( urlId );
    m_collection->sqlStorage()->query( query );
    query = QString( "UPDATE statistics SET deleted=1 WHERE url=%1;").arg( urlId );
    m_collection->sqlStorage()->query( query );

    // --- delete the track from memory
    if( m_uidMap.contains( uid ) )
    {
        // -- remove from hashes
        Meta::TrackPtr track = m_uidMap.take( uid );
        Meta::SqlTrack *sqlTrack = static_cast<Meta::SqlTrack*>( track.data() );

        int deviceId = m_collection->mountPointManager()->getIdForUrl( sqlTrack->playableUrl().path() );
        QString rpath = m_collection->mountPointManager()->getRelativePath( deviceId, sqlTrack->playableUrl().path() );
        TrackPath id(deviceId, rpath);
        m_trackMap.remove( id );
    }
}

// -------- artist

Meta::ArtistPtr
SqlRegistry::getArtist( const QString &name )
{
    QMutexLocker locker( &m_artistMutex );

    if( m_artistMap.contains( name ) )
        return m_artistMap.value( name );

    int id;

    QString query = QString( "SELECT id FROM artists WHERE name = '%1';" ).arg( m_collection->sqlStorage()->escape( name ) );
    QStringList res = m_collection->sqlStorage()->query( query );
    if( res.isEmpty() )
    {
        QString insert = QString( "INSERT INTO artists( name ) VALUES ('%1');" ).arg( m_collection->sqlStorage()->escape( name ) );
        id = m_collection->sqlStorage()->insert( insert, "artists" );
        m_collectionChanged = true;
    }
    else
    {
        id = res[0].toInt();
    }

    Meta::ArtistPtr artist( new Meta::SqlArtist( m_collection, id, name ) );
    m_artistMap.insert( name, artist );
    m_artistIdMap.insert( id, artist );
    return artist;
}

Meta::ArtistPtr
SqlRegistry::getArtist( int id )
{
    QMutexLocker locker( &m_artistMutex );

    if( m_artistIdMap.contains( id ) )
        return m_artistIdMap.value( id );

    QString query = QString( "SELECT name FROM artists WHERE id = %1;" ).arg( id );
    QStringList res = m_collection->sqlStorage()->query( query );
    if( res.isEmpty() )
        return Meta::ArtistPtr();

    QString name = res[0];
    Meta::ArtistPtr artist( new Meta::SqlArtist( m_collection, id, name ) );
    m_artistMap.insert( name, artist );
    m_artistIdMap.insert( id, artist );
    return artist;
}

Meta::ArtistPtr
SqlRegistry::getArtist( int id, const QString &name )
{
    Q_ASSERT( id > 0 ); // must be a valid id
    QMutexLocker locker( &m_artistMutex );

    if( m_artistMap.contains( name ) )
        return m_artistMap.value( name );

    Meta::ArtistPtr artist( new Meta::SqlArtist( m_collection, id, name ) );
    m_artistMap.insert( name, artist );
    m_artistIdMap.insert( id, artist );
    return artist;
}

// -------- genre

Meta::GenrePtr
SqlRegistry::getGenre( const QString &name )
{
    QMutexLocker locker( &m_genreMutex );

    if( m_genreMap.contains( name ) )
        return m_genreMap.value( name );

    int id;

    QString query = QString( "SELECT id FROM genres WHERE name = '%1';" ).arg( m_collection->sqlStorage()->escape( name ) );
    QStringList res = m_collection->sqlStorage()->query( query );
    if( res.isEmpty() )
    {
        QString insert = QString( "INSERT INTO genres( name ) VALUES ('%1');" ).arg( m_collection->sqlStorage()->escape( name ) );
        id = m_collection->sqlStorage()->insert( insert, "genres" );
        m_collectionChanged = true;
    }
    else
    {
        id = res[0].toInt();
    }

    Meta::GenrePtr genre( new Meta::SqlGenre( m_collection, id, name ) );
    m_genreMap.insert( name, genre );
    return genre;
}

Meta::GenrePtr
SqlRegistry::getGenre( int id )
{
    QMutexLocker locker( &m_genreMutex );

    QString query = QString( "SELECT name FROM genres WHERE id = '%1';" ).arg( id );
    QStringList res = m_collection->sqlStorage()->query( query );
    if( res.isEmpty() )
        return Meta::GenrePtr();

    QString name = res[0];
    Meta::GenrePtr genre( new Meta::SqlGenre( m_collection, id, name ) );
    m_genreMap.insert( name, genre );
    return genre;
}

Meta::GenrePtr
SqlRegistry::getGenre( int id, const QString &name )
{
    Q_ASSERT( id > 0 ); // must be a valid id
    QMutexLocker locker( &m_genreMutex );

    if( m_genreMap.contains( name ) )
        return m_genreMap.value( name );

    Meta::GenrePtr genre( new Meta::SqlGenre( m_collection, id, name ) );
    m_genreMap.insert( name, genre );
    return genre;
}

// -------- composer

Meta::ComposerPtr
SqlRegistry::getComposer( const QString &name )
{
    QMutexLocker locker( &m_composerMutex );

    if( m_composerMap.contains( name ) )
        return m_composerMap.value( name );

    int id;

    QString query = QString( "SELECT id FROM composers WHERE name = '%1';" ).arg( m_collection->sqlStorage()->escape( name ) );
    QStringList res = m_collection->sqlStorage()->query( query );
    if( res.isEmpty() )
    {
        QString insert = QString( "INSERT INTO composers( name ) VALUES ('%1');" ).arg( m_collection->sqlStorage()->escape( name ) );
        id = m_collection->sqlStorage()->insert( insert, "composers" );
        m_collectionChanged = true;
    }
    else
    {
        id = res[0].toInt();
    }

    Meta::ComposerPtr composer( new Meta::SqlComposer( m_collection, id, name ) );
    m_composerMap.insert( name, composer );
    return composer;
}

Meta::ComposerPtr
SqlRegistry::getComposer( int id )
{
    if( id <= 0 )
        return Meta::ComposerPtr();

    QMutexLocker locker( &m_composerMutex );

    QString query = QString( "SELECT name FROM composers WHERE id = '%1';" ).arg( id );
    QStringList res = m_collection->sqlStorage()->query( query );
    if( res.isEmpty() )
        return Meta::ComposerPtr();

    QString name = res[0];
    Meta::ComposerPtr composer( new Meta::SqlComposer( m_collection, id, name ) );
    m_composerMap.insert( name, composer );
    return composer;
}

Meta::ComposerPtr
SqlRegistry::getComposer( int id, const QString &name )
{
    Q_ASSERT( id > 0 ); // must be a valid id
    QMutexLocker locker( &m_composerMutex );

    if( m_composerMap.contains( name ) )
        return m_composerMap.value( name );

    Meta::ComposerPtr composer( new Meta::SqlComposer( m_collection, id, name ) );
    m_composerMap.insert( name, composer );
    return composer;
}

// -------- year

Meta::YearPtr
SqlRegistry::getYear( int year, int yearId )
{
    QMutexLocker locker( &m_yearMutex );

    if( m_yearMap.contains( year ) )
        return m_yearMap.value( year );

    // don't know the id yet
    if( yearId <= 0 )
    {
        QString query = QString( "SELECT id FROM years WHERE name = '%1';" ).arg( QString::number( year ) );
        QStringList res = m_collection->sqlStorage()->query( query );
        if( res.isEmpty() )
        {
            QString insert = QString( "INSERT INTO years( name ) VALUES ('%1');" ).arg( QString::number( year ) );
            yearId = m_collection->sqlStorage()->insert( insert, "years" );
            m_collectionChanged = true;
        }
        else
        {
            yearId = res[0].toInt();
        }
    }
    Meta::YearPtr yearPtr( new Meta::SqlYear( m_collection, yearId, year ) );
    m_yearMap.insert( year, yearPtr );
    return yearPtr;
}

// -------- album

Meta::AlbumPtr
SqlRegistry::getAlbum( const QString &name, const QString &artist )
{
    QString albumArtist( artist );
    if( name.isEmpty() ) // the empty album is special. all singles are collected here
        albumArtist.clear();

    QMutexLocker locker( &m_albumMutex );

    AlbumKey key(name, albumArtist);
    if( m_albumMap.contains( key ) )
        return m_albumMap.value( key );

    int albumId = -1;
    int artistId = -1;

    QString query = QString( "SELECT id FROM albums WHERE name = '%1' AND " ).arg( m_collection->sqlStorage()->escape( name ) );

    if( albumArtist.isEmpty() )
    {
        query += QString( "artist IS NULL" );
    }
    else
    {
        Meta::ArtistPtr artistPtr = getArtist( albumArtist );
        Meta::SqlArtist *sqlArtist = static_cast<Meta::SqlArtist*>(artistPtr.data());
        artistId = sqlArtist->id();

        query += QString( "artist=%1" ).arg( artistId );
    }

    QStringList res = m_collection->sqlStorage()->query( query );
    if( res.isEmpty() )
    {
        // ok. have to create a new album
        QString insert = QString( "INSERT INTO albums( name, artist ) VALUES ('%1',%2);" ).
            arg( m_collection->sqlStorage()->escape( name ),
                 artistId > 0 ? QString::number( artistId ) : "NULL" );
        albumId = m_collection->sqlStorage()->insert( insert, "albums" );
        m_collectionChanged = true; // we just added a new album
    }
    else
    {
        albumId = res[0].toInt();
    }

    Meta::SqlAlbum *sqlAlbum = new Meta::SqlAlbum( m_collection, albumId, name, artistId );
    Meta::AlbumPtr album( sqlAlbum );
    m_albumMap.insert( key, album );
    m_albumIdMap.insert( albumId, album );
    locker.unlock(); // prevent deadlock
    return album;
}

Meta::AlbumPtr
SqlRegistry::getAlbum( int albumId )
{
    Q_ASSERT( albumId > 0 ); // must be a valid id

    if( m_albumIdMap.contains( albumId ) )
        return m_albumIdMap.value( albumId );

    QString query = QString( "SELECT name, artist FROM albums WHERE id = %1" ).arg( albumId );
    QStringList res = m_collection->sqlStorage()->query( query );
    if( res.isEmpty() )
        return Meta::AlbumPtr(); // someone messed up

    QString name = res[0];
    int artistId = res[1].toInt();
    return getAlbum( albumId, name, artistId );
}

Meta::AlbumPtr
SqlRegistry::getAlbum( int albumId, const QString &name, int artistId )
{
    Q_ASSERT( albumId > 0 ); // must be a valid id
    QMutexLocker locker( &m_albumMutex );

    if( m_albumIdMap.contains( albumId ) )
        return m_albumIdMap.value( albumId );

    Meta::ArtistPtr artist = getArtist( artistId );
    AlbumKey key(name, artist ? artist->name() : QString() );
    if( m_albumMap.contains( key ) )
        return m_albumMap.value( key );

    Meta::SqlAlbum *sqlAlbum = new Meta::SqlAlbum( m_collection, albumId, name, artistId );
    Meta::AlbumPtr album( sqlAlbum );
    m_albumMap.insert( key, album );
    m_albumIdMap.insert( albumId, album );
    return album;
}

// ------------ label

Meta::LabelPtr
SqlRegistry::getLabel( const QString &label )
{
    QMutexLocker locker( &m_labelMutex );

    if( m_labelMap.contains( label ) )
        return m_labelMap.value( label );

    int id;

    QString query = QString( "SELECT id FROM labels WHERE label = '%1';" ).arg( m_collection->sqlStorage()->escape( label ) );
    QStringList res = m_collection->sqlStorage()->query( query );
    if( res.isEmpty() )
    {
        QString insert = QString( "INSERT INTO labels( label ) VALUES ('%1');" ).arg( m_collection->sqlStorage()->escape( label ) );
        id = m_collection->sqlStorage()->insert( insert, "albums" );
    }
    else
    {
        id = res[0].toInt();
    }

    Meta::LabelPtr labelPtr( new Meta::SqlLabel( m_collection, id, label ) );
    m_labelMap.insert( label, labelPtr );
    return labelPtr;
}

Meta::LabelPtr
SqlRegistry::getLabel( int id )
{
    Q_ASSERT( id > 0 ); // must be a valid id
    QMutexLocker locker( &m_labelMutex );

    QString query = QString( "SELECT label FROM labels WHERE id = '%1';" ).arg( id );
    QStringList res = m_collection->sqlStorage()->query( query );
    if( res.isEmpty() )
        return Meta::LabelPtr();

    QString label = res[0];
    Meta::LabelPtr labelPtr( new Meta::SqlLabel( m_collection, id, label ) );
    m_labelMap.insert( label, labelPtr );
    return labelPtr;
}

Meta::LabelPtr
SqlRegistry::getLabel( int id, const QString &label )
{
    Q_ASSERT( id > 0 ); // must be a valid id
    QMutexLocker locker( &m_labelMutex );

    if( m_labelMap.contains( label ) )
        return m_labelMap.value( label );

    Meta::LabelPtr labelPtr( new Meta::SqlLabel( m_collection, id, label ) );
    m_labelMap.insert( label, labelPtr );
    return labelPtr;
}



// ---------------- generic database management --------------

void
SqlRegistry::blockDatabaseUpdate()
{
    QMutexLocker locker( &m_blockMutex );
    m_blockDatabaseUpdateCount ++;
}

void
SqlRegistry::unblockDatabaseUpdate()
{
    {
        QMutexLocker locker( &m_blockMutex );
        Q_ASSERT( m_blockDatabaseUpdateCount > 0 );
        m_blockDatabaseUpdateCount --;
    }

    // update the database
    commitDirtyTracks();
}

void
SqlRegistry::commitDirtyTracks()
{
    QMutexLocker locker( &m_blockMutex );

    if( m_blockDatabaseUpdateCount > 0 )
        return;

    QList< Meta::SqlYearPtr > dirtyYears = m_dirtyYears.toList();
    QList< Meta::SqlGenrePtr > dirtyGenres = m_dirtyGenres.toList();
    QList< Meta::SqlAlbumPtr > dirtyAlbums = m_dirtyAlbums.toList();
    QList< Meta::SqlTrackPtr > dirtyTracks = m_dirtyTracks.toList();
    QList< Meta::SqlArtistPtr > dirtyArtists = m_dirtyArtists.toList();
    QList< Meta::SqlComposerPtr > dirtyComposers = m_dirtyComposers.toList();

    m_dirtyYears.clear();
    m_dirtyGenres.clear();
    m_dirtyAlbums.clear();
    m_dirtyTracks.clear();
    m_dirtyArtists.clear();
    m_dirtyComposers.clear();
    locker.unlock(); // need to unlock before notifying the observers

    // -- commit all the dirty tracks
    TrackUrlsTableCommitter().commit( dirtyTracks );
    TrackTracksTableCommitter().commit( dirtyTracks );
    TrackStatisticsTableCommitter().commit( dirtyTracks );

    // -- notify all observers
    foreach( Meta::SqlYearPtr year, dirtyYears )
    {
        year->invalidateCache();
        year->notifyObservers();
    }
    foreach( Meta::SqlGenrePtr genre, dirtyGenres )
    {
        genre->invalidateCache();
        genre->notifyObservers();
    }
    foreach( Meta::SqlAlbumPtr album, dirtyAlbums )
    {
        album->invalidateCache();
        album->notifyObservers();
    }
    foreach( Meta::SqlTrackPtr track, dirtyTracks )
    {
        track->notifyObservers();
    }
    foreach( Meta::SqlArtistPtr artist, dirtyArtists )
    {
        artist->invalidateCache();
        artist->notifyObservers();
    }
    foreach( Meta::SqlComposerPtr composer, dirtyComposers )
    {
        composer->invalidateCache();
        composer->notifyObservers();
    }
    if( m_collectionChanged )
        m_collection->collectionUpdated();
    m_collectionChanged = false;
}



void
SqlRegistry::emptyCache()
{
    if( m_collection->scanManager() && m_collection->scanManager()->isRunning() )
        return; // don't clean the cache if a scan is done

    bool hasTrack, hasAlbum, hasArtist, hasYear, hasGenre, hasComposer, hasLabel;
    hasTrack = hasAlbum = hasArtist = hasYear = hasGenre = hasComposer = hasLabel = false;

    //try to avoid possible deadlocks by aborting when we can't get all locks
    if ( ( hasTrack = m_trackMutex.tryLock() )
         && ( hasAlbum = m_albumMutex.tryLock() )
         && ( hasArtist = m_artistMutex.tryLock() )
         && ( hasYear = m_yearMutex.tryLock() )
         && ( hasGenre = m_genreMutex.tryLock() )
         && ( hasComposer = m_composerMutex.tryLock() )
         && ( hasLabel = m_labelMutex.tryLock() ) )
    {
        DEBUG_BLOCK
        #define mapCached( Cache, Key, Map, Res ) \
        Cache[Key] = qMakePair( Map.count(), Res.join(QLatin1String(" ")).toInt() );

        QMap<QString, QPair<int, int> > cachedBefore;

        QString query = QString( "SELECT COUNT(*) FROM albums;" );
        QStringList res = m_collection->sqlStorage()->query( query );
        mapCached( cachedBefore, "albums", m_albumMap, res );

        query = QString( "SELECT COUNT(*) FROM tracks;" );
        res = m_collection->sqlStorage()->query( query );
        mapCached( cachedBefore, "tracks", m_trackMap, res );

        query = QString( "SELECT COUNT(*) FROM artists;" );
        res = m_collection->sqlStorage()->query( query );
        mapCached( cachedBefore, "artists", m_artistMap, res );

        query = QString( "SELECT COUNT(*) FROM genres;" );
        res = m_collection->sqlStorage()->query( query );
        mapCached( cachedBefore, "genres", m_genreMap, res );

        //this very simple garbage collector doesn't handle cyclic object graphs
        //so care has to be taken to make sure that we are not dealing with a cyclic graph
        //by invalidating the tracks cache on all objects
        #define foreachInvalidateCache( Key, Type, RealType, x ) \
        for( QMutableHashIterator<Key,Type > iter(x); iter.hasNext(); ) \
            RealType::staticCast( iter.next().value() )->invalidateCache()

        foreachInvalidateCache( AlbumKey, Meta::AlbumPtr, KSharedPtr<Meta::SqlAlbum>, m_albumMap );
        foreachInvalidateCache( QString, Meta::ArtistPtr, KSharedPtr<Meta::SqlArtist>, m_artistMap );
        foreachInvalidateCache( QString, Meta::GenrePtr, KSharedPtr<Meta::SqlGenre>, m_genreMap );
        foreachInvalidateCache( QString, Meta::ComposerPtr, KSharedPtr<Meta::SqlComposer>, m_composerMap );
        foreachInvalidateCache( int, Meta::YearPtr, KSharedPtr<Meta::SqlYear>, m_yearMap );
        foreachInvalidateCache( QString, Meta::LabelPtr, KSharedPtr<Meta::SqlLabel>, m_labelMap );
        #undef foreachInvalidateCache

        // elem.count() == 2 is correct because elem is one pointer to the object
        // and the other is stored in the hash map (except for m_trackMap, m_albumMap
        // and m_artistMap , where another refence is stored in m_uidMap, m_albumIdMap
        // and m_artistIdMap
        #define foreachCollectGarbage( Key, Type, RefCount, x ) \
        for( QMutableHashIterator<Key,Type > iter(x); iter.hasNext(); ) \
        { \
            Type elem = iter.next().value(); \
            if( elem.count() == RefCount ) \
                iter.remove(); \
        }

        foreachCollectGarbage( TrackPath, Meta::TrackPtr, 3, m_trackMap );
        foreachCollectGarbage( QString, Meta::TrackPtr, 2, m_uidMap );
        // run before artist so that album artist pointers can be garbage collected
        foreachCollectGarbage( AlbumKey, Meta::AlbumPtr, 3, m_albumMap );
        foreachCollectGarbage( int, Meta::AlbumPtr, 2, m_albumIdMap );
        foreachCollectGarbage( QString, Meta::ArtistPtr, 3, m_artistMap );
        foreachCollectGarbage( int, Meta::ArtistPtr, 2, m_artistIdMap );
        foreachCollectGarbage( QString, Meta::GenrePtr, 2, m_genreMap );
        foreachCollectGarbage( QString, Meta::ComposerPtr, 2, m_composerMap );
        foreachCollectGarbage( int, Meta::YearPtr, 2, m_yearMap );
        foreachCollectGarbage( QString, Meta::LabelPtr, 2, m_labelMap );
        #undef foreachCollectGarbage

        QMap<QString, QPair<int, int> > cachedAfter;

        query = QString( "SELECT COUNT(*) FROM albums;" );
        res = m_collection->sqlStorage()->query( query );
        mapCached( cachedAfter, "albums", m_albumMap, res );

        query = QString( "SELECT COUNT(*) FROM tracks;" );
        res = m_collection->sqlStorage()->query( query );
        mapCached( cachedAfter, "tracks", m_trackMap, res );

        query = QString( "SELECT COUNT(*) FROM artists;" );
        res = m_collection->sqlStorage()->query( query );
        mapCached( cachedAfter, "artists", m_artistMap, res );

        query = QString( "SELECT COUNT(*) FROM genres;" );
        res = m_collection->sqlStorage()->query( query );
        mapCached( cachedAfter, "genres", m_genreMap, res );
        #undef mapCached

        if( cachedBefore != cachedAfter )
        {
            QMapIterator<QString, QPair<int, int> > i(cachedAfter), iLast(cachedBefore);
            while( i.hasNext() && iLast.hasNext() )
            {
                i.next();
                iLast.next();
                int count = i.value().first;
                int total = i.value().second;
                QString diff = QString::number( count - iLast.value().first );
                QString text = QString( "%1 (%2) of %3 cached" ).arg( count ).arg( diff ).arg( total );
                debug() << QString( "%1: %2" ).arg( i.key(), 8 ).arg( text ).toLocal8Bit().constData();
            }
        }
        else
            debug() << "Cache unchanged";
    }

    //make sure to unlock all necessary locks
    //important: calling unlock() on an unlocked mutex gives an undefined result
    //unlocking a mutex locked by another thread results in an error, so be careful
    if( hasTrack ) m_trackMutex.unlock();
    if( hasAlbum ) m_albumMutex.unlock();
    if( hasArtist ) m_artistMutex.unlock();
    if( hasYear ) m_yearMutex.unlock();
    if( hasGenre ) m_genreMutex.unlock();
    if( hasComposer ) m_composerMutex.unlock();
    if( hasLabel ) m_labelMutex.unlock();
}

#include "SqlRegistry.moc"
