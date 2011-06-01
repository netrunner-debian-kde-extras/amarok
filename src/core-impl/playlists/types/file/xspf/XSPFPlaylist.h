/****************************************************************************************
 * Copyright (c) 2007 Bart Cerneels <bart.cerneels@kde.org>                             *
 * Copyright (c) 2006 Mattias Fliesberg <mattias.fliesberg@gmail.com>                   *
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

#ifndef XSPFPLAYLIST_H
#define XSPFPLAYLIST_H

#include "core-impl/playlists/types/file/PlaylistFile.h"
#include "core/capabilities/EditablePlaylistCapability.h"

#include <QDomDocument>
#include <QTextStream>

class QTextStream;
class KUrl;

namespace Playlists
{
class XSPFPlaylist;

typedef KSharedPtr<XSPFPlaylist> XSPFPlaylistPtr;
typedef QList<XSPFPlaylistPtr> XSPFPlaylistList;

/* convenience struct for internal use */
typedef struct {
    KUrl location;
    QString identifier;
    QString title;
    QString creator;
    QString annotation;
    KUrl info;
    KUrl image;
    QString album;
    uint trackNum;
    uint duration;
    KUrl link;
} XSPFTrack;

typedef QList<XSPFTrack> XSPFTrackList;

/**
	@author Bart Cerneels <bart.cerneels@kde.org>
*/
class AMAROK_EXPORT XSPFPlaylist : public PlaylistFile, public QDomDocument,
            public Capabilities::EditablePlaylistCapability
{
public:
    XSPFPlaylist();

    /**
    * Creates a new XSPFPlaylist and starts loading the xspf file of the url.
    * @param url The Url of the xspf file to load.
    * @param autoAppend Should this playlist automatically append itself to the playlist when loaded (useful when loading a remote url as it
    * allows the caller to do it in a "one shot" way and not have to worry about waiting untill download and parsing is completed.
    */
    explicit XSPFPlaylist( const KUrl &url, bool autoAppend = false );
    XSPFPlaylist( Meta::TrackList list );

    ~XSPFPlaylist();

    virtual QString name() const { return title(); }
    virtual QString description() const;

    virtual int trackCount() const;
    virtual Meta::TrackList tracks();
    virtual void triggerTrackLoad();

    virtual void addTrack( Meta::TrackPtr track, int position = -1 );
    virtual void removeTrack( int position );

    /* convenience functions */
    QString title() const;
    QString creator() const;
    QString annotation() const;
    KUrl info() const;
    KUrl location() const;
    QString identifier() const;
    KUrl image() const;
    QDateTime date() const;
    KUrl license() const;
    KUrl::List attribution() const ;
    KUrl link() const;

    /* EditablePlaylistCapability virtual functions */
    void setTitle( const QString &title );
    void setCreator( const QString &creator );
    void setAnnotation( const QString &annotation );
    void setInfo( const KUrl &info );
    void setLocation( const KUrl &location );
    void setIdentifier( const QString &identifier );
    void setImage( const KUrl &image );
    void setDate( const QDateTime &date );
    void setLicense( const KUrl &license );
    void setAttribution( const KUrl &attribution, bool append = true );
    void setLink( const KUrl &link );
    void setTrackList( Meta::TrackList trackList, bool append = false );

    //TODO: implement these
    void beginMetaDataUpdate() {}
    void endMetaDataUpdate() {}

    bool isEditable() const { return true; }


    /* Meta::Playlist virtual functions */
    virtual KUrl uidUrl() const { return m_url; }
    bool hasCapabilityInterface( Capabilities::Capability::Type type ) const;

    Capabilities::Capability* createCapabilityInterface( Capabilities::Capability::Type type );

    /* PlaylistFile methods */
    bool isWritable();
    /** Changes both the filename and the title in XML */
    void setName( const QString &name );
    bool load( QTextStream &stream ) { return loadXSPF( stream ); }
    /** save to location */
    bool save( const KUrl &location, bool relative );

    void setQueue( const QList<int> &queue );
    QList<int> queue();

private:
    XSPFTrackList trackList();
    QString trackLocation( Meta::TrackPtr &track );
    bool loadXSPF( QTextStream& );
    bool m_tracksLoaded;
    //cache for the tracklist since a tracks() is a called *a lot*.
    Meta::TrackList m_tracks;

    KUrl m_url;
    bool m_autoAppendAfterLoad;
    bool m_relativePaths;
    bool m_saveLock;
};

}

Q_DECLARE_METATYPE( Playlists::XSPFPlaylistPtr )
Q_DECLARE_METATYPE( Playlists::XSPFPlaylistList )

#endif
