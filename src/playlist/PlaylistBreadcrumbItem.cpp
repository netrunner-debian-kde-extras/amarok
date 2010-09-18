/****************************************************************************************
 * Copyright (c) 2009 Nikolaj Hald Nielsen <nhn@kde.org>                                *
 * Copyright (c) 2009 Téo Mrnjavac <teo@kde.org>                                        *
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

#include "PlaylistBreadcrumbItem.h"

#include "PlaylistDefines.h"
#include "PlaylistSortWidget.h"

#include <KIcon>
#include <KLocale>

#include <QMenu>

namespace Playlist
{

BreadcrumbItem::BreadcrumbItem( BreadcrumbLevel *level, QWidget *parent )
    : KHBox( parent )
{
     m_name = level->name();
     m_prettyName = level->prettyName();

     //Let's set up the "siblings" button first...
    m_menuButton = new BreadcrumbItemMenuButton( this );
    QMenu *menu = new QMenu( this );
    QStringList usedBreadcrumbLevels = qobject_cast< SortWidget * >( parent )->levels();

    QMap< QString, QPair< KIcon, QString > > siblings = level->siblings();
    for( QMap< QString, QPair< KIcon, QString > >::const_iterator i = siblings.begin(); i != siblings.end(); ++i )
    {
        QAction *action = menu->addAction( i.value().first, i.value().second );
        action->setData( i.key() );
        if( usedBreadcrumbLevels.contains( i.key() ) )
            action->setEnabled( false );
    }
    m_menuButton->setMenu( menu );
    const int offset = 6;
    menu->setContentsMargins( offset, 1, 1, 2 );
    connect( menu, SIGNAL( triggered( QAction* ) ), this, SLOT( siblingTriggered( QAction* ) ) );

    //And then the main breadcrumb button...
    bool noArrow = false;
    if( m_name == "Shuffle" )
        noArrow = true;
    m_mainButton = new BreadcrumbItemSortButton( level->icon(), level->prettyName(), noArrow, this );

    connect( m_mainButton, SIGNAL( clicked() ), this, SIGNAL( clicked() ) );
    connect( m_mainButton, SIGNAL( arrowToggled( Qt::SortOrder ) ), this, SIGNAL( orderInverted() ) );

    connect( m_mainButton, SIGNAL( sizePolicyChanged() ), this, SLOT( updateSizePolicy() ) );
    menu->hide();

    updateSizePolicy();

}

BreadcrumbItem::~BreadcrumbItem()
{}

QString
BreadcrumbItem::name() const
{
    return m_name;
}

Qt::SortOrder
BreadcrumbItem::sortOrder() const
{
    return m_mainButton->orderState();
}

void
BreadcrumbItem::invertOrder()
{
    m_mainButton->invertOrder();
}

void
BreadcrumbItem::updateSizePolicy()
{
    setSizePolicy( m_mainButton->sizePolicy() );
}

void
BreadcrumbItem::siblingTriggered( QAction * action )
{
    emit siblingClicked( action );
}


/////// BreadcrumbAddMenuButton methods begin here

BreadcrumbAddMenuButton::BreadcrumbAddMenuButton( QWidget *parent )
    : BreadcrumbItemMenuButton( parent )
{
    setToolTip( i18n( "Add a sorting level to the playlist." ) );

    m_menu = new QMenu( this );
    for( int i = 0; i < NUM_COLUMNS; ++i )  //might be faster if it used a const_iterator
    {
        if( !sortableCategories.contains( internalColumnNames.at( i ) ) )
            continue;
        QAction *action = m_menu->addAction( KIcon( iconNames.at( i ) ), QString( columnNames.at( i ) ) );
        action->setData( internalColumnNames.at( i ) );
        //FIXME: this menu should have the same margins as other Playlist::Breadcrumb and
        //       BrowserBreadcrumb menus.
    }
    QAction *action = m_menu->addAction( KIcon( "media-playlist-shuffle" ), QString( i18n( "Shuffle" ) ) );
    action->setData( "Shuffle" );

    connect( m_menu, SIGNAL( triggered( QAction* ) ), this, SLOT( siblingTriggered( QAction* ) ) );

    setMenu( m_menu );
}

BreadcrumbAddMenuButton::~BreadcrumbAddMenuButton()
{}

void
BreadcrumbAddMenuButton::siblingTriggered( QAction *action )
{
    emit siblingClicked( action->data().toString() );
}

void
BreadcrumbAddMenuButton::updateMenu( const QStringList &usedBreadcrumbLevels )
{
    if( usedBreadcrumbLevels.contains( "Shuffle" ) )
        hide();
    else
        show();
    foreach( QAction *action, m_menu->actions() )
    {
        if( usedBreadcrumbLevels.contains( action->data().toString() ) )
            action->setEnabled( false );
        else
            action->setEnabled( true );
    }

}

}   //namespace Playlist
