/****************************************************************************************
 * Copyright (c) 2004-2007 Mark Kretschmann <kretschmann@kde.org>                       *
 * Copyright (c) 2004 Frederik Holljen <fh@ez.no>                                       *
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

#include "NotificationsConfig.h"

#include "amarokconfig.h"
#include "Debug.h"
#include "KNotificationBackend.h"
#include "Osd.h"
//#include "QStringx.h"

#include <QDesktopWidget>

NotificationsConfig::NotificationsConfig( QWidget* parent )
    : ConfigDialogBase( parent )
{
    setupUi( this );

    m_osdPreview = new OSDPreviewWidget( this ); //must be child!!!
    m_osdPreview->setAlignment( static_cast<OSDWidget::Alignment>( AmarokConfig::osdAlignment() ) );
    m_osdPreview->setOffset( AmarokConfig::osdYOffset() );

    #ifdef Q_WS_MAC
        QCheckBox* growl = new QCheckBox( i18n( "Use Growl for notifications" ), this );
        growl->setChecked( AmarokConfig::growlEnabled() );
        gridLayout_5->addWidget( growl, 2, 0, 1, 1 );
        connect( growl,         SIGNAL( toggled( bool ) ),
                 this,                      SLOT( setGrowlEnabled( bool ) ) );
    #endif
    // Enable/disable the translucency option depending on whether the QWidget has the WindowOpacity property
    // kcfg_OsdUseTranslucency->setEnabled( CheckHasWindowOpacityProperty )

    connect( m_osdPreview, SIGNAL( positionChanged() ), SLOT( slotPositionChanged() ) );

    const int numScreens = QApplication::desktop()->numScreens();
    for( int i = 0; i < numScreens; i++ )
        kcfg_OsdScreen->addItem( QString::number( i ) );

    connect( kcfg_OsdTextColor,       SIGNAL( changed( const QColor& ) ),
             m_osdPreview,            SLOT( setTextColor(const QColor& ) ) );
    connect( kcfg_OsdUseCustomColors, SIGNAL( toggled( bool ) ),
             this,                    SLOT( useCustomColorsToggled( bool ) ) );
    connect( kcfg_OsdScreen,          SIGNAL( activated( int ) ),
             m_osdPreview,            SLOT( setScreen( int ) ) );
    connect( kcfg_OsdEnabled,         SIGNAL( toggled( bool ) ),
             m_osdPreview,            SLOT( setVisible( bool ) ) );
    connect( kcfg_OsdUseTranslucency, SIGNAL( toggled( bool ) ),
             m_osdPreview,            SLOT( setTranslucent( bool ) ) );

    /*
    Amarok::QStringx text = i18n(
            "<h3>Tags Displayed in OSD</h3>"
            "You can use the following tokens:"
                "<ul>"
                "<li>Title - %1"
                "<li>Album - %2"
                "<li>Artist - %3"
                "<li>Genre - %4"
                "<li>Bitrate - %5"
                "<li>Year - %6"
                "<li>Track Length - %7"
                "<li>Track Number - %8"
                "<li>Filename - %9"
                "<li>Directory - %10"
                "<li>Type - %11"
                "<li>Comment - %12"
                "<li>Score - %13"
                "<li>Playcount - %14"
                "<li>Disc Number - %15"
                "<li>Rating - %16"
		"<li>Elapsed Time - %17"
                "</ul>"
            "If you surround sections of text that contain a token with curly-braces, that section will be hidden if the token is empty, for example:"
                "<pre>%18</pre>"
            "Will not show <b>Score: <i>%score</i></b> if the track has no score." );
    */
}

NotificationsConfig::~NotificationsConfig()
{}


///////////////////////////////////////////////////////////////
// REIMPLEMENTED METHODS from ConfigDialogBase
///////////////////////////////////////////////////////////////

bool
NotificationsConfig::hasChanged()
{
    return false;
}

bool
NotificationsConfig::isDefault()
{
    return false;
}

void
NotificationsConfig::updateSettings()
{
    DEBUG_BLOCK

    AmarokConfig::setOsdAlignment( m_osdPreview->alignment() );
    AmarokConfig::setOsdYOffset( m_osdPreview->y() );
    AmarokConfig::setOsdUseTranslucency( kcfg_OsdUseTranslucency->isChecked() );

    // Enable/disable knotify support
    if( kcfg_KNotifyEnabled->isChecked() )
        Amarok::KNotificationBackend::instance();
    else
        Amarok::KNotificationBackend::destroy();

    emit settingsChanged( QString() );
}


///////////////////////////////////////////////////////////////
// PRIVATE METHODS
///////////////////////////////////////////////////////////////

void
NotificationsConfig::slotPositionChanged()
{
    kcfg_OsdScreen->blockSignals( true );
    kcfg_OsdScreen->setCurrentIndex( m_osdPreview->screen() );
    kcfg_OsdScreen->blockSignals( false );

    // Update button states (e.g. "Apply")
    //emit settingsChanged();
}

void
NotificationsConfig::hideEvent( QHideEvent* )
{
    m_osdPreview->hide();
}

void
NotificationsConfig::showEvent( QShowEvent* )
{
    useCustomColorsToggled( kcfg_OsdUseCustomColors->isChecked() );

    m_osdPreview->setScreen( kcfg_OsdScreen->currentIndex() );
    m_osdPreview->setVisible( kcfg_OsdEnabled->isChecked() );
}

void
NotificationsConfig::setGrowlEnabled( bool enable )
{
    DEBUG_BLOCK
    AmarokConfig::setGrowlEnabled( enable );
}

void
NotificationsConfig::useCustomColorsToggled( bool on )
{
    m_osdPreview->setUseCustomColors( on, kcfg_OsdTextColor->color() );
}

#include "NotificationsConfig.moc"