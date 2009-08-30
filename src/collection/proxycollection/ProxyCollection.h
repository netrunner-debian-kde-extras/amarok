/*
 *  Copyright (c) 2009 Maximilian Kossick <maximilian.kossick@googlemail.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifndef PROXYCOLLECTION_H
#define PROXYCOLLECTION_H

#include "collection/Collection.h"
#include "meta/Meta.h"

#include <QString>
#include <QHash>
#include <QReadWriteLock>

struct AlbumKey
{
    QString albumName;
    QString artistName;

    AlbumKey &operator=( const AlbumKey &o )
    { albumName = o.albumName; artistName = o.artistName; return *this; }
};

struct TrackKey
{
    QString trackName;
    QString albumName;
    QString artistName;
    //more?

    TrackKey &operator=( const TrackKey &o )
    { trackName = o.trackName; albumName = o.albumName; artistName = o.artistName; return *this; }
};

namespace ProxyCollection
{
    class Year;
    class Track;
    class Artist;
    class Album;
    class Genre;
    class Composer;





    class Collection : public Amarok::Collection
    {
        Q_OBJECT
        public:
        Collection();
        ~Collection();

        //Amarok::Collection methods

        virtual QString prettyName() const;
        virtual KIcon icon() const;

        virtual bool possiblyContainsTrack( const KUrl &url ) const;
        virtual Meta::TrackPtr trackForUrl( const KUrl &url );

        virtual QueryMaker* queryMaker();

        virtual QString collectionId() const;

        //ProxyCollection::Collection methods

        void removeTrack( const TrackKey &key );
        Track* getTrack( Meta::TrackPtr track );
        void setTrack( ProxyCollection::Track *track );
        bool hasTrack( const TrackKey &key );

        void removeAlbum( const QString &album, const QString &albumArtist );
        Album* getAlbum( Meta::AlbumPtr album );
        void setAlbum( ProxyCollection::Album *album );
        bool hasAlbum( const QString &album, const QString &albumArtist );

        void removeArtist( const QString &artist );
        Artist* getArtist( Meta::ArtistPtr artist );
        void setArtist( ProxyCollection::Artist *artist );
        bool hasArtist( const QString &artist );

        void removeGenre( const QString &genre );
        Genre* getGenre( Meta::GenrePtr genre );
        void setGenre( ProxyCollection::Genre *genre );
        bool hasGenre( const QString &genre );

        void removeComposer( const QString &name );
        Composer* getComposer( Meta::ComposerPtr composer );
        void setComposer( ProxyCollection::Composer *composer );
        bool hasComposer( const QString &name );

        bool hasYear( const QString &name );
        void removeYear( const QString &name );
        Year* getYear( Meta::YearPtr year );
        void setYear( ProxyCollection::Year *year );

        public slots:
        void removeCollection( const QString &collectionId );
        void removeCollection( Amarok::Collection *collection );
        void addCollection( Amarok::Collection *collection );

        private slots:
        void slotUpdated();
        void emptyCache();

        private:
        QHash<QString, Amarok::Collection*> m_idCollectionMap;

        QHash<QString, KSharedPtr<Year> > m_yearMap;
        QHash<QString, KSharedPtr<Genre> > m_genreMap;
        QHash<QString, KSharedPtr<Composer> > m_composerMap;
        QHash<QString, KSharedPtr<Artist> > m_artistMap;
        QHash<AlbumKey, KSharedPtr<Album> > m_albumMap;
        QHash<TrackKey, KSharedPtr<Track> > m_trackMap;

        QReadWriteLock m_yearLock;
        QReadWriteLock m_genreLock;
        QReadWriteLock m_composerLock;
        QReadWriteLock m_artistLock;
        QReadWriteLock m_albumLock;
        QReadWriteLock m_trackLock;

    };

    TrackKey keyFromTrack( const Meta::TrackPtr &track );
}

inline bool
operator==( const TrackKey &k1, const TrackKey &k2 )
{
    return k1.trackName == k2.trackName &&
                          k1.albumName == k2.albumName &&
                          k1.artistName == k2.artistName;
}

inline uint
qHash( const TrackKey &key )
{
    return qHash( key.trackName ) + 17 * qHash( key.albumName ) + 31 * qHash( key.artistName );
}

inline bool
operator==( const AlbumKey &k1, const AlbumKey &k2 )
{
    return k1.albumName == k2.albumName && k1.artistName == k2.artistName;
}

inline uint
qHash( const AlbumKey &key )
{
    return qHash( key.albumName ) + 17 * qHash( key.artistName );
}

#endif
