/****************************************************************************************
 * Copyright (c) 2009 Simon Esneault <simon.esneault@gmail.com>                         *
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

#include "TextScrollingWidget.h"

#include "core/support/Debug.h"

#include <QFont>
#include <QFontMetrics>
#include <QGraphicsTextItem>
#include <QGraphicsSceneHoverEvent>
#include <QGraphicsWidget>
#include <QPainter>
#include <QTextDocument>
#include <QTimer>

#include <Plasma/Animator>

#define DEBUG_PREFIX "TextScrollingWidget"


TextScrollingWidget::TextScrollingWidget( QGraphicsItem* parent )
    : QGraphicsTextItem( parent )
    , m_fm( 0 )
    , m_textFormat( Qt::PlainText )
    , m_delta( 0 )
    , m_currentDelta( 0. )
    , m_animfor( 0 )
    , m_animback( 0 )
    , m_animating( false )
{
    setAcceptHoverEvents( true );
    connect ( Plasma::Animator::self(), SIGNAL(customAnimationFinished ( int ) ), this, SLOT( animationFinished( int ) ) );
    document()->setDocumentMargin( 0 );
}

TextScrollingWidget::~TextScrollingWidget()
{
    delete m_fm;
}

void
TextScrollingWidget::setBrush( const QBrush &brush )
{
    setDefaultTextColor( brush.color() );
}

void
TextScrollingWidget::setScrollingText( const QString &text, const QRectF &rect )
{
 //   DEBUG_BLOCK
    if ( !m_fm )
        m_fm = new QFontMetrics( font() );
    m_rect = rect;
    m_text = text;
    m_currentDelta = 0;

    // reset the animation and stuff
    Plasma::Animator::self()->stopCustomAnimation( m_animback );
    Plasma::Animator::self()->stopCustomAnimation( m_animfor );
    m_animating = false ;

    const int textWidth = m_fm->width( m_text );
    m_delta = textWidth + 5 > m_rect.width() ? textWidth + 8 - m_rect.width() : 0;
    setText( m_fm->elidedText( m_text, Qt::ElideRight, (int)m_rect.width() ) );
}

void
TextScrollingWidget::setText( const QString &text )
{
    if( requirePlainText() )
        setPlainText( text );
    else
        setHtml( text );
}

void
TextScrollingWidget::setTextFormat( Qt::TextFormat fmt )
{
    m_textFormat = fmt;
}

QString
TextScrollingWidget::text() const
{
    if( requirePlainText() )
        return toPlainText();
    else
        return toHtml();
}

bool
TextScrollingWidget::requirePlainText() const
{
    bool plain;
    switch( m_textFormat )
    {
    case Qt::RichText:
        plain = false;
        break;

    case Qt::AutoText:
        plain = !Qt::mightBeRichText( m_text );
        break;

    case Qt::PlainText:
    default:
        plain = true;
    }
    return plain;
}

Qt::TextFormat
TextScrollingWidget::textFormat() const
{
    return m_textFormat;
}

void
TextScrollingWidget::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    // clip the widget.
    QRect rec( boundingRect().translated( m_currentDelta, 0 ).toRect() );
    rec.setWidth( m_rect.width() );
    
    painter->setClipRegion( QRegion( rec ) );
    QGraphicsTextItem::paint( painter, option, widget );
}

void
TextScrollingWidget::hoverEnterEvent( QGraphicsSceneHoverEvent* e )
{
    Q_UNUSED( e );    
    if ( !m_animating && m_delta )
    {
        DEBUG_BLOCK
        
        m_animating = true ;
        setText( m_text );
        QTimer::singleShot( 0, this, SLOT( startAnimFor() ) );
    }
}

bool
TextScrollingWidget::isAnimating()
{
    return ( m_animating != 0 );
}

void
TextScrollingWidget::animationFinished( int id )
{
    if ( id == m_animfor )
    {
        Plasma::Animator::self()->stopCustomAnimation( m_animfor );
        QTimer::singleShot( 250, this, SLOT( startAnimBack() ) );
    }
    else if ( id == m_animback )
    {
        Plasma::Animator::self()->stopCustomAnimation( m_animback );
        // Scroll again if the mouse is still over.
        if ( isUnderMouse() )
        {
            m_animating = true ;
            QTimer::singleShot(250, this, SLOT( startAnimFor() ) );
        }
        else
        {
            m_animating = false;
            setText( m_fm->elidedText( m_text, Qt::ElideRight, (int)( m_rect.width() ) ) );
        }
    }
}

void
TextScrollingWidget::startAnimFor()
{
    m_animfor = Plasma::Animator::self()->customAnimation( m_delta*2, m_delta*15, Plasma::Animator::EaseInOutCurve, this, "animateFor" );
}

void
TextScrollingWidget::startAnimBack()
{
    m_animback = Plasma::Animator::self()->customAnimation( m_delta*2, m_delta*15, Plasma::Animator::EaseInOutCurve, this, "animateBack" );
}

void
TextScrollingWidget::animateFor( qreal anim )
{
//    DEBUG_BLOCK
    m_currentDelta = ( float )( anim * ( m_delta ) );
    setPos( m_rect.left() - m_currentDelta, pos().y() );
}

void
TextScrollingWidget::animateBack( qreal anim )
{
    // DEBUG_BLOCK
    m_currentDelta = m_delta - ( float )( anim * ( m_delta ) );
    setPos( m_rect.left() - m_currentDelta, pos().y() );
}

#include "TextScrollingWidget.moc"
