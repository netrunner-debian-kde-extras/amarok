/****************************************************************************************
 * Copyright (c) 2010 Bart Cerneels <bart.cerneels@kde.org>                             *
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

#ifndef SYNCEDPLAYLIST_H
#define SYNCEDPLAYLIST_H

#include <src/core/playlists/Playlist.h>

class SyncedPlaylist : public Playlists::Playlist, public Playlists::PlaylistObserver
{
    public:
        SyncedPlaylist();
        SyncedPlaylist( Playlists::PlaylistPtr playlist );
        virtual ~SyncedPlaylist() {}

        //Playlists::Playlist methods
        virtual KUrl uidUrl() const;
        virtual QString name() const;
        virtual QString prettyName() const;
        virtual QString description() const;
        virtual void setName( const QString &name ) { Q_UNUSED( name ); }

        virtual Meta::TrackList tracks();

        virtual void addTrack( Meta::TrackPtr track, int position = -1 );
        virtual void removeTrack( int position );

        virtual QActionList actions();
        virtual QActionList trackActions( int trackIndex );

        //PlaylistObserver methods
        virtual void trackAdded( Playlists::PlaylistPtr playlist, Meta::TrackPtr track,
                                 int position );
        virtual void trackRemoved( Playlists::PlaylistPtr playlist, int position );

        //SyncedPlaylist methods
        /** returns true when there is no active playlist associated with it anymore. */
        virtual bool isEmpty() const;
        virtual void addPlaylist( Playlists::PlaylistPtr playlist );

        virtual bool syncNeeded() const;
        virtual void doSync() const;

        virtual void removePlaylistsFrom( Playlists::PlaylistProvider *provider );

        virtual Playlists::PlaylistList playlists() const { return m_playlists; }

    protected:

    private:
        Playlists::PlaylistList m_playlists;
};

typedef KSharedPtr<SyncedPlaylist> SyncedPlaylistPtr;
typedef QList<SyncedPlaylistPtr> SyncedPlaylistList;

Q_DECLARE_METATYPE( SyncedPlaylistPtr )
Q_DECLARE_METATYPE( SyncedPlaylistList )

#endif // SYNCEDPLAYLIST_H