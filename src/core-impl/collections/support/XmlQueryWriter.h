/****************************************************************************************
 * Copyright (c) 2008 Daniel Caleb Jones <danielcjones@gmail.com>                       *
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

#ifndef AMAROK_XMLQUERYWRITER_H
#define AMAROK_XMLQUERYWRITER_H

#include "core/collections/QueryMaker.h"

#include <QDomDocument>
#include <QDomElement>

namespace Collections {

/**
 * A special query maker that acts as a wrapper for another query maker. It
 * behaves just like the query maker it wraps, but records the query
 * to xml suitable to be read by XmlQueryReader. 
 */
class XmlQueryWriter : public QueryMaker
{
    public:
        XmlQueryWriter( QueryMaker*, QDomDocument );
        ~XmlQueryWriter();

        QString     getXml( int indent = 0 );
        QDomElement getDomElement() const;
        QueryMaker* getEmbeddedQueryMaker() const;

        void run();
        void abortQuery();
        int resultCount() const;

        QueryMaker* setQueryType( QueryType type );
        QueryMaker* setReturnResultAsDataPtrs( bool resultAsDataPtrs );

        QueryMaker* addReturnValue( qint64 value );
        QueryMaker* addReturnFunction( ReturnFunction function, qint64 value );
        QueryMaker* orderBy( qint64 value, bool descending = false );
        QueryMaker* orderByRandom();

        QueryMaker* includeCollection( const QString &collectionId );
        QueryMaker* excludeCollection( const QString &collectionId );

        QueryMaker* addMatch( const Meta::TrackPtr &track );
        QueryMaker* addMatch( const Meta::ArtistPtr &artist );
        QueryMaker* addMatch( const Meta::AlbumPtr &album );
        QueryMaker* addMatch( const Meta::ComposerPtr &composer );
        QueryMaker* addMatch( const Meta::GenrePtr &genre );
        QueryMaker* addMatch( const Meta::YearPtr &year );
        QueryMaker* addMatch( const Meta::LabelPtr &label );

        QueryMaker* addFilter( qint64 value, const QString &filter, bool matchBegin = false, bool matchEnd = false );
        QueryMaker* excludeFilter( qint64 value, const QString &filter, bool matchBegin = false, bool matchEnd = false );

        QueryMaker* addNumberFilter( qint64 value, qint64 filter, NumberComparison compare );
        QueryMaker* excludeNumberFilter( qint64 value, qint64 filter, NumberComparison compare );

        QueryMaker* limitMaxResultSize( int size );

        QueryMaker* setAlbumQueryMode( AlbumQueryMode mode );
        QueryMaker* setArtistQueryMode( ArtistQueryMode mode );

        QueryMaker* beginAnd();
        QueryMaker* beginOr();
        QueryMaker* endAndOr();


        int validFilterMask();

        /**
         * Creates the dom element for one filter.
         * This is the equivalent function to XmlQueryReader::readFilter.
         */
        static QDomElement xmlForFilter( QDomDocument doc, bool exclude, quint64 field, QString value);
        static QDomElement xmlForFilter( QDomDocument doc, bool exclude, quint64 field, quint64 numValue, NumberComparison compare);

        static QString compareName( QueryMaker::NumberComparison );

    private:
        void insertRetValue( QString );

        QueryMaker* m_qm;
        QDomDocument m_doc;
        QDomElement m_element;
        QDomElement m_filterElement;
        QDomElement m_retvalElement;
        int m_andorLevel;
};

} //namespace Collections

#endif

