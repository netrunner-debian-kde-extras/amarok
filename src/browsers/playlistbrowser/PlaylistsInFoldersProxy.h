/****************************************************************************************
 * Copyright (c) 2009 Bart Cerneels <bart.cerneels@kde.org>                             *
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

#ifndef PLAYLISTSINFOLDERSPROXY_H
#define PLAYLISTSINFOLDERSPROXY_H

#include "QtGroupingProxy.h"
#include "PlaylistBrowserModel.h"

#include <QAction>

class QAction;

class PlaylistsInFoldersProxy : public QtGroupingProxy
{
    Q_OBJECT
    public:
        PlaylistsInFoldersProxy( QAbstractItemModel *model );
        ~PlaylistsInFoldersProxy();

        /* PlaylistInGroupsProxy methods */
        QModelIndex createNewFolder( const QString &groupName );

        /* QtGroupingProxy methods */
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

    private slots:
        void slotRename( QModelIndex idx );

        void slotDeleteFolder();
        void slotRenameFolder();

    private:
        QList<QAction *> createGroupActions();
        void deleteFolder( const QModelIndex &groupIdx );

        QAction *m_renameFolderAction;
        QAction *m_deleteFolderAction;
};

#endif //PLAYLISTSINFOLDERSPROXY_H
