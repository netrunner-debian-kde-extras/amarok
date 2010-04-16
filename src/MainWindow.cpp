/****************************************************************************************
 * Copyright (c) 2002-2009 Mark Kretschmann <kretschmann@kde.org>                       *
 * Copyright (c) 2002 Max Howell <max.howell@methylblue.com>                            *
 * Copyright (c) 2002 Gabor Lehel <illissius@gmail.com>                                 *
 * Copyright (c) 2002 Nikolaj Hald Nielsen <nhn@kde.org>                                *
 * Copyright (c) 2009 Artur Szymiec <artur.szymiec@gmail.com>                           *
 * Copyright (c) 2010 Téo Mrnjavac <teo@kde.org>                                        *
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

#define DEBUG_PREFIX "MainWindow"

#include "MainWindow.h"

#include <config-amarok.h>    //HAVE_LIBVISUAL definition

#include "aboutdialog/ExtendedAboutDialog.h"
#include "ActionClasses.h"
#include "core/support/Amarok.h"
#include "core/support/Debug.h"
#include "EngineController.h" //for actions in ctor
#include "KNotificationBackend.h"
#include "Osd.h"
#include "PaletteHandler.h"
#include "ScriptManager.h"
#include "SearchWidget.h"
#include "amarokconfig.h"
#include "aboutdialog/OcsData.h"
#include "amarokurls/AmarokUrlHandler.h"
#include "amarokurls/BookmarkManager.h"
#include "browsers/collectionbrowser/CollectionWidget.h"
#include "browsers/filebrowser/FileBrowser.h"
#include "browsers/playlistbrowser/PlaylistBrowser.h"
#include "browsers/servicebrowser/ServiceBrowser.h"
#include "core-impl/collections/support/CollectionManager.h"
#include "context/ContextScene.h"
#include "context/ContextView.h"
#include "context/ToolbarView.h"
#include "covermanager/CoverManager.h" // for actions
#include "dialogs/EqualizerDialog.h"
#include "likeback/LikeBack.h"
#include "moodbar/MoodbarManager.h"
#include "playlist/layouts/LayoutConfigAction.h"
#include "playlist/PlaylistActions.h"
#include "playlist/PlaylistController.h"
#include "playlist/PlaylistModelStack.h"
#include "playlist/PlaylistWidget.h"
#include "playlist/ProgressiveSearchWidget.h"
#include "playlistmanager/file/PlaylistFileProvider.h"
#include "playlistmanager/PlaylistManager.h"
#include "PodcastCategory.h"
#include "services/ServicePluginManager.h"
#include "services/scriptable/ScriptableService.h"
#include "statusbar/StatusBar.h"
#include "toolbar/SlimToolbar.h"
#include "toolbar/MainToolbar.h"
#include "SvgHandler.h"
#include "widgets/Splitter.h"
//#include "mediabrowser.h"

#include <KAction>          //m_actionCollection
#include <KActionCollection>
#include <KApplication>     //kapp
#include <KFileDialog>      //openPlaylist()
#include <KInputDialog>     //slotAddStream()
#include <KMessageBox>
#include <KLocale>
#include <KMenu>
#include <KMenuBar>
#include <KPixmapCache>
#include <KBugReport>
#include <KStandardAction>
#include <KStandardDirs>
#include <KWindowSystem>
#include <kabstractfilewidget.h>

#include <plasma/plasma.h>

#include <QCheckBox>
#include <QDesktopServices>
#include <QDesktopWidget>
#include <QDockWidget>
#include <QList>
#include <QSizeGrip>
#include <QStyle>
#include <QSysInfo>
#include <QVBoxLayout>

#ifdef Q_WS_X11
#include <fixx11h.h>
#endif

#ifdef Q_WS_MAC
#include "mac/GrowlInterface.h"
#endif

#define AMAROK_CAPTION "Amarok"


extern KAboutData aboutData;
extern OcsData ocsData;

class ContextWidget : public KVBox
{
    // Set a useful size default of the center tab.
    public:
        ContextWidget( QWidget *parent ) : KVBox( parent ) {}

        QSize sizeHint() const
        {
            return QSize( static_cast<QWidget*>( parent() )->size().width() / 3, 300 );
        }
};

QPointer<MainWindow> MainWindow::s_instance = 0;

namespace The {
    MainWindow* mainWindow() { return MainWindow::s_instance; }
}

MainWindow::MainWindow()
    : KMainWindow( 0 )
    , Engine::EngineObserver( The::engineController() )
    , m_lastBrowser( 0 )
    , m_waitingForCd( false )
    , m_mouseDown( false )
    , m_LH_initialized( false )
{
    DEBUG_BLOCK

    m_saveLayoutChangesTimer = new QTimer( this );
    m_saveLayoutChangesTimer->setSingleShot( true );
    connect( m_saveLayoutChangesTimer, SIGNAL( timeout() ), this, SLOT( saveLayout() ) );

    setObjectName( "MainWindow" );
    s_instance = this;

#ifdef Q_WS_MAC
    QSizeGrip* grip = new QSizeGrip( this );
    GrowlInterface* growl = new GrowlInterface( qApp->applicationName() );
#endif


    /* The ServicePluginManager needs to be loaded before the playlist model
    * (which gets started by "statusBar" below up so that it can handle any
    * tracks in the saved playlist that are associated with services. Eg, if
    * the playlist has a Magnatune track in it when Amarok is closed, then the
    * Magnatune service needs to be initialized before the playlist is loaded
    * here. */

    ServicePluginManager::instance();

    StatusBar * statusBar = new StatusBar( this );

    setStatusBar( statusBar );

    // Sets caption and icon correctly (needed e.g. for GNOME)
//     kapp->setTopWidget( this );
    PERF_LOG( "Set Top Widget" )
    createActions();
    PERF_LOG( "Created actions" )

    //new K3bExporter();

    KConfigGroup config = Amarok::config();
    const QSize size = config.readEntry( "MainWindow Size", QSize() );
    const QPoint pos = config.readEntry( "MainWindow Position", QPoint() );
    if( size.isValid() )
    {
        resize( size );
        move( pos );
    }

    The::paletteHandler()->setPalette( palette() );
    setPlainCaption( i18n( AMAROK_CAPTION ) );

    init();  // We could as well move the code from init() here, but meh.. getting a tad long

    //restore active category ( as well as filters and levels and whatnot.. )
    const QString path = config.readEntry( "Browser Path", QString() );
    if( !path.isEmpty() )
        m_browsers->list()->navigate( path );
}

MainWindow::~MainWindow()
{
    DEBUG_BLOCK

    KConfigGroup config = Amarok::config();
    config.writeEntry( "MainWindow Size", size() );
    config.writeEntry( "MainWindow Position", pos() );

    //save currently active category
    config.writeEntry( "Browser Path", m_browsers->list()->path() );

    QList<int> sPanels;

    //foreach( int a, m_splitter->saveState() )
    //    sPanels.append( a );

    saveLayout();

    //AmarokConfig::setPanelsSavedState( sPanels );

    delete m_browserDummyTitleBarWidget;
    delete m_contextDummyTitleBarWidget;
    delete m_playlistDummyTitleBarWidget;

    delete m_contextView;
    delete m_corona;
    //delete m_splitter;
    delete The::statusBar();
    delete The::svgHandler();
    delete The::paletteHandler();
}


///////// public interface

/**
 * This function will initialize the main window.
 */
void
MainWindow::init()
{
    DEBUG_BLOCK

    layout()->setContentsMargins( 0, 0, 0, 0 );
    layout()->setSpacing( 0 );

    //create main toolbar
    m_mainToolbar = new MainToolbar( 0 );
    m_mainToolbar->setAllowedAreas( Qt::TopToolBarArea | Qt::BottomToolBarArea );
    m_mainToolbar->setMovable ( true );
    addToolBar( Qt::TopToolBarArea, m_mainToolbar );
    m_mainToolbar->hide();

    //create slim toolbar
    m_slimToolbar = new SlimToolbar( 0 );
    m_slimToolbar->setAllowedAreas( Qt::TopToolBarArea | Qt::BottomToolBarArea );
    m_slimToolbar->setMovable ( true );
    addToolBar( Qt::TopToolBarArea, m_slimToolbar );
    m_slimToolbar->hide();

    //BEGIN Creating Widgets
    PERF_LOG( "Create sidebar" )
    m_browsers = new BrowserWidget( this );
    m_browsers->setSizePolicy( QSizePolicy::Preferred, QSizePolicy::Ignored );

    m_browserDummyTitleBarWidget = new QWidget();
    m_contextDummyTitleBarWidget = new QWidget();
    m_playlistDummyTitleBarWidget = new QWidget();

    m_browsersDock = new QDockWidget( i18n( "Media Sources" ), this );
    m_browsersDock->setObjectName( "Media Sources dock" );
    m_browsersDock->setWidget( m_browsers );
    m_browsersDock->setAllowedAreas( Qt::AllDockWidgetAreas );
    m_browsersDock->installEventFilter( this );
    PERF_LOG( "Sidebar created" )

    PERF_LOG( "Create Playlist" )
    m_playlistWidget = new Playlist::Widget( 0 );
    m_playlistWidget->setSizePolicy( QSizePolicy::Preferred, QSizePolicy::Ignored );
    m_playlistWidget->setFocus( Qt::ActiveWindowFocusReason );

    m_playlistDock = new QDockWidget( i18n( "Playlist" ), this );
    m_playlistDock->setObjectName( "Playlist dock" );
    m_playlistDock->setWidget( m_playlistWidget );
    m_playlistDock->setAllowedAreas( Qt::AllDockWidgetAreas );
    m_playlistDock->installEventFilter( this );
    PERF_LOG( "Playlist created" )

    PERF_LOG( "Creating ContextWidget" )
    m_contextWidget = new ContextWidget( this );
    m_contextWidget->setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Expanding );
    m_contextWidget->setSpacing( 0 );
    m_contextWidget->setFrameShape( QFrame::NoFrame );
    m_contextWidget->setFrameShadow( QFrame::Sunken );
    m_contextWidget->setMinimumSize( 100, 100 );
    PERF_LOG( "ContextWidget created" )

    PERF_LOG( "Creating ContexScene" )
    m_corona = new Context::ContextScene( this );
    connect( m_corona, SIGNAL( containmentAdded( Plasma::Containment* ) ),
            this, SLOT( createContextView( Plasma::Containment* ) ) );

    m_contextDock = new QDockWidget( i18n( "Context" ), this );
    m_contextDock->setObjectName( "Context dock" );
    m_contextDock->setWidget( m_contextWidget );
    m_contextDock->setAllowedAreas( Qt::AllDockWidgetAreas );
    m_contextDock->installEventFilter( this );
    PERF_LOG( "ContextScene created" )
    //END Creating Widgets

    createMenus();

    PERF_LOG( "Loading default contextScene" )
    m_corona->loadDefaultSetup(); // this method adds our containment to the scene
    PERF_LOG( "Loaded default contextScene" )

    setDockOptions ( QMainWindow::AllowNestedDocks | QMainWindow::AllowTabbedDocks | QMainWindow::AnimatedDocks );

    addDockWidget( Qt::LeftDockWidgetArea, m_browsersDock );
    addDockWidget( Qt::LeftDockWidgetArea, m_contextDock, Qt::Horizontal );
    addDockWidget( Qt::LeftDockWidgetArea, m_playlistDock, Qt::Horizontal );

    setLayoutLocked( AmarokConfig::lockLayout() );

    //<Browsers>
    {
        Debug::Block block( "Creating browsers. Please report long start times!" );

        PERF_LOG( "Creating CollectionWidget" )
        m_collectionBrowser = new CollectionWidget( "collections", 0 );
        m_collectionBrowser->setPrettyName( i18n( "Local Music" ) );
        m_collectionBrowser->setIcon( KIcon( "collection-amarok" ) );
        m_collectionBrowser->setShortDescription( i18n( "Local sources of content" ) );
        m_browsers->list()->addCategory( m_collectionBrowser );
        PERF_LOG( "Created CollectionWidget" )


        PERF_LOG( "Creating ServiceBrowser" )
        ServiceBrowser *internetContentServiceBrowser = ServiceBrowser::instance();
        internetContentServiceBrowser->setParent( 0 );
        internetContentServiceBrowser->setPrettyName( i18n( "Internet" ) );
        internetContentServiceBrowser->setIcon( KIcon( "applications-internet" ) );
        internetContentServiceBrowser->setShortDescription( i18n( "Online sources of content" ) );
        m_browsers->list()->addCategory( internetContentServiceBrowser );
        PERF_LOG( "Created ServiceBrowser" )

        PERF_LOG( "Creating PlaylistBrowser" )
        m_playlistBrowser = new PlaylistBrowserNS::PlaylistBrowser( "playlists", 0 );
        m_playlistBrowser->setPrettyName( i18n("Playlists") );
        m_playlistBrowser->setIcon( KIcon( "view-media-playlist-amarok" ) );
        m_playlistBrowser->setShortDescription( i18n( "Various types of playlists" ) );
        m_browsers->list()->addCategory( m_playlistBrowser );
        PERF_LOG( "CreatedPlaylsitBrowser" )


        PERF_LOG( "Creating FileBrowser" )
        FileBrowser * fileBrowserMkII = new FileBrowser( "files", 0 );
        fileBrowserMkII->setPrettyName( i18n("Files") );
        fileBrowserMkII->setIcon( KIcon( "folder-amarok" ) );
        fileBrowserMkII->setShortDescription( i18n( "Browse local hard drive for content" ) );
        m_browsers->list()->addCategory( fileBrowserMkII );


        PERF_LOG( "Created FileBrowser" )

        PERF_LOG( "Initialising ServicePluginManager" )
        ServicePluginManager::instance()->init();
        PERF_LOG( "Initialised ServicePluginManager" )

        internetContentServiceBrowser->setScriptableServiceManager( The::scriptableServiceManager() );
        PERF_LOG( "ScriptableServiceManager done" )

        PERF_LOG( "Creating Podcast Category" )
        m_browsers->list()->addCategory( The::podcastCategory() );
        PERF_LOG( "Created Podcast Category" )

        PERF_LOG( "finished MainWindow::init" )
    }

    The::amarokUrlHandler(); //Instantiate

    // Runtime check for Qt 4.6 here.
    // We delete the layout file once, because of binary incompatibility with older Qt version.
    // @see: https://bugs.kde.org/show_bug.cgi?id=213990
    const QChar major = qVersion()[0];
    const QChar minor = qVersion()[2];
    if( major.digitValue() >= 4 && minor.digitValue() > 5 )
    {
        KConfigGroup config = Amarok::config();
        if( !config.readEntry( "LayoutFileDeleted", false ) )
        {
            QFile::remove( Amarok::saveLocation() + "layout" );
            config.writeEntry( "LayoutFileDeleted", true );
            config.sync();
        }
    }

    // we must filter ourself to get mouseevents on the "splitter" - what is us, but filtered by the layouter
    installEventFilter( this );

    //restore the layout
    restoreLayout();
    saveLayout();
    // wait for the eventloop
    QTimer::singleShot( 0, this, SLOT( initLayoutHack() ) );
}

void
MainWindow::createContextView( Plasma::Containment *containment )
{
    DEBUG_BLOCK
    disconnect( m_corona, SIGNAL( containmentAdded( Plasma::Containment* ) ),
            this, SLOT( createContextView( Plasma::Containment* ) ) );
    PERF_LOG( "Creating ContexView" )
    m_contextView = new Context::ContextView( containment, m_corona, m_contextWidget );
    m_contextView->setFrameShape( QFrame::NoFrame );
    m_contextToolbarView = new Context::ToolbarView( containment, m_corona, m_contextWidget );
    m_contextToolbarView->setFrameShape( QFrame::NoFrame );

    connect( m_contextToolbarView, SIGNAL( hideAppletExplorer() ), m_contextView, SLOT( hideAppletExplorer() ) );
    connect( m_contextToolbarView, SIGNAL( showAppletExplorer() ), m_contextView, SLOT( showAppletExplorer() ) );
    m_contextView->showHome();
    PERF_LOG( "ContexView created" )

    bool hide = AmarokConfig::hideContextView();
    hideContextView( hide );
}

QMenu*
MainWindow::createPopupMenu()
{
    DEBUG_BLOCK
    QMenu* menu = new QMenu( this );
    menu->setTitle( i18nc("@item:inmenu", "&View" ) );

    // Layout locking:
    QAction* lockAction = new QAction( i18n( "Lock Layout" ), this );
    lockAction->setCheckable( true );
    lockAction->setChecked( AmarokConfig::lockLayout() );
    connect( lockAction, SIGNAL( toggled( bool ) ), SLOT( setLayoutLocked( bool ) ) );
    menu->addAction( lockAction );

    menu->addSeparator();

    // Dock widgets:
    QList<QDockWidget *> dockwidgets = qFindChildren<QDockWidget *>( this );

    foreach( QDockWidget* dockWidget, dockwidgets )
    {
        if( dockWidget->parentWidget() == this )
            menu->addAction( dockWidget->toggleViewAction());
    }

    menu->addSeparator();

    // Toolbars:
    QList<QToolBar *> toolbars = qFindChildren<QToolBar *>( this );
    QActionGroup* toolBarGroup = new QActionGroup( this );
    toolBarGroup->setExclusive( true );

    foreach( QToolBar* toolBar, toolbars )
    {
        if( toolBar->parentWidget() == this )
        {
            QAction* action = toolBar->toggleViewAction();
            connect( action, SIGNAL( toggled( bool ) ), toolBar, SLOT( setVisible( bool ) ) );
            toolBarGroup->addAction( action );
            menu->addAction( action );
        }
    }

    return menu;
}

void
MainWindow::showBrowser( const QString &name )
{
    const int index = m_browserNames.indexOf( name );
    showBrowser( index );
}

void
MainWindow::showBrowser( const int index )
{
    Q_UNUSED( index )
    //if( index >= 0 && index != m_browsers->currentIndex() )
    //    m_browsers->showWidget( index );
}

void
MainWindow::saveLayout()  //SLOT
{
    DEBUG_BLOCK

    //save layout to file. Does not go into to rc as it is binary data.
    QFile file( Amarok::saveLocation() + "layout" );

    if( file.open( QIODevice::WriteOnly | QIODevice::Truncate ) )
    {
        file.write( saveState( LAYOUT_VERSION ) );

        #ifdef Q_OS_UNIX  // fsync() only exists on Posix
        fsync( file.handle() );
        #endif

        file.close();
    }
}

void
MainWindow::keyPressEvent( QKeyEvent *e )
{
    if( !( e->modifiers() & Qt::ControlModifier ) )
        return KMainWindow::keyPressEvent( e );

    /*int n = -1;
    switch( e->key() )
    {
        case Qt::Key_0: n = 0; break;
        case Qt::Key_1: n = 1; break;
        case Qt::Key_2: n = 2; break;
        case Qt::Key_3: n = 3; break;
        case Qt::Key_4: n = 4; break;
        default:
            return KMainWindow::keyPressEvent( e );
    }
    if( n == 0 && m_browsers->currentIndex() >= 0 )
        m_browsers->showWidget( m_browsers->currentIndex() );
    else if( n > 0 )
        showBrowser( n - 1 ); // map from human to computer counting*/
}


void
MainWindow::closeEvent( QCloseEvent *e )
{
    DEBUG_BLOCK

#ifdef Q_WS_MAC
    Q_UNUSED( e );
    hide();
#else
    //KDE policy states we should hide to tray and not quit() when the
    //close window button is pushed for the main widget

    if( AmarokConfig::showTrayIcon() && e->spontaneous() && !kapp->sessionSaving() )
    {
        KMessageBox::information( this,
                i18n( "<qt>Closing the main window will keep Amarok running in the System Tray. "
                      "Use <B>Quit</B> from the menu, or the Amarok tray icon to exit the application.</qt>" ),
                i18n( "Docking in System Tray" ), "hideOnCloseInfo" );

        hide();
        e->ignore();
        return;
    }

    e->accept();
    kapp->quit();
#endif
}

void
MainWindow::exportPlaylist() const //SLOT
{
    DEBUG_BLOCK

    KFileDialog fileDialog( KUrl("kfiledialog:///amarok-playlist-export"), QString(), 0 );
    QCheckBox *saveRelativeCheck = new QCheckBox( i18n("Use relative path for &saving") );

    QStringList supportedMimeTypes;
    supportedMimeTypes << "audio/x-mpegurl"; //M3U
    supportedMimeTypes << "audio/x-scpls"; //PLS
    supportedMimeTypes << "application/xspf+xml"; //XSPF

    fileDialog.setMimeFilter( supportedMimeTypes, supportedMimeTypes.first() );
    fileDialog.fileWidget()->setCustomWidget( saveRelativeCheck );
    fileDialog.setOperationMode( KFileDialog::Saving );
    fileDialog.setMode( KFile::File );
    fileDialog.setCaption( i18n("Save As") );
    fileDialog.setObjectName( "PlaylistExport" );

    fileDialog.exec();

    QString playlistPath = fileDialog.selectedFile();

    if( !playlistPath.isEmpty() )
    {
        AmarokConfig::setRelativePlaylist( saveRelativeCheck->isChecked() );
        The::playlist()->exportPlaylist( playlistPath );
    }
}

void
MainWindow::slotShowActiveTrack() const
{
    m_playlistWidget->showActiveTrack();
}

void
MainWindow::slotShowCoverManager() const //SLOT
{
    CoverManager::showOnce();
}

void MainWindow::slotShowBookmarkManager() const
{
    The::bookmarkManager()->showOnce();
}

void MainWindow::slotShowEqualizer() const
{
    The::equalizer()->showOnce();
}

void
MainWindow::slotPlayMedia() //SLOT
{
    // Request location and immediately start playback
    slotAddLocation( true );
}

void
MainWindow::slotAddLocation( bool directPlay ) //SLOT
{
    // open a file selector to add media to the playlist
    KUrl::List files;
    KFileDialog dlg( KUrl(QDesktopServices::storageLocation(QDesktopServices::MusicLocation)), QString("*.*|"), this );
    dlg.setCaption( directPlay ? i18n("Play Media (Files or URLs)") : i18n("Add Media (Files or URLs)") );
    dlg.setMode( KFile::Files | KFile::Directory );
    dlg.setObjectName( "PlayMedia" );
    dlg.exec();
    files = dlg.selectedUrls();

    if( files.isEmpty() )
        return;

    The::playlistController()->insertOptioned( files, directPlay ? Playlist::AppendAndPlayImmediately : Playlist::AppendAndPlay );
}

void
MainWindow::slotAddStream() //SLOT
{
    bool ok;
    QString url = KInputDialog::getText( i18n("Add Stream"), i18n("Enter Stream URL:"), QString(), &ok, this );

    if( !ok )
        return;

    Meta::TrackPtr track = CollectionManager::instance()->trackForUrl( KUrl( url ) );

    The::playlistController()->insertOptioned( track, Playlist::Append );
}

void
MainWindow::slotJumpTo() // slot
{
    DEBUG_BLOCK

    m_playlistWidget->searchWidget()->focusInputLine();
}

void
MainWindow::showScriptSelector() //SLOT
{
    ScriptManager::instance()->show();
    ScriptManager::instance()->raise();
}

/**
 * "Toggle Main Window" global shortcut connects to this slot
 */
void
MainWindow::showHide() //SLOT
{
    const KWindowInfo info = KWindowSystem::windowInfo( winId(), 0, 0 );
    const int currentDesktop = KWindowSystem::currentDesktop();

    if( !isVisible() )
    {
        setVisible( true );
    }
    else
    {
        if( !isMinimized() )
        {
            if( !isActiveWindow() ) // not minimised and without focus
            {
                KWindowSystem::setOnDesktop( winId(), currentDesktop );
                KWindowSystem::activateWindow( winId() );
            }
            else // Amarok has focus
            {
                setVisible( false );
            }
        }
        else // Amarok is minimised
        {
            setWindowState( windowState() & ~Qt::WindowMinimized );
            KWindowSystem::setOnDesktop( winId(), currentDesktop );
            KWindowSystem::activateWindow( winId() );
        }
    }
}

void
MainWindow::showNotificationPopup() // slot
{
    if( Amarok::KNotificationBackend::instance()->isEnabled()
            && !Amarok::OSD::instance()->isEnabled() )
        Amarok::KNotificationBackend::instance()->showCurrentTrack();
    else
        Amarok::OSD::instance()->forceToggleOSD();
}

void
MainWindow::slotFullScreen() // slot
{
    setWindowState( windowState() ^ Qt::WindowFullScreen );
}

void
MainWindow::activate()
{
#ifdef Q_WS_X11
    const KWindowInfo info = KWindowSystem::windowInfo( winId(), 0, 0 );

    if( KWindowSystem::activeWindow() != winId())
        setVisible( true );
    else if( !info.isMinimized() )
        setVisible( true );
    if( !isHidden() )
        KWindowSystem::activateWindow( winId() );
#else
    setVisible( true );
#endif
}

bool
MainWindow::isReallyShown() const
{
#ifdef Q_WS_X11
    const KWindowInfo info = KWindowSystem::windowInfo( winId(), NET::WMDesktop, 0 );
    return !isHidden() && !info.isMinimized() && info.isOnDesktop( KWindowSystem::currentDesktop() );
#else
    return !isHidden();
#endif
}

void
MainWindow::createActions()
{
    KActionCollection* const ac = Amarok::actionCollection();
    const EngineController* const ec = The::engineController();
    const Playlist::Actions* const pa = The::playlistActions();
    const Playlist::Controller* const pc = The::playlistController();

    KStandardAction::keyBindings( kapp, SLOT( slotConfigShortcuts() ), ac );
    KStandardAction::preferences( kapp, SLOT( slotConfigAmarok() ), ac );
    ac->action(KStandardAction::name(KStandardAction::KeyBindings))->setIcon( KIcon( "configure-shortcuts-amarok" ) );
    ac->action(KStandardAction::name(KStandardAction::Preferences))->setIcon( KIcon( "configure-amarok" ) );
    ac->action(KStandardAction::name(KStandardAction::Preferences))->setMenuRole(QAction::PreferencesRole); // Define OS X Prefs menu here, removes need for ifdef later

    KStandardAction::quit( kapp, SLOT( quit() ), ac );

    KAction *action = new KAction( KIcon( "folder-amarok" ), i18n("&Add Media..."), this );
    ac->addAction( "playlist_add", action );
    connect( action, SIGNAL( triggered(bool) ), this, SLOT( slotAddLocation() ) );
    action->setGlobalShortcut( KShortcut( Qt::META + Qt::Key_A ) );

    action = new KAction( KIcon( "edit-clear-list-amarok" ), i18nc( "clear playlist", "&Clear Playlist" ), this );
    connect( action, SIGNAL( triggered( bool ) ), pc, SLOT( clear() ) );
    ac->addAction( "playlist_clear", action );

    action = new KAction( i18nc( "Remove duplicate and dead (unplayable) tracks from the playlist", "Re&move Duplicates" ), this );
    connect( action, SIGNAL( triggered( bool ) ), pc, SLOT( removeDeadAndDuplicates() ) );
    ac->addAction( "playlist_remove_dead_and_duplicates", action );

    action = new Playlist::LayoutConfigAction( this );
    ac->addAction( "playlist_layout", action );

    action = new KAction( KIcon( "folder-amarok" ), i18n("&Add Stream..."), this );
    connect( action, SIGNAL( triggered(bool) ), this, SLOT( slotAddStream() ) );
    ac->addAction( "stream_add", action );

    action = new KAction( KIcon( "document-export-amarok" ), i18n("&Export Playlist As..."), this );
    connect( action, SIGNAL( triggered(bool) ), this, SLOT( exportPlaylist() ) );
    ac->addAction( "playlist_export", action );

    action = new KAction( KIcon( "bookmark-new" ), i18n( "Bookmark Media Sources View" ), this );
    ac->addAction( "bookmark_browser", action );
    connect( action, SIGNAL( triggered() ), The::amarokUrlHandler(), SLOT( bookmarkCurrentBrowserView() ) );

    action = new KAction( KIcon( "bookmarks-organize" ), i18n( "Bookmark Manager" ), this );
    ac->addAction( "bookmark_manager", action );
    connect( action, SIGNAL( triggered(bool) ), SLOT( slotShowBookmarkManager() ) );

    action = new KAction( KIcon( "view-media-equalizer" ), i18n( "Equalizer" ), this );
    ac->addAction( "equalizer_dialog", action );
    connect( action, SIGNAL( triggered(bool) ), SLOT( slotShowEqualizer() ) );

    action = new KAction( KIcon( "bookmark-new" ), i18n( "Bookmark Playlist Setup" ), this );
    ac->addAction( "bookmark_playlistview", action );
    connect( action, SIGNAL( triggered() ), The::amarokUrlHandler(), SLOT( bookmarkCurrentPlaylistView() ) );

    action = new KAction( KIcon( "bookmark-new" ), i18n( "Bookmark Context Applets" ), this );
    ac->addAction( "bookmark_contextview", action );
    connect( action, SIGNAL( triggered() ), The::amarokUrlHandler(), SLOT( bookmarkCurrentContextView() ) );

    action = new KAction( KIcon( "media-album-cover-manager-amarok" ), i18n( "Cover Manager" ), this );
    connect( action, SIGNAL( triggered(bool) ), SLOT( slotShowCoverManager() ) );
    ac->addAction( "cover_manager", action );

    action = new KAction( KIcon("folder-amarok"), i18n("Play Media..."), this );
    ac->addAction( "playlist_playmedia", action );
    action->setShortcut( Qt::CTRL + Qt::Key_O );
    connect(action, SIGNAL(triggered(bool)), SLOT(slotPlayMedia()));

    action = new KAction( KIcon("preferences-plugin-script-amarok"), i18n("Script Manager"), this );
    connect(action, SIGNAL(triggered(bool)), SLOT(showScriptSelector()));
    ac->addAction( "script_manager", action );

    action = new KAction( KIcon( "media-seek-forward-amarok" ), i18n("&Seek Forward"), this );
    ac->addAction( "seek_forward", action );
    action->setShortcut( Qt::Key_Right );
    action->setGlobalShortcut( KShortcut( Qt::META + Qt::SHIFT + Qt::Key_Plus ) );
    connect(action, SIGNAL(triggered(bool)), ec, SLOT(seekForward()));

    action = new KAction( KIcon( "media-seek-backward-amarok" ), i18n("&Seek Backward"), this );
    ac->addAction( "seek_backward", action );
    action->setShortcut( Qt::Key_Left );
    action->setGlobalShortcut( KShortcut( Qt::META + Qt::SHIFT + Qt::Key_Minus ) );
    connect(action, SIGNAL(triggered(bool)), ec, SLOT(seekBackward()));

    PERF_LOG( "MainWindow::createActions 6" )
    action = new KAction( KIcon("collection-refresh-amarok"), i18n( "Update Collection" ), this );
    connect(action, SIGNAL(triggered(bool)), CollectionManager::instance(), SLOT(checkCollectionChanges()));
    ac->addAction( "update_collection", action );

    action = new KAction( this );
    ac->addAction( "prev", action );
    action->setIcon( KIcon("media-skip-backward-amarok") );
    action->setText( i18n( "Previous Track" ) );
    action->setGlobalShortcut( KShortcut( Qt::Key_MediaPrevious ) );
    connect( action, SIGNAL(triggered(bool)), pa, SLOT( back() ) );

    action = new KAction( this );
    ac->addAction( "repopulate", action );
    action->setText( i18n( "Repopulate Playlist" ) );
    action->setIcon( KIcon("view-refresh-amarok") );
    connect( action, SIGNAL(triggered(bool)), pa, SLOT( repopulateDynamicPlaylist() ) );

    action = new KAction( this );
    ac->addAction( "disable_dynamic", action );
    action->setText( QString() ); //TODO; Give a propper string if we want to use it for anything user visible
    action->setIcon( KIcon("edit-delete-amarok") );
    //this is connected inside the dynamic playlist category

    action = new KAction( KIcon("media-skip-forward-amarok"), i18n( "Next Track" ), this );
    ac->addAction( "next", action );
    action->setGlobalShortcut( KShortcut( Qt::Key_MediaNext ) );
    connect( action, SIGNAL(triggered(bool)), pa, SLOT( next() ) );

    action = new KAction( i18n( "Increase Volume" ), this );
    ac->addAction( "increaseVolume", action );
    action->setGlobalShortcut( KShortcut( Qt::META + Qt::Key_Plus ) );
    action->setShortcut( Qt::Key_Plus );
    connect( action, SIGNAL( triggered() ), ec, SLOT( increaseVolume() ) );

    action = new KAction( i18n( "Decrease Volume" ), this );
    ac->addAction( "decreaseVolume", action );
    action->setGlobalShortcut( KShortcut( Qt::META + Qt::Key_Minus ) );
    action->setShortcut( Qt::Key_Minus );
    connect( action, SIGNAL( triggered() ), ec, SLOT( decreaseVolume() ) );

    action = new KAction( i18n( "Toggle Main Window" ), this );
    ac->addAction( "toggleMainWindow", action );
    action->setGlobalShortcut( KShortcut( Qt::META + Qt::Key_P ) );
    connect( action, SIGNAL( triggered() ), SLOT( showHide() ) );

    action = new KAction( i18n( "Toggle Full Screen" ), this );
    ac->addAction( "toggleFullScreen", action );
    action->setShortcut( KShortcut( Qt::CTRL + Qt::SHIFT + Qt::Key_F ) );
    connect( action, SIGNAL( triggered() ), SLOT( slotFullScreen() ) );

    action = new KAction( i18n( "Jump to" ), this );
    ac->addAction( "jumpTo", action );
    action->setShortcut( KShortcut( Qt::CTRL + Qt::Key_J ) );
    connect( action, SIGNAL( triggered() ), SLOT( slotJumpTo() ) );

    action = new KAction( KIcon( "music-amarok" ), i18n("Show active track"), this );
    ac->addAction( "show_active_track", action );
    connect( action, SIGNAL( triggered( bool ) ), SLOT( slotShowActiveTrack() ) );

    action = new KAction( i18n( "Show Notification Popup" ), this );
    ac->addAction( "showNotificationPopup", action );
    action->setGlobalShortcut( KShortcut( Qt::META + Qt::Key_O ) );
    connect( action, SIGNAL( triggered() ), SLOT( showNotificationPopup() ) );

    action = new KAction( i18n( "Mute Volume" ), this );
    ac->addAction( "mute", action );
    action->setGlobalShortcut( KShortcut( Qt::META + Qt::Key_M ) );
    connect( action, SIGNAL( triggered() ), ec, SLOT( toggleMute() ) );

    action = new KAction( i18n( "Last.fm: Love Current Track" ), this );
    ac->addAction( "loveTrack", action );
    action->setGlobalShortcut( KShortcut( Qt::META + Qt::Key_L ) );
    connect( action, SIGNAL( triggered() ), SLOT( slotLoveTrack() ) );

    action = new KAction( i18n( "Last.fm: Ban Current Track" ), this );
    ac->addAction( "banTrack", action );
    //action->setGlobalShortcut( KShortcut( Qt::META + Qt::Key_B ) );
    connect( action, SIGNAL( triggered() ), SIGNAL( banTrack() ) );

    action = new KAction( i18n( "Last.fm: Skip Current Track" ), this );
    ac->addAction( "skipTrack", action );
    action->setGlobalShortcut( KShortcut( Qt::META + Qt::Key_S ) );
    connect( action, SIGNAL( triggered() ), SIGNAL( skipTrack() ) );

    action = new KAction( KIcon( "media-track-queue-amarok" ), i18n( "Queue Track" ), this );
    ac->addAction( "queueTrack", action );
    action->setShortcut( KShortcut( Qt::CTRL + Qt::Key_D ) );
    connect( action, SIGNAL( triggered() ), SIGNAL( switchQueueStateShortcut() ) );

    action = new KAction( i18n( "Rate Current Track: 1" ), this );
    ac->addAction( "rate1", action );
    action->setGlobalShortcut( KShortcut( Qt::META + Qt::Key_1 ) );
    connect( action, SIGNAL( triggered() ), SLOT( setRating1() ) );

    action = new KAction( i18n( "Rate Current Track: 2" ), this );
    ac->addAction( "rate2", action );
    action->setGlobalShortcut( KShortcut( Qt::META + Qt::Key_2 ) );
    connect( action, SIGNAL( triggered() ), SLOT( setRating2() ) );

    action = new KAction( i18n( "Rate Current Track: 3" ), this );
    ac->addAction( "rate3", action );
    action->setGlobalShortcut( KShortcut( Qt::META + Qt::Key_3 ) );
    connect( action, SIGNAL( triggered() ), SLOT( setRating3() ) );

    action = new KAction( i18n( "Rate Current Track: 4" ), this );
    ac->addAction( "rate4", action );
    action->setGlobalShortcut( KShortcut( Qt::META + Qt::Key_4 ) );
    connect( action, SIGNAL( triggered() ), SLOT( setRating4() ) );

    action = new KAction( i18n( "Rate Current Track: 5" ), this );
    ac->addAction( "rate5", action );
    action->setGlobalShortcut( KShortcut( Qt::META + Qt::Key_5 ) );
    connect( action, SIGNAL( triggered() ), SLOT( setRating5() ) );

    action = KStandardAction::redo(pc, SLOT(redo()), this);
    ac->addAction( "playlist_redo", action );
    action->setEnabled(false);
    action->setIcon( KIcon( "edit-redo-amarok" ) );
    connect(pc, SIGNAL(canRedoChanged(bool)), action, SLOT(setEnabled(bool)));

    action = KStandardAction::undo(pc, SLOT(undo()), this);
    ac->addAction( "playlist_undo", action );
    action->setEnabled(false);
    action->setIcon( KIcon( "edit-undo-amarok" ) );
    connect(pc, SIGNAL(canUndoChanged(bool)), action, SLOT(setEnabled(bool)));

    action = new KAction( KIcon( "amarok" ), i18n( "&About Amarok" ), this );
    ac->addAction( "extendedAbout", action );
    connect( action, SIGNAL( triggered() ), SLOT( showAbout() ) );

    action = new KAction( KIcon( "tools-report-bug" ), i18n("&Report Bug..."), this );
    ac->addAction( "reportBug", action );
    connect( action, SIGNAL( triggered() ), SLOT( showReportBug() ) );

    LikeBack *likeBack = new LikeBack( LikeBack::AllButBugs,
        LikeBack::isDevelopmentVersion( KGlobal::mainComponent().aboutData()->version() ) );
    likeBack->setServer( "likeback.kollide.net", "/send.php" );
    likeBack->setAcceptedLanguages( QStringList( "en" ) );
    likeBack->setWindowNamesListing( LikeBack::WarnUnnamedWindows );    //Notify if a window has no name

    KActionCollection *likeBackActions = new KActionCollection( this, KGlobal::mainComponent() );
    likeBackActions->addAssociatedWidget( this );
    likeBack->createActions( likeBackActions );

    ac->addAction( "likeBackSendComment", likeBackActions->action( "likeBackSendComment" ) );
    ac->addAction( "likeBackShowIcons", likeBackActions->action( "likeBackShowIcons" ) );

    PERF_LOG( "MainWindow::createActions 8" )
    new Amarok::MenuAction( ac, this );
    new Amarok::StopAction( ac, this );
    new Amarok::StopPlayingAfterCurrentTrackAction( ac, this );
    new Amarok::PlayPauseAction( ac, this );
    new Amarok::ReplayGainModeAction( ac, this );
    new Amarok::EqualizerAction( ac, this);

    ac->addAssociatedWidget( this );
    foreach (QAction* action, ac->actions())
        action->setShortcutContext(Qt::WindowShortcut);
}

void
MainWindow::setRating( int n )
{
    n *= 2;

    Meta::TrackPtr track = The::engineController()->currentTrack();
    if( track )
    {
        // if we're setting an identical rating then we really must
        // want to set the half-star below rating
        if( track->rating() == n )
            n -= 1;

        track->setRating( n );
        Amarok::OSD::instance()->OSDWidget::ratingChanged( track->rating() );
    }
}

void
MainWindow::createMenus()
{
    //BEGIN Actions menu
    KMenu *actionsMenu;
#ifdef Q_WS_MAC
    m_menubar = new QMenuBar(0);  // Fixes menubar in OS X
    actionsMenu = new KMenu( m_menubar );
    // Add these functions to the dock icon menu in OS X
    //extern void qt_mac_set_dock_menu(QMenu *);
    //qt_mac_set_dock_menu(actionsMenu);
    // Change to avoid duplicate menu titles in OS X
    actionsMenu->setTitle( i18n("&Music") );
#else
    m_menubar = menuBar();
    actionsMenu = new KMenu( m_menubar );
    actionsMenu->setTitle( i18n("&Amarok") );
#endif
    actionsMenu->addAction( Amarok::actionCollection()->action("playlist_playmedia") );
    actionsMenu->addSeparator();
    actionsMenu->addAction( Amarok::actionCollection()->action("prev") );
    actionsMenu->addAction( Amarok::actionCollection()->action("play_pause") );
    actionsMenu->addAction( Amarok::actionCollection()->action("stop") );
    actionsMenu->addAction( Amarok::actionCollection()->action("stop_after_current") );
    actionsMenu->addAction( Amarok::actionCollection()->action("next") );


#ifndef Q_WS_MAC    // Avoid duplicate "Quit" in OS X dock menu
    actionsMenu->addSeparator();
    actionsMenu->addAction( Amarok::actionCollection()->action(KStandardAction::name(KStandardAction::Quit)) );
#endif
    //END Actions menu

    //BEGIN Playlist menu
    KMenu *playlistMenu = new KMenu( m_menubar );
    playlistMenu->setTitle( i18n("&Playlist") );
    playlistMenu->addAction( Amarok::actionCollection()->action("playlist_add") );
    playlistMenu->addAction( Amarok::actionCollection()->action("stream_add") );
    //playlistMenu->addAction( Amarok::actionCollection()->action("playlist_save") ); //FIXME: See FIXME in PlaylistWidget.cpp
    playlistMenu->addAction( Amarok::actionCollection()->action( "playlist_export" ) );
    playlistMenu->addSeparator();
    playlistMenu->addAction( Amarok::actionCollection()->action("playlist_undo") );
    playlistMenu->addAction( Amarok::actionCollection()->action("playlist_redo") );
    playlistMenu->addSeparator();
    playlistMenu->addAction( Amarok::actionCollection()->action("playlist_clear") );
    playlistMenu->addAction( Amarok::actionCollection()->action("playlist_remove_dead_and_duplicates") );
    playlistMenu->addAction( Amarok::actionCollection()->action("playlist_layout") );
    //END Playlist menu

    //BEGIN Tools menu
    m_toolsMenu = new KMenu( m_menubar );
    m_toolsMenu->setTitle( i18n("&Tools") );

    m_toolsMenu->addAction( Amarok::actionCollection()->action("bookmark_manager") );
    m_toolsMenu->addAction( Amarok::actionCollection()->action("cover_manager") );
    m_toolsMenu->addAction( Amarok::actionCollection()->action("equalizer_dialog") );
    m_toolsMenu->addAction( Amarok::actionCollection()->action("script_manager") );
    m_toolsMenu->addSeparator();
    m_toolsMenu->addAction( Amarok::actionCollection()->action("update_collection") );
    //END Tools menu

    //BEGIN Settings menu
    m_settingsMenu = new KMenu( m_menubar );
    m_settingsMenu->setTitle( i18n("&Settings") );

    //TODO use KStandardAction or KXmlGuiWindow

    // the phonon-coreaudio  backend has major issues with either the VolumeFaderEffect itself
    // or with it in the pipeline. track playback stops every ~3-4 tracks, and on tracks >5min it
    // stops at about 5:40. while we get this resolved upstream, don't make playing amarok such on osx.
    // so we disable replaygain on osx

#ifndef Q_WS_MAC
    m_settingsMenu->addAction( Amarok::actionCollection()->action("replay_gain_mode") );
    m_settingsMenu->addSeparator();
#endif

    m_settingsMenu->addAction( Amarok::actionCollection()->action(KStandardAction::name(KStandardAction::KeyBindings)) );
    m_settingsMenu->addAction( Amarok::actionCollection()->action(KStandardAction::name(KStandardAction::Preferences)) );
    //END Settings menu

    m_menubar->addMenu( actionsMenu );
    m_menubar->addMenu( createPopupMenu() );
    m_menubar->addMenu( playlistMenu );
    m_menubar->addMenu( m_toolsMenu );
    m_menubar->addMenu( m_settingsMenu );

    KMenu *helpMenu = Amarok::Menu::helpMenu();
    helpMenu->insertAction( helpMenu->actions().first(),
                            Amarok::actionCollection()->action( "reportBug" ) );
    helpMenu->insertAction( helpMenu->actions().last(),
                            Amarok::actionCollection()->action( "extendedAbout" ) );
    helpMenu->insertAction( helpMenu->actions().at(4),
                            Amarok::actionCollection()->action( "likeBackSendComment" ) );
    helpMenu->insertAction( helpMenu->actions().at(5),
                            Amarok::actionCollection()->action( "likeBackShowIcons" ) );

    m_menubar->addMenu( helpMenu );
}

void
MainWindow::showAbout()
{
    ExtendedAboutDialog dialog( &aboutData, &ocsData );
    dialog.exec();
}

void
MainWindow::showReportBug()
{
    KBugReport * rbDialog = new KBugReport( this, true, KGlobal::mainComponent().aboutData() );
    rbDialog->setObjectName( "KBugReport" );
    rbDialog->exec();
}

void
MainWindow::paletteChange(const QPalette & oldPalette)
{
    Q_UNUSED( oldPalette )

    KPixmapCache cache( "Amarok-pixmaps" );
    cache.discard();
    The::paletteHandler()->setPalette( palette() );
}

QSize
MainWindow::backgroundSize()
{
    const QPoint topLeft = mapToGlobal( QPoint( 0, 0 ) );
    const QPoint bottomRight1 = mapToGlobal( QPoint( width(), height() ) );

    return QSize( bottomRight1.x() - topLeft.x() + 1, bottomRight1.y() - topLeft.y() );
}

//BEGIN DOCK LAYOUT FIXING HACK ====================================================================

/**
    Docks that are either hidden, floating or tabbed shall not be taken into account for ratio calculations
    unfortunately docks that are hiddeen in tabs are not "hidden", but just have an invalid geometry
*/
bool
MainWindow::LH_isIrrelevant( const QDockWidget *dock )
{
    bool ret = !dock->isVisibleTo( this ) || dock->isFloating();
    if( !ret )
    {
        const QRect r = dock->geometry();
        ret = r.right() < 1 || r.bottom() < 1;
    }
    return  ret;
}

/**
    We've atm three dock, ie. 0 or 1 tabbar. It's also the only bar as direct child.
    We need to dig it as it constructed and deleted on UI changes by the user
*/
QTabBar *
MainWindow::LH_dockingTabbar()
{
    QObjectList kids = children();
    foreach ( QObject *kid, kids )
    {
        if( kid->isWidgetType() )
            if( QTabBar *bar = qobject_cast<QTabBar*>(kid) )
                return bar;
    }
    return 0;
}

/**
    this function calculates the area covered by the docks - i.e. rect() minus menubar and toolbar
*/
void
MainWindow::LH_extend( QRect &target, const QDockWidget *dock )
{
    if( LH_isIrrelevant( dock ) )
        return;
    if( target.isNull() )
    {
        target = dock->geometry();
        return;
    }
    const QRect source = dock->geometry();
    if( source.left() < target.left() ) target.setLeft( source.left() );
    if( source.right() > target.right() ) target.setRight( source.right() );
    if( source.top() < target.top() ) target.setTop( source.top() );
    if( source.bottom() > target.bottom() ) target.setBottom( source.bottom() );
}

/**
    which size the dock should have in our opinion, based upon the ratios and the docking area.
    takes size constraints into account
*/
QSize
MainWindow::LH_desiredSize( QDockWidget *dock, const QRect &area, float rx, float ry, int splitter )
{
    if( LH_isIrrelevant( dock ) )
        return QSize(0,0);
    QSize ret;
    int tabHeight = 0;
    if( !tabifiedDockWidgets( dock ).isEmpty() )
    {
        if( QTabBar *bar = LH_dockingTabbar() )
            tabHeight = bar->height();
    }
    const QSize min = dock->minimumSize();
    ret.setWidth( qMax( min.width(), (int)(rx == 1.0 ? area.width() : rx*area.width() - splitter/2) ) );
    ret.setHeight( qMax( min.height(), (int)(ry == 1.0 ? area.height() - tabHeight : ry*area.height() - ( splitter/2 + tabHeight ) ) ) );
    return ret;
}
#define DESIRED_SIZE(_DOCK_) LH_desiredSize( _DOCK_##Dock, m_dockingRect, _DOCK_##Ratio.x, _DOCK_##Ratio.y, splitterSize )


/**
    used to check whether the current dock size "more or less" matches our opinion
    if one of the sizes isNull() it's invalid and the current size is ok.
*/
bool
MainWindow::LH_fuzzyMatch( const QSize &sz1, const QSize &sz2 )
{
    return sz1.isNull() || sz2.isNull() ||
           ( sz1.width() < sz2.width() + 6 && sz1.width() > sz2.width() - 6 &&
             sz1.height() < sz2.height() + 6 && sz1.height() > sz2.height() - 6 );
}

/**
    whether the dock has reached it's minimum width OR height
*/
bool
MainWindow::LH_isConstrained( const QDockWidget *dock )
{
    if( LH_isIrrelevant( dock ) )
        return false;
    return dock->minimumWidth() == dock->width() || dock->minimumHeight() == dock->height();
}

#define FREE_SIZE(_DOCK_,_INDEX_)   if( !_DOCK_->isFloating() ) { \
                                        _DOCK_->setMinimumSize( mins[_INDEX_] );\
                                        _DOCK_->setMaximumSize( maxs[_INDEX_] ); } //

void
MainWindow::initLayoutHack()
{
    // the init _should_ be superflous, but hey: why risk it ;-)
    m_playlistRatio.x = 0.0; m_playlistRatio.y = 0.0;
    m_browsersRatio = m_contextRatio = m_playlistRatio;

    m_dockingRect = QRect();
    LH_extend( m_dockingRect, m_browsersDock );
    LH_extend( m_dockingRect, m_contextDock );
    LH_extend( m_dockingRect, m_playlistDock );

    // initially calculate ratios
    updateDockRatio( m_browsersDock );
    updateDockRatio( m_contextDock );
    updateDockRatio( m_playlistDock );

    m_LH_initialized = true;

    // connect to dock changes (esp. tab selection)
    // this _is_ required...
    connect ( m_browsersDock, SIGNAL( visibilityChanged(bool) ), this, SLOT( updateDockRatio() ) );
    connect ( m_contextDock, SIGNAL( visibilityChanged(bool) ), this, SLOT( updateDockRatio() ) );
    connect ( m_playlistDock, SIGNAL( visibilityChanged(bool) ), this, SLOT( updateDockRatio() ) );

    // but i think these can be omitted (move along a show event for tech. reasons)
//     topLevelChanged(bool)
//     dockLocationChanged(Qt::DockWidgetArea)

    slotShowActiveTrack();    // See comment in 'playlist/view/PrettyListView.cpp constructor'.
}

void
MainWindow::resizeEvent( QResizeEvent * event )
{
    DEBUG_BLOCK
    // NOTICE: uncomment this to test default behaviour
//     QMainWindow::resizeEvent( event );
//     return;

    // reset timer for saving changes
    // 30 secs - this is no more crucial and we don't have to store every sh** ;-)
    m_saveLayoutChangesTimer->start( 30000 );

    // prevent flicker, NOTICE to prevent all flicker, this must be done by an (bug prone) passing-on eventfiler :-(
    setUpdatesEnabled( false );

    // stop listening to resize events, we're gonna trigger them now
    m_browsersDock->removeEventFilter( this );
    m_contextDock->removeEventFilter( this );
    m_playlistDock->removeEventFilter( this );

    QMainWindow::resizeEvent( event );

    // ok, the window was resized and our little docklings fill the entire docking area
    // -> m_dockingRect is their bounding rect
    m_dockingRect = QRect();
    LH_extend( m_dockingRect, m_browsersDock );
    LH_extend( m_dockingRect, m_contextDock );
    LH_extend( m_dockingRect, m_playlistDock );

    // if we hit a minimumSize constraint, we can no more keep aspects at all atm. and just hope
    // that Qt distributed the lacking space evenly ;-)
    if( !m_LH_initialized ||
        LH_isConstrained( m_contextDock ) || LH_isConstrained( m_browsersDock ) || LH_isConstrained( m_playlistDock ) )
    {
        setUpdatesEnabled( true );

        // continue to listen to resize events
        m_browsersDock->installEventFilter( this );
        m_contextDock->installEventFilter( this );
        m_playlistDock->installEventFilter( this );

        return;
    }

    int splitterSize = style()->pixelMetric( QStyle::PM_DockWidgetSeparatorExtent, 0, 0 );
    const QSize dContextSz = DESIRED_SIZE(m_context);
    const QSize dBrowsersSz = DESIRED_SIZE(m_browsers);
    const QSize dPlaylistSz = DESIRED_SIZE(m_playlist);

    // good god: prevent recursive resizes ;-)
    setFixedSize(size());

    layout()->setEnabled( false );
    QCoreApplication::sendPostedEvents( this, QEvent::LayoutRequest );
    layout()->invalidate();
    layout()->setEnabled( true );

    // don't be super picky - there's an integer imprecision anyway and we just want to fix
    // major resize errors w/o being to intrusive or flickering
    if( !( LH_fuzzyMatch( dContextSz, m_contextDock->size() ) &&
           LH_fuzzyMatch( dBrowsersSz, m_browsersDock->size() ) &&
           LH_fuzzyMatch( dPlaylistSz, m_playlistDock->size() ) ) )
    {
//         debug() << "SIZE MISMATCH, FORCING";
        const QSize mins[3] = { m_contextDock->minimumSize(), m_browsersDock->minimumSize(), m_playlistDock->minimumSize() };
        const QSize maxs[3] = { m_contextDock->maximumSize(), m_browsersDock->maximumSize(), m_playlistDock->maximumSize() };

        // fix sizes to our idea, so the layout has few options left ;-)
        if( !m_contextDock->isFloating() ) m_contextDock->setFixedSize( dContextSz );
        if( !m_browsersDock->isFloating() ) m_browsersDock->setFixedSize( dBrowsersSz );
        if( !m_playlistDock->isFloating() ) m_playlistDock->setFixedSize( dPlaylistSz );

        // trigger a no-choice layouting
        layout()->activate();
        QCoreApplication::sendPostedEvents( this, QEvent::LayoutRequest );

        // unleash sizes
        layout()->setEnabled( false );
        FREE_SIZE( m_contextDock, 0 );
        FREE_SIZE( m_browsersDock, 1 );
        FREE_SIZE( m_playlistDock, 2 );
        layout()->setEnabled( true );
    }

    // unleash size
    setMinimumSize(0,0);
    setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);

    // continue to listen to resize events
    m_browsersDock->installEventFilter( this );
    m_contextDock->installEventFilter( this );
    m_playlistDock->installEventFilter( this );

    // update on screen
    setUpdatesEnabled( true );
}

#undef DESIRED_SIZE
#undef FREE_SIZE

// i hate these slots - but we get no show/hide events on tab selection :-(
void MainWindow::updateDockRatio( QDockWidget *dock )
{
    if( !LH_isIrrelevant( dock ) )
    {
        QRect area = m_dockingRect;
        int tabHeight = 0;
        if( !tabifiedDockWidgets( dock ).isEmpty() )
        {
            if( QTabBar *bar = LH_dockingTabbar() )
                tabHeight = bar->height();
        }
        int splitterSize = style()->pixelMetric( QStyle::PM_DockWidgetSeparatorExtent, 0, dock );

        Ratio r;
        if( dock->width() == area.width() )
            r.x = 1.0;
        else
            r.x = ( (float)(dock->width() + splitterSize/2) ) / area.width();
        if( dock->height() == area.height() - tabHeight )
            r.y = 1.0;
        else
            r.y = ( (float)(dock->height() + splitterSize/2 + tabHeight ) ) / area.height();

        if( dock == m_browsersDock )
            m_browsersRatio = r;
        else if( dock == m_contextDock )
            m_contextRatio = r;
        else
            m_playlistRatio = r;
//         debug() << "==>" << r.x << r.y;
    }
    // UI changed -> trigger delayed storage
    m_saveLayoutChangesTimer->start( 30000 );
}

// this slot is connected to the visibilityChange event, fired on show/hide (in most cases... *sigh*) and tab changes
void MainWindow::updateDockRatio()
{
    if( QDockWidget *dock = qobject_cast<QDockWidget*>( sender() ) )
        updateDockRatio( dock );
}

bool MainWindow::eventFilter(QObject *o, QEvent *e)
{
    // NOTICE this _must_ be handled by an eventfilter, as otherwise the "spliters" eventfilter
    // will eat and we don't receive it
    if( o == this )
    {
        if( e->type() == QEvent::MouseButtonPress || e->type() == QEvent::MouseButtonRelease )
        {
            QMouseEvent *me = static_cast<QMouseEvent*>(e);
            if( me->button() == Qt::LeftButton )
                m_mouseDown = ( e->type() == QEvent::MouseButtonPress );
        }
        return false;
    }

    if( ( ( e->type() == QEvent::Resize && m_mouseDown ) || // only when resized by the splitter :)
             e->type() == QEvent::Show || e->type() == QEvent::Hide ) && // show/hide is _NOT_ sufficient for tab changes
        ( o == m_browsersDock || o == m_contextDock || o == m_playlistDock ) )
    {
        QDockWidget *dock = static_cast<QDockWidget*>(o);
//         if(e->type() == QEvent::Resize)
//             debug() << dock << dock->size() << m_dockingRect.size();
//         else
//             debug() << "other!" << dock << dock->size() << m_dockingRect.size();
        updateDockRatio( dock );
    }
    return false;
}

//END DOCK LAYOUT FIXING HACK ======================================================================

QPoint
MainWindow::globalBackgroundOffset()
{
    return menuBar()->mapToGlobal( QPoint( 0, 0 ) );
}

QRect
MainWindow::contextRectGlobal()
{
    //debug() << "pos of context vidget within main window is: " << m_contextWidget->pos();
    QPoint contextPos = mapToGlobal( m_contextWidget->pos() );
    return QRect( contextPos.x(), contextPos.y(), m_contextWidget->width(), m_contextWidget->height() );
}

void
MainWindow::engineStateChanged( Phonon::State state, Phonon::State oldState )
{
    Q_UNUSED( oldState )

    Meta::TrackPtr track = The::engineController()->currentTrack();

    switch( state )
    {
    case Phonon::StoppedState:
        m_currentTrack = 0;
        setPlainCaption( i18n( AMAROK_CAPTION ) );
        break;

    case Phonon::PlayingState:
        unsubscribeFrom( m_currentTrack );
        m_currentTrack = track;
        subscribeTo( track );
        metadataChanged( track );
        break;

    case Phonon::PausedState:
        setPlainCaption( i18n( "Paused  ::  %1", QString( AMAROK_CAPTION ) ) );
        break;

    default:
        break;
    }
}

void
MainWindow::engineNewTrackPlaying()
{
    m_currentTrack = The::engineController()->currentTrack();
    metadataChanged( m_currentTrack );
}

void
MainWindow::metadataChanged( Meta::TrackPtr track )
{
    if( track )
        setPlainCaption( i18n( "%1 - %2  ::  %3", track->artist() ? track->artist()->prettyName() : i18n( "Unknown" ), track->prettyName(), AMAROK_CAPTION ) );
}

CollectionWidget *
MainWindow::collectionBrowser()
{
    return m_collectionBrowser;
}

QString
MainWindow::activeBrowserName()
{
    if( m_browsers->list()->activeCategory() )
        return m_browsers->list()->activeCategory()->name();
    else
        return QString();
}

PlaylistBrowserNS::PlaylistBrowser *
MainWindow::playlistBrowser()
{
    return m_playlistBrowser;
}

void
MainWindow::hideContextView( bool hide )
{
    DEBUG_BLOCK

    if( hide )
        m_contextWidget->hide();
    else
        m_contextWidget->show();
}

void MainWindow::setLayoutLocked( bool locked )
{
    DEBUG_BLOCK

    if( locked )
    {
        debug() << "locked!";
        const QFlags<QDockWidget::DockWidgetFeature> features = QDockWidget::NoDockWidgetFeatures;

        m_browsersDock->setFeatures( features );
        m_browsersDock->setTitleBarWidget( m_browserDummyTitleBarWidget );

        m_contextDock->setFeatures( features );
        m_contextDock->setTitleBarWidget( m_contextDummyTitleBarWidget );

        m_playlistDock->setFeatures( features );
        m_playlistDock->setTitleBarWidget( m_playlistDummyTitleBarWidget );

        m_slimToolbar->setFloatable( false );
        m_slimToolbar->setMovable( false );

        m_mainToolbar->setFloatable( false );
        m_mainToolbar->setMovable( false );
    }
    else
    {
        debug() << "unlocked!";
        const QFlags<QDockWidget::DockWidgetFeature> features = QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable | QDockWidget::DockWidgetClosable;

        m_browsersDock->setFeatures( features );
        m_contextDock->setFeatures( features );
        m_playlistDock->setFeatures( features );


        m_browsersDock->setTitleBarWidget( 0 );
        m_contextDock->setTitleBarWidget( 0 );
        m_playlistDock->setTitleBarWidget( 0 );

        m_slimToolbar->setFloatable( true );
        m_slimToolbar->setMovable( true );

        m_mainToolbar->setFloatable( true );
        m_mainToolbar->setMovable( true );
    }

    AmarokConfig::setLockLayout( locked );
    AmarokConfig::self()->writeConfig();
    m_layoutLocked = locked;
}

bool
MainWindow::isLayoutLocked()
{
    return m_layoutLocked;
}

void
MainWindow::restoreLayout()
{
    DEBUG_BLOCK

    QFile file( Amarok::saveLocation() + "layout" );
    QByteArray layout;
    if( file.open( QIODevice::ReadOnly ) )
    {
        layout = file.readAll();
        file.close();
    }

    if( !restoreState( layout, LAYOUT_VERSION ) )
    {
        //since no layout has been loaded, we know that the items are all placed next to each other in the main window
        //so get the combined size of the widgets, as this is the space we have to play with. Then figure out
        //how much to give to each. Give the context view any pixels leftover from the integer division.

        //int totalWidgetWidth = m_browsers->width() + m_contextView->width() + m_playlistWidget->width();
        int totalWidgetWidth = contentsRect().width();

        //get the width of the splitter handles, we need to subtract these...
        const int splitterHandleWidth = style()->pixelMetric( QStyle::PM_DockWidgetSeparatorExtent, 0, 0 );
        debug() << "splitter handle widths " << splitterHandleWidth;

        totalWidgetWidth -= ( splitterHandleWidth * 2 );

        debug() << "mainwindow width" <<  contentsRect().width();
        debug() << "totalWidgetWidth" <<  totalWidgetWidth;

        const int widgetWidth = totalWidgetWidth / 3;
        const int leftover = totalWidgetWidth - 3*widgetWidth;

        //We need to set fixed widths initially, just until the main window has been properly layed out. As soon as this has
        //happened, we will unlock these sizes again so that the elements can be resized by the user.
        const int mins[3] = { m_browsersDock->minimumWidth(), m_contextDock->minimumWidth(), m_playlistDock->minimumWidth() };
        const int maxs[3] = { m_browsersDock->maximumWidth(), m_contextDock->maximumWidth(), m_playlistDock->maximumWidth() };

        m_browsersDock->setFixedWidth( widgetWidth );
        m_contextDock->setFixedWidth( widgetWidth + leftover );
        m_playlistDock->setFixedWidth( widgetWidth );
        this->layout()->activate();

        m_browsersDock->setMinimumWidth( mins[0] ); m_browsersDock->setMaximumWidth( maxs[0] );
        m_contextDock->setMinimumWidth( mins[1] ); m_contextDock->setMaximumWidth( maxs[1] );
        m_playlistDock->setMinimumWidth( mins[2] ); m_playlistDock->setMaximumWidth( maxs[2] );
    }

    // Ensure that only one toolbar is visible
    if( !m_mainToolbar->isHidden() && !m_slimToolbar->isHidden() )
        m_slimToolbar->hide();

    // Ensure that we don't end up without any toolbar (can happen after upgrading)
    if( m_mainToolbar->isHidden() && m_slimToolbar->isHidden() )
        m_mainToolbar->show();
}


bool MainWindow::playAudioCd()
{
    DEBUG_BLOCK
    //drop whatever we are doing and play auidocd

    QList<Collections::Collection*> collections = CollectionManager::instance()->viewableCollections();

    foreach( Collections::Collection *collection, collections )
    {
        if( collection->collectionId() == "AudioCd" )
        {

            debug() << "got audiocd collection";

            Collections::MemoryCollection * cdColl = dynamic_cast<Collections::MemoryCollection *>( collection );

            if( !cdColl || cdColl->trackMap().count() == 0 )
            {
                debug() << "cd collection not ready yet (track count = 0 )";
                m_waitingForCd = true;
                return false;
            }

            The::engineController()->stop( true );
            The::playlistController()->clear();

            Collections::QueryMaker * qm = collection->queryMaker();
            qm->setQueryType( Collections::QueryMaker::Track );
            The::playlistController()->insertOptioned( qm, Playlist::DirectPlay );

            m_waitingForCd = false;
            return true;
        }
    }

    debug() << "waiting for cd...";
    m_waitingForCd = true;
    return false;
}

bool MainWindow::isWaitingForCd()
{
    DEBUG_BLOCK
    debug() << "waiting?: " << m_waitingForCd;
    return m_waitingForCd;
}

#include "MainWindow.moc"
