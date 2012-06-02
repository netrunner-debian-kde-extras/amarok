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
#include "core/support/Debug.h"
#include "DynamicModel.h"
#include "EngineController.h"
#include "core-impl/collections/support/CollectionManager.h"
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
#include "PlaylistController.h"
#include "playlist/PlaylistDock.h"
#include "playlistmanager/PlaylistManager.h"

#include <KStandardDirs>
#include <typeinfo>

Playlist::Actions* Playlist::Actions::s_instance = 0;

Playlist::Actions* Playlist::Actions::instance()
{
    if( !s_instance )
    {
        s_instance = new Actions();
        s_instance->init(); // prevent infinite recursion by using the playlist actions only after setting the instance.
    }
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
        , m_nextTrackCandidate( 0 )
        , m_trackToBeLast( 0 )
        , m_navigator( 0 )
        , m_stopAfterMode( StopNever )
        , m_waitingForNextTrack( false )
{
    EngineController *engine = The::engineController();

    connect( engine, SIGNAL( trackPlaying( Meta::TrackPtr ) ),
             this, SLOT( slotTrackPlaying( Meta::TrackPtr ) ) );
}

Playlist::Actions::~Actions()
{
    delete m_navigator;
}

void
Playlist::Actions::init()
{
    playlistModeChanged(); // sets m_navigator.
    restoreDefaultPlaylist();
}

Meta::TrackPtr
Playlist::Actions::likelyNextTrack()
{
    return The::playlist()->trackForId( m_navigator->likelyNextTrack() );
}

Meta::TrackPtr
Playlist::Actions::likelyPrevTrack()
{
    return The::playlist()->trackForId( m_navigator->likelyLastTrack() );
}

void
Playlist::Actions::requestNextTrack()
{
    DEBUG_BLOCK
    if ( m_nextTrackCandidate != 0 )
        return;

    debug() << "so far so good!";
    if( stopAfterMode() == StopAfterQueue && The::playlist()->activeId() == m_trackToBeLast )
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
        The::playlist()->setActiveRow( -1 );

        //We also need to mark all tracks as unplayed or some navigators might be unhappy.
        The::playlist()->setAllUnplayed();

        //if what is currently playing is a cd track, we need to stop playback as the cd will otherwise continue playing
        if( The::engineController()->isPlayingAudioCd() )
            The::engineController()->stop();

        return;
    }

    if( stopAfterMode() == StopAfterCurrent )  //stop after current / stop after track starts here
    {
        The::playlist()->setActiveId( m_nextTrackCandidate );
        setStopAfterMode( StopNever );
    }
    else
    {
        play( m_nextTrackCandidate, false );
    }
}

void
Playlist::Actions::requestUserNextTrack()
{
    m_nextTrackCandidate = m_navigator->requestUserNextTrack();
    play( m_nextTrackCandidate );
}

void
Playlist::Actions::requestPrevTrack()
{
    m_nextTrackCandidate = m_navigator->requestLastTrack();
    play( m_nextTrackCandidate );
}

void
Playlist::Actions::requestTrack( quint64 id )
{
    m_nextTrackCandidate = id;
}


void
Playlist::Actions::play()
{
    DEBUG_BLOCK

    if ( m_nextTrackCandidate == 0 )
    {
        m_nextTrackCandidate = The::playlist()->activeId();
        // the queue has priority, and requestNextTrack() respects the queue.
        // this is a bit of a hack because we "know" that all navigators will look at the queue first.
        if ( !m_nextTrackCandidate || !m_navigator->queue().isEmpty() )
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

    m_nextTrackCandidate = The::playlist()->idAt( row );
    play( m_nextTrackCandidate );
}

void
Playlist::Actions::play( const quint64 trackid, bool now )
{
    DEBUG_BLOCK

    Meta::TrackPtr track = The::playlist()->trackForId( trackid );
    if ( track )
    {
        if ( now )
            The::engineController()->play( track );
        else
            The::engineController()->setNextTrack( track );
    }
    else
    {
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
Playlist::Actions::enableDynamicMode( bool enable )
{
    if( AmarokConfig::dynamicMode() == enable )
        return;

    AmarokConfig::setDynamicMode( enable );
    // TODO: turn off other incompatible modes
    // TODO: should we restore the state of other modes?
    AmarokConfig::self()->writeConfig();

    //if the playlist is empty, repopulate while we are at it:
    if( enable )
    {
        if ( Playlist::ModelStack::instance()->bottom()->rowCount() == 0 )
            repopulateDynamicPlaylist();
    }

    playlistModeChanged();
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
        m_navigator = new DynamicTrackNavigator();
        emit navigatorChanged();
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

        //repeat playlist, standard, only queue and fallback are all the normal navigator.
        case AmarokConfig::EnumTrackProgression::RepeatPlaylist:
        case AmarokConfig::EnumTrackProgression::OnlyQueue:
        case AmarokConfig::EnumTrackProgression::Normal:
        default:
            m_navigator = new StandardTrackNavigator();
            break;
    }

    m_navigator->queueIds( currentQueue );

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

bool
Playlist::Actions::queueMoveUp( quint64 id )
{
    const bool ret = m_navigator->queueMoveUp( id );
    if ( ret )
        Playlist::ModelStack::instance()->bottom()->emitQueueChanged();
    return ret;
}

bool
Playlist::Actions::queueMoveDown( quint64 id )
{
    const bool ret = m_navigator->queueMoveDown( id );
    if ( ret )
        Playlist::ModelStack::instance()->bottom()->emitQueueChanged();
    return ret;
}

void
Playlist::Actions::dequeue( quint64 id )
{
    m_navigator->dequeueId( id ); // has no return value, *shrug*
    Playlist::ModelStack::instance()->bottom()->emitQueueChanged();
    return;
}

void
Playlist::Actions::queue( QList<int> rows )
{
    DEBUG_BLOCK

    foreach( int row, rows )
    {
        quint64 id = The::playlist()->idAt( row );
        debug() << "About to queue proxy row"<< row;
        m_navigator->queueId( id );
    }
    if ( !rows.isEmpty() )
        Playlist::ModelStack::instance()->bottom()->emitQueueChanged();
}

void
Playlist::Actions::dequeue( QList<int> rows )
{
    DEBUG_BLOCK

    foreach( int row, rows )
    {
        quint64 id = The::playlist()->idAt( row );
        m_navigator->dequeueId( id );
    }
    if ( !rows.isEmpty() )
        Playlist::ModelStack::instance()->bottom()->emitQueueChanged();
}

void
Playlist::Actions::slotTrackPlaying( Meta::TrackPtr engineTrack )
{
    DEBUG_BLOCK

    if ( engineTrack )
    {
        Meta::TrackPtr candidateTrack = The::playlist()->trackForId( m_nextTrackCandidate );    // May be 0.
        if ( engineTrack == candidateTrack )
        {   // The engine is playing what we planned: everything is OK.
            The::playlist()->setActiveId( m_nextTrackCandidate );
        }
        else
        {
            warning() << "engineNewTrackPlaying:" << engineTrack->prettyName() << "does not match what the playlist controller thought it should be";
            if ( The::playlist()->activeTrack() != engineTrack )
            {
                 // this will set active row to -1 if the track isn't in the playlist at all
                int row = The::playlist()->firstRowForTrack( engineTrack );
                if( row != -1 )
                    The::playlist()->setActiveRow( row );
                else
                    The::playlist()->setActiveRow( AmarokConfig::lastPlaying() );
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
    The::mainWindow()->playlistDock()->currentView()->repaint();
}

void
Playlist::Actions::restoreDefaultPlaylist()
{
    DEBUG_BLOCK

    // The PlaylistManager needs to be loaded or podcast episodes and other
    // non-collection Tracks will not be loaded correctly.
    The::playlistManager();

    Playlists::PlaylistFilePtr playlist = Playlists::loadPlaylistFile( Amarok::defaultPlaylistPath() );

    if( playlist ) // This pointer will be 0 on first startup
        playlist->triggerTrackLoad(); // playlist track loading is on demand

    if( playlist && playlist->tracks().count() > 0 )
    {
        Meta::TrackList tracks = playlist->tracks();

        QMutableListIterator<Meta::TrackPtr> i( tracks );
        while( i.hasNext() )
        {
            i.next();
            Meta::TrackPtr track = i.value();
            if( ! track )
                i.remove();
            else if( Playlists::canExpand( track ) )
            {
                Playlists::PlaylistPtr playlist = Playlists::expand( track );
                //expand() can return 0 if the KIO job errors out
                if( playlist )
                {
                    i.remove();
                    playlist->triggerTrackLoad(); //playlist track loading is on demand.
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

    //Check if we should load the first run jingle, since there is no saved playlist to load
    else if( AmarokConfig::playFirstRunJingle() )
    {
        QString jingle = KStandardDirs::locate( "data", "amarok/data/first_run_jingle.ogg" );
        The::playlistController()->clear();
        The::playlistController()->insertTrack( 0, CollectionManager::instance()->trackForUrl( jingle ) );
        AmarokConfig::setPlayFirstRunJingle( false );
    }
}

namespace The
{
    AMAROK_EXPORT Playlist::Actions* playlistActions() { return Playlist::Actions::instance(); }
}
