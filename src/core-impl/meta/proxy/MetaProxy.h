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

#ifndef AMAROK_METAPROXY_H
#define AMAROK_METAPROXY_H

#include "MetaProxyWorker.h"

#include "core/meta/Meta.h"
#include "core/capabilities/Capability.h"

#include <QObject>

namespace Collections
{
    class TrackProvider;
}

namespace MetaProxy
{
    class Track;
    typedef KSharedPtr<Track> TrackPtr;
    class AMAROK_EXPORT Track : public Meta::Track
    {
        public:
            class Private;

            Track( const KUrl &url );
            virtual ~Track();

        // methods inherited from Meta::MetaCapability
            virtual bool hasCapabilityInterface( Capabilities::Capability::Type type ) const;
            virtual Capabilities::Capability* createCapabilityInterface( Capabilities::Capability::Type type );

        // methods inherited from Meta::MetaBase
            virtual QString name() const;
            virtual QString prettyName() const;
            virtual QString fullPrettyName() const;
            virtual QString sortableName() const;
            virtual QString fixedName() const;

            virtual void subscribe( Meta::Observer *observer );
            virtual void unsubscribe( Meta::Observer *observer );

        // methods inherited from Meta::Track
            virtual KUrl playableUrl() const;
            virtual QString prettyUrl() const;
            virtual QString uidUrl() const;

            virtual bool isPlayable() const;
            virtual Meta::AlbumPtr album() const;
            virtual Meta::ArtistPtr artist() const;
            virtual Meta::GenrePtr genre() const;
            virtual Meta::ComposerPtr composer() const;
            virtual Meta::YearPtr year() const;

            virtual Meta::LabelList labels() const;
            virtual qreal bpm() const;
            virtual QString comment() const;
            virtual double score() const;
            virtual void setScore( double newScore );
            virtual int rating() const;
            virtual void setRating( int newRating );
            virtual qint64 length() const;
            virtual int filesize() const;
            virtual int sampleRate() const;
            virtual int bitrate() const;
            virtual QDateTime createDate() const;
            virtual QDateTime modifyDate() const;
            virtual int trackNumber() const;
            virtual int discNumber() const;
            virtual QDateTime lastPlayed() const;
            virtual QDateTime firstPlayed() const;
            virtual int playCount() const;
            virtual qreal replayGain( Meta::ReplayGainTag mode ) const;

            virtual QString type() const;

            virtual void prepareToPlay();
            virtual void finishedPlaying( double playedFraction );

            virtual bool inCollection() const;
            virtual Collections::Collection *collection() const;

            virtual QString cachedLyrics() const;
            virtual void setCachedLyrics( const QString &lyrics );

            virtual void addLabel( const QString &label );
            virtual void addLabel( const Meta::LabelPtr &label );
            virtual void removeLabel( const Meta::LabelPtr &label );

            virtual bool operator==( const Meta::Track &track ) const;

        // custom MetaProxy methods
            virtual void setName( const QString &name );
            virtual void setAlbum( const QString &album );
            virtual void setAlbumArtist( const QString &artist );
            virtual void setArtist( const QString &artist );
            virtual void setGenre( const QString &genre );
            virtual void setComposer( const QString &composer );
            virtual void setYear( int year );
            virtual void setBpm( const qreal bpm );
            virtual void setTrackNumber( int number );
            virtual void setDiscNumber( int discNumber );
            virtual void setLength( qint64 length );

            /**
             * allows subclasses to create an instance of trackprovider which will only check the TrackProvider
             * passed to lookupTrack(TrackProvider*) for the real track.
             */
            Track( const KUrl &url, bool awaitLookupNotification );

            /**
             * MetaProxy will check the given trackprovider if it can provide the track for the proxy's url.
             */
            void lookupTrack( Collections::TrackProvider *provider );

            /**
             * MetaProxy will update the proxy with the track.
             */
            void updateTrack( Meta::TrackPtr track );

        private:
            void init( const KUrl &url, bool awaitLookupNotification );
            Private *const d;
    };

}

#endif
