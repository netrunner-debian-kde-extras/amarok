/****************************************************************************************
 * Copyright (c) 2007-2008 Maximilian Kossick <maximilian.kossick@googlemail.com>       *
 * Copyright (c) 2008 Mark Kretschmann <kretschmann@kde.org>                            *
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

#ifndef AMAROK_STREAM_P_H
#define AMAROK_STREAM_P_H

#include "core/support/Debug.h"
#include "core/meta/Meta.h"
#include "core/meta/support/MetaConstants.h"
#include "covermanager/CoverCache.h"
#include "EngineController.h"

#include <Phonon/Global>

#include <QList>
#include <QObject>
#include <QPixmap>

using namespace MetaStream;

class MetaStream::Track::Private : public QObject
{
    Q_OBJECT

    public:
        Private( Track *t )
            : track( t )
        {
            EngineController *engine = The::engineController();

            connect( engine, SIGNAL( currentMetadataChanged( QVariantMap ) ),
                     this, SLOT( currentMetadataChanged( QVariantMap ) ) );
        }

        void notify() const
        {
            DEBUG_BLOCK

            foreach( Meta::Observer *observer, observers )
            {
                debug() << "Notifying observer: " << observer;
                observer->metadataChanged( Meta::TrackPtr( const_cast<MetaStream::Track*>( track ) ) );
            }
        }

    public Q_SLOTS:
        void currentMetadataChanged( QVariantMap metaData )
        {
            DEBUG_BLOCK
            if( metaData.value( Meta::Field::URL ) == url.url() )
            {
                DEBUG_BLOCK
                debug() << "Applying new Metadata.";

                if( metaData.contains( Meta::Field::ARTIST ) )
                    artist = metaData.value( Meta::Field::ARTIST ).toString();
                if( metaData.contains( Meta::Field::TITLE ) )
                    title = metaData.value( Meta::Field::TITLE ).toString();
                if( metaData.contains( Meta::Field::ALBUM ) )
                    album = metaData.value( Meta::Field::ALBUM ).toString();

                // Special demangling of artist/title for Shoutcast streams, which usually have "Artist - Title" in the title tag:
                if( artist.isEmpty() && title.contains( " - " ) ) {
                    const QStringList artist_title = title.split( " - " );
                    if( artist_title.size() >= 2 ) {
                        artist = artist_title[0];
                        title  = title.remove( 0, artist.length() + 3 );
                    }
                }

                notify();
            }
        }

    public:
        QSet<Meta::Observer*> observers;
        KUrl url;
        QString title;
        QString artist;
        QString album;

        Meta::ArtistPtr artistPtr;
        Meta::AlbumPtr albumPtr;
        Meta::GenrePtr genrePtr;
        Meta::ComposerPtr composerPtr;
        Meta::YearPtr yearPtr;

    private:
        Track *track;
};


// internal helper classes

class StreamArtist : public Meta::Artist
{
    public:
        StreamArtist( MetaStream::Track::Private *dptr )
            : Meta::Artist()
            , d( dptr )
            {}

        Meta::TrackList tracks()
        {
            return Meta::TrackList();
        }

        Meta::AlbumList albums()
        {
            return Meta::AlbumList();
        }

        QString name() const
        {
            if( d )
                return d->artist;
            return QString();
        }

        QString prettyName() const
        {
            return name();
        }

        bool operator==( const Meta::Artist &other ) const {
            return name() == other.name();
        }

        MetaStream::Track::Private * const d;
};

class StreamAlbum : public Meta::Album
{
public:
    StreamAlbum( MetaStream::Track::Private *dptr )
        : Meta::Album()
        , d( dptr )
    {}

    ~StreamAlbum()
    {
        CoverCache::invalidateAlbum( this );
    }

    bool isCompilation() const
    {
        return false;
    }

    bool hasAlbumArtist() const
    {
        return false;
    }

    Meta::ArtistPtr albumArtist() const
    {
        return Meta::ArtistPtr();
    }

    Meta::TrackList tracks()
    {
        return Meta::TrackList();
    }

    QString name() const
    {
        if( d )
            return d->album;
        return QString();
    }

    QString prettyName() const
    {
        return name();
    }

    QImage image( int size ) const
    {
        if( m_cover.isNull() )
            return Meta::Album::image( size );
        else
            return m_cover.scaled( size, size, Qt::KeepAspectRatio, Qt::SmoothTransformation );
    }

    void setImage( const QImage &image )
    {
        m_cover = image;
        CoverCache::invalidateAlbum( this );
    }


    bool operator==( const Meta::Album &other ) const {
        return name() == other.name();
    }

    MetaStream::Track::Private * const d;
    QImage m_cover;
};


#endif

