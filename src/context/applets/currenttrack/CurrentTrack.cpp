/*****************************************************************************
 * copyright            : (C) 2007 Leo Franchi <lfranchi@gmail.com>          *
 *                      : (C) 2008 William Viana Soares <vianasw@gmail.com>  *
 *****************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "CurrentTrack.h"

#include "Amarok.h"
#include "App.h"
#include "Debug.h"
#include "EngineController.h"
#include "context/popupdropper/libpud/PopupDropperAction.h"
#include "meta/capabilities/CurrentTrackActionsCapability.h"
#include "meta/MetaUtility.h"
#include "PaletteHandler.h"
#include "SvgHandler.h"
#include <context/widgets/RatingWidget.h>

#include <plasma/theme.h>
#include <plasma/widgets/tabbar.h>

#include <KColorScheme>

#include <QFont>
#include <QGraphicsLinearLayout>
#include <QPainter>


CurrentTrack::CurrentTrack( QObject* parent, const QVariantList& args )
    : Context::Applet( parent, args )
    , m_configLayout( 0 )
    , m_width( 0 )
    , m_aspectRatio( 0.0 )
    , m_rating( -1 )
    , m_trackLength( 0 )
    , m_tracksToShow( 0 )
    , m_tabBar( 0 )
{
    setHasConfigurationInterface( false );
}

CurrentTrack::~CurrentTrack()
{}

void CurrentTrack::init()
{
    DEBUG_BLOCK

    m_theme = new Context::Svg( this );
    m_theme->setImagePath( "widgets/amarok-currenttrack" );
    m_theme->setContainsMultipleImages( true );
 
    m_width = globalConfig().readEntry( "width", 500 );

    QFont labelFont;
    labelFont.setPointSize( labelFont.pointSize() + 1  );

    m_ratingWidget = new RatingWidget( this );
    m_ratingWidget->setSpacing( 2 );

    connect( m_ratingWidget, SIGNAL( ratingChanged( int ) ), SLOT( changeTrackRating( int ) ) );

    m_title        = new QGraphicsSimpleTextItem( this );
    m_artist       = new QGraphicsSimpleTextItem( this );
    m_album        = new QGraphicsSimpleTextItem( this );
    m_score        = new QGraphicsSimpleTextItem( this );
    m_numPlayed    = new QGraphicsSimpleTextItem( this );
    m_playedLast   = new QGraphicsSimpleTextItem( this );
    m_noTrack      = new QGraphicsSimpleTextItem( this );
    m_albumCover   = new QGraphicsPixmapItem    ( this );

    m_scoreIconBox      = new QGraphicsRectItem( this );
    m_numPlayedIconBox  = new QGraphicsRectItem( this );
    m_playedLastIconBox = new QGraphicsRectItem( this );

    m_score->setToolTip( i18n( "Score:" ) );
    m_numPlayed->setToolTip( i18n( "Play Count:" ) );
    m_playedLast->setToolTip( i18nc("a single item (singular)", "Last Played:") );

    m_scoreIconBox->setToolTip( i18n( "Score:" ) );
    m_numPlayedIconBox->setToolTip( i18n( "Play Count:" ) );
    m_playedLastIconBox->setToolTip( i18nc("a single item (singular)", "Last Played:") );

    m_scoreIconBox->setPen( QPen( Qt::NoPen ) );
    m_numPlayedIconBox->setPen( QPen( Qt::NoPen ) );
    m_playedLastIconBox->setPen( QPen( Qt::NoPen ) );
    

    QBrush brush = KColorScheme( QPalette::Active ).foreground( KColorScheme::NormalText );

    m_title->setBrush( brush );
    m_artist->setBrush( brush );
    m_album->setBrush( brush );
    m_score->setBrush( brush );
    m_numPlayed->setBrush( brush );
    m_playedLast->setBrush( brush );
    m_noTrack->setBrush( brush );

    QFont bigFont( labelFont );
    bigFont.setPointSize( bigFont.pointSize() +  2 );
    
    QFont tinyFont( labelFont );
    tinyFont.setPointSize( tinyFont.pointSize() - 4 );

    m_noTrack->setFont( bigFont );
    m_title->setFont( labelFont );
    m_artist->setFont( labelFont );
    m_album->setFont( labelFont );
    
    m_score->setFont( tinyFont );
    m_numPlayed->setFont( tinyFont );
    m_playedLast->setFont( tinyFont );

    // get natural aspect ratio, so we can keep it on resize
    m_theme->resize();
    m_aspectRatio = (qreal)m_theme->size().height() / (qreal)m_theme->size().width();
    resize( m_width, m_aspectRatio );
    
    m_noTrackText = i18n( "No track playing" );
    m_noTrack->hide();
    m_noTrack->setText( m_noTrackText );
    
    m_tabBar = new Plasma::TabBar( this );

    for( int i = 0; i < MAX_PLAYED_TRACKS; i++ )
    {
        m_tracks[i] = new TrackWidget( this );
    }
    m_tabBar->addTab( i18n( "Last played" ) );
    m_tabBar->addTab( i18n( "Favorite tracks" ) );

    connectSource( "current" );
    connect( m_tabBar, SIGNAL( currentChanged( int ) ), this, SLOT( tabChanged( int ) ) );
    connect( dataEngine( "amarok-current" ), SIGNAL( sourceAdded( const QString& ) ), this, SLOT( connectSource( const QString& ) ) );
    connect( The::paletteHandler(), SIGNAL( newPalette( const QPalette& ) ), SLOT(  paletteChanged( const QPalette &  ) ) );
}

void
CurrentTrack::connectSource( const QString &source )
{
    if( source == "current" )
    {
        dataEngine( "amarok-current" )->connectSource( source, this );
        dataUpdated( source, dataEngine("amarok-current" )->query( "current" ) ); // get data initally
    }
}

void CurrentTrack::changeTrackRating( int rating )
{
    DEBUG_BLOCK
    Meta::TrackPtr track = The::engineController()->currentTrack();
    track->setRating( rating );
    debug() << "change rating to: " << rating;
}

QList<QAction*>
CurrentTrack::contextualActions()
{
    QList<QAction*> actions;

    Meta::TrackPtr track = The::engineController()->currentTrack();
    
    if( !track )
        return actions;
    
    Meta::AlbumPtr album = track->album();

    if( album )
    {
        Meta::CustomActionsCapability *cac = album->as<Meta::CustomActionsCapability>();
        if( cac )
        {
            QList<PopupDropperAction *> pudActions = cac->customActions();
             
            foreach( PopupDropperAction *action, pudActions )
                actions.append( action );
        }
    }

    return actions;
}

void CurrentTrack::constraintsEvent( Plasma::Constraints constraints )
{
    Q_UNUSED( constraints )
    DEBUG_BLOCK

    prepareGeometryChange();

    /*if( constraints & Plasma::SizeConstraint )
         m_theme->resize(size().toSize());*/

    //bah! do away with trying to get postions from an svg as this is proving wildly inaccurate
    const qreal margin = 14.0;
    const qreal albumWidth = size().toSize().height() - 2.0 * margin;
    resizeCover( m_bigCover, margin, albumWidth );

    const qreal labelX = albumWidth + margin + 14.0;
    const qreal labelWidth = 15;
    const qreal textX = labelX + labelWidth + margin;

    const qreal textHeight = ( ( size().toSize().height() - 3 * margin )  / 5.0 ) ;
    const qreal textWidth = size().toSize().width() - ( textX + margin + 53 ); // add 53 to ensure room for small label + their text
    

    // here we put all of the text items into the correct locations    
    
    m_title->setPos( QPointF( textX, margin + 2 ) );
    m_artist->setPos(  QPointF( textX, margin * 2 + textHeight + 2  ) );
    m_album->setPos( QPointF( textX, margin * 3 + textHeight * 2.0 + 2 ) );

    const int addLabelOffset = contentsRect().width() - 25;

    m_score->setPos( QPointF( addLabelOffset, margin + 2 ) );
    m_numPlayed->setPos( QPointF( addLabelOffset, margin * 2 + textHeight + 2 ) );
    m_playedLast->setPos( QPointF( addLabelOffset, margin * 3 + textHeight * 2.0 + 2 ) );

    m_scoreIconBox->setRect( addLabelOffset - 25, margin, 25, 22 );
    m_numPlayedIconBox->setRect( addLabelOffset - 25, margin * 2 + textHeight, 25, 22 );
    m_playedLastIconBox->setRect( addLabelOffset - 25, margin * 3 + textHeight * 2.0, 25, 22 );

    const QString title = m_currentInfo[ Meta::Field::TITLE ].toString();
    const QString artist = m_currentInfo.contains( Meta::Field::ARTIST ) ? m_currentInfo[ Meta::Field::ARTIST ].toString() : QString();
    const QString album = m_currentInfo.contains( Meta::Field::ALBUM ) ? m_currentInfo[ Meta::Field::ALBUM ].toString() : QString();
    const QString lastPlayed = m_currentInfo.contains( Meta::Field::LAST_PLAYED ) ? Amarok::conciseTimeSince( m_currentInfo[ Meta::Field::LAST_PLAYED ].toUInt() ) : QString();

    const QFont textFont = shrinkTextSizeToFit( title, QRectF( 0, 0, textWidth, textHeight ) );
    const QFont labeFont = textFont;
    QFont tinyFont( textFont );

    if ( tinyFont.pointSize() > 7 )
        tinyFont.setPointSize( tinyFont.pointSize() - 2 );
    else
        tinyFont.setPointSize( 5 );

    m_maxTextWidth = textWidth;
    //m_maxTextWidth = size().toSize().width() - m_title->pos().x() - 14;
    
    m_title->setFont( textFont );
    m_artist->setFont( textFont );
    m_album->setFont( textFont );
    
    m_score->setFont( textFont );
    m_numPlayed->setFont( textFont );
    m_playedLast->setFont( textFont );

    m_artist->setText( truncateTextToFit( artist, m_artist->font(), QRectF( 0, 0, textWidth, 30 ) ) );
    m_title->setText( truncateTextToFit( title, m_title->font(), QRectF( 0, 0, textWidth, 30 ) ) );    
    m_album->setText( truncateTextToFit( album, m_album->font(), QRectF( 0, 0, textWidth, 30 ) ) );
    
    if( !m_lastTracks.isEmpty() )
    {                
        m_tracksToShow = qMin( m_lastTracks.count(), ( int )( ( contentsRect().height() - 30 ) / ( textHeight * 1.2 ) ) );

        QFontMetrics fm( m_tabBar->font() );
        m_tabBar->resize( QSizeF( contentsRect().width() - margin * 2 - 2, m_tabBar->size().height() * 0.7 ) ); // Why is the height factor ignored?
        m_tabBar->setPos( size().width() / 2 - m_tabBar->size().width() / 2 - 1, 10 );
        m_tabBar->show();
        
        for( int i = 0; i < m_tracksToShow; i++ )
        {
            m_tracks[i]->resize( contentsRect().width() - margin * 2, textHeight * 1.2 );
            m_tracks[i]->setPos( ( rect().width() - m_tracks[i]->boundingRect().width() ) / 2, textHeight * 1.2 * i + 43 );
        }
    }        
    else if( !m_noTrackText.isEmpty() )
    {
        m_noTrack->setText( truncateTextToFit( m_noTrackText, m_noTrack->font(), QRectF( 0, 0, textWidth, 30 ) ) );
        m_noTrack->setPos( size().toSize().width() / 2 - m_noTrack->boundingRect().width() / 2,
                       size().toSize().height() / 2  - 30 );
    }

    m_ratingWidget->setMinimumSize( contentsRect().width() / 5, textHeight );
    m_ratingWidget->setMaximumSize( contentsRect().width() / 5, textHeight );
    
    //lets center this widget

    const int availableSpace = contentsRect().width() - labelX;
    const int offsetX = ( availableSpace - ( contentsRect().width() / 5 ) ) / 2;

    m_ratingWidget->setPos( labelX + offsetX, margin * 4.0 + textHeight * 3.0 - 5.0 );    
    
    dataEngine( "amarok-current" )->setProperty( "coverWidth", m_theme->elementRect( "albumart" ).size().width() );
}

void CurrentTrack::dataUpdated( const QString& name, const Plasma::DataEngine::Data& data )
{
    DEBUG_BLOCK
    Q_UNUSED( name );

    if( data.size() == 0 ) 
        return;

    QRect textRect( 0, 0, m_maxTextWidth, 30 );

    m_noTrackText = data[ "notrack" ].toString();
    m_lastTracks = data[ "lastTracks" ].value<Meta::TrackList>();
    m_favoriteTracks = data[ "favoriteTracks" ].value<Meta::TrackList>();
    
    if( !m_lastTracks.isEmpty() )
    {
        
        Meta::TrackList tracks;
        if( m_tabBar->currentIndex() == 0 )
            tracks = m_lastTracks;
        else
            tracks = m_favoriteTracks;
        int i = 0;
        foreach( Meta::TrackPtr track, tracks )
        {
            m_tracks[i]->setTrack( track );
            i++;
        }
        
        updateConstraints();
        update();
        return;
    }
    else if( !m_noTrackText.isEmpty() )
    {
        QRect rect( 0, 0, size().toSize().width(), 30 );
        m_noTrack->setText( truncateTextToFit( m_noTrackText, m_noTrack->font(), rect ) );
        m_noTrack->setPos( size().toSize().width() / 2 - m_noTrack->boundingRect().width() / 2,
                       size().toSize().height() / 2  - 30 );
        update();
        return;
    }

    m_noTrack->setText( QString() );
    m_lastTracks.clear();
    m_favoriteTracks.clear();
    
    m_currentInfo = data[ "current" ].toMap();
    m_title->setText( truncateTextToFit( m_currentInfo[ Meta::Field::TITLE ].toString(), m_title->font(), textRect ) );
    
    const QString artist = m_currentInfo.contains( Meta::Field::ARTIST ) ? m_currentInfo[ Meta::Field::ARTIST ].toString() : QString();
    m_artist->setText( truncateTextToFit( artist, m_artist->font(), textRect ) );
    
    const QString album = m_currentInfo.contains( Meta::Field::ALBUM ) ? m_currentInfo[ Meta::Field::ALBUM ].toString() : QString();
    m_album->setText( truncateTextToFit( album, m_album->font(), textRect ) );
    
    m_rating = m_currentInfo[ Meta::Field::RATING ].toInt();
    
    const QString score = QString::number( m_currentInfo[ Meta::Field::SCORE ].toInt() );
    m_score->setText( score );
    
    m_trackLength = m_currentInfo[ Meta::Field::LENGTH ].toInt();

    QString playedLast = Amarok::conciseTimeSince( m_currentInfo[ Meta::Field::LAST_PLAYED ].toUInt() );
    QString playedLastVerbose =  Amarok::verboseTimeSince( m_currentInfo[ Meta::Field::LAST_PLAYED ].toUInt() );
    m_playedLast->setText( playedLast  );

    QString numPlayed = m_currentInfo[ Meta::Field::PLAYCOUNT ].toString();
    m_numPlayed->setText( numPlayed );
    
    m_ratingWidget->setRating( m_rating );

    m_score->setToolTip( i18n( "Score:" ) + ' ' + score );
    m_numPlayed->setToolTip( i18n( "Playcount:" ) + ' ' + numPlayed );
    m_playedLast->setToolTip( i18nc("a single item (singular)", "Last Played:") + ' ' + playedLastVerbose );

    m_scoreIconBox->setToolTip( i18n( "Score:" ) + ' ' + score );
    m_numPlayedIconBox->setToolTip( i18n( "Playcount:" ) + ' ' + numPlayed );
    m_playedLastIconBox->setToolTip( i18nc("a single item (singular)", "Last Played:") + ' ' + playedLastVerbose );

    //scale pixmap on demand
    //store the big cover : avoid blur when resizing the applet
    m_bigCover = data[ "albumart" ].value<QPixmap>();
//     m_sourceEmblemPixmap = data[ "source_emblem" ].value<QPixmap>();


    if( !resizeCover( m_bigCover, 14.0, size().toSize().height() - 28.0 ) )
    {
        warning() << "album cover of current track is null, did you forget to call Meta::Album::image?";
    }
    // without that the rating doesn't get update for a playing track
    update();
}


QSizeF 
CurrentTrack::sizeHint( Qt::SizeHint which, const QSizeF & constraint) const
{
    Q_UNUSED( which )

    if( constraint.height() == -1 && constraint.width() > 0 ) // asking height for given width basically
        return QSizeF( constraint.width(), 150 );
//         return QSizeF( constraint.width(), m_aspectRatio * constraint.width() );

    return constraint;
}

void CurrentTrack::paintInterface( QPainter *p, const QStyleOptionGraphicsItem *option, const QRect &contentsRect )
{
    Q_UNUSED( option );

    //bail out if there is no room to paint. Prevents crashes and really there is no sense in painting if the
    //context view has been minimized completely
    if( ( contentsRect.width() < 20 ) || ( contentsRect.height() < 20 ) )
    {
        foreach ( QGraphicsItem * childItem, QGraphicsItem::children() )
            childItem->hide();
        return;
    }
    else
    {
        foreach ( QGraphicsItem * childItem, QGraphicsItem::children () )
            childItem->show();        
    }    

    if( !m_lastTracks.isEmpty() )
    {
        QList<QGraphicsItem*> children = QGraphicsItem::children();
        foreach ( QGraphicsItem *childItem, children )
            childItem->hide();
        m_tabBar->show();
        for( int i = 0; i < m_tracksToShow; i++)
            m_tracks[i]->show();
        return;
    }
    else if( !m_noTrack->text().isEmpty() )
    {
        QList<QGraphicsItem*> children = QGraphicsItem::children();
        foreach ( QGraphicsItem *childItem, children )
            childItem->hide();
        m_noTrack->show();
        return;
    }
    else
    {
        m_tabBar->hide();
        m_noTrack->hide();
        for( int i = 0; i < MAX_PLAYED_TRACKS; i++)
            m_tracks[i]->hide();
    }
    
    const qreal margin = 14.0;
    const qreal albumWidth = size().toSize().height() - 2.0 * margin;

    const qreal labelX = albumWidth + margin + 14.0;

    const qreal textHeight = ( ( size().toSize().height() - 3 * margin )  / 5.0 );

    p->save();
    
    
    Meta::TrackPtr track = The::engineController()->currentTrack();

    // Only show the ratings widget if the current track is in the collection
    if( track && track->inCollection() )
        m_ratingWidget->show();
    else
        m_ratingWidget->hide();

    //don't paint this until we have something better looking that also works with non square covers
    /*if( track && track->album() && track->album()->hasImage() )
        m_theme->paint( p, QRect( margin - 5, margin, albumWidth + 12, albumWidth ), "cd-box" );*/

    const int lineSpacing = margin + textHeight;
    const int line1Y = margin + 1;
    const int line2Y = line1Y + lineSpacing;
    const int line3Y = line2Y + lineSpacing;
    
    m_theme->paint( p, QRectF( labelX, line1Y, 23, 23 ), "track_png_23" );
    m_theme->paint( p, QRectF( labelX, line2Y , 23, 23 ), "artist_png_23" );
    m_theme->paint( p, QRectF( labelX, line3Y, 23, 23 ), "album_png_23" );


    const int label2X = contentsRect.width() - 53;
    
    m_theme->paint( p, QRectF( label2X, line1Y, 23, 23 ), "score_png_23" );
    m_theme->paint( p, QRectF( label2X, line2Y, 23, 23 ), "times-played_png_23" );
    m_theme->paint( p, QRectF( label2X, line3Y, 23, 23 ), "last-time_png_23" );

    p->restore();

    // TODO get, and then paint, album pixmap
    // constraintsEvent();
}

void CurrentTrack::showConfigurationInterface()
{}

void CurrentTrack::configAccepted() // SLOT
{}


bool CurrentTrack::resizeCover( QPixmap cover,qreal margin, qreal width )
{
    const int borderWidth = 5;
    
    if( !cover.isNull() )
    {

        width -= borderWidth * 2;
        
        //QSizeF rectSize = m_theme->elementRect( "albumart" ).size();
        //QPointF rectPos = m_theme->elementRect( "albumart" ).topLeft();
        qreal size = width;
        qreal pixmapRatio = (qreal)cover.width()/size;

        qreal moveByX = 0.0;
        qreal moveByY = 0.0;

        //center the cover : if the cover is not squared, we get the missing pixels and center
        if( cover.height()/pixmapRatio > width )
        {
            cover = cover.scaledToHeight( size, Qt::SmoothTransformation );
            moveByX = qAbs( cover.rect().width() - cover.rect().height() ) / 2.0;
        }
        else
        {
            cover = cover.scaledToWidth( size, Qt::SmoothTransformation );
            moveByY = qAbs( cover.rect().height() - cover.rect().width() ) / 2.0;
        }
        m_albumCover->setPos( margin + moveByX, margin + moveByY );
//         m_sourceEmblem->setPos( margin + moveByX, margin + moveByY );


        QPixmap coverWithBorders = The::svgHandler()->addBordersToPixmap( cover, borderWidth, m_album->text(), true );

        
        m_albumCover->setPixmap( coverWithBorders );
//         m_sourceEmblem->setPixmap( m_sourceEmblemPixmap );
        return true;
    }
    return false;
}

void CurrentTrack::paletteChanged( const QPalette & palette )
{
    DEBUG_BLOCK

    m_title->setBrush( palette.text() );
    m_artist->setBrush( palette.text() );
    m_album->setBrush( palette.text() );
    m_score->setBrush( palette.text() );
    m_numPlayed->setBrush( palette.text() );
    m_playedLast->setBrush( palette.text() );
    m_noTrack->setBrush( palette.text() );
}

void
CurrentTrack::tabChanged( int index )
{
    Meta::TrackList tracks;
    if( index == 0 )
        tracks = m_lastTracks;
    else
        tracks = m_favoriteTracks;
    
    int i = 0;    
    foreach( Meta::TrackPtr track, tracks )
    {
        m_tracks[i]->hide();
        m_tracks[i]->setTrack( track );
        i++;
    }
    for( i = 0; i < m_tracksToShow; i++ )
        m_tracks[i]->show();
    
}

#include "CurrentTrack.moc"

