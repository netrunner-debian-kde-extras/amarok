/****************************************************************************************
 * Copyright (c) 2008 Seb Ruiz <ruiz@kde.org>                                           *
 * Copyright (c) 2008 Soren Harward <stharward@gmail.com>                               *
 * Copyright (c) 2008 Nikolaj Hald Nielsen <nhn@kde.org>                                *
 * Copyright (c) 2009 Téo Mrnjavac <teo.mrnjavac@gmail.com>                             *
 *                                                                                      *
 * This program is free software; you can redistribute it and/or modify it under        *
 * the terms of the GNU General Public License as published by the Free Software        *
 * Foundation; either version 2 of the License, or (at your option) version 3 or        *
 * any later version accepted by the membership of KDE e.V. (or its successor approved  *
 * by the membership of KDE e.V.), which shall act as a proxy defined in Section 14 of  *
 * version 3 of the license.                                                            *
 *                                                                                      *
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY      *
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A      *
 * PARTICULAR PURPOSE. See the GNU General Public License for more details.             *
 *                                                                                      *
 * You should have received a copy of the GNU General Public License along with         *
 * this program.  If not, see <http://www.gnu.org/licenses/>.                           *
 ****************************************************************************************/

#define DEBUG_PREFIX "Playlist::RandomTrackNavigator"

#include "RandomTrackNavigator.h"

#include "Debug.h"
#include "playlist/PlaylistModelStack.h"

#include <KRandom>

#include <algorithm> // STL

Playlist::RandomTrackNavigator::RandomTrackNavigator()
{
    m_model = Playlist::ModelStack::instance()->top();

    reset();

    connect( model(), SIGNAL( insertedIds( const QList<quint64>& ) ),
             this, SLOT( recvInsertedIds( const QList<quint64>& ) ) );
    connect( model(), SIGNAL( removedIds( const QList<quint64>& ) ),
             this, SLOT( recvRemovedIds( const QList<quint64>& ) ) );
    connect( model(), SIGNAL( activeTrackChanged( const quint64 ) ),
             this, SLOT( recvActiveTrackChanged( const quint64 ) ) );
}

void
Playlist::RandomTrackNavigator::recvInsertedIds( const QList<quint64>& list )
{
    foreach( quint64 t, list )
    {
        if ( ( m_model->stateOfId( t ) == Item::Unplayed   ) ||
             ( m_model->stateOfId( t ) == Item::NewlyAdded ) )
        {
            m_unplayedRows.append( t );
        }
    }

    std::random_shuffle( m_unplayedRows.begin(), m_unplayedRows.end() );
}

void
Playlist::RandomTrackNavigator::recvRemovedIds( const QList<quint64>& list )
{
    foreach( quint64 t, list )
    {
        m_unplayedRows.removeAll( t );
        m_playedRows.removeAll( t );
    }
}

void
Playlist::RandomTrackNavigator::recvActiveTrackChanged( const quint64 id )
{
    if( m_replayedRows.contains( id ) )
    {
        m_playedRows.prepend( m_replayedRows.takeAt( m_replayedRows.indexOf( id ) ) );
    }
    else if( m_unplayedRows.contains( id ) )
    {
        m_playedRows.prepend( m_unplayedRows.takeAt( m_unplayedRows.indexOf( id ) ) );
    }
}

quint64
Playlist::RandomTrackNavigator::requestNextTrack()
{
    if( !m_queue.isEmpty() )
        return m_queue.takeFirst();

    if( m_unplayedRows.isEmpty() && m_playedRows.isEmpty() )
        return 0;
    else if( m_unplayedRows.isEmpty() && !m_repeatPlaylist )
        return 0;
    else
    {
        if ( m_unplayedRows.isEmpty() )
        {
            // reset when playlist finishes
            reset();
        }

        quint64 requestedTrack = 0;
        // Respect queue priority over random track
        if( !m_queue.isEmpty() )
        {
            requestedTrack = m_queue.takeFirst();
            // remove the id from the unplayed rows list
            m_unplayedRows.removeAll( requestedTrack );
        }
        else if( !m_replayedRows.isEmpty() )
        {
            requestedTrack = m_replayedRows.takeFirst();
        }
        else if( !m_unplayedRows.isEmpty() )
        {
            requestedTrack = m_unplayedRows.takeFirst();
        }

        if( requestedTrack == m_model->activeId() )
        {
            m_playedRows.prepend( requestedTrack );

            if( !m_replayedRows.isEmpty() )
                requestedTrack = m_replayedRows.takeFirst();
            else if( !m_unplayedRows.isEmpty() )
                requestedTrack = m_unplayedRows.takeFirst();
        }
        m_playedRows.prepend( requestedTrack );

        return requestedTrack;
    }
}

quint64
Playlist::RandomTrackNavigator::requestLastTrack()
{
    if ( m_model->tracks().isEmpty() )
        return 0;
    else if ( m_playedRows.isEmpty() && !m_repeatPlaylist )
        return 0;
    else
    {
        if ( m_playedRows.isEmpty() )
        {
            m_playedRows = m_unplayedRows;
            m_replayedRows.clear();
            m_unplayedRows.clear();
        }

        quint64 requestedTrack =  !m_playedRows.isEmpty() ? m_playedRows.takeFirst() : 0;

        if( requestedTrack == m_model->activeId() )
        {
            m_replayedRows.prepend( requestedTrack );
            if ( !m_playedRows.isEmpty() )
                requestedTrack = m_playedRows.takeFirst();
        }
        m_replayedRows.prepend( requestedTrack );

        return requestedTrack;
    }
}

void Playlist::RandomTrackNavigator::reset()
{
    DEBUG_BLOCK

    m_unplayedRows.clear();
    m_replayedRows.clear();
    m_playedRows.clear();

    const int max = m_model->rowCount();
    for( int i = 0; i < max; i++ )
    {
        // everything is unplayed upon reset
        m_unplayedRows.append( m_model->idAt( i ) );
    }

    std::random_shuffle( m_unplayedRows.begin(), m_unplayedRows.end() );
}
