/****************************************************************************************
 * Copyright (c) 2006 Mattias Fliesberg <mattias.fliesberg@gmail.com>                   *
 * Copyright (c) 2007 Ian Monroe <ian@monroe.nu>                                        *
 * Copyright (c) 2007 Bart Cerneels <bart.cerneels@kde.org>                             *
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

#define DEBUG_PREFIX "XSPFPlaylist"

#include "core-impl/playlists/types/file/xspf/XSPFPlaylist.h"

#include "core/support/Debug.h"
#include "core-impl/collections/support/CollectionManager.h"
#include "core-impl/meta/proxy/MetaProxy.h"
#include "core/meta/support/MetaUtility.h"
#include "core-impl/meta/stream/Stream.h"
#include "core/capabilities/StreamInfoCapability.h"
#include "playlist/PlaylistActions.h"
#include "playlist/PlaylistController.h"
#include "playlist/PlaylistModelStack.h"
#include "PlaylistManager.h"
#include "core-impl/playlists/types/file/PlaylistFileSupport.h"

#include "core-impl/meta/timecode/TimecodeMeta.h"

#include <KUrl>
#include <KMessageBox>
#include <KMimeType>

#include <QDateTime>
#include <QDomElement>
#include <QFile>
#include <QString>

#include <typeinfo>

namespace Playlists
{

XSPFPlaylist::XSPFPlaylist()
    : QDomDocument()
    , m_tracksLoaded( false )
    , m_relativePaths( false )
    , m_saveLock( false )
{
    QDomElement root = createElement( "playlist" );

    root.setAttribute( "version", 1 );
    root.setAttribute( "xmlns", "http://xspf.org/ns/1/" );

    root.appendChild( createElement( "trackList" ) );

    appendChild( root );
}

XSPFPlaylist::XSPFPlaylist( const KUrl &url, bool autoAppend )
    : QDomDocument()
    , m_tracksLoaded( false )
    , m_url( url )
    , m_autoAppendAfterLoad( autoAppend )
    , m_relativePaths( false )
    , m_saveLock( false )
{
    //check if file is local or remote
    if ( m_url.isLocalFile() )
    {
        QFile file( m_url.toLocalFile() );
        if( !file.open( QIODevice::ReadOnly ) )
        {
            error() << "cannot open file " << url.url();
            return;
        }

        QTextStream stream( &file );
        stream.setAutoDetectUnicode( true );

        loadXSPF( stream );
    }
    else
    {
        The::playlistManager()->downloadPlaylist( m_url, PlaylistFilePtr( this ) );
    }
}

XSPFPlaylist::XSPFPlaylist( Meta::TrackList tracks )
    : QDomDocument()
    , m_relativePaths( false )
    , m_saveLock( false )
{
    QDomElement root = createElement( "playlist" );

    root.setAttribute( "version", 1 );
    root.setAttribute( "xmlns", "http://xspf.org/ns/0/" );

    root.appendChild( createElement( "trackList" ) );

    appendChild( root );

    setTrackList( tracks );

    m_tracks = tracks;
    m_tracksLoaded = true;
}

XSPFPlaylist::~XSPFPlaylist()
{}

QString
XSPFPlaylist::description() const
{
    if( !annotation().isEmpty() )
        return annotation();

    KMimeType::Ptr mimeType = KMimeType::mimeType( "application/xspf+xml" );
    return QString( "%1 (%2)").arg( mimeType->name(), "xspf" );
}

bool
XSPFPlaylist::save( const KUrl &location, bool relative )
{
    m_url = location;
    //if the location is a directory append the name of this playlist.
    if( m_url.fileName( KUrl::ObeyTrailingSlash ).isNull() )
        m_url.setFileName( name() );
    
    if( relative )
    {
        m_relativePaths = true;
        m_saveLock = true;
        setTrackList( tracks(), false );
        m_saveLock = false;
    }
    else if( m_relativePaths )
    {
        m_relativePaths = false;
        m_saveLock = true;
        setTrackList( tracks(), false );
        m_saveLock = false;
    }

    QFile file;

    if( location.isLocalFile() )
    {
        file.setFileName( m_url.toLocalFile() );
    }
    else
    {
        file.setFileName( m_url.path() );
    }

    if( !file.open( QIODevice::WriteOnly ) )
    {
        warning() << QString( "Cannot write playlist (%1)." ).arg( file.fileName() )
                  << file.errorString();

        return false;
    }

    QTextStream stream( &file );
    stream.setCodec( "UTF-8" );
    QDomDocument::save( stream, 2 /*indent*/, QDomNode::EncodingFromTextStream );

    return true;
}

bool
XSPFPlaylist::loadXSPF( QTextStream &stream )
{
    QString errorMsg;
    int errorLine, errorColumn;

    QString rawText = stream.readAll();

    if( !setContent( rawText, &errorMsg, &errorLine, &errorColumn ) )
    {
        error() << "Error loading xml file: " "(" << errorMsg << ")"
                << " at line " << errorLine << ", column " << errorColumn;
        return false;
    }

    //FIXME: this needs to be moved to whatever is creating the XSPFPlaylist
    if( m_autoAppendAfterLoad )
        The::playlistController()->insertPlaylist(
                    ::Playlist::ModelStack::instance()->bottom()->rowCount(),
                    Playlists::PlaylistPtr( this )
                );

    return true;
}

int
XSPFPlaylist::trackCount() const
{
    if( m_tracksLoaded )
        return m_tracks.count();

    //TODO: lookup in XML directly, without loading tracks
    return -1;
}

Meta::TrackList
XSPFPlaylist::tracks()
{
    return m_tracks;
}

void
XSPFPlaylist::triggerTrackLoad()
{
    //TODO make sure we've got all tracks first.
    if( m_tracksLoaded )
        return;

    XSPFTrackList xspfTracks = trackList();

    foreach( const XSPFTrack &track, xspfTracks )
    {
        Meta::TrackPtr trackPtr;
        if( !track.identifier.isEmpty() )
            trackPtr = CollectionManager::instance()->trackForUrl( track.identifier );
        else
            trackPtr = CollectionManager::instance()->trackForUrl( track.location );
        if( trackPtr )
        {
            /**
             * NOTE: If this is a MetaProxy::Track, it probably isn't playable yet,
             *       but that's okay. However, it's not a good idea to get another
             *       one from the same provider, since the proxy probably means that
             *       making one involves quite a bit of work.
             *         - Andy Coder <andrew.coder@gmail.com>
             */
            if( !trackPtr->isPlayable() && ( typeid( * trackPtr.data() ) != typeid( MetaProxy::Track ) ) )
                trackPtr = CollectionManager::instance()->trackForUrl( track.identifier );
        }

        if( trackPtr )
        {
            if( typeid( * trackPtr.data() ) == typeid( MetaStream::Track ) )
            {
                MetaStream::Track * streamTrack = dynamic_cast<MetaStream::Track *> ( trackPtr.data() );
                if ( streamTrack )
                {
                    streamTrack->setTitle( track.title );
                    streamTrack->setAlbum( track.album );
                    streamTrack->setArtist( track.creator );
                }
            }
            else if( typeid( * trackPtr.data() ) == typeid( Meta::TimecodeTrack ) )
            {
                Meta::TimecodeTrack * timecodeTrack =
                        dynamic_cast<Meta::TimecodeTrack *>( trackPtr.data() );
                if( timecodeTrack )
                {
                    timecodeTrack->beginMetaDataUpdate();
                    timecodeTrack->setTitle( track.title );
                    timecodeTrack->setAlbum( track.album );
                    timecodeTrack->setArtist( track.creator );
                    timecodeTrack->endMetaDataUpdate();
                }
            }

            m_tracks << trackPtr;
        }


        // why do we need this? sqlplaylist is not doing this
        // we don't want (probably) unplayable tracks
        // and it causes problems for me (DanielW) as long
        // amarok not respects Track::isPlayable()
        /*else {

            MetaProxy::Track *proxyTrack = new MetaProxy::Track( track.location );
            {
                //Fill in values from xspf..
                QVariantMap map;
                map.insert( Meta::Field::TITLE, track.title );
                map.insert( Meta::Field::ALBUM, track.album );
                map.insert( Meta::Field::ARTIST, track.creator );
                map.insert( Meta::Field::LENGTH, track.duration );
                map.insert( Meta::Field::TRACKNUMBER, track.trackNum );
                map.insert( Meta::Field::URL, track.location );
                Meta::Field::updateTrack( proxyTrack, map );
            }
            m_tracks << Meta::TrackPtr( proxyTrack );
    //         m_tracks << CollectionManager::instance()->trackForUrl( track.location );
        }*/

    }

    m_tracksLoaded = true;
}

void
XSPFPlaylist::addTrack( Meta::TrackPtr track, int position )
{
    if( !m_tracksLoaded )
        triggerTrackLoad();

    Meta::TrackList trackList = tracks();
    int trackPos = position < 0 ? trackList.count() : position;
    if( trackPos > trackList.count() )
        trackPos = trackList.count();
    trackList.insert( trackPos, track );
    setTrackList( trackList );
    //also add to cache
    m_tracks.insert( trackPos, track );
    //set in case no track was in the playlist before
    m_tracksLoaded = true;

    notifyObserversTrackAdded( track, position );
}

void
XSPFPlaylist::removeTrack( int position )
{
    Meta::TrackList trackList = tracks();
    if( position < 0  || position > trackList.count() )
        return;

    trackList.removeAt( position );
    setTrackList( trackList );
    //also remove from cache
    m_tracks.removeAt( position );

    notifyObserversTrackRemoved( position );
}

QString
XSPFPlaylist::title() const
{
    return documentElement().namedItem( "title" ).firstChild().nodeValue();
}

QString
XSPFPlaylist::creator() const
{
    return documentElement().namedItem( "creator" ).firstChild().nodeValue();
}

QString
XSPFPlaylist::annotation() const
{
    return documentElement().namedItem( "annotation" ).firstChild().nodeValue();
}

KUrl
XSPFPlaylist::info() const
{
    return KUrl( documentElement().namedItem( "info" ).firstChild().nodeValue() );
}

KUrl
XSPFPlaylist::location() const
{
    return KUrl( documentElement().namedItem( "location" ).firstChild().nodeValue() );
}

QString
XSPFPlaylist::identifier() const
{
    return documentElement().namedItem( "identifier" ).firstChild().nodeValue();
}

KUrl
XSPFPlaylist::image() const
{
    return KUrl( documentElement().namedItem( "image" ).firstChild().nodeValue() );
}

QDateTime
XSPFPlaylist::date() const
{
    return QDateTime::fromString( documentElement().namedItem( "date" ).firstChild().nodeValue(), Qt::ISODate );
}

KUrl
XSPFPlaylist::license() const
{
    return KUrl( documentElement().namedItem( "license" ).firstChild().nodeValue() );
}

KUrl::List
XSPFPlaylist::attribution() const
{
    const QDomNodeList nodes = documentElement().namedItem( "attribution" ).childNodes();
    KUrl::List list;

    for( int i = 0, count = nodes.length(); i < count; ++i  )
    {
        const QDomNode &node = nodes.at( i );
        if( !node.firstChild().nodeValue().isNull() )
            list.append( node.firstChild().nodeValue() );
    }
    return list;
}

KUrl
XSPFPlaylist::link() const
{
    return KUrl( documentElement().namedItem( "link" ).firstChild().nodeValue() );
}

void
XSPFPlaylist::setTitle( const QString &title )
{
    QDomNode titleNode = documentElement().namedItem( "title" );
    if( titleNode.isNull() || !titleNode.hasChildNodes() )
    {
        QDomNode node = createElement( "title" );
        QDomNode subNode = createTextNode( title );
        node.appendChild( subNode );
        documentElement().insertBefore( node, documentElement().namedItem( "trackList" ) );
    }
    else
    {
        documentElement().namedItem( "title" ).replaceChild( createTextNode( title ),
                                    documentElement().namedItem( "title" ).firstChild()
                                );
    }

    //write changes to file directly if we know where.
    if( !m_url.isEmpty() )
        save( m_url, false );
}

void
XSPFPlaylist::setCreator( const QString &creator )
{
    if( documentElement().namedItem( "creator" ).isNull() )
    {
        QDomNode node = createElement( "creator" );
        QDomNode subNode = createTextNode( creator );
        node.appendChild( subNode );
        documentElement().insertBefore( node, documentElement().namedItem( "trackList" ) );
    }
    else
    {
        documentElement().namedItem( "creator" ).replaceChild( createTextNode( creator ),
                                            documentElement().namedItem( "creator" ).firstChild() );
    }

    //write changes to file directly if we know where.
    if( !m_url.isEmpty() )
        save( m_url, false );
}

void
XSPFPlaylist::setAnnotation( const QString &annotation )
{
    if( documentElement().namedItem( "annotation" ).isNull() )
    {
        QDomNode node = createElement( "annotation" );
        QDomNode subNode = createTextNode( annotation );
        node.appendChild( subNode );
        documentElement().insertBefore( node, documentElement().namedItem( "trackList" ) );
    }
    else
    {
        documentElement().namedItem( "annotation" ).replaceChild( createTextNode( annotation ),
                                        documentElement().namedItem( "annotation" ).firstChild() );
    }

    //write changes to file directly if we know where.
    if( !m_url.isEmpty() )
        save( m_url, false );
}

void
XSPFPlaylist::setInfo( const KUrl &info )
{
    if( documentElement().namedItem( "info" ).isNull() )
    {
        QDomNode node = createElement( "info" );
        QDomNode subNode = createTextNode( info.url() );
        node.appendChild( subNode );
        documentElement().insertBefore( node, documentElement().namedItem( "trackList" ) );
    }
    else
    {
        documentElement().namedItem( "info" ).replaceChild( createTextNode( info.url() ),
                                            documentElement().namedItem( "info" ).firstChild() );
    }

    //write changes to file directly if we know where.
    if( !m_url.isEmpty() )
        save( m_url, false );
}

void
XSPFPlaylist::setLocation( const KUrl &location )
{
    if( documentElement().namedItem( "location" ).isNull() )
    {
        QDomNode node = createElement( "location" );
        QDomNode subNode = createTextNode( location.url() );
        node.appendChild( subNode );
        documentElement().insertBefore( node, documentElement().namedItem( "trackList" ) );
    }
    else
    {
        documentElement().namedItem( "location" ).replaceChild( createTextNode( location.url() ),
                                        documentElement().namedItem( "location" ).firstChild() );
    }

    //write changes to file directly if we know where.
    if( !m_url.isEmpty() )
        save( m_url, false );
}

void
XSPFPlaylist::setIdentifier( const QString &identifier )
{
    if( documentElement().namedItem( "identifier" ).isNull() )
    {
        QDomNode node = createElement( "identifier" );
        QDomNode subNode = createTextNode( identifier );
        node.appendChild( subNode );
        documentElement().insertBefore( node, documentElement().namedItem( "trackList" ) );
    }
    else
    {
        documentElement().namedItem( "identifier" ).replaceChild( createTextNode( identifier ),
                                        documentElement().namedItem( "identifier" ).firstChild() );
    }

    //write changes to file directly if we know where.
    if( !m_url.isEmpty() )
        save( m_url, false );
}

void
XSPFPlaylist::setImage( const KUrl &image )
{
    if( documentElement().namedItem( "image" ).isNull() )
    {
        QDomNode node = createElement( "image" );
        QDomNode subNode = createTextNode( image.url() );
        node.appendChild( subNode );
        documentElement().insertBefore( node, documentElement().namedItem( "trackList" ) );
    }
    else
    {
        documentElement().namedItem( "image" ).replaceChild( createTextNode( image.url() ),
                                            documentElement().namedItem( "image" ).firstChild() );
    }

    //write changes to file directly if we know where.
    if( !m_url.isEmpty() )
        save( m_url, false );
}

void
XSPFPlaylist::setDate( const QDateTime &date )
{
    /* date needs timezone info to be compliant with the standard
    (ex. 2005-01-08T17:10:47-05:00 ) */

    if( documentElement().namedItem( "date" ).isNull() )
    {
        QDomNode node = createElement( "date" );
        QDomNode subNode = createTextNode( date.toString( "yyyy-MM-ddThh:mm:ss" ) );
        node.appendChild( subNode );
        documentElement().insertBefore( node, documentElement().namedItem( "trackList" ) );
    }
    else
    {
        documentElement().namedItem( "date" )
                .replaceChild( createTextNode( date.toString( "yyyy-MM-ddThh:mm:ss" ) ),
                               documentElement().namedItem( "date" ).firstChild() );
    }

    //write changes to file directly if we know where.
    if( !m_url.isEmpty() )
        save( m_url, false );
}

void
XSPFPlaylist::setLicense( const KUrl &license )
{
    if( documentElement().namedItem( "license" ).isNull() )
    {
        QDomNode node = createElement( "license" );
        QDomNode subNode = createTextNode( license.url() );
        node.appendChild( subNode );
        documentElement().insertBefore( node, documentElement().namedItem( "trackList" ) );
    }
    else
    {
        documentElement().namedItem( "license" ).replaceChild( createTextNode( license.url() ),
                                        documentElement().namedItem( "license" ).firstChild() );
    }

    //write changes to file directly if we know where.
    if( !m_url.isEmpty() )
        save( m_url, false );
}

void
XSPFPlaylist::setAttribution( const KUrl &attribution, bool append )
{
    if( !attribution.isValid() )
        return;

    if( documentElement().namedItem( "attribution" ).isNull() )
    {
        documentElement().insertBefore( createElement( "attribution" ),
                                        documentElement().namedItem( "trackList" ) );
    }

    if( append )
    {
        QDomNode subNode = createElement( "location" );
        QDomNode subSubNode = createTextNode( attribution.url() );
        subNode.appendChild( subSubNode );

        QDomNode first = documentElement().namedItem( "attribution" ).firstChild();
        documentElement().namedItem( "attribution" ).insertBefore( subNode, first );
    }
    else
    {
        QDomNode node = createElement( "attribution" );
        QDomNode subNode = createElement( "location" );
        QDomNode subSubNode = createTextNode( attribution.url() );
        subNode.appendChild( subSubNode );
        node.appendChild( subNode );
        documentElement().replaceChild( node, documentElement().namedItem( "attribution" ) );
    }

    //write changes to file directly if we know where.
    if( !m_url.isEmpty() )
        save( m_url, false );
}

void
XSPFPlaylist::setLink( const KUrl &link )
{
    if( documentElement().namedItem( "link" ).isNull() )
    {
        QDomNode node = createElement( "link" );
        QDomNode subNode = createTextNode( link.url() );
        node.appendChild( subNode );
        documentElement().insertBefore( node, documentElement().namedItem( "trackList" ) );
    }
    else
    {
        documentElement().namedItem( "link" ).replaceChild( createTextNode( link.url() ),
                                            documentElement().namedItem( "link" ).firstChild() );
    }

    //write changes to file directly if we know where.
    if( !m_url.isEmpty() )
        save( m_url, false );
}

XSPFTrackList
XSPFPlaylist::trackList()
{
    XSPFTrackList list;

    QDomNode trackList = documentElement().namedItem( "trackList" );
    QDomNode subNode = trackList.firstChild();
    QDomNode subSubNode;

    while( !subNode.isNull() )
    {
        XSPFTrack track;
        subSubNode = subNode.firstChild();
        if( subNode.nodeName() == "track" )
        {
            while( !subSubNode.isNull() )
            {
                if( subSubNode.nodeName() == "location" )
                {
                    QString path = QUrl::fromPercentEncoding( subSubNode.firstChild().nodeValue().toAscii() );
                    path.replace( '\\', '/' );

                    KUrl url = path;
                    if( url.isRelative() )
                    {
                        m_relativePaths = true;
                        url = m_url.directory();
                        url.addPath( path );
                        url.cleanPath();
                    }

                    track.location = url;
                }
                else if( subSubNode.nodeName() == "title" )
                    track.title = subSubNode.firstChild().nodeValue();
                else if( subSubNode.nodeName() == "creator" )
                    track.creator = subSubNode.firstChild().nodeValue();
                else if( subSubNode.nodeName() == "duration" )
                    track.duration = subSubNode.firstChild().nodeValue().toInt();
                else if( subSubNode.nodeName() == "annotation" )
                    track.annotation = subSubNode.firstChild().nodeValue();
                else if( subSubNode.nodeName() == "album" )
                    track.album = subSubNode.firstChild().nodeValue();
                else if( subSubNode.nodeName() == "trackNum" )
                    track.trackNum = (uint)subSubNode.firstChild().nodeValue().toInt();
                else if( subSubNode.nodeName() == "identifier" )
                    track.identifier = subSubNode.firstChild().nodeValue();
                else if( subSubNode.nodeName() == "info" )
                    track.info = subSubNode.firstChild().nodeValue();
                else if( subSubNode.nodeName() == "image" )
                    track.image = subSubNode.firstChild().nodeValue();
                else if( subSubNode.nodeName() == "link" )
                    track.link = subSubNode.firstChild().nodeValue();

                subSubNode = subSubNode.nextSibling();
            }
        }
        list.append( track );
        subNode = subNode.nextSibling();
    }

    return list;
}


//documentation of attributes from http://www.xspf.org/xspf-v1.html
void
XSPFPlaylist::setTrackList( Meta::TrackList trackList, bool append )
{
    if( documentElement().namedItem( "trackList" ).isNull() )
        documentElement().appendChild( createElement( "trackList" ) );

    QDomNode node = createElement( "trackList" );

    Meta::TrackPtr track;
    foreach( track, trackList ) // krazy:exclude=foreach
    {
        QDomNode subNode = createElement( "track" );

        //URI of resource to be rendered.
        QDomNode location = createElement( "location" );

        //Human-readable name of the track that authored the resource
        QDomNode title = createElement( "title" );

        //Human-readable name of the entity that authored the resource.
        QDomNode creator = createElement( "creator" );

        //A human-readable comment on the track.
        QDomNode annotation = createElement( "annotation" );

        //Human-readable name of the collection from which the resource comes
        QDomNode album = createElement( "album" );

        //Integer > 0 giving the ordinal position of the media in the album.
        QDomNode trackNum = createElement( "trackNum" );

        //The time to render a resource, in milliseconds. It MUST be a nonNegativeInteger.
        QDomNode duration = createElement( "duration" );

        //location-independent name, such as a MusicBrainz identifier. MUST be a legal URI.
        QDomNode identifier = createElement( "identifier" );

        //info - URI of a place where this resource can be bought or more info can be found.
        //QDomNode info = createElement( "info" );

        //image - URI of an image to display for the duration of the track.
        //QDomNode image = createElement( "image" );

        //link - element allows XSPF to be extended without the use of XML namespaces.
        //QDomNode link = createElement( "link" );

        //QDomNode meta
        //amarok specific queue info, see the XSPF specification's meta element
        QDomElement queue = createElement( "meta" );
        queue.setAttribute( "rel", "http://amarok.kde.org/queue" );

        //QDomNode extension

        #define APPENDNODE( X, Y ) \
        { \
            X.appendChild( createTextNode( Y ) );    \
            subNode.appendChild( X ); \
        }

        APPENDNODE( location, trackLocation( track ) )
        APPENDNODE( identifier, track->uidUrl() )

        Capabilities::StreamInfoCapability *streamInfo = track->create<Capabilities::StreamInfoCapability>();
        if( streamInfo ) // We have a stream, use it's metadata instead of the tracks.
        {
            if( !streamInfo->streamName().isEmpty() )
                APPENDNODE( title, streamInfo->streamName() )
            if( !streamInfo->streamSource().isEmpty() )
                APPENDNODE( creator, streamInfo->streamSource() )

            delete streamInfo;
        }
        else
        {
            if( !track->name().isEmpty() )
                APPENDNODE(title, track->name() )
            if( track->artist() && !track->artist()->name().isEmpty() )
                APPENDNODE(creator, track->artist()->name() );
        }
        if( !track->comment().isEmpty() )
            APPENDNODE(annotation, track->comment() );
        if( track->album() && !track->album()->name().isEmpty() )
            APPENDNODE( album, track->album()->name() );
        if( track->trackNumber() > 0 )
            APPENDNODE( trackNum, QString::number( track->trackNumber() ) );
        if( track->length() > 0 )
            APPENDNODE( duration, QString::number( track->length() ) );

        node.appendChild( subNode );
    }
    #undef APPENDNODE

    if( append )
    {
        while( !node.isNull() )
        {
            documentElement().namedItem( "trackList" ).appendChild( node.firstChild() );
            node = node.nextSibling();
        }
    }
    else
        documentElement().replaceChild( node, documentElement().namedItem( "trackList" ) );

    //write changes to file directly if we know where.
    if( !m_url.isEmpty() && !m_saveLock )
        save( m_url, false );
}

void
XSPFPlaylist::setQueue( const QList<int> &queue )
{
    QDomElement q = createElement( "queue" );

    foreach( int row, queue )
    {
        QDomElement qTrack = createElement( "track" );
        qTrack.appendChild( createTextNode( QString::number( row ) ) );
        q.appendChild( qTrack );
    }

    QDomElement extension = createElement( "extension" );
    extension.setAttribute( "application", "http://amarok.kde.org" );
    extension.appendChild( q );

    QDomNode root = firstChild();
    root.appendChild( extension );
}

QList<int>
XSPFPlaylist::queue()
{
    QList<int> tracks;

    QDomElement extension = documentElement().firstChildElement( "extension" );
    if( extension.isNull() )
        return tracks;
    
    if( extension.attribute( "application" ) != "http://amarok.kde.org" )
        return tracks;

    QDomElement queue = extension.firstChildElement( "queue" );
    if( queue.isNull() )
        return tracks;

    for( QDomElement trackElem = queue.firstChildElement( "track" );
         !trackElem.isNull();
         trackElem = trackElem.nextSiblingElement( "track" ) )
    {
        tracks << trackElem.text().toInt();
    }

    return tracks;
}

bool
XSPFPlaylist::hasCapabilityInterface( Capability::Type type ) const
{
    switch( type )
    {
        case Capability::EditablePlaylist: return true;
        default: return false;
    }
}

Capabilities::Capability*
XSPFPlaylist::createCapabilityInterface( Capability::Type type )
{
    switch( type )
    {
        case Capability::EditablePlaylist: return static_cast<EditablePlaylistCapability *>( this );
        default: return 0;
    }
}

bool
XSPFPlaylist::isWritable()
{
    if( m_url.isEmpty() )
        return false;

    return QFileInfo( m_url.path() ).isWritable();
}

void
XSPFPlaylist::setName( const QString &name )
{
    //can't save to a new file if we don't know where.
    if( !m_url.isEmpty() && !name.isEmpty() )
    {
        if( QFileInfo( m_url.toLocalFile() ).exists() )
        {
            warning() << "Deleting old playlist file:" << m_url.toLocalFile();
            QFile::remove( m_url.toLocalFile() );
        }
        m_url.setFileName( name + ( name.endsWith( ".xspf", Qt::CaseInsensitive ) ? "" : ".xspf" ) );
    }
    //setTitle will save if there is a url.
    setTitle( name );
}

QString 
XSPFPlaylist::trackLocation( Meta::TrackPtr &track )
{
    KUrl path = track->playableUrl();
    if( path.isEmpty() )
        return track->uidUrl();
    
    if( !m_relativePaths || m_url.isEmpty() || !path.isLocalFile() || !m_url.isLocalFile() )
        return path.url();

    QDir playlistDir( m_url.directory() );
    return QUrl::toPercentEncoding( playlistDir.relativeFilePath( path.path() ), "/" );
}

} //namespace Playlists
