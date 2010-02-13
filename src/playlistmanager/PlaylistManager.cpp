/****************************************************************************************
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

#include "PlaylistManager.h"

#include "amarokurls/AmarokUrl.h"
#include "amarokconfig.h"
#include "App.h"
#include "statusbar/StatusBar.h"
#include "CollectionManager.h"
#include "PlaylistFile.h"
#include "playlist/PlaylistModelStack.h"
#include "PlaylistFileSupport.h"
#include "PodcastProvider.h"
#include "file/PlaylistFileProvider.h"
#include "sql/SqlPodcastProvider.h"
#include "sql/SqlUserPlaylistProvider.h"
#include "Debug.h"
#include "MainWindow.h"
#include "browsers/playlistbrowser/UserPlaylistModel.h"

#include <kdirlister.h>
#include <kio/jobclasses.h>
#include <kio/job.h>
#include <KInputDialog>
#include <KLocale>
#include <KUrl>

#include <QFileInfo>

PlaylistManager *PlaylistManager::s_instance = 0;

namespace The
{
    PlaylistManager *playlistManager() { return PlaylistManager::instance(); }
}

PlaylistManager *
PlaylistManager::instance()
{
    return s_instance ? s_instance : new PlaylistManager();
}

void
PlaylistManager::destroy()
{
    if (s_instance) {
        delete s_instance;
        s_instance = 0;
    }
}

PlaylistManager::PlaylistManager()
{
    s_instance = this;

    m_defaultPodcastProvider = new SqlPodcastProvider();
    addProvider( m_defaultPodcastProvider, PlaylistManager::PodcastChannel );
    CollectionManager::instance()->addTrackProvider( m_defaultPodcastProvider );

    m_defaultUserPlaylistProvider = new SqlUserPlaylistProvider();
    addProvider( m_defaultUserPlaylistProvider, UserPlaylist );

    m_playlistFileProvider = new PlaylistFileProvider();
    addProvider( m_playlistFileProvider, UserPlaylist );
}

PlaylistManager::~PlaylistManager()
{
    delete m_defaultPodcastProvider;
    delete m_defaultUserPlaylistProvider;
    delete m_playlistFileProvider;
}

void
PlaylistManager::addProvider( PlaylistProvider * provider, int category )
{
    DEBUG_BLOCK

    bool newCategory = false;
    if( !m_providerMap.uniqueKeys().contains( category ) )
            newCategory = true;

    m_providerMap.insert( category, provider );
    connect( provider, SIGNAL(updated()), SLOT(slotUpdated( /*PlaylistProvider **/ )) );

    if( newCategory )
        emit( categoryAdded( category ) );

    emit( providerAdded( provider, category ) );
    emit( updated() );
}

void
PlaylistManager::removeProvider( PlaylistProvider *provider )
{
    DEBUG_BLOCK

    if ( !provider )
        return;

    if ( m_providerMap.values( provider->category() ).contains( provider ) )
    {
        debug() << "Providers of this category: " << providersForCategory( provider->category() ).count();
        debug() << "Removing provider from map";
        int removed = m_providerMap.remove( provider->category(), provider );
        debug() << "Removed provider from map:" << ( m_providerMap.contains( provider->category(), provider ) ? "false" : "true" );
        debug() << "Providers removed: " << removed;

        emit( providerRemoved( provider, provider->category() ) );

        slotUpdated();

    }


}

void
PlaylistManager::slotUpdated( /*PlaylistProvider * provider*/ )
{
    DEBUG_BLOCK
    emit(updated());
}

Meta::PlaylistList
PlaylistManager::playlistsOfCategory( int playlistCategory )
{
    DEBUG_BLOCK
    QList<PlaylistProvider *> providers = m_providerMap.values( playlistCategory );
    QListIterator<PlaylistProvider *> i( providers );

    Meta::PlaylistList list;
    while ( i.hasNext() )
    {
        list << i.next()->playlists();
    }

    return list;
}

PlaylistProviderList
PlaylistManager::providersForCategory( int playlistCategory )
{
    return m_providerMap.values( playlistCategory );
}

PlaylistProvider *
PlaylistManager::playlistProvider(int category, QString name)
{
    QList<PlaylistProvider *> providers( m_providerMap.values( category ) );

    QListIterator<PlaylistProvider *> i(providers);
    while( i.hasNext() )
    {
        PlaylistProvider * p = static_cast<PlaylistProvider *>( i.next() );
        if( p->prettyName() == name )
            return p;
    }

    return 0;
}

void
PlaylistManager::downloadPlaylist( const KUrl &path, const Meta::PlaylistFilePtr playlist )
{
    DEBUG_BLOCK

    KIO::StoredTransferJob * downloadJob =  KIO::storedGet( path );

    m_downloadJobMap[downloadJob] = playlist;

    connect( downloadJob, SIGNAL( result( KJob * ) ),
             this, SLOT( downloadComplete( KJob * ) ) );

    The::statusBar()->newProgressOperation( downloadJob, i18n( "Downloading Playlist" ) );
}

void
PlaylistManager::downloadComplete( KJob * job )
{
    DEBUG_BLOCK

    if ( !job->error() == 0 )
    {
        //TODO: error handling here
        return ;
    }

    Meta::PlaylistFilePtr playlist = m_downloadJobMap.take( job );

    QString contents = static_cast<KIO::StoredTransferJob *>(job)->data();
    QTextStream stream;
    stream.setString( &contents );

    playlist->load( stream );
}

bool
PlaylistManager::save( Meta::TrackList tracks, const QString &name,
                       UserPlaylistProvider *toProvider )
{
    AMAROK_DEPRECATED
    // used by: Playlist::Widget::slotSaveCurrentPlaylist()
    //if toProvider is 0 use the default UserPlaylistProvider (SQL)
    UserPlaylistProvider *prov = toProvider ? toProvider : m_defaultUserPlaylistProvider;
    Meta::PlaylistPtr playlist = Meta::PlaylistPtr();
    if( name.isEmpty() )
    {
        debug() << "Empty name of playlist, or editing now";
        playlist = prov->save( tracks );
        if( playlist.isNull() )
            return false;

        AmarokUrl("amarok://navigate/playlists/user playlists").run();
        emit( renamePlaylist( playlist ) );
    }
    else
    {
        debug() << "Playlist is being saved with name: " << name;
        playlist = prov->save( tracks, name );
    }

    return !playlist.isNull();
}

bool
PlaylistManager::import( const QString& fromLocation )
{
    // used by: PlaylistBrowserNS::UserModel::dropMimeData()
    AMAROK_DEPRECATED
    DEBUG_BLOCK
    if( !m_playlistFileProvider )
    {
        debug() << "ERROR: m_playlistFileProvider was null";
        return false;
    }
    return m_playlistFileProvider->import( KUrl(fromLocation) );
}

void
PlaylistManager::rename( Meta::PlaylistPtr playlist )
{
    DEBUG_BLOCK

    if( playlist.isNull() )
        return;

    UserPlaylistProvider *prov;
    prov = qobject_cast<UserPlaylistProvider *>( getProviderForPlaylist( playlist ) );

    if( !prov )
        return;

    bool ok;
    const QString newName = KInputDialog::getText( i18n("Change playlist"),
                i18n("Enter new name for playlist:"), playlist->name(),
                                                   &ok );
    if ( ok )
    {
        debug() << "Changing name from " << playlist->name() << " to " << newName.trimmed();
        prov->rename( playlist, newName.trimmed() );
        emit( updated() );
    }
}

void
PlaylistManager::deletePlaylists( Meta::PlaylistList playlistlist )
{
    // Map the playlists to their respective providers

    QHash<UserPlaylistProvider*, Meta::PlaylistList> provLists;
    foreach( Meta::PlaylistPtr playlist, playlistlist )
    {
        // Get the providers of the respective playlists

        UserPlaylistProvider *prov = qobject_cast<UserPlaylistProvider *>( getProviderForPlaylist( playlist ) );

        if( prov )
        {
            Meta::PlaylistList pllist;
            pllist << playlist;
            // If the provider already has at least one playlist to delete, add another to its list
            if( provLists.contains( prov ) )
            {
                provLists[ prov ] << pllist;

            }
            // If we are adding a new provider, put it in the hash, initialize its list
            else
                provLists.insert( prov, pllist );
        }
    }

    // Pass each list of playlists to the respective provider for deletion

    foreach( UserPlaylistProvider* prov, provLists.keys() )
    {
        prov->deletePlaylists( provLists[ prov ] );
    }
}

PlaylistProvider*
PlaylistManager::getProviderForPlaylist( const Meta::PlaylistPtr playlist )
{
    PlaylistProvider* provider = playlist->provider();
    if( provider )
        return provider;

    // Iteratively check all providers' playlists for ownership
    foreach( PlaylistProvider* provider, m_providerMap.values( UserPlaylist ) )
    {
        if( provider->playlists().contains( playlist ) )
                return provider;
    }
    return 0;
}


bool
PlaylistManager::isWritable( const Meta::PlaylistPtr &playlist )
{
    UserPlaylistProvider *prov;
    prov = qobject_cast<UserPlaylistProvider *>( getProviderForPlaylist( playlist ) );

    if( prov )
        return prov->isWritable();
    else
        return false;
}

QList<QAction *>
PlaylistManager::playlistActions( const Meta::PlaylistList playlists )
{
    QList<QAction *> actions;
    foreach( const Meta::PlaylistPtr playlist, playlists )
    {
        PlaylistProvider *provider = getProviderForPlaylist( playlist );
        if( !provider )
            continue;

        //only add actions that are not in the list yet.
        QList<QAction *> playlistActions( provider->playlistActions( playlist ) );
        foreach( QAction *action, playlistActions )
        {
            if( !actions.contains( action ) )
                actions << action;
        }
    }

    return actions;
}

QList<QAction *>
PlaylistManager::trackActions( const Meta::PlaylistPtr playlist, int trackIndex )
{
    QList<QAction *> actions;
    PlaylistProvider *provider = getProviderForPlaylist( playlist );
    if( provider )
        actions << provider->trackActions( playlist, trackIndex );

    return actions;
}

void
PlaylistManager::completePodcastDownloads()
{
    foreach( PlaylistProvider *prov, providersForCategory( PodcastChannel ) )
    {
        PodcastProvider *podcastProvider = dynamic_cast<PodcastProvider *>( prov );
        if( !podcastProvider )
            continue;

        podcastProvider->completePodcastDownloads();
    }
}

#include "PlaylistManager.moc"
