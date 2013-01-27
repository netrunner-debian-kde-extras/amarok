/****************************************************************************************
 * Copyright (c) 2007 Maximilian Kossick <maximilian.kossick@googlemail.com>            *
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

#include "MetaProxy.h"

#include "core/meta/Statistics.h"
#include "core/capabilities/EditCapability.h"
#include "core-impl/collections/support/CollectionManager.h"
#include "core-impl/meta/proxy/MetaProxy_p.h"
#include "core-impl/meta/proxy/MetaProxy_p.moc"
#include "core-impl/meta/proxy/MetaProxyWorker.h"

#include <KSharedPtr>
#include <ThreadWeaver/Weaver>

#include <QCoreApplication>
#include <QThread>
#include <QTimer>
#include <QWeakPointer>

using namespace MetaProxy;

class ProxyArtist;
class ProxyFmAlbum;
class ProxyGenre;
class ProxyComposer;
class ProxyYear;

class EditCapabilityProxy : public Capabilities::EditCapability
{
    public:
        EditCapabilityProxy( MetaProxy::Track *track )
            : Capabilities::EditCapability()
            , m_track( track ) {}

        virtual bool isEditable() const { return true; }
        virtual void setTitle( const QString &title ) { m_track->setName( title ); }
        virtual void setAlbum( const QString &newAlbum ) { m_track->setAlbum( newAlbum ); }
        virtual void setAlbumArtist( const QString &newAlbumArtist ) { m_track->setAlbumArtist( newAlbumArtist ); }
        virtual void setArtist( const QString &newArtist ) { m_track->setArtist( newArtist ); }
        virtual void setComposer( const QString &newComposer ) { m_track->setComposer( newComposer ); }
        virtual void setGenre( const QString &newGenre ) { m_track->setGenre( newGenre ); }
        virtual void setYear( int newYear ) { m_track->setYear( newYear ); }
        virtual void setBpm( const qreal newBpm ) { m_track->setBpm( newBpm ); }
        virtual void setComment( const QString &newComment ) { Q_UNUSED( newComment ); /*m_track->setComment( newComment );*/ } // Do we want to support this?
        virtual void setTrackNumber( int newTrackNumber ) { m_track->setTrackNumber( newTrackNumber ); }
        virtual void setDiscNumber( int newDiscNumber ) { m_track->setDiscNumber( newDiscNumber ); }

        virtual void beginMetaDataUpdate() {}  // Nothing to do, we cache everything
        virtual void endMetaDataUpdate() {}

    private:
        KSharedPtr<MetaProxy::Track> m_track;
};

MetaProxy::Track::Track( const KUrl &url, LookupType lookupType )
    : Meta::Track()
    , d( new Private() )
{
    d->url = url;
    d->proxy = this;
    d->cachedLength = 0;
    d->albumPtr = Meta::AlbumPtr( new ProxyAlbum( d ) );
    d->artistPtr = Meta::ArtistPtr( new ProxyArtist( d ) );
    d->genrePtr = Meta::GenrePtr( new ProxyGenre( d ) );
    d->composerPtr = Meta::ComposerPtr( new ProxyComposer( d ) );
    d->yearPtr = Meta::YearPtr( new ProxyYear( d ) );

    QThread *mainThread = QCoreApplication::instance()->thread();
    bool foreignThread = QThread::currentThread() != mainThread;
    if( foreignThread )
        d->moveToThread( mainThread );

    if( lookupType == AutomaticLookup )
    {
        Worker *worker = new Worker( d->url );
        if( foreignThread )
            worker->moveToThread( mainThread );

        QObject::connect( worker, SIGNAL(finishedLookup(Meta::TrackPtr)),
                          d, SLOT(slotUpdateTrack(Meta::TrackPtr)) );
        ThreadWeaver::Weaver::instance()->enqueue( worker );
    }
}

MetaProxy::Track::~Track()
{
    delete d;
}

void
MetaProxy::Track::lookupTrack( Collections::TrackProvider *provider )
{
    Worker *worker = new Worker( d->url, provider );
    QThread *mainThread = QCoreApplication::instance()->thread();
    if( QThread::currentThread() != mainThread )
        worker->moveToThread( mainThread );

    QObject::connect( worker, SIGNAL(finishedLookup(Meta::TrackPtr)),
                      d, SLOT(slotUpdateTrack(Meta::TrackPtr)) );
    ThreadWeaver::Weaver::instance()->enqueue( worker );
}

QString
MetaProxy::Track::name() const
{
    if( d->realTrack )
        return d->realTrack->name();
    else
        return d->cachedName;
}

void
MetaProxy::Track::setName( const QString &name )
{
    d->cachedName = name;
}

QString
MetaProxy::Track::prettyName() const
{
    if( d->realTrack )
        return d->realTrack->prettyName();
    else
        return d->cachedName;
}

QString
MetaProxy::Track::fullPrettyName() const
{
    if( d->realTrack )
        return d->realTrack->fullPrettyName();
    else
        return d->cachedName;
}

QString
MetaProxy::Track::sortableName() const
{
    if( d->realTrack )
        return d->realTrack->sortableName();
    else
        return d->cachedName;
}

QString
Track::fixedName() const
{
    if( d->realTrack )
        return d->realTrack->fixedName();
    else
        return d->cachedName;
}

KUrl
MetaProxy::Track::playableUrl() const
{
    if( d->realTrack )
        return d->realTrack->playableUrl();
    else
        /* don't return d->url here, it may be something like
         * amarok-sqltrackuid://2f9277bb7e49962c1c4c5612811807a1 and Phonon may choke
         * on such urls trying to find a codec and causing hang (bug 308371) */
        return KUrl();
}

QString
MetaProxy::Track::prettyUrl() const
{
    if( d->realTrack )
        return d->realTrack->prettyUrl();
    else
        return d->url.url();
}

QString
MetaProxy::Track::uidUrl() const
{
    if( d->realTrack )
        return d->realTrack->uidUrl();
    else
        return d->url.url();
}

bool
MetaProxy::Track::isPlayable() const
{
    if( d->realTrack )
        return d->realTrack->isPlayable();
    else
        return false;
}

Meta::AlbumPtr
MetaProxy::Track::album() const
{
    return d->albumPtr;
}

void
MetaProxy::Track::setAlbum( const QString &album )
{
    d->cachedAlbum = album;
}

void
Track::setAlbumArtist( const QString &artist )
{
    Q_UNUSED( artist );
}

Meta::ArtistPtr
MetaProxy::Track::artist() const
{
    return d->artistPtr;
}

void
MetaProxy::Track::setArtist( const QString &artist )
{
    d->cachedArtist = artist;
}

Meta::GenrePtr
MetaProxy::Track::genre() const
{
    return d->genrePtr;
}

void
MetaProxy::Track::setGenre( const QString &genre )
{
    d->cachedGenre = genre;
}

Meta::ComposerPtr
MetaProxy::Track::composer() const
{
    return d->composerPtr;
}

void
MetaProxy::Track::setComposer( const QString &composer )
{
    d->cachedComposer = composer;
}

Meta::YearPtr
MetaProxy::Track::year() const
{
    return d->yearPtr;
}

void
MetaProxy::Track::setYear( int year )
{
    d->cachedYear = year;
}

Meta::LabelList
Track::labels() const
{
    if( d->realTrack )
        return d->realTrack->labels();
    else
        return Meta::Track::labels();
}

qreal
MetaProxy::Track::bpm() const
{
    if( d->realTrack )
        return d->realTrack->bpm();
    else
        return d->cachedBpm;
}

void
MetaProxy::Track::setBpm( const qreal bpm )
{
    d->cachedBpm = bpm;
}

QString
MetaProxy::Track::comment() const
{
    if( d->realTrack )
        return d->realTrack->comment();
    else
        return QString();       //do we cache the comment??
}

int
MetaProxy::Track::trackNumber() const
{
    if( d->realTrack )
        return d->realTrack->trackNumber();
    else
        return d->cachedTrackNumber;
}

void
MetaProxy::Track::setTrackNumber( int number )
{
    d->cachedTrackNumber = number;
}

int
MetaProxy::Track::discNumber() const
{
    if( d->realTrack )
        return d->realTrack->discNumber();
    else
        return d->cachedDiscNumber;
}

void
MetaProxy::Track::setDiscNumber( int discNumber )
{
    d->cachedDiscNumber = discNumber;
}

qint64
MetaProxy::Track::length() const
{
    if( d->realTrack )
        return d->realTrack->length();
    else
        return d->cachedLength;
}

void
MetaProxy::Track::setLength( qint64 length )
{
    d->cachedLength = length;
}

int
MetaProxy::Track::filesize() const
{
    if( d->realTrack )
        return d->realTrack->filesize();
    else
        return 0;
}

int
MetaProxy::Track::sampleRate() const
{
    if( d->realTrack )
        return d->realTrack->sampleRate();
    else
        return 0;
}

int
MetaProxy::Track::bitrate() const
{
    if( d->realTrack )
        return d->realTrack->bitrate();
    else
        return 0;
}

QDateTime
MetaProxy::Track::createDate() const
{
    if( d->realTrack )
        return d->realTrack->createDate();
    else
        return Meta::Track::createDate();
}

QDateTime
Track::modifyDate() const
{
    if( d->realTrack )
        return d->realTrack->modifyDate();
    else
        return Meta::Track::modifyDate();
}

qreal
Track::replayGain( Meta::ReplayGainTag mode ) const
{
    if( d->realTrack )
        return d->realTrack->replayGain( mode );
    else
        return Meta::Track::replayGain( mode );
}

QString
MetaProxy::Track::type() const
{
    if( d->realTrack )
        return d->realTrack->type();
    else
        // just debugging, normal users shouldn't hit this
        return QString( "MetaProxy::Track" );
}

void
Track::prepareToPlay()
{
    if( d->realTrack )
        d->realTrack->prepareToPlay();
}

void
MetaProxy::Track::finishedPlaying( double playedFraction )
{
    if( d->realTrack )
        d->realTrack->finishedPlaying( playedFraction );
}

bool
MetaProxy::Track::inCollection() const
{
    if( d->realTrack )
        return d->realTrack->inCollection();
    else
        return false;
}

Collections::Collection *
MetaProxy::Track::collection() const
{
    if( d->realTrack )
        return d->realTrack->collection();
    else
        return 0;
}

QString
Track::cachedLyrics() const
{
    if( d->realTrack )
        return d->realTrack->cachedLyrics();
    else
        return Meta::Track::cachedLyrics();
}

void
Track::setCachedLyrics(const QString& lyrics)
{
    if( d->realTrack )
        d->realTrack->setCachedLyrics( lyrics );
    else
        Meta::Track::setCachedLyrics( lyrics );
}

void
Track::addLabel( const QString &label )
{
    if( d->realTrack )
        d->realTrack->addLabel( label );
    else
        Meta::Track::addLabel( label );
}

void
Track::addLabel( const Meta::LabelPtr &label )
{
    if( d->realTrack )
        d->realTrack->addLabel( label );
    else
        Meta::Track::addLabel( label );
}

void
Track::removeLabel( const Meta::LabelPtr &label )
{
    if( d->realTrack )
        d->realTrack->removeLabel( label );
    else
        Meta::Track::removeLabel( label );
}

void
MetaProxy::Track::updateTrack( Meta::TrackPtr track )
{
    d->slotUpdateTrack( track );
}

bool
MetaProxy::Track::hasCapabilityInterface( Capabilities::Capability::Type type ) const
{
    if( d->realTrack )
        return d->realTrack->hasCapabilityInterface( type );
    else
    {
        if( type == Capabilities::Capability::Editable )
            return true;
        return false;
    }
}

Capabilities::Capability *
MetaProxy::Track::createCapabilityInterface( Capabilities::Capability::Type type )
{
    if( d->realTrack )
        return d->realTrack->createCapabilityInterface( type );
    else
    {
        if( type == Capabilities::Capability::Editable )
            return new EditCapabilityProxy( this );
        return 0;
    }
}

bool
MetaProxy::Track::operator==( const Meta::Track &track ) const
{
    const MetaProxy::Track *proxy = dynamic_cast<const MetaProxy::Track *>( &track );
    if( proxy && d->realTrack )
        return d->realTrack == proxy->d->realTrack;
    else if( proxy )
        return d->url == proxy->d->url;

    return d->realTrack && d->realTrack.data() == &track;
}

Meta::StatisticsPtr
Track::statistics()
{
    if( d->realTrack )
        return d->realTrack->statistics();
    else
        return Meta::Track::statistics();
}
