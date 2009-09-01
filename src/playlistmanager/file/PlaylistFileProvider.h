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
 * PARTICULAR PURPOSE. See the GNU General Public License for more details.              *
 *                                                                                      *
 * You should have received a copy of the GNU General Public License along with         *
 * this program.  If not, see <http://www.gnu.org/licenses/>.                           *
 ****************************************************************************************/

#ifndef PLAYLISTFILEPROVIDER_H
#define PLAYLISTFILEPROVIDER_H

#include <UserPlaylistProvider.h>
#include <PlaylistFileSupport.h>

#include <kicon.h>

class KConfigGroup;
class KUrl;

/**
    @author Bart Cerneels <bart.cerneels@kde.org>
*/
class PlaylistFileProvider : public UserPlaylistProvider
{
    Q_OBJECT

    public:
        PlaylistFileProvider();
        virtual ~PlaylistFileProvider();

        virtual QString prettyName() const;
        virtual KIcon icon() const { return KIcon( "folder-documents" ); }

        virtual int category() const { return PlaylistManager::UserPlaylist; }

        virtual Meta::PlaylistList playlists();

        virtual QList<QAction *> playlistActions( Meta::PlaylistPtr playlist );
        virtual QList<QAction *> trackActions( Meta::PlaylistPtr playlist,
                                                  int trackIndex );

        virtual bool canSavePlaylists() { return true; }

        virtual Meta::PlaylistPtr save( const Meta::TrackList &tracks );
        virtual Meta::PlaylistPtr save( const Meta::TrackList &tracks,
                                        const QString &name );

        virtual bool import( const KUrl &path );

        virtual bool isWritable() { return true; }
        virtual void rename( Meta::PlaylistPtr playlist, const QString &newName );
        virtual void deletePlaylists( Meta::PlaylistList playlistList );

    signals:
            virtual void updated();

    private:
        KConfigGroup loadedPlaylistsConfig();

        Meta::PlaylistList m_playlists;
        Meta::PlaylistFormat m_defaultFormat;
        QMultiMap<QString, Meta::PlaylistPtr> m_groupMap;
};

#endif
