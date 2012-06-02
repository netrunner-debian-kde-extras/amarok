/****************************************************************************************
 * Copyright (c) 2008 Nikolaj Hald Nielsen <nhn@kde.org>                                *
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

#define DEBUG_PREFIX "PlaylistBrowserView"

#include "PlaylistBrowserView.h"

#include "core/support/Debug.h"
#include "playlist/PlaylistModel.h"
#include "playlist/PlaylistController.h"
#include "context/ContextView.h"
#include "context/popupdropper/libpud/PopupDropperItem.h"
#include "context/popupdropper/libpud/PopupDropper.h"
#include "PlaylistBrowserModel.h"
#include "PaletteHandler.h"
#include "PopupDropperFactory.h"
#include "PlaylistTreeItemDelegate.h"
#include "SvgHandler.h"
#include "PlaylistsInFoldersProxy.h"
#include "PlaylistsByProviderProxy.h"

#include <KAction>
#include <KGlobalSettings>
#include <KMenu>

#include <QKeyEvent>
#include <QMouseEvent>
#include <QModelIndex>
#include <QToolTip>

PlaylistBrowserNS::PlaylistBrowserView::PlaylistBrowserView( QAbstractItemModel *model,
                                                             QWidget *parent )
    : Amarok::PrettyTreeView( parent )
    , m_pd( 0 )
    , m_addFolderAction( 0 )
    , m_ongoingDrag( false )
    , m_dragMutex()
    , m_expandToggledWhenPressed( false )
{
    DEBUG_BLOCK
    setModel( model );
    setSelectionMode( QAbstractItemView::ExtendedSelection );
    setSelectionBehavior( QAbstractItemView::SelectItems );
    setDragDropMode( QAbstractItemView::DragDrop );
    setAcceptDrops( true );
    setEditTriggers( QAbstractItemView::EditKeyPressed );
    if( KGlobalSettings::graphicEffectsLevel() != KGlobalSettings::NoEffects )
        setAnimated( true );

    The::paletteHandler()->updateItemView( this );
}

PlaylistBrowserNS::PlaylistBrowserView::~PlaylistBrowserView()
{
}

void
PlaylistBrowserNS::PlaylistBrowserView::setModel( QAbstractItemModel *model )
{
    disconnect( this->model(), 0, this, 0 );
    Amarok::PrettyTreeView::setModel( model );

    connect( this->model(), SIGNAL(renameIndex(QModelIndex)), SLOT(edit(QModelIndex)) );
}

void
PlaylistBrowserNS::PlaylistBrowserView::mousePressEvent( QMouseEvent *event )
{
    const QModelIndex index = indexAt( event->pos() );
    if( !index.isValid() )
    {
        event->accept();
        return;
    }

    // HACK: provider elements hide the root decorations
    // Don't bother checking actions for the others.
    if( !rootIsDecorated() && !index.parent().isValid() )
    {
        const int actionCount =
            index.data( PlaylistBrowserNS::PlaylistBrowserModel::ActionCountRole ).toInt();
        if( actionCount > 0 )
        {
            const QRect rect = PlaylistTreeItemDelegate::actionsRect( index );
            if( rect.contains( event->pos() ) )
                return;
        }
    }

    bool prevExpandState = isExpanded( index );

    // This will toggle the expansion of the current item when clicking
    // on the fold marker but not on the item itself. Required here to
    // enable dragging.
    Amarok::PrettyTreeView::mousePressEvent( event );

    m_expandToggledWhenPressed = ( prevExpandState != isExpanded(index) );
}

QAction *
PlaylistBrowserNS::PlaylistBrowserView::decoratorActionAt( const QModelIndex &idx, const QPoint pos )
{
    const int actionCount =
        idx.data( PlaylistBrowserNS::PlaylistBrowserModel::ActionCountRole ).toInt();
    if( actionCount > 0 )
    {
        const QRect rect = PlaylistTreeItemDelegate::actionsRect( idx );
        if( rect.contains( pos ) )
        {
            QVariantList variantList =
                    idx.data( PlaylistBrowserNS::PlaylistBrowserModel::ActionRole ).toList();
            if( variantList.isEmpty() )
                return 0;

            QActionList actions = variantList.first().value<QActionList>();
            int indexOfAction = ( pos.x() - rect.left() ) /
                                PlaylistTreeItemDelegate::delegateActionIconWidth();
            if( indexOfAction >= actions.count() )
                return 0;
            return actions.value( indexOfAction );
        }
    }
    return 0;
}

void
PlaylistBrowserNS::PlaylistBrowserView::mouseReleaseEvent( QMouseEvent *event )
{
    if( m_pd )
    {
        connect( m_pd, SIGNAL( fadeHideFinished() ), m_pd, SLOT( deleteLater() ) );
        m_pd->hide();
        m_pd = 0;
    }

    QModelIndex index = indexAt( event->pos() );
    if( !index.isValid() )
    {
        event->accept();
        return;
    }

    if( event->button() == Qt::MidButton )
    {
        appendAndPlay( index );
        event->accept();
        return;
    }

    // HACK: provider elements hide the root decorations
    // Don't bother checking actions for the others.
    if( !rootIsDecorated() && !index.parent().isValid() )
    {
        QAction *action = decoratorActionAt( index, event->pos() );
        if( action )
            action->trigger();
    }

    if( !m_expandToggledWhenPressed &&
        event->button() == Qt::LeftButton &&
        event->modifiers() == Qt::NoModifier &&
        KGlobalSettings::singleClick() &&
        model()->hasChildren( index ) )
    {
        m_expandToggledWhenPressed = !m_expandToggledWhenPressed;
        setExpanded( index, !isExpanded( index ) );
        event->accept();
        return;
    }
    Amarok::PrettyTreeView::mouseReleaseEvent( event );
}

void
PlaylistBrowserNS::PlaylistBrowserView::mouseMoveEvent( QMouseEvent *event )
{
    if( event->buttons() || event->modifiers() )
    {
        Amarok::PrettyTreeView::mouseMoveEvent( event );
        return;
    }
    event->accept();
}

void PlaylistBrowserNS::PlaylistBrowserView::startDrag( Qt::DropActions supportedActions )
{
    //Waah? when a parent item is dragged, startDrag is called a bunch of times
    m_dragMutex.lock();
    if( m_ongoingDrag )
    {
        m_dragMutex.unlock();
        return;
    }
    m_ongoingDrag = true;
    m_dragMutex.unlock();

    if( !m_pd )
        m_pd = The::popupDropperFactory()->createPopupDropper( Context::ContextView::self() );

    QList<QAction *> actions;

    if( m_pd && m_pd->isHidden() )
    {
        actions = actionsFor( selectedIndexes() );

        foreach( QAction *action, actions )
            m_pd->addItem( The::popupDropperFactory()->createItem( action ) );

        m_pd->show();
    }

    QTreeView::startDrag( supportedActions );
    debug() << "After the drag!";

    //We keep the items that the actions need to be applied to in the actions private data.
    //Clear the data from all actions now that the PUD has executed.
    foreach( QAction *action, actions )
        action->setData( QVariant() );

    if( m_pd )
    {
        debug() << "clearing PUD";
        connect( m_pd, SIGNAL( fadeHideFinished() ), m_pd, SLOT( clear() ) );
        m_pd->hide();
    }
    m_dragMutex.lock();
    m_ongoingDrag = false;
    m_dragMutex.unlock();
}

void
PlaylistBrowserNS::PlaylistBrowserView::keyPressEvent( QKeyEvent *event )
{
    QModelIndexList indices = selectedIndexes();
    if( indices.isEmpty() )
    {
        Amarok::PrettyTreeView::keyPressEvent( event );
        return;
    }

    switch( event->key() )
    {
        case Qt::Key_Enter:
        case Qt::Key_Return:
            appendAndPlay( indices );
            return;
        case Qt::Key_Delete:
            deletePlaylistsTracks( indices );
            return;
        default:
            break;
     }
    Amarok::PrettyTreeView::keyPressEvent( event );
}

void
PlaylistBrowserNS::PlaylistBrowserView::mouseDoubleClickEvent( QMouseEvent *event )
{
    if( event->button() == Qt::MidButton )
    {
        event->accept();
        return;
    }

    QModelIndex index = indexAt( event->pos() );
    if( !index.isValid() )
    {
        event->accept();
        return;
    }

    bool isExpandable = model()->hasChildren( index );
    bool wouldExpand = isExpandable && !KGlobalSettings::singleClick(); // we're in doubleClick
    if( event->button() == Qt::LeftButton &&
        event->modifiers() == Qt::NoModifier &&
        !wouldExpand )
    {
        appendAndPlay( index );
        event->accept();
        return;
    }
    Amarok::PrettyTreeView::mouseDoubleClickEvent( event );
}

void PlaylistBrowserNS::PlaylistBrowserView::contextMenuEvent( QContextMenuEvent *event )
{
    QModelIndex clickedIdx = indexAt( event->pos() );

    QModelIndexList indices;
    if( selectedIndexes().contains( clickedIdx ) )
        indices << selectedIndexes();
    else
        indices << clickedIdx;

    QActionList actions = actionsFor( indices );

    if( actions.isEmpty() )
        return;

    KMenu menu;
    foreach( QAction *action, actions )
    {
        if( action )
            menu.addAction( action );
    }

    if( indices.count() == 0 )
        menu.addAction( m_addFolderAction );

    menu.exec( mapToGlobal( event->pos() ) );

    //We keep the items that the action need to be applied to in the action's private data.
    //Clear the data from all actions now that the context menu has executed.
    foreach( QAction *action, actions )
        action->setData( QVariant() );
}

bool
PlaylistBrowserNS::PlaylistBrowserView::viewportEvent( QEvent *event )
{
    if( event->type() == QEvent::ToolTip )
    {
        QHelpEvent *helpEvent = static_cast<QHelpEvent *>( event );
        const QModelIndex &index = indexAt( helpEvent->pos() );
        if( !rootIsDecorated() && !index.parent().isValid() )
        {
            QAction *action = decoratorActionAt( index, helpEvent->pos() );
            if( action )
            {
                QToolTip::showText( helpEvent->globalPos(), action->toolTip() );
                return true;
            }
        }
    }

    return Amarok::PrettyTreeView::viewportEvent( event );
}

QList<QAction *>
PlaylistBrowserNS::PlaylistBrowserView::actionsFor( QModelIndexList indexes )
{
    QActionList actions;
    foreach( QModelIndex idx, indexes )
    {
        QActionList idxActions = model()->data( idx,
                PlaylistBrowserNS::PlaylistBrowserModel::ActionRole ).value<QActionList>();
        //only add unique actions model is responsible for making them unique
        foreach( QAction *action, idxActions )
        {
            if( !actions.contains( action ) )
                actions << action;
        }
    }
    return actions;
}

void
PlaylistBrowserNS::PlaylistBrowserView::setNewFolderAction( KAction *action )
{
    m_addFolderAction = action;
}

void
PlaylistBrowserNS::PlaylistBrowserView::currentChanged( const QModelIndex &current,
                                                        const QModelIndex &previous )
{
    Q_UNUSED( previous )
    emit currentItemChanged( current );
    Amarok::PrettyTreeView::currentChanged( current, previous );
}

void
PlaylistBrowserNS::PlaylistBrowserView::appendAndPlay( const QModelIndex &index )
{
    appendAndPlay( QModelIndexList() << index );
}

void
PlaylistBrowserNS::PlaylistBrowserView::appendAndPlay( const QModelIndexList &list )
{
    performActionNamed( "appendAction", list );
}

void
PlaylistBrowserNS::PlaylistBrowserView::deletePlaylistsTracks( const QModelIndexList &list )
{
    performActionNamed( "deleteAction", list );
}

void PlaylistBrowserNS::PlaylistBrowserView::performActionNamed( const QString &name, const QModelIndexList &list )
{
    QActionList actions = actionsFor( list );

    foreach( QAction *action, actions )
    {
        if( action->objectName() == name )
            action->trigger();
        action->setData( QVariant() );  // reset data of all actions
    }
}

#include "PlaylistBrowserView.moc"
