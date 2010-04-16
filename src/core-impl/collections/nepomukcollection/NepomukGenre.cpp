/****************************************************************************************
 * Copyright (c) 2008 Daniel Winter <dw@danielwinter.de>                                *
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

#include "NepomukGenre.h"

#include "core/meta/Meta.h"

#include <QString>

using namespace Meta;

NepomukGenre::NepomukGenre( const QString &name )
        : Meta::Genre()
        , m_name( name )
{
}

QString
 NepomukGenre::name() const
{
    return m_name;
}

QString
 NepomukGenre::prettyName() const
{
    return m_name;
}

TrackList
NepomukGenre::tracks()
{
    return TrackList();
}


