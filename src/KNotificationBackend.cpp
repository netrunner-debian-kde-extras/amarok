/****************************************************************************************
 * Copyright (c) 2009-2011 Kevin Funk <krf@electrostorm.net>                            *
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

#include "KNotificationBackend.h"

#include "core/support/Amarok.h"
#include "core/meta/Meta.h"
#include "SvgHandler.h"
#include "EngineController.h"

#include <KIcon>
#include <KLocale>
#include <KWindowSystem>

#include <QTimer>

namespace Amarok
{
    KNotificationBackend*
    KNotificationBackend::s_instance = 0;

    KNotificationBackend*
    KNotificationBackend::instance()
    {
        return s_instance ? s_instance : s_instance = new KNotificationBackend();
    }

    void
    KNotificationBackend::destroy()
    {
        if( s_instance ) { delete s_instance; s_instance = 0; }
    }
}

Amarok::KNotificationBackend::KNotificationBackend()
    : m_notify( 0 )
    , m_enabled( false )
{
    m_timer = new QTimer( this );
    m_timer->setSingleShot( true );
    connect( m_timer, SIGNAL( timeout() ), this, SLOT( showCurrentTrack() ) );

    EngineController *engine = The::engineController();

    connect( engine, SIGNAL( trackPlaying( Meta::TrackPtr ) ),
             this, SLOT( trackPlaying() ) );
    connect( engine, SIGNAL( trackMetadataChanged( Meta::TrackPtr ) ),
             this, SLOT( trackPlaying() ) );
    connect( engine, SIGNAL( albumMetadataChanged( Meta::AlbumPtr ) ),
             this, SLOT( trackPlaying() ) );

}

Amarok::KNotificationBackend::~KNotificationBackend()
{
    if (m_notify)
        m_notify->close();
}

void
Amarok::KNotificationBackend::setEnabled(bool enable)
{
    m_enabled = enable;
}

bool
Amarok::KNotificationBackend::isEnabled() const
{
    return m_enabled;
}

bool
Amarok::KNotificationBackend::isFullscreenWindowActive() const
{
    // Get information of the active window.
    KWindowInfo activeWindowInfo = KWindowSystem::windowInfo( KWindowSystem::activeWindow(), NET::WMState );

    // Check if it is running in fullscreen mode.
    return activeWindowInfo.hasState( NET::FullScreen );
}

void
Amarok::KNotificationBackend::notificationClosed()
{
    if( sender() == m_notify )
        m_notify = 0;
}

void
Amarok::KNotificationBackend::trackPlaying()
{
    if( !m_enabled )
        return; // KNotify is disabled, so don't start timer

    m_timer->start( 3000 ); // Wait some time to display the correct cover and also to check if phonon really managed to play the track
}

void Amarok::KNotificationBackend::show(const QString& title, const QString& body, const QPixmap& pixmap)
{
    QPixmap icon;
    if (pixmap.isNull()) {
        KIconLoader loader;
        icon = loader.loadIcon( QString("amarok"), KIconLoader::Desktop );
    }
    else
        icon = pixmap;

    KNotification::event( KNotification::Notification , title, body, icon );
}

void
Amarok::KNotificationBackend::showCurrentTrack( bool force ) // slot
{
    if( !force && !m_enabled )
        return;

    EngineController *engine = The::engineController();

    Meta::TrackPtr track = engine->currentTrack();
    if( engine->isPlaying() && track )
    {
        if( m_notify ) {
            m_notify->close(); // Close old notification (when switching quickly between tracks)
        }

        m_notify = new KNotification( "trackChange" );
        connect( m_notify, SIGNAL(closed()), this, SLOT(notificationClosed()) );

        Meta::AlbumPtr album = track->album();
        if( album )
            m_notify->setPixmap( The::svgHandler()->imageWithBorder( album, 80 ) );

        m_notify->setTitle( i18n( "Now playing" ) );

        m_notify->setText( engine->prettyNowPlaying() );
        m_notify->sendEvent();
    }
}

#include "KNotificationBackend.moc"
