/****************************************************************************************
 * Copyright (c) 2010 Nikolaj Hald Nielsen <nhn@kde.org>                                *
 * Copyright (c) 2010 Casey Link <unnamedrambler@gmail.com>                             *
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

#ifndef AMAROK_FILEVIEW_H
#define AMAROK_FILEVIEW_H

#include "core/collections/Collection.h"
#include "widgets/PrettyTreeView.h"

#include <KFileItem>

#include <QAction>
#include <QList>
#include <QTreeView>
#include <QMutex>

class PopupDropper;

/**
* Stores a collection associated with an action for move/copy to collection
*/
class CollectionAction : public QAction
{
    public:
        explicit CollectionAction( Collections::Collection *coll, QObject *parent = 0 )
        : QAction( parent )
        , m_collection( coll )
        {
            setText( m_collection->prettyName() );
        }

        Collections::Collection *collection() const
        {
            return m_collection;
        }

    private:
        Collections::Collection *m_collection;
};


class FileView : public Amarok::PrettyTreeView
{
    Q_OBJECT

public:
    FileView( QWidget *parent );

protected slots:

    void slotAppendToPlaylist();
    void slotReplacePlaylist();
    void slotEditTracks();
    void slotPrepareMoveTracks();
    void slotPrepareCopyTracks();
    void slotPrepareTranscodeTracks();
    void slotMoveTracks( const Meta::TrackList &tracks );
    void slotCopyTracks( const Meta::TrackList &tracks );
    void slotDelete( Qt::MouseButtons buttons, Qt::KeyboardModifiers modifiers );


protected:
    QList<QAction *> actionsForIndices( const QModelIndexList &indices );
    void addSelectionToPlaylist( bool replace );
    
    virtual void contextMenuEvent( QContextMenuEvent *e );
    virtual void mouseReleaseEvent( QMouseEvent *event );
    virtual void mouseDoubleClickEvent( QMouseEvent *event );
    virtual void startDrag( Qt::DropActions supportedActions );
    KFileItemList selectedItems() const;

private slots:
    void slotEditTriggered();

private:
    Meta::TrackList tracksForEdit() const;

    QAction *m_appendAction;
    QAction *m_loadAction;
    QAction *m_editAction;
    QAction *m_separator1;
    QAction *m_deleteAction;

    PopupDropper *m_pd;
    QMutex m_dragMutex;
    bool m_ongoingDrag;
    bool m_moveActivated;
    bool m_copyActivated;
    CollectionAction *m_moveAction;
    CollectionAction *m_copyAction;
    QPersistentModelIndex m_lastSelectedIndex;
};

#endif // end include guard
