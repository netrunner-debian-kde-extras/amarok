/****************************************************************************************
 * Copyright (c) 2009 Nikolaj Hald Nielsen <nhn@kde.org>                                *
 * Copyright (c) 2010 Ralf Engels <ralf-engels@gmx.de>                                  *
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

#include "InlineEditorWidget.h"

#include "core/support/Debug.h"
#include "moodbar/MoodbarManager.h"
#include "playlist/PlaylistDefines.h"
#include "playlist/layouts/LayoutManager.h"
#include "playlist/proxymodels/GroupingProxy.h"
#include "PrettyItemDelegate.h"
#include "SvgHandler.h"
#include "widgets/kratingwidget.h"

#include <KHBox>
#include <KVBox>

#include <QEvent>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QPaintEvent>

using namespace Amarok;
using namespace Playlist;

InlineEditorWidget::InlineEditorWidget( QWidget * parent, const QModelIndex &index,
                                        PlaylistLayout layout, int height )
    : KHBox( parent )
    , m_index( index )
    , m_layout( layout )
    , m_itemHeight( height )
    , m_layoutChanged( false )
{
    setAutoFillBackground( false ); // we want our own playlist background

    int frameHMargin = style()->pixelMetric( QStyle::PM_FocusFrameHMargin );
    int frameVMargin = style()->pixelMetric( QStyle::PM_FocusFrameVMargin );
    setContentsMargins( frameHMargin, frameVMargin, frameHMargin, frameVMargin );
    setContentsMargins( frameHMargin, 0, frameHMargin, 0 );

    //prevent editor closing when clicking a rating widget or pressing return in a line edit.
    setFocusPolicy( Qt::StrongFocus );

    createChildWidgets();
}

InlineEditorWidget::~InlineEditorWidget()
{
}

void InlineEditorWidget::createChildWidgets()
{
    //For now, we don't allow editing of the "head" data, just the body
    LayoutItemConfig config = m_layout.layoutForItem( m_index );

    int trackRows = config.rows();
    if ( trackRows == 0 )
        return;

    // some style margins:
    int frameHMargin = style()->pixelMetric( QStyle::PM_FocusFrameHMargin );
    int frameVMargin = style()->pixelMetric( QStyle::PM_FocusFrameVMargin );

    int coverHeight = m_itemHeight - frameVMargin * 2;
    int rowHeight = m_itemHeight / trackRows;

    if ( config.showCover() )
    {
        QModelIndex coverIndex = m_index.model()->index( m_index.row(), CoverImage );
        QPixmap albumPixmap = coverIndex.data( Qt::DisplayRole ).value<QPixmap>();
        if ( !albumPixmap.isNull() )
        {
            if ( albumPixmap.width() > albumPixmap.width() )
                albumPixmap = albumPixmap.scaledToWidth( coverHeight );
            else
                albumPixmap = albumPixmap.scaledToHeight( coverHeight );

            QLabel *coverLabel = new QLabel( this );
            coverLabel->setPixmap( albumPixmap );

            QWidget *spacing = new QWidget( this );
            spacing->setFixedWidth( frameHMargin * 2 );
        }
    }

    KVBox *rowsWidget = new KVBox( this );

    for ( int i = 0; i < trackRows; i++ )
    {

        QSplitter *rowWidget = new QSplitter( rowsWidget );
        connect( rowWidget, SIGNAL( splitterMoved ( int, int ) ), this, SLOT( splitterMoved( int, int ) ) );

        m_splitterRowMap.insert( rowWidget, i );


        LayoutItemConfigRow row = config.row( i );
        const int elementCount = row.count();

        //we need to do a quick pass to figure out how much space is left for auto sizing elements
        qreal spareSpace = 1.0;
        int autoSizeElemCount = 0;
        for ( int k = 0; k < elementCount; ++k )
        {
            spareSpace -= row.element( k ).size();
            if ( row.element( k ).size() < 0.001 )
                autoSizeElemCount++;
        }

        qreal spacePerAutoSizeElem = spareSpace / (qreal) autoSizeElemCount;

        int itemIndex = 0;
        for ( int j = 0; j < elementCount; ++j )
        {
            LayoutItemConfigRowElement element = row.element( j );

            // -- calculate the size
            qreal size;
            if ( element.size() > 0.0001 )
                size = element.size();
            else
                size = spacePerAutoSizeElem;

            qreal itemWidth = 100 * size;

            int value = element.value();

            QModelIndex textIndex = m_index.model()->index( m_index.row(), value );
            QString text = textIndex.data( Qt::DisplayRole ).toString();
            m_orgValues.insert( value, text );

            //special case for painting the rating...
            if ( value == Rating )
            {
                int rating = textIndex.data( Qt::DisplayRole ).toInt();

                KRatingWidget * ratingWidget = new KRatingWidget( 0 );
                rowWidget->addWidget( ratingWidget );
                rowWidget->setStretchFactor( itemIndex, itemWidth );
                ratingWidget->setAlignment( element.alignment() );
                ratingWidget->setRating( rating );
                ratingWidget->setAttribute( Qt::WA_NoMousePropagation, true );

                connect( ratingWidget, SIGNAL( ratingChanged( uint ) ), this, SLOT( ratingValueChanged() ) );

                m_editorRoleMap.insert( ratingWidget, value );

            }
            else if ( value == Divider )
            {
                debug() << "painting divider...";
                QPixmap left = The::svgHandler()->renderSvg(
                        "divider_left",
                        1, rowHeight,
                        "divider_left" );

                QPixmap right = The::svgHandler()->renderSvg(
                        "divider_right",
                        1, rowHeight,
                        "divider_right" );

                QPixmap dividerPixmap( 2, rowHeight );
                dividerPixmap.fill( Qt::transparent );

                QPainter painter( &dividerPixmap );
                painter.drawPixmap( 0, 0, left );
                painter.drawPixmap( 1, 0, right );

                QLabel * dividerLabel = new QLabel( 0 );
                rowWidget->addWidget( dividerLabel );
                dividerLabel->setPixmap( dividerPixmap );
                dividerLabel->setAlignment( element.alignment() );
                rowWidget->setStretchFactor( itemIndex, itemWidth );
            }
            else if( value == Moodbar )
            {
                //we cannot ask the model for the moodbar directly as we have no
                //way of asking for a specific size. Instead just get the track from
                //the model and ask the moodbar manager ourselves.


                debug() << "painting moodbar in PrettyItemDelegate::paintItem";

                Meta::TrackPtr track = m_index.data( TrackRole ).value<Meta::TrackPtr>();

                if( The::moodbarManager()->hasMoodbar( track ) )
                {
                    QPixmap moodbar = The::moodbarManager()->getMoodbar( track, itemWidth, rowHeight - 8 );

                    QLabel * moodbarLabel = new QLabel( 0 );
                    moodbarLabel->setScaledContents( true );
                    rowWidget->addWidget( moodbarLabel );
                    moodbarLabel->setPixmap( moodbar );
                    rowWidget->setStretchFactor( itemIndex, itemWidth );
                }
            }
            else
            {
                    QLineEdit * edit = new QLineEdit( text, 0 );
                    rowWidget->addWidget( edit );
                    rowWidget->setStretchFactor( itemIndex, itemWidth );
                    edit->setFrame( false );
                    edit->setAlignment( element.alignment() );
                    edit->installEventFilter(this);

                    // -- set font
                    bool bold = element.bold();
                    bool italic = element.italic();
                    bool underline = element.underline();

                    QFont font = edit->font();
                    font.setBold( bold );
                    font.setItalic( italic );
                    font.setUnderline( underline );
                    edit->setFont( font );

                    connect( edit, SIGNAL( editingFinished() ), this, SLOT( editValueChanged() ) );

                    //check if this is a column that is editable. If not, make the
                    //line edit read only.
                    if ( !editableColumns.contains( value ) )
                    {
                        edit->setReadOnly( true );
                        edit->setDisabled( true );
                    }

                    m_editorRoleMap.insert( edit, value );
            }

            itemIndex++;
        }
    }
}

void InlineEditorWidget::editValueChanged()
{
    DEBUG_BLOCK

    QObject * senderObject = sender();

    QLineEdit * edit = dynamic_cast<QLineEdit *>( senderObject );
    if( !edit )
        return;

    int role = m_editorRoleMap.value( edit );

    //only save values if something has actually changed.
    if( m_orgValues.value( role ) != edit->text() )
    {
        debug() << "Storing changed value: " << edit->text();
        m_changedValues.insert( role, edit->text() );
    }


}

void InlineEditorWidget::ratingValueChanged()
{
    DEBUG_BLOCK

    KRatingWidget * edit = qobject_cast<KRatingWidget *>( sender() );
    if( !edit )
        return;

    int role = m_editorRoleMap.value( edit );
    m_changedValues.insert( role, QString::number( edit->rating() ) );
}

QMap<int, QString> InlineEditorWidget::changedValues()
{
    DEBUG_BLOCK
    if( m_layoutChanged )
        LayoutManager::instance()->updateCurrentLayout( m_layout );
    return m_changedValues;
}


void InlineEditorWidget::splitterMoved( int pos, int index )
{
    DEBUG_BLOCK

    Q_UNUSED( pos )
    Q_UNUSED( index )

    QSplitter * splitter = dynamic_cast<QSplitter *>( sender() );
    if ( !splitter )
        return;

    int row = m_splitterRowMap.value( splitter );
    debug() << "on row: " << row;

    //first, get total size of all items;
    QList<int> sizes = splitter->sizes();

    int total = 0;
    foreach( int size, sizes )
        total += size;

    //resize all items as the splitters take up some space, so we need to normalize the combined size to 1.
    QList<qreal> newSizes;

    foreach( int size, sizes )
    {
        qreal newSize = (qreal) size / (qreal) total;
        newSizes << newSize;
    }

    LayoutItemConfig itemConfig = m_layout.layoutForItem( m_index );

    LayoutItemConfigRow rowConfig = itemConfig.row( row );

    //and now we rebuild a new layout...
    LayoutItemConfigRow newRowConfig;

    for( int i = 0; i<rowConfig.count(); i++ )
    {
        LayoutItemConfigRowElement element = rowConfig.element( i );
        debug() << "item " << i << " old/new: " << element.size() << "/" << newSizes.at( i );
        element.setSize( newSizes.at( i ) );
        newRowConfig.addElement( element );
    }

    LayoutItemConfig newItemConfig;
    newItemConfig.setActiveIndicatorRow( itemConfig.activeIndicatorRow() );
    newItemConfig.setShowCover( itemConfig.showCover() );

    for( int i = 0; i<itemConfig.rows(); i++ )
    {
        if( i == row )
            newItemConfig.addRow( newRowConfig );
        else
            newItemConfig.addRow( itemConfig.row( i ) );
    }

    m_layout.setLayoutForPart( m_layout.partForItem( m_index ), newItemConfig );

    m_layoutChanged = true;
}

bool
InlineEditorWidget::eventFilter( QObject *obj, QEvent *event )
{
    QList<QWidget *> editWidgets = m_editorRoleMap.keys();

    QWidget * widget = qobject_cast<QWidget *>( obj );

    if( editWidgets.contains( widget ) )
    {
        if( event->type() == QEvent::KeyPress )
        {
            QKeyEvent * keyEvent = static_cast<QKeyEvent *>( event );
            if( keyEvent->key() == Qt::Key_Return )
            {
                debug() << "InlineEditorWidget ate a return press for a child widget";
                if( widget )
                {
                    widget->clearFocus ();
                    debug() << "emitting editingDone!";
                    emit editingDone( this );
                }

                return true;
            }
            else
                return false;
        }
        else
            return false;

    }
    else
        return KHBox::eventFilter( obj, event );
}

#include "InlineEditorWidget.moc"

