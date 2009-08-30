/****************************************************************************************
 * Copyright (c) 2009  Nikolaj Hald Nielsen <nhnFreespirit@gmail.com>    *
 *                                                                                      *
 * This program is free software; you can redistribute it and/or modify it under        *
 * the terms of the GNU General Public License as published by the Free Software        *
 * Foundation; either version 2 of the License, or (at your option) any later           *
 * version.                                                                             *
 *                                                                                      *
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY      *
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A      *
 * PARTICULAR PURPOSE. See the GNU General Pulic License for more details.              *
 *                                                                                      *
 * You should have received a copy of the GNU General Public License along with         *
 * this program.  If not, see <http://www.gnu.org/licenses/>.                           *
 ****************************************************************************************/
 
#include "MainToolbarNG.h"

#include "ActionClasses.h"
#include "Amarok.h"
#include "EngineController.h"
#include "VolumePopupButton.h"

#include "widgets/ProgressWidget.h"

#include <KIcon>
#include <KLocale>
#include <KVBox>

#include <QAction>
#include <QLabel>
#include <QLayout>
#include <QMenu>
#include <QSlider>
#include <QToolButton>
#include <QWidgetAction>

MainToolbarNG::MainToolbarNG( QWidget * parent )
    : QToolBar( i18n( "Main Toolbar NG" ), parent )
    , EngineObserver( The::engineController() )
    , m_currentTrackToolbar( 0 )

{
    setObjectName( "Main Toolbar NG" );

    setIconSize( QSize( 28, 28 ) );
    layout()->setSpacing( 0 );
    setContentsMargins( 0, 0, 0, 0 );

    addAction( Amarok::actionCollection()->action( "prev" ) );
    addAction( Amarok::actionCollection()->action( "play_pause" ) );
    addAction( Amarok::actionCollection()->action( "stop" ) );
    addAction( Amarok::actionCollection()->action( "next" ) );

    m_currentTrackToolbar = new CurrentTrackToolbar( 0 );

    addWidget( m_currentTrackToolbar );

    ProgressWidget *progressWidget = new ProgressWidget( 0 );
    addWidget( progressWidget );


    QToolBar *volumeToolBar = new QToolBar( this );
    volumeToolBar->setIconSize( QSize( 22, 22 ) );
    volumeToolBar->setContentsMargins( 0, 0, 0, 0 );
    volumeToolBar->addWidget( new VolumePopupButton( this ) );
    addWidget( volumeToolBar );


}

MainToolbarNG::~MainToolbarNG()
{
}

#include "MainToolbarNG.moc"

