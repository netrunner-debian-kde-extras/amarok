/****************************************************************************************
 * Copyright (c) 2007 Maximilian Kossick <maximilian.kossick@googlemail.com>            *
 * Copyright (c) 2008 Seb Ruiz <ruiz@kde.org>                                           *
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

#include "File.h"
#include "File_p.h"

#include "core/support/Amarok.h"
#include "amarokurls/BookmarkMetaActions.h"
#include "browsers/filebrowser/FileBrowser.h"
#include <config-amarok.h>
#include "MainWindow.h"
#include "core/meta/Meta.h"
#include "core/capabilities/BookmarkThisCapability.h"
#include "core/capabilities/EditCapability.h"
#include "core/capabilities/FindInSourceCapability.h"
#include "core/capabilities/StatisticsCapability.h"
#include "core-impl/capabilities/timecode/TimecodeWriteCapability.h"
#include "core-impl/capabilities/timecode/TimecodeLoadCapability.h"
#include "core-impl/statistics/providers/url/PermanentUrlStatisticsProvider.h"
#include "core/meta/support/MetaUtility.h"
#include "amarokurls/PlayUrlRunner.h"

#include <QAction>
#include <QList>
#include <QWeakPointer>
#include <QString>

#ifdef HAVE_LIBLASTFM
  #include "LastfmReadLabelCapability.h"
#endif
using namespace MetaFile;

class EditCapabilityImpl : public Capabilities::EditCapability
{
    Q_OBJECT
    public:
        EditCapabilityImpl( MetaFile::Track *track )
            : Capabilities::EditCapability()
            , m_track( track )
        {}

        virtual bool isEditable() const { return m_track->isEditable(); }
        virtual void setAlbum( const QString &newAlbum ) { m_track->setAlbum( newAlbum ); }
        virtual void setAlbumArtist( const QString &newAlbumArtist ) { m_track->setAlbumArtist( newAlbumArtist ); }
        virtual void setArtist( const QString &newArtist ) { m_track->setArtist( newArtist ); }
        virtual void setComposer( const QString &newComposer ) { m_track->setComposer( newComposer ); }
        virtual void setGenre( const QString &newGenre ) { m_track->setGenre( newGenre ); }
        virtual void setYear( int newYear ) { m_track->setYear( newYear ); }
        virtual void setBpm( const qreal newBpm ) { m_track->setBpm( newBpm ); }
        virtual void setTitle( const QString &newTitle ) { m_track->setTitle( newTitle ); }
        virtual void setComment( const QString &newComment ) { m_track->setComment( newComment ); }
        virtual void setTrackNumber( int newTrackNumber ) { m_track->setTrackNumber( newTrackNumber ); }
        virtual void setDiscNumber( int newDiscNumber ) { m_track->setDiscNumber( newDiscNumber ); }
        virtual void setUidUrl( const QString &newUidUrl ) { m_track->setUidUrl( newUidUrl ); }
        virtual void beginMetaDataUpdate() { m_track->beginMetaDataUpdate(); }
        virtual void endMetaDataUpdate() { m_track->endMetaDataUpdate(); }

    private:
        KSharedPtr<MetaFile::Track> m_track;
};

class StatisticsCapabilityImpl : public Capabilities::StatisticsCapability
{
    public:
        StatisticsCapabilityImpl( MetaFile::Track *track )
            : Capabilities::StatisticsCapability()
            , m_track( track )
        {}

        virtual void setScore( const int score ) { m_track->setScore( score ); }
        virtual void setRating( const int rating ) { m_track->setRating( rating ); }
        virtual void setFirstPlayed( const QDateTime &time ) { m_track->setFirstPlayed( time ); }
        virtual void setLastPlayed( const QDateTime &time ) { m_track->setLastPlayed( time ); }
        virtual void setPlayCount( const int playcount ) { m_track->setPlayCount( playcount ); }
        virtual void beginStatisticsUpdate() {};
        virtual void endStatisticsUpdate() {};

    private:
        KSharedPtr<MetaFile::Track> m_track;
};

class TimecodeWriteCapabilityImpl : public Capabilities::TimecodeWriteCapability
{
    public:
        TimecodeWriteCapabilityImpl( MetaFile::Track *track )
            : Capabilities::TimecodeWriteCapability()
            , m_track( track )
        {}

    virtual bool writeTimecode ( qint64 miliseconds )
    {
        DEBUG_BLOCK
        return Capabilities::TimecodeWriteCapability::writeTimecode( miliseconds, Meta::TrackPtr( m_track.data() ) );
    }

    virtual bool writeAutoTimecode ( qint64 miliseconds )
    {
        DEBUG_BLOCK
        return Capabilities::TimecodeWriteCapability::writeAutoTimecode( miliseconds, Meta::TrackPtr( m_track.data() ) );
    }

    private:
        KSharedPtr<MetaFile::Track> m_track;
};

class TimecodeLoadCapabilityImpl : public Capabilities::TimecodeLoadCapability
{
    public:
        TimecodeLoadCapabilityImpl( MetaFile::Track *track )
        : Capabilities::TimecodeLoadCapability()
        , m_track( track )
        {}

        virtual bool hasTimecodes()
        {
            if ( loadTimecodes().size() > 0 )
                return true;
            return false;
        }

        virtual BookmarkList loadTimecodes()
        {
            BookmarkList list = PlayUrlRunner::bookmarksFromUrl( m_track->playableUrl() );
            return list;
        }

    private:
        KSharedPtr<MetaFile::Track> m_track;
};


class FindInSourceCapabilityImpl : public Capabilities::FindInSourceCapability
{
public:
    FindInSourceCapabilityImpl( MetaFile::Track *track )
        : Capabilities::FindInSourceCapability()
        , m_track( track )
        {}

    virtual void findInSource( QFlags<TargetTag> tag )
    {
        Q_UNUSED( tag )
        //first show the filebrowser
        AmarokUrl url;
        url.setCommand( "navigate" );
        url.setPath( "files" );
        url.run();

        //then navigate to the correct directory
        BrowserCategory * fileCategory = The::mainWindow()->browserDock()->list()->activeCategoryRecursive();
        if( fileCategory )
        {
            FileBrowser * fileBrowser = dynamic_cast<FileBrowser *>( fileCategory );
            if( fileBrowser )
            {
                //get the path of the parent directory of the file
                KUrl playableUrl = m_track->playableUrl();
                fileBrowser->setDir( playableUrl.directory() );       
            }
        }
    }

private:
    KSharedPtr<MetaFile::Track> m_track;
};


Track::Track( const KUrl &url )
    : Meta::Track()
    , d( new Track::Private( this ) )
{
    d->url = url;
    d->provider = new PermanentUrlStatisticsProvider( url.url() );
    d->readMetaData();
    d->album = Meta::AlbumPtr( new MetaFile::FileAlbum( d ) );
    d->artist = Meta::ArtistPtr( new MetaFile::FileArtist( d ) );
    d->albumArtist = Meta::ArtistPtr( new MetaFile::FileArtist( d, true ) );
    d->genre = Meta::GenrePtr( new MetaFile::FileGenre( d ) );
    d->composer = Meta::ComposerPtr( new MetaFile::FileComposer( d ) );
    d->year = Meta::YearPtr( new MetaFile::FileYear( d ) );
}

Track::~Track()
{
    delete d->provider;
    delete d;
}

QString
Track::name() const
{
    if( d )
    {
        const QString trackName = d->m_data.title;
        return trackName;
    }
    return "This is a bug!";
}

KUrl
Track::playableUrl() const
{
    return d->url;
}

QString
Track::prettyUrl() const
{
    if(d->url.isLocalFile())
    {
        return d->url.toLocalFile();
    }
    else
    {
        return d->url.path();
    }
}

QString
Track::uidUrl() const
{
    return d->url.url();
}

void
Track::setUidUrl( const QString &newUid ) const
{
    if( newUid.isEmpty() )
        return;

    d->changes.insert( Meta::valUniqueId, QVariant( newUid ) );
    if( !d->batchUpdate )
    {
        d->writeMetaData();
        notifyObservers();
    }
}

bool
Track::isPlayable() const
{
    //simple implementation, check Internet connectivity or ping server?
    return true;
}

bool
Track::isEditable() const
{
    DEBUG_BLOCK

    //note this probably needs more work on *nix
    QFile::Permissions p;
    if(d->url.isLocalFile())
    {
        p = QFile::permissions( d->url.toLocalFile() );
    }
    else
    {
        p = QFile::permissions( d->url.path() );
    }
    const bool editable = ( p & QFile::WriteUser ) || ( p & QFile::WriteGroup ) || ( p & QFile::WriteOther );

    debug() << d->url.path() << " editable: " << editable;
    return editable;
}

Meta::AlbumPtr
Track::album() const
{
    return d->album;
}

Meta::ArtistPtr
Track::artist() const
{
    return d->artist;
}

Meta::GenrePtr
Track::genre() const
{
    return d->genre;
}

Meta::ComposerPtr
Track::composer() const
{
    return d->composer;
}

Meta::YearPtr
Track::year() const
{
    return d->year;
}

void
Track::setAlbum( const QString &newAlbum )
{
    DEBUG_BLOCK
    d->changes.insert( Meta::valAlbum, QVariant( newAlbum ) );
    debug() << "CHANGES HERE: " << d->changes;
    if( !d->batchUpdate )
    {
        d->m_data.album = newAlbum;
        d->writeMetaData();
        notifyObservers();
    }
}

void
Track::setAlbumArtist( const QString &newAlbumArtist )
{
    DEBUG_BLOCK
    d->changes.insert( Meta::valAlbumArtist, QVariant( newAlbumArtist ) );
    if( !d->batchUpdate )
    {
        d->m_data.albumArtist = newAlbumArtist;
        d->writeMetaData();
        notifyObservers();
    }
}

void
Track::setArtist( const QString& newArtist )
{
    d->changes.insert( Meta::valArtist, QVariant( newArtist ) );
    if( !d->batchUpdate )
    {
        d->m_data.artist = newArtist;
        d->writeMetaData();
        notifyObservers();
    }
}

void
Track::setGenre( const QString& newGenre )
{
    d->changes.insert( Meta::valGenre, QVariant( newGenre ) );
    if( !d->batchUpdate )
    {
        d->writeMetaData();
        notifyObservers();
    }
}

void
Track::setComposer( const QString& newComposer )
{
    d->changes.insert( Meta::valComposer, QVariant( newComposer ) );
    if( !d->batchUpdate )
    {
        d->m_data.composer = newComposer;
        d->writeMetaData();
        notifyObservers();
    }
}

void
Track::setYear( int newYear )
{
    d->changes.insert( Meta::valYear, QVariant( newYear ) );
    if( !d->batchUpdate )
    {
        d->m_data.year = newYear;
        d->writeMetaData();
        notifyObservers();
    }
}

void
Track::setTitle( const QString &newTitle )
{
    d->changes.insert( Meta::valTitle, QVariant( newTitle ) );
    if( !d->batchUpdate )
    {
        d->m_data.title = newTitle;
        d->writeMetaData();
        notifyObservers();
    }
}

void
Track::setBpm( const qreal newBpm )
{
    d->changes.insert( Meta::valBpm, QVariant( newBpm ) );
    if( !d->batchUpdate )
    {
        d->m_data.bpm = newBpm;
        d->writeMetaData();
        notifyObservers();
    }
}

qreal
Track::bpm() const
{
    const qreal bpm = d->m_data.bpm;
    return bpm;
}

QString
Track::comment() const
{
    const QString commentName = d->m_data.comment;
    if( !commentName.isEmpty() )
        return commentName;
    return QString();
}

void
Track::setComment( const QString& newComment )
{
    d->changes.insert( Meta::valComment, QVariant( newComment ) );
    if( !d->batchUpdate )
    {
        d->m_data.comment = newComment;
        d->writeMetaData();
        notifyObservers();
    }
}

double
Track::score() const
{

    if( d->provider )
        return d->provider->score();
    else
        return 0.0;
}

void
Track::setScore( double newScore )
{
    if( d->provider )
        d->provider->setScore( newScore );
}

int
Track::rating() const
{
    if( d->provider )
        return d->provider->rating();
    else
        return 0;
}

void
Track::setRating( int newRating )
{
    if( d->provider )
        d->provider->setRating( newRating );
}

int
Track::trackNumber() const
{
    return d->m_data.trackNumber;
}

void
Track::setTrackNumber( int newTrackNumber )
{
    d->changes.insert( Meta::valTrackNr, QVariant( newTrackNumber ) );
    if( !d->batchUpdate )
    {
        d->m_data.trackNumber = newTrackNumber;
        d->writeMetaData();
        notifyObservers();
    }
}

int
Track::discNumber() const
{
    return d->m_data.discNumber;
}

void
Track::setDiscNumber( int newDiscNumber )
{
    d->changes.insert( Meta::valDiscNr, QVariant ( newDiscNumber ) );
    if( !d->batchUpdate )
    {
        d->m_data.discNumber = newDiscNumber;
        d->writeMetaData();
        notifyObservers();
    }
}

qint64
Track::length() const
{
    qint64 length = d->m_data.length;
    if( length == -2 /*Undetermined*/ )
        length = 0;
    return length;
}

int
Track::filesize() const
{
    return d->m_data.fileSize;
}

int
Track::sampleRate() const
{
    int sampleRate = d->m_data.sampleRate;
    if( sampleRate == -2 /*Undetermined*/ )
        sampleRate = 0;
    return sampleRate;
}

int
Track::bitrate() const
{
   int bitrate = d->m_data.bitRate;
   if( bitrate == -2 /*Undetermined*/ )
       bitrate = 0;
   return bitrate;
}

QDateTime
Track::createDate() const
{
    if( d->m_data.created > 0 )
        return QDateTime::fromTime_t(d->m_data.created);
    else
        return QDateTime();
}

QDateTime
Track::lastPlayed() const
{
    if( d->provider )
        return d->provider->lastPlayed();
    else
        return QDateTime();
}

void
Track::setLastPlayed( const QDateTime &newTime )
{
    if( d->provider )
        d->provider->setLastPlayed( newTime );
}

QDateTime
Track::firstPlayed() const
{
    if( d->provider )
        return d->provider->firstPlayed();
    else
        return QDateTime();
}

void
Track::setFirstPlayed( const QDateTime &newTime )
{
    if( d->provider )
        d->provider->setFirstPlayed( newTime );
}

int
Track::playCount() const
{
    if( d->provider )
        return d->provider->playCount();
    else
        return 0;
}

void
Track::setPlayCount( int newCount )
{
    if( d->provider )
        d->provider->setPlayCount( newCount );
}

qreal
Track::replayGain( Meta::ReplayGainTag mode ) const
{
    switch( mode )
    {
    case Meta::ReplayGain_Track_Gain:
        return d->m_data.trackGain;
    case Meta::ReplayGain_Track_Peak:
        return d->m_data.trackPeak;
    case Meta::ReplayGain_Album_Gain:
        return d->m_data.albumGain;
    case Meta::ReplayGain_Album_Peak:
        return d->m_data.albumPeak;
    }
    return 0.0;
}

QString
Track::type() const
{
    return Amarok::extension( d->url.fileName() );
}

void
Track::beginMetaDataUpdate()
{
    d->batchUpdate = true;
}

void
Track::endMetaDataUpdate()
{
 DEBUG_LINE_INFO
    debug() << "CHANGES HERE: " << d->changes;
    d->writeMetaData();
    d->batchUpdate = false;
    notifyObservers();
}

void
Track::finishedPlaying( double playedFraction )
{
    if( d->provider )
        d->provider->played( playedFraction, Meta::TrackPtr( this ) );
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

bool
Track::hasCapabilityInterface( Capabilities::Capability::Type type ) const
{
    bool readlabel = false;
#ifdef HAVE_LIBLASTFM
    readlabel = true;
#endif
    return type == Capabilities::Capability::Editable ||
           type == Capabilities::Capability::Importable ||
           type == Capabilities::Capability::BookmarkThis ||
           type == Capabilities::Capability::WriteTimecode ||
           type == Capabilities::Capability::LoadTimecode ||
           ( type == Capabilities::Capability::ReadLabel && readlabel ) ||
           type == Capabilities::Capability::FindInSource;
}

Capabilities::Capability*
Track::createCapabilityInterface( Capabilities::Capability::Type type )
{
    switch( type )
    {
        case Capabilities::Capability::Editable:
            return new EditCapabilityImpl( this );

        case Capabilities::Capability::Importable:
            return new StatisticsCapabilityImpl( this );

        case Capabilities::Capability::BookmarkThis:
            return new Capabilities::BookmarkThisCapability( new BookmarkCurrentTrackPositionAction( 0 ) );

        case Capabilities::Capability::WriteTimecode:
            return new TimecodeWriteCapabilityImpl( this );

        case Capabilities::Capability::LoadTimecode:
            return new TimecodeLoadCapabilityImpl( this );

        case Capabilities::Capability::FindInSource:
            return new FindInSourceCapabilityImpl( this );

#ifdef HAVE_LIBLASTFM
       case Capabilities::Capability::ReadLabel:
           if( !d->readLabelCapability )
               d->readLabelCapability = new Capabilities::LastfmReadLabelCapability( this );
#endif

        default: // fall-through


        return 0;
    }
}

QImage
Track::getEmbeddedCover() const
{
    if( d->m_data.embeddedImage )
        return Meta::Tag::embeddedCover( d->url.path()  );

    return QImage();
}

#include "File.moc"
