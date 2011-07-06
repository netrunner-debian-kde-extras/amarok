/****************************************************************************************
 * Copyright (c) 2002-2009 Mark Kretschmann <kretschmann@kde.org>                       *
 * Copyright (c) 2002 Max Howell <max.howell@methylblue.com>                            *
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

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <config-amarok.h>

#include "amarok_export.h"
#include "core/meta/Meta.h"
#include "browsers/BrowserDock.h"

#include <KMainWindow>
#include <KVBox>
#include <Phonon/Global>
#include <Phonon/Global>

#include <QWeakPointer>

class CollectionWidget;
class SlimToolbar;
class MainToolbar;
class MainWindow;
#ifdef DEBUG_BUILD_TYPE
class NetworkAccessViewer;
#endif // DEBUG_BUILD_TYPE
class PlaylistFileProvider;

namespace PlaylistBrowserNS { class PlaylistBrowser; }
namespace Playlist { class Dock; }
class ContextDock;


class KMenu;
class KAction;
class QMenuBar;
class QSplitter;
class QTimer;

namespace The {
    AMAROK_EXPORT MainWindow* mainWindow();
}

//This should only change if docks or toolbars are added or removed
#define LAYOUT_VERSION 3

/**
  * @class MainWindow
  * @short The MainWindow widget class.
  *
  * This is the main window widget.
  */
class AMAROK_EXPORT MainWindow : public KMainWindow
{
    friend MainWindow* The::mainWindow();

    Q_OBJECT

    public:
        enum AmarokDockId {
            AmarokDockNavigation,
            AmarokDockContext,
            AmarokDockPlaylist
        };
        
        MainWindow();
        ~MainWindow();

        void activate();

        //allows us to switch browsers from within other browsers etc
        void showBrowser( const QString& name );

        //ensures the dock widget is visible in case it is tabbed
        void showDock( AmarokDockId dockId );

        BrowserDock *browserDock() const { return m_browserDock.data(); }
        KMenu *ToolsMenu() const { return m_toolsMenu.data(); }
        KMenu *SettingsMenu() const { return m_settingsMenu.data(); }
        Playlist::Dock *playlistDock() const { return m_playlistDock.data(); }
        void deleteBrowsers();

        /* Reimplemented from QMainWindow to allow only one active toolbar at any time */
        virtual QMenu* createPopupMenu();

        void addViewMenuItems(QMenu* menu);

        QString activeBrowserName();

        CollectionWidget * collectionBrowser();

        bool isLayoutLocked() const;

        /**
        *    If an audiocd collection is present. Stop current playback, clear playlist,
        *    add cd to playlist and start playback
        */
        bool playAudioCd();

        bool isWaitingForCd() const;

    signals:
        void loveTrack( Meta::TrackPtr track );
        void banTrack();
        void skipTrack();
        void switchQueueStateShortcut();

    public slots:
        void showHide();
        void slotFullScreen();
        void slotLoveTrack();
        void showNotificationPopup();
        void setLayoutLocked( bool locked );
        void showAbout();
        void showReportBug();

    private slots:
        void slotStopped();
        void slotPaused();
        void slotNewTrackPlaying();
        void slotMetadataChanged( Meta::TrackPtr track );

        void exportPlaylist() const;
        void slotShowActiveTrack() const;
        void slotShowBookmarkManager() const;
        void slotShowEqualizer() const;
        void slotShowCoverManager() const;
        void slotShowMenuBar();
        void slotPlayMedia();
        void slotAddLocation( bool directPlay = false );
        void slotAddStream();
        void slotJumpTo();
#ifdef DEBUG_BUILD_TYPE
        void showNetworkRequestViewer();
#endif // DEBUG_BUILD_TYPE

        /**
         * Save state and position of dock widgets.
         */
        void saveLayout();

        /**
         * Try to restore saved layout, if this fails, try to use the default layout.
         */
        void restoreLayout();

    protected:
        virtual void closeEvent( QCloseEvent* );
        virtual void paletteChange( const QPalette & oldPalette );
        virtual bool queryExit(); 

    private slots:
        void setRating1() { setRating( 1 ); }
        void setRating2() { setRating( 2 ); }
        void setRating3() { setRating( 3 ); }
        void setRating4() { setRating( 4 ); }
        void setRating5() { setRating( 5 ); }

    private:
        void init();
        void setRating( int n );

        CollectionWidget * m_collectionBrowser;
        PlaylistBrowserNS::PlaylistBrowser * m_playlistBrowser;

        QWeakPointer<QMenuBar>  m_menubar;
        QWeakPointer<KMenu>     m_toolsMenu;
        QWeakPointer<KMenu>     m_settingsMenu;
        QWeakPointer<BrowserDock> m_browserDock;
#ifdef DEBUG_BUILD_TYPE
        QWeakPointer<NetworkAccessViewer> m_networkViewer;
#endif // DEBUG_BUILD_TYPE

        QByteArray m_splitterState;

        QWeakPointer<ContextDock> m_contextDock;
        QWeakPointer<Playlist::Dock> m_playlistDock;

        QWeakPointer<SlimToolbar> m_slimToolbar;
        QWeakPointer<MainToolbar> m_mainToolbar;

        void createActions();
        void createMenus();

        KAction* m_showMenuBar;

        int m_lastBrowser;
        int m_searchField;

        static QWeakPointer<MainWindow> s_instance;

        bool m_layoutLocked;
        bool m_waitingForCd;
};


#endif //AMAROK_PLAYLISTWINDOW_H

