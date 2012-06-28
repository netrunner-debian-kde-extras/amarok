/****************************************************************************************
 * Copyright (c) 2007 Maximilian Kossick <maximilian.kossick@googlemail.com>            *
 * Copyright (c) 2008 Seb Ruiz <ruiz@kde.org>                                           *
 * Copyright (c) 2009-2010 Jeff Mitchell <mitchell@kde.org>                             *
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

#define DEBUG_PREFIX "ScanResultProcessor"

#include "ScanResultProcessor.h"
#include "core/support/Debug.h"
#include "playlistmanager/PlaylistManager.h"
#include "MountPointManager.h"

// include files from the collection scanner utility
#include <collectionscanner/Directory.h>
#include <collectionscanner/Album.h>
#include <collectionscanner/Track.h>
#include <collectionscanner/Playlist.h>

ScanResultProcessor::ScanResultProcessor( QObject *parent )
    : QObject( parent )
    , m_type( PartialUpdateScan )
{
}

ScanResultProcessor::~ScanResultProcessor()
{
    foreach( CollectionScanner::Directory *dir, m_directories )
        delete dir;

    QHash<QString, CollectionScanner::Album*>::const_iterator i = m_albumNames.constBegin();
    while (i != m_albumNames.constEnd()) {
        delete i.value();
        ++i;
    }
 }

#include <core-impl/collections/support/ArtistHelper.h>

void
ScanResultProcessor::addDirectory( CollectionScanner::Directory *dir )
{
    m_directories.append( dir );
}

void
ScanResultProcessor::commit()
{
    // the default for albums with several artists is that it's a compilation
    // however, some album names are unlikely to be a compilation
    static QStringList nonCompilationAlbumNames;
    if( nonCompilationAlbumNames.isEmpty() )
    {
        nonCompilationAlbumNames
            << "" // don't throw together albums without name. At least not here
            << "Best Of"
            << "Anthology"
            << "Hit collection"
            << "Greatest Hits"
            << "All Time Greatest Hits"
            << "Live";
    }

    // we are blocking the updated signal for maximum of one second.
    QDateTime blockedTime = QDateTime::currentDateTime();
    blockUpdates();

    // -- commit the directories
    foreach( CollectionScanner::Directory* dir, m_directories )
    {
        commitDirectory( dir );

        // -- sort the tracks into albums
        QSet<CollectionScanner::Album*> dirAlbums;
        QSet<QString> dirAlbumNames;
        QList<CollectionScanner::Track*> tracks = dir->tracks();

        for( int i = tracks.count() - 1; i >= 0; --i )
        {
            CollectionScanner::Album *album = sortTrack( tracks.at( i ) );
            if( album )
            {
                dirAlbums.insert( album );
                dirAlbumNames.insert( album->name() );
                tracks.removeAt( i );
            }
        }

        // -- sort the remainder
        if( dirAlbums.count() == 0 )
        {
            // -- use the directory name as album name
            QString dirAlbumName = QDir( dir->path() ).dirName();
            for( int i = tracks.count() - 1; i >= 0; --i )
            {
                CollectionScanner::Album *album = sortTrack( tracks.at( i ), dirAlbumName, QString() );
                if( album )
                {
                    dirAlbums.insert( album );
                    dirAlbumNames.insert( album->name() );
                    tracks.removeAt( i );
                }
            }
        }
        else
        {
            // -- put into the empty album
            for( int i = tracks.count() - 1; i >= 0; --i )
            {
                CollectionScanner::Album *album = sortTrack( tracks.at( i ), QString(), QString() );
                if( album )
                {
                    dirAlbums.insert( album );
                    dirAlbumNames.insert( album->name() );
                    tracks.removeAt( i );
                }
            }
        }

        // if all the tracks from this directory end up in one album
        // (or they have at least the same name) then it's likely that an image
        // from this directory could be a cover
        if( dirAlbumNames.count() == 1 )
            (*dirAlbums.begin())->setCovers( dir->covers() );
    }


    // --- add all albums
    QList<QString> keys = m_albumNames.uniqueKeys();
    foreach( const QString &key, keys )
    {
        // --- commit the albums as compilation or normal album

        QList<CollectionScanner::Album*> albums = m_albumNames.values( key );
        // debug() << "commit got" <<albums.count() << "x" << key;

        // if we have multiple albums with the same name, check if it
        // might be a compilation

        for( int i = albums.count() - 1; i >= 0; --i )
        {
            CollectionScanner::Album *album = albums.at( i );
            // commit all albums with a track with the noCompilation flag
            if( album->isNoCompilation() ||
                nonCompilationAlbumNames.contains( album->name(), Qt::CaseInsensitive ) )
                commitAlbum( albums.takeAt( i ) );
        }

        // only one album left. It's no compilation.
        if( albums.count() == 1 )
        {
            commitAlbum( albums.takeFirst() );
        }

        // compilation
        else if( albums.count() > 1 )
        {
            CollectionScanner::Album compilation( key, QString() );
            for( int i = albums.count() - 1; i >= 0; --i )
            {
                CollectionScanner::Album *album = albums.takeAt( i );
                foreach( CollectionScanner::Track *track, album->tracks() )
                    compilation.addTrack( track );
                compilation.setCovers( album->covers() + compilation.covers() );
            }
            commitAlbum( &compilation );
        }

        // --- unblock every 5 second. Maybe not really needed, but still nice
        if( blockedTime.secsTo( QDateTime::currentDateTime() ) >= 5 )
        {
            unblockUpdates();
            blockedTime = QDateTime::currentDateTime();
            blockUpdates();
        }
    }

    // -- now check if some of the tracks are not longer used and also not moved to another directory
    foreach( CollectionScanner::Directory* dir, m_directories )
        if( !dir->isSkipped() )
            deleteDeletedTracks( dir );

    // -- delete all not-found directories
    if( m_type != PartialUpdateScan )
        deleteDeletedDirectories();

    unblockUpdates();
}

void
ScanResultProcessor::rollback()
{
    // nothing to do
}

QStringList
ScanResultProcessor::getLastErrors() const
{
    return m_lastErrors;
}

void
ScanResultProcessor::clearLastErrors()
{
    m_lastErrors.clear();
}

void
ScanResultProcessor::commitDirectory( CollectionScanner::Directory *directory )
{
    if( directory->path().isEmpty() )
    {
        warning() << "got directory with no path from the scanner, not adding";
        return;
    }

    if( directory->isSkipped() )
    {
        emit directorySkipped();
        return;
    }

    // --- add all playlists
    foreach( CollectionScanner::Playlist playlist, directory->playlists() )
        commitPlaylist( &playlist );

    emit directoryCommitted();
}

void
ScanResultProcessor::commitPlaylist( CollectionScanner::Playlist *playlist )
{
    // debug() << "commitPlaylist on " << playlist->path();

    if( The::playlistManager() )
        The::playlistManager()->import( "file:"+playlist->path() );
}

CollectionScanner::Album*
ScanResultProcessor::sortTrack( CollectionScanner::Track *track )
{
    QString albumArtist = ArtistHelper::bestGuessAlbumArtist( track->albumArtist(),
        track->artist(), track->genre(), track->composer() );

    if( track->album().isEmpty() && albumArtist.isEmpty() )
        return 0;  // unknown album from various artists
    return sortTrack( track, track->album(), albumArtist );
}

/** This will just put the tracks into an album.
    @param album the name of the target album
    @returns true if the track was put into an album
*/
CollectionScanner::Album*
ScanResultProcessor::sortTrack( CollectionScanner::Track *track,
                                const QString &albumName,
                                QString albumArtist )
{
    // we allow albums with empty name and nonepty artist, see bug 272471
    if( track->isCompilation() )
        albumArtist.clear();  // no album artist denotes a compilation

    AlbumKey key( albumName, albumArtist );

    CollectionScanner::Album *album;
    if( m_albums.contains( key ) )
        album = m_albums.value( key );
    else
    {
        album = new CollectionScanner::Album( albumName, albumArtist );
        m_albums.insert( key, album );
        m_albumNames.insert( albumName, album );
    }

    album->addTrack( track );
    return album;
}


#include "ScanResultProcessor.moc"

