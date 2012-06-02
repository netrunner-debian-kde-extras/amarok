/****************************************************************************************
 * Copyright (c) 2008-2011 Soren Harward <stharward@gmail.com>                          *
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

#define DEBUG_PREFIX "Constraint::TrackSpreader"

#include "TrackSpreader.h"

#include "playlistgenerator/Constraint.h"

#include "core/support/Debug.h"

#include <QtGlobal>
#include <math.h>
#include <stdlib.h>

Constraint*
ConstraintTypes::TrackSpreader::createNew( ConstraintNode* p )
{
    if ( p )
        return new TrackSpreader( p );
    else
        return 0;
}

ConstraintFactoryEntry*
ConstraintTypes::TrackSpreader::registerMe()
{
    return 0;
}

ConstraintTypes::TrackSpreader::TrackSpreader( ConstraintNode* p ) : Constraint( p ) {
    DEBUG_BLOCK
}

QWidget*
ConstraintTypes::TrackSpreader::editWidget() const
{
    return 0;
}

void
ConstraintTypes::TrackSpreader::toXml( QDomDocument&, QDomElement& ) const {}

double
ConstraintTypes::TrackSpreader::satisfaction( const Meta::TrackList& tl ) const
{
    QHash<Meta::TrackPtr, int> locations;
    double dist = 0.0;
    for ( int i = 0; i < tl.size(); i++ ) {
        Meta::TrackPtr t = tl.value( i );
        if ( locations.contains( t ) ) {
            foreach( int j, locations.values( t ) ) {
                dist += distance( i, j );
            }
        }
        locations.insertMulti( tl.value( i ), i );
    }

    return 1.0 / exp( 0.1 * dist );
}

#ifndef KDE_NO_DEBUG_OUTPUT
void
ConstraintTypes::TrackSpreader::audit( const Meta::TrackList& tl ) const
{
    QHash<Meta::TrackPtr, int> locations;
    for ( int i = 0; i < tl.size(); i++ ) {
        locations.insertMulti( tl.value( i ), i );
    }

    foreach ( Meta::TrackPtr t, locations.keys() ) {
        QString output;
        output += t->prettyName() + ": ";
        foreach ( int idx, locations.values( t ) ) {
            output += QString::number( idx ) + " ";
        }
        debug() << output;
    }
}
#endif

double
ConstraintTypes::TrackSpreader::distance( const int a, const int b ) const
{
    if ( a == b ) {
        return 0.0;
    }

    int d = qAbs( a - b ) - 1;
    return exp( -0.05 * ( double )d );
}
