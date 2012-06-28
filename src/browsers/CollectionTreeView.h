/****************************************************************************************
 * Copyright (c) 2007 Alexandre Pereira de Oliveira <aleprj@gmail.com>                  *
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

#ifndef COLLECTIONTREEVIEW_H
#define COLLECTIONTREEVIEW_H

#include "BrowserDefines.h"
#include "widgets/PrettyTreeView.h"
#include "core/meta/Meta.h"
#include "playlist/PlaylistController.h"

#include <QModelIndex>
#include <QMutex>
#include <QSet>

class AmarokMimeData;
class CollectionSortFilterProxyModel;
class CollectionTreeItemModelBase;
class CollectionTreeItem;
class PopupDropper;
class QAction;
class QSortFilterProxyModel;

typedef QList<QAction *> QActionList;

class CollectionTreeView: public Amarok::PrettyTreeView
{
        Q_OBJECT

    public:
        explicit CollectionTreeView( QWidget *parent = 0 );
        ~CollectionTreeView();

        QSortFilterProxyModel* filterModel() const;

        AMAROK_EXPORT void setLevels( const QList<CategoryId::CatMenuId> &levels );
        QList<CategoryId::CatMenuId> levels() const;

        void setLevel( int level, CategoryId::CatMenuId type );

        void setModel( QAbstractItemModel *model );

        //Helper function to remove children if their parent is already present
        static QSet<CollectionTreeItem*> cleanItemSet( const QSet<CollectionTreeItem*> &items );

    public slots:
        void slotSetFilter( const QString &filter );

        void playChildTracksSlot( Meta::TrackList list );

    protected:
        void contextMenuEvent( QContextMenuEvent *event );
        void mouseDoubleClickEvent( QMouseEvent *event );
        void mouseMoveEvent( QMouseEvent *event );
        void mousePressEvent( QMouseEvent *event );
        void mouseReleaseEvent( QMouseEvent *event );
        void keyPressEvent( QKeyEvent * event );
        void dragEnterEvent( QDragEnterEvent *event );
        void dragMoveEvent( QDragMoveEvent *event );
        void startDrag( Qt::DropActions supportedActions );

    protected slots:
        virtual void selectionChanged ( const QItemSelection & selected, const QItemSelection & deselected );
        void slotCollapsed( const QModelIndex &index );
        void slotExpanded( const QModelIndex &index );
        void slotExpandIndex( const QModelIndex &index );

        void slotCheckAutoExpand();

        void slotPlayChildTracks();
        void slotAppendChildTracks();
        void slotQueueChildTracks();
        void slotEditTracks();
        void slotCopyTracks();
        void slotMoveTracks();
        void slotTrashTracks();
        void slotDeleteTracks();
        void slotOrganize();

    private:
        // Utility function to play all items
        // that have this as a parent..
        void playChildTracks( CollectionTreeItem *item, Playlist::AddOptions insertMode );
        void playChildTracks( const QSet<CollectionTreeItem*> &items, Playlist::AddOptions insertMode );
        void editTracks( const QSet<CollectionTreeItem*> &items ) const;
        void organizeTracks( const QSet<CollectionTreeItem*> &items ) const;
        void copyTracks( const QSet<CollectionTreeItem*> &items, Collections::Collection *destination,
                         bool removeSources) const;
        void removeTracks( const QSet<CollectionTreeItem*> &items, bool useTrash ) const;

        // creates different actions from the different objects.
        // note: you should not delete the created actions.
        QActionList createBasicActions( const QModelIndexList &indcies );
        QActionList createExtendedActions( const QModelIndexList &indcies );
        QActionList createCollectionActions( const QModelIndexList &indices );
        QActionList createCustomActions( const QModelIndexList &indices );

        bool onlyOneCollection( const QModelIndexList &indices );
        Collections::Collection *getCollection( const QModelIndex &index );
        QHash<QAction*, Collections::Collection*> getCopyActions( const QModelIndexList &indcies );
        QHash<QAction*, Collections::Collection*> getMoveActions( const QModelIndexList &indcies );

        Collections::QueryMaker* createMetaQueryFromItems( const QSet<CollectionTreeItem*> &items, bool cleanItems=true ) const;
        CollectionTreeItem* getItemFromIndex( QModelIndex &index );

        CollectionSortFilterProxyModel *m_filterModel;
        CollectionTreeItemModelBase *m_treeModel;
        PopupDropper* m_pd;
        QAction* m_appendAction;
        QAction* m_loadAction;
        QAction* m_editAction;
        QAction* m_organizeAction;

        QHash<QAction*, Collections::Collection*> m_currentCopyDestination;
        QHash<QAction*, Collections::Collection*> m_currentMoveDestination;

        QMap<AmarokMimeData*, Playlist::AddOptions> m_playChildTracksMode;

        QSet<CollectionTreeItem*> m_currentItems;

        QMutex m_dragMutex;
        bool m_ongoingDrag;
        bool m_expandToggledWhenPressed;

    signals:
        void itemSelected( CollectionTreeItem * item );
        void leavingTree();
};

#endif
