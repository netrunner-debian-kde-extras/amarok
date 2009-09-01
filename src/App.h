/****************************************************************************************
 * Copyright (c) 2002 Mark Kretschmann <kretschmann@kde.org>                            *
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

#ifndef AMAROK_APP_H
#define AMAROK_APP_H

#include <config-amarok.h>

#include "aboutdialog/OcsData.h"
#include "amarok_export.h"
#include "MainWindow.h"

#include <KAboutData>
#include <KUniqueApplication>   //baseclass
#include <KUrl>

#include <QHash>
#include <QPointer>
#include <QString>

namespace Amarok {
    class TrayIcon;
}

class OcsData;

namespace KIO { class Job; }

class KJob;
class MediaDeviceManager;
class KSplashScreen;

class AMAROK_EXPORT App : public KUniqueApplication
{
    Q_OBJECT

    public:
        App();
       ~App();

        static App *instance() { return static_cast<App*>( kapp ); }

        void setUniqueInstance( bool isUnique ) { m_isUniqueInstance = isUnique; }
        bool isNonUniqueInstance() const { return m_isUniqueInstance; }

        Amarok::TrayIcon* trayIcon() { return m_tray; }
        static void handleCliArgs();
        static void initCliArgs( int argc, char *argv[] );
        static void initCliArgs();

        static int mainThreadId;

        virtual int newInstance();

        inline MainWindow *mainWindow() const { return m_mainWindow; }

        // FRIENDS ------
        friend class MainWindow; //requires access to applySettings()

    signals:
        void prepareToQuit();

    private slots:
        void continueInit();

    public slots:
        void applySettings( bool firstTime = false );
#ifdef DEBUG
        static void runUnitTests();
#endif // DEBUG
        void slotConfigAmarok( const QString& page = QString() );
        void slotConfigShortcuts();
        void slotConfigEqualizer();
        KIO::Job *trashFiles( const KUrl::List &files );
        void quit();

    protected:
        bool event( QEvent *event );

    private slots:
        void slotTrashResult( KJob *job );

    private:
        // ATTRIBUTES ------
        bool                 m_isUniqueInstance;
        QPointer<MainWindow> m_mainWindow;
        Amarok::TrayIcon     *m_tray;
        MediaDeviceManager   *m_mediaDeviceManager;
        KSplashScreen        *m_splash;
};

#define pApp static_cast<App*>(kapp)


#endif  // AMAROK_APP_H
