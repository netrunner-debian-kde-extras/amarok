/****************************************************************************************
 * Copyright (c) 2008-2010 Soren Harward <stharward@gmail.com>                          *
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

#define DEBUG_PREFIX "TrackSelectWidget"

#include "TrackSelectWidget.h"

#include "browsers/CollectionTreeItem.h"
#include "browsers/CollectionTreeItemModel.h"
#include "browsers/CollectionTreeView.h"
#include "browsers/collectionbrowser/CollectionTreeItemDelegate.h"
#include "core/support/Debug.h"

#include <amarokconfig.h>

#include <KLocale>
#include <KSqueezedTextLabel>
#include <KVBox>

#include <QLabel>

TrackSelectWidget::TrackSelectWidget( QWidget* parent )
    : KVBox( parent )
{
    DEBUG_BLOCK
    m_label = new KSqueezedTextLabel( this );
    m_label->hide(); // TODO: decide whether the label should be shown or not
    m_label->setTextElideMode( Qt::ElideRight );
    setData( Meta::DataPtr() );

    m_view = new CollectionTreeView( this );
    m_view->setAlternatingRowColors( true );
    m_view->setRootIsDecorated( false );
    m_view->setFrameShape( QFrame::NoFrame );

    CollectionTreeItemDelegate* delegate = new CollectionTreeItemDelegate( m_view );
    m_view->setItemDelegate( delegate );

    QList<int> levelNumbers = Amarok::config( "Collection Browser" ).readEntry( "TreeCategory", QList<int>() );
    QList<CategoryId::CatMenuId> levels;
    foreach( int levelNumber, levelNumbers )
        levels << CategoryId::CatMenuId( levelNumber );
    if ( levels.isEmpty() )
        levels << CategoryId::Artist << CategoryId::Album;
    m_model = new CollectionTreeItemModel( levels );
    m_view->setModel( m_model );

    connect( m_view, SIGNAL( itemSelected( CollectionTreeItem* ) ),
             this, SLOT( recvNewSelection( CollectionTreeItem* ) ) );
}

TrackSelectWidget::~TrackSelectWidget() {}

void TrackSelectWidget::setData( const Meta::DataPtr& data )
{
    debug() << "setting label to" << dataToLabel( data );
    m_label->setText( i18n("Checkpoint: <b>%1</b>", dataToLabel( data ) ) );
}

void
TrackSelectWidget::recvNewSelection( CollectionTreeItem* item )
{
    if ( item && item->isDataItem() ) {
        Meta::DataPtr data = item->data();
        if ( data != Meta::DataPtr() ) {
            setData( data );
            debug() << "new selection" << data->prettyName();
            emit selectionChanged( data );
        }
    }
}

const QString TrackSelectWidget::dataToLabel( const Meta::DataPtr& data ) const
{
    if ( data != Meta::DataPtr() ) {
        if ( Meta::TrackPtr track = Meta::TrackPtr::dynamicCast( data ) ) {
            return QString( i18n("Track: %1", track->prettyName() ) );
        } else if ( Meta::AlbumPtr album = Meta::AlbumPtr::dynamicCast( data ) ) {
            return QString( i18n("Album: %1", album->prettyName() ) );
        } else if ( Meta::ArtistPtr artist = Meta::ArtistPtr::dynamicCast( data ) ) {
            return QString( i18n("Artist: %1", artist->prettyName() ) );
        }
        // TODO: can things other than tracks, artists, and albums end up here?
    }
    return QString( i18n("empty") );
}
