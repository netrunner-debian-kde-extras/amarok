/****************************************************************************************
 * Copyright (c) 2009 Nikolaj Hald Nielsen <nhn@kde.org>                                *
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

#include "core-impl/meta/multi/MultiTrack.h"

#include "core/support/Debug.h"
#include "core-impl/capabilities/multisource/MultiSourceCapabilityImpl.h"

namespace Meta {

MultiTrack::MultiTrack( Playlists::PlaylistPtr playlist )
    : QObject()
    , Track()
    , m_playlist( playlist )
    , m_index( 0 )
{
    DEBUG_BLOCK
    debug() << "playlist size: " << m_playlist->tracks().count();
    setSource( 0 );
}


MultiTrack::~MultiTrack()
{
}

void Meta::MultiTrack::setSource( int source )
{
    if( source >= m_playlist->tracks().count() ) // avaid crash by doing nothing
        return;

    if( m_currentTrack )
        unsubscribeFrom( m_currentTrack );

    m_index = source;
    m_currentTrack = m_playlist->tracks()[ m_index ];
    subscribeTo( m_currentTrack );

    notifyObservers();
    emit urlChanged( playableUrl() );
}


KUrl Meta::MultiTrack::first()
{
    if( m_currentTrack )
        unsubscribeFrom( m_currentTrack );
    
    m_index = 0;
    if ( m_playlist->tracks().count() > m_index ) {
        m_currentTrack = m_playlist->tracks()[ m_index ];
        subscribeTo( m_currentTrack );
        return m_currentTrack->playableUrl();
    }

    return KUrl();
}

KUrl Meta::MultiTrack::next()
{
    if( m_currentTrack )
        unsubscribeFrom( m_currentTrack );
    
    m_index++;
    
    if ( m_playlist->tracks().count() > m_index ){
        m_currentTrack = m_playlist->tracks()[ m_index ];
        subscribeTo( m_currentTrack );
        return m_currentTrack->playableUrl();
    }

    return KUrl();
}


int Meta::MultiTrack::current()
{
    return m_index;
}

QStringList Meta::MultiTrack::sources()
{
    QStringList trackNames;
    foreach ( TrackPtr track, m_playlist->tracks() )
    {
        trackNames << track->prettyUrl();
    }

    return trackNames;
}

bool Meta::MultiTrack::hasCapabilityInterface(Capabilities::Capability::Type type) const
{
    return type == Capabilities::Capability::MultiSource;
}

Capabilities::Capability * Meta::MultiTrack::createCapabilityInterface(Capabilities::Capability::Type type)
{
    switch( type )
    {
        case Capabilities::Capability::MultiSource:
            return new Capabilities::MultiSourceCapabilityImpl( this );
        default:
            return 0;
    }
}

}

void Meta::MultiTrack::metadataChanged( Meta::TrackPtr track )
{
    Q_UNUSED( track )
    //forward changes from active tracks
    notifyObservers();
}


#include "MultiTrack.moc"

