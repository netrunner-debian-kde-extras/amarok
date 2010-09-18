/****************************************************************************************
 * Copyright (c) 2010 Bart Cerneels <bart.cerneels@kde.org>                             *
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
#ifndef PLAYLISTSBYPROVIDERPROXY_H
#define PLAYLISTSBYPROVIDERPROXY_H

#include "QtGroupingProxy.h"
#include "PlaylistBrowserModel.h"

#include <QAction>

class PlaylistsByProviderProxy : public QtGroupingProxy
{
    Q_OBJECT
    public:
        //TODO: move these internal drag and drop functions to QtGroupingProxy
        /** serializes the indexes into a bytearray
          */
        QByteArray encodeMimeRows( const QList<QModelIndex> indexes ) const;

        /** \arg data serialized data
          * \model this model is used to get the indexes.
          */
        QList<QModelIndex> decodeMimeRows( QByteArray data, QAbstractItemModel *model ) const;
        static const QString AMAROK_PROVIDERPROXY_INDEXES;

        PlaylistsByProviderProxy( QAbstractItemModel *model, int column );
        ~PlaylistsByProviderProxy() {}

        /* QtGroupingProxy methods */
        /* reimplement to handle tracks with multiple providers (synced) */
        virtual QVariant data( const QModelIndex &idx, int role ) const;

        /* QAbstractModel methods */
        virtual bool removeRows( int row, int count,
                                 const QModelIndex &parent = QModelIndex() );
        virtual QStringList mimeTypes() const;
        virtual QMimeData *mimeData( const QModelIndexList &indexes ) const;
        virtual bool dropMimeData( const QMimeData *data, Qt::DropAction action,
                                   int row, int column, const QModelIndex &parent );

        virtual Qt::DropActions supportedDropActions() const;
        virtual Qt::DropActions supportedDragActions() const;

    signals:
        void renameIndex( QModelIndex idx );

    protected slots:
        virtual void buildTree();

    private slots:
        void slotRename( QModelIndex idx );

};

#endif // PLAYLISTSBYPROVIDERPROXY_H
