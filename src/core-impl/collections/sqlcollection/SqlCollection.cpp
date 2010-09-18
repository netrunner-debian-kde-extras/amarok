/****************************************************************************************
 * Copyright (c) 2007 Maximilian Kossick <maximilian.kossick@googlemail.com>            *
 * Copyright (c) 2007 Casey Link <unnamedrambler@gmail.com>                             *
 * Copyright (c) 2008-2009 Jeff Mitchell <mitchell@kde.org>                             *
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

#include "SqlCollection.h"

#include "CapabilityDelegate.h"
#include "DatabaseUpdater.h"
#include "core/support/Debug.h"
#include "ScanManager.h"
#include "SqlCollectionLocation.h"
#include "SqlQueryMaker.h"
#include "SvgHandler.h"

#ifdef Q_OS_WIN32
class XesamCollectionBuilder
{
public:
    XesamCollectionBuilder(Collections::SqlCollection *collection) {}
};
#else
#include "XesamCollectionBuilder.h"
#endif

#include <klocale.h>
#include <KIcon>
#include <KMessageBox> // TODO put the delete confirmation code somewhere else?
#include <QTimer>

using namespace Collections;

SqlCollection::SqlCollection( const QString &id, const QString &prettyName )
    : Collection()
    , m_registry( 0 )
    , m_updater( 0 )
    , m_capabilityDelegate( 0 )
    , m_sqlStorage( 0 )
    , m_collectionLocationFactory( 0 )
    , m_queryMakerFactory( 0 )
    , m_scanManager( 0 )
    , m_mpm( 0 )
    , m_collectionId( id )
    , m_prettyName( prettyName )
    , m_xesamBuilder( 0 )
{
    qRegisterMetaType<TrackUrls>( "TrackUrls" );
    qRegisterMetaType<ChangedTrackUrls>( "ChangedTrackUrls" );
}

SqlCollection::~SqlCollection()
{
    delete m_registry;
    delete m_capabilityDelegate;
    delete m_updater;
    delete m_collectionLocationFactory;
    delete m_queryMakerFactory;
    delete m_sqlStorage;
    delete m_mpm;
}

void
SqlCollection::setUpdater( DatabaseUpdater *updater )
{
    DEBUG_BLOCK
    if( !updater )
    {
        debug() << "Could not load updater!";
        return;
    }
    m_updater = updater;
    if( m_updater->needsUpdate() )
    {
        debug() << "Needs update!";
        m_updater->update();
    }
}

void
SqlCollection::init()
{
    QTimer::singleShot( 0, this, SLOT( initXesam() ) );

    QStringList result = m_sqlStorage->query( "SELECT count(*) FROM tracks" );
    // If database version is updated, the collection needs to be rescanned.
    // Works also if the collection is empty for some other reason
    // (e.g. deleted collection.db)
    if( m_updater->rescanNeeded() || ( !result.isEmpty() && result.first().toInt() == 0 ) )
    {
        QTimer::singleShot( 0, m_scanManager, SLOT( startFullScan() ) );
    }
    //perform a quick check of the database
    m_updater->cleanupDatabase();
}

void
SqlCollection::startFullScan()
{
    if( m_scanManager )
        m_scanManager->startFullScan();
}

void
SqlCollection::startIncrementalScan( const QString &directory )
{
    if( m_scanManager )
        m_scanManager->startIncrementalScan( directory );
}

void
SqlCollection::stopScan()
{
    DEBUG_BLOCK

    if( m_scanManager )
        m_scanManager->abort( "Abort requested from SqlCollection::stopScan()" );
}

QString
SqlCollection::uidUrlProtocol() const
{
    return "amarok-sqltrackuid";
}

QString
SqlCollection::collectionId() const
{
    return m_collectionId;
}

QString
SqlCollection::prettyName() const
{
    return m_prettyName;
}

QueryMaker*
SqlCollection::queryMaker()
{
    Q_ASSERT( m_queryMakerFactory );
    return m_queryMakerFactory->createQueryMaker();
}

SqlRegistry*
SqlCollection::registry() const
{
    Q_ASSERT( m_registry );
    return m_registry;
}

DatabaseUpdater*
SqlCollection::dbUpdater() const
{
    Q_ASSERT( m_updater );
    return m_updater;
}

IScanManager*
SqlCollection::scanManager() const
{
    Q_ASSERT( m_scanManager );
    return m_scanManager;
}

void
SqlCollection::setScanManager( IScanManager *manager )
{
    m_scanManager = manager;
}

SqlStorage*
SqlCollection::sqlStorage() const
{
    Q_ASSERT( m_sqlStorage );
    return m_sqlStorage;
}

SqlMountPointManager*
SqlCollection::mountPointManager() const
{
    Q_ASSERT( m_mpm );
    return m_mpm;
}

void
SqlCollection::setMountPointManager( SqlMountPointManager *mpm )
{
    Q_ASSERT( mpm );
    connect( mpm, SIGNAL( deviceAdded(int) ), SLOT( slotDeviceAdded(int) ) );
    connect( mpm, SIGNAL( deviceRemoved(int) ), SLOT( slotDeviceRemoved(int) ) );
    m_mpm = mpm;
}

void
SqlCollection::removeCollection()
{
    emit remove();
}

bool
SqlCollection::isDirInCollection( QString path )
{
    return m_scanManager->isDirInCollection( path );
}

bool
SqlCollection::possiblyContainsTrack( const KUrl &url ) const
{
    foreach( const QString &folder, collectionFolders() )
    {
        if ( url.path().contains( folder ) )
            return true;
    }
    return url.protocol() == "file" || url.protocol() == uidUrlProtocol();
}

Meta::TrackPtr
SqlCollection::trackForUrl( const KUrl &url )
{
    if( url.protocol() == uidUrlProtocol() )
        return m_registry->getTrackFromUid( url.url() );
    foreach( const QString &folder, collectionFolders() )
    {
        if ( url.path().contains( folder ) )
            return m_registry->getTrack( url.path() );
    }
    return Meta::TrackPtr();
}

CollectionLocation*
SqlCollection::location() const
{
    Q_ASSERT( m_collectionLocationFactory );
    return m_collectionLocationFactory->createSqlCollectionLocation();
}

bool
SqlCollection::isWritable() const
{
    CollectionLocation *loc = location();
    if( loc )
    {
        bool res = loc->isWritable();
        delete loc;
        return res;
    }
    return false;
}

bool
SqlCollection::isOrganizable() const
{
    CollectionLocation *loc = location();
    if( loc )
    {
        bool res = loc->isOrganizable();
        delete loc;
        return res;
    }
    return false;
}

QStringList
SqlCollection::collectionFolders() const
{
    return mountPointManager()->collectionFolders();
}

void
SqlCollection::setCollectionFolders( const QStringList &folders )
{
    mountPointManager()->setCollectionFolders( folders );
}

void
SqlCollection::sendChangedSignal()
{
    emit updated();
}

QStringList
SqlCollection::knownUIDsInDirectory( const QString &dir )
{
    const int deviceId = mountPointManager()->getIdForUrl( dir );
    const QString rdir = mountPointManager()->getRelativePath( deviceId, dir );


    const QStringList values =
            m_sqlStorage->query( QString( "SELECT id FROM directories WHERE dir = '%2' AND deviceid = %1;" )
            .arg( QString::number( deviceId ), m_sqlStorage->escape( rdir ) ) );

    if( values.isEmpty() )
    {
        return QStringList();
    }

    QString id = values.first();

    const QStringList uids =
            m_sqlStorage->query( QString( "SELECT uniqueid FROM urls INNER JOIN tracks ON urls.id = tracks.url WHERE directory = %1;" ).arg( id ) );

    return uids;
}

void
SqlCollection::updateTrackUrlsUids( const ChangedTrackUrls &changedUrls, const QHash<QString, QString> &changedUids ) //SLOT
{
    DEBUG_BLOCK
    QHash<QString, KSharedPtr<Meta::SqlTrack> > trackList; //dun want duplicates
    foreach( const QString &key, changedUrls.keys() )
    {
        if( m_registry->checkUidExists( key ) )
        {
            m_registry->updateCachedUrl( changedUrls.value( key ) );
            Meta::TrackPtr track = m_registry->getTrackFromUid( key );
            if( track )
                trackList.insert( key, KSharedPtr<Meta::SqlTrack>::staticCast( track ) );
        }
    }
    foreach( const QString &key, changedUids.keys() )
    {
        //old uid is key, new is value
        if( m_registry->checkUidExists( key ) )
        {
            m_registry->updateCachedUid( key, changedUids[key] );
            Meta::TrackPtr track = m_registry->getTrackFromUid( key );
            if( track )
                trackList.insert( changedUids[key], KSharedPtr<Meta::SqlTrack>::staticCast( track ) );
        }
    }
    foreach( const QString &key, trackList.keys() )
        trackList[key]->refreshFromDatabase( key, this, true );
}

void
SqlCollection::initXesam() //SLOT
{
    m_xesamBuilder = new XesamCollectionBuilder( this );
}

void
SqlCollection::slotDeviceAdded( int id )
{
    QString query = "select count(*) from tracks inner join urls on tracks.url = urls.id where urls.deviceid = %1";
    QStringList rs = m_sqlStorage->query( query.arg( id ) );
    if( !rs.isEmpty() )
    {
        int count = rs.first().toInt();
        if( count > 0 )
        {
            emit updated();
        }
    }
    else
    {
        warning() << "Query " << query << "did not return a result! Is the database available?";
    }
}

void
SqlCollection::slotDeviceRemoved( int id )
{
    QString query = "select count(*) from tracks inner join urls on tracks.url = urls.id where urls.deviceid = %1";
    QStringList rs = m_sqlStorage->query( query.arg( id ) );
    if( !rs.isEmpty() )
    {
        int count = rs.first().toInt();
        if( count > 0 )
        {
            emit updated();
        }
    }
    else
    {
        warning() << "Query " << query << "did not return a result! Is the database available?";
    }
}

bool
SqlCollection::hasCapabilityInterface( Capabilities::Capability::Type type ) const
{
    return ( m_capabilityDelegate ? m_capabilityDelegate->hasCapabilityInterface( type, this ) : false );
}

Capabilities::Capability*
SqlCollection::createCapabilityInterface( Capabilities::Capability::Type type )
{
    return ( m_capabilityDelegate ? m_capabilityDelegate->createCapabilityInterface( type, this ) : 0 );
}

void
SqlCollection::deleteTracksSlot( Meta::TrackList tracklist )
{
    DEBUG_BLOCK
    QStringList files;
    foreach( Meta::TrackPtr track, tracklist )
        files << track->prettyUrl();

	CollectionLocation *loc = location();
    // remove the tracks from the collection maps
    //TODO make unblocking
    foreach( Meta::TrackPtr track, tracklist )
        loc->remove( track );

    loc->deleteLater();
    // inform treeview collection has updated
    emit updated();
}

void
SqlCollection::dumpDatabaseContent()
{
    QStringList tables = m_sqlStorage->query( "select table_name from INFORMATION_SCHEMA.tables WHERE table_schema='amarok'" );
    foreach( const QString &table, tables )
    {
        m_updater->writeCSVFile( table, table, true );
    }
}

#include "SqlCollection.moc"

