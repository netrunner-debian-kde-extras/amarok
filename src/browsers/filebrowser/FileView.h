/****************************************************************************************
 * Copyright (c) 2010 Nikolaj Hald Nielsen <nhn@kde.org>                                *
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

#ifndef FILETREEVIEW_H
#define FILETREEVIEW_H

#include "collection/Collection.h"
#include "widgets/PrettyTreeView.h"

#include <KFileItem>

#include <QAction>
#include <QList>
#include <QTreeView>
#include <QMutex>

class PopupDropper;

class FileView : public Amarok::PrettyTreeView
{
    Q_OBJECT
public:
    FileView( QWidget * parent );


protected slots:

    void slotAppendToPlaylist();
    void slotReplacePlaylist();
    void slotEditTracks();
    
protected:
            
    QList<QAction *> actionsForIndices( const QModelIndexList &indices );
    void addSelectionToPlaylist( bool replace );
    
    virtual void contextMenuEvent ( QContextMenuEvent * e );
    void startDrag( Qt::DropActions supportedActions );

private:
    Meta::TrackList tracksForEdit() const;
    
    QAction * m_appendAction;
    QAction * m_loadAction;
    QAction * m_editAction;

    PopupDropper* m_pd;
    QMutex m_dragMutex;
    bool m_ongoingDrag;
};

#endif // FILETREEVIEW_H
