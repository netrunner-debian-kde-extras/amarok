/****************************************************************************************
 * Copyright (c) 2003-2009 Mark Kretschmann <kretschmann@kde.org>                       *
 * Copyright (c) 2005 Gabor Lehel <illissius@gmail.com>                                 *
 * Copyright (c) 2008 Dan Meltzer <parallelgrapefruit@gmail.com>                        *
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

#include "SliderWidget.h"

#include <config-amarok.h>

#include "Amarok.h"
#include "amarokurls/AmarokUrlHandler.h"
#include "amarokconfig.h"
#include "App.h"
#include "BookmarkTriangle.h"
#include "Debug.h"
#include "EngineController.h"
#include "meta/MetaUtility.h"
#include "SvgHandler.h"
#include "ProgressWidget.h"

#include <KIcon>
#include <KLocale>
#include <KStandardDirs>
#include <KGlobalSettings>

#include <QAction>
#include <QContextMenuEvent>
#include <QFontMetrics>
#include <QMenu>
#include <QPainter>


Amarok::Slider::Slider( Qt::Orientation orientation, uint max, QWidget *parent )
    : QSlider( orientation, parent )
    , m_sliding( false )
    , m_outside( false )
    , m_prevValue( 0 )
    , m_needsResize( true )
{
    setRange( 0, max );
    setAttribute( Qt::WA_NoMousePropagation, true );

    m_moodbarManager = The::moodbarManager();
}

QRect
Amarok::Slider::sliderHandleRect( const QRect &slider, qreal percent ) const
{
    QRect rect;

    if( m_usingCustomStyle)
        rect = The::svgHandler()->sliderKnobRect( slider, percent );
    else
    {
        const int handleSize = style()->pixelMetric( QStyle::PM_SliderControlThickness );
        rect = QRect( 0, 0, handleSize, handleSize );
        rect.moveTo( slider.x() + qRound( ( slider.width() - handleSize ) * percent ), slider.y() + 1 );
    }

    return rect;
}

void
Amarok::Slider::wheelEvent( QWheelEvent *e )
{
    DEBUG_BLOCK

    if( orientation() == Qt::Vertical )
    {
        // Will be handled by the parent widget
        e->ignore();
        return;
    }

    // Position Slider (horizontal)
    // only used for progress slider now!
    int step = e->delta() * 24;
    int nval = adjustValue( QSlider::value() + step );
    nval = qMax(nval, minimum());
    nval = qMin(nval, maximum());

    QSlider::setValue( nval );

    emit sliderReleased( value() );
}

void
Amarok::Slider::mouseMoveEvent( QMouseEvent *e )
{
    if ( m_sliding )
    {
        //feels better, but using set value of 20 is bad of course
        QRect rect( -20, -20, width()+40, height()+40 );

        if ( orientation() == Qt::Horizontal && !rect.contains( e->pos() ) )
        {
            if ( !m_outside )
            {
                QSlider::setValue( m_prevValue );
                //if mouse released outside of slider, emit sliderMoved to previous value
                emit sliderMoved( m_prevValue );
            }
            m_outside = true;
        }
        else
        {
            m_outside = false;
            slideEvent( e );
            emit sliderMoved( value() );
        }
    }
    else
        QSlider::mouseMoveEvent( e );
}

void
Amarok::Slider::slideEvent( QMouseEvent *e )
{
    QRect knob;
    if ( maximum() > minimum() )
        knob = sliderHandleRect( rect(), ((qreal)value()) / ( maximum() - minimum() ) );

    int position;
    int span;

    if( orientation() == Qt::Horizontal )
    {
        position = e->pos().x() - knob.width() / 2;
        span = width() - knob.width();
    }
    else
    {
        position = e->pos().y() - knob.height() / 2;
        span = height() - knob.height();
    }

    const int val = QStyle::sliderValueFromPosition( minimum(), maximum(), position, span );
    QSlider::setValue( val );
}

void
Amarok::Slider::mousePressEvent( QMouseEvent *e )
{
    m_sliding   = true;
    m_prevValue = adjustValue( QSlider::value() );

    QRect knob;
    if ( maximum() > minimum() )
        knob = sliderHandleRect( rect(), ((qreal)value()) / ( maximum() - minimum() ) );
    if ( !knob.contains( e->pos() ) )
        mouseMoveEvent( e );
}

void
Amarok::Slider::mouseReleaseEvent( QMouseEvent* )
{
    if( !m_outside && adjustValue( QSlider::value() ) != m_prevValue )
       emit sliderReleased( value() );

    m_sliding = false;
    m_outside = false;
}

void
Amarok::Slider::setValue( int newValue )
{
    //don't adjust the slider while the user is dragging it!
    if ( !m_sliding || m_outside )
        QSlider::setValue( adjustValue( newValue ) );
    else
        m_prevValue = adjustValue( newValue );
}


void Amarok::Slider::paintCustomSlider( QPainter *p, int x, int y, int width, int height, bool drawMoodbar )
{
    const QString prefix = "slider_bg_";

    const QString topleft = prefix + "topleft";
    const QString top = prefix + "top";
    const QString topright = prefix + "topright";
    const QString right = prefix + "right";
    const QString bottomright = prefix + "bottomright";
    const QString bottom = prefix + "bottom";
    const QString bottomleft = prefix + "bottomleft";
    const QString left = prefix + "left";

    if( m_needsResize )
    {
        m_topLeft = The::svgHandler()->renderSvg( topleft, m_borderWidth, m_borderHeight, topleft );
        m_top = The::svgHandler()->renderSvg( top, width - ( 2 * m_borderWidth ), m_borderHeight, top );
        m_topRight = The::svgHandler()->renderSvg( topright, m_borderWidth, m_borderHeight, topright );
        m_right = The::svgHandler()->renderSvg( right, m_borderWidth, height - ( 2 * m_borderHeight ), right );
        m_bottomRight = The::svgHandler()->renderSvg( bottomright, m_borderWidth, m_borderHeight, bottomright );
        m_bottom = The::svgHandler()->renderSvg( bottom, width - 2 * m_borderWidth, m_borderHeight, bottom );
        m_bottomLeft = The::svgHandler()->renderSvg( bottomleft, m_borderWidth, m_borderHeight, bottomleft );
        m_left = The::svgHandler()->renderSvg( left, m_borderWidth, height - 2 * m_borderHeight, left );
        m_needsResize = false;
    }


    //HACK: just for testing/fun
    if( drawMoodbar )
    {

        int moodWidth = width - ( 2 * m_borderWidth );
        int moodHeight = height - 8;

        if ( m_currentMoodBar.width() != moodWidth )
            {
            Meta::TrackPtr track = The::engineController()->currentTrack();
            if ( track && m_moodbarManager->hasMoodbar( track ) )
            {
                m_currentMoodBar = m_moodbarManager->getMoodbar( track, moodWidth, moodHeight );
            }
        }

        if ( !m_currentMoodBar.isNull() )
            p->drawPixmap( x + m_borderWidth, y + 4, m_currentMoodBar );
    }

    p->drawPixmap( x, y, m_topLeft );
    p->drawPixmap( x + m_borderWidth, y, m_top );
    p->drawPixmap( x + ( width - m_borderWidth ), y, m_topRight );
    p->drawPixmap( x + ( width - m_borderWidth ), y + m_borderHeight, m_right );
    p->drawPixmap( x + ( width - m_borderWidth ), y + ( height - m_borderHeight ), m_bottomRight );
    p->drawPixmap( x + m_borderWidth, y + ( height - m_borderHeight ), m_bottom );
    p->drawPixmap( x, y + ( height - m_borderHeight ) , m_bottomLeft );
    p->drawPixmap( x, y + m_borderHeight, m_left );

    if( value() != minimum() )
    {
        const int sliderHeight = height - ( m_sliderInsertY * 2 );
        const int sliderLeftWidth = sliderHeight / 3;
        const int sliderRightWidth = sliderLeftWidth;

        
        int knobX = ( ( (double) value() - (double) minimum()) / (maximum() - minimum()) ) * (width - (sliderLeftWidth + sliderRightWidth + m_sliderInsertX * 2) );

        const QString barLeft = "slider_bar_left";
        const QString barCenter = "slider_bar_center";
        const QString barRight = "slider_bar_right";

        p->drawPixmap( x + m_sliderInsertX, y + m_sliderInsertY, The::svgHandler()->renderSvg( barLeft, sliderLeftWidth , sliderHeight, barLeft ) );
        p->drawPixmap( x + m_sliderInsertX + sliderLeftWidth, y + m_sliderInsertY, The::svgHandler()->renderSvg( barCenter, knobX, sliderHeight, barCenter ) );
        p->drawPixmap( x + m_sliderInsertX + knobX + sliderLeftWidth, y + m_sliderInsertY, The::svgHandler()->renderSvg( barRight, sliderRightWidth, sliderHeight, barRight ) );
    }
}

void Amarok::Slider::paintCustomSliderNG( QPainter *p, int x, int y, int width, int height, double /*pos*/, bool paintMoodbar )
{
    qreal percent = 0.0;
    if ( maximum() > minimum() )
        percent = ((qreal)value()) / ( maximum() - minimum() );
    The::svgHandler()->paintCustomSlider( p,x, y, width, height, percent, underMouse(), paintMoodbar );
}


//////////////////////////////////////////////////////////////////////////////////////////
/// CLASS VolumeSlider
//////////////////////////////////////////////////////////////////////////////////////////

Amarok::VolumeSlider::VolumeSlider( uint max, QWidget *parent, bool customStyle )
    : Amarok::Slider( customStyle ? Qt::Horizontal : Qt::Vertical, max, parent )
{
    m_usingCustomStyle = customStyle;
    setFocusPolicy( Qt::NoFocus );

    if( !customStyle )
    {
        setInvertedAppearance( true );
        setInvertedControls( true );
    }
}

void
Amarok::VolumeSlider::mousePressEvent( QMouseEvent *e )
{
    if( e->button() != Qt::RightButton )
    {
        Amarok::Slider::mousePressEvent( e );
        slideEvent( e );
    }
}

void
Amarok::VolumeSlider::contextMenuEvent( QContextMenuEvent *e )
{
    QMenu menu;
    menu.setTitle(   i18n( "Volume" ) );
    menu.addAction(  i18n(   "100%" ) )->setData( 100 );
    menu.addAction(  i18n(    "80%" ) )->setData(  80 );
    menu.addAction(  i18n(    "60%" ) )->setData(  60 );
    menu.addAction(  i18n(    "40%" ) )->setData(  40 );
    menu.addAction(  i18n(    "20%" ) )->setData(  20 );
    menu.addAction(  i18n(     "0%" ) )->setData(   0 );

    /*
    // TODO: Phonon
    menu.addSeparator();
    menu.addAction( KIcon( "view-media-equalizer-amarok" ), i18n( "&Equalizer" ), kapp, SLOT( slotConfigEqualizer() ) )->setData( -1 );
    */

    QAction* a = menu.exec( mapToGlobal( e->pos() ) );
    if( a )
    {
        const int n = a->data().toInt();
        if( n >= 0 )
        {
            QSlider::setValue( n );
            emit sliderReleased( n );
        }
    }
}

void
Amarok::VolumeSlider::wheelEvent( QWheelEvent *e )
{
    const uint step = e->delta() / Amarok::VOLUME_SENSITIVITY;
    QSlider::setValue( QSlider::value() + step );

    emit sliderReleased( value() );
}

void
Amarok::VolumeSlider::paintEvent( QPaintEvent *event )
{
    if( m_usingCustomStyle )
    {
        QPainter p( this );
        paintCustomSliderNG( &p, 0, 2, width(), height() );
        p.end();
        return;
    }

    QSlider::paintEvent( event );
}


//////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////// TIMESLIDER ////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////

Amarok::TimeSlider::TimeSlider( QWidget *parent )
    : Amarok::Slider( Qt::Horizontal, 0, parent )
    , m_triangles()
    , m_knobX( 0.0 )
{
    m_usingCustomStyle = true;
    setFocusPolicy( Qt::NoFocus );
}

void
Amarok::TimeSlider::setSliderValue( int value )
{
    Amarok::Slider::setValue( value );
}

void
Amarok::TimeSlider::paintEvent( QPaintEvent *pe )
{
    QPainter p( this );
    //paintCustomSlider( &p, 0, 0, width(), height(), m_knobX );

    p.setClipRegion(pe->region());
    paintCustomSliderNG( &p, 0, 0, width(), height(), m_knobX, AmarokConfig::showMoodbarInSlider() );
    p.end();

}

void Amarok::TimeSlider::resizeEvent(QResizeEvent * event)
{
    Amarok::Slider::resizeEvent( event );
    The::amarokUrlHandler()->updateTimecodes();
}

void Amarok::TimeSlider::sliderChange( SliderChange change )
{  
    if ( change == SliderValueChange || change == SliderRangeChange )
    {
        int oldKnobX = m_knobX;
        qreal percent = 0.0;
        if ( maximum() > minimum() )
            percent = ((qreal)value()) / ( maximum() - minimum() );
        QRect knob = The::svgHandler()->sliderKnobRect( rect(), percent );
        m_knobX = knob.x();

        if (oldKnobX < m_knobX)
            update( oldKnobX, knob.y(), knob.right() + 1 - oldKnobX, knob.height() );
        else if (oldKnobX > m_knobX)
            update( m_knobX, knob.y(), oldKnobX + knob.width(), knob.height() );
    }
    else
        Amarok::Slider::sliderChange( change ); // calls update()
}

void Amarok::TimeSlider::drawTriangle( const QString& name, int milliSeconds, bool showPopup )
{
    DEBUG_BLOCK
    int sliderHeight = height() - ( m_sliderInsertY * 2 );
    int sliderLeftWidth = sliderHeight / 3;

    // This mess converts the # of seconds into the pixel width value where the triangle should be drawn
    int x_pos = ( ( ( double ) milliSeconds - ( double ) minimum() ) / ( maximum() - minimum() ) ) * ( width() - ( sliderLeftWidth + sliderLeftWidth + m_sliderInsertX * 2 ) );
    debug() << "drawing triangle at " << x_pos;
    BookmarkTriangle * tri = new BookmarkTriangle( this, milliSeconds, name, showPopup );
    connect( tri, SIGNAL( clicked( int ) ), SLOT( slotTriangleClicked( int ) ) );
    connect( tri, SIGNAL( focused( int ) ), SLOT( slotTriangleFocused( int ) ) );
    m_triangles << tri;
    tri->setGeometry( x_pos + 6 /* to center the point */, 1 /*y*/, 11, 11 ); // 6 = hard coded border width
    tri->show();
}

void Amarok::TimeSlider::slotTriangleClicked( int seconds )
{
    emit sliderReleased( seconds );
}

void Amarok::TimeSlider::slotTriangleFocused( int seconds )
{
    QList<BookmarkTriangle *>::iterator i;
    for( i = m_triangles.begin(); i != m_triangles.end(); ++i ){
         if( (*i)->getTimeValue() != seconds )
             (*i)->hidePopup();
    }
}

void Amarok::TimeSlider::clearTriangles()
{
    DEBUG_BLOCK
    debug() << "number of triangles: " << m_triangles.count();
    qDeleteAll( m_triangles );
    debug() << "deleted them all...";
    m_triangles.clear();
}

void Amarok::TimeSlider::mousePressEvent( QMouseEvent *event )
{
    if( !The::engineController()->phononMediaObject()->isSeekable() )
        return; // Eat the event,, it's not possible to seek
    Amarok::Slider::mousePressEvent( event );
}

bool Amarok::TimeSlider::event( QEvent * event )
{
    if( event->type() == QEvent::ToolTip )
    {
        // Make a QHelpEvent out of this
        QHelpEvent * helpEvent = dynamic_cast<QHelpEvent *>( event );
        if( helpEvent )
        {
            // Update tooltip to show the current track time position
            const int trackPosition = The::engineController()->trackPosition();
            setToolTip( i18n( "Time position: %1", Meta::secToPrettyTime( trackPosition ) ) );
        }
    }

    return QWidget::event( event );
}


#include "SliderWidget.moc"
