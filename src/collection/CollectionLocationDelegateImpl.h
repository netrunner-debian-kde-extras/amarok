/****************************************************************************************
 * Copyright (c) 2010 Maximilian Kossick <maximilian.kossick@googlemail.com>            *
 * Copyright (c) 2010 Casey Link <unnamedrambler@gmail.com>                             *
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

#ifndef COLLECTIONLOCATIONDELEGATEIMPL_H
#define COLLECTIONLOCATIONDELEGATEIMPL_H

#include "amarok_export.h"
#include "collection/CollectionLocationDelegate.h"

class AMAROK_EXPORT CollectionLocationDelegateImpl : public CollectionLocationDelegate
{
public:
    CollectionLocationDelegateImpl() {};
    virtual ~ CollectionLocationDelegateImpl() {};

    virtual bool reallyDelete( CollectionLocation *loc, const Meta::TrackList &tracks ) const;
    virtual bool reallyMove(CollectionLocation* loc, const Meta::TrackList& tracks) const;
    virtual void errorDeleting( CollectionLocation* loc, const Meta::TrackList& tracks ) const;
    virtual void notWriteable(CollectionLocation* loc) const;

 private:
    /**
     * Builds a string of the format "ARTIST - TITLE",
     * for media device tracks.
     * @param track Pointer of the Meta::Track item
     * @return String with artist and title
     */
    virtual QString realTrackName( const Meta::TrackPtr track ) const;
};


#endif
