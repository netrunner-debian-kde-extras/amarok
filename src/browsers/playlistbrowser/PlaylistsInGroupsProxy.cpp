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
 * PARTICULAR PURPOSE. See the GNU General Public License for more details.              *
 *                                                                                      *
 * You should have received a copy of the GNU General Public License along with         *
 * this program.  If not, see <http://www.gnu.org/licenses/>.                           *
 ****************************************************************************************/

#include "PlaylistsInGroupsProxy.h"

#include "AmarokMimeData.h"
#include "Debug.h"
#include "meta/Playlist.h"
#include "SvgHandler.h"
#include "UserPlaylistModel.h"
#include "playlist/PlaylistModelStack.h"

#include <KIcon>
#include <KInputDialog>

PlaylistsInGroupsProxy::PlaylistsInGroupsProxy( QAbstractItemModel *model )
    : QAbstractProxyModel( model )
    , m_model( model )
    , m_renameFolderAction( 0 )
    , m_deleteFolderAction( 0 )
{
    setSourceModel( model );
    // signal proxies
    connect( m_model,
        SIGNAL( dataChanged( const QModelIndex&, const QModelIndex& ) ),
        this, SLOT( modelDataChanged( const QModelIndex&, const QModelIndex& ) )
    );
    connect( m_model,
        SIGNAL( rowsInserted( const QModelIndex&, int, int ) ), this,
        SLOT( modelRowsInserted( const QModelIndex &, int, int ) ) );
    connect( m_model, SIGNAL( rowsRemoved( const QModelIndex&, int, int ) ),
        this, SLOT( modelRowsRemoved( const QModelIndex&, int, int ) ) );
    connect( m_model, SIGNAL( rowsAboutToBeRemoved( const QModelIndex &, int, int ) ),
             SLOT( modelRowsAboutToBeRemoved( const QModelIndex &, int, int ) ) );
    connect( m_model, SIGNAL( renameIndex( QModelIndex ) ), SLOT( slotRename( QModelIndex ) ) );
    connect( m_model, SIGNAL( layoutChanged() ), SLOT( buildTree() ) );

    buildTree();
}

PlaylistsInGroupsProxy::~PlaylistsInGroupsProxy()
{
}

/* m_groupHash layout
*  group index in m_groupNames: originalRow in m_model of child Playlists (no sub-groups yet)
*  group index = -1  for non-grouped playlists
*
* TODO: sub-groups
*/
void
PlaylistsInGroupsProxy::buildTree()
{
    DEBUG_BLOCK
    if( !m_model )
        return;

    emit layoutAboutToBeChanged();
    QStringList emptyGroups;
    for( int i = 0; i < m_groupNames.count(); i++ )
        if( m_groupHash.values( i ).count() == 0 )
            emptyGroups << m_groupNames[i];

    m_groupHash.clear();
    m_groupNames.clear();
    m_parentCreateList.clear();
    m_groupNames = emptyGroups;

    int max = m_model->rowCount();
    debug() << QString("building tree with %1 leafs.").arg( max );
    for( int row = 0; row < max; row++ )
    {
        QModelIndex idx = m_model->index( row, 0, QModelIndex() );
        //Playlists can be in multiple groups but we only use the first TODO: multigroup
        QStringList groups = idx.data( PlaylistBrowserNS::UserModel::GroupRole ).toStringList();
        QString groupName = groups.isEmpty() ? QString() : groups.first();
        debug() << QString("index %1 belongs to groupName %2").arg( row ).arg( groupName );

        int groupIndex = m_groupNames.indexOf( groupName ); //groups are added to the end of the existing list
        if( groupIndex == -1 && !groupName.isEmpty() )
        {
            m_groupNames << groupName;
            groupIndex = m_groupNames.count() - 1;
        }

        m_groupHash.insertMulti( groupIndex, row );
    }
    debug() << "m_groupHash: ";
    for( int groupIndex = 0; groupIndex < m_groupNames.count(); groupIndex++ )
        debug() << m_groupNames[groupIndex] << ": " << m_groupHash.values( groupIndex );
    debug() << m_groupHash.values( -1 );

    emit layoutChanged();
}

int
PlaylistsInGroupsProxy::indexOfParentCreate( const QModelIndex &parent ) const
{
    if( !parent.isValid() )
        return -1;

    struct ParentCreate pc;
    for( int i = 0 ; i < m_parentCreateList.size() ; i++ )
    {
        pc = m_parentCreateList[i];
        if( pc.parentCreateIndex == parent.internalId() && pc.row == parent.row() )
            return i;
    }
    pc.parentCreateIndex = parent.internalId();
    pc.row = parent.row();
    m_parentCreateList << pc;
    return m_parentCreateList.size() - 1;
}

QModelIndex
PlaylistsInGroupsProxy::index( int row, int column, const QModelIndex& parent ) const
{
    if( !hasIndex(row, column, parent) )
        return QModelIndex();

    int parentCreateIndex = indexOfParentCreate( parent );

    return createIndex( row, column, parentCreateIndex );
}

QModelIndex
PlaylistsInGroupsProxy::parent( const QModelIndex &index ) const
{
    if (!index.isValid())
        return QModelIndex();

    int parentCreateIndex = index.internalId();
    if( parentCreateIndex == -1 || parentCreateIndex >= m_parentCreateList.count() )
        return QModelIndex();

    struct ParentCreate pc = m_parentCreateList[parentCreateIndex];
    return createIndex( pc.row, index.column(), pc.parentCreateIndex );
}

int
PlaylistsInGroupsProxy::rowCount( const QModelIndex& index ) const
{
    if( !index.isValid() )
    {
        //the number of top level groups + the number of non-grouped playlists
        int rows = m_groupNames.count() + m_groupHash.values( -1 ).count();
        return rows;
    }

    //TODO:group in group support.
    if( isGroup( index ) )
    {
        qint64 groupIndex = index.row();
        return m_groupHash.count( groupIndex );
    }

    QModelIndex originalIndex = mapToSource( index );
    return m_model->rowCount( originalIndex );
}

QVariant
PlaylistsInGroupsProxy::data( const QModelIndex &index, int role ) const
{
    if( !index.isValid() )
        return QVariant();

    int row = index.row();
    if( isGroup( index ) )
    {
        switch( role )
        {
            case Qt::DisplayRole: return QVariant( m_groupNames[row] );
            case Qt::DecorationRole: return QVariant( KIcon( "folder" ) );
            default: return QVariant();
        }
    }

    return mapToSource( index ).data( role );
}

bool
PlaylistsInGroupsProxy::setData( const QModelIndex &index, const QVariant &value, int role )
{
    DEBUG_BLOCK
    if( isGroup( index ) )
        return changeGroupName( index.data( Qt::DisplayRole ).toString(), value.toString() );

    QModelIndex originalIdx = mapToSource( index );
    return m_model->setData( originalIdx, value, role );
}

bool
PlaylistsInGroupsProxy::removeRows( int row, int count, const QModelIndex &parent )
{
    DEBUG_BLOCK
    debug() << "in parent " << parent << "remove " << count << " starting at row " << row;
    if( isGroup( parent ) )
    {
        deleteFolder( parent );
        return true;
    }

    QModelIndex originalIdx = mapToSource( parent );
    bool success = m_model->removeRows( row, count, originalIdx );

    return success;
}

QStringList
PlaylistsInGroupsProxy::mimeTypes() const
{
    QStringList mimeTypes = m_model->mimeTypes();
    mimeTypes << AmarokMimeData::PLAYLISTBROWSERGROUP_MIME;
    return mimeTypes;
}

QMimeData *
PlaylistsInGroupsProxy::mimeData( const QModelIndexList &indexes ) const
{
    DEBUG_BLOCK
    AmarokMimeData* mime = new AmarokMimeData();
    QModelIndexList sourceIndexes;
    foreach( const QModelIndex &idx, indexes )
    {
        debug() << idx;
        if( isGroup( idx ) )
        {
            debug() << "is a group, add mimeData of all children";
        }
        else
        {
            debug() << "is original item, add mimeData from source model";
            sourceIndexes << mapToSource( idx );
        }
    }

    if( !sourceIndexes.isEmpty() )
        return m_model->mimeData( sourceIndexes );

    return mime;
}

bool
PlaylistsInGroupsProxy::dropMimeData( const QMimeData *data, Qt::DropAction action,
                                   int row, int column, const QModelIndex &parent )
{
    DEBUG_BLOCK
    Q_UNUSED( row );
    Q_UNUSED( column );
    debug() << "droped on " << QString("row: %1, column: %2, parent:").arg( row ).arg( column );
    debug() << parent;
    if( action == Qt::IgnoreAction )
    {
        debug() << "ignored";
        return true;
    }

    if( !parent.isValid() )
    {
        debug() << "dropped on the root";
        const AmarokMimeData *amarokMime = dynamic_cast<const AmarokMimeData *>(data);
        if( amarokMime == 0 )
        {
            debug() << "ERROR: could not cast to amarokMimeData at " << __FILE__ << __LINE__;
            return false;
        }
        Meta::PlaylistList playlists = amarokMime->playlists();
        foreach( Meta::PlaylistPtr playlist, playlists )
            playlist->setGroups( QStringList() );
        buildTree();
        return true;
    }

    if( isGroup( parent ) )
    {
        debug() << "dropped on a group";
        if( data->hasFormat( AmarokMimeData::PLAYLIST_MIME ) )
        {
            debug() << "playlist dropped on group";
            if( parent.row() < 0 || parent.row() >= m_groupNames.count() )
            {
                debug() << "ERROR: something went seriously wrong in " << __FILE__ << __LINE__;
                return false;
            }
            //apply the new groupname to the source index
            QString groupName = m_groupNames.value( parent.row() );
            debug() << QString("apply the new groupname %1 to the source index").arg( groupName );
            const AmarokMimeData *amarokMime = dynamic_cast<const AmarokMimeData *>(data);
            if( amarokMime == 0 )
            {
                debug() << "ERROR: could not cast to amarokMimeData at " << __FILE__ << __LINE__;
                return false;
            }
            Meta::PlaylistList playlists = amarokMime->playlists();
            foreach( Meta::PlaylistPtr playlist, playlists )
                playlist->setGroups( QStringList( groupName ) );
            buildTree();
            return true;
        }
        else if( data->hasFormat( AmarokMimeData::PLAYLISTBROWSERGROUP_MIME ) )
        {
            debug() << "playlistgroup dropped on group";
            //TODO: multilevel group support
            debug() << "ignore drop until we have multilevel group support";
            return false;
        }
    }
    else
    {
        debug() << "not dropped on the root or on a group";
        QModelIndex sourceIndex = mapToSource( parent );
        return m_model->dropMimeData( data, action, row, column,
                               sourceIndex );
    }

    return false;
}

Qt::DropActions
PlaylistsInGroupsProxy::supportedDropActions() const
{
    //always add MoveAction because playlists can be put into a different group
    return m_model->supportedDropActions() | Qt::MoveAction;
}

Qt::DropActions
PlaylistsInGroupsProxy::supportedDragActions() const
{
    //always add MoveAction because playlists can be put into a different group
    return m_model->supportedDragActions() | Qt::MoveAction;
}

int
PlaylistsInGroupsProxy::columnCount( const QModelIndex& index ) const
{
    return m_model->columnCount( mapToSource( index ) );
}

bool
PlaylistsInGroupsProxy::isGroup( const QModelIndex &index ) const
{
    int parentCreateIndex = index.internalId();
    if( parentCreateIndex == -1 && index.row() < m_groupNames.count() )
        return true;
    return false;
}

QModelIndex
PlaylistsInGroupsProxy::mapToSource( const QModelIndex& index ) const
{
    //DEBUG_BLOCK
//    debug() << "index: " << index;
    if( !index.isValid() )
        return QModelIndex();

    if( isGroup( index ) )
        return QModelIndex();

    QModelIndex proxyParent = index.parent();
//    debug() << "proxyParent: " << proxyParent;
    QModelIndex originalParent = mapToSource( proxyParent );
//    debug() << "originalParent: " << originalParent;
    int originalRow = index.row();
    if( !originalParent.isValid() ) //it is a child of the parent's rootnode (1st level)
    {
        int indexInGroup = index.row();
        if( !proxyParent.isValid() ) //it is not in a group so it's after the group items
            indexInGroup -= m_groupNames.count();
//        debug() << "indexInGroup" << indexInGroup;
        QList<int> childRows = m_groupHash.values( proxyParent.row() );
        if( childRows.isEmpty() || indexInGroup >= childRows.count() )
            return QModelIndex();

        originalRow = childRows.at( indexInGroup );
//        debug() << "originalRow: " << originalRow;
    }
    return m_model->index( originalRow, index.column(), originalParent );
}

QModelIndexList
PlaylistsInGroupsProxy::mapToSource( const QModelIndexList& list ) const
{
    QModelIndexList originalList;
    foreach( const QModelIndex &index, list )
    {
        QModelIndex originalIndex = mapToSource( index );
        if( originalIndex.isValid() )
            originalList << originalIndex;
    }
    return originalList;
}

QModelIndex
PlaylistsInGroupsProxy::mapFromSource( const QModelIndex& idx ) const
{
    DEBUG_BLOCK
    debug() << "index: " << idx;
    if( !idx.isValid() )
        return QModelIndex();

    QModelIndex proxyParent;
    QModelIndex sourceParent = idx.parent();
    debug() << "sourceParent: " << sourceParent;
    int proxyRow = idx.row();
    int sourceRow = idx.row();

    if( sourceParent.isValid() )
    {
        //idx is a child of one of the items in the source model
        proxyParent = mapFromSource( sourceParent );
    }
    else
    {
        //idx is an item of the top level of the source model (child of the rootnode)
        int groupRow = m_groupHash.key( sourceRow, -1 );

        if( groupRow != -1 ) //it's in a group, let's find the correct row.
        {
            proxyParent = this->index( groupRow, 0, QModelIndex() );
            proxyRow = m_groupHash.values( groupRow ).indexOf( sourceRow );
        }
        else
        {
            proxyParent = QModelIndex();
            // if the proxy item is not in a group it will be below the groups.
            int groupLength = m_groupNames.count();
            debug() << "groupNames length: " << groupLength;
            int i = m_groupHash.values( -1 ).indexOf( sourceRow );
            debug() << "index in hash: " << i;
            proxyRow = groupLength + i;
        }
    }

    debug() << "proxyParent: " << proxyParent;
    debug() << "proxyRow: " << proxyRow;
    return this->index( proxyRow, 0, proxyParent );
}

Qt::ItemFlags
PlaylistsInGroupsProxy::flags( const QModelIndex &index ) const
{
    if( isGroup( index ) )
        return ( Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable |
                 Qt::ItemIsDropEnabled | Qt::ItemIsDragEnabled );

    QModelIndex originalIdx = mapToSource( index );
    Qt::ItemFlags originalItemFlags = m_model->flags( originalIdx );

    //make the original one drag enabled if it didn't have it yet. We can drag it on a group.
    return originalItemFlags | Qt::ItemIsDragEnabled;
}

void
PlaylistsInGroupsProxy::modelDataChanged( const QModelIndex& start, const QModelIndex& end )
{
    DEBUG_BLOCK
    //TODO: see if new groups have to be created, deleted or adjusted. Try to avoid buildTree()
    emit dataChanged( mapFromSource( start ), mapFromSource( end ) );
}

void
PlaylistsInGroupsProxy::modelRowsInserted( const QModelIndex& index, int start, int end )
{
    Q_UNUSED( index )
    Q_UNUSED( start )
    Q_UNUSED( end )
    DEBUG_BLOCK
    //TODO: see if new groups have to be created, deleted or adjusted. Try to avoid buildTree()
    buildTree();
}

void
PlaylistsInGroupsProxy::modelRowsAboutToBeRemoved( const QModelIndex &sourceParent,
                                                   int start, int end ) //SLOT
{
    DEBUG_BLOCK
    debug() << "sourceParent: " << sourceParent;
    debug() << "start: " << start;
    debug() << "end: " << end;
    QModelIndex proxyParent = mapFromSource( sourceParent );
    debug() << "proxyParent: " << proxyParent;
//HACK: we need to use beginRemoveRows( proxyParent, start, end ); but it's not working
}

void
PlaylistsInGroupsProxy::modelRowsRemoved( const QModelIndex &sourceParent, int start,
                                          int end )
{
    DEBUG_BLOCK
    //TODO: see if groups have to be created or adjusted.
    debug() << "sourceParent: " << sourceParent;
    debug() << "start: " << start;
    debug() << "end: " << end;
    QModelIndex proxyParent = mapFromSource( sourceParent );
    debug() << "proxyParent: " << proxyParent;
//HACK: we need to use endRemoveRows() here, but it's not working. So reset the entire proxy
    buildTree();
}

void
PlaylistsInGroupsProxy::slotRename( QModelIndex sourceIdx )
{
    QModelIndex proxyIdx = mapFromSource( sourceIdx );
    emit renameIndex( proxyIdx );
}

void
PlaylistsInGroupsProxy::slotDeleteFolder()
{
    DEBUG_BLOCK
    if( m_selectedGroups.count() == 0 )
        return;

    QModelIndex groupIdx = m_selectedGroups.first();
    deleteFolder( groupIdx );
}

void
PlaylistsInGroupsProxy::slotRenameFolder()
{
    DEBUG_BLOCK
    //get the name for this new group
    //TODO: do inline rename
    QModelIndex folder = m_selectedGroups.first();
    QString folderName = folder.data( Qt::DisplayRole ).toString();
    bool ok;
    const QString newName = KInputDialog::getText( i18n("New name"),
                i18nc("Enter a new name for a folder that already exists",
                      "Enter new folder name:"),
                folderName,
                &ok );
    if( !ok || newName == folderName )
        return;

    foreach( int originalRow, m_groupHash.values( folder.row() ) )
    {
        QModelIndex index = m_model->index( originalRow, 0, QModelIndex() );
        Meta::PlaylistPtr playlist = index.data( 0xf00d ).value<Meta::PlaylistPtr>();
        QStringList groups;
        groups << newName;
        if( playlist )
            playlist->setGroups( groups );
    }
    buildTree();
    emit layoutChanged();
}

void
PlaylistsInGroupsProxy::slotAddToFolder()
{
    DEBUG_BLOCK
    const QString name = KInputDialog::getText( i18n("New name"),
                i18n("Enter new group name:") );
    foreach( QModelIndex index, m_selectedPlaylists )
    {
        Meta::PlaylistPtr playlist = index.data( 0xf00d ).value<Meta::PlaylistPtr>();
        QStringList groups;
        groups << name;
        if( playlist )
            playlist->setGroups( groups );
    }
    buildTree();
}

QList<QAction *>
PlaylistsInGroupsProxy::createGroupActions()
{
    QList<QAction *> actions;

    if ( m_deleteFolderAction == 0 )
    {
        m_deleteFolderAction = new QAction( KIcon( "media-track-remove-amarok" ), i18n( "&Delete Folder" ), this );
        m_deleteFolderAction->setProperty( "popupdropper_svg_id", "delete_group" );
        connect( m_deleteFolderAction, SIGNAL( triggered() ), this, SLOT( slotDeleteGroup() ) );
    }
    actions << m_deleteFolderAction;

    if ( m_renameFolderAction == 0 )
    {
        m_renameFolderAction =  new QAction( KIcon( "media-track-edit-amarok" ), i18n( "&Rename Folder..." ), this );
        m_renameFolderAction->setProperty( "popupdropper_svg_id", "edit_group" );
        connect( m_renameFolderAction, SIGNAL( triggered() ), this, SLOT( slotRenameGroup() ) );
    }
    actions << m_renameFolderAction;

    return actions;
}

bool
PlaylistsInGroupsProxy::isAGroupSelected( const QModelIndexList& list ) const
{
    foreach( const QModelIndex &index, list )
    {
        if( isGroup( index ) )
            return true;
    }
    return false;
}

bool
PlaylistsInGroupsProxy::isAPlaylistSelected( const QModelIndexList& list ) const
{
    return mapToSource( list ).count() > 0;
}

bool
PlaylistsInGroupsProxy::changeGroupName( const QString &from, const QString &to )
{
    DEBUG_BLOCK
    debug() << " from:" << from << "to: " << to;
    if( !m_groupNames.contains( from ) )
        return false;

    int groupRow = m_groupNames.indexOf( from );
    debug() << "row to change: " << groupRow;
    QModelIndex groupIdx = this->index( groupRow, 0, QModelIndex() );
    foreach( int childRow, m_groupHash.values( groupRow ) )
    {
        QModelIndex childIndex = this->index( childRow, 0, groupIdx );
        QModelIndex originalIdx = mapToSource( childIndex );
        m_model->setData( originalIdx, to, PlaylistBrowserNS::UserModel::GroupRole );
    }
    m_groupNames[groupRow] = to;
    buildTree();
    return 0;
}

void
PlaylistsInGroupsProxy::deleteFolder( const QModelIndex &groupIdx )
{
    DEBUG_BLOCK
    //TODO: ask the user for configmation to delete children
    for( int i = 0; i < m_groupHash.values( groupIdx.row() ).count(); i++ )
    {
        QModelIndex proxyIdx = index( i, 0, groupIdx );
        if( !proxyIdx.isValid() )
            continue;
        QModelIndex sourceIdx = mapToSource( proxyIdx );
        m_model->removeRow( sourceIdx.row(), sourceIdx.parent() );
    }
    m_groupNames.removeAt( groupIdx.row() );
    buildTree();
}

QList<QAction *>
PlaylistsInGroupsProxy::actionsFor( const QModelIndexList &list )
{
    DEBUG_BLOCK
    bool playlistSelected = isAPlaylistSelected( list );
    bool groupSelected = isAGroupSelected( list );

    QList<QAction *> actions;
    m_selectedGroups.clear();
    m_selectedPlaylists.clear();

    //only playlists selected
    if( playlistSelected )
    {
        foreach( const QModelIndex &index, list )
        {
            QModelIndexList tempList;
            tempList << index;
            if( isAPlaylistSelected( tempList ) )
                m_selectedPlaylists << index;
        }
        QModelIndexList originalList = mapToSource( list );
        debug() << originalList.count() << "original indices";
        MetaPlaylistModel *mpm = dynamic_cast<MetaPlaylistModel *>(m_model);
        if( mpm == 0 )
            return actions;
        if( !originalList.isEmpty() )
            actions << mpm->actionsFor( originalList );
    }
    else if( groupSelected )
    {
        QModelIndexList originalList;
        originalList << m_model->index( 0, 0, QModelIndex() );
        originalList << m_model->index( 1, 0, QModelIndex() );
        MetaPlaylistModel *mpm = dynamic_cast<MetaPlaylistModel *>(m_model);
        if( mpm == 0 )
            return actions;
        actions << mpm->actionsFor( originalList );
        actions << createGroupActions();
        foreach( const QModelIndex &index, list )
        {
            QModelIndexList tempList;
            tempList << index;
            if( isAGroupSelected( tempList ) )
                m_selectedGroups << index;
        }
    }

    return actions;
}

void
PlaylistsInGroupsProxy::loadItems( QModelIndexList list, Playlist::AddOptions insertMode )
{
    QModelIndexList originalList = mapToSource( list );
    MetaPlaylistModel *mpm = dynamic_cast<MetaPlaylistModel *>(m_model);
    if( mpm == 0 )
        return;
    mpm->loadItems( originalList, insertMode );
}

QModelIndex
PlaylistsInGroupsProxy::createNewGroup( const QString &groupName )
{
    m_groupNames << groupName;
    emit layoutChanged();
    return index( m_groupNames.count() - 1, 0, QModelIndex() );
}

#include "PlaylistsInGroupsProxy.moc"
