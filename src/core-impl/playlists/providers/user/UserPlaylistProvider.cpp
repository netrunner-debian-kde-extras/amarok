/****************************************************************************************
 * Copyright (c) 2009 Alejandro Wainzinger <aikawarazuni@gmail.com>                     *
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

#include "core-impl/playlists/providers/user/UserPlaylistProvider.h"

Playlists::UserPlaylistProvider::~UserPlaylistProvider()
{
}

int
Playlists::UserPlaylistProvider::category() const
{
     return Playlists::UserPlaylist;
}

bool
Playlists::UserPlaylistProvider::supportsEmptyGroups()
{
    return false;
}

QList<QAction *>
Playlists::UserPlaylistProvider::playlistActions( Playlists::PlaylistPtr playlist )
{
    Q_UNUSED( playlist );
    return QList<QAction *>();
}

QList<QAction *>
Playlists::UserPlaylistProvider::trackActions( Playlists::PlaylistPtr playlist, int trackIndex )
{
    Q_UNUSED( playlist );
    Q_UNUSED( trackIndex );
    return QList<QAction *>();
}

#include "UserPlaylistProvider.moc"
