/****************************************************************************************
 * Copyright (c) 2007 Alexandre Pereira de Oliveira <aleprj@gmail.com>                  *
 * Copyright (c) 2007 Ian Monroe <ian@monroe.nu>                                        *
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

#define DEBUG_PREFIX "CollectionTreeView"
#include "core/support/Debug.h"

#include "CollectionTreeView.h"

#include "core/support/Amarok.h"
#include "AmarokMimeData.h"
#include "core/collections/CollectionLocation.h"
#include "core-impl/collections/support/CollectionManager.h"
#include "CollectionSortFilterProxyModel.h"
#include "core-impl/collections/support/TrashCollectionLocation.h"
#include "core-impl/collections/support/TextualQueryFilter.h"
#include "browsers/CollectionTreeItem.h"
#include "browsers/CollectionTreeItemModel.h"
#include "context/ContextView.h"
#include "GlobalCollectionActions.h"
#include "core/meta/Meta.h"
#include "core/collections/MetaQueryMaker.h"
#include "core/capabilities/BookmarkThisCapability.h"
#include "core/capabilities/ActionsCapability.h"
#include "playlist/PlaylistModelStack.h"
#include "PopupDropperFactory.h"
#include "context/popupdropper/libpud/PopupDropper.h"
#include "context/popupdropper/libpud/PopupDropperItem.h"
#include "core/collections/QueryMaker.h"
#include "SvgHandler.h"
#include "TagDialog.h"
#include "transcoding/TranscodingAssistantDialog.h"

#include <KAction>
#include <KGlobalSettings>
#include <KIcon>
#include <KComboBox>
#include <KMenu>
#include <KMessageBox> // NOTE: for delete dialog, will move to CollectionCapability later

#include <QContextMenuEvent>
#include <QHash>
#include <QMouseEvent>
#include <QSortFilterProxyModel>

CollectionTreeView::CollectionTreeView( QWidget *parent)
    : Amarok::PrettyTreeView( parent )
    , m_filterModel( 0 )
    , m_treeModel( 0 )
    , m_pd( 0 )
    , m_appendAction( 0 )
    , m_loadAction( 0 )
    , m_editAction( 0 )
    , m_organizeAction( 0 )
    , m_dragMutex()
    , m_ongoingDrag( false )
    , m_expandToggledWhenPressed( false )
{
    setSortingEnabled( true );
    setFocusPolicy( Qt::StrongFocus );
    sortByColumn( 0, Qt::AscendingOrder );
    setSelectionMode( QAbstractItemView::ExtendedSelection );
    setSelectionBehavior( QAbstractItemView::SelectRows );
    setEditTriggers( /* SelectedClicked | */ EditKeyPressed );
#ifdef Q_WS_MAC
    setVerticalScrollMode( QAbstractItemView::ScrollPerItem ); // for some bizarre reason w/ some styles on mac
    setHorizontalScrollMode( QAbstractItemView::ScrollPerItem ); // per-pixel scrolling is slower than per-item
#else
    setVerticalScrollMode( QAbstractItemView::ScrollPerPixel ); // Scrolling per item is really not smooth and looks terrible
    setHorizontalScrollMode( QAbstractItemView::ScrollPerPixel ); // Scrolling per item is really not smooth and looks terrible
#endif

    setDragDropMode( QAbstractItemView::DragDrop ); // implement drop when time allows

    if( KGlobalSettings::graphicEffectsLevel() != KGlobalSettings::NoEffects )
        setAnimated( true );

    connect( this, SIGNAL( collapsed( const QModelIndex & ) ), SLOT( slotCollapsed( const QModelIndex & ) ) );
    connect( this, SIGNAL( expanded( const QModelIndex & ) ), SLOT( slotExpanded( const QModelIndex & ) ) );
}

void CollectionTreeView::setModel( QAbstractItemModel * model )
{
    m_treeModel = qobject_cast<CollectionTreeItemModelBase*>( model );
    if( !m_treeModel )
        return;

    m_filterTimer.setSingleShot( true );
    connect( &m_filterTimer, SIGNAL( timeout() ), m_treeModel, SLOT( slotFilter() ) );
    connect( m_treeModel, SIGNAL( allQueriesFinished() ), SLOT( slotCheckAutoExpand() ));
    connect( m_treeModel, SIGNAL(expandIndex(QModelIndex)), SLOT(slotExpandIndex(QModelIndex)) );

    m_filterModel = new CollectionSortFilterProxyModel( this );
    m_filterModel->setSortRole( CustomRoles::SortRole );
    m_filterModel->setFilterRole( CustomRoles::FilterRole );
    m_filterModel->setSortCaseSensitivity( Qt::CaseInsensitive );
    m_filterModel->setFilterCaseSensitivity( Qt::CaseInsensitive );
    m_filterModel->setSourceModel( model );
    m_filterModel->setDynamicSortFilter( true );

    QTreeView::setModel( m_filterModel );

    QTimer::singleShot( 0, this, SLOT( slotCheckAutoExpand() ) );
}

CollectionTreeView::~CollectionTreeView()
{
    DEBUG_BLOCK

    delete m_treeModel;
    delete m_filterModel;
}

void
CollectionTreeView::setLevels( const QList<int> &levels )
{
    if( m_treeModel )
        m_treeModel->setLevels( levels );
}

QList< int > CollectionTreeView::levels() const
{
    if( m_treeModel )
        return m_treeModel->levels();
    return QList<int>();
}

void
CollectionTreeView::setLevel( int level, int type )
{
    if( !m_treeModel )
        return;
    QList<int> levels = m_treeModel->levels();
    if ( type == CategoryId::None )
    {
        while( levels.count() >= level )
            levels.removeLast();
    }
    else
    {
        levels.removeAll( type );
        levels[level] = type;
    }
    setLevels( levels );
}

QSortFilterProxyModel*
CollectionTreeView::filterModel() const
{
    return m_filterModel;
}

void
CollectionTreeView::contextMenuEvent( QContextMenuEvent* event )
{
    if( !m_treeModel )
        return;

    QModelIndex index = indexAt( event->pos() );
    if( !index.isValid() )
    {
        Amarok::PrettyTreeView::contextMenuEvent( event );
        return;
    }

    QModelIndexList indices = selectedIndexes();

    // if previously selected indices do not contain the index of the item
    // currently under the mouse when context menu is invoked.
    if( !indices.contains( index ) )
    {
        indices.clear();
        indices << index;
        setCurrentIndex( index );
    }

    if( m_filterModel )
    {
        QModelIndexList tmp;
        foreach( const QModelIndex &idx, indices )
        {
            tmp.append( m_filterModel->mapToSource( idx ) );
        }
        indices = tmp;
    }

    // Abort if nothing is selected
    if( indices.isEmpty() )
        return;

    m_currentItems.clear();
    foreach( const QModelIndex &index, indices )
    {
        if( index.isValid() && index.internalPointer() )
            m_currentItems.insert( static_cast<CollectionTreeItem*>( index.internalPointer() ) );
    }

    KMenu menu( this );

    // Destroy the menu when the model is reset (collection update), so that we don't operate on invalid data.
    // see BUG 190056
    connect( m_treeModel, SIGNAL( modelReset() ), &menu, SLOT( deleteLater() ) );

    // create basic actions
    QActionList actions = createBasicActions( indices );
    foreach( QAction *action, actions ) {
        menu.addAction( action );
    }
    menu.addSeparator();
    actions.clear();

    QActionList customActions = createCustomActions( indices );
    KMenu menuCustom( i18n( "Album" )  );
    foreach( QAction *action, customActions )
    {
        if( !action->parent() )
            action->setParent( &menuCustom );
    }

    if( customActions.count() > 1 )
    {
        menuCustom.addActions( customActions );
        menuCustom.setIcon( KIcon( "filename-album-amarok" ) );
        menu.addMenu( &menuCustom );
        menu.addSeparator();
    }
    else if( customActions.count() == 1 )
    {
        menu.addActions( customActions );
    }

    QActionList collectionActions = createCollectionActions( indices );
    KMenu menuCollection( i18n( "Collection" ) );
    foreach( QAction *action, collectionActions )
    {
        if( !action->parent() )
            action->setParent( &menuCollection );
    }

    if( collectionActions.count() > 1 )
    {
        menuCollection.setIcon( KIcon( "collection-amarok" ) );
        menuCollection.addActions( collectionActions );
        menu.addMenu( &menuCollection );
        menu.addSeparator();
    }
    else if( collectionActions.count() == 1 )
    {
        menu.addActions( collectionActions );
    }

    m_currentCopyDestination =   getCopyActions(   indices );
    m_currentMoveDestination =   getMoveActions(   indices );
    m_currentRemoveDestination = getRemoveActions( indices );

    KMenu copyMenu( i18n( "Copy to Collection" ) );
    if( !m_currentCopyDestination.empty() )
    {
        copyMenu.setIcon( KIcon( "edit-copy" ) );
        copyMenu.addActions( m_currentCopyDestination.keys() );
        menu.addMenu( &copyMenu );
    }

    KMenu moveMenu( i18n( "Move to Collection" ) );
    if( !m_currentMoveDestination.empty() )
    {
        // moveMenu.setIcon(); // TODO: no move icon
        moveMenu.addActions( m_currentMoveDestination.keys() );
        menu.addMenu( &moveMenu );
    }

    if( !m_currentRemoveDestination.empty() )
    {
        menu.addActions( m_currentRemoveDestination.keys() );
    }

    // add extended actions
    menu.addSeparator();
    actions += createExtendedActions( indices );
    foreach( QAction *action, actions ) {
        menu.addAction( action );
    }

    menu.exec( event->globalPos() );
}

void CollectionTreeView::mouseDoubleClickEvent( QMouseEvent *event )
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

    if( event->button() == Qt::LeftButton &&
        event->modifiers() == Qt::NoModifier &&
        (KGlobalSettings::singleClick() || !model()->hasChildren( index )) )
    {
        CollectionTreeItem *item = getItemFromIndex( index );
        playChildTracks( item, Playlist::AppendAndPlay );
        event->accept();
        return;
    }
    Amarok::PrettyTreeView::mouseDoubleClickEvent( event );
}

void CollectionTreeView::mousePressEvent( QMouseEvent *event )
{
    const QModelIndex index = indexAt( event->pos() );
    if( !index.isValid() )
    {
        event->accept();
        return;
    }

    bool prevExpandState = isExpanded( index );

    // This will toggle the expansion of the current item when clicking
    // on the fold marker but not on the item itself. Required here to
    // enable dragging.
    Amarok::PrettyTreeView::mousePressEvent( event );

    m_expandToggledWhenPressed = ( prevExpandState != isExpanded(index) );
}

void CollectionTreeView::mouseReleaseEvent( QMouseEvent *event )
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
        CollectionTreeItem *item = getItemFromIndex( index );
        playChildTracks( item, Playlist::AppendAndPlay );
        event->accept();
        return;
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

void CollectionTreeView::mouseMoveEvent( QMouseEvent *event )
{
    const QModelIndex index = indexAt( event->pos() );
    if( !index.isValid() )
    {
        event->accept();
        return;
    }

    // pass event to parent widget
    if( event->buttons() || event->modifiers() )
    {
        Amarok::PrettyTreeView::mouseMoveEvent( event );
        return;
    }
    event->accept();
}

CollectionTreeItem* CollectionTreeView::getItemFromIndex( QModelIndex &index )
{
    QModelIndex filteredIndex;
    if( m_filterModel )
        filteredIndex = m_filterModel->mapToSource( index );
    else
        filteredIndex = index;

    if( !filteredIndex.isValid() )
    {
        return 0;
    }

    return static_cast<CollectionTreeItem*>( filteredIndex.internalPointer() );
}

void CollectionTreeView::keyPressEvent( QKeyEvent *event )
{
    QModelIndexList indices = selectedIndexes();
    if( indices.isEmpty() )
    {
        QTreeView::keyPressEvent( event );
        return;
    }

    if( m_filterModel )
    {
        QModelIndexList tmp;
        foreach( const QModelIndex &idx, indices )
            tmp.append( m_filterModel->mapToSource( idx ) );
        indices = tmp;
    }

    m_currentItems.clear();
    foreach( const QModelIndex &index, indices )
    {
        if( index.isValid() && index.internalPointer() )
            m_currentItems.insert( static_cast<CollectionTreeItem*>( index.internalPointer() ) );
    }

    QModelIndex current = currentIndex();
    switch( event->key() )
    {
        case Qt::Key_Enter:
        case Qt::Key_Return:
            slotAppendChildTracks();
            return;
        case Qt::Key_Up:
            if( current.parent() == QModelIndex() && current.row() == 0 )
            {
                emit leavingTree();
                return;
            }
            break;
        case Qt::Key_Down:
            break;
        // L and R should magically work when we get a patched version of qt
        case Qt::Key_Right:
        case Qt::Key_Direction_R:
            expand( current );
            return;
        case Qt::Key_Left:
        case Qt::Key_Direction_L:
            collapse( current );
            return;
        default:
            break;
    }
    QTreeView::keyPressEvent( event );
}

void
CollectionTreeView::startDrag(Qt::DropActions supportedActions)
{
    DEBUG_BLOCK
    QModelIndexList indices = selectedIndexes();
    if( indices.isEmpty() )
        return;

    //setSelectionMode( QAbstractItemView::NoSelection );

    // When a parent item is dragged, startDrag() is called a bunch of times. Here we prevent that:
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

    if( m_pd && m_pd->isHidden() )
    {
        if( m_filterModel )
        {
            QModelIndexList tmp;
            foreach( const QModelIndex &idx, indices )
            {
                tmp.append( m_filterModel->mapToSource( idx ) );
            }
            indices = tmp;
        }

        QActionList actions = createBasicActions( indices );

        QFont font;
        font.setPointSize( 16 );
        font.setBold( true );

        foreach( QAction * action, actions )
            m_pd->addItem( The::popupDropperFactory()->createItem( action ) );

        m_currentCopyDestination = getCopyActions( indices );
        m_currentMoveDestination = getMoveActions( indices );

        m_currentItems.clear();
        foreach( const QModelIndex &index, indices )
        {
            if( index.isValid() && index.internalPointer() )
                m_currentItems.insert( static_cast<CollectionTreeItem*>( index.internalPointer() ) );
        }

        PopupDropperItem* subItem;

        actions = createExtendedActions( indices );

        PopupDropper * morePud = 0;
        if( actions.count() > 1 )
        {
            morePud = The::popupDropperFactory()->createPopupDropper( 0, true );

            foreach( QAction * action, actions )
                morePud->addItem( The::popupDropperFactory()->createItem( action ) );
        }
        else
            m_pd->addItem( The::popupDropperFactory()->createItem( actions[0] ) );

        //TODO: Keep bugging i18n team about problems with 3 dots
        if ( actions.count() > 1 )
        {
            subItem = m_pd->addSubmenu( &morePud, i18n( "More..." )  );
            The::popupDropperFactory()->adjustItem( subItem );
        }

        m_pd->show();
    }

    QTreeView::startDrag( supportedActions );
    debug() << "After the drag!";

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

void CollectionTreeView::selectionChanged(const QItemSelection & selected, const QItemSelection & deselected)
{
    QModelIndexList indexes = selected.indexes();

    QModelIndexList changedIndexes = indexes;
    changedIndexes << deselected.indexes();
    foreach( const QModelIndex &index, changedIndexes )
        update( index );

    if ( indexes.count() < 1 )
        return;

    QModelIndex index;
    if ( m_filterModel )
        index = m_filterModel->mapToSource( indexes[0] );
    else
        index = indexes[0];

    CollectionTreeItem * item = static_cast<CollectionTreeItem *>( index.internalPointer() );
    emit( itemSelected ( item ) );
}

void
CollectionTreeView::slotSetFilterTimeout()
{
    KComboBox *comboBox = qobject_cast<KComboBox*>( sender() );
    if( comboBox )
    {
        if( m_treeModel )
            m_treeModel->setCurrentFilter( comboBox->currentText() );
        m_filterTimer.stop();
        m_filterTimer.start( 500 );
    }
}

void
CollectionTreeView::slotCollapsed( const QModelIndex &index )
{
    if( !m_treeModel )
        return;
    if( m_filterModel )
        m_treeModel->slotCollapsed( m_filterModel->mapToSource( index ) );
    else
        m_treeModel->slotCollapsed( index );
}

void
CollectionTreeView::slotExpanded( const QModelIndex &index )
{
    if( !m_treeModel )
        return;
    if( m_filterModel )
        m_treeModel->slotExpanded( m_filterModel->mapToSource( index ));
    else
        m_treeModel->slotExpanded( index );
}

void
CollectionTreeView::slotExpandIndex( const QModelIndex &index )
{
    if( !m_treeModel )
        return;
    if( m_filterModel )
        expand( m_filterModel->mapFromSource( index ) );
}

void
CollectionTreeView::slotCheckAutoExpand()
{
    if( m_filterModel )
    {
        // Cases where root is not collections but
        // for example magnatunes's plethora of genres, don't expand by default
        if( m_filterModel->rowCount() > 6 )
            return;

        QModelIndexList indicesToCheck;
        for( int i = 0; i < m_filterModel->rowCount(); i++ ) //need something to start for'ing with
            indicesToCheck += m_filterModel->index( i, 0 ); //lowest level is fine for that

        QModelIndex current;
        for( int j = 0; j < indicesToCheck.size(); j++)
        {
            current = indicesToCheck.at( j );
            if( m_filterModel->rowCount( current ) < 4 )
            { //don't expand if many results
                expand( current );
                for( int i = 0; i < m_filterModel->rowCount( current ); i++ )
                    indicesToCheck += m_filterModel->index( i, 0, current );
            }
        }
    }
}

void
CollectionTreeView::playChildTracks( CollectionTreeItem *item, Playlist::AddOptions insertMode)
{
    QSet<CollectionTreeItem*> items;
    items.insert( item );

    playChildTracks( items, insertMode );
}

void
CollectionTreeView::playChildTracks( const QSet<CollectionTreeItem*> &items, Playlist::AddOptions insertMode )
{
    if( !m_treeModel )
        return;
    //Ensure that if a parent and child are both selected we ignore the child
    QSet<CollectionTreeItem*> parents( cleanItemSet( items ) );

    //Store the type of playlist insert to be done and cause a slot to be invoked when the tracklist has been generated.
    AmarokMimeData *mime = dynamic_cast<AmarokMimeData*>( m_treeModel->mimeData( QList<CollectionTreeItem*>::fromSet( parents ) ) );
    m_playChildTracksMode.insert( mime, insertMode );
    connect( mime, SIGNAL( trackListSignal( Meta::TrackList ) ), this, SLOT( playChildTracksSlot( Meta::TrackList) ) );
    mime->getTrackListSignal();
}

void
CollectionTreeView::playChildTracksSlot( Meta::TrackList list ) //slot
{
    AmarokMimeData *mime = dynamic_cast<AmarokMimeData*>( sender() );

    Playlist::AddOptions insertMode = m_playChildTracksMode.take( mime );

    qStableSort( list.begin(), list.end(), Meta::Track::lessThan );
    The::playlistController()->insertOptioned( list, insertMode );

    mime->deleteLater();
}

void
CollectionTreeView::organizeTracks( const QSet<CollectionTreeItem*> &items ) const
{
    DEBUG_BLOCK
    if( !items.count() )
        return;

    //Create query based upon items, ensuring that if a parent and child are both selected we ignore the child
    Collections::QueryMaker *qm = createMetaQueryFromItems( items, true );
    if( !qm )
        return;

    CollectionTreeItem *item = items.toList().first();
    while( item->isDataItem() )
        item = item->parent();

    Collections::Collection *coll = item->parentCollection();
    Collections::CollectionLocation *location = coll->location();
    if( !location->isOrganizable() )
    {
        debug() << "Collection not organizable";
        //how did we get here??
        delete location;
        delete qm;
        return;
    }
    location->prepareMove( qm, coll->location() );
}

void
CollectionTreeView::copyTracks( const QSet<CollectionTreeItem*> &items, Collections::Collection *destination,
                                bool removeSources, Transcoding::Configuration configuration ) const
{
    DEBUG_BLOCK
    if( !destination || !destination->isWritable() )
    {
        warning() << "collection " << destination->prettyName() << " is not writable! Aborting";
        return;
    }
    //copied from organizeTracks. create a method for this somewhere
    if( !items.count() )
    {
        warning() << "No items to copy! Aborting";
        return;
    }

    //Create query based upon items, ensuring that if a parent and child are both selected we ignore the child
    Collections::QueryMaker *qm = createMetaQueryFromItems( items, true );
    if( !qm )
    {
        warning() << "could not get qm!";
        return;
    }

    CollectionTreeItem *item = items.toList().first();
    while( item->isDataItem() )
    {
        item = item->parent();
    }
    Collections::Collection *coll = item->parentCollection();
    Collections::CollectionLocation *source = coll->location();
    Collections::CollectionLocation *dest = destination->location();
    if( removeSources )
    {
        if( !source->isWritable() ) //error
        {
            warning() << "We can not write to ze source!!! OMGooses!";
            delete dest;
            delete source;
            delete qm;
            return;
        }

        debug() << "starting source->prepareMove";
        source->prepareMove( qm, dest );
    }
    else
    {
        debug() << "starting source->prepareCopy";
        source->prepareCopy( qm, dest, configuration );
    }
}

void
CollectionTreeView::removeTracks( const QSet<CollectionTreeItem*> &items, bool useTrash ) const
{
    DEBUG_BLOCK

    //copied from organizeTracks. create a method for this somewhere
    if( !items.count() )
        return;

    //Create query based upon items, ensuring that if a parent and child are both selected we ignore the child
    Collections::QueryMaker *qm = createMetaQueryFromItems( items, true );
    if( !qm )
        return;

    CollectionTreeItem *item = items.toList().first();
    while( item->isDataItem() )
        item = item->parent();

    Collections::Collection *coll = item->parentCollection();

    if( !coll->isWritable() )
        return;

    Collections::CollectionLocation *source = coll->location();

    if( !source->isWritable() ) //error
    {
        warning() << "We can not write to ze source!!! OMGooses!";
        delete source;
        delete qm;
        return;
    }

    if( useTrash )
    {
        Collections::TrashCollectionLocation *trash = new Collections::TrashCollectionLocation();
        source->prepareMove( qm, trash );
    }
    else
        source->prepareRemove( qm );
}

void
CollectionTreeView::editTracks( const QSet<CollectionTreeItem*> &items ) const
{
    //Create query based upon items, ensuring that if a parent and child are both selected we ignore the child
    Collections::QueryMaker *qm = createMetaQueryFromItems( items, true );
    if( !qm )
        return;

    (void)new TagDialog( qm ); //the dialog will show itself automatically as soon as it is ready
}

void CollectionTreeView::slotFilterNow()
{
    if( m_treeModel )
        m_treeModel->slotFilter();
    setFocus( Qt::OtherFocusReason );
}

QActionList CollectionTreeView::createBasicActions( const QModelIndexList & indices )
{
    QActionList actions;

    if( !indices.isEmpty() )
    {
        if( m_appendAction == 0 )
        {
            m_appendAction = new QAction( KIcon( "media-track-add-amarok" ), i18n( "&Add to Playlist" ), this );
            m_appendAction->setProperty( "popupdropper_svg_id", "append" );
            connect( m_appendAction, SIGNAL( triggered() ), this, SLOT( slotAppendChildTracks() ) );
        }

        actions.append( m_appendAction );

        if( m_loadAction == 0 )
        {
            m_loadAction = new QAction( i18nc( "Replace the currently loaded tracks with these", "&Replace Playlist" ), this );
            m_loadAction->setProperty( "popupdropper_svg_id", "load" );
            connect( m_loadAction, SIGNAL( triggered() ), this, SLOT( slotPlayChildTracks() ) );
        }

        actions.append( m_loadAction );
    }

    return actions;
}

QActionList CollectionTreeView::createExtendedActions( const QModelIndexList & indices )
{
    QActionList actions;

    if( !indices.isEmpty() )
    {

        {   //keep the scope of item minimal
            CollectionTreeItem *item = static_cast<CollectionTreeItem*>( indices.first().internalPointer() );
            while( item->isDataItem() )
            {
                item = item->parent();
            }

            Collections::Collection *collection = item->parentCollection();
            const Collections::CollectionLocation* location = collection->location();

            if( location->isOrganizable() )
            {
                bool onlyOneCollection = true;
                foreach( const QModelIndex &index, indices )
                {
                    Q_UNUSED( index )
                    CollectionTreeItem *item = static_cast<CollectionTreeItem*>( indices.first().internalPointer() );
                    while( item->isDataItem() )
                        item = item->parent();

                    onlyOneCollection = item->parentCollection() == collection;
                    if( !onlyOneCollection )
                        break;
                }

                if( onlyOneCollection )
                {
                    if ( m_organizeAction == 0 )
                    {
                        m_organizeAction = new QAction( KIcon("folder-open" ), i18nc( "Organize Files", "Organize Files" ), this );
                        m_organizeAction->setProperty( "popupdropper_svg_id", "organize" );
                        connect( m_organizeAction, SIGNAL( triggered() ), this, SLOT( slotOrganize() ) );
                    }
                    actions.append( m_organizeAction );
                }
            }
            delete location;
        }


        //hmmm... figure out what kind of item we are dealing with....

        if ( indices.size() == 1 )
        {
            debug() << "checking for global actions";
            CollectionTreeItem *item = static_cast<CollectionTreeItem*>( indices.first().internalPointer() );

            QActionList gActions = The::globalCollectionActions()->actionsFor( item->data() );
            foreach( QAction *action, gActions )
            {
                if( action ) // Can become 0-pointer, see http://bugs.kde.org/show_bug.cgi?id=183250
                {
                    actions.append( action );
                    debug() << "Got global action: " << action->text();
                }
            }
        }

        if ( m_editAction == 0 )
        {
            m_editAction = new QAction( KIcon( "media-track-edit-amarok" ), i18n( "&Edit Track Details" ), this );
            setProperty( "popupdropper_svg_id", "edit" );
            connect( m_editAction, SIGNAL( triggered() ), this, SLOT( slotEditTracks() ) );
        }
        actions.append( m_editAction );
    }
    else
        debug() << "invalid index or null internalPointer";

    return actions;
}

QActionList
CollectionTreeView::createCustomActions( const QModelIndexList &indices )
{
    QActionList actions;
    if( indices.count() == 1 )
    {
        if( indices.first().isValid() && indices.first().internalPointer() )
        {
            Meta::DataPtr data = static_cast<CollectionTreeItem*>( indices.first().internalPointer() )->data();
            if( data )
            {
                QScopedPointer<Capabilities::ActionsCapability> ac( data->create<Capabilities::ActionsCapability>() );
                if( ac )
                {
                    QActionList cActions = ac->actions();

                    foreach( QAction *action, cActions )
                    {
                        Q_ASSERT( action );
                        actions.append( action );
                        debug() << "Got custom action: " << action->text();
                    }
                }

                //check if this item can be bookmarked...
                QScopedPointer<Capabilities::BookmarkThisCapability> btc( data->create<Capabilities::BookmarkThisCapability>() );
                if( btc && btc->isBookmarkable() && btc->bookmarkAction() )
                    actions.append( btc->bookmarkAction() );
            }
        }
    }
    return actions;
}

QActionList
CollectionTreeView::createCollectionActions( const QModelIndexList & indices )
{
    QActionList actions;
    // Extract collection whose constituent was selected

    CollectionTreeItem *item = static_cast<CollectionTreeItem*>( indices.first().internalPointer() );

    // Don't return any collection actions for non collection items
    if( item->isDataItem() )
        return actions;

    Collections::Collection *collection = item->parentCollection();

    // Generate CollectionCapability, test for existence

    QScopedPointer<Capabilities::ActionsCapability> cc( collection->create<Capabilities::ActionsCapability>() );

    if( cc )
        actions = cc->actions();

    return actions;
}


QHash<QAction*, Collections::Collection*> CollectionTreeView::getCopyActions(const QModelIndexList & indices )
{
    QHash<QAction*, Collections::Collection*> currentCopyDestination;

    if( onlyOneCollection( indices ) )
    {
        Collections::Collection *collection = getCollection( indices.first() );
        QList<Collections::Collection*> writableCollections;
        QHash<Collections::Collection*, CollectionManager::CollectionStatus> hash = CollectionManager::instance()->collections();
        QHash<Collections::Collection*, CollectionManager::CollectionStatus>::const_iterator it = hash.constBegin();
        while( it != hash.constEnd() )
        {
            Collections::Collection *coll = it.key();
            if( coll && coll->isWritable() && coll != collection )
                writableCollections.append( coll );
            ++it;
        }
        if( !writableCollections.isEmpty() )
        {
            foreach( Collections::Collection *coll, writableCollections )
            {
                QAction *action = new QAction( coll->icon(), coll->prettyName(), 0 );
                action->setProperty( "popupdropper_svg_id", "collection" );
                connect( action, SIGNAL( triggered() ), this, SLOT( slotCopyTracks() ) );

                currentCopyDestination.insert( action, coll );
            }
        }
    }
    return currentCopyDestination;
}

QHash<QAction*, Collections::Collection*> CollectionTreeView::getMoveActions( const QModelIndexList & indices )
{
    QHash<QAction*, Collections::Collection*> currentMoveDestination;

    if( onlyOneCollection( indices) )
    {
        Collections::Collection *collection = getCollection( indices.first() );
        QList<Collections::Collection*> writableCollections;
        QHash<Collections::Collection*, CollectionManager::CollectionStatus> hash = CollectionManager::instance()->collections();
        QHash<Collections::Collection*, CollectionManager::CollectionStatus>::const_iterator it = hash.constBegin();
        while( it != hash.constEnd() )
        {
            Collections::Collection *coll = it.key();
            if( coll && coll->isWritable() && coll != collection )
                writableCollections.append( coll );
            ++it;
        }
        if( !writableCollections.isEmpty() )
        {
            if( collection->isWritable() )
            {
                foreach( Collections::Collection *coll, writableCollections )
                {
                    QAction *action = new QAction( coll->icon(), coll->prettyName(), 0 );
                    action->setProperty( "popupdropper_svg_id", "collection" );
                    connect( action, SIGNAL( triggered() ), this, SLOT( slotMoveTracks() ) );
                    currentMoveDestination.insert( action, coll );
                }
            }
        }
    }
    return currentMoveDestination;
}

QHash<QAction*, Collections::Collection*> CollectionTreeView::getRemoveActions( const QModelIndexList & indices )
{
    QHash<QAction*, Collections::Collection*> currentRemoveDestination;

    if( onlyOneCollection( indices) )
    {
        Collections::Collection *collection = getCollection( indices.first() );
        if( collection && collection->isWritable() )
        {
            //writableCollections.append( collection );
            KAction *action = new KAction( KIcon( "remove-amarok" ), i18n( "Delete Tracks" ), 0 );
            action->setProperty( "popupdropper_svg_id", "delete" );

            connect( action, SIGNAL(triggered(Qt::MouseButtons,Qt::KeyboardModifiers)),
                     this, SLOT(slotRemoveTracks(Qt::MouseButtons,Qt::KeyboardModifiers)) );

            currentRemoveDestination.insert( action, collection );
        }


    }
    return currentRemoveDestination;
}

bool CollectionTreeView::onlyOneCollection( const QModelIndexList & indices )
{
    if( !indices.isEmpty() )
    {
        Collections::Collection *collection = getCollection( indices.first() );
        foreach( const QModelIndex &index, indices )
        {
            Collections::Collection *currentCollection = getCollection( index );
            if( collection != currentCollection )
                return false;
        }
    }

    return true;
}

Collections::Collection * CollectionTreeView::getCollection( const QModelIndex & index )
{
    Collections::Collection *collection = 0;
    if( index.isValid() )
    {
        CollectionTreeItem *item = static_cast<CollectionTreeItem*>( index.internalPointer() );
        while( item->isDataItem() )
        {
            item = item->parent();
        }
        collection = item->parentCollection();
    }

    return collection;
}

void CollectionTreeView::slotPlayChildTracks()
{
    playChildTracks( m_currentItems, Playlist::LoadAndPlay );
}

void CollectionTreeView::slotAppendChildTracks()
{
    playChildTracks( m_currentItems, Playlist::AppendAndPlay );
}

void CollectionTreeView::slotQueueChildTracks()
{
    playChildTracks( m_currentItems, Playlist::Queue );
}

void CollectionTreeView::slotEditTracks()
{
    editTracks( m_currentItems );
}

void CollectionTreeView::slotCopyTracks()
{
    if( sender() )
    {
        if( QAction * action = dynamic_cast<QAction *>( sender() ) )
        {
            Transcoding::Configuration configuration = Transcoding::Configuration();

            Collections::Collection *destCollection = m_currentCopyDestination[ action ];

            if( destCollection->uidUrlProtocol() == "amarok-sqltrackuid" ) //we can only transcode to a SqlCollection for now
            {
                if( !Amarok::Components::transcodingController()->availableFormats().isEmpty() )
                {
                    Transcoding::AssistantDialog dialog( this );
                    if( dialog.exec() )
                        configuration = dialog.configuration();
                }
                else
                    debug() << "FFmpeg is not installed or does not support any of the required formats.";
            }
            else
                debug() << "The destination collection does not support transcoding.";

            copyTracks( m_currentItems, destCollection, false, configuration );
        }
    }
}

void CollectionTreeView::slotMoveTracks()
{
    if( sender() )
    {
        if ( QAction * action = dynamic_cast<QAction *>( sender() ) )
            copyTracks( m_currentItems, m_currentMoveDestination[ action ], true );
    }
}

void CollectionTreeView::slotRemoveTracks( Qt::MouseButtons buttons, Qt::KeyboardModifiers modifiers )
{
    Q_UNUSED( buttons )
    KAction *action = qobject_cast<KAction*>( sender() );
    if( action )
    {
        bool skipTrash = modifiers.testFlag( Qt::ShiftModifier );
        removeTracks( m_currentItems, !skipTrash );
    }
}

void CollectionTreeView::slotOrganize()
{
    if( sender() )
    {
        if( QAction * action = dynamic_cast<QAction *>( sender() ) )
        {
            Q_UNUSED( action )
            organizeTracks( m_currentItems );
        }
    }
}

QSet<CollectionTreeItem*>
CollectionTreeView::cleanItemSet( const QSet<CollectionTreeItem*> &items )
{
    QSet<CollectionTreeItem*> parents;
    foreach( CollectionTreeItem *item, items )
    {
        CollectionTreeItem *tmpItem = item;
        while( tmpItem )
        {
            if( items.contains( tmpItem->parent() ) )
                tmpItem = tmpItem->parent();
            else
            {
                parents.insert( tmpItem );
                break;
            }
        }
    }
    return parents;
}

Collections::QueryMaker*
CollectionTreeView::createMetaQueryFromItems( const QSet<CollectionTreeItem*> &items, bool cleanItems ) const
{
    if( !m_treeModel )
        return 0;

    QSet<CollectionTreeItem*> parents = cleanItems ? cleanItemSet( items ) : items;

    QList<Collections::QueryMaker*> queryMakers;
    foreach( CollectionTreeItem *item, parents )
    {
        Collections::QueryMaker *qm = item->queryMaker();
        CollectionTreeItem *tmp = item;
        while( tmp->isDataItem() || tmp->isVariousArtistItem() )
        {
            if( tmp->isVariousArtistItem() )
                qm->setAlbumQueryMode( Collections::QueryMaker::OnlyCompilations );
            else if( tmp->data() )
            {
                if( m_treeModel->levelCategory( tmp->data() - 1 ) == CategoryId::AlbumArtist )
                    qm->setArtistQueryMode( Collections::QueryMaker::AlbumArtists );
                tmp->addMatch( qm );
            }
            tmp = tmp->parent();
        }
        Collections::addTextualFilter( qm, m_treeModel->currentFilter() );
        queryMakers.append( qm );
    }
    return new Collections::MetaQueryMaker( queryMakers );
}



#include "CollectionTreeView.moc"

