/****************************************************************************************
 * Copyright (c) 2007 Bart Cerneels <bart.cerneels@kde.org>                             *
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

#include "core-impl/playlists/types/file/m3u/M3UPlaylist.h"

#define _PREFIX "M3UPlaylist"

#include "core/support/Amarok.h"
#include "core-impl/collections/support/CollectionManager.h"
#include "core/support/Debug.h"
#include "PlaylistManager.h"
#include "core-impl/playlists/types/file/PlaylistFileSupport.h"

#include <KMimeType>
#include <KUrl>

#include <QFile>
#include <QFileInfo>
#include <QTextStream>

namespace Playlists {

M3UPlaylist::M3UPlaylist()
    : m_url( Playlists::newPlaylistFilePath( "m3u" ) )
    , m_tracksLoaded( true )
{
    m_name = m_url.fileName();
}

M3UPlaylist::M3UPlaylist( Meta::TrackList tracks )
    : m_url( Playlists::newPlaylistFilePath( "m3u" ) )
    , m_tracksLoaded( true )
    , m_tracks( tracks )
{
    m_name = m_url.fileName();
}

M3UPlaylist::M3UPlaylist( const KUrl &url )
    : m_url( url )
    , m_tracksLoaded( false )
{
    m_name = m_url.fileName();
}

M3UPlaylist::~M3UPlaylist()
{
}

QString
M3UPlaylist::description() const
{
    KMimeType::Ptr mimeType = KMimeType::mimeType( "audio/x-mpegurl" );
    return QString( "%1 (%2)").arg( mimeType->name(), "m3u" );
}

int
M3UPlaylist::trackCount() const
{
    if( m_tracksLoaded )
        return m_tracks.count();

    //TODO: count the number of lines starting with #
    return -1;
}

Meta::TrackList
M3UPlaylist::tracks()
{
    return m_tracks;
}

void
M3UPlaylist::triggerTrackLoad()
{
    //TODO make sure we've got all tracks first.
    if( m_tracksLoaded )
        return;

    //check if file is local or remote
    if( m_url.isLocalFile() )
    {
        QFile file( m_url.toLocalFile() );
        if( !file.open( QIODevice::ReadOnly ) )
        {
            error() << "cannot open file";
            return;
        }

        QString contents( file.readAll() );
        file.close();

        QTextStream stream;
        stream.setString( &contents );
        loadM3u( stream );
        m_tracksLoaded = true;
    }
    else
    {
        The::playlistManager()->downloadPlaylist( m_url, PlaylistFilePtr( this ) );
    }
}

bool
M3UPlaylist::loadM3u( QTextStream &stream )
{
    const QString directory = m_url.directory();
    bool hasTracks = false;
    m_tracksLoaded = false;

    do
    {
        QString line = stream.readLine();
        if( line.startsWith( "#EXTINF" ) )
        {
            //const QString extinf = line.section( ':', 1 );
            //const int length = extinf.section( ',', 0, 0 ).toInt();
        }
        else if( !line.startsWith( '#' ) && !line.isEmpty() )
        {
            Meta::TrackPtr trackPtr;
            line = line.replace( "\\", "/" );

            // KUrl::isRelativeUrl() expects absolute URLs to start with a protocol, so prepend it if missing
            QString url = line;
            if( url.startsWith( '/' ) )
                url.prepend( "file://" );
            // Won't be relative if it begins with a /
            // Also won't be windows url, so no need to worry about swapping \ for /
            if( KUrl::isRelativeUrl( url ) )
            {
                //Replace \ with / for windows playlists
                line.replace('\\','/');
                KUrl kurl( directory );
                kurl.addPath( line ); // adds directory separator if required
                kurl.cleanPath();

                trackPtr = CollectionManager::instance()->trackForUrl( kurl );
            }
            else
            {
                trackPtr = CollectionManager::instance()->trackForUrl( KUrl( line ) );
            }

            if( trackPtr )
            {
                m_tracks.append( trackPtr );
                hasTracks = true;
                m_tracksLoaded = true;
            }
        }
    } while( !stream.atEnd() );
    return hasTracks;
}

bool
M3UPlaylist::save( const KUrl &location, bool relative )
{
    KUrl savePath = location;
    //if the location is a directory append the name of this playlist.
    if( savePath.fileName().isNull() )
        savePath.setFileName( name() );

    QFile file( savePath.path() );

    if( !file.open( QIODevice::WriteOnly ) )
    {
        error() << "Unable to write to playlist " << savePath.path();
        return false;
    }

    QTextStream stream( &file );

    stream << "#EXTM3U\n";

    KUrl::List urls;
    QStringList titles;
    QList<int> lengths;
    foreach( Meta::TrackPtr track, m_tracks )
    {
        Q_ASSERT(track);

        const KUrl &url = track->playableUrl();
        int length = track->length() / 1000;
        const QString &title = track->name();
        const QString &artist = track->artist()->name();

        if( !title.isEmpty() && !artist.isEmpty() && length )
        {
            stream << "#EXTINF:";
            stream << QString::number( length );
            stream << ',';
            stream << artist << " - " << title;
            stream << '\n';
        }
        if( url.protocol() == "file" )
        {
            if( relative )
            {
                const QFileInfo fi( file );
                QString relativePath = KUrl::relativePath( fi.path(), url.path() );
                relativePath.remove( 0, 2 ); //remove "./"
                stream << relativePath;
            }
            else
            {
                stream << url.path();
            }
        }
        else
        {
            stream << url.url();
        }
        stream << "\n";
    }

    return true;
}

bool
M3UPlaylist::isWritable()
{
    if( m_url.isEmpty() )
        return false;

    return QFileInfo( m_url.path() ).isWritable();
}

void
M3UPlaylist::setName( const QString &name )
{
    m_url.setFileName( name );
}

} //namespace Playlists

