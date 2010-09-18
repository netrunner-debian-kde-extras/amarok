/****************************************************************************************
 * Copyright (c) 2007 Maximilian Kossick <maximilian.kossick@googlemail.com>            *
 * Copyright (c) 2008 Jason A. Donenfeld <Jason@zx2c4.com>                              *
 * Copyright (c) 2010 Casey Link <unnamedrambler@gmail.com>                             *
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

#include "SqlCollectionLocation.h"

#include "collectionscanner/AFTUtility.h"

#include "core/collections/CollectionLocationDelegate.h"
#include "core/support/Components.h"
#include "core/support/Debug.h"
#include "core/interfaces/Logger.h"
#include "core/collections/support/SqlStorage.h"
#include "core/meta/Meta.h"
#include "core/meta/support/MetaUtility.h"
#include "ScanManager.h"
#include "ScanResultProcessor.h"
#include "SqlCollection.h"
#include "SqlMeta.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>

#include <kdiskfreespaceinfo.h>
#include <kjob.h>
#include <KLocale>
#include <KSharedPtr>
#include <kio/job.h>
#include <kio/jobclasses.h>
#include <kio/deletejob.h>
#include <KSortableList>


using namespace Collections;

SqlCollectionLocation::SqlCollectionLocation( SqlCollection const *collection )
    : CollectionLocation( collection )
    , m_collection( const_cast<Collections::SqlCollection*>( collection ) )
    , m_delegateFactory( 0 )
    , m_overwriteFiles( false )
    , m_migrateLabels( true )
    , m_transferjob( 0 )
{
    //nothing to do
}

SqlCollectionLocation::~SqlCollectionLocation()
{
    //nothing to do
    delete m_delegateFactory;
}

QString
SqlCollectionLocation::prettyLocation() const
{
    return i18n( "Local Collection" );
}

QStringList
SqlCollectionLocation::actualLocation() const
{
    return m_collection->mountPointManager()->collectionFolders();
}

bool
SqlCollectionLocation::isWritable() const
{
    DEBUG_BLOCK
    // The collection is writeable if there exists a path that has less than
    // 95% free space.
    bool path_exists_with_space = false;
    bool path_exists_writeable = false;
    QStringList folders = actualLocation();
    foreach(QString path, folders)
    {
        float used = KDiskFreeSpaceInfo::freeSpaceInfo( path ).used();
        float total = KDiskFreeSpaceInfo::freeSpaceInfo( path ).size();
	debug() << path;
	debug() << "\tused: " << used;
	debug() << "\ttotal: " << total;

        if( total <= 0 ) // protect against div by zero
            continue; //How did this happen?

        float free_space = total - used;
        debug() <<"\tfree space: " << free_space;
        if( free_space >= 500*1000*1000 ) // ~500 megabytes
            path_exists_with_space = true;

        QFileInfo info( path );
        if( info.isWritable() )
            path_exists_writeable = true;
	debug() << "\tpath_exists_writeable" << path_exists_writeable;
	debug() << "\tpath_exists_with_space" << path_exists_with_space;
    }
    return path_exists_with_space && path_exists_writeable;
}

bool
SqlCollectionLocation::isOrganizable() const
{
    return true;
}

bool
SqlCollectionLocation::remove( const Meta::TrackPtr &track )
{
    DEBUG_BLOCK
    KSharedPtr<Meta::SqlTrack> sqlTrack = KSharedPtr<Meta::SqlTrack>::dynamicCast( track );
    if( sqlTrack && sqlTrack->inCollection() && sqlTrack->collection()->collectionId() == m_collection->collectionId() )
    {
        bool removed;
        KUrl src = track->playableUrl();
        if( isGoingToRemoveSources() ) // is organize operation?
        {
            SqlCollectionLocation* destinationloc = dynamic_cast<SqlCollectionLocation*>( destination() );
            src = destinationloc->m_originalUrls[track];
            if( src == track->playableUrl() )
                return false;
        }
        // we are going to delete it from the database only if is no longer on disk
        removed = !QFile::exists( src.path() );
        if( removed )
        {
            debug() << "file not on disk, remove from dbase";
            int deviceId = m_collection->mountPointManager()->getIdForUrl( src );
            QString rpath = m_collection->mountPointManager()->getRelativePath( deviceId, src.url() );
            QString query = QString( "SELECT id FROM urls WHERE deviceid = %1 AND rpath = '%2';" )
                                .arg( QString::number( deviceId ), m_collection->sqlStorage()->escape( rpath ) );
            QStringList res = m_collection->sqlStorage()->query( query );
            if( res.isEmpty() )
            {
                warning() << "Tried to remove a track from SqlCollection which is not in the collection";
            }
            else
            {
                int id = res[0].toInt();
                QString query = QString( "DELETE FROM tracks where url = %1;" ).arg( id );
                m_collection->sqlStorage()->query( query );
            }

            QFileInfo file( src.path() );
            QDir dir = file.dir();
            dir.setFilter( QDir::NoDotAndDotDot );
            const QStringList collectionFolders = m_collection->mountPointManager()->collectionFolders();
            while( !collectionFolders.contains( dir.absolutePath() ) && !dir.isRoot() && dir.count() == 0 )
            {
                const QString name = dir.dirName();
                dir.cdUp();
                if( !dir.rmdir( name ) )
                    break;
            }

        }
        return removed;
    }
    else
    {
        debug() << "Remove Failed";
        return false;
    }
}

void
SqlCollectionLocation::showDestinationDialog( const Meta::TrackList &tracks, bool removeSources )
{
    DEBUG_BLOCK
    setGoingToRemoveSources( removeSources );

    KIO::filesize_t transferSize = 0;
    foreach( Meta::TrackPtr track, tracks )
        transferSize += track->filesize();

    QStringList actual_folders = actualLocation(); // the folders in the collection
    QStringList available_folders; // the folders which have freespace available
    foreach(QString path, actual_folders)
    {
        if( path.isEmpty() )
            continue;
        debug() << "Path" << path;
        KDiskFreeSpaceInfo spaceInfo = KDiskFreeSpaceInfo::freeSpaceInfo( path );
        if( !spaceInfo.isValid() )
            continue;

        KIO::filesize_t totalCapacity = spaceInfo.size();
        KIO::filesize_t used = spaceInfo.used();

        KIO::filesize_t freeSpace = totalCapacity - used;

        debug() << "used:" << used;
        debug() << "total:" << totalCapacity;
        debug() << "Free space" << freeSpace;
        debug() << "transfersize" << transferSize;

        if( totalCapacity <= 0 ) // protect against div by zero
            continue; //How did this happen?

        QFileInfo info( path );

        // since bad things happen when drives become totally full
	// we make sure there is at least ~500MB left
        // finally, ensure the path is writeable
        debug() << ( freeSpace - transferSize );
        if( ( freeSpace - transferSize ) > 1000*1000*500 && info.isWritable() )
            available_folders << path;
    }

    if( available_folders.size() <= 0 )
    {
        debug() << "No space available or not writable";
        CollectionLocationDelegate *delegate = Amarok::Components::collectionLocationDelegate();
        delegate->notWriteable( this );
        abort();
        return;
    }

    OrganizeCollectionDelegate *delegate = m_delegateFactory->createDelegate();
    delegate->setTracks( tracks );
    delegate->setFolders( available_folders );
    delegate->setIsOrganizing( ( collection() == source()->collection() ) );
    connect( delegate, SIGNAL( accepted() ), SLOT( slotDialogAccepted() ) );
    connect( delegate, SIGNAL( rejected() ), SLOT( slotDialogRejected() ) );
    delegate->show();
}

void
SqlCollectionLocation::slotDialogAccepted()
{
    sender()->deleteLater();
    OrganizeCollectionDelegate *ocDelegate = qobject_cast<OrganizeCollectionDelegate*>( sender() );
    m_destinations = ocDelegate->destinations();
    m_overwriteFiles = ocDelegate->overwriteDestinations();
    m_migrateLabels = ocDelegate->migrateLabels();
    if( isGoingToRemoveSources() )
    {
        CollectionLocationDelegate *delegate = Amarok::Components::collectionLocationDelegate();
        const bool del = delegate->reallyMove( this, m_destinations.keys() );
        if( !del )
        {
            abort();
            return;
        }
    }
    slotShowDestinationDialogDone();
}

void
SqlCollectionLocation::slotDialogRejected()
{
    DEBUG_BLOCK
    sender()->deleteLater();
    abort();
}

void
SqlCollectionLocation::slotJobFinished( KJob *job )
{
    DEBUG_BLOCK
    if( job->error() )
    {
        //TODO: proper error handling
        warning() << "An error occurred when copying a file: " << job->errorString();
        source()->transferError(m_jobs.value( job ), KIO::buildErrorString( job->error(), job->errorString() ) );
        m_destinations.remove( m_jobs.value( job ) );
    }

    m_jobs.remove( job );
    job->deleteLater();

}

void
SqlCollectionLocation::slotRemoveJobFinished( KJob *job )
{
    DEBUG_BLOCK
    int error = job->error();
    if( error != 0 && error != KIO::ERR_DOES_NOT_EXIST )
    {
        //TODO: proper error handling
        debug() << "KIO::ERR_DOES_NOT_EXIST" << KIO::ERR_DOES_NOT_EXIST;
        warning() << "An error occurred when removing a file: " << job->errorString();
        transferError(m_removejobs.value( job ), KIO::buildErrorString( job->error(), job->errorString() ) );
    }
    else
    {
        // Remove the track from the database
        remove( m_removejobs.value( job ) );

        //we  assume that KIO works correctly...
        transferSuccessful( m_removejobs.value( job ) );
    }

    m_removejobs.remove( job );
    job->deleteLater();

    if( !startNextRemoveJob() )
    {
        m_collection->scanManager()->setBlockScan( false );
        slotRemoveOperationFinished();
    }

}

void SqlCollectionLocation::slotTransferJobFinished( KJob* job )
{
    DEBUG_BLOCK
    if( job->error() )
    {
        debug() << job->errorText();
    }
    // filter the list of destinations to only include tracks
    // that were successfully copied
    foreach( const Meta::TrackPtr &track, m_destinations.keys() )
    {
        if( !QFileInfo( m_destinations[ track ] ).exists() )
            m_destinations.remove( track );
        m_originalUrls[track] = track->playableUrl();
        debug() << "inserting original url" << m_originalUrls[track];
        // report successful transfer with the original url
        source()->transferSuccessful( track );

    }
    debug () << "m_originalUrls" << m_originalUrls;
    insertTracks( m_destinations );
    insertStatistics( m_destinations );
    if( m_migrateLabels )
    {
        migrateLabels( m_destinations );
    }
    m_collection->scanManager()->setBlockScan( false );
    slotCopyOperationFinished();
}

void SqlCollectionLocation::slotTransferJobAborted()
{
    DEBUG_BLOCK
    if( !m_transferjob )
        return;
    m_transferjob->kill();
    // filter the list of destinations to only include tracks
    // that were successfully copied
    foreach( const Meta::TrackPtr &track, m_destinations.keys() )
    {
        if( !QFileInfo( m_destinations[ track ] ).exists() )
            m_destinations.remove( track );
        m_originalUrls[track] = track->playableUrl();
    }
    insertTracks( m_destinations );
    insertStatistics( m_destinations );
    m_collection->scanManager()->setBlockScan( false );
    abort();
}


void
SqlCollectionLocation::copyUrlsToCollection( const QMap<Meta::TrackPtr, KUrl> &sources )
{
    m_collection->scanManager()->setBlockScan( true );  //make sure the collection scanner does not run while we are coyping stuff

    m_sources = sources;

    QString statusBarTxt;

    if( destination() == source() )
        statusBarTxt = i18n( "Organizing tracks" );
    else if ( isGoingToRemoveSources() )
        statusBarTxt = i18n( "Moving tracks" );
    else
        statusBarTxt = i18n( "Copying tracks" );

    m_transferjob = new TransferJob( this );
    Amarok::Components::logger()->newProgressOperation( m_transferjob, statusBarTxt, this, SLOT( slotTransferJobAborted() ) );
    connect( m_transferjob, SIGNAL( result( KJob * ) ), this, SLOT( slotTransferJobFinished( KJob * ) ) );
    m_transferjob->start();
}

void
SqlCollectionLocation::removeUrlsFromCollection(  const Meta::TrackList &sources )
{
    DEBUG_BLOCK

    m_collection->scanManager()->setBlockScan( true );  //make sure the collection scanner does not run while we are deleting stuff

    m_removetracks = sources;

    if( !startNextRemoveJob() ) //this signal needs to be called no matter what, even if there are no job finishes to call it
    {
        m_collection->scanManager()->setBlockScan( false );
        slotRemoveOperationFinished();
    }
}

void
SqlCollectionLocation::insertTracks( const QMap<Meta::TrackPtr, QString> &trackMap )
{
    //sort metadata by directory later on
    KSortableList<QVariantMap, QString> metadata;
    QStringList urls;
    AFTUtility aftutil;
    foreach( const Meta::TrackPtr &track, trackMap.keys() )
    {
        QVariantMap trackData = Meta::Field::mapFromTrack( track );
        trackData.insert( Meta::Field::URL, trackMap[ track ] );  //store the new url of the file
        // overwrite any uidUrl that came with the track with our own sql AFT one
        trackData.insert( Meta::Field::UNIQUEID, QString( "amarok-sqltrackuid://" ) + aftutil.readUniqueId( trackMap[ track ] ) );
        metadata.insert( trackMap[ track ], trackData );
        urls.append( trackMap[ track ] );
    }
    ScanResultProcessor processor( m_collection );
    processor.setSqlStorage( m_collection->sqlStorage() );
    processor.setScanType( ScanResultProcessor::IncrementalScan );
    QMap<QString, uint> mtime = updatedMtime( urls );
    //ScanResultProcessor removes all existing tracks in a given directory from the db when using processDirectory
    //therefore we pretend that the existing tracks are new as well.
    //SRP leaves the statistics table alone, so we do not destroy anything
    foreach( const QString &dir , mtime.keys() )
    {
        const QStringList uids = m_collection->knownUIDsInDirectory( dir );
        foreach( const QString &uid, uids )
        {
            Meta::TrackPtr track = m_collection->registry()->getTrackFromUid( uid );
            QVariantMap map = Meta::Field::mapFromTrack( track );

            metadata.insert(  map.value( Meta::Field::URL ).toString(), map );
        }
    }

    //sort by url (and therefore by directory)
    metadata.sort();

    foreach( const QString &dir, mtime.keys() )
    {
        processor.addDirectory( dir, mtime[ dir ] );
    }
    if( !metadata.isEmpty() )
    {
        QFileInfo info( metadata.first().value().value( Meta::Field::URL ).toString() );
        QString currentDir = info.dir().absolutePath();
        QList<QVariantMap > currentMetadata;

        typedef KSortableItem<QVariantMap, QString> MetaDataListItem;

        foreach( const MetaDataListItem &item, metadata )
        {
            QVariantMap map = item.value();
            debug() << "processing file " << map.value( Meta::Field::URL );
            QFileInfo info( map.value( Meta::Field::URL ).toString() );
            QString dir = info.dir().absolutePath();
            if( dir != currentDir )
            {
                processor.processDirectory( currentMetadata );
                currentDir = dir;
                currentMetadata.clear();
            }
            currentMetadata.append( map );
        }
        if( !currentMetadata.isEmpty() )
        {
            processor.processDirectory( currentMetadata );
        }
    }
    processor.commit();
}

void
SqlCollectionLocation::migrateLabels( const QMap<Meta::TrackPtr, QString> &trackMap )
{
    //there are two possible options here:
    //either we are organizing the collection, the the track collection is equal to this->collection()
    //or we are copying tracks from an external source.
    //in the first case, we just have to make sure that the existing labels point to the right track in the database.
    //in the latter case, we have to migrate the labels as well.
    foreach( Meta::TrackPtr track, trackMap.keys() )
    {
        if( track->collection() == collection() )
        {
            //first case
            //AFT will already have switched the playableUrl of track behind our back
        }
        else
        {
            //second case: migrate labels from the external track to the database.
            Meta::TrackPtr dest = m_collection->registry()->getTrack( trackMap.value( track ) );
            if( dest )
            {
                foreach( Meta::LabelPtr label, track->labels() )
                {
                    dest->addLabel( label );
                }
            }
        }
    }
}

QMap<QString, uint>
SqlCollectionLocation::updatedMtime( const QStringList &urls )
{
    QMap<QString, uint> mtime;
    foreach( const QString &url, urls )
    {
        QFileInfo fileInfo( url );
        QDir parentDir = fileInfo.dir();
        while( !mtime.contains( parentDir.absolutePath() ) && m_collection->isDirInCollection( parentDir.absolutePath() ) )
        {
            QFileInfo dir( parentDir.absolutePath() );
            mtime.insert( parentDir.absolutePath(), dir.lastModified().toTime_t() );
            if( !parentDir.cdUp() )
            {
                break;  //we arrived at the root of the filesystem
            }
        }
    }
    return mtime;
}

void
SqlCollectionLocation::insertStatistics( const QMap<Meta::TrackPtr, QString> &trackMap )
{
    SqlMountPointManager *mpm = m_collection->mountPointManager();
    foreach( const Meta::TrackPtr &track, trackMap.keys() )
    {
        QString url = trackMap[ track ];
        int deviceid = mpm->getIdForUrl( url );
        QString rpath = mpm->getRelativePath( deviceid, url );
        QString sql = QString( "SELECT COUNT(*) FROM statistics LEFT JOIN urls ON statistics.url = urls.id "
                               "WHERE urls.deviceid = %1 AND urls.rpath = '%2';" ).arg( QString::number( deviceid ), m_collection->sqlStorage()->escape( rpath ) );
        QStringList count = m_collection->sqlStorage()->query( sql );
        if( count.isEmpty() || count.first().toInt() != 0 )    //crash if the sql is bad
        {
            continue;   //a statistics row already exists for that url, and we cannot merge the statistics
        }
        //the row will exist because this method is called after insertTracks
        QString select = QString( "SELECT id FROM urls WHERE deviceid = %1 AND rpath = '%2';" ).arg( QString::number( deviceid ), m_collection->sqlStorage()->escape( rpath ) );
        QStringList result = m_collection->sqlStorage()->query( select );
        if( result.isEmpty() )
        {
            warning() << "SQL Query returned no results:" << select;
            continue;
        }
        QString id = result.first();    //if result is empty something is going very wrong
        //the following sql was copied from SqlMeta.cpp
        QString insert = "INSERT INTO statistics(url,rating,score,playcount,accessdate,createdate) VALUES ( %1 );";
        QString data = "%1,%2,%3,%4,%5,%6";
        data = data.arg( id, QString::number( track->rating() ), QString::number( track->score() ),
                    QString::number( track->playCount() ), QString::number( track->lastPlayed() ), QString::number( track->firstPlayed() ) );
        m_collection->sqlStorage()->insert( insert.arg( data ), "statistics" );
    }
}

void
SqlCollectionLocation::setOrganizeCollectionDelegateFactory( OrganizeCollectionDelegateFactory *fac )
{
    m_delegateFactory = fac;
}

bool SqlCollectionLocation::startNextJob()
{
    DEBUG_BLOCK
    if( !m_sources.isEmpty() )
    {
        Meta::TrackPtr track = m_sources.keys().first();
        KUrl src = m_sources.take( track );

        KIO::FileCopyJob *job = 0;
        KUrl dest = m_destinations[ track ];
        dest.cleanPath();

        src.cleanPath();
        debug() << "copying from " << src << " to " << dest;
        KIO::JobFlags flags = KIO::HideProgressInfo;
        if( m_overwriteFiles )
        {
            flags |= KIO::Overwrite;
        }
        QFileInfo info( dest.pathOrUrl() );
        QDir dir = info.dir();
        if( !dir.exists() )
        {
            if( !dir.mkpath( "." ) )
            {
                warning() << "Could not create directory " << dir;
                source()->transferError(track, i18n( "Could not create directory: %1", dir.path() ) );
                return true; // Attempt to copy/move the next item in m_sources
            }
        }
        if( src.equals( dest ) )
        {
            debug() << "move to itself found";
            source()->transferSuccessful( track );
            m_transferjob->slotJobFinished( 0 );
            if( m_sources.isEmpty() )
                return false;
            return true;
        }
        else if( isGoingToRemoveSources() && source()->collection() == collection() )
        {
            debug() << "moving!";
            job = KIO::file_move( src, dest, -1, flags );
        }
        else
        {
            //later on in the case that remove is called, the file will be deleted because we didn't apply moveByDestination to the track
            job = KIO::file_copy( src, dest, -1, flags );
        }
        if( job )   //just to be safe
        {
            connect( job, SIGNAL( result( KJob* ) ), SLOT( slotJobFinished( KJob* ) ) );
            connect( job, SIGNAL( result( KJob* ) ), m_transferjob, SLOT( slotJobFinished( KJob* ) ) );
            m_transferjob->addSubjob( job );
            QString name = track->prettyName();
            if( track->artist() )
                name = QString( "%1 - %2" ).arg( track->artist()->name(), track->prettyName() );

            m_transferjob->emitInfo( i18n( "Transferring: %1", name ) );
            m_jobs.insert( job, track );
            return true;
        }
        debug() << "JOB NULL OMG!11";
    }
    return false;
}

bool SqlCollectionLocation::startNextRemoveJob()
{
    DEBUG_BLOCK
    while ( !m_removetracks.isEmpty() )
    {
        Meta::TrackPtr track = m_removetracks.takeFirst();
        KUrl src = track->playableUrl();

        debug() << "isGoingToRemoveSources() " << isGoingToRemoveSources();
        if( isGoingToRemoveSources() ) // is organize operation?
        {
            SqlCollectionLocation* destinationloc = dynamic_cast<SqlCollectionLocation*>( destination() );
            src = destinationloc->m_originalUrls[track];
            if( src == track->playableUrl() ) {
                debug() << "src == dst";
                break;
            }
        }

        KIO::DeleteJob *job = 0;

        src.cleanPath();
        debug() << "deleting  " << src;
        KIO::JobFlags flags = KIO::HideProgressInfo;
        job = KIO::del( src, flags );
        if( job )   //just to be safe
        {
            connect( job, SIGNAL( result( KJob* ) ), SLOT( slotRemoveJobFinished( KJob* ) ) );
            QString name = track->prettyName();
            if( track->artist() )
                name = QString( "%1 - %2" ).arg( track->artist()->name(), track->prettyName() );

            Amarok::Components::logger()->newProgressOperation( job, i18n( "Removing: %1", name ) );
            m_removejobs.insert( job, track );
            return true;
        }
        break;
    }
    return false;
}


TransferJob::TransferJob( SqlCollectionLocation * location )
    : KCompositeJob( 0 )
    , m_location( location )
    , m_killed( false )
{
    setCapabilities( KJob::Killable );
}

bool TransferJob::addSubjob(KJob* job)
{
    return KCompositeJob::addSubjob(job);
}

void TransferJob::emitInfo(const QString& message)
{
    emit infoMessage( this, message );
}


void TransferJob::start()
{
    DEBUG_BLOCK
    if( m_location == 0 )
    {
        setError( 1 );
        setErrorText( "Location is null!" );
        emitResult();
        return;
    }
    QTimer::singleShot( 0, this, SLOT( doWork() ) );
}

void TransferJob::doWork()
{
    DEBUG_BLOCK
    setTotalAmount(  KJob::Files, m_location->m_sources.size() );
    setProcessedAmount( KJob::Files, 0 );
    if( !m_location->startNextJob() )
    {
        if( !hasSubjobs() )
            emitResult();
    }
}

void TransferJob::slotJobFinished( KJob* job )
{
    DEBUG_BLOCK
    if( job )
        removeSubjob( job );
    if( m_killed )
    {
        debug() << "slotJobFinished entered, but it should be killed!";
        return;
    }
    setProcessedAmount( KJob::Files, processedAmount( KJob::Files ) + 1 );
    emitPercent( processedAmount( KJob::Files ), totalAmount( KJob::Files ) );
    debug() << "processed" << processedAmount( KJob::Files ) << " totalAmount" << totalAmount( KJob::Files );
    if( !m_location->startNextJob() )
    {
        debug() << "sources empty";
        // don't quit if there are still subjobs
        if( !hasSubjobs() )
            emitResult();
        else
            debug() << "have subjobs";
    }
}

bool TransferJob::doKill()
{
    DEBUG_BLOCK
    m_killed = true;
    foreach( KJob* job, subjobs() )
    {
        job->kill();
    }
    clearSubjobs();
    return KJob::doKill();
}

#include "SqlCollectionLocation.moc"
