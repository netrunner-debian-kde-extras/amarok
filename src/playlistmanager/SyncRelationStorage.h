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

#ifndef SYNCRELATIONSTORAGE_H
#define SYNCRELATIONSTORAGE_H

#include "core/playlists/Playlist.h"

#include "SyncedPlaylist.h"

class SyncRelationStorage
{
public:
    SyncRelationStorage();
    virtual ~SyncRelationStorage() {}

    virtual bool shouldBeSynced( Playlists::PlaylistPtr playlist ) = 0;
    virtual SyncedPlaylistPtr asSyncedPlaylist( Playlists::PlaylistPtr playlist ) = 0;
};

#endif // SYNCRELATIONSTORAGE_H
