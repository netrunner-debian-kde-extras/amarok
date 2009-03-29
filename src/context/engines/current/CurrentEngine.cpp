/***************************************************************************
 * copyright            : (C) 2007 Leo Franchi <lfranchi@gmail.com>        *
 **************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "CurrentEngine.h"

#include "Amarok.h"
#include "ContextView.h"
#include "Debug.h"
#include "EngineController.h"
#include "collection/Collection.h"
#include "collection/CollectionManager.h"
#include "meta/MetaUtility.h"
#include "meta/capabilities/SourceInfoCapability.h"

#include <QVariant>

using namespace Context;

CurrentEngine::CurrentEngine( QObject* parent, const QList<QVariant>& args )
    : DataEngine( parent )
    , ContextObserver( ContextView::self() )
    , m_coverWidth( 0 )
    , m_requested( true )
	, m_qm( 0 )
	, m_qmTracks( 0 )
	, m_qmFavTracks( 0 )
    , m_currentArtist( 0 )
{
    DEBUG_BLOCK
    Q_UNUSED( args )

    m_sources << "current" << "albums";

    m_timer = new QTimer(this);
    connect( m_timer, SIGNAL( timeout() ), this, SLOT( stoppedState() ) );

    update();
}

CurrentEngine::~CurrentEngine()
{
    DEBUG_BLOCK
}

QStringList CurrentEngine::sources() const
{
    DEBUG_BLOCK

    return m_sources; // we don't have sources, if connected, it is enabled.
}

bool CurrentEngine::sourceRequestEvent( const QString& name )
{
    DEBUG_BLOCK
    Q_UNUSED( name );

    removeAllData( name );
    setData( name, QVariant() );
    update();
    m_requested = true;

    return true;
}

void CurrentEngine::message( const ContextState& state )
{
    DEBUG_BLOCK
    
    if( state == Current )
    {
        update();
        m_timer->stop();
    }
    else if( state == Home )
    {
        if( m_currentTrack )
        {
            unsubscribeFrom( m_currentTrack );
            if( m_currentTrack->album() )
                unsubscribeFrom( m_currentTrack->album() );
        }        
        m_timer->start( 1000 );
    }
}

void
CurrentEngine::stoppedState()
{
    DEBUG_BLOCK

    m_timer->stop();
    removeAllData( "current" );
    setData( "current", "notrack", i18n( "No track playing") );
    removeAllData( "albums" );
    m_currentArtist = 0;

    // Collect data for the recently added albums
    setData( "albums", "headerText", QVariant( i18n( "Recently added albums" ) ) );
    
    Amarok::Collection *coll = CollectionManager::instance()->primaryCollection();
    if( m_qm )
		m_qm->reset();
	else
    	m_qm = coll->queryMaker();
    m_qm->setQueryType( QueryMaker::Album );
    m_qm->excludeFilter( Meta::valAlbum, QString(), true, true ); 
    m_qm->orderBy( Meta::valCreateDate, true );
    m_qm->limitMaxResultSize( 5 );
    m_albums.clear();
    
    connect( m_qm, SIGNAL( newResultReady( QString, Meta::AlbumList ) ),
            SLOT( resultReady( QString, Meta::AlbumList ) ), Qt::QueuedConnection );
    connect( m_qm, SIGNAL( queryDone() ), SLOT( setupAlbumsData() ) );
    
    m_qm->run();

    // Get the latest tracks played:

    if( m_qmTracks )
		m_qmTracks->reset();
	else
    	m_qmTracks = coll->queryMaker();
    m_qmTracks->setQueryType( QueryMaker::Track );
    m_qmTracks->excludeFilter( Meta::valTitle, QString(), true, true );
    m_qmTracks->orderBy( Meta::valLastPlayed, true );
    m_qmTracks->limitMaxResultSize( 5 );
    
    m_latestTracks.clear();

    connect( m_qmTracks, SIGNAL( newResultReady( QString, Meta::TrackList ) ),
             SLOT( resultReady( QString, Meta::TrackList ) ), Qt::QueuedConnection );
    connect( m_qmTracks, SIGNAL( queryDone() ), SLOT( setupTracksData() ) );

    m_qmTracks->run();

    // Get the favorite tracks:

    if( m_qmFavTracks )
		m_qmFavTracks->reset();
	else
    	m_qmFavTracks = coll->queryMaker();
    m_qmFavTracks->setQueryType( QueryMaker::Track );
    m_qmFavTracks->excludeFilter( Meta::valTitle, QString(), true, true );
    m_qmFavTracks->orderBy( Meta::valScore, true );
    m_qmFavTracks->limitMaxResultSize( 5 );

    m_qmFavTracks->run();

    connect( m_qmFavTracks, SIGNAL( newResultReady( QString, Meta::TrackList ) ),
             SLOT( resultReady( QString, Meta::TrackList ) ), Qt::QueuedConnection );
    connect( m_qmFavTracks, SIGNAL( queryDone() ), SLOT( setupTracksData() ) );
}

void CurrentEngine::metadataChanged( Meta::AlbumPtr album )
{
    DEBUG_BLOCK
    const int width = 156;
    setData( "current", "albumart", album->image( width ) );
}

void
CurrentEngine::metadataChanged( Meta::TrackPtr track )
{
    QVariantMap trackInfo = Meta::Field::mapFromTrack( track );
    setData( "current", "current", trackInfo );
}

void CurrentEngine::update()
{
    DEBUG_BLOCK

    if ( m_currentTrack )
    {
        unsubscribeFrom( m_currentTrack );
        if ( m_currentTrack->album() )
            unsubscribeFrom( m_currentTrack->album() );
    }

    m_currentTrack = The::engineController()->currentTrack();
    
    if( !m_currentTrack )
        return;
    
    subscribeTo( m_currentTrack );

    QVariantMap trackInfo = Meta::Field::mapFromTrack( m_currentTrack );

    //const int width = coverWidth(); // this is always == 0, someone needs to setCoverWidth()
    const int width = 156; // workaround to make the art less grainy. 156 is the width of the nocover image
                          // there is no way to resize the currenttrack applet at this time, so this size
                          // will always look good.
    if( m_currentTrack->album() )
        subscribeTo( m_currentTrack->album() );
    
    removeAllData( "current" );
        
    if( m_currentTrack->album() )
    {
        //add a source info emblem ( if available ) to the cover
        QPixmap art = m_currentTrack->album()->image( width );
        setData( "current", "albumart",  QVariant( art ) );
     }
    else
        setData( "current", "albumart", QVariant( QPixmap() ) );

    setData( "current", "current", trackInfo );

    Meta::SourceInfoCapability *sic = m_currentTrack->as<Meta::SourceInfoCapability>();
    if( sic )
    {
        //is the source defined
        const QString source = sic->sourceName();
        if( !source.isEmpty() )
            setData( "current", "source_emblem",  QVariant( sic->emblem() ) );

        delete sic;
    }
    else
        setData( "current", "source_emblem",  QVariant( QPixmap() ) );

    //generate data for album applet
    Meta::ArtistPtr artist = m_currentTrack->artist();
    
    //We need to update the albums data even if the artist is the same, since the current track is
    //most likely different, and thus the rendering of the albums applet should change
    if( artist )
    {
        m_currentArtist = artist;
        removeAllData( "albums" );
        Meta::AlbumList albums = artist->albums();
        setData( "albums", "headerText", QVariant( i18n( "Albums by %1", artist->name() ) ) );

        if( albums.count() == 0 )
        {
            //try searching the collection as we might be dealing with a non local track
            Amarok::Collection *coll = CollectionManager::instance()->primaryCollection();
            m_qm = coll->queryMaker();
            m_qm->setQueryType( QueryMaker::Album );
            m_qm->addMatch( artist );

            m_albums.clear();

            connect( m_qm, SIGNAL( newResultReady( QString, Meta::AlbumList ) ),
                    SLOT( resultReady( QString, Meta::AlbumList ) ), Qt::QueuedConnection );
            connect( m_qm, SIGNAL( queryDone() ), SLOT( setupAlbumsData() ) );
            m_qm->run();

        }
        else
        {
            m_albums.clear();
            m_albums << albums;
            setupAlbumsData();
        }
    }
}

void
CurrentEngine::setupAlbumsData()
{
    QVariant v;
    v.setValue( m_albums );
    setData( "albums", "albums", v );
}

void
CurrentEngine::setupTracksData()
{
    DEBUG_BLOCK

    QVariant v;
    if( sender() == m_qmTracks )
    {
        v.setValue( m_latestTracks );
        setData( "current", "lastTracks", v );
    }
    else if( sender() == m_qmFavTracks )
    {
        v.setValue( m_favoriteTracks );
        setData( "current", "favoriteTracks", v );
    }
}

void
CurrentEngine::resultReady( const QString &collectionId, const Meta::AlbumList &albums )
{
    DEBUG_BLOCK
    Q_UNUSED( collectionId )

    m_albums.clear();
    m_albums << albums;
}

void
CurrentEngine::resultReady( const QString &collectionId, const Meta::TrackList &tracks )
{
    DEBUG_BLOCK
    Q_UNUSED( collectionId )
    if( sender() == m_qmTracks )
    {
        m_latestTracks.clear();
        m_latestTracks << tracks;
    }
    else if( sender() == m_qmFavTracks )
    {
        m_favoriteTracks.clear();
        m_favoriteTracks << tracks;
    }
}


#include "CurrentEngine.moc"
