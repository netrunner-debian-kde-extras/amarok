/****************************************************************************************
 * Copyright (c) 2007 Maximilian Kossick <maximilian.kossick@googlemail.com>            *
 * Copyright (c) 2008 Daniel Winter <dw@danielwinter.de>                                *
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

#include "SqlMeta.h"

#include "amarokconfig.h"
#include "core/support/Amarok.h"
#include "CapabilityDelegate.h"
#include "core/support/Debug.h"
#include "core/meta/support/MetaUtility.h"
#include "core-impl/meta/file/File.h"
#include "core-impl/meta/file/TagLibUtils.h"
#include "ArtistHelper.h"
#include "SqlCollection.h"
#include "SqlQueryMaker.h"
#include "SqlRegistry.h"
#include "covermanager/CoverFetcher.h"
#include "core/collections/support/SqlStorage.h"

#include "collectionscanner/AFTUtility.h"

#include <QAction>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QMultiHash>
#include <QMutexLocker>
#include <QPointer>
#if QT_VERSION >= 0x040600
#include <QPixmapCache>
#endif

#include <KCodecs>
#include <KLocale>
#include <KSharedPtr>

using namespace Meta;

QString
SqlTrack::getTrackReturnValues()
{
    //do not use any weird column names that contains commas: this will break getTrackReturnValuesCount()
    return "urls.deviceid, urls.rpath, urls.uniqueid, "
           "tracks.id, tracks.title, tracks.comment, "
           "tracks.tracknumber, tracks.discnumber, "
           "statistics.score, statistics.rating, "
           "tracks.bitrate, tracks.length, "
           "tracks.filesize, tracks.samplerate, "
           "statistics.createdate, statistics.accessdate, "
           "statistics.playcount, tracks.filetype, tracks.bpm, "
           "tracks.createdate, tracks.albumgain, tracks.albumpeakgain, "
           "tracks.trackgain, tracks.trackpeakgain, "
           "artists.name, artists.id, "
           "albums.name, albums.id, albums.artist, "
           "genres.name, genres.id, "
           "composers.name, composers.id, "
           "years.name, years.id";
}

QString
SqlTrack::getTrackJoinConditions()
{
    return "INNER JOIN tracks ON urls.id = tracks.url "
           "LEFT JOIN statistics ON urls.id = statistics.url "
           "LEFT JOIN artists ON tracks.artist = artists.id "
           "LEFT JOIN albums ON tracks.album = albums.id "
           "LEFT JOIN genres ON tracks.genre = genres.id "
           "LEFT JOIN composers ON tracks.composer = composers.id "
           "LEFT JOIN years ON tracks.year = years.id";
}

int
SqlTrack::getTrackReturnValueCount()
{
    static int count = getTrackReturnValues().split( ',' ).count();
    return count;
}

TrackPtr
SqlTrack::getTrack( int deviceid, const QString &rpath, Collections::SqlCollection *collection )
{
    QString query = "SELECT %1 FROM urls %2 "
                    "WHERE urls.deviceid = %3 AND urls.rpath = '%4';";
    query = query.arg( getTrackReturnValues(), getTrackJoinConditions(),
                       QString::number( deviceid ), collection->sqlStorage()->escape( rpath ) );
    QStringList result = collection->sqlStorage()->query( query );
    if( result.isEmpty() )
        return TrackPtr();
    return TrackPtr( new SqlTrack( collection, result ) );
}

TrackPtr
SqlTrack::getTrackFromUid( const QString &uid, Collections::SqlCollection* collection )
{
    QString query = "SELECT %1 FROM urls %2 "
                    "WHERE urls.uniqueid = '%3';";
    query = query.arg( getTrackReturnValues(), getTrackJoinConditions(),
                       collection->sqlStorage()->escape( uid ) );
    QStringList result = collection->sqlStorage()->query( query );
    if( result.isEmpty() )
        return TrackPtr();
    return TrackPtr( new SqlTrack( collection, result ) );
}

void
SqlTrack::refreshFromDatabase( const QString &uid, Collections::SqlCollection* collection, bool updateObservers )
{
    QString query = "SELECT %1 FROM urls %2 "
                    "WHERE urls.uniqueid = '%3';";
    query = query.arg( getTrackReturnValues(), getTrackJoinConditions(),
                       collection->sqlStorage()->escape( uid ) );
    QStringList result = collection->sqlStorage()->query( query );
    if( result.isEmpty() )
        return;

    updateData( result, true );
    if( updateObservers )
        notifyObservers();
}

void
SqlTrack::updateData( const QStringList &result, bool forceUpdates )
{
    QStringList::ConstIterator iter = result.constBegin();
    m_deviceid = (*(iter++)).toInt();
    m_rpath = *(iter++);
    m_uid = *(iter++);
    m_url = KUrl( m_collection->mountPointManager()->getAbsolutePath( m_deviceid, m_rpath ) );
    m_trackId = (*(iter++)).toInt();
    m_title = *(iter++);
    m_comment = *(iter++);
    m_trackNumber = (*(iter++)).toInt();
    m_discNumber = (*(iter++)).toInt();
    m_score = (*(iter++)).toDouble();
    m_rating = (*(iter++)).toInt();
    m_bitrate = (*(iter++)).toInt();
    m_length = (*(iter++)).toInt();
    m_filesize = (*(iter++)).toInt();
    m_sampleRate = (*(iter++)).toInt();
    m_firstPlayed = (*(iter++)).toInt();
    m_lastPlayed = (*(iter++)).toUInt();
    m_playCount = (*(iter++)).toInt();
    ++iter; //file type
    m_bpm = (*(iter++)).toFloat();
    m_createDate = QDateTime::fromTime_t( (*(iter++)).toUInt() );

    // if there is no track gain, we assume a gain of 0
    // if there is no album gain, we use the track gain
    QString albumGain = *(iter++);
    QString albumPeakGain = *(iter++);
    m_trackGain = (*(iter++)).toDouble();
    m_trackPeakGain = (*(iter++)).toDouble();
    if ( albumGain.isEmpty() )
    {
        m_albumGain = m_trackGain;
        m_albumPeakGain = m_trackPeakGain;
    }
    else
    {
        m_albumGain = albumGain.toDouble();
        m_albumPeakGain = albumPeakGain.toDouble();
    }
    SqlRegistry* registry = m_collection->registry();
    QString artist = *(iter++);
    int artistId = (*(iter++)).toInt();
    m_artist = registry->getArtist( artist, artistId, forceUpdates );
    QString album = *(iter++);
    int albumId =(*(iter++)).toInt();
    int albumArtistId = (*(iter++)).toInt();
    m_album = registry->getAlbum( album, albumId, albumArtistId, forceUpdates );
    QString genre = *(iter++);
    int genreId = (*(iter++)).toInt();
    m_genre = registry->getGenre( genre, genreId, forceUpdates );
    QString composer = *(iter++);
    int composerId = (*(iter++)).toInt();
    m_composer = registry->getComposer( composer, composerId, forceUpdates );
    QString year = *(iter++);
    int yearId = (*(iter++)).toInt();
    m_year = registry->getYear( year, yearId, forceUpdates );
    //Q_ASSERT_X( iter == result.constEnd(), "SqlTrack( Collections::SqlCollection*, QStringList )", "number of expected fields did not match number of actual fields: expected " + result.size() );
}

SqlTrack::SqlTrack( Collections::SqlCollection* collection, const QStringList &result )
    : Track()
    , m_collection( QPointer<Collections::SqlCollection>( collection ) )
    , m_capabilityDelegate( 0 )
    , m_batchUpdate( false )
    , m_writeAllStatisticsFields( false )
    , m_labelsInCache( false )
{
    updateData( result, false );
}

SqlTrack::~SqlTrack()
{
    delete m_capabilityDelegate;
}

bool
SqlTrack::isPlayable() const
{
    //a song is not playable anymore if the collection was removed
    return m_collection && QFile::exists( m_url.path() );
}

bool
SqlTrack::isEditable() const
{
    QFile::Permissions p = QFile::permissions( m_url.path() );
    const bool editable = ( p & QFile::WriteUser ) || ( p & QFile::WriteGroup ) || ( p & QFile::WriteOther );
    return m_collection && QFile::exists( m_url.path() ) && editable;
}

QString
SqlTrack::type() const
{
    return m_url.isLocalFile()
           ? Amarok::extension( m_url.fileName() )
           : i18n( "Stream" );
}

QString
SqlTrack::fullPrettyName() const
{
    QString s = m_artist->name();

    //FIXME doesn't work for resume playback

    if( s.isEmpty() )
        s = name();
    else
        s = i18n("%1 - %2", m_artist->name(), name() );

    //TODO
    if( s.isEmpty() )
        s = prettyTitle( m_url.fileName() );

    return s;
}

QString
SqlTrack::prettyTitle( const QString &filename ) //static
{
    QString s = filename; //just so the code is more readable

    //remove .part extension if it exists
    if (s.endsWith( ".part" ))
        s = s.left( s.length() - 5 );

    //remove file extension, s/_/ /g and decode %2f-like sequences
    s = s.left( s.lastIndexOf( '.' ) ).replace( '_', ' ' );
    s = KUrl::fromPercentEncoding( s.toAscii() );

    return s;
}


QString
SqlTrack::prettyName() const
{
    if ( !name().isEmpty() )
        return name();
    return  prettyTitle( m_url.fileName() );
}

void
SqlTrack::setUrl( const QString &url )
{
    DEBUG_BLOCK
    m_deviceid = m_collection->mountPointManager()->getIdForUrl( url );
    m_rpath = m_collection->mountPointManager()->getRelativePath( m_deviceid, url );

    m_cache.insert( Meta::Field::URL, m_collection->mountPointManager()->getAbsolutePath( m_deviceid, m_rpath ) );
    if( !m_batchUpdate )
        commitMetaDataChanges();
}

void
SqlTrack::setUrl( const int deviceid, const QString &rpath )
{
    m_deviceid = deviceid;
    m_rpath = rpath;

    m_url = KUrl( m_collection->mountPointManager()->getAbsolutePath( m_deviceid, m_rpath ) );
    if( !m_batchUpdate )
        commitMetaDataChanges();
}

void
SqlTrack::setArtist( const QString &newArtist )
{
    if ( m_artist && m_artist->name() == newArtist )
        return;

    m_cache.insert( Meta::Field::ARTIST, newArtist );
    if( !m_batchUpdate )
        commitMetaDataChanges();
}

void
SqlTrack::setGenre( const QString &newGenre )
{
    if ( m_genre && m_genre->name() == newGenre )
        return;

    m_cache.insert( Meta::Field::GENRE, newGenre );
    if( !m_batchUpdate )
        commitMetaDataChanges();
}

void
SqlTrack::setComposer( const QString &newComposer )
{
    if ( m_composer && m_composer->name() == newComposer )
        return;

    m_cache.insert( Meta::Field::COMPOSER, newComposer );
    if( !m_batchUpdate )
        commitMetaDataChanges();
}

void
SqlTrack::setYear( const QString &newYear )
{
    if ( m_year && m_year->name() == newYear )
        return;

    m_cache.insert( Meta::Field::YEAR, newYear );
    if( !m_batchUpdate )
        commitMetaDataChanges();
}

void
SqlTrack::setBpm( const qreal newBpm )
{
    if ( m_bpm && m_bpm == newBpm )
        return;

    m_cache.insert( Meta::Field::BPM, newBpm );
    if( !m_batchUpdate )
        commitMetaDataChanges();
}

void
SqlTrack::setAlbum( const QString &newAlbum )
{
    if ( m_album && m_album->name() == newAlbum )
        return;

    m_cache.insert( Meta::Field::ALBUM, newAlbum );
    if( !m_batchUpdate )
        commitMetaDataChanges();
}

void
SqlTrack::setScore( double newScore )
{
    if( m_batchUpdate )
        m_cache.insert( Meta::Field::SCORE, newScore );
    else
    {
        m_score = newScore;
        updateStatisticsInDb( Meta::Field::SCORE );
        notifyObservers();
    }
}

void
SqlTrack::setRating( int newRating )
{
    if( m_batchUpdate )
        m_cache.insert( Meta::Field::RATING, newRating );
    else
    {
        m_rating = newRating;
        updateStatisticsInDb( Meta::Field::RATING );
        notifyObservers();
    }
}

void
SqlTrack::setTrackNumber( int newTrackNumber )
{
    m_cache.insert( Meta::Field::TRACKNUMBER, newTrackNumber );
    if( !m_batchUpdate )
        commitMetaDataChanges();
}

void
SqlTrack::setDiscNumber( int newDiscNumber )
{
    m_cache.insert( Meta::Field::DISCNUMBER, newDiscNumber );
    if( !m_batchUpdate )
        commitMetaDataChanges();
}

void
SqlTrack::setComment( const QString &newComment )
{
    m_cache.insert( Meta::Field::COMMENT, newComment );
    if( !m_batchUpdate )
        commitMetaDataChanges();
}

void
SqlTrack::setTitle( const QString &newTitle )
{
    m_cache.insert( Meta::Field::TITLE, newTitle );
    if( !m_batchUpdate )
        commitMetaDataChanges();
}

void
SqlTrack::setLastPlayed( const uint newTime )
{
    if( m_batchUpdate )
        m_cache.insert( Meta::Field::LAST_PLAYED, newTime );
    else
    {
        m_lastPlayed = newTime;
        updateStatisticsInDb( Meta::Field::LAST_PLAYED );
        notifyObservers();
    }
}

void
SqlTrack::setFirstPlayed( const uint newTime )
{
    if( m_batchUpdate )
        m_cache.insert( Meta::Field::FIRST_PLAYED, newTime );
    else
    {
        m_firstPlayed = newTime;
        updateStatisticsInDb( Meta::Field::FIRST_PLAYED );
        notifyObservers();
    }
}

void
SqlTrack::setPlayCount( const int newCount )
{
    if( m_batchUpdate )
        m_cache.insert( Meta::Field::PLAYCOUNT, newCount );
    else
    {
        m_playCount = newCount;
        updateStatisticsInDb( Meta::Field::PLAYCOUNT);
        notifyObservers();
    }
}

void
SqlTrack::setUidUrl( const QString &uid )
{
    DEBUG_BLOCK
    QString newid = uid;
    if( !newid.startsWith( "amarok-sqltrackuid" ) )
        newid.prepend( "amarok-sqltrackuid://" );

    if( m_batchUpdate )
        m_cache.insert( Meta::Field::UNIQUEID, newid );
    else
    {
        debug() << "setting uidUrl manually...did you really mean to do this?";
        m_newUid = newid;
        writeMetaDataToDb( QStringList() );
        notifyObservers();
    }
}

void
SqlTrack::beginMetaDataUpdate()
{
    m_batchUpdate = true;
}

void
SqlTrack::endMetaDataUpdate()
{
    commitMetaDataChanges();
    m_batchUpdate = false;
}

void
SqlTrack::writeMetaDataToFile()
{
    Meta::Field::writeFields( m_url.path(), m_cache );
    //updating the fields might have changed the filesize
    //read the current filesize so that we can update the db
    QFile file( m_url.path() );
    if( file.exists() )
        m_filesize = file.size();
    AFTUtility aftutil;
    m_newUid = QString( "amarok-sqltrackuid://" ) + aftutil.readUniqueId( m_url.path() );
}

void
SqlTrack::updateFileSize()
{
    QFile file( m_url.path() );
    if( file.exists() )
        m_filesize = file.size();
}

void
SqlTrack::commitMetaDataChanges()
{
    if( m_cache.isEmpty() )
        return; // nothing to do

    bool collectionChanged = false;

    // for all the following objects we need to invalidate the cache and
    // notify the observers after the update
    KSharedPtr<SqlArtist>   oldArtist;
    KSharedPtr<SqlArtist>   newArtist;
    KSharedPtr<SqlAlbum>    oldAlbum;
    KSharedPtr<SqlAlbum>    newAlbum;
    KSharedPtr<SqlComposer> oldComposer;
    KSharedPtr<SqlComposer> newComposer;
    KSharedPtr<SqlGenre>    oldGenre;
    KSharedPtr<SqlGenre>    newGenre;
    KSharedPtr<SqlYear>     oldYear;
    KSharedPtr<SqlYear>     newYear;

    if( m_cache.contains( Meta::Field::TITLE ) )
        m_title = m_cache.value( Meta::Field::TITLE ).toString();
    if( m_cache.contains( Meta::Field::COMMENT ) )
        m_comment = m_cache.value( Meta::Field::COMMENT ).toString();
    if( m_cache.contains( Meta::Field::SCORE ) )
        m_score = m_cache.value( Meta::Field::SCORE ).toDouble();
    if( m_cache.contains( Meta::Field::RATING ) )
        m_rating = m_cache.value( Meta::Field::RATING ).toInt();
    if( m_cache.contains( Meta::Field::PLAYCOUNT ) )
        m_playCount = m_cache.value( Meta::Field::PLAYCOUNT ).toInt();
    if( m_cache.contains( Meta::Field::FIRST_PLAYED ) )
        m_firstPlayed = m_cache.value( Meta::Field::FIRST_PLAYED ).toUInt();
    if( m_cache.contains( Meta::Field::LAST_PLAYED ) )
        m_lastPlayed = m_cache.value( Meta::Field::LAST_PLAYED ).toUInt();
    if( m_cache.contains( Meta::Field::TRACKNUMBER ) )
        m_trackNumber = m_cache.value( Meta::Field::TRACKNUMBER ).toInt();
    if( m_cache.contains( Meta::Field::DISCNUMBER ) )
        m_discNumber = m_cache.value( Meta::Field::DISCNUMBER ).toInt();
    if( m_cache.contains( Meta::Field::UNIQUEID ) )
        m_uid = m_cache.value( Meta::Field::UNIQUEID ).toString();
    if( m_cache.contains( Meta::Field::URL ) )
    {
        debug() << "m_cache contains a new URL, setting m_url";
        m_url = m_cache.value( Meta::Field::URL ).toString();
    }

    if( m_cache.contains( Meta::Field::ARTIST ) )
    {
        //invalidate cache of the old artist...
        oldArtist = static_cast<SqlArtist*>(m_artist.data());
        m_artist = m_collection->registry()->getArtist( m_cache.value( Meta::Field::ARTIST ).toString() );
        //and the new one
        newArtist = static_cast<SqlArtist*>(m_artist.data());

        // if the current album is no compilation and we aren't changing
        // the album anyway, then we need to create a new album with the
        // new artist.
        if( m_album &&
            m_album->hasAlbumArtist() &&
            !m_cache.contains( Meta::Field::ALBUM ) )
            m_cache.insert( Meta::Field::ALBUM, m_album->name() );

        collectionChanged = true;
    }

    if( m_cache.contains( Meta::Field::ALBUM ) )
    {
        oldAlbum = static_cast<SqlAlbum*>(m_album.data());
        //the album should remain a compilation after renaming it
        int artistId = 0;
        if( m_album->hasAlbumArtist() )
        {
            artistId = KSharedPtr<SqlArtist>::staticCast( m_album->albumArtist() )->id();
        }
        m_album = m_collection->registry()->getAlbum( m_cache.value( Meta::Field::ALBUM ).toString(), -1, artistId );
        newAlbum = static_cast<SqlAlbum*>(m_album.data());

        // copy the image BUG: 203211
        // IMPROVEMENT: use setImage(QString) to prevent the image being
        // physically copied.
        if( oldAlbum &&
            oldAlbum->hasImage(0) && !newAlbum->hasImage(0) )
            newAlbum->setImage( oldAlbum->image(0) );

        collectionChanged = true;
    }

    if( m_cache.contains( Meta::Field::COMPOSER ) )
    {
        oldComposer = static_cast<SqlComposer*>(m_composer.data());
        m_composer = m_collection->registry()->getComposer( m_cache.value( Meta::Field::COMPOSER ).toString() );
        newComposer = static_cast<SqlComposer*>(m_composer.data());
        collectionChanged = true;
    }

    if( m_cache.contains( Meta::Field::GENRE ) )
    {
        oldGenre = static_cast<SqlGenre*>(m_genre.data());
        m_genre = m_collection->registry()->getGenre( m_cache.value( Meta::Field::GENRE ).toString() );
        newGenre = static_cast<SqlGenre*>(m_genre.data());
        collectionChanged = true;
    }

    if( m_cache.contains( Meta::Field::YEAR ) )
    {
        oldYear = static_cast<SqlYear*>(m_year.data());
        m_year = m_collection->registry()->getYear( m_cache.value( Meta::Field::YEAR ).toString() );
        newYear = static_cast<SqlYear*>(m_year.data());
        collectionChanged = true;
    }

    if( m_cache.contains( Meta::Field::BPM ) )
        m_bpm = m_cache.value( Meta::Field::BPM ).toDouble();

    //updating the tags of the file might change the filesize
    //therefore write the tag to the file first, and update the db
    //with the new filesize
    writeMetaDataToFile();
    writeMetaDataToDb( m_cache.keys() );
    updateStatisticsInDb( m_cache.keys() );

    m_cache.clear();
    notifyObservers();

    // these calls must be made after the database has been updated
#define INVALIDATE_AND_UPDATE(X) if( X ) \
    { \
        X->invalidateCache(); \
        X->notifyObservers(); \
    }
    INVALIDATE_AND_UPDATE(oldArtist);
    INVALIDATE_AND_UPDATE(newArtist);
    INVALIDATE_AND_UPDATE(oldAlbum);
    INVALIDATE_AND_UPDATE(newAlbum);
    INVALIDATE_AND_UPDATE(oldComposer);
    INVALIDATE_AND_UPDATE(newComposer);
    INVALIDATE_AND_UPDATE(oldGenre);
    INVALIDATE_AND_UPDATE(newGenre);
    INVALIDATE_AND_UPDATE(oldYear);
    INVALIDATE_AND_UPDATE(newYear);
#undef INVALIDATE_AND_UPDATE

    if( collectionChanged )
        m_collection->sendChangedSignal();
}

void
SqlTrack::writeMetaDataToDb( const QStringList &fields )
{
    DEBUG_BLOCK
    //TODO store the tracks id in SqlTrack
    if( !fields.isEmpty() )
    {
        debug() << "looking for UID " << m_uid;
        QString query = "SELECT tracks.id FROM tracks LEFT JOIN urls ON tracks.url = urls.id WHERE urls.uniqueid = '%1';";
        query = query.arg( m_collection->sqlStorage()->escape( m_uid ) );
        QStringList res = m_collection->sqlStorage()->query( query );
        if( res.isEmpty() )
        {
            debug() << "Could not perform update in writeMetaDataToDb";
            return;
        }
        int id = res[0].toInt();
        QString update = "UPDATE tracks SET %1 WHERE id = %2;";
        //This next line is do-nothing SQLwise but is here to prevent the need for tons of if-else brackets just to keep track of
        //whether or not commas are needed
        QString tags = QString( "id=%1" ).arg( id );
        if( fields.contains( Meta::Field::TITLE ) )
            tags += QString( ",title='%1'" ).arg( m_collection->sqlStorage()->escape( m_title ) );
        if( fields.contains( Meta::Field::COMMENT ) )
            tags += QString( ",comment='%1'" ).arg( m_collection->sqlStorage()->escape( m_comment ) );
        if( fields.contains( Meta::Field::TRACKNUMBER ) )
            tags += QString( ",tracknumber=%1" ).arg( QString::number( m_trackNumber ) );
        if( fields.contains( Meta::Field::DISCNUMBER ) )
            tags += QString( ",discnumber=%1" ).arg( QString::number( m_discNumber ) );
        if( fields.contains( Meta::Field::ARTIST ) )
            tags += QString( ",artist=%1" ).arg( QString::number( KSharedPtr<SqlArtist>::staticCast( m_artist )->id() ) );
        if( fields.contains( Meta::Field::ALBUM ) )
            tags += QString( ",album=%1" ).arg( QString::number( KSharedPtr<SqlAlbum>::staticCast( m_album )->id() ) );
        if( fields.contains( Meta::Field::GENRE ) )
            tags += QString( ",genre=%1" ).arg( QString::number( KSharedPtr<SqlGenre>::staticCast( m_genre )->id() ) );
        if( fields.contains( Meta::Field::COMPOSER ) )
            tags += QString( ",composer=%1" ).arg( QString::number( KSharedPtr<SqlComposer>::staticCast( m_composer )->id() ) );
        if( fields.contains( Meta::Field::YEAR ) )
            tags += QString( ",year=%1" ).arg( QString::number( KSharedPtr<SqlYear>::staticCast( m_year )->id() ) );
        if( fields.contains( Meta::Field::BPM ) )
            tags += QString( ",bpm=%1" ).arg( QString::number( m_bpm ) );
        updateFileSize();
        tags += QString( ",filesize=%1" ).arg( m_filesize );
        update = update.arg( tags, QString::number( id ) );
        debug() << "Running following update query: " << update;
        m_collection->sqlStorage()->query( update );
    }

    if( !m_newUid.isEmpty() )
    {
        QString update = "UPDATE urls SET uniqueid='%1' WHERE uniqueid='%2';";
        update = update.arg( m_newUid, m_uid );
        debug() << "Updating uid from " << m_uid << " to " << m_newUid;
        m_collection->sqlStorage()->query( update );
        m_uid = m_newUid;
        m_newUid.clear();
    }
}

void
SqlTrack::updateStatisticsInDb( const QStringList &fields )
{
    QString query = "SELECT urls.id FROM urls WHERE urls.deviceid = %1 AND urls.rpath = '%2';";
    query = query.arg( QString::number( m_deviceid ), m_collection->sqlStorage()->escape( m_rpath ) );
    QStringList res = m_collection->sqlStorage()->query( query );
    if( res.isEmpty() )
        return; // No idea why this happens.. but it does
    int urlId = res[0].toInt();
    QStringList count = m_collection->sqlStorage()->query( QString( "SELECT count(*) FROM statistics WHERE url = %1;" ).arg( urlId ) );
    if( count[0].toInt() == 0 )
    {
        m_firstPlayed = QDateTime::currentDateTime().toTime_t();
        QString insert = "INSERT INTO statistics(url,rating,score,playcount,accessdate,createdate) VALUES ( %1 );";
        QString data = "%1,%2,%3,%4,%5,%6";
        data = data.arg( QString::number( urlId )
                , QString::number( m_rating )
                , QString::number( m_score )
                , QString::number( m_playCount )
                , QString::number( m_lastPlayed )
                , QString::number( m_firstPlayed ) );
        insert = insert.arg( data );
        m_collection->sqlStorage()->insert( insert, "statistics" );
    }
    else
    {
        QString update = "UPDATE statistics SET %1 WHERE url = %2;";
        QString stats = QString( "url=%1" ).arg( QString::number( urlId ) ); //see above function for explanation
        if( fields.contains( Meta::Field::RATING ) )
            stats += QString( ",rating=%1" ).arg( QString::number( m_rating ) );
        if( fields.contains( Meta::Field::SCORE ) )
            stats += QString( ",score=%1" ).arg( QString::number( m_score ) );
        if( fields.contains( Meta::Field::PLAYCOUNT ) )
            stats += QString( ",playcount=%1" ).arg( QString::number( m_playCount ) );
        if( fields.contains( Meta::Field::LAST_PLAYED ) )
            stats += QString( ",accessdate=%1" ).arg( QString::number( m_lastPlayed ) );

        if( m_writeAllStatisticsFields )
            stats += QString(",createdate=%1").arg( QString::number( m_firstPlayed ) );

        update = update.arg( stats, QString::number( urlId ) );

        m_collection->sqlStorage()->query( update );
    }
}

void
SqlTrack::finishedPlaying( double playedFraction )
{
    DEBUG_BLOCK

    beginMetaDataUpdate();    // Batch updates, so we only bother our observers once.

    bool doUpdate = false;

    if( m_length < 30000 && playedFraction == 1.0 )
        doUpdate = true;
    if( playedFraction >= 0.5 && m_length >= 30000 ) //song >= 30 seconds and at least half played
        doUpdate = true;
    if( playedFraction * m_length > 240000 )
        doUpdate = true;

    if( doUpdate )
    {
        setPlayCount( playCount() + 1 );
        if( firstPlayed() == 0 )
            setFirstPlayed( QDateTime::currentDateTime().toTime_t() );
        setLastPlayed( QDateTime::currentDateTime().toTime_t() );
    }

    setScore( Amarok::computeScore( score(), playCount(), playedFraction ) );

    endMetaDataUpdate();
}

bool
SqlTrack::inCollection() const
{
    return true;
}

Collections::Collection*
SqlTrack::collection() const
{
    return m_collection;
}

QString
SqlTrack::cachedLyrics() const
{
    QString query = QString( "SELECT lyrics FROM lyrics WHERE url = '%1'" )
                        .arg( m_collection->sqlStorage()->escape( m_rpath ) );
    QStringList result = m_collection->sqlStorage()->query( query );
    if( result.isEmpty() )
        return QString();
    return result[0];
}

void
SqlTrack::setCachedLyrics( const QString &lyrics )
{
    QString query = QString( "SELECT count(*) FROM lyrics WHERE url = '%1'")
                        .arg( m_collection->sqlStorage()->escape(m_rpath) );

    const QStringList queryResult = m_collection->sqlStorage()->query( query );

    if( queryResult.isEmpty() )
        return;

    if( queryResult[0].toInt() == 0 )
    {
        QString insert = QString( "INSERT INTO lyrics( url, lyrics ) VALUES ( '%1', '%2' );" )
                            .arg( m_collection->sqlStorage()->escape( m_rpath ),
                                  m_collection->sqlStorage()->escape( lyrics ) );
        m_collection->sqlStorage()->insert( insert, "lyrics" );
    }
    else
    {
        QString update = QString( "UPDATE lyrics SET lyrics = '%1' WHERE url = '%2';" )
                            .arg( m_collection->sqlStorage()->escape( lyrics ),
                                  m_collection->sqlStorage()->escape( m_rpath ) );
        m_collection->sqlStorage()->query( update );
    }
}

bool
SqlTrack::hasCapabilityInterface( Capabilities::Capability::Type type ) const
{
    return ( m_capabilityDelegate ? m_capabilityDelegate->hasCapabilityInterface( type, this ) : false );
}

Capabilities::Capability*
SqlTrack::createCapabilityInterface( Capabilities::Capability::Type type )
{
    return ( m_capabilityDelegate ? m_capabilityDelegate->createCapabilityInterface( type, this) : 0 );
}

void
SqlTrack::addLabel( const QString &label )
{
    Meta::LabelPtr realLabel = m_collection->registry()->getLabel( label, -1 );
    addLabel( realLabel );
}

void
SqlTrack::addLabel( const Meta::LabelPtr &label )
{
    KSharedPtr<SqlLabel> sqlLabel = KSharedPtr<SqlLabel>::dynamicCast( label );
    if( !sqlLabel )
    {
        Meta::LabelPtr tmp = m_collection->registry()->getLabel( label->name(), -1 );
        sqlLabel = KSharedPtr<SqlLabel>::dynamicCast( tmp );
    }
    if( sqlLabel )
    {
        QStringList rs = m_collection->sqlStorage()->query( QString( "SELECT url FROM tracks WHERE id = %1;" ).arg( m_trackId ) );
        if( rs.isEmpty() )
        {
            warning() << "Did not find entry in TRACKS table for track ID " << m_trackId;
            return;
        }
        QString urlId = rs.first();
        QString countQuery = "SELECT COUNT(*) FROM urls_labels WHERE url = %1 AND label = %2;";
        QStringList countRs = m_collection->sqlStorage()->query( countQuery.arg( urlId, QString::number( sqlLabel->id() ) ) );
        if( countRs.first().toInt() == 0 )
        {
            QString insert = "INSERT INTO urls_labels(url,label) VALUES (%1,%2);";
            m_collection->sqlStorage()->insert( insert.arg( urlId, QString::number( sqlLabel->id() ) ), "urls_labels" );

            if( m_labelsInCache )
            {
                m_labelsCache.append( Meta::LabelPtr::staticCast( sqlLabel ) );
            }
            notifyObservers();
            sqlLabel->invalidateCache();
            sqlLabel->notifyObservers();
        }
    }
}

void
SqlTrack::removeLabel( const Meta::LabelPtr &label )
{
    KSharedPtr<SqlLabel> sqlLabel = KSharedPtr<SqlLabel>::dynamicCast( label );
    if( !sqlLabel )
    {
        Meta::LabelPtr tmp = m_collection->registry()->getLabel( label->name(), -1 );
        sqlLabel = KSharedPtr<SqlLabel>::dynamicCast( tmp );
    }
    if( sqlLabel )
    {
        QString query = "DELETE FROM urls_labels WHERE label = %2 and url = (SELECT url FROM tracks WHERE id = %1);";
        m_collection->sqlStorage()->query( query.arg( QString::number( m_trackId ), QString::number( sqlLabel->id() ) ) );
        if( m_labelsInCache )
        {
            m_labelsCache.removeAll( Meta::LabelPtr::staticCast( sqlLabel ) );
        }
        notifyObservers();
        sqlLabel->invalidateCache();
        sqlLabel->notifyObservers();
    }
}

Meta::LabelList
SqlTrack::labels() const
{
    if( m_labelsInCache )
    {
        return m_labelsCache;
    }
    else if( m_collection )
    {
        Collections::SqlQueryMaker *qm = static_cast< Collections::SqlQueryMaker* >( m_collection->queryMaker() );
        qm->setQueryType( Collections::QueryMaker::Label );
        const_cast<SqlTrack*>( this )->addMatchTo( qm );
        qm->setBlocking( true );
        qm->run();

        m_labelsInCache = true;
        m_labelsCache = qm->labels();

        delete qm;
        return m_labelsCache;
    }
    else
    {
        return Meta::LabelList();
    }
}

void
SqlTrack::setCapabilityDelegate( Capabilities::TrackCapabilityDelegate *delegate )
{
    m_capabilityDelegate = delegate;
}

//---------------------- class Artist --------------------------

SqlArtist::SqlArtist( Collections::SqlCollection* collection, int id, const QString &name ) : Artist()
    ,m_collection( QPointer<Collections::SqlCollection>( collection ) )
    ,m_delegate( 0 )
    ,m_name( name )
    ,m_id( id )
    ,m_tracksLoaded( false )
    ,m_albumsLoaded( false )
    ,m_mutex( QMutex::Recursive )
{
    //nothing to do (yet)
}

Meta::SqlArtist::~SqlArtist()
{
    delete m_delegate;
}

void
SqlArtist::updateData( Collections::SqlCollection *collection, int id, const QString &name )
{
    m_mutex.lock();
    m_collection = QPointer<Collections::SqlCollection>( collection );
    m_id = id;
    m_name = name;
    m_mutex.unlock();
}

void
SqlArtist::invalidateCache()
{
    m_mutex.lock();
    m_tracksLoaded = false;
    m_tracks.clear();
    m_mutex.unlock();
}

TrackList
SqlArtist::tracks()
{
    QMutexLocker locker( &m_mutex );
    if( m_tracksLoaded )
    {
        return m_tracks;
    }
    else if( m_collection )
    {
        Collections::SqlQueryMaker *qm = static_cast< Collections::SqlQueryMaker* >( m_collection->queryMaker() );
        qm->setQueryType( Collections::QueryMaker::Track );
        addMatchTo( qm );
        qm->setBlocking( true );
        qm->run();
        m_tracks = qm->tracks( m_collection->collectionId() );
        delete qm;
        m_tracksLoaded = true;
        return m_tracks;
    }
    return TrackList();
}

AlbumList
SqlArtist::albums()
{
    QMutexLocker locker( &m_mutex );
    if( m_albumsLoaded )
    {
        return m_albums;
    }
    else if( m_collection )
    {
        Collections::SqlQueryMaker *qm = static_cast< Collections::SqlQueryMaker* >( m_collection->queryMaker() );
        qm->setQueryType( Collections::QueryMaker::Album );
        addMatchTo( qm );
        qm->setBlocking( true );
        qm->run();
        m_albums = qm->albums( m_collection->collectionId() );
        delete qm;
        m_albumsLoaded = true;
        return m_albums;
    }
    return AlbumList();
}

bool
SqlArtist::hasCapabilityInterface( Capabilities::Capability::Type type ) const
{
    return ( m_delegate ? m_delegate->hasCapabilityInterface( type, this ) : 0 );
}

Capabilities::Capability*
SqlArtist::createCapabilityInterface( Capabilities::Capability::Type type )
{
    return ( m_delegate ? m_delegate->createCapabilityInterface( type, this ) : 0 );
}


//---------------SqlAlbum---------------------------------
const QString SqlAlbum::AMAROK_UNSET_MAGIC = QString( "AMAROK_UNSET_MAGIC" );

SqlAlbum::SqlAlbum( Collections::SqlCollection* collection, int id, const QString &name, int artist ) : Album()
    , m_collection( QPointer<Collections::SqlCollection>( collection ) )
    , m_delegate( 0 )
    , m_name( name )
    , m_id( id )
    , m_artistId( artist )
    , m_imageId( -1 )
    , m_hasImage( false )
    , m_hasImageChecked( false )
    , m_unsetImageId( -1 )
    , m_tracksLoaded( false )
    , m_suppressAutoFetch( false )
    , m_artist()
    , m_mutex( QMutex::Recursive )
{
    //nothing to do
}

Meta::SqlAlbum::~SqlAlbum()
{
    delete m_delegate;
}

void
SqlAlbum::updateData( Collections::SqlCollection *collection, int id, const QString &name, int artist )
{
    m_mutex.lock();
    m_collection = QPointer<Collections::SqlCollection>( collection );
    m_id = id;
    m_name = name;
    m_artistId = artist;
    m_mutex.unlock();
}

void
SqlAlbum::invalidateCache()
{
    m_mutex.lock();
    m_tracksLoaded = false;
    m_hasImage = false;
    m_hasImageChecked = false;
    m_tracks.clear();
    m_mutex.unlock();
}

TrackList
SqlAlbum::tracks()
{
    QMutexLocker locker( &m_mutex );
    if( m_tracksLoaded )
    {
        return m_tracks;
    }
    else if( m_collection )
    {
        Collections::SqlQueryMaker *qm = static_cast< Collections::SqlQueryMaker* >( m_collection->queryMaker() );
        qm->setQueryType( Collections::QueryMaker::Track );
        addMatchTo( qm );
        qm->orderBy( Meta::valDiscNr );
        qm->orderBy( Meta::valTrackNr );
        qm->orderBy( Meta::valTitle );
        qm->setBlocking( true );
        qm->run();
        m_tracks = qm->tracks( m_collection->collectionId() );
        delete qm;
        m_tracksLoaded = true;
        return m_tracks;
    }
    else
        return TrackList();
}

// note for internal implementation:
// if hasImage returns true then m_imagePath is set
bool
SqlAlbum::hasImage( int size ) const
{
    Q_UNUSED(size); // we have every size if we have an image at all

    if( !m_hasImageChecked )
    {
        m_hasImageChecked = true;

        const_cast<SqlAlbum*>( this )->largeImagePath();

        // The user has explicitly set no cover
        if( m_imagePath == AMAROK_UNSET_MAGIC )
            m_hasImage = false;

        // if we don't have an image but it was not explicitely blocked
        else if( m_imagePath.isEmpty() )
        {
            // Cover fetching runs in another thread. If there is a retrieved cover
            // then updateImage() gets called which updates the cache and alerts the
            // subscribers. We use queueAlbum() because this runs the fetch as a
            // background job and doesn't give an intruding popup asking for confirmation
            if( !m_suppressAutoFetch && !m_name.isEmpty() && AmarokConfig::autoGetCoverArt() )
                CoverFetcher::instance()->queueAlbum( AlbumPtr(const_cast<SqlAlbum *>(this)) );

            m_hasImage = false;
        }
        else
            m_hasImage = true;
    }

    return m_hasImage;
}

QPixmap
SqlAlbum::image( int size )
{
    if( !hasImage() )
        return Meta::Album::image( size );

    QPixmap pixmap;
    // look in the memory pixmap cache
    // large scale images are not stored in memory
#if QT_VERSION >= 0x040600
    QString cachedPixmapKey = QString::number(size) + "@acover" + QString::number(m_imageId);
    if( size > 1 && QPixmapCache::find( cachedPixmapKey, &pixmap ) )
        return pixmap;
#endif

    // findCachedImage looks for a scaled version of the fullsize image
    // which may have been saved on a previous lookup
    QString cachedImagePath;
    if( size <= 1 && !hasEmbeddedImage() )
        cachedImagePath = m_imagePath;
    else
        cachedImagePath = scaledDiskCachePath( size );

    //FIXME this cache doesn't differentiate between shadowed/unshadowed
    // a image exists. just load it.
    if( !cachedImagePath.isEmpty() && QFile( cachedImagePath ).exists() )
    {
        pixmap = QPixmap( cachedImagePath );
        if( pixmap.isNull() )
            return Meta::Album::image( size );
#if QT_VERSION >= 0x040600
        if( size > 0)
            QPixmapCache::insert( cachedPixmapKey, pixmap );
#endif
        return pixmap;
    }

    // no cached scaled image exists. Have to create it
    QImage img;
    if( hasEmbeddedImage() )
        img = getEmbeddedImage();
    else
        img = QImage( m_imagePath );

    if( img.isNull() )
        return Meta::Album::image( size );

    if( size > 1 && size < 1000 )
    {
        img = img.scaled( size, size, Qt::KeepAspectRatio, Qt::SmoothTransformation );
        img.save( cachedImagePath, "JPG" );
    }
    else if( hasEmbeddedImage() )
    {
        // store it on the disk for later reference.
        img.save( cachedImagePath, "JPG" );
    }

    pixmap = QPixmap::fromImage( img );
#if QT_VERSION >= 0x040600
    if( size > 1)
        QPixmapCache::insert( cachedPixmapKey, pixmap );
#endif
    return pixmap;
}

KUrl
SqlAlbum::imageLocation( int size )
{
    if( !hasImage() )
        return KUrl();

    // findCachedImage looks for a scaled version of the fullsize image
    // which may have been saved on a previous lookup
    QString cachedImagePath;
    if( size >= 1 && !hasEmbeddedImage() )
        cachedImagePath = m_imagePath;
    else
        cachedImagePath = scaledDiskCachePath( size );

    if( cachedImagePath.isEmpty() )
        return KUrl();

    if( !QFile( cachedImagePath ).exists() )
    {
        // If we don't have the location, it's possible that we haven't tried to find the image yet
        // So, let's look for it and just ignore the result
        QPixmap i = image( size );
        Q_UNUSED( i )
    }

    if( !QFile( cachedImagePath ).exists() )
        return KUrl();

    return cachedImagePath;
}

void
SqlAlbum::setImage( const QPixmap &pixmap )
{
    if( pixmap.isNull() )
        return;

    // removeImage() will destroy all scaled cached versions of the artwork
    // and remove references from the database if required.
    removeImage();

    QString path = largeDiskCachePath();
    // make sure not to overwrite existing images
    while( QFile(path).exists() )
        path += "_"; // not that nice but it shouldn't happen that often.

    setImage( path );
    pixmap.save( path, "JPG" );

    notifyObservers();
}

void
SqlAlbum::removeImage()
{
    if( !hasImage() )
        return;

    // Update the database image path
    // Set the album image to a magic value which will tell Amarok not to fetch it automatically
    const int unsetId = unsetImageId();
    QString query = "UPDATE albums SET image = %1 WHERE id = %2";
    m_collection->sqlStorage()->query( query.arg( QString::number( unsetId ), QString::number( m_id ) ) );

    // From here on we check if there are any remaining references to that particular image in the database
    // If there aren't, then we should remove the image path from the database ( and possibly delete the file? )
    // If there are, we need to leave it since other albums will reference this particular image path.
    //
    query = "SELECT count( albums.id ) FROM albums "
                    "WHERE albums.image = %1";
    QStringList res = m_collection->sqlStorage()->query( query.arg( QString::number( m_imageId ) ) );

    if( !res.isEmpty() )
    {
        int references = res[0].toInt();

        // If there are no more references to this particular image, then we should clean up
        if( references <= 0 )
        {
            query = "DELETE FROM images WHERE id = %1";
            m_collection->sqlStorage()->query( query.arg( QString::number( m_imageId ) ) );

            // remove the large cover only if it was cached.
            QDir largeCoverDir( Amarok::saveLocation( "albumcovers/large/" ) );
            if( QFileInfo(m_imagePath).absoluteDir() == largeCoverDir )
                QFile::remove( m_imagePath );

            // remove all cache images
            QString key = md5sum( QString(), QString(), m_imagePath );
            QDir        cacheDir( Amarok::saveLocation( "albumcovers/cache/" ) );
            QStringList cacheFilter;
            cacheFilter << QString( "*@" ) + key;
            QStringList cachedImages = cacheDir.entryList( cacheFilter );

            foreach( const QString &image, cachedImages )
            {
                bool r = QFile::remove( cacheDir.filePath( image ) );
                debug() << "deleting cached image: " << image << " : " + ( r ? QString("ok") : QString("fail") );
            }
        }
    }

    m_imageId = -1;
    m_imagePath = QString();
    m_hasImage = false;
    m_hasImageChecked = true;

    notifyObservers();
}

int
SqlAlbum::unsetImageId() const
{
    // Return the cached value if we have already done the lookup before
    if( m_unsetImageId >= 0 )
        return m_unsetImageId;

    QString query = "SELECT id FROM images WHERE path = '%1'";
    QStringList res = m_collection->sqlStorage()->query( query.arg( AMAROK_UNSET_MAGIC ) );

    // We already have the AMAROK_UNSET_MAGIC variable in the database
    if( !res.isEmpty() )
    {
        m_unsetImageId = res[0].toInt();
    }
    else
    {
        // We need to create this value
        query = QString( "INSERT INTO images( path ) VALUES ( '%1' )" )
                         .arg( m_collection->sqlStorage()->escape( AMAROK_UNSET_MAGIC ) );
        m_unsetImageId = m_collection->sqlStorage()->insert( query, "images" );
    }
    return m_unsetImageId;
}

bool
SqlAlbum::isCompilation() const
{
    return !hasAlbumArtist();
}

bool
SqlAlbum::hasAlbumArtist() const
{
    return albumArtist();
}

Meta::ArtistPtr
SqlAlbum::albumArtist() const
{
    if( m_artistId != 0 && !m_artist )
    {
        QString query = QString( "SELECT artists.name FROM artists WHERE artists.id = %1;" ).arg( m_artistId );
        QStringList result = m_collection->sqlStorage()->query( query );
        if( result.isEmpty() )
            return Meta::ArtistPtr(); //FIXME BORKED LOGIC: can return 0, although m_artistId != 0
        const_cast<SqlAlbum*>( this )->m_artist =
            m_collection->registry()->getArtist( result.first(), m_artistId );
    }
    return m_artist;
}

QByteArray
SqlAlbum::md5sum( const QString& artist, const QString& album, const QString& file ) const
{
    // FIXME: names with unicode characters are not supported.
    // FIXME: "The Beatles"."Collection" and "The"."Beatles Collection" will produce the same hash.
    // FIXME: Correcting this now would invalidate all existing image stores.
    KMD5 context( artist.toLower().toLocal8Bit() + album.toLower().toLocal8Bit() + file.toLocal8Bit() );
    return context.hexDigest();
}

QString
SqlAlbum::largeDiskCachePath() const
{
    // IMPROVEMENT: the large disk cache path could be human readable
    const QString artist = hasAlbumArtist() ? albumArtist()->name() : QString();
    if( artist.isEmpty() && m_name.isEmpty() )
        return QString();

    QDir largeCoverDir( Amarok::saveLocation( "albumcovers/large/" ) );
    const QString key = md5sum( artist, m_name, QString() );
        return largeCoverDir.filePath( key );
}

QString
SqlAlbum::scaledDiskCachePath( int size ) const
{
    const QByteArray widthKey = QByteArray::number( size ) + '@';
    QDir cacheCoverDir( Amarok::saveLocation( "albumcovers/cache/" ) );
    QString key = md5sum( QString(), QString(), m_imagePath );

    if( !cacheCoverDir.exists( widthKey + key ) )
    {
        // the correct location is empty
        // check deprecated locations for the image cache and delete them
        // (deleting the scaled image cache is fine)

        const QString artist = hasAlbumArtist() ? albumArtist()->name() : QString();
        if( artist.isEmpty() && m_name.isEmpty() )
            ; // do nothing special
        else
        {
            QString oldKey;
            oldKey = md5sum( artist, m_name, m_imagePath );
            if( cacheCoverDir.exists( widthKey + oldKey ) )
                cacheCoverDir.remove( widthKey + oldKey );

            oldKey = md5sum( artist, m_name, QString() );
            if( cacheCoverDir.exists( widthKey + oldKey ) )
                cacheCoverDir.remove( widthKey + oldKey );
        }
    }

    return cacheCoverDir.filePath( widthKey + key );
}

bool
SqlAlbum::hasEmbeddedImage() const
{
    if( !hasImage() )
        return false;

    return m_imagePath.startsWith( "amarok-sqltrackuid://" );
}

QImage
SqlAlbum::getEmbeddedImage() const
{
    if( !hasEmbeddedImage() )
        return QImage();

    QString query = QString( "SELECT deviceid, rpath FROM urls WHERE uniqueid = '%1';" ).arg( m_imagePath );
    QStringList result = m_collection->sqlStorage()->query( query );


    if( result.isEmpty() )
        return QImage();

    QString finalPath = m_collection->mountPointManager()->getAbsolutePath( result[0].toInt(), result[1] );

    return MetaFile::Track::getEmbeddedCover( finalPath );
}


QString
SqlAlbum::largeImagePath()
{
    // Look up in the database
    QString query = "SELECT images.id, images.path FROM images, albums WHERE albums.image = images.id AND albums.id = %1;";
    QStringList res = m_collection->sqlStorage()->query( query.arg( m_id ) );
    if( !res.isEmpty() )
    {
        m_imageId = res.at(0).toInt();
        m_imagePath = res.at(1);

        // explicitely deleted image
        if( m_imagePath == AMAROK_UNSET_MAGIC )
            return AMAROK_UNSET_MAGIC;

        // embedded image (e.g. id3v2 APIC
        if( m_imagePath.startsWith( "amarok-sqltrackuid://" ) )
            return m_imagePath;

        // normal file
        if( !m_imagePath.isEmpty() && QFile::exists( m_imagePath ) )
            return m_imagePath;
    }

    // After a rescan we currently lose all image information, so we need
    // to check that we haven't already downloaded this image before.
    m_imagePath = largeDiskCachePath();
    if( !m_imagePath.isEmpty() && QFile::exists( m_imagePath ) ) {
        setImage(m_imagePath);
        return m_imagePath;
    }

    m_imageId = -1;
    m_imagePath = QString();

    return QString();
}

void
SqlAlbum::setImage( const QString &path )
{
    DEBUG_BLOCK

    QString query = "SELECT id FROM images WHERE path = '%1'";
    query = query.arg( m_collection->sqlStorage()->escape( path ) );
    QStringList res = m_collection->sqlStorage()->query( query );

    if( res.isEmpty() )
    {
        QString insert = QString( "INSERT INTO images( path ) VALUES ( '%1' )" )
                            .arg( m_collection->sqlStorage()->escape( path ) );
        m_imageId = m_collection->sqlStorage()->insert( insert, "images" );
    }
    else
        m_imageId = res[0].toInt();

    if( m_imageId >= 0 )
    {
        query = QString("UPDATE albums SET image = %1 WHERE albums.id = %2" )
                    .arg( QString::number( m_imageId ), QString::number( m_id ) );
        m_collection->sqlStorage()->query( query );

        m_imagePath = path;
        m_hasImage = true;
        m_hasImageChecked = true;
    }
}

void
SqlAlbum::setCompilation( bool compilation )
{
    DEBUG_BLOCK
    if( isCompilation() == compilation )
    {
        return;
    }
    else
    {
        QString uidQuery = "SELECT uniqueid FROM tracks INNER JOIN urls ON tracks.url = urls.id WHERE album = %1;";
        QStringList uids = m_collection->sqlStorage()->query( uidQuery.arg( m_id ) );
        if( compilation )
        {
            // A compilation is an album where artist is NULL. Set the album's artist to NULL when the album is set to compilation.
            m_artistId = 0;
            m_artist = Meta::ArtistPtr();

            // Check to see if another album with the same name is already an collection?
            QString select = "SELECT id FROM albums WHERE name = '%1' AND id != %2 AND artist IS NULL";
            QStringList albumId = m_collection->sqlStorage()->query( select.arg( name() ).arg( m_id ) );
            if( !albumId.empty() ) {
                // Another album with the same name is already a collection, move all the tracks from the old album to the existing one and
                // delete the current one. This avoids duplicate entries in the compilation list.
                int otherId = albumId[0].toInt();
                QString update = "UPDATE tracks SET album = %1 WHERE album = %2";
                m_collection->sqlStorage()->query( update.arg( otherId ).arg( m_id ) );

                removeImage();
                QString delete_album = "DELETE FROM albums WHERE id = %1";
                m_collection->sqlStorage()->query( delete_album.arg( m_id ) );

                m_id = otherId;
            } else {
                QString update = "UPDATE albums SET artist = NULL WHERE id = %1;";
                m_collection->sqlStorage()->query( update.arg( m_id ) );
            }
        }
        else
        {
            //step1: get the actual artists per track
            QHash<int,QString> artists;
            QHash<int,int> trackToArtist;
            {
                QString trackSelect = "SELECT tracks.id, tracks.artist, artists.name "
                                      "FROM tracks INNER JOIN artists ON tracks.artist = artists.id WHERE tracks.album = %1;";
                QStringList trackSelectResult = m_collection->sqlStorage()->query( trackSelect.arg( m_id ) );

                QStringListIterator iter( trackSelectResult );
                while( iter.hasNext() )
                {
                    int track = iter.next().toInt();
                    int artist = iter.next().toInt();
                    QString artistName = iter.next();
                    trackToArtist.insert( track, artist );
                    artists.insert( artist, artistName );
                }
            }

            //step2: figure out, per artist, if an appropriate album already exists
            //taking into account A feat. B stuff
            QHash<int,int> artistToTargetAlbum;
            {
                QMultiHash<QString, int> actualNameToTrackArtistIds;
                foreach( int id, artists.keys() )
                {
                    QString name = ArtistHelper::realTrackArtist( artists.value( id ) );
                    actualNameToTrackArtistIds.insert( name, id );
                }

                //the loop below assumes two return values per row!
                QString select = "SELECT albums.id, artists.name FROM albums "
                                 "INNER JOIN artists ON albums.artist = artists.id "
                                 "WHERE albums.name = '%1' AND albums.id != %2 AND ( 0 %3 )";
                QString artistNames;
                foreach( const QString &name, actualNameToTrackArtistIds.keys() )
                {
                    QString tmp = "OR artists.name = '%1' ";
                    artistNames += tmp.arg( m_collection->sqlStorage()->escape( name ) );
                }
                QStringList data = m_collection->sqlStorage()->query( select.arg( m_collection->sqlStorage()->escape( m_name ), QString::number( m_id ), artistNames ) );

                QStringListIterator iter( data );
                while( iter.hasNext() )
                {
                    int albumId = iter.next().toInt();
                    QString artist = iter.next();
                    //lookup original artist:
                    if( actualNameToTrackArtistIds.contains( artist ) )
                    {
                        foreach( int trackArtist, actualNameToTrackArtistIds.values( artist ) )
                        {
                            artistToTargetAlbum.insert( trackArtist, albumId );
                        }
                    }
                }

            }

            //step3: create missing album entries
            //special case: if there is only one album, and it does not exist yet, change the current object instead
            bool currentObjectUpdated = false;
            foreach( int artistId, artists.keys() )
            {
                if( artistToTargetAlbum.contains( artistId ) )
                {
                    //target album for the given artist already exists, do nothing here
                    continue;
                } else if( !currentObjectUpdated ) {
                    // update this album
                    // There isn't another album with the same name and artist, just change the artist on the album
                    currentObjectUpdated = true;
                    QString update = "UPDATE albums SET artist = %1 WHERE id = %2";
                    m_collection->sqlStorage()->query( update.arg( artistId ).arg( m_id ) );
                    m_artistId = artistId;
                } else {
                    // create a new album
                    QString insert = "INSERT INTO albums (name, artist, image) VALUES ('%1',%2)";
                    SqlStorage *s = m_collection->sqlStorage();
                    int albumId = s->insert( insert.arg( s->escape( m_name ), QString::number( artistId ) ), "albums" );
                    artistToTargetAlbum.insert( artistId, albumId );
                }
            }

            //step 4: update all tracks
            foreach( int trackId, trackToArtist.keys() )
            {
                int artistId = trackToArtist.value( trackId );
                int albumId = artistToTargetAlbum.value( artistId );
                QString update = "UPDATE tracks SET album = %1 WHERE id = %2;";
                m_collection->sqlStorage()->query( update.arg( albumId ).arg( trackId ) );
            }

            //step 5: delete the original album, if necessary
            if( !currentObjectUpdated )
            {
                removeImage();
                QString del = "DELETE FROM albums WHERE id = %1;";
                m_collection->sqlStorage()->query( del.arg( m_id ) );
            }
        }

        //ensure that all currently loaded tracks will return the correct album from now on
        foreach( const QString &uid, uids )
        {
            if( m_collection->registry()->checkUidExists( uid ) )
            {
                Meta::TrackPtr track = m_collection->registry()->getTrackFromUid( uid );
                SqlTrack *sqlTrack = dynamic_cast<SqlTrack*>( track.data() );
                if( sqlTrack )
                {
                    sqlTrack->refreshFromDatabase( uid, m_collection, false ); //we will notify observers below
                }
            }
        }

        notifyObservers();
        m_collection->sendChangedSignal();
    }
}

bool
SqlAlbum::hasCapabilityInterface( Capabilities::Capability::Type type ) const
{
    return ( m_delegate ? m_delegate->hasCapabilityInterface( type, this ) : false );
}

Capabilities::Capability*
SqlAlbum::createCapabilityInterface( Capabilities::Capability::Type type )
{
    return ( m_delegate ? m_delegate->createCapabilityInterface( type, this ) : 0 );
}

//---------------SqlComposer---------------------------------

SqlComposer::SqlComposer( Collections::SqlCollection* collection, int id, const QString &name ) : Composer()
    ,m_collection( QPointer<Collections::SqlCollection>( collection ) )
    ,m_name( name )
    ,m_id( id )
    ,m_tracksLoaded( false )
    ,m_mutex( QMutex::Recursive )
{
    //nothing to do
}

void
SqlComposer::updateData( Collections::SqlCollection *collection, int id, const QString &name )
{
    m_mutex.lock();
    m_collection = QPointer<Collections::SqlCollection>( collection );
    m_id = id;
    m_name = name;
    m_mutex.unlock();
}

void
SqlComposer::invalidateCache()
{
    m_mutex.lock();
    m_tracksLoaded = false;
    m_tracks.clear();
    m_mutex.unlock();
}

TrackList
SqlComposer::tracks()
{
    QMutexLocker locker( &m_mutex );
    if( m_tracksLoaded )
    {
        return m_tracks;
    }
    else if( m_collection )
    {
        Collections::SqlQueryMaker *qm = static_cast< Collections::SqlQueryMaker* >( m_collection->queryMaker() );
        qm->setQueryType( Collections::QueryMaker::Track );
        addMatchTo( qm );
        qm->setBlocking( true );
        qm->run();
        m_tracks = qm->tracks( m_collection->collectionId() );
        delete qm;
        m_tracksLoaded = true;
        return m_tracks;
    }
    else
        return TrackList();
}

//---------------SqlGenre---------------------------------

SqlGenre::SqlGenre( Collections::SqlCollection* collection, int id, const QString &name ) : Genre()
    ,m_collection( QPointer<Collections::SqlCollection>( collection ) )
    ,m_name( name )
    ,m_id( id )
    ,m_tracksLoaded( false )
    ,m_mutex( QMutex::Recursive )
{
    //nothing to do
}

void
SqlGenre::updateData( Collections::SqlCollection *collection, int id, const QString &name )
{
    m_mutex.lock();
    m_collection = QPointer<Collections::SqlCollection>( collection );
    m_id = id;
    m_name = name;
    m_mutex.unlock();
}

void
SqlGenre::invalidateCache()
{
    m_mutex.lock();
    m_tracksLoaded = false;
    m_tracks.clear();
    m_mutex.unlock();
}

TrackList
SqlGenre::tracks()
{
    QMutexLocker locker( &m_mutex );
    if( m_tracksLoaded )
    {
        return m_tracks;
    }
    else if( m_collection )
    {
        Collections::SqlQueryMaker *qm = static_cast< Collections::SqlQueryMaker* >( m_collection->queryMaker() );
        qm->setQueryType( Collections::QueryMaker::Track );
        addMatchTo( qm );
        qm->setBlocking( true );
        qm->run();
        m_tracks = qm->tracks( m_collection->collectionId() );
        delete qm;
        m_tracksLoaded = true;
        return m_tracks;
    }
    else
        return TrackList();
}

//---------------SqlYear---------------------------------

SqlYear::SqlYear( Collections::SqlCollection* collection, int id, const QString &name ) : Year()
    ,m_collection( QPointer<Collections::SqlCollection>( collection ) )
    ,m_name( name )
    ,m_id( id )
    ,m_tracksLoaded( false )
    ,m_mutex( QMutex::Recursive )
{
    //nothing to do
}

void
SqlYear::updateData( Collections::SqlCollection *collection, int id, const QString &name )
{
    m_mutex.lock();
    m_collection = QPointer<Collections::SqlCollection>( collection );
    m_id = id;
    m_name = name;
    m_mutex.unlock();
}

void
SqlYear::invalidateCache()
{
    m_mutex.lock();
    m_tracksLoaded = false;
    m_tracks.clear();
    m_mutex.unlock();
}

TrackList
SqlYear::tracks()
{
    QMutexLocker locker( &m_mutex );
    if( m_tracksLoaded )
    {
        return m_tracks;
    }
    else if( m_collection )
    {
        Collections::SqlQueryMaker *qm = static_cast< Collections::SqlQueryMaker* >( m_collection->queryMaker() );
        qm->setQueryType( Collections::QueryMaker::Track );
        addMatchTo( qm );
        qm->setBlocking( true );
        qm->run();
        m_tracks = qm->tracks( m_collection->collectionId() );
        delete qm;
        m_tracksLoaded = true;
        return m_tracks;
    }
    else
        return TrackList();
}

//---------------SqlLabel---------------------------------

SqlLabel::SqlLabel( Collections::SqlCollection *collection, int id, const QString &name ) : Meta::Label()
    ,m_collection( QPointer<Collections::SqlCollection>( collection ) )
    ,m_name( name )
    ,m_id( id )
    ,m_tracksLoaded( false )
    ,m_mutex( QMutex::Recursive )
{
    //nothing to do
}

void
SqlLabel::invalidateCache()
{
    m_mutex.lock();
    m_tracksLoaded = false;
    m_tracks.clear();
    m_mutex.unlock();
}

TrackList
SqlLabel::tracks()
{
    QMutexLocker locker( &m_mutex );
    if( m_tracksLoaded )
    {
        return m_tracks;
    }
    else if( m_collection )
    {
        Collections::SqlQueryMaker *qm = static_cast< Collections::SqlQueryMaker* >( m_collection->queryMaker() );
        qm->setQueryType( Collections::QueryMaker::Track );
        addMatchTo( qm );
        qm->setBlocking( true );
        qm->run();
        m_tracks = qm->tracks();
        delete qm;
        m_tracksLoaded = true;
        return m_tracks;
    }
    else
        return TrackList();
}

