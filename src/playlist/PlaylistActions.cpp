/****************************************************************************************
 * Copyright (c) 2007-2008 Ian Monroe <ian@monroe.nu>                                   *
 * Copyright (c) 2007-2009 Nikolaj Hald Nielsen <nhn@kde.org>                           *
 * Copyright (c) 2008 Seb Ruiz <ruiz@kde.org>                                           *
 * Copyright (c) 2008 Soren Harward <stharward@gmail.com>                               *
 * Copyright (c) 2009 Téo Mrnjavac <teo@kde.org>                                        *
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

#define DEBUG_PREFIX "Playlist::Actions"

#include "PlaylistActions.h"

#include "core/support/Amarok.h"
#include "core/support/Components.h"
#include "core-impl/playlists/types/file/PlaylistFileSupport.h"
#include "amarokconfig.h"
#include "dbus/PlayerDBusHandler.h"
#include "core/support/Debug.h"
#include "DynamicModel.h"
#include "EngineController.h"
#include "core/engine/EngineObserver.h"
#include "core/interfaces/Logger.h"
#include "MainWindow.h"
#include "navigators/DynamicTrackNavigator.h"
#include "navigators/RandomAlbumNavigator.h"
#include "navigators/RandomTrackNavigator.h"
#include "navigators/RepeatAlbumNavigator.h"
#include "navigators/RepeatTrackNavigator.h"
#include "navigators/StandardTrackNavigator.h"
#include "navigators/FavoredRandomTrackNavigator.h"
#include "PlaylistModelStack.h"
#include "playlist/PlaylistWidget.h"
#include "playlistmanager/PlaylistManager.h"

#include <typeinfo>

Playlist::Actions* Playlist::Actions::s_instance = 0;

Playlist::Actions* Playlist::Actions::instance()
{
    if( !s_instance )
        s_instance = new Actions();
    return s_instance;
}

void
Playlist::Actions::destroy()
{
    delete s_instance;
    s_instance = 0;
}

Playlist::Actions::Actions()
        : QObject()
        , Engine::EngineObserver( The::engineController() )
        , m_nextTrackCandidate( 0 )
        , m_trackToBeLast( 0 )
        , m_navigator( 0 )
        , m_stopAfterMode( StopNever )
        , m_trackError( false )
        , m_waitingForNextTrack( false )
{
    DEBUG_BLOCK

    m_topmostModel = Playlist::ModelStack::instance()->top();
    playlistModeChanged(); // sets m_navigator.

    restoreDefaultPlaylist();
}

Playlist::Actions::~Actions()
{
    DEBUG_BLOCK

    delete m_navigator;
}

Meta::TrackPtr
Playlist::Actions::likelyNextTrack()
{
    return m_topmostModel->trackForId( m_navigator->likelyNextTrack() );
}

Meta::TrackPtr
Playlist::Actions::likelyPrevTrack()
{
    return m_topmostModel->trackForId( m_navigator->likelyLastTrack() );
}

void
Playlist::Actions::requestNextTrack()
{
    DEBUG_BLOCK
    if ( m_nextTrackCandidate != 0 )
        return;
    if( m_trackError )
        return;

    debug() << "so far so good!";
    m_trackError = false;
    if ( stopAfterMode() == StopAfterQueue && m_topmostModel->activeId() == m_trackToBeLast )
    {
        setStopAfterMode( StopAfterCurrent );
        m_trackToBeLast = 0;
    }

    m_nextTrackCandidate = m_navigator->requestNextTrack();

    if( m_nextTrackCandidate == 0 )
    {

        debug() << "nothing more to play...";
        //No more stuff to play. make sure to reset the active track so that
        //pressing play will start at the top of the playlist (or whereever the navigator wants to start)
        //instead of just replaying the last track.
        m_topmostModel->setActiveRow( -1 );

        //We also need to mark all tracks as unplayed or some navigators might be unhappy.
        m_topmostModel->setAllUnplayed();

        //if what is currently playing is a cd track, we need to stop playback as the cd will otherwise continue playing
        if( The::engineController()->isPlayingAudioCd() )
            The::engineController()->stop();

        return;
    }

    if ( stopAfterMode() == StopAfterCurrent )  //stop after current / stop after track starts here
        setStopAfterMode( StopNever );
    else
        play( m_nextTrackCandidate, false );
}

void
Playlist::Actions::requestUserNextTrack()
{
    m_trackError = false;
    m_nextTrackCandidate = m_navigator->requestUserNextTrack();
    play( m_nextTrackCandidate );
}

void
Playlist::Actions::requestPrevTrack()
{
    m_trackError = false;
    m_nextTrackCandidate = m_navigator->requestLastTrack();
    play( m_nextTrackCandidate );
}

void
Playlist::Actions::requestTrack( quint64 id )
{
    m_trackError = false;
    m_nextTrackCandidate = id;
}


void
Playlist::Actions::play()
{
    DEBUG_BLOCK

    if( 0 == m_nextTrackCandidate )
    {
        m_nextTrackCandidate = m_topmostModel->activeId();
        if( 0 == m_nextTrackCandidate )
            m_nextTrackCandidate = m_navigator->requestNextTrack();
    }

    play( m_nextTrackCandidate );
}

void
Playlist::Actions::play( const QModelIndex& index )
{
    DEBUG_BLOCK

    if( index.isValid() )
    {
        m_nextTrackCandidate = index.data( UniqueIdRole ).value<quint64>();
        play( m_nextTrackCandidate );
    }
}

void
Playlist::Actions::play( const int row )
{
    DEBUG_BLOCK

    m_nextTrackCandidate = m_topmostModel->idAt( row );
    play( m_nextTrackCandidate );
}

void
Playlist::Actions::play( const quint64 trackid, bool now )
{
    DEBUG_BLOCK

    Meta::TrackPtr track = m_topmostModel->trackForId( trackid );
    if ( track )
    {
        if ( now )
        {
            Meta::TrackPtr currentTrack = The::engineController()->currentTrack();
            Phonon::State engineState = The::engineController()->state();
            if( currentTrack && ( engineState == Phonon::PlayingState
                               || engineState == Phonon::PausedState
                               || engineState == Phonon::BufferingState ) )
            {
                //Theres a track playing now, calculate statistics for that track before playing a new one.
                const double finishedPercent = (double)The::engineController()->trackPositionMs() / (double)currentTrack->length();
                debug() << "Manually advancing to the next track, calculating previous statistics for track here.  Finished % is: "  << finishedPercent;
                currentTrack->finishedPlaying( finishedPercent );
            }
            The::engineController()->play( track );
        }
        else
            The::engineController()->setNextTrack( track );
    }
    else
    {
        m_trackError = true;
        warning() << "Invalid trackid" << trackid;
    }
}

void
Playlist::Actions::next()
{
    DEBUG_BLOCK
    requestUserNextTrack();
}

void
Playlist::Actions::back()
{
    DEBUG_BLOCK
    requestPrevTrack();
}

void
Playlist::Actions::playlistModeChanged()
{
    DEBUG_BLOCK

    QQueue<quint64> currentQueue;

    if ( m_navigator )
    {
        //HACK: Migrate the queue to the new navigator
        //TODO: The queue really should not be maintained by the navigators in this way
        // but should be handled by a seperate and persistant object.

        currentQueue = m_navigator->queue();
        m_navigator->deleteLater();
    }

    debug() << "Dynamic mode:   " << AmarokConfig::dynamicMode();

    if ( AmarokConfig::dynamicMode() )
    {
        PlaylistBrowserNS::DynamicModel* dm = PlaylistBrowserNS::DynamicModel::instance();

        Dynamic::DynamicPlaylistPtr playlist = dm->activePlaylist();

        if ( !playlist )
        {
            debug() << "No dynamic playlist current loaded! Creating dynamic track navigator with null playlist!";
        }

        m_navigator = new DynamicTrackNavigator( playlist );

        return;

    }

    m_navigator = 0;


    switch( AmarokConfig::trackProgression() )
    {

        case AmarokConfig::EnumTrackProgression::RepeatTrack:
            m_navigator = new RepeatTrackNavigator();
            break;

        case AmarokConfig::EnumTrackProgression::RepeatAlbum:
            m_navigator = new RepeatAlbumNavigator();
            break;

        case AmarokConfig::EnumTrackProgression::RandomTrack:
            switch( AmarokConfig::favorTracks() )
            {
                case AmarokConfig::EnumFavorTracks::HigherScores:
                case AmarokConfig::EnumFavorTracks::HigherRatings:
                case AmarokConfig::EnumFavorTracks::LessRecentlyPlayed:
                    m_navigator = new FavoredRandomTrackNavigator();
                    break;

                case AmarokConfig::EnumFavorTracks::Off:
                default:
                    m_navigator = new RandomTrackNavigator();
                    break;
            }
            break;

        case AmarokConfig::EnumTrackProgression::RandomAlbum:
            m_navigator = new RandomAlbumNavigator();
            break;

        //repeat playlist, standard and fallback are all the normal navigator.
        case AmarokConfig::EnumTrackProgression::RepeatPlaylist:
        case AmarokConfig::EnumTrackProgression::Normal:
        default:
            m_navigator = new StandardTrackNavigator();
            break;
    }

    m_navigator->queueIds( currentQueue );

    The::playerDBusHandler()->updateStatus();

    emit navigatorChanged();
}

void
Playlist::Actions::repopulateDynamicPlaylist()
{
    DEBUG_BLOCK

    if ( typeid( *m_navigator ) == typeid( DynamicTrackNavigator ) )
    {
        static_cast<DynamicTrackNavigator*>(m_navigator)->repopulate();
    }
}

int
Playlist::Actions::queuePosition( quint64 id )
{
    return m_navigator->queuePosition( id );
}

QQueue<quint64>
Playlist::Actions::queue()
{
    return m_navigator->queue();
}

void
Playlist::Actions::queue( QList<int> rows )
{
    DEBUG_BLOCK

    foreach( int row, rows )
    {
        quint64 id = m_topmostModel->idAt( row );
        debug() << "About to queue proxy row"<< row;
        m_navigator->queueId( id );
        m_topmostModel->setRowQueued( row );
    }
}

void
Playlist::Actions::dequeue( QList<int> rows )
{
    DEBUG_BLOCK

    foreach( int row, rows )
    {
        quint64 id = m_topmostModel->idAt( row );
        m_navigator->dequeueId( id );
        m_topmostModel->setRowDequeued( row );
    }
}

void
Playlist::Actions::engineStateChanged( Phonon::State currentState, Phonon::State )
{
    static int failures = 0;
    const int maxFailures = 10;

    m_trackError = false;

    if ( currentState == Phonon::ErrorState )
    {
        failures++;
        warning() << "Error, can not play this track.";
        warning() << "Failure count: " << failures;
        if ( failures >= maxFailures )
        {
            Amarok::Components::logger()->longMessage( i18n( "Too many errors encountered in playlist. Playback stopped." ), Amarok::Logger::Warning );
            error() << "Stopping playlist.";
            failures = 0;
            m_trackError = true;
        }
    }
    else if ( currentState == Phonon::PlayingState )
    {
        if ( failures > 0 )
        {
            debug() << "Successfully played track. Resetting failure count.";
            failures = 0;
        }
    }
}

void
Playlist::Actions::engineNewTrackPlaying()
{
    DEBUG_BLOCK

    Meta::TrackPtr engineTrack = The::engineController()->currentTrack();
    if ( engineTrack )
    {
        Meta::TrackPtr candidateTrack = m_topmostModel->trackForId( m_nextTrackCandidate );    // May be 0.
        if ( engineTrack == candidateTrack )
        {   // The engine is playing what we planned: everything is OK.
            m_topmostModel->setActiveId( m_nextTrackCandidate );
        }
        else
        {
            warning() << "engineNewTrackPlaying:" << engineTrack->prettyName() << "does not match what the playlist controller thought it should be";
            if ( m_topmostModel->activeTrack() != engineTrack )
            {
                 // this will set active row to -1 if the track isn't in the playlist at all
                int row = m_topmostModel->firstRowForTrack( engineTrack );
                if( row != -1 )
                    m_topmostModel->setActiveRow( row );
                else
                    m_topmostModel->setActiveRow( AmarokConfig::lastPlaying() );
            }
            //else
            //  Engine and playlist are in sync even though we didn't plan it; do nothing
        }
    }
    else
        warning() << "engineNewTrackPlaying: not really a track";

    m_nextTrackCandidate = 0;
}


void
Playlist::Actions::normalizeDynamicPlaylist()
{
    if ( typeid( *m_navigator ) == typeid( DynamicTrackNavigator ) )
    {
        static_cast<DynamicTrackNavigator*>(m_navigator)->appendUpcoming();
    }
}

void
Playlist::Actions::repaintPlaylist()
{
    The::mainWindow()->playlistWidget()->currentView()->repaint();
}

void
Playlist::Actions::restoreDefaultPlaylist()
{
    DEBUG_BLOCK

    // The PlaylistManager needs to be loaded or podcast episodes and other
    // non-collection Tracks will not be loaded correctly.
    The::playlistManager();


    Playlists::PlaylistFilePtr playlist = Playlists::loadPlaylistFile( Playlist::ModelStack::instance()->bottom()->defaultPlaylistPath() );
    if ( playlist )
    {
        Meta::TrackList tracks = playlist->tracks();

        QMutableListIterator<Meta::TrackPtr> i( tracks );
        while ( i.hasNext() )
        {
            i.next();
            Meta::TrackPtr track = i.value();
            if ( ! track )
                i.remove();
            else if( Playlists::canExpand( track ) )
            {
                Playlists::PlaylistPtr playlist = Playlists::expand( track );
                //expand() can return 0 if the KIO job errors out
                if( playlist )
                {
                    i.remove();
                    Meta::TrackList newtracks = playlist->tracks();
                    foreach( Meta::TrackPtr t, newtracks )
                        if( t )
                            i.insert( t );
                }
            }
        }

        The::playlistController()->insertTracks( 0, tracks );

        QList<int> queuedRows = playlist->queue();
        queue( playlist->queue() );

        //Select previously playing track
        const int lastPlayingRow = AmarokConfig::lastPlaying();
        if( lastPlayingRow >= 0 )
            Playlist::ModelStack::instance()->bottom()->setActiveRow( lastPlayingRow );
    }
}

namespace The
{
    AMAROK_EXPORT Playlist::Actions* playlistActions() { return Playlist::Actions::instance(); }
}
