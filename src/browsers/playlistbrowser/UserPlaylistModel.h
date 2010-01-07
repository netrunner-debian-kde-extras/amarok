/****************************************************************************************
 * Copyright (c) 2008 Nikolaj Hald Nielsen <nhn@kde.org>                                *
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

#ifndef USERPLAYLISTMODEL_H
#define USERPLAYLISTMODEL_H

#include "MetaPlaylistModel.h"
#include "Meta.h"
#include "meta/Playlist.h"

#include <QAction>
#include <QAbstractItemModel>
#include <QModelIndex>
#include <QVariant>

#define PLAYLIST_DB_VERSION 1

namespace PlaylistBrowserNS {

/**
        @author Nikolaj Hald Nielsen <nhn@kde.org>
*/
class UserModel : public QAbstractItemModel, public MetaPlaylistModel,
                  public Meta::PlaylistObserver
{
    Q_OBJECT
    public:
        enum {
            PlaylistColumn = 0, //Data form the playlist itself
            GroupColumn = 1, //Data form the group (a.k.a. folder) the playlist is in.
            ProviderColumn = 2 //data form the PlaylistProvider
        };
        static UserModel * instance();
        static void destroy();

        ~UserModel();

        virtual QVariant data( const QModelIndex &index, int role ) const;
        virtual Qt::ItemFlags flags( const QModelIndex &index ) const;
        virtual QVariant headerData( int section, Qt::Orientation orientation,
                            int role = Qt::DisplayRole ) const;
        virtual QModelIndex index( int row, int column,
                        const QModelIndex &parent = QModelIndex() ) const;
        virtual QModelIndex parent( const QModelIndex &index ) const;
        virtual int rowCount( const QModelIndex &parent = QModelIndex() ) const;
        virtual int columnCount( const QModelIndex &parent = QModelIndex() ) const;
        virtual bool setData( const QModelIndex &index, const QVariant &value, int role = Qt::EditRole );
        virtual bool removeRows( int row, int count, const QModelIndex & parent = QModelIndex() );

        virtual Qt::DropActions supportedDropActions() const {
            return Qt::CopyAction | Qt::MoveAction;
        }

        virtual Qt::DropActions supportedDragActions() const {
            return Qt::MoveAction | Qt::CopyAction;
        }

        virtual QStringList mimeTypes() const;
        QMimeData* mimeData( const QModelIndexList &indexes ) const;
        bool dropMimeData ( const QMimeData * data, Qt::DropAction action, int row, int column, const QModelIndex & parent );

        QList<QAction *> actionsFor( const QModelIndexList &indexes );

        void loadItems( QModelIndexList list, Playlist::AddOptions insertMode );

        /* UserPlaylistModel specific methods */
        Meta::PlaylistList selectedPlaylists() { return m_selectedPlaylists; }
        Meta::TrackList selectedTracks() { return m_selectedTracks; }

        /* Meta::PlaylistObserver methods */
        virtual void trackAdded( Meta::PlaylistPtr playlist, Meta::TrackPtr track,
                                 int position );
        virtual void trackRemoved( Meta::PlaylistPtr playlist, int position );

    public slots:
        void slotLoad();
        void slotAppend();

        void slotRename();
        void slotDelete(); // Deletes playlists

        void slotUpdate();
        void slotRenamePlaylist( Meta::PlaylistPtr playlist );

    signals:
        void renameIndex( const QModelIndex & index );
        void rowsInserted( const QModelIndex & parent, int start, int end );

    private:
        UserModel();
        void loadPlaylists();

        static UserModel * s_instance;

        Meta::PlaylistList m_playlists;
        QList<QAction *> createCommonActions( QModelIndexList indices );
        QList<QAction *> createWriteActions( QModelIndexList indices );
        QAction *m_appendAction;
        QAction *m_loadAction;
        QAction *m_renameAction;
        QAction *m_deleteAction;

        Meta::PlaylistList m_selectedPlaylists;
        Meta::PlaylistList selectedPlaylists( const QModelIndexList &list );
        Meta::TrackList m_selectedTracks;
        Meta::TrackList selectedTracks( const QModelIndexList &list );
        Meta::TrackPtr trackFromIndex( const QModelIndex &index ) const;
};

}

namespace The {
    AMAROK_EXPORT PlaylistBrowserNS::UserModel* userPlaylistModel();
}
#endif
