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

#include "core-impl/playlists/types/file/pls/PLSPlaylist.h"

#include "core-impl/collections/support/CollectionManager.h"
#include "core/support/Debug.h"
#include "core/capabilities/EditCapability.h"
#include "core/meta/Meta.h"
#include "PlaylistManager.h"
#include "core-impl/playlists/types/file/PlaylistFileSupport.h"

#include <KMimeType>
#include <KLocale>

#include <QTextStream>
#include <QRegExp>
#include <QString>
#include <QFile>

namespace Playlists {

PLSPlaylist::PLSPlaylist()
    : m_url( Playlists::newPlaylistFilePath( "pls" ) )
    , m_tracksLoaded( true )
{
    m_name = m_url.fileName();
}

PLSPlaylist::PLSPlaylist( Meta::TrackList tracks )
    : m_url( Playlists::newPlaylistFilePath( "pls" ) )
    , m_tracksLoaded( true )
    , m_tracks( tracks )
{
    m_name = m_url.fileName();
}

PLSPlaylist::PLSPlaylist( const KUrl &url )
    : m_url( url )
    , m_tracksLoaded( false )
{
    m_name = m_url.fileName();
}

PLSPlaylist::~PLSPlaylist()
{
}

QString
PLSPlaylist::description() const
{
    KMimeType::Ptr mimeType = KMimeType::mimeType( "audio/x-scpls" );
    return QString( "%1 (%2)").arg( mimeType->name(), "pls" );
}

int
PLSPlaylist::trackCount() const
{
    if( m_tracksLoaded )
        return m_tracks.count();

    //TODO: count the number of lines starting with #
    return -1;
}

Meta::TrackList
PLSPlaylist::tracks()
{
    return m_tracks;
}

void
PLSPlaylist::triggerTrackLoad()
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
        loadPls( stream );
        m_tracksLoaded = true;
    }
    else
    {
        The::playlistManager()->downloadPlaylist( m_url, PlaylistFilePtr( this ) );
    }
}

bool
PLSPlaylist::loadPls( QTextStream &stream )
{
    DEBUG_BLOCK

    Meta::TrackPtr currentTrack;

    // Counted number of "File#=" lines.
    unsigned int entryCnt = 0;
    // Value of the "NumberOfEntries=#" line.
    unsigned int numberOfEntries = 0;
    // Does the file have a "[playlist]" section? (as it's required by the standard)
    bool havePlaylistSection = false;
    QString tmp;
    QStringList lines;

    const QRegExp regExp_NumberOfEntries("^NumberOfEntries\\s*=\\s*\\d+$");
    const QRegExp regExp_File("^File\\d+\\s*=");
    const QRegExp regExp_Title("^Title\\d+\\s*=");
    const QRegExp regExp_Length("^Length\\d+\\s*=\\s*\\d+$");
    const QRegExp regExp_Version("^Version\\s*=\\s*\\d+$");
    const QString section_playlist("[playlist]");

    /* Preprocess the input data.
    * Read the lines into a buffer; Cleanup the line strings;
    * Count the entries manually and read "NumberOfEntries".
    */
    while (!stream.atEnd()) {
        tmp = stream.readLine();
        tmp = tmp.trimmed();
        if (tmp.isEmpty())
            continue;
        lines.append(tmp);

        if (tmp.contains(regExp_File)) {
            entryCnt++;
            continue;
        }
        if (tmp == section_playlist) {
            havePlaylistSection = true;
            continue;
        }
        if (tmp.contains(regExp_NumberOfEntries)) {
            numberOfEntries = tmp.section('=', -1).trimmed().toUInt();
            continue;
        }
    }
    if (numberOfEntries != entryCnt) {
        warning() << ".pls playlist: Invalid \"NumberOfEntries\" value.  "
                << "NumberOfEntries=" << numberOfEntries << "  counted="
                << entryCnt << endl;
        /* Corrupt file. The "NumberOfEntries" value is
        * not correct. Fix it by setting it to the manually
        * counted number and go on parsing.
        */
        numberOfEntries = entryCnt;
    }
    if (!numberOfEntries)
        return true;

    unsigned int index;
    bool ok = false;
    bool inPlaylistSection = false;

    /* Now iterate through all beautified lines in the buffer
    * and parse the playlist data.
    */
    QStringList::const_iterator i = lines.constBegin(), end = lines.constEnd();
    for ( ; i != end; ++i) {
        if (!inPlaylistSection && havePlaylistSection) {
            /* The playlist begins with the "[playlist]" tag.
            * Skip everything before this.
            */
            if ((*i) == section_playlist)
                inPlaylistSection = true;
            continue;
        }
        if ((*i).contains(regExp_File)) {
            // Have a "File#=XYZ" line.
            index = loadPls_extractIndex(*i);
            if (index > numberOfEntries || index == 0)
                continue;
            tmp = (*i).section('=', 1).trimmed();
            currentTrack = CollectionManager::instance()->trackForUrl( tmp );
            if( currentTrack.isNull() )
            {
                debug() << "track could not be loaded: " << tmp;
                continue;
            }
            m_tracks.append( currentTrack );
            continue;
        }
        if ((*i).contains(regExp_Title)) {
            // Have a "Title#=XYZ" line.
            index = loadPls_extractIndex(*i);
            if (index > numberOfEntries || index == 0)
                continue;
            tmp = (*i).section('=', 1).trimmed();

            if ( currentTrack.data() != 0 && currentTrack->is<Capabilities::EditCapability>() )
            {
                Capabilities::EditCapability *ec = currentTrack->create<Capabilities::EditCapability>();
                if( ec )
                    ec->setTitle( tmp );
                delete ec;
            }
            continue;
        }
        if ((*i).contains(regExp_Length)) {
            // Have a "Length#=XYZ" line.
            index = loadPls_extractIndex(*i);
            if (index > numberOfEntries || index == 0)
                continue;
            tmp = (*i).section('=', 1).trimmed();
            //tracks.append( KUrl(tmp) );
//             Q_ASSERT(ok);
            continue;
        }
        if ((*i).contains(regExp_NumberOfEntries)) {
            // Have the "NumberOfEntries=#" line.
            continue;
        }
        if ((*i).contains(regExp_Version)) {
            // Have the "Version=#" line.
            tmp = (*i).section('=', 1).trimmed();
            // We only support Version=2
            if (tmp.toUInt(&ok) != 2)
                warning() << ".pls playlist: Unsupported version." << endl;
//             Q_ASSERT(ok);
            continue;
        }
        warning() << ".pls playlist: Unrecognized line: \"" << *i << "\"" << endl;
    }

    return true;
}

bool
PLSPlaylist::save( const KUrl &location, bool relative )
{
    Q_UNUSED( relative );

    KUrl savePath = location;
    //if the location is a directory append the name of this playlist.
    if( savePath.fileName().isNull() )
        savePath.setFileName( name() );

    QFile file( savePath.path() );

    if( !file.open( QIODevice::WriteOnly ) )
    {
        debug() << "Unable to write to playlist " << savePath.path();
        return false;
    }

    QTextStream stream( &file );
    stream << "[Playlist]\n";
    stream << "NumberOfEntries=" << m_tracks.count() << endl;
    int i = 1; //PLS starts at File1=
    foreach( Meta::TrackPtr track, m_tracks )
    {
        stream << "File" << i << "=";
        stream << KUrl( track->playableUrl() ).path();
        stream << "\nTitle" << i << "=";
        stream << track->name();
        stream << "\nLength" << i << "=";
        stream << track->length() / 1000;
        stream << "\n";
        i++;
    }

    stream << "Version=2\n";
    file.close();
    return true;
}

unsigned int
PLSPlaylist::loadPls_extractIndex( const QString &str ) const
{
    /* Extract the index number out of a .pls line.
     * Example:
    *   loadPls_extractIndex("File2=foobar") == 2
    */
    bool ok = false;
    unsigned int ret;
    QString tmp(str.section('=', 0, 0));
    tmp.remove(QRegExp("^\\D*"));
    ret = tmp.trimmed().toUInt(&ok);
    Q_ASSERT(ok);
    return ret;
}

bool
PLSPlaylist::isWritable()
{
    if( m_url.isEmpty() )
        return false;

    return QFileInfo( m_url.path() ).isWritable();
}

void
PLSPlaylist::setName( const QString &name )
{
    m_url.setFileName( name );
}

}
