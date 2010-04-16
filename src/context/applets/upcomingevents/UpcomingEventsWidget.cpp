/****************************************************************************************
 * Copyright (c) 2009-2010 Ludovic Deveaux <deveaux.ludovic31@gmail.com>                *
 * Copyright (c) 2010 Hormiere Guillaume <hormiere.guillaume@gmail.com>                 *
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

#include "UpcomingEventsWidget.h"
#include "SvgHandler.h"

// Kde include
#include <KDateTime>
#include <KIcon>
#include <KIO/Job>
#include <KLocale>
#include <KLocalizedString>
#include <KUrl>

// Qt include
#include <QLabel>
#include <QString>
#include <QGridLayout>
#include <QGraphicsProxyWidget>
#include <QGraphicsScene>
#include <QDesktopServices>

#define NBR_MAX_PARTICIPANT 5

UpcomingEventsWidget::UpcomingEventsWidget( QWidget *parent ): QWidget( parent )
{
    m_image = new QLabel( this );
    m_participants = new QLabel( this );
    m_date = new QLabel( this );
    m_name = new QLabel( this );
    m_url = new QLabel( this );
    m_location = new QLabel( this );
    m_frame = new QFrame( this );

    m_frame->setFrameStyle( QFrame::HLine );
    m_frame->setAutoFillBackground( false );
    m_frame->setMaximumWidth( size().width() );

    m_participants->setWordWrap( true );
    m_name->setWordWrap( true );

    m_layout = new QGridLayout;
    m_layout->addWidget( m_image, 0, 0, 5, 1, Qt::AlignCenter );
    m_layout->addWidget( m_name, 0, 1, 1, 1 );
    m_layout->addWidget( m_participants, 1, 1, 1, 1 );
    m_layout->addWidget( m_location, 2, 1, 1, 1 );
    m_layout->addWidget( m_date, 3, 1, 1, 1 );
    m_layout->addWidget( m_url, 4, 1, 1, 1 );
    m_layout->addWidget( m_frame, 6, 0, 1, 2 );
    m_layout->setColumnMinimumWidth( 0, 140 );
    m_layout->setHorizontalSpacing( 5 );
    m_layout->setAlignment( Qt::AlignLeft );
    m_layout->setSizeConstraint( QLayout::SetMinimumSize );
    setLayout( m_layout );

    m_url->setOpenExternalLinks( true );
    m_url->setTextInteractionFlags( Qt::TextBrowserInteraction );
}

UpcomingEventsWidget::~UpcomingEventsWidget()
{
    delete m_layout;
    delete m_frame;
    delete m_image;
    delete m_participants;
    delete m_date;
    delete m_name;
    delete m_url;
}

QLabel *
UpcomingEventsWidget::date() const
{
    return m_date;
}

QLabel *
UpcomingEventsWidget::image() const
{
    return m_image;
}

QLabel *
UpcomingEventsWidget::name() const
{
    return m_name;
}

QLabel *
UpcomingEventsWidget::location() const
{
    return m_location;
}

QLabel *
UpcomingEventsWidget::participants() const
{
    return m_participants;
}

QLabel *
UpcomingEventsWidget::url() const
{
    return m_url;
}

void
UpcomingEventsWidget::setImage( const KUrl &url )
{
    m_image->setText( i18n("Loading picture...") );
    KJob* job = KIO::storedGet( url, KIO::NoReload, KIO::HideProgressInfo );
    connect( job, SIGNAL( result( KJob * ) ), SLOT( loadImage( KJob * ) ) );
}

void
UpcomingEventsWidget::loadImage( KJob *job ) // SLOT
{
    if( job )
    {
        KIO::StoredTransferJob *const storedJob = static_cast< KIO::StoredTransferJob * >( job );
        QPixmap image;
        QByteArray buffer = storedJob->data();
        if( !buffer.isEmpty() )
        {
            image.loadFromData( buffer );
            m_image->setPixmap( The::svgHandler()->addBordersToPixmap( image, 5, QString(), true ) );
        }
        else
        {
            m_image->setPixmap( KIcon( "weather-none-available" ).pixmap( 128 ) );
        }
    }
}

void
UpcomingEventsWidget::setParticipants( const QString &participants )
{
    QFont font;
    if( participants == "No other participants" )
    {
        m_participants->setText( participants );
        font.setItalic( true );
        m_participants->setFont( font );
    }
    else
    {
        QStringList listbuff = participants.split(" - ");
        QString buffer( "" );
        QString toolTipText;
        for( int i = 0; i < listbuff.size(); i++ )
        {
            toolTipText += listbuff.at( i );
            if( i % 3 == 0 && i )
            {
                toolTipText += "\n";
            }
            else
            {
                if( i < listbuff.size() - 1 ) toolTipText += " - ";
            }
                        
            if( i < NBR_MAX_PARTICIPANT )
            {
                buffer += listbuff.at( i );
                if( i < NBR_MAX_PARTICIPANT - 1 && i < listbuff.size()-1 ) buffer += " - ";
            }
        }
        if( listbuff.size() > NBR_MAX_PARTICIPANT )
        {
            buffer += "...";
        }
        m_participants->setToolTip( toolTipText );
        m_participants->setText( buffer );
        font.setItalic( false );
        m_participants->setFont( font );
    }
    m_participants->setAttribute( Qt::WA_TranslucentBackground );
}

void
UpcomingEventsWidget::setDate( const KDateTime &date )
{
    m_date->setText( KGlobal::locale()->formatDateTime( date.toClockTime(), KLocale::FancyLongDate ) );
    m_date->setAttribute( Qt::WA_TranslucentBackground );
}

void
UpcomingEventsWidget::setLocation( const QString &location )
{
    m_location->setText( location );
    m_location->setAttribute( Qt::WA_TranslucentBackground );
}

void
UpcomingEventsWidget::setName( const QString &name )
{
    QFont nameFont;
    nameFont.setBold( true );
    nameFont.setPointSize( m_name->font().pointSize() + 2 );
    m_name->setFont( nameFont );
    m_name->setText( name );
    m_name->setAttribute( Qt::WA_TranslucentBackground );
}

void
UpcomingEventsWidget::setUrl( const KUrl &url )
{
    m_url->setText( "<html><body><a href=\"" + url.prettyUrl() + "\"><u>" + i18n( "Event website" ) + "</u></a></body></html>" );
    m_url->setAttribute( Qt::WA_TranslucentBackground );
}

#include "UpcomingEventsWidget.moc"
