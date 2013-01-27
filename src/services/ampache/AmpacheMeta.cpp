/****************************************************************************************
 * Copyright (c) 2007 Casey Link <unnamedrambler@gmail.com>                             *
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

#include "AmpacheMeta.h"
#include "core/support/Debug.h"

#include <Solid/Networking>

using namespace Meta;

//// AmpacheAlbum ////

AmpacheAlbum::AmpacheAlbum( const QString &name )
    : ServiceAlbumWithCover( name )
{}

AmpacheAlbum::AmpacheAlbum(const QStringList & resultRow)
    : ServiceAlbumWithCover( resultRow )
{}

AmpacheAlbum::~ AmpacheAlbum()
{}

void AmpacheAlbum::setCoverUrl( const QString &coverURL )
{
    m_coverURL = coverURL;
}

QString AmpacheAlbum::coverUrl( ) const
{
    return m_coverURL;
}

QList< QAction * > Meta::AmpacheTrack::currentTrackActions()
{
    QList< QAction * > actions;
    return actions;
}

bool
AmpacheTrack::isPlayable() const
{
    switch( Solid::Networking::status() )
    {
        case Solid::Networking::Unknown:
        case Solid::Networking::Connected:
            return true;
        case Solid::Networking::Unconnected:
        case Solid::Networking::Disconnecting:
        case Solid::Networking::Connecting:
            return false;
    }
    return true;
}
