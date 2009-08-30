/****************************************************************************************
 * Copyright (c) 2008 Edward Toroshchin <edward.hades@gmail.com>                        *
 * Copyright (c) 2009 Jeff Mitchell <mitchell@kde.org>                                  *
 *                                                                                      *
 * This program is free software; you can redistribute it and/or modify it under        *
 * the terms of the GNU General Public License as published by the Free Software        *
 * Foundation; either version 2 of the License, or (at your option) any later           *
 * version.                                                                             *
 *                                                                                      *
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY      *
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A      *
 * PARTICULAR PURPOSE. See the GNU General Pulic License for more details.              *
 *                                                                                      *
 * You should have received a copy of the GNU General Public License along with         *
 * this program.  If not, see <http://www.gnu.org/licenses/>.                           *
 ****************************************************************************************/

#ifndef AMAROK_COLLECTION_MYSQLEMBEDDEDCOLLECTION_H
#define AMAROK_COLLECTION_MYSQLEMBEDDEDCOLLECTION_H

#include "mysql-shared/MySqlCollection.h"
#include "Collection.h"
#include "amarok_export.h"

class MySqlEmbeddedCollectionFactory : public Amarok::CollectionFactory
{
    Q_OBJECT

    public:
        MySqlEmbeddedCollectionFactory() {}
        virtual ~MySqlEmbeddedCollectionFactory() {}

        virtual void init();
};

/**
 * Implements a MySqlCollection using a MySQL Embedded Server
 */
class MySqlEmbeddedCollection : public MySqlCollection
{
    Q_OBJECT

    public:
        MySqlEmbeddedCollection( const QString &id, const QString &prettyName );
        virtual ~MySqlEmbeddedCollection();

        virtual QString type() const;
};

#endif
