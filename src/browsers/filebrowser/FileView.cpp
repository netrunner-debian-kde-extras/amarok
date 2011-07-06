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

#define DEBUG_PREFIX "FileView"

#include "FileView.h"

#include "context/ContextView.h"
#include "context/popupdropper/libpud/PopupDropper.h"
#include "context/popupdropper/libpud/PopupDropperItem.h"
#include "core-impl/collections/support/CollectionManager.h"
#include "core-impl/collections/support/FileCollectionLocation.h"
#include "core-impl/playlists/types/file/PlaylistFileSupport.h"
#include "core/interfaces/Logger.h"
#include "core/playlists/PlaylistFormat.h"
#include "core/support/Debug.h"
#include "dialogs/TagDialog.h"
#include "DirectoryLoader.h"
#include "EngineController.h"
#include "PaletteHandler.h"
#include "playlist/PlaylistController.h"
#include "PopupDropperFactory.h"

#include "SvgHandler.h"
#include "src/transcoding/TranscodingJob.h"
#include "src/transcoding/TranscodingAssistantDialog.h"
#include "src/core/transcoding/TranscodingController.h"

#include <KAction>
#include <KIO/CopyJob>
#include <KIO/DeleteJob>
#include <KDialog>
#include <KDirModel>
#include <KFileItem>
#include <KGlobalSettings>
#include <KMessageBox>
#include <KIcon>
#include <KLocale>
#include <KMenu>
#include <KUrl>

#include <QContextMenuEvent>
#include <QFileSystemModel>
#include <QItemDelegate>
#include <QPainter>

FileView::FileView( QWidget *parent )
    : Amarok::PrettyTreeView( parent )
    , m_appendAction( 0 )
    , m_loadAction( 0 )
    , m_editAction( 0 )
    , m_separator1( 0 )
    , m_deleteAction( 0 )
    , m_pd( 0 )
    , m_ongoingDrag( false )
    , m_moveActivated( false )
    , m_copyActivated( false )
    , m_moveAction( 0 )
    , m_copyAction( 0 )
{
    setFrameStyle( QFrame::NoFrame );
    setItemsExpandable( false );
    setRootIsDecorated( false );
    setAlternatingRowColors( true );
    setUniformRowHeights( true );

    The::paletteHandler()->updateItemView( this );
    connect( The::paletteHandler(), SIGNAL(newPalette( const QPalette & )),
                                    SLOT(newPalette( const QPalette & )) );
}

void
FileView::contextMenuEvent( QContextMenuEvent *e )
{
    if( !model() )
        return;

    //trying to do fancy stuff while showing places only leads to tears!
    if( model()->objectName() == "PLACESMODEL" )
    {
        e->accept();
        return;
    }

    QModelIndexList indices = selectedIndexes();
    // Abort if nothing is selected
    if( indices.isEmpty() )
        return;

    KMenu* menu = new KMenu( this );
    QList<QAction *> actions = actionsForIndices( indices );
    foreach( QAction * action, actions )
        menu->addAction( action );

    // Create Copy/Move to menu items
    // ported from old filebrowser
    QList<Collections::Collection*> writableCollections;
    QHash<Collections::Collection*, CollectionManager::CollectionStatus> hash =
            CollectionManager::instance()->collections();
    QHash<Collections::Collection*, CollectionManager::CollectionStatus>::const_iterator it =
            hash.constBegin();
    while( it != hash.constEnd() )
    {
        Collections::Collection *coll = it.key();
        if( coll && coll->isWritable() )
            writableCollections.append( coll );
        ++it;
    }
    if( !writableCollections.isEmpty() )
    {
        QMenu *moveMenu = new QMenu( i18n( "Move to Collection" ), this );
        foreach( Collections::Collection *coll, writableCollections )
        {
            CollectionAction *moveAction = new CollectionAction( coll, this );
            connect( moveAction, SIGNAL( triggered() ), this, SLOT( slotPrepareMoveTracks() ) );
            moveMenu->addAction( moveAction );
        }
        menu->addMenu( moveMenu );

        QMenu *copyMenu = new QMenu( i18n( "Copy to Collection" ), this );
        foreach( Collections::Collection *coll, writableCollections )
        {
            CollectionAction *copyAction = new CollectionAction( coll, this );
            connect( copyAction, SIGNAL( triggered() ), this, SLOT( slotPrepareCopyTracks() ) );
            copyMenu->addAction( copyAction );
        }
        menu->addMenu( copyMenu );
    }
    KAction *transcodeAction = new KAction( "Transcode here", this );
    connect( transcodeAction, SIGNAL( triggered() ), this, SLOT( slotPrepareTranscodeTracks() ) );
    menu->addAction( transcodeAction );
    transcodeAction->setVisible( false ); //This is just used for debugging, hide it!

    menu->exec( e->globalPos() );
}

void
FileView::mouseReleaseEvent( QMouseEvent *event )
{
    QModelIndex index = indexAt( event->pos() );
    if( !index.isValid() )
    {
        m_lastSelectedIndex = QModelIndex();
        event->accept();
        return;
    }

    QModelIndexList indices = selectedIndexes();
    if( indices.count() == 1 && KGlobalSettings::singleClick() )
    {
        const QVariant qvar = index.data( KDirModel::FileItemRole );
        if( qvar.canConvert<KFileItem>() )
        {
            KFileItem item = index.data( KDirModel::FileItemRole ).value<KFileItem>();
            if( item.isDir() )
            {
                m_lastSelectedIndex = QModelIndex();
                Amarok::PrettyTreeView::mouseReleaseEvent( event );
                return;
            }

            // check if the last selected item was clicked again, if so then trigger editor
            if( m_lastSelectedIndex != index )
            {
                m_lastSelectedIndex = index;
            }
            else
            {
                QTimer::singleShot( QApplication::doubleClickInterval(), this,
                                    SLOT(slotEditTriggered())
                                  );
                m_lastSelectedIndex = QModelIndex();
            }
            event->accept();
            return;
        }
    }
    m_lastSelectedIndex = QModelIndex();
    Amarok::PrettyTreeView::mouseReleaseEvent( event );
}

void
FileView::slotEditTriggered()
{
    QModelIndexList indices = selectedIndexes();
    if( indices.count() == 1 && indices.first().isValid() )
        Amarok::PrettyTreeView::edit( indices.first() );
}

void
FileView::mouseDoubleClickEvent( QMouseEvent *event )
{
    m_lastSelectedIndex = QModelIndex();
    QModelIndex index = indexAt( event->pos() );
    if( !index.isValid() )
    {
        event->accept();
        return;
    }
    emit activated( index );
    event->accept();
}

void
FileView::slotAppendToPlaylist()
{
    addSelectionToPlaylist( false );
}


void
FileView::slotReplacePlaylist()
{
    addSelectionToPlaylist( true );
}

void
FileView::slotEditTracks()
{
    Meta::TrackList tracks = tracksForEdit();
    if( !tracks.isEmpty() )
    {
        TagDialog *dialog = new TagDialog( tracks, this );
        dialog->show();
    }
}

void
FileView::slotPrepareTranscodeTracks()
{
    DEBUG_BLOCK
    KAction *action = qobject_cast< KAction * >( sender() );
    if( !action )
        return;

    const KFileItemList list = selectedItems();
    if( list.isEmpty() )
        return;

    if( !Amarok::Components::transcodingController()->availableFormats().isEmpty() )
    {
        Transcoding::AssistantDialog *d = new Transcoding::AssistantDialog( this );
        d->show();
    }
    else
    {
        debug() << "FFmpeg is not installed or does not support any of the required formats.";
    }
}

void
FileView::slotPrepareMoveTracks()
{
    if( m_moveActivated )
        return;

    CollectionAction *action = dynamic_cast<CollectionAction*>( sender() );
    if( !action )
        return;

    m_moveActivated = true;
    m_moveAction = action;

    const KFileItemList list = selectedItems();
    if( list.isEmpty() )
        return;

    DirectoryLoader* dl = new DirectoryLoader();
    connect( dl, SIGNAL(finished( const Meta::TrackList& )),
                 SLOT(slotMoveTracks( const Meta::TrackList& )) );
    dl->init( list.urlList() );
}

void
FileView::slotPrepareCopyTracks()
{
    if( m_copyActivated )
        return;

    CollectionAction *action = dynamic_cast<CollectionAction*>( sender() );
    if( !action )
        return;

    m_copyActivated = true;
    m_copyAction = action;

    const KFileItemList list = selectedItems();
    if( list.isEmpty() )
        return;

    DirectoryLoader* dl = new DirectoryLoader();
    connect( dl, SIGNAL(finished( const Meta::TrackList & )),
                 SLOT(slotCopyTracks( const Meta::TrackList & )) );
    dl->init( list.urlList() );
}

void
FileView::slotCopyTracks( const Meta::TrackList& tracks )
{
    DEBUG_BLOCK
    if( !m_copyAction || !m_copyActivated )
        return;

    QSet<Collections::Collection*> collections;
    foreach( const Meta::TrackPtr &track, tracks )
    {
        collections.insert( track->collection() );
    }

    if( collections.count() == 1 )
    {
        Collections::Collection *sourceCollection = collections.values().first();
        Collections::CollectionLocation *source;
        if( sourceCollection )
        {
            source = sourceCollection->location();
        }
        else
        {
            source = new Collections::FileCollectionLocation();
        }
        Collections::CollectionLocation *destination = m_copyAction->collection()->location();
        Transcoding::AssistantDialog dialog( this );
        Transcoding::Configuration configuration = Transcoding::Configuration();
        if( dialog.exec() )
        {
            configuration = dialog.configuration();
            source->prepareCopy( tracks, destination, configuration );
        }
    }
    else
    {
        warning() << "Cannot handle copying tracks from multiple collections, doing nothing to be safe";
    }
    m_copyActivated = false;
    m_copyAction = 0;
}

void
FileView::slotMoveTracks( const Meta::TrackList& tracks )
{
    if( !m_moveAction || !m_moveActivated )
        return;

    QSet<Collections::Collection*> collections;
    foreach( const Meta::TrackPtr &track, tracks )
    {
        collections.insert( track->collection() );
    }
    if( collections.count() == 1 )
    {
        Collections::Collection *sourceCollection = collections.values().first();
        Collections::CollectionLocation *source;
        if( sourceCollection )
        {
            source = sourceCollection->location();
        }
        else
        {
            source = new Collections::FileCollectionLocation();
        }
        Collections::CollectionLocation *destination = m_moveAction->collection()->location();

        source->prepareMove( tracks, destination );
    }
    else
    {
        warning() << "Cannot handle moving tracks from multipe collections, doing nothing to be safe";
    }
    m_moveActivated = false;
    m_moveAction = 0;
}

QList<QAction *>
FileView::actionsForIndices( const QModelIndexList &indices )
{
    QList<QAction *> actions;

    if( indices.isEmpty() )
        return actions; // get out of here!

    if( m_appendAction == 0 )
    {
        m_appendAction = new QAction( KIcon( "media-track-add-amarok" ), i18n( "&Add to Playlist" ),
                                     this );
        m_appendAction->setProperty( "popupdropper_svg_id", "append" );
        connect( m_appendAction, SIGNAL( triggered() ), this, SLOT( slotAppendToPlaylist() ) );
    }

    actions.append( m_appendAction );

    if( m_loadAction == 0 )
    {
        m_loadAction = new QAction( KIcon( "folder-open" ),
                                   i18nc( "Replace the currently loaded tracks with these",
                                          "&Replace Playlist" ),
                                   this
                                  );
        m_loadAction->setProperty( "popupdropper_svg_id", "load" );
        connect( m_loadAction, SIGNAL( triggered() ), this, SLOT( slotReplacePlaylist() ) );
    }

    actions.append( m_loadAction );

    if( m_editAction == 0 )
    {
        m_editAction = new QAction( KIcon( "media-track-edit-amarok" ),
                                    i18n( "&Edit Track Details" ), this );
        m_editAction->setProperty( "popupdropper_svg_id", "edit" );
        connect( m_editAction, SIGNAL( triggered() ), this, SLOT( slotEditTracks() ) );
    }

    actions.append( m_editAction );

    if( m_separator1 == 0 )
    {
            m_separator1 = new QAction( this );
            m_separator1->setSeparator( true );
    }

    actions.append( m_separator1 );

    if( m_deleteAction == 0 )
    {
        m_deleteAction = new KAction( KIcon( "media-track-remove-amarok" ), i18n( "&Delete" ), this );
        m_deleteAction->setProperty( "popupdropper_svg_id", "delete_file" );
        connect( m_deleteAction, SIGNAL(triggered(Qt::MouseButtons,Qt::KeyboardModifiers)),
                 this, SLOT(slotDelete(Qt::MouseButtons,Qt::KeyboardModifiers)) );
    }

    actions.append( m_deleteAction );

    Meta::TrackList tracks = tracksForEdit();
    m_editAction->setEnabled( !tracks.isEmpty() );

    return actions;
}

void
FileView::addSelectionToPlaylist( bool replace )
{
    QModelIndexList indices = selectedIndexes();

    if( indices.count() == 0 )
        return;
    QList<KUrl> urls;

    foreach( const QModelIndex& index, indices )
    {
        KFileItem file = index.data( KDirModel::FileItemRole ).value<KFileItem>();
        if( EngineController::canDecode( file.url() ) || Playlists::isPlaylist( file.url() ) || file.isDir() )
        {
            urls << file.url();
        }
    }

    The::playlistController()->insertOptioned( urls, replace ? Playlist::Replace : Playlist::AppendAndPlay );
}

void
FileView::startDrag( Qt::DropActions supportedActions )
{
    m_lastSelectedIndex = QModelIndex();

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
        QModelIndexList indices = selectedIndexes();

        QList<QAction *> actions = actionsForIndices( indices );

        QFont font;
        font.setPointSize( 16 );
        font.setBold( true );

        foreach( QAction *action, actions )
            m_pd->addItem( The::popupDropperFactory()->createItem( action ) );

        m_pd->show();
    }

    QTreeView::startDrag( supportedActions );

    if( m_pd )
    {
        connect( m_pd, SIGNAL( fadeHideFinished() ), m_pd, SLOT( clear() ) );
        m_pd->hide();
    }

    m_dragMutex.lock();
    m_ongoingDrag = false;
    m_dragMutex.unlock();
}

KFileItemList
FileView::selectedItems() const
{
    KFileItemList items;
    QModelIndexList indices = selectedIndexes();
    if( indices.isEmpty() )
        return items;

    foreach( const QModelIndex& index, indices )
    {
        KFileItem item = index.data( KDirModel::FileItemRole ).value<KFileItem>();
        items << item;
    }
    return items;
}

Meta::TrackList
FileView::tracksForEdit() const
{
    Meta::TrackList tracks;

    QModelIndexList indices = selectedIndexes();
    if( indices.isEmpty() )
        return tracks;

    foreach( const QModelIndex& index, indices )
    {   
        KFileItem item = index.data( KDirModel::FileItemRole ).value<KFileItem>();
        Meta::TrackPtr track = CollectionManager::instance()->trackForUrl( item.url() );
        if( track )
            tracks << track;
    }
    return tracks;
}

void
FileView::slotDelete( Qt::MouseButtons buttons, Qt::KeyboardModifiers modifiers )
{
    Q_UNUSED( buttons )
    DEBUG_BLOCK

    QModelIndexList indices = selectedIndexes();
    if( indices.isEmpty() )
        return;

    const bool skipTrash = modifiers.testFlag( Qt::ShiftModifier );
    QString caption;
    QString labelText;
    if( skipTrash )
    {
        caption = i18nc( "@title:window", "Confirm Delete" );
        labelText = i18np( "Are you sure you want to delete this item?",
                           "Are you sure you want to delete these %1 items?",
                           indices.count() );
    }
    else
    {
        caption = i18nc( "@title:window", "Confirm Move to Trash" );
        labelText = i18np( "Are you sure you want to move this item to trash?",
                           "Are you sure you want to move these %1 items to trash?",
                           indices.count() );
    }

    KDialog dialog;
    dialog.setCaption( caption );
    dialog.setButtons( KDialog::Ok | KDialog::Cancel );
    QLabel label( labelText, &dialog );
    dialog.setMainWidget( &label );
    if( dialog.exec() != QDialog::Accepted )
        return;

    KUrl::List urls;
    QStringList filepaths;
    foreach( const QModelIndex& index, indices )
    {
        KFileItem file = index.data( KDirModel::FileItemRole ).value<KFileItem>();
        filepaths << file.localPath();
        urls << file.url();
    }

    const bool cont = KMessageBox::warningContinueCancelList(
        0, labelText, filepaths, caption, KStandardGuiItem::remove() ) == KMessageBox::Continue;

    if( !cont )
        return;

    KIO::Job *job = skipTrash
        ? static_cast<KIO::Job*>( KIO::del( urls, KIO::HideProgressInfo ) )
        : static_cast<KIO::Job*>( KIO::trash( urls, KIO::HideProgressInfo ) );

    if( job )
    {
        QString statusText = i18ncp( "@info:status", "Moving to trash: 1 file",
                                     "Moving to trash: %1 files", urls.count() );
        Amarok::Components::logger()->newProgressOperation( job, statusText );
    }
}

#include "FileView.moc"
