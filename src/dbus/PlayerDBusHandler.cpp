/****************************************************************************************
 * Copyright (c) 2008 Ian Monroe <ian@monroe.nu>                                        *
 * Copyright (c) 2008 Peter ZHOU <peterzhoulei@gmail.com>                               *
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

#include "PlayerDBusHandler.h"

#include "amarokconfig.h"
#include "ActionClasses.h"
#include "App.h"
#include "core/support/Debug.h"
#include "EngineController.h"
#include "core/meta/Meta.h"
#include "core/meta/support/MetaUtility.h"
#include "PlayerAdaptor.h"
#include "playlist/PlaylistActions.h"
#include "playlist/PlaylistModelStack.h"
#include "Osd.h"
#include "SvgHandler.h"

// Marshall the DBusStatus data into a D-BUS argument
QDBusArgument &operator<<(QDBusArgument &argument, const DBusStatus &status)
{
    argument.beginStructure();
    argument << status.Play;
    argument << status.Random;
    argument << status.Repeat;
    argument << status.RepeatPlaylist;
    argument.endStructure();
    return argument;
}

// Retrieve the DBusStatus data from the D-BUS argument
const QDBusArgument &operator>>(const QDBusArgument &argument, DBusStatus &status)
{
    argument.beginStructure();
    argument >> status.Play;
    argument >> status.Random;
    argument >> status.Repeat;
    argument >> status.RepeatPlaylist;
    argument.endStructure();
    return argument;
}


namespace Amarok
{

    PlayerDBusHandler::PlayerDBusHandler()
        : QObject(kapp),
          EngineObserver( The::engineController() )
    {
        qDBusRegisterMetaType<DBusStatus>();

        setObjectName("PlayerDBusHandler");

        new PlayerAdaptor( this );
        QDBusConnection::sessionBus().registerObject("/Player", this);

        connect( The::playlistActions(), SIGNAL(navigatorChanged()),
                 this, SLOT(updateStatus()) );
    }

    DBusStatus PlayerDBusHandler::GetStatus()
    {
        DBusStatus status = { 0, 0, 0, 0 };
        switch( The::engineController()->state() )
        {
            case Phonon::PlayingState:
            case Phonon::BufferingState:
                status.Play = 0; //Playing
                break;
            case Phonon::PausedState:
                status.Play = 1; //Paused
                break;
            case Phonon::LoadingState:
            case Phonon::StoppedState:
            case Phonon::ErrorState:
                status.Play = 2; //Stopped
        };
        if ( AmarokConfig::trackProgression() == AmarokConfig::EnumTrackProgression::RandomTrack ||
             AmarokConfig::trackProgression() == AmarokConfig::EnumTrackProgression::RandomAlbum )
            status.Random = 1;
        else
            status.Random = 0;

        if ( AmarokConfig::trackProgression() == AmarokConfig::EnumTrackProgression::RepeatTrack  )
            status.Repeat = 1;
        else
            status.Repeat = 0;
        if ( AmarokConfig::trackProgression() == AmarokConfig::EnumTrackProgression::RepeatPlaylist ||
             AmarokConfig::trackProgression() == AmarokConfig::EnumTrackProgression::RepeatAlbum ||
             AmarokConfig::trackProgression() == AmarokConfig::EnumTrackProgression::RandomTrack ||
             AmarokConfig::trackProgression() == AmarokConfig::EnumTrackProgression::RandomAlbum )
            status.RepeatPlaylist = 1;
        else
            status.RepeatPlaylist = 0; //the music will not end if we play random
        return status;
    }

    void PlayerDBusHandler::Pause()
    {
        The::engineController()->playPause();
    }

    void PlayerDBusHandler::Play()
    {
        The::engineController()->play();
    }

    void PlayerDBusHandler::PlayPause()
    {
        if(The::engineController()->state() == Phonon::PlayingState) {
            The::engineController()->pause();
        } else {
            The::engineController()->play();
        }
    }

    void PlayerDBusHandler::Next()
    {
        The::playlistActions()->next();
    }

    void PlayerDBusHandler::Prev()
    {
        The::playlistActions()->back();
    }

    void PlayerDBusHandler::Repeat( bool on )
    {
        debug() << (on ? "Turning repeat on" : "Turning repeat off");
        if( on )
        {
            //if set on, just switch to repeat track
            AmarokConfig::setTrackProgression( AmarokConfig::EnumTrackProgression::RepeatTrack );
            The::playlistActions()->playlistModeChanged();
        }
        else if( AmarokConfig::trackProgression() == AmarokConfig::EnumTrackProgression::RepeatTrack ||
                 AmarokConfig::trackProgression() == AmarokConfig::EnumTrackProgression::RepeatAlbum ||
                 AmarokConfig::trackProgression() == AmarokConfig::EnumTrackProgression::RepeatPlaylist )
        {
            //if set to off, switch to normal mode if we are currently in one of the repeat modes.
            AmarokConfig::setTrackProgression( AmarokConfig::EnumTrackProgression::Normal );
            The::playlistActions()->playlistModeChanged();
        }

        //else just ignore event...
    }

    //position is specified in milliseconds
    int PlayerDBusHandler::PositionGet()
    {
        return The::engineController()->trackPositionMs();
    }

    void PlayerDBusHandler::PositionSet( int time )
    {
        if ( time > 0 && The::engineController()->state() != Phonon::StoppedState )
            The::engineController()->seek( time );
    }

    void PlayerDBusHandler::Stop()
    {
        The::engineController()->stop();
    }

    void PlayerDBusHandler::StopAfterCurrent()
    {
        The::playlistActions()->setStopAfterMode( Playlist::StopAfterCurrent );
    }

    int PlayerDBusHandler::VolumeGet()
    {
        return The::engineController()->volume();
    }

    void PlayerDBusHandler::VolumeSet( int vol )
    {
        The::engineController()->setVolume(vol);
    }

    void PlayerDBusHandler::VolumeUp( int step ) const
    {
        The::engineController()->increaseVolume( step );
    }

    void PlayerDBusHandler::VolumeDown( int step ) const
    {
        The::engineController()->decreaseVolume( step );
    }

    void PlayerDBusHandler::Mute() const
    {
        The::engineController()->toggleMute();
    }

    void PlayerDBusHandler::ShowOSD() const
    {
        Amarok::OSD::instance()->forceToggleOSD();
    }

    void PlayerDBusHandler::LoadThemeFile( const QString &path ) const
    {
        The::svgHandler()->setThemeFile( path );
    }

    void PlayerDBusHandler::Forward( int time )
    {
        if ( time > 0 && The::engineController()->state() != Phonon::StoppedState )
            The::engineController()->seek( The::engineController()->trackPosition() * 1000 + time );
    }

    void PlayerDBusHandler::Backward( int time )
    {
        if ( time > 0 && The::engineController()->state() != Phonon::StoppedState )
            The::engineController()->seek( The::engineController()->trackPosition() * 1000 - time );
    }

    QVariantMap PlayerDBusHandler::GetMetadata()
    {
        return GetTrackMetadata( The::engineController()->currentTrack() );
    }

    int PlayerDBusHandler::GetCaps()
    {
        int caps = NONE;
        Meta::TrackPtr track = The::engineController()->currentTrack();
        caps |= CAN_HAS_TRACKLIST;
        if ( track ) caps |= CAN_PROVIDE_METADATA;
        if ( GetStatus().Play == 0 /*playing*/ ) caps |= CAN_PAUSE;
        if ( ( GetStatus().Play == 1 /*paused*/ ) || ( GetStatus().Play == 2 /*stoped*/ ) ) caps |= CAN_PLAY;
        if ( ( GetStatus().Play == 0 /*playing*/ ) || ( GetStatus().Play == 1 /*paused*/ ) ) caps |= CAN_SEEK;
        if ( ( The::playlist()->activeRow() >= 0 ) && ( The::playlist()->activeRow() <= The::playlist()->qaim()->rowCount() ) )
        {
            caps |= CAN_GO_NEXT;
            caps |= CAN_GO_PREV;
        }
        return caps;
    }

    void PlayerDBusHandler::updateStatus()
    {
        DBusStatus status = GetStatus();
        emit StatusChange( status );
        emit CapsChange( GetCaps() );
    }

    QVariantMap PlayerDBusHandler::GetTrackMetadata( Meta::TrackPtr track )
    {
        return Meta::Field::mprisMapFromTrack( track );
    }

    void PlayerDBusHandler::engineTrackChanged( Meta::TrackPtr track )
    {
        Q_UNUSED( track );
        emit TrackChange( GetMetadata() );
        updateStatus();
    }
    void PlayerDBusHandler::engineStateChanged( Phonon::State currentState, Phonon::State oldState )
    {
        Q_UNUSED( currentState );
        Q_UNUSED( oldState );
        updateStatus();
    }
} // namespace Amarok

#include "PlayerDBusHandler.moc"
