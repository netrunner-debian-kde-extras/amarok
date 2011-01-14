/****************************************************************************************
 * Copyright (c) 2009 Maximilian Kossick <maximilian.kossick@googlemail.com>            *
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

#ifndef CAPABILITYDELEGATEIMPL_H
#define CAPABILITYDELEGATEIMPL_H

#include "CapabilityDelegate.h"
#include "amarok_sqlcollection_export.h"
#include "core/capabilities/Capability.h"

namespace Collections {
    class SqlCollection;
}

namespace Capabilities {

class TrackCapabilityDelegateImpl : public TrackCapabilityDelegate
{
public:
    TrackCapabilityDelegateImpl();
    virtual ~ TrackCapabilityDelegateImpl() {};

    virtual bool hasCapabilityInterface( Capabilities::Capability::Type type, const Meta::SqlTrack *track ) const;
    virtual Capabilities::Capability* createCapabilityInterface( Capabilities::Capability::Type type, Meta::SqlTrack *track );
};

class ArtistCapabilityDelegateImpl : public ArtistCapabilityDelegate
{
public:
    ArtistCapabilityDelegateImpl();
    virtual ~ArtistCapabilityDelegateImpl();

    virtual bool hasCapabilityInterface( Capabilities::Capability::Type type, const Meta::SqlArtist *artist ) const;
    virtual Capabilities::Capability* createCapabilityInterface( Capabilities::Capability::Type type, Meta::SqlArtist *artist );

};

class AlbumCapabilityDelegateImpl : public AlbumCapabilityDelegate
{
public:
    AlbumCapabilityDelegateImpl();
    virtual ~AlbumCapabilityDelegateImpl();

    virtual bool hasCapabilityInterface( Capabilities::Capability::Type type, const Meta::SqlAlbum *album ) const;
    virtual Capabilities::Capability* createCapabilityInterface( Capabilities::Capability::Type type, Meta::SqlAlbum *album );

};

} //namespace Capabilities

#endif // CAPABILITYDELEGATEIMPL_H
