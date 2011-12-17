/****************************************************************************************
 * Copyright (c) 2007 Maximilian Kossick <maximilian.kossick@googlemail.com>            *
 * Copyright (c) 2008 Shane King <kde@dontletsstart.com>                                *
 * Copyright (c) 2008 Leo Franchi <lfranchi@kde.org>                                    *
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

#include "LastFmMeta.h"
#include "LastFmMeta_p.h"
#include "LastFmMeta_p.moc"
#include "LastFmCapabilityImpl_p.h"
#include "LastFmCapabilityImpl_p.moc"
#include "MultiPlayableCapabilityImpl_p.h"
#include "MultiPlayableCapabilityImpl_p.moc"
#include "ServiceCapabilities.h"

#include "LastFmService.h"
#include "LastFmStreamInfoCapability.h"
#include "ScrobblerAdapter.h"

#include "EngineController.h"
#include "core/support/Debug.h"
#include "core/capabilities/ActionsCapability.h"

#include <KLocale>
#include <KSharedPtr>
#include <KStandardDirs>
#include <Solid/Networking>

#include <QWeakPointer>
#include <QUrl>

#include <lastfm/Track>

namespace LastFm {

class LastFmArtist;
class LastFmAlbum;
class LastFmGenre;
class LastFmComposer;
class LastFmYear;

Track::Track( const QString &lastFmUri )
    : QObject()
    , Meta::Track()
    , d( new Private() )
{
    d->lastFmUri = QUrl( lastFmUri );
    d->t = this;


    init();
}

Track::Track( lastfm::Track track )
    : QObject()
    , Meta::Track()
    , d( new Private() )
{
    d->t = this;
    d->track = track.title();
    d->lastFmTrack = track;
    QMap< QString, QString > params;
    params[ "method" ] = "track.getInfo";
    params[ "artist" ] = track.artist();
    params[ "track" ]  = track.title();

    d->trackFetch = lastfm::ws::post( params );

    connect( d->trackFetch, SIGNAL( finished() ), SLOT( slotResultReady() ) );
}


Track::~Track()
{
    delete d;
}

void Track::init( int id /* = -1*/ )
{
    if( id != -1 )
        d->lastFmUri = QUrl( "lastfm://play/tracks/" + QString::number( id ) );
    d->length = 0;

    d->albumPtr = Meta::AlbumPtr( new LastFmAlbum( d ) );
    d->artistPtr = Meta::ArtistPtr( new LastFmArtist( d ) );
    d->genrePtr = Meta::GenrePtr( new LastFmGenre( d ) );
    d->composerPtr = Meta::ComposerPtr( new LastFmComposer( d ) );
    d->yearPtr = Meta::YearPtr( new LastFmYear( d ) );

    QAction * banAction = new QAction( KIcon( "remove-amarok" ), i18n( "Last.fm: &Ban" ), this );
    banAction->setShortcut( i18n( "Ctrl+B" ) );
    banAction->setStatusTip( i18n( "Ban this track" ) );
    connect( banAction, SIGNAL( triggered() ), this, SLOT( ban() ) );
    m_trackActions.append( banAction );

    QAction * skipAction = new QAction( KIcon( "media-seek-forward-amarok" ), i18n( "Last.fm: &Skip" ), this );
    skipAction->setShortcut( i18n( "Ctrl+S" ) );
    skipAction->setStatusTip( i18n( "Skip this track" ) );
    connect( skipAction, SIGNAL( triggered() ), this, SLOT( skip() ) );
    m_trackActions.append( skipAction );
}

QString
Track::name() const
{
    if( d->track.isEmpty() )
    {
        return streamName();
    }
    else
    {
        return d->track;
    }
}

QString
Track::fullPrettyName() const
{
    if( d->track.isEmpty() || d->artist.isEmpty() )
        return prettyName();
    else
        return i18n("%1 - %2", d->artist, d->track );
}

QString
Track::sortableName() const
{
    // TODO
    return name();
}

QString LastFm::Track::fixedName() const
{

    //unless this is a stream for a single track, return the generic name
    if ( streamName() != i18n( "Track Radio" ) )
        return streamName();
    else
        return name();
}


KUrl
Track::playableUrl() const
{
    return d->lastFmUri.toString();
}

KUrl
Track::internalUrl() const
{
    return KUrl( d->trackPath );
}

QString
Track::prettyUrl() const
{
    return d->lastFmUri.toString();
}

QString
Track::uidUrl() const
{
    return d->lastFmUri.toString();
}

bool
Track::isPlayable() const
{
    if( Solid::Networking::status() != Solid::Networking::Connected )
        return false;

    return !d->trackPath.isEmpty();
}

Meta::AlbumPtr
Track::album() const
{
    return d->albumPtr;
}

Meta::ArtistPtr
Track::artist() const
{
    return d->artistPtr;
}

Meta::GenrePtr
Track::genre() const
{
    return d->genrePtr;
}

Meta::ComposerPtr
Track::composer() const
{
    return d->composerPtr;
}

Meta::YearPtr
Track::year() const
{
    return d->yearPtr;
}

qreal
Track::bpm() const
{
    return -1.0;
}

QString
Track::comment() const
{
    return QString();
}

double
Track::score() const
{
    if( d->statisticsProvider )
        return d->statisticsProvider->score();
    else
        return 0.0;
}

void
Track::setScore( double newScore )
{
    if( d->statisticsProvider )
    {
        d->statisticsProvider->setScore( newScore );
        notifyObservers();
    }
}

int
Track::rating() const
{
    if( d->statisticsProvider )
        return d->statisticsProvider->rating();
    else
        return 0;
}

void
Track::setRating( int newRating )
{
    if( d->statisticsProvider )
    {
        d->statisticsProvider->setRating( newRating );
        notifyObservers();
    }
}

int
Track::trackNumber() const
{
    return 0;
}

int
Track::discNumber() const
{
    return 0;
}

qint64
Track::length() const
{
    return d->length;
}

int
Track::filesize() const
{
    return 0; //stream
}

int
Track::sampleRate() const
{
    return 0; //does the engine deliver this?
}

int
Track::bitrate() const
{
    return 0; //does the engine deliver this??
}

QDateTime
Track::lastPlayed() const
{
    if( d->statisticsProvider )
        return d->statisticsProvider->lastPlayed();
    else
        return QDateTime();
}

QDateTime
Track::firstPlayed() const
{
    if( d->statisticsProvider )
        return d->statisticsProvider->firstPlayed();
    else
        return QDateTime();
}

int
Track::playCount() const
{
    if( d->statisticsProvider )
        return d->statisticsProvider->playCount();
    return 0;
}

QString
Track::type() const
{
    return "stream/lastfm";
}
void
Track::finishedPlaying( double playedFraction )
{
    if( d->statisticsProvider )
    {
        d->statisticsProvider->played( playedFraction, Meta::TrackPtr( this ) );
        notifyObservers();
    }
}

bool
Track::inCollection() const
{
    return false;
}

Collections::Collection*
Track::collection() const
{
    return 0;
}

void
Track::setTrackInfo( const lastfm::Track &track )
{
    if( !track.isNull() )
        d->setTrackInfo( track );
}

QString
Track::streamName() const
{
    // parse the url to get a name if we don't have a track name (ie we're not playing the station)
    // do it as name rather than prettyname so it shows up nice in the playlist.
    QStringList elements = d->lastFmUri.toString().split( '/', QString::SkipEmptyParts );
    if( elements.size() >= 2 && elements[0] == "lastfm:" )
    {
        QString customPart = QUrl::fromPercentEncoding( elements[2].toUtf8() );

        if( elements[1] == "globaltags" )
        {
                // lastfm://globaltag/<tag>
            if( elements.size() >= 3 )
                return i18n( "Global Tag Radio: \"%1\"", customPart );
        }
        else if( elements[1] == "usertags" )
        {
                // lastfm://usertag/<tag>
            if( elements.size() >= 3 )
                return i18n( "User Tag Radio: \"%1\"", customPart );
        }
        else if( elements[1] == "artist" )
        {
            if( elements.size() >= 4 )
            {
                    // lastfm://artist/<artist>/similarartists
                if( elements[3] == "similarartists" )
                    return i18n( "Similar Artists to \"%1\"", customPart );

                    // lastfm://artist/<artist>/fans
                else if( elements[3] == "fans" )
                    return i18n( "Artist Fan Radio: \"%1\"", customPart );
            }
        }
        else if( elements[1] == "user" )
        {
            if( elements.size() >= 4 )
            {
                // lastfm://user/<user>/neighbours
                if( elements[3] == "neighbours" )
                    return i18n( "%1's Neighbor Radio", elements[2] );

                // lastfm://user/<user>/personal
                else if( elements[3] == "personal" )
                    return i18n( "%1's Personal Radio", elements[2] );

                // lastfm://user/<user>/mix
                else if( elements[3] == "mix" )
                    return i18n( "%1's Mix Radio", elements[2] );

                // lastfm://user/<user>/recommended
                else if( elements.size() < 5 && elements[3] == "recommended" )
                    return i18n( "%1's Recommended Radio", elements[2] );

                // lastfm://user/<user>/recommended/<popularity>
                else if( elements.size() >= 5 && elements[3] == "recommended" )
                    return i18n( "%1's Recommended Radio (Popularity %2)", elements[2], elements[4] );
            }
        }
        else if( elements[1] == "group" )
        {
                // lastfm://group/<group>
            if( elements.size() >= 3 )
                return i18n( "Group Radio: %1", elements[2] );
        }
        else if( elements[1] == "play" )
        {
            if( elements.size() >= 4 )
            {
                    // lastfm://play/tracks/<track #s>
                if ( elements[2] == "tracks" )
                    return i18n( "Track Radio" );

                    // lastfm://play/artists/<artist #s>
                else if ( elements[2] == "artists" )
                    return i18n( "Artist Radio" );
            }
        }
    }

    return d->lastFmUri.toString();
}

void
Track::love()
{
    DEBUG_BLOCK

    debug() << "info:" << d->lastFmTrack.artist() << d->lastFmTrack.title();
    d->wsReply = lastfm::MutableTrack( d->lastFmTrack ).love();
    connect( d->wsReply, SIGNAL( finished() ), this, SLOT( slotWsReply() ) );
}

void
Track::ban()
{
    DEBUG_BLOCK
    d->wsReply = lastfm::MutableTrack( d->lastFmTrack ).ban();
    connect( d->wsReply, SIGNAL( finished() ), this, SLOT( slotWsReply() ) );
    if( The::engineController()->currentTrack() == this )
        emit( skipTrack() );
}

void
Track::skip()
{
    DEBUG_BLOCK
    //MutableTrack( d->lastFmTrack ).skip();
    emit( skipTrack() );
}

void Track::slotResultReady()
{
    if( d->trackFetch->error() == QNetworkReply::NoError )
    {
        try
        {
            lastfm::XmlQuery lfm( d->trackFetch->readAll() );
            QString id = lfm[ "track" ][ "id" ].text();
            QString streamable = lfm[ "track" ][ "streamable" ].text();
            if( streamable.toInt() == 1 )
                init( id.toInt() );
            else
                init();

        } catch( lastfm::ws::ParseError& e )
        {
            debug() << "Got exception in parsing from last.fm:" << e.what();
        }
    } else
    {
        init();
    }
    d->trackFetch->deleteLater();
}


void
Track::slotWsReply()
{
    if( d->wsReply->error() == QNetworkReply::NoError )
    {
        //debug() << "successfully completed WS transaction";
    } else
    {
        debug() << "ERROR in last.fm skip or ban!" << d->wsReply->error();
    }
}

bool
Track::hasCapabilityInterface( Capabilities::Capability::Type type ) const
{
    return type == Capabilities::Capability::LastFm
                || type == Capabilities::Capability::MultiPlayable
                || type == Capabilities::Capability::SourceInfo
                || type == Capabilities::Capability::Actions
                || type == Capabilities::Capability::StreamInfo;
}

Capabilities::Capability*
Track::createCapabilityInterface( Capabilities::Capability::Type type )
{
    switch( type )
    {
        case Capabilities::Capability::LastFm:
            return new LastFmCapabilityImpl( this );
        case Capabilities::Capability::MultiPlayable:
            return new MultiPlayableCapabilityImpl( this );
        case Capabilities::Capability::SourceInfo:
            return new ServiceSourceInfoCapability( this );
        case Capabilities::Capability::Actions:
            return new Capabilities::ActionsCapability( m_trackActions );
        case Capabilities::Capability::StreamInfo:
            return new LastFmStreamInfoCapability( this );
        default:
            return 0;
    }
}

} // namespace LastFm

QString LastFm::Track::sourceName()
{
    return "Last.fm";
}

QString LastFm::Track::sourceDescription()
{
    return i18n( "Last.fm is cool..." );
}

QPixmap LastFm::Track::emblem()
{
    if (  !d->track.isEmpty() )
        return QPixmap( KStandardDirs::locate( "data", "amarok/images/emblem-lastfm.png" ) );
    else
        return QPixmap();
}

QString LastFm::Track::scalableEmblem()
{
    if ( !d->track.isEmpty() )
        return KStandardDirs::locate( "data", "amarok/images/emblem-lastfm-scalable.svg" );
    else
        return QString();
}

#include "LastFmMeta.moc"



