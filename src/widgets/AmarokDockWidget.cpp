/****************************************************************************************
 * Copyright (c) 2009 Nikolaj Hald Nielsen <nhn@kde.org>                                *
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


#include "AmarokDockWidget.h"
#include "core/support/Debug.h"

AmarokDockWidget::AmarokDockWidget( const QString & title, QWidget * parent, Qt::WindowFlags flags )
    : QDockWidget( title, parent, flags )
    , m_polished( false )
{
    m_dummyTitleBarWidget = new QWidget();
    connect( this, SIGNAL( visibilityChanged( bool ) ), SLOT( slotVisibilityChanged( bool ) ) );
}

void AmarokDockWidget::closeEvent( QCloseEvent* event )
{
    QDockWidget::closeEvent( event );
    emit( layoutChanged() );
}

void AmarokDockWidget::resizeEvent( QResizeEvent* event )
{
    QWidget::resizeEvent( event );
    emit( layoutChanged() );
}

void AmarokDockWidget::showEvent( QShowEvent* event )
{
    QWidget::showEvent( event );
    emit( layoutChanged() );
}

void AmarokDockWidget::hideEvent( QHideEvent* event )
{
    QWidget::hideEvent( event );
    emit( layoutChanged() );
}

void AmarokDockWidget::slotVisibilityChanged( bool visible )
{
    // DEBUG_BLOCK
    if( visible )
        ensurePolish();
}

void AmarokDockWidget::ensurePolish()
{
    if( !m_polished )
    {
        polish();
        m_polished = true;
    }
}

void AmarokDockWidget::setMovable( bool movable )
{

    if( movable )
    {
        const QFlags<QDockWidget::DockWidgetFeature> features = QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable | QDockWidget::DockWidgetClosable;
        setTitleBarWidget( 0 );
        setFeatures( features );
    }
    else
    {
        const QFlags<QDockWidget::DockWidgetFeature> features = QDockWidget::NoDockWidgetFeatures;
        setTitleBarWidget( m_dummyTitleBarWidget );
        setFeatures( features );
    }


}


#include "AmarokDockWidget.moc"
