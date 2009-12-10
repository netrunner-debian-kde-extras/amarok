/****************************************************************************************
 * Copyright (c) 2008 Nikolaj Hald Nielsen <nhnFreespirit@gmail.com>                    *
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

#include "AmarokUrlHandler.h"

#include "BookmarkMetaActions.h"
#include "CollectionManager.h"
#include "Debug.h"
#include "GlobalCurrentTrackActions.h"
#include "NavigationUrlGenerator.h"
#include "NavigationUrlRunner.h"
#include "PlayUrlRunner.h"
#include "playlist/PlaylistViewUrlGenerator.h"
#include "BookmarkModel.h"
#include "SqlStorage.h"
#include "timecode/TimecodeObserver.h"
#include "ContextUrlGenerator.h"
#include "PlayUrlGenerator.h"

#include <KIcon>

namespace The {
    static AmarokUrlHandler* s_AmarokUrlHandler_instance = 0;

    AmarokUrlHandler* amarokUrlHandler()
    {
        if( !s_AmarokUrlHandler_instance )
            s_AmarokUrlHandler_instance = new AmarokUrlHandler();

        return s_AmarokUrlHandler_instance;
    }
}

AmarokUrlHandler::AmarokUrlHandler()
    : QObject()
    , m_navigationRunner( 0 )
    , m_playRunner ( 0 )
    , m_timecodeObserver( 0 )
{
    DEBUG_BLOCK

    //init the bookmark model to make sure that db tables are created/updated if needed.
    BookmarkModel::instance();
    
    //we init some of the default runners here.
    m_navigationRunner = new NavigationUrlRunner();
    m_playlistViewRunner = new Playlist::ViewUrlRunner();
    m_playRunner = new PlayUrlRunner();
    m_timecodeObserver = new TimecodeObserver();
    registerRunner( m_navigationRunner, m_navigationRunner->command() );
    registerRunner( m_playRunner, m_playRunner->command() );
    registerRunner( m_playlistViewRunner, m_playlistViewRunner->command() );

    registerGenerator( ContextUrlGenerator::instance() );
    registerGenerator( NavigationUrlGenerator::instance() );
    registerGenerator( Playlist::ViewUrlGenerator::instance() );
    registerGenerator( PlayUrlGenerator::instance() );
}


AmarokUrlHandler::~AmarokUrlHandler()
{
    delete m_navigationRunner;
    delete m_playlistViewRunner;
}

void AmarokUrlHandler::registerRunner( AmarokUrlRunnerBase * runner, const QString & command )
{
    m_registeredRunners.insert( command, runner );
}

void AmarokUrlHandler::unRegisterRunner( AmarokUrlRunnerBase * runner )
{
    //get the key of the runner
    QString key = m_registeredRunners.key( runner, QString() );

    if ( !key.isEmpty() )
        m_registeredRunners.remove( key );
}

void AmarokUrlHandler::registerGenerator( AmarokUrlGenerator * generator )
{
    if( !m_registeredGenerators.contains( generator ) )
        m_registeredGenerators.append( generator );
}

void AmarokUrlHandler::unRegisterGenerator( AmarokUrlGenerator * generator )
{
    m_registeredGenerators.removeAll( generator );
}

bool AmarokUrlHandler::run( AmarokUrl url )
{

    DEBUG_BLOCK

    QString command = url.command();

    debug() << "command: " << command;
    debug() << "registered commands: " << m_registeredRunners.keys();

    if ( m_registeredRunners.contains( command ) )
        return m_registeredRunners.value( command )->run( url );
    else
        return false;

}

void AmarokUrlHandler::bookmarkAlbum( Meta::AlbumPtr album ) //slot
{
    NavigationUrlGenerator::instance()->urlFromAlbum( album ).saveToDb();
    BookmarkModel::instance()->reloadFromDb();
}

void AmarokUrlHandler::bookmarkArtist( Meta::ArtistPtr artist ) //slot
{
    NavigationUrlGenerator::instance()->urlFromArtist( artist ).saveToDb();
    BookmarkModel::instance()->reloadFromDb();
}

BookmarkList AmarokUrlHandler::urlsByCommand( const QString &command )
{
    DEBUG_BLOCK

    QString query = "SELECT id, parent_id, name, url, description, custom FROM bookmarks where url like 'amarok://%1%' ORDER BY name;";
    query = query.arg( command );
    QStringList result = CollectionManager::instance()->sqlStorage()->query( query );

    debug() << "Result: " << result;
    int resultRows = result.count() / 6;

    BookmarkList resultList;
    for( int i = 0; i < resultRows; i++ )
    {
        QStringList row = result.mid( i*6, 6 );
        resultList << AmarokUrlPtr( new AmarokUrl( row ) );
    }

    return resultList;
}

void AmarokUrlHandler::bookmarkCurrentBrowserView()
{
    AmarokUrl url = NavigationUrlGenerator::instance()->CreateAmarokUrl();
    url.saveToDb();
    BookmarkModel::instance()->reloadFromDb();
}

void
AmarokUrlHandler::bookmarkCurrentPlaylistView()
{
    AmarokUrl url = Playlist::ViewUrlGenerator::instance()->createUrl();
    url.saveToDb();
    BookmarkModel::instance()->reloadFromDb();
}

void
AmarokUrlHandler::bookmarkCurrentContextView()
{
    AmarokUrl url = ContextUrlGenerator::instance()->createContextBookmark();
    url.saveToDb();
    BookmarkModel::instance()->reloadFromDb(); 
}

KIcon
AmarokUrlHandler::iconForCommand( const QString &command )
{
    if( m_registeredRunners.keys().contains( command ) )
        return m_registeredRunners.value( command )->icon();

    return KIcon( "unknown" );
}

void
AmarokUrlHandler::registerForTimecodes( ProgressWidget * progressWidget )
{
    if( !m_progresWidgets.contains( progressWidget ) )
        m_progresWidgets.append( progressWidget );
}

void
AmarokUrlHandler::unregisterForTimecodes( ProgressWidget * progressWidget )
{
    m_progresWidgets.removeAll( progressWidget );
}

void AmarokUrlHandler::updateTimecodes(const QString* BookmarkName)
{
    foreach( ProgressWidget * progressWidget, m_progresWidgets )
    {
        if (progressWidget->isVisible())  progressWidget->redrawBookmarks( BookmarkName );
    }
}

void
AmarokUrlHandler::paintNewTimecode( const QString &name, int pos )
{
    foreach( ProgressWidget * progressWidget, m_progresWidgets )
    {
        progressWidget->addBookmark( name, pos );
    }
}



#include "AmarokUrlHandler.moc"
