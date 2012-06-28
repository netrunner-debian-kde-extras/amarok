/****************************************************************************************
 * Copyright (c) 2012 Jasneet Singh Bhatti <jazneetbhatti@gmail.com>                    *
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

#ifndef COLLECTIONS_MOCKQUERYMAKER
#define COLLECTIONS_MOCKQUERYMAKER

#include "core/collections/QueryMaker.h"

using namespace Collections;

/**
 * Ad-hoc mock to test the QueryMaker class
 */
class MockQueryMaker : public QueryMaker
{
    Q_OBJECT

    public:
        /**
         * For the vtable generated by the compiler
         */
        virtual ~MockQueryMaker();

        /**
         * Mock implementations of pure virtual methods of class Collections::QueryMaker
         * to enable creation of an instance of this mock class
         *
         * NOT TO BE USED ANYWHERE IN THE TEST
         */
        virtual void run()
        {
            Q_ASSERT_X( false, __PRETTY_FUNCTION__, "should not be called");
        }

        virtual void abortQuery()
        {
            Q_ASSERT_X( false, __PRETTY_FUNCTION__, "should not be called");
        }

        virtual QueryMaker *setQueryType( QueryType )
        {
            Q_ASSERT_X( false, __PRETTY_FUNCTION__, "should not be called");
            return 0;
        }

        virtual QueryMaker *addReturnValue( qint64 )
        {
            Q_ASSERT_X( false, __PRETTY_FUNCTION__, "should not be called");
            return 0;
        }

        virtual QueryMaker *addReturnFunction( ReturnFunction, qint64 )
        {
            Q_ASSERT_X( false, __PRETTY_FUNCTION__, "should not be called");
            return 0;
        }

        virtual QueryMaker *orderBy( qint64, bool )
        {
            Q_ASSERT_X( false, __PRETTY_FUNCTION__, "should not be called");
            return 0;
        }

        virtual QueryMaker *addMatch( const Meta::TrackPtr& )
        {
            Q_ASSERT_X( false, __PRETTY_FUNCTION__, "should not be called");
            return 0;
        }

        virtual QueryMaker *addMatch( const Meta::ArtistPtr& )
        {
            Q_ASSERT_X( false, __PRETTY_FUNCTION__, "should not be called");
            return 0;
        }

        virtual QueryMaker *addMatch( const Meta::AlbumPtr& )
        {
            Q_ASSERT_X( false, __PRETTY_FUNCTION__, "should not be called");
            return 0;
        }

        virtual QueryMaker *addMatch( const Meta::ComposerPtr& )
        {
            Q_ASSERT_X( false, __PRETTY_FUNCTION__, "should not be called");
            return 0;
        }

        virtual QueryMaker *addMatch( const Meta::GenrePtr& )
        {
            Q_ASSERT_X( false, __PRETTY_FUNCTION__, "should not be called");
            return 0;
        }

        virtual QueryMaker *addMatch( const Meta::YearPtr& )
        {
            Q_ASSERT_X( false, __PRETTY_FUNCTION__, "should not be called");
            return 0;
        }

        virtual QueryMaker *addFilter( qint64, const QString&, bool, bool )
        {
            Q_ASSERT_X( false, __PRETTY_FUNCTION__, "should not be called");
            return 0;
        }

        virtual QueryMaker *excludeFilter( qint64, const QString&, bool, bool )
        {
            Q_ASSERT_X( false, __PRETTY_FUNCTION__, "should not be called");
            return 0;
        }

        virtual QueryMaker *addNumberFilter( qint64, qint64, NumberComparison )
        {
            Q_ASSERT_X( false, __PRETTY_FUNCTION__, "should not be called");
            return 0;
        }

        virtual QueryMaker *excludeNumberFilter( qint64, qint64, NumberComparison )
        {
            Q_ASSERT_X( false, __PRETTY_FUNCTION__, "should not be called");
            return 0;
        }

        virtual QueryMaker *limitMaxResultSize( int )
        {
            Q_ASSERT_X( false, __PRETTY_FUNCTION__, "should not be called");
            return 0;
        }

        virtual QueryMaker *beginAnd()
        {
            Q_ASSERT_X( false, __PRETTY_FUNCTION__, "should not be called");
            return 0;
        }

        virtual QueryMaker *beginOr()
        {
            Q_ASSERT_X( false, __PRETTY_FUNCTION__, "should not be called");
            return 0;
        }

        virtual QueryMaker *endAndOr()
        {
            Q_ASSERT_X( false, __PRETTY_FUNCTION__, "should not be called");
            return 0;
        }

        /**
         * This method helps to determine if queryDone() has been connected/disconnected
         * with slot deleteLater()
         */
        virtual void emitQueryDone()
        {
             emit queryDone();
        }

    public slots:
        /**
         * Overrides the default deleteLater() slot provided by QObject since the default
         * slot implements a deferred delete of the object and is not easily testable
         * We only need to test if the slot is triggered or not
         */
        virtual void deleteLater()
        {
            emit destroyed();
        }
};

#endif // COLLECTIONS_MOCKQUERYMAKER_H
