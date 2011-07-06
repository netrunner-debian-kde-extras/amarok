/****************************************************************************************
 * Copyright (c) 2007 Nikolaj Hald Nielsen <nhn@kde.org>                                *
 * Copyright (c) 2007 Adam Pigg <adam@piggz.co.uk>                                      *
 * Copyright (c) 2007 Casey Link <unnamedrambler@gmail.com>                             *
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

#define DEBUG_PREFIX "AmpacheServiceQueryMaker"

#include "AmpacheServiceQueryMaker.h"

#include "AmpacheMeta.h"
#include "core/support/Amarok.h"
#include "core/support/Debug.h"
#include "core-impl/collections/support/MemoryMatcher.h"

#include <QDomDocument>

using namespace Collections;

struct AmpacheServiceQueryMaker::Private
{
    enum QueryType { NONE, TRACK, ARTIST, ALBUM, COMPOSER, YEAR, GENRE, CUSTOM };
    QueryType type;
    int maxsize;
    QHash<QLatin1String, KUrl> urls;
};

AmpacheServiceQueryMaker::AmpacheServiceQueryMaker( AmpacheServiceCollection * collection, const QString &server, const QString &sessionId  )
    : DynamicServiceQueryMaker()
    , m_collection( collection )
    , d( new Private )
    , m_server( server )
    , m_sessionId( sessionId )
    , m_dateFilter( -1 )
{
    // DEBUG_BLOCK

    d->type = Private::NONE;
    d->maxsize = 0;
    d->urls.clear();
    m_dateFilter = 0;
    //m_lastArtistFilter = QString(); this one really should survive a reset....
}

AmpacheServiceQueryMaker::~AmpacheServiceQueryMaker()
{
    delete d;
}

void
AmpacheServiceQueryMaker::run()
{
    DEBUG_BLOCK

    if( !d->urls.isEmpty() )
        return;

    //naive implementation, fix this
    //note: we are not handling filtering yet

    //TODO error handling
    if ( d->type == Private::NONE )
        return;

    m_collection->acquireReadLock();

    if (  d->type == Private::ARTIST )
        fetchArtists();
    else if( d->type == Private::ALBUM )
        fetchAlbums();
    else if( d->type == Private::TRACK )
        fetchTracks();

     m_collection->releaseLock();
}

void
AmpacheServiceQueryMaker::abortQuery()
{
}

QueryMaker *
AmpacheServiceQueryMaker::setQueryType( QueryType type )
{
    DEBUG_BLOCK
    switch( type ) {

    case QueryMaker::Artist:
    case QueryMaker::AlbumArtist:
        d->type = Private::ARTIST;
        return this;

    case QueryMaker::Album:
        d->type = Private::ALBUM;
        return this;

    case QueryMaker::Track:
        d->type = Private::TRACK;
        return this;

    case QueryMaker::Genre:
    case QueryMaker::Composer:
    case QueryMaker::Year:
    case QueryMaker::Custom:
    case QueryMaker::Label:
    case QueryMaker::None:
        //TODO: Implement.
        return this;
    }

    return this;
}

QueryMaker *
AmpacheServiceQueryMaker::addMatch( const Meta::ArtistPtr & artist )
{
    DEBUG_BLOCK
    if( m_parentAlbumId.isEmpty() )
    {
        const Meta::ServiceArtist * serviceArtist = dynamic_cast< const Meta::ServiceArtist * >( artist.data() );
        if( serviceArtist )
        {
            m_parentArtistId = QString::number( serviceArtist->id() );
        }
        else
        {
            if( m_collection->artistMap().contains( artist->name() ) )
            {
                serviceArtist = static_cast< const Meta::ServiceArtist* >( m_collection->artistMap().value( artist->name() ).data() );
                m_parentArtistId = QString::number( serviceArtist->id() );
            }
            else
            {
                //hmm, not sure what to do now
            }
        }
        //debug() << "parent id set to: " << m_parentArtistId;
    }
    return this;
}

QueryMaker *
AmpacheServiceQueryMaker::addMatch( const Meta::AlbumPtr & album )
{
    DEBUG_BLOCK
    const Meta::ServiceAlbum * serviceAlbum = dynamic_cast< const Meta::ServiceAlbum * >( album.data() );
    if( serviceAlbum )
    {
        m_parentAlbumId = QString::number( serviceAlbum->id() );
        //debug() << "parent id set to: " << m_parentAlbumId;
        m_parentArtistId.clear();
    }
    else
    {
        if( m_collection->albumMap().contains( album->name() ) )
        {
            serviceAlbum = static_cast< const Meta::ServiceAlbum* >( m_collection->albumMap().value( album->name() ).data() );
            m_parentAlbumId = QString::number( serviceAlbum->id() );
        }
        else
        {
            //hmm, not sure what to do now
        }
    }

    return this;
}

void
AmpacheServiceQueryMaker::fetchArtists()
{
    DEBUG_BLOCK

    //this stuff causes crashes and "whiteouts" and will need to change anyway if the Ampache API is updated to
    // allow multilevel filtering. Hence it is commented out but left in for future reference
    /*if ( ( m_collection->artistMap().values().count() != 0 ) && ( m_artistFilter == m_lastArtistFilter ) ) {
        emit newResultReady( m_collection->artistMap().values() );
        debug() << "no need to fetch artists again! ";
        debug() << "    filter: " << m_artistFilter;
        debug() << "    last filter: " << m_lastArtistFilter;

    }*/
    //else {
        KUrl request( m_server );
        request.addPath( "/server/xml.server.php" );
        request.addQueryItem( "action", "artists" );
        request.addQueryItem( "auth", m_sessionId );

        if ( !m_artistFilter.isEmpty() )
            request.addQueryItem( "filter", m_artistFilter );

        if( m_dateFilter > 0 )
        {
            QDateTime from;
            from.setTime_t( m_dateFilter );
            request.addQueryItem( "add", from.toString( Qt::ISODate ) );
            debug() << "added date filter with time:" <<  from.toString( Qt::ISODate );
        } else
            debug() << "m_dateFilter is:" << m_dateFilter;

        request.addQueryItem( "limit", QString::number( d->maxsize ) ); // set to 0 in reset() so fine to use uncondiationally
        debug() << "Artist url: " << request.url();

        d->urls[QLatin1String("artists")] = request;
        The::networkAccessManager()->getData( request, this,
             SLOT(artistDownloadComplete(KUrl,QByteArray,NetworkAccessManagerProxy::Error)) );
    //}

    m_lastArtistFilter = m_artistFilter;
}

void
AmpacheServiceQueryMaker::fetchAlbums()
{
    DEBUG_BLOCK

    Meta::AlbumList albums;

    if( !m_parentArtistId.isEmpty() )
    {
        albums = matchAlbums( m_collection, m_collection->artistById( m_parentArtistId.toInt() ) );
    }

    if ( albums.count() > 0 )
    {
        emit newResultReady( albums );
        emit queryDone();
    }
    else
    {
        KUrl request( m_server );
        request.addPath( "/server/xml.server.php" );
        request.addQueryItem( "action", "artist_albums" );
        request.addQueryItem( "auth", m_sessionId );

        if( !m_parentArtistId.isEmpty() )
            request.addQueryItem( "filter", m_parentArtistId );

        if( m_dateFilter > 0 )
        {
            QDateTime from;
            from.setTime_t( m_dateFilter );
            request.addQueryItem( "add", from.toString( Qt::ISODate ) );
        }
        request.addQueryItem( "limit", QString::number( d->maxsize ) ); // set to 0 in reset() so fine to use uncondiationally
        debug() << "request url: " << request.url();

        d->urls[QLatin1String("albums")] = request;
        The::networkAccessManager()->getData( request, this,
             SLOT(albumDownloadComplete(KUrl,QByteArray,NetworkAccessManagerProxy::Error)) );
    }
}

void
AmpacheServiceQueryMaker::fetchTracks()
{
    DEBUG_BLOCK

    Meta::TrackList tracks;

    //debug() << "parent album id: " << m_parentAlbumId;

    if( !m_parentAlbumId.isEmpty() )
    {
        AlbumMatcher albumMatcher( m_collection->albumById( m_parentAlbumId.toInt() ) );
        tracks = albumMatcher.match( m_collection->trackMap().values() );
    }
    else if ( !m_parentArtistId.isEmpty() )
    {
        ArtistMatcher artistMatcher( m_collection->artistById( m_parentArtistId.toInt() ) );
        tracks = artistMatcher.match( m_collection->trackMap().values() );
    }

    if( tracks.count() > 0 )
    {
        emit newResultReady( tracks );
        emit queryDone();
    }
    else
    {
        KUrl request( m_server );
        request.addPath( "/server/xml.server.php" );
        request.addQueryItem( "auth", m_sessionId );

        if( !m_parentAlbumId.isEmpty() )
            request.addQueryItem( "action", "album_songs" );
        else if( !m_parentArtistId.isEmpty() )
            request.addQueryItem( "action", "artist_songs" );
        else
            request.addQueryItem( "action", "songs" );

        if( !m_parentAlbumId.isEmpty() )
            request.addQueryItem( "filter", m_parentAlbumId );
        else if( !m_parentArtistId.isEmpty() )
            request.addQueryItem( "filter", m_parentArtistId );
        if( m_dateFilter > 0 )
        {
            QDateTime from;
            from.setTime_t( m_dateFilter );
            request.addQueryItem( "add", from.toString( Qt::ISODate ) );
        }
        debug() << "request url: " << request.url();

        request.addQueryItem( "limit", QString::number( d->maxsize ) );// set to 0 in reset() so fine to use uncondiationally

        d->urls[QLatin1String("tracks")] = request;
        The::networkAccessManager()->getData( request, this,
             SLOT(trackDownloadComplete(KUrl,QByteArray,NetworkAccessManagerProxy::Error)) );
    }
}

void
AmpacheServiceQueryMaker::artistDownloadComplete( const KUrl &url, QByteArray data, NetworkAccessManagerProxy::Error e )
{
    if( d->urls.value(QLatin1String("artists")) != url )
        return;

    d->urls.remove( QLatin1String("artists") );
    if( e.code != QNetworkReply::NoError )
    {
        debug() << "Artist download error:" << e.description;
        return;
    }

    // DEBUG_BLOCK

    Meta::ArtistList artists;

     //so lets figure out what we got here:
    QDomDocument doc( "reply" );
    doc.setContent( data );
    QDomElement root = doc.firstChildElement( "root" );

    // Is this an error, if so we need to 'un-ready' the service and re-authenticate before contiuning
    QDomElement domError = root.firstChildElement( "error" );

    if ( !domError.isNull() )
    {
        debug () << "Error getting Artist List" << domError.text();
        AmpacheService *m_parentService = dynamic_cast< AmpacheService * >( m_collection->service() );
        if ( m_parentService == 0 )
            return;
        else
            m_parentService->reauthenticate();
    }

    QDomNode n = root.firstChild();
    while( !n.isNull() )
    {
        QDomElement e = n.toElement(); // try to convert the node to an element.
        //if ( ! (e.tagName() == "item") )
        //    break;

        QDomElement element = n.firstChildElement( "name" );
        Meta::ServiceArtist * artist = new Meta::AmpacheArtist( element.text(), m_collection->service() );

        int artistId = e.attribute( "id", "0").toInt();

        //debug() << "Adding artist: " << element.text() << " with id: " << artistId;

        artist->setId( artistId );

        Meta::ArtistPtr artistPtr( artist );

        artists.push_back( artistPtr );

        m_collection->acquireWriteLock();
        m_collection->addArtist( artistPtr );
        m_collection->releaseLock();

        n = n.nextSibling();
    }

    emit newResultReady( artists );
    emit queryDone();
}

void
AmpacheServiceQueryMaker::albumDownloadComplete( const KUrl &url, QByteArray data, NetworkAccessManagerProxy::Error e )
{
    if( d->urls.value(QLatin1String("albums")) != url )
        return;

    d->urls.remove( QLatin1String("albums") );
    if( e.code != QNetworkReply::NoError )
    {
        debug() << "Album download error:" << e.description;
        return;
    }

    // DEBUG_BLOCK

    Meta::AlbumList albums;

     //so lets figure out what we got here:
    QDomDocument doc( "reply" );
    doc.setContent( data );
    QDomElement root = doc.firstChildElement( "root" );

    // Is this an error, if so we need to 'un-ready' the service and re-authenticate before contiuning
    QDomElement domError = root.firstChildElement( "error" );

    if( !domError.isNull() )
    {
        debug () << "Error getting Album List" << domError.text();
        AmpacheService *m_parentService = dynamic_cast< AmpacheService * >(m_collection->service());
        if ( m_parentService == 0 )
            return;
        else
            m_parentService->reauthenticate();
    }

    QDomNode n = root.firstChild();
    while( !n.isNull() )
    {
        QDomElement e = n.toElement(); // try to convert the node to an element.
        //if ( ! (e.tagName() == "item") )
        //    break;

        QDomElement element = n.firstChildElement( "name" );

        QString title = element.text();
        if ( title.isEmpty() )
            title = "Unknown";

        int albumId = e.attribute( "id", "0" ).toInt();

        Meta::AmpacheAlbum * album = new Meta::AmpacheAlbum( title );
        album->setId( albumId );


        element = n.firstChildElement( "art" );

        QString coverUrl = element.text();
        album->setCoverUrl( coverUrl );

        Meta::AlbumPtr albumPtr( album );

        //debug() << "Adding album: " <<  title;
        //debug() << "   Id: " <<  albumId;
        //debug() << "   Cover url: " <<  coverUrl;

        m_collection->acquireWriteLock();
        m_collection->addAlbum( albumPtr );
        m_collection->releaseLock();

        element = n.firstChildElement( "artist" );

        Meta::ArtistPtr artistPtr = m_collection->artistById( m_parentArtistId.toInt() );
        if ( artistPtr.data() != 0 )
        {
           //debug() << "Found parent artist";
           //Meta::ServiceArtist *artist = dynamic_cast< Meta::ServiceArtist * > ( artistPtr.data() );
           album->setAlbumArtist( artistPtr );
        }

        albums.push_back( albumPtr );

        n = n.nextSibling();
    }

    emit newResultReady( albums );
    emit queryDone();
}

void
AmpacheServiceQueryMaker::trackDownloadComplete( const KUrl &url, QByteArray data, NetworkAccessManagerProxy::Error e )
{
    if( d->urls.value(QLatin1String("tracks")) != url )
        return;

    d->urls.remove( QLatin1String("tracks") );
    if( e.code != QNetworkReply::NoError )
    {
        debug() << "Track download error:" << e.description;
        return;
    }

    // DEBUG_BLOCK

    Meta::TrackList tracks;

     //so lets figure out what we got here:
    QDomDocument doc( "reply" );
    doc.setContent( data );
    QDomElement root = doc.firstChildElement( "root" );

    // Is this an error, if so we need to 'un-ready' the service and re-authenticate before contiuning
    QDomElement domError = root.firstChildElement( "error" );

    if( !domError.isNull() )
    {
        debug () << "Error getting Track Download " << domError.text();
        AmpacheService *m_parentService = dynamic_cast< AmpacheService * >( m_collection->service() );
        if ( m_parentService == 0 )
            return;
        else
            m_parentService->reauthenticate();
    }

    QDomNode n = root.firstChild();
    while( !n.isNull() )
    {
        QDomElement e = n.toElement(); // try to convert the node to an element.
        //if ( ! (e.tagName() == "item") )
        //    break;

        int trackId = e.attribute( "id", "0" ).toInt();
        QDomElement element = n.firstChildElement( "title" );

        QString title = element.text();
        if ( title.isEmpty() )
            title = "Unknown";
        element = n.firstChildElement( "url" );
        Meta::AmpacheTrack * track = new Meta::AmpacheTrack( title, m_collection->service() );
        Meta::TrackPtr trackPtr( track );

        //debug() << "Adding track: " <<  title;

        track->setId( trackId );
        track->setUidUrl( element.text() );

        element = n.firstChildElement( "time" );
        track->setLength( element.text().toInt() * 1000 );

        element = n.firstChildElement( "track" );
        track->setTrackNumber( element.text().toInt() );

        m_collection->acquireWriteLock();
        m_collection->addTrack( trackPtr );
        m_collection->releaseLock();

        QDomElement albumElement = n.firstChildElement( "album" );
        int albumId = albumElement.attribute( "id", "0").toInt();

        QDomElement artistElement = n.firstChildElement( "artist" );
        int artistId = artistElement.attribute( "id", "0").toInt();

        Meta::ArtistPtr artistPtr = m_collection->artistById( artistId );
        if ( artistPtr.data() != 0 )
        {
            //debug() << "Found parent artist " << artistPtr->name();
            Meta::ServiceArtist *artist = dynamic_cast< Meta::ServiceArtist * > ( artistPtr.data() );
            track->setArtist( artistPtr );
            artist->addTrack( trackPtr );
        }

        Meta::AlbumPtr albumPtr = m_collection->albumById( albumId );
        if ( albumPtr.data() != 0 )
        {
            //debug() << "Found parent album " << albumPtr->name() ;
            Meta::ServiceAlbum *album = dynamic_cast< Meta::ServiceAlbum * > ( albumPtr.data() );
            track->setAlbumPtr( albumPtr );
            album->addTrack( trackPtr );
        }

        tracks.push_back( trackPtr );

        n = n.nextSibling();
    }

    emit newResultReady( tracks );
    emit queryDone();
}

QueryMaker *
AmpacheServiceQueryMaker::addFilter( qint64 value, const QString & filter, bool matchBegin, bool matchEnd )
{
    DEBUG_BLOCK
    Q_UNUSED( matchBegin )
    Q_UNUSED( matchEnd )

    //debug() << "value: " << value;
    //for now, only accept artist filters
    if ( value == Meta::valArtist )
    {
        //debug() << "Filter: " << filter;
        m_artistFilter = filter;
    }
    return this;
}

QueryMaker*
AmpacheServiceQueryMaker::addNumberFilter( qint64 value, qint64 filter, QueryMaker::NumberComparison compare )
{
    DEBUG_BLOCK

    if( value == Meta::valCreateDate && compare == QueryMaker::GreaterThan )
    {
        debug() << "asking to filter based on added date";
        m_dateFilter = filter;
        debug() << "setting dateFilter to:" << m_dateFilter;
    }
    return this;
}

int
AmpacheServiceQueryMaker::validFilterMask()
{
    //we only supprt artist filters for now...
    return ArtistFilter;
}

QueryMaker *
AmpacheServiceQueryMaker::limitMaxResultSize( int size )
{
    d->maxsize = size;
    return this;
}

#include "AmpacheServiceQueryMaker.moc"

