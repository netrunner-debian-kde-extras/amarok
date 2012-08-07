/****************************************************************************************
 * Copyright (c) 2009-2011 Bart Cerneels <bart.cerneels@kde.org>                        *
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

#ifndef METAPLAYLISTFILE_H
#define METAPLAYLISTFILE_H

#include "core/playlists/Playlist.h"
#include "core/meta/Meta.h"

class PlaylistProvider;

namespace Playlists
{

    class PlaylistFile;

    typedef KSharedPtr<PlaylistFile> PlaylistFilePtr;
    typedef QList<PlaylistFilePtr> PlaylistFileList;

    /**
     * Base class for all playlist files
     *
     **/
    class AMAROK_EXPORT PlaylistFile : public Playlist
    {
        public:
            PlaylistFile() : Playlist(), m_provider( 0 ) {}
            virtual ~PlaylistFile() {}

            virtual bool isWritable() { return false; }

            virtual bool save( const KUrl &url, bool relative )
                { Q_UNUSED( url ); Q_UNUSED( relative ); return false; }

            /** Loads the playlist from the stream adding the newly found tracks to the current playlist.
                This function is called automatically if the playlist is created with a file.
                It only needs to be called when the playlist object is created from an url.
                @returns true if the loading was successful.
            */
            virtual bool load( QTextStream &stream ) { Q_UNUSED( stream ); return false; }

            virtual QList<int> queue() { return QList<int>(); }
            virtual void setQueue( const QList<int> &rows ) { Q_UNUSED( rows ); }

            virtual void setName( const QString &name ) = 0;
            virtual void setGroups( const QStringList &groups ) { m_groups = groups; }
            virtual QStringList groups() { return m_groups; }

            //default implementation prevents crashes related to PlaylistFileProvider
            virtual void setProvider( PlaylistProvider *provider ) { m_provider = provider; }

            /* Playlist Methods */
            virtual PlaylistProvider *provider() const { return m_provider; }

        protected:
            /** Schedule this playlist file to be saved on the next iteration of the mainloop.
              * Useful in addTrack() and removeTrack() functions.
              */
            void saveLater();

            PlaylistProvider *m_provider;
            QStringList m_groups;
    };

}

Q_DECLARE_METATYPE( Playlists::PlaylistFilePtr )
Q_DECLARE_METATYPE( Playlists::PlaylistFileList )

#endif
