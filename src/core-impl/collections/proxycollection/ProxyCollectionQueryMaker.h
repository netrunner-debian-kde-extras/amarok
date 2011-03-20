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
#ifndef PROXYCOLLECTIONQUERYMAKER_H
#define PROXYCOLLECTIONQUERYMAKER_H

#include "core/collections/QueryMaker.h"
#include "core/collections/Collection.h"
#include "ProxyCollectionMeta.h"

#include <QList>
#include <QMutex>
#include <QSet>

#include <KRandomSequence>
#include <KSharedPtr>

class CustomReturnFunction;
class CustomReturnValue;

namespace Collections
{

class Collection;

class AMAROK_EXPORT_TESTS ProxyQueryMaker : public QueryMaker
{
    Q_OBJECT

    public:
        ProxyQueryMaker( Collections::ProxyCollection *collection, const QList<QueryMaker*> &queryMakers );
        ~ProxyQueryMaker();

        virtual void run();
        virtual void abortQuery();
        virtual int resultCount() const;

        virtual QueryMaker* setQueryType( QueryType type );

        virtual QueryMaker* setReturnResultAsDataPtrs( bool resultAsDataPtrs );

        virtual QueryMaker* addReturnValue( qint64 value);
        virtual QueryMaker* addReturnFunction( ReturnFunction function, qint64 value );
        virtual QueryMaker* orderBy( qint64 value, bool descending = false );
        virtual QueryMaker* orderByRandom();

        virtual QueryMaker* addMatch( const Meta::TrackPtr &track );
        virtual QueryMaker* addMatch( const Meta::ArtistPtr &artist );
        virtual QueryMaker* addMatch( const Meta::AlbumPtr &album );
        virtual QueryMaker* addMatch( const Meta::ComposerPtr &composer );
        virtual QueryMaker* addMatch( const Meta::GenrePtr &genre );
        virtual QueryMaker* addMatch( const Meta::YearPtr &year );
        virtual QueryMaker* addMatch( const Meta::LabelPtr &label );

        virtual QueryMaker* addFilter( qint64 value, const QString &filter, bool matchBegin, bool matchEnd );
        virtual QueryMaker* excludeFilter( qint64 value, const QString &filter, bool matchBegin, bool matchEnd );

        virtual QueryMaker* addNumberFilter( qint64 value, qint64 filter, QueryMaker::NumberComparison compare );
        virtual QueryMaker* excludeNumberFilter( qint64 value, qint64 filter, QueryMaker::NumberComparison compare );

        virtual QueryMaker* limitMaxResultSize( int size );

        virtual QueryMaker* beginAnd();
        virtual QueryMaker* beginOr();
        virtual QueryMaker* endAndOr();

        virtual QueryMaker* setAlbumQueryMode( AlbumQueryMode mode );
        virtual QueryMaker* setArtistQueryMode( ArtistQueryMode mode );
        virtual QueryMaker* setLabelQueryMode( LabelQueryMode mode );

    private:
        template <class PointerType>
        void emitProperResult( const QList<PointerType > &list );

        void handleResult();

    private slots:
        void slotQueryDone();
        void slotNewResultReady( const QString &collectionId, const Meta::TrackList &tracks );
        void slotNewResultReady( const QString &collectionId, const Meta::ArtistList &artists );
        void slotNewResultReady( const QString &collectionId, const Meta::AlbumList &albums );
        void slotNewResultReady( const QString &collectionId, const Meta::GenreList &genres );
        void slotNewResultReady( const QString &collectionId, const Meta::ComposerList &composers );
        void slotNewResultReady( const QString &collectionId, const Meta::YearList &years );
        void slotNewResultReady( const QString &collectionId, const Meta::LabelList &labels );

    private:
        ProxyCollection *m_collection;
        QList<QueryMaker*> m_builders;
        int m_queryDoneCount;
        bool m_returnDataPointers;
        int m_maxResultSize;
        bool m_randomize;
        QueryType m_queryType;
        bool m_orderDescending;
        qint64 m_orderField;
        bool m_orderByNumberField;
        QMutex m_queryDoneCountMutex;
        //store ProxyCollection meta stuff using KSharedPtr,
        //otherwise ProxyCollection might delete it (as soon as it gets garbage collection)
        QSet<KSharedPtr<Meta::ProxyTrack> > m_tracks;
        QSet<KSharedPtr<Meta::ProxyArtist> > m_artists;
        QSet<KSharedPtr<Meta::ProxyAlbum> > m_albums;
        QSet<KSharedPtr<Meta::ProxyGenre> > m_genres;
        QSet<KSharedPtr<Meta::ProxyComposer> > m_composers;
        QSet<KSharedPtr<Meta::ProxyYear> > m_years;
        QSet<KSharedPtr<Meta::ProxyLabel> > m_labels;
        KRandomSequence m_sequence; //do not reset
        QList<CustomReturnFunction*> m_returnFunctions;
        QList<CustomReturnValue*> m_returnValues;

};

}

#endif /* PROXYCOLLECTIONQUERYMAKER_H */
