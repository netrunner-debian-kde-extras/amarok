/****************************************************************************************
 * Copyright (c) 2005,2006 Martin Aumueller <aumuell@reserv.at>                         *
 * Copyright (c) 2004 Christian Muehlhaeuser <chris@chris.de>                           *
 * Copyright (c) 2008 Alejandro Wainzinger <aikawarazuni@gmail.com>                     *
 * Copyright (c) 2009 Seb Ruiz <ruiz@kde.org>                                           *
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

#define DEBUG_PREFIX "IpodHandler"

#include "IpodHandler.h"

#include "IpodCollection.h"
#include "CollectionManager.h"
#include "Debug.h"

extern "C" {
#include <glib-object.h> // g_type_init
#ifdef GDK_FOUND
#include <gdk-pixbuf/gdk-pixbuf.h>
#endif
}

#include "SvgHandler.h"

#include "meta/file/File.h" // for KIO file handling

#include <KCodecs> // KMD5
#include <kdiskfreespaceinfo.h>
#include <KIO/Job>
#include <KIO/CopyJob>
#include <KIO/DeleteJob>
#include <KIO/Scheduler>
#include <KIO/NetAccess>
#include <kinputdialog.h>
#include "kjob.h"
#include <KMessageBox>
#include <KPasswordDialog>
#include <KUrl>
#include <threadweaver/ThreadWeaver.h>

#include <solid/device.h>
#include <solid/storageaccess.h>

#include <QAction>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMutexLocker>
#include <QPixmap>
#include <QProcess>
#include <QRegExp>
#include <QString>
#include <QStringList>
#include <QTime>

using namespace Meta;

/// IpodHandler

IpodHandler::IpodHandler( IpodCollection *mc, const QString& mountPoint )
    : MediaDeviceHandler( mc )
    , m_itdb( 0 )
    , m_masterPlaylist( 0 )
    , m_currtracklist( 0 )
    , m_currtrack( 0 )
    , m_staletracksremoved( 0 )
    , m_orphanedadded( 0 )
    , m_filepath()
    , m_capacity( 0.0 )
    , m_currplaylistlist( 0 )
    , m_currplaylist( 0 )
    , m_jobcounter( 0 )
    , m_libtrack( 0 )
    , m_autoConnect( false )
    , m_mountPoint( mountPoint )
    , m_name()
    , m_isShuffle( false )
    , m_isMobile( false )
    , m_isIPhone( false )
    , m_supportsArtwork( false )
    , m_supportsVideo( false )
    , m_rockboxFirmware( false )
    , m_needsFirewireGuid( false )
    , m_dbChanged( false )
    , m_copyFailed( false )
    , m_isCanceled( false )
    , m_wait( false )
    , m_trackCreated( false )
    , m_tempdir( new KTempDir() )
{
    DEBUG_BLOCK

    m_copyingthreadsafe = false;

    g_type_init();
    m_success = false;
}

IpodHandler::~IpodHandler()
{
    DEBUG_BLOCK
    delete m_tempdir;
    // Write to DB before closing, for ratings updates etc.
    debug() << "Cleaning up Ipod Database";
    writeITunesDB( false );
    if ( m_itdb )
        itdb_free( m_itdb );
}

void
IpodHandler::init()
{
    if( m_mountPoint.isEmpty() )
    {
        debug() << "Error: empty mountpoint, probably an unmounted iPod, aborting";
        m_memColl->slotAttemptConnectionDone( false );
        return;
    }

    GError *err = 0;
    QString initError = i18n( "iPod was not initialized:" );
    QString initErrorCaption = i18n( "iPod Initialization Failed" );
    bool wasInitialized = false;

    // First attempt to parse the database

    debug() << "Calling the db parser";
    m_itdb = itdb_parse( QFile::encodeName( m_mountPoint ),  &err );

    // If this fails, we will ask the user if he wants to init the device

    if( err )
    {
        debug() << "There was an error, attempting to free db: " << err->message;
        g_error_free( err );
        if ( m_itdb )
        {
            itdb_free( m_itdb );
            m_itdb = 0;
        }

        // Attempt to init the device

        // TODO: turn into a switch statement, this is too convoluted

        QString msg = i18n(  "Media Device: could not find iTunesDB on device mounted at %1. "
                             "Attempt to initialize your iPod?" ).arg(  mountPoint() );
        if( KMessageBox::warningContinueCancel( 0, msg, i18n( "Initialize iPod?" ),
                                                KGuiItem( i18n( "&Initialize" ), "new" ) ) == KMessageBox::Continue )
        {
            QStringList modelList;

            // Pull model information

            const Itdb_IpodInfo *info = itdb_info_get_ipod_info_table();

            if( !info )
            {
                debug() << "libgpod failed to get the ipod info table, that's a libgpod bug, should never happen";
                m_success = false;
                m_memColl->slotAttemptConnectionDone( m_success );
                return;
            }

            // Iterate through the models to prepare model strings to display to the user

            while( info->model_number )
            {
                QString mod;
                QTextStream model( &mod );
                model << QString::fromUtf8( itdb_info_get_ipod_generation_string( info->ipod_generation) )
                        << ": "
                        << QString::number( info->capacity )
                        << " GB "
                        << QString::fromUtf8( itdb_info_get_ipod_model_name_string( info->ipod_model ) )
                        << "(x"
                        << QString::fromUtf8( info->model_number )
                        << ")";


                modelList << mod;

                info++;
            }

            bool ok = false;

            // Present the dialog to the user

            QString item = KInputDialog::getItem( i18n( "Set iPod Model"), i18n( "iPod Models" ), modelList, 0, false, &ok, 0 );

            if( !ok )
            {
                KMessageBox::error( 0, i18n( "%1 the iPod Model is necessary to initialize the iPod", initError ), initErrorCaption );
                m_success = false;
                m_memColl->slotAttemptConnectionDone( m_success );
                return;
            }

            // Pull out the model number based on the index

            info = itdb_info_get_ipod_info_table();
            QString modelnum = QString::fromUtf8( info[ modelList.indexOf( item ) ].model_number );

            debug() << "Model number: " << modelnum;

            // Prepare to create the SysInfo file

            // First, ensure the directories exist

            QDir root( QDir::rootPath() );
            QDir dir( mountPoint() + "/iPod_Control/Device" );
            // Check if directory exists
            if ( !dir.exists() )
            {
                debug() << "Creating device dir, since doesn't exist";
                // If it doesn't exist, make it and the path to it
                if ( !root.mkpath( dir.absolutePath() ) )
                {
                    KMessageBox::error( 0, i18n( "%1 failed to write to iPod, make sure you have write permissions on the iPod", initError ), initErrorCaption );
                    debug() << "Creating directory failed";
                    m_success = false;
                    m_memColl->slotAttemptConnectionDone( m_success );
                    return;
                }
                else
                    debug() << "Directory created: " << dir.absolutePath();
            }

            // Now we stick this information into the SysInfo file

            bool wrote = writeToSysInfoFile( "ModelNumStr: x" + modelnum + '\n' );

            if( !wrote )
            {
                KMessageBox::error( 0, i18n( "%1 failed to write SysInfo file to iPod, make sure you have write permissions on the iPod", initError ), initErrorCaption );
                debug() << "Failed to write modelnum to sysinfo file";
                m_success = false;
                m_memColl->slotAttemptConnectionDone( m_success );
                return;
            }

            // Now we stick in the firewireguid information (if available)


            debug() << "Writing the firewireguid";
            wrote = writeFirewireGuid();

            if( !wrote )
                debug() << "warning: could not write firewire guid, but perhaps it's just missing";

            if ( !initializeIpod() )
            {
                if ( m_itdb )
                {
                    itdb_free( m_itdb );
                    m_itdb = 0;
                }

                KMessageBox::error( 0, i18n( "%1 failed to initialize the iPod", initError ), initErrorCaption );

                m_success = false;
                m_memColl->slotAttemptConnectionDone( m_success );
                return;
            }
            else
            {
                KMessageBox::information( 0, i18n( "The iPod was successfully initialized." ), i18n( "iPod Initialized" ) );
                debug() << "iPod was initialized";
                wasInitialized = true;
                m_success = true;
            }
        }
        else
        {
            KMessageBox::information( 0, i18n( "%1 you chose not to initialize the iPod. It will not be usable until it is initialized.", initError), initErrorCaption );
            m_success = false;
        }
    }
    else
        m_success = true;

    // If failed to parse or init, we have failed, return

    if ( !m_success )
    {
        m_memColl->slotAttemptConnectionDone( m_success );
        return;
    }

    m_success = true;

    // Either db was parsed, or initialized. Prepare variables, get model info

    m_tempdir->setAutoRemove( true );

    // read device info
    debug() << "Getting model information";
    detectModel(); // get relevant info about device

    qsrand( QTime::currentTime().msec() ); // random number used for folder number generation

    // Get storage access for getting device space capacity/usage

    Solid::Device device = Solid::Device( m_memColl->udi() );
    if( device.isValid() )
    {
        Solid::StorageAccess *storage = device.as<Solid::StorageAccess>();
        m_filepath = storage->filePath();
        m_capacity = KDiskFreeSpaceInfo::freeSpaceInfo( m_filepath ).size();
    }
    else
    {
        m_filepath = "";
        m_capacity = 0.0;
    }

    debug() << "Succeeded: " << m_success;

    m_memColl->slotAttemptConnectionDone( m_success );
}

bool
IpodHandler::isWritable() const
{
    // TODO: check if read-only
    return true;
}

QString
IpodHandler::prettyName() const
{
    return QString::fromUtf8( itdb_playlist_mpl( m_itdb )->name );
}

QList<QAction *>
IpodHandler::collectionActions()
{
    QList< QAction* > actions;

    if( isWritable() )
    {
        QAction *staleOrphanedAction = new QAction( KIcon( "media-track-edit-amarok" ), i18n( "&Stale and Orphaned" ), this );
        staleOrphanedAction->setProperty( "popupdropper_svg_id", "edit" );

        connect( staleOrphanedAction, SIGNAL( triggered() ), this, SLOT( slotStaleOrphaned() ) );

        actions.append( staleOrphanedAction );

#if 0
        if( m_supportsArtwork )
        {
            QAction *syncArtworkAction = new QAction( KIcon( "insert-image" ), i18n( "Synchronize Artwork" ), this );
            syncArtworkAction->setProperty( "popupdropper_svg_id", "edit" );

            connect( syncArtworkAction, SIGNAL( triggered() ), this, SLOT( slotSyncArtwork() ) );

            actions.append( syncArtworkAction );
        }
#endif
    }

    return actions;
}

void
IpodHandler::slotInitializeIpod()
{
    const QString text( i18n( "Do you really want to initialize this iPod? Its database will be cleared of all information, but the files will not be deleted." ) );

    const bool init = KMessageBox::warningContinueCancel(0,
                                                         text,
                                                         i18n("Initialize iPod") ) == KMessageBox::Continue;
    if( init )
    {
        const bool success = initializeIpod();

        if ( success )
            The::statusBar()->shortMessage( i18n( "The iPod has been initialized" ) );
        else
            The::statusBar()->shortMessage( i18n( "The iPod was unable to be initialized" ) );
    }
}

void
IpodHandler::slotStaleOrphaned()
{
    DEBUG_BLOCK

    const QString text( i18n( "Amarok is about to search for stale tracks. This may take a while, do you want to continue?" ) );

    const bool init = KMessageBox::warningContinueCancel(0,
                                                         text,
                                                         i18n("Find Stale Tracks") ) == KMessageBox::Continue;

    if( init )
        ThreadWeaver::Weaver::instance()->enqueue( new OrphanedWorkerThread( this ) );
}

bool
IpodHandler::findStale()
{
    m_staletracks.clear();
    m_staletracks = staleTracks();

    return true;
}

bool
IpodHandler::findOrphaned()
{
    m_orphanedPaths.clear();
    m_orphanedPaths = orphanedTracks();
    return true;
}

bool
IpodHandler::addNextOrphaned()
{
    DEBUG_BLOCK
    QString realPath;
    QString path = m_orphanedPaths.takeFirst();
    if( !pathExists( path, &realPath ) )
        return false;

    // Create track based on URL

    Meta::TrackPtr filetrack( new MetaFile::Track( realPath ) );

    // Create new track

    Meta::MediaDeviceTrackPtr destTrack ( new Meta::MediaDeviceTrack( m_memColl ) );

    // Create a track struct, associate it to track

    libCreateTrack( destTrack );

    // Fill the track struct of the destTrack with info from the filetrack as source

    setBasicMediaDeviceTrackInfo( filetrack, destTrack );

    // set up the play url

    m_itdbtrackhash[ destTrack ]->ipod_path = g_strdup( path.toLatin1() );

    // Add the track struct into the database

    addTrackInDB( destTrack );

    // A track has been added to the db

    setDatabaseChanged();

    // Add the new Meta::MediaDeviceTrackPtr into the device collection

    // add track to collection
    addMediaDeviceTrackToCollection( destTrack );

    m_orphanedadded++;

    return true;
}

void
IpodHandler::slotOrphaned()
{
    writeDatabase();

    const QString msg( i18ncp( "@info", "One stale track removed from the database. Scan for orphaned tracks?",
                                "%1 tracks removed from the database. Scan for orphaned tracks?", m_staletracksremoved ) );

    const bool init = KMessageBox::warningContinueCancel(0,
                                                         msg,
                                                         i18n("Find Orphaned Tracks") ) == KMessageBox::Continue;

    if( init )
        ThreadWeaver::Weaver::instance()->enqueue( new OrphanedWorkerThread( this ) );
}

bool
IpodHandler::syncArtwork()
{
    DEBUG_BLOCK

    Amarok::Collection *localCollection = CollectionManager::instance()->primaryCollection();

    if( !localCollection )
        return false;

    Meta::AlbumMap albumMap = m_memColl->albumMap();

    foreach( Meta::AlbumPtr album, albumMap.values() )
    {
        debug() << "Updating artwork for" << (album->albumArtist() ? album->albumArtist()->name() : "unknown") << "-" << album->name();
    }

    return true;
}

void
IpodHandler::slotSyncArtwork()
{
    DEBUG_BLOCK

    const QString text( i18n( "Amarok is about to synchronize artwork on <i>%1</i>. Do you want to continue?", prettyName() ) );

    if( KMessageBox::warningContinueCancel(0, text, i18n("Synchronize Artwork") ) == KMessageBox::Continue )
        ThreadWeaver::Weaver::instance()->enqueue( new SyncArtworkWorkerThread( this ) );
}

void
IpodHandler::slotSyncArtworkFailed( ThreadWeaver::Job *job )
{
    Q_UNUSED( job )

    const QString msg( i18n( "iPod artwork could not be synchronized" ) );
    The::statusBar()->shortMessage( msg );
}


void
IpodHandler::slotSyncArtworkSucceeded( ThreadWeaver::Job *job )
{
    Q_UNUSED( job )

    writeDatabase();
    const QString msg( i18n( "Artwork synchronized" ) );
    The::statusBar()->shortMessage( msg );
}

bool
IpodHandler::initializeIpod()
{
    DEBUG_BLOCK
    QDir dir( mountPoint() );
    if( !dir.exists() )
    {
        debug() << "Media device: Mount point does not exist!";
        return false;
    }

    debug() << "initializing iPod mounted at " << mountPoint();

    // initialize iPod
    m_itdb = itdb_new();
    if( !m_itdb )
        return false;

    // in order to get directories right
    //detectModel();

    itdb_set_mountpoint(m_itdb, QFile::encodeName(mountPoint()));

    Itdb_Playlist *mpl = itdb_playlist_new("iPod", false);
    itdb_playlist_set_mpl(mpl);
    Itdb_Playlist *podcasts = itdb_playlist_new("Podcasts", false);
    itdb_playlist_set_podcasts(podcasts);
    itdb_playlist_add(m_itdb, podcasts, -1);
    itdb_playlist_add(m_itdb, mpl, 0);

    QString realPath;
    if(!pathExists( itunesDir(), &realPath) )
    {
        dir.setPath( realPath );
        dir.mkdir( dir.absolutePath() );
    }
    if( !dir.exists() )
        return false;

    if( !pathExists( itunesDir( "Music" ), &realPath ) )
    {
        dir.setPath( realPath );
        dir.mkdir( dir.absolutePath() );
    }
    if( !dir.exists() )
        return false;

    if( !pathExists( itunesDir( "iTunes" ), &realPath ) )
    {
        dir.setPath( realPath );
        dir.mkdir( dir.absolutePath() );
    }
    if( !dir.exists() )
        return false;

    m_dbChanged = true;

    // NOTE: writing itunes DB allowed to block since
    // initializing a device is rare, and requires focus
    // to minimize possible error
    // TODO: database methods abstraction needed
    if( !writeITunesDB( false ) )
       return false;

    return true;
}

void
IpodHandler::detectModel()
{
    DEBUG_BLOCK
    // set some sane default values
    m_isShuffle = false;

    m_supportsArtwork = true;

    m_supportsVideo = false;
    m_isIPhone = false;
    m_needsFirewireGuid = false;
    m_rockboxFirmware = false;

    // needs recent libgpod-0.3.3 from cvs
    bool guess = false;
    if( m_itdb && m_itdb->device )
    {
        debug() << "Attempting to get info...";
        const Itdb_IpodInfo *ipodInfo = itdb_device_get_ipod_info( m_itdb->device );
        debug() << "Got ipodinfo";
        const gchar *modelString = 0;
        #ifdef GDK_FOUND
        m_supportsArtwork = itdb_device_supports_artwork( m_itdb->device );
        #else
        m_supportsArtwork = false;
        #endif
        debug() << "Supports Artwork: " << ( m_supportsArtwork ? "true" : "false" );
        QString musicdirs;
        musicdirs.setNum( itdb_musicdirs_number(m_itdb) );
        debug() << "Musicdirs: " << musicdirs;

        if( ipodInfo )
        {
            debug() << "Checking info...";
            debug() << "Capacity is: " << ipodInfo->capacity;
            modelString = itdb_info_get_ipod_model_name_string ( ipodInfo->ipod_model );

            debug() << "Ipod model: " << QString::fromUtf8( modelString );

            switch( ipodInfo->ipod_model )
            {
            case ITDB_IPOD_MODEL_SHUFFLE:
            case ITDB_IPOD_MODEL_SHUFFLE_SILVER:
            case ITDB_IPOD_MODEL_SHUFFLE_PINK:
            case ITDB_IPOD_MODEL_SHUFFLE_BLUE:
            case ITDB_IPOD_MODEL_SHUFFLE_GREEN:
            case ITDB_IPOD_MODEL_SHUFFLE_ORANGE:
            case ITDB_IPOD_MODEL_SHUFFLE_PURPLE:
                m_isShuffle = true;
                break;

            case ITDB_IPOD_MODEL_IPHONE_1:
            //TODO newer libgpod thinks that an iPod touch is silver, older that it's black
            //case ITDB_IPOD_MODEL_TOUCH_BLACK:
            //case ITDB_IPOD_MODEL_TOUCH_SILVER:
                m_isIPhone = true;
                debug() << "detected iPhone/iPod Touch" << endl;
                break;

            case ITDB_IPOD_MODEL_CLASSIC_SILVER:
            case ITDB_IPOD_MODEL_CLASSIC_BLACK:
                debug() << "detected iPod classic";
            case ITDB_IPOD_MODEL_VIDEO_WHITE:
            case ITDB_IPOD_MODEL_VIDEO_BLACK:
            case ITDB_IPOD_MODEL_VIDEO_U2:
                m_supportsVideo = true;
                debug() << "detected video-capable iPod";
                break;

            case ITDB_IPOD_MODEL_MOBILE_1:
                m_isMobile = true;
                debug() << "detected iTunes phone" << endl;
                break;

            case ITDB_IPOD_MODEL_INVALID:
            case ITDB_IPOD_MODEL_UNKNOWN:
                modelString = 0;
                guess = true;
                break;

            default:
                break;
            }


            debug() << "Generation is: " << ipodInfo->ipod_generation;
            switch( ipodInfo->ipod_generation )
            {
               case ITDB_IPOD_GENERATION_CLASSIC_1:
               case ITDB_IPOD_GENERATION_NANO_3:
               case ITDB_IPOD_GENERATION_TOUCH_1:
                  m_needsFirewireGuid = true;
                  m_supportsVideo = true;
                  break;
               case ITDB_IPOD_GENERATION_VIDEO_1:
               case ITDB_IPOD_GENERATION_VIDEO_2:
                  m_supportsVideo = true;
                  break;
               case ITDB_IPOD_GENERATION_SHUFFLE_1:
               case ITDB_IPOD_GENERATION_SHUFFLE_2:
               case ITDB_IPOD_GENERATION_SHUFFLE_3:
                  m_isShuffle = true;
                  break;
               default:
                  break;
            }

        }
        if( modelString )
            m_name = QString( "iPod %1" ).arg( QString::fromUtf8( modelString ) );

        if( m_needsFirewireGuid )
        {
            gchar *fwid = itdb_device_get_sysinfo( m_itdb->device, "FirewireGuid" );
            if( fwid )
               g_free( fwid );
        }
    }
    else
    {
        debug() << "iPod type detection failed, no video support";
        m_needsFirewireGuid = true; // can't read db because no firewire, maybe
        guess = true;
    }

    if( guess )
    {
        if( pathExists( ":iTunes:iTunes_Control" ) )
        {
            debug() << "iTunes/iTunes_Control found - assuming itunes phone" << endl;
            m_isMobile = true;
        }
        else if( pathExists( ":iTunes_Control" ) )
        {
            debug() << "iTunes_Control found - assuming iPhone/iPod Touch" << endl;
            m_isIPhone = true;
        }
    }

    if( m_isIPhone )
    {
        #ifdef GDK_FOUND
        m_supportsArtwork = true;
        #else
        m_supportsArtwork = false;
        #endif
        m_supportsVideo = true;
    }

    if( pathExists( ":.rockbox" ) )
    {
        debug() << "RockBox firmware detected" << endl;
        m_rockboxFirmware = true;
    }
}

bool
IpodHandler::writeToSysInfoFile( const QString &text )
{
    DEBUG_BLOCK
    QFile sysinfofile( mountPoint() + "/iPod_Control/Device/SysInfo" );

    if (!sysinfofile.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        debug() << "Failed to open SysInfo file for writing!";
        return false;
    }

    QTextStream out( &sysinfofile );

    out << text;

    sysinfofile.close();

    return true;

}

bool
IpodHandler::appendToSysInfoFile( const QString &text )
{
    DEBUG_BLOCK
    QFile sysinfofile( mountPoint() + "/iPod_Control/Device/SysInfo" );

    if (!sysinfofile.open(QIODevice::Append | QIODevice::Text))
    {
        debug() << "Failed to open SysInfo file for appending!";
        return false;
    }

    QTextStream out( &sysinfofile );

    out << text;

    sysinfofile.close();

    return true;
}

bool
IpodHandler::writeFirewireGuid()
{
    DEBUG_BLOCK

    // HACK: KDesu's SuProcess is not well-documented enough to use
    // for elevated privileges, so we are assuming the user has sudo set up
    // for now, which is a _very big_ assumption.

    KPasswordDialog dlg( 0 );
    dlg.setPrompt(i18n("Amarok requires sudo access to get the FirewireGuid required to connect to your device. Please enter your sudo password"));
    if( !dlg.exec() )
    {
        debug() << "sudo dialog cancelled";
        return false; //the user canceled
    }

    //QString program = "sudo";

    //QStringList arguments;
    //arguments << "lsusb" << "-s" << "-v";

    QProcess lsusb;
    lsusb.start( "sudo -S lsusb -v" );
    if (!lsusb.waitForStarted())
    {
        debug() << "failed to start sudo lsusb call";
        return false;
    }

    // Write the password to run the command

    lsusb.write( dlg.password().toUtf8() );
    lsusb.closeWriteChannel();

    // Wait until sudo has processed the password
    if (!lsusb.waitForFinished())
    {
        debug() << "failed to write password to sudo";
        return false;
    }

    // Check if sudo fails to execute, usually due to wrong password or bad/no sudoers file

    QByteArray lsusbinfoarray = lsusb.readAllStandardOutput();

    QString lsusbinfostring( lsusbinfoarray );

    // If it failed, abort
    if( lsusbinfostring == "1" )
    {
        debug() << "sudo failed to run, probably due to a wrong password";
        return false;
    }

    QRegExp rx( "iSerial\\s*[0-9] [0-9A-Z]{5}[0-9A-Z]+" );
    QString firewireguid;

    int pos = lsusbinfostring.indexOf( rx );

    if( pos != -1 )
    {
        QString iserial = rx.capturedTexts().first();
        rx.setPattern( "[0-9A-Z]{2,}" );
        pos = iserial.indexOf( rx );
        if( pos != -1 )
        {
            debug() << rx.capturedTexts();
            firewireguid = "FirewireGuid: 0x" + rx.capturedTexts().first();
        }

    }

    debug() << "Firewire is: " << firewireguid;

    // If found, write it out to the SysInfo file
    if( firewireguid.isEmpty() )
        return false;

    return appendToSysInfoFile( firewireguid );
}

bool
IpodHandler::pathExists( const QString &ipodPath, QString *realPath )
{
    QDir curDir( mountPoint() );
    QString curPath = mountPoint();
    QStringList components;
    // HACK: remove a trailing ':' if present. For some inexplicable
    // reason, splitting when starting with a : fails in Amarok,
    // even though it works in a small isolated Qt test case.
    if( ipodPath.at( 0 ) == ':' )
        components = QString( ipodPath ).remove(0, 1).split(':');
    else
        components = ipodPath.split( ':' );

    bool found = false;
    QStringList::iterator it = components.begin();
    for( ; it != components.end(); ++it )
    {
        found = false;
        for( uint i = 0;i < curDir.count(); i++ )
        {
            if( curDir[i].toLower() == (*it).toLower() )
            {
                curPath += '/' + curDir[i];
                curDir.cd( curPath );
                found = true;
                break;
            }
        }
        if(!found)
            break;
    }

    for( ; it != components.end(); ++it )
    {
        curPath += '/' + *it;
    }

    if( realPath )
        *realPath = curPath;

    return found;
}

bool
IpodHandler::writeITunesDB( bool threaded )
{
    DEBUG_BLOCK

    QMutexLocker locker( &m_dbLocker );
    if( !m_itdb )
        return false;

    if( m_dbChanged )
    {
        bool ok = false;
        if( !threaded )
        {
            if( !m_itdb )
                return false;

            ok = true;
            GError *error = 0;
            if ( !itdb_write (m_itdb, &error) )
            {   /* an error occurred */
                if(error)
                {
                    if (error->message)
                        debug() << "itdb_write error: " << error->message;
                    else
                        debug() << "itdb_write error: error->message == 0!";
                    g_error_free (error);
                }
                error = 0;
                ok = false;
            }

            if( m_isShuffle )
            {
                /* write shuffle data */
                if (!itdb_shuffle_write (m_itdb, &error))
                {   /* an error occurred */
                    if(error)
                    {
                        if (error->message)
                            debug() << "itdb_shuffle_write error: " << error->message;
                        else
                            debug() << "itdb_shuffle_write error: error->message == 0!";
                        g_error_free (error);
                    }
                    error = 0;
                    ok = false;
                }
            }
            // Kill status bar only once DB is written
            //emit databaseWritten( this );
        }

        if( ok )
            m_dbChanged = false;
        else
            debug() << "Failed to write iPod database";

        return ok;
    }

    debug() << "Database was not changed, will not flush";

    return false;
}

QString
IpodHandler::itunesDir(const QString &p) const
{
    QString base( ":iPod_Control" );
    if( m_isMobile )
        base = ":iTunes:iTunes_Control";

    if( !p.startsWith( ':' ) )
        base += ':';
    return base + p;
}

/// Finds path to copy track to on Ipod
void
IpodHandler::findPathToCopy( const Meta::TrackPtr &srcTrack, const Meta::MediaDeviceTrackPtr &destTrack )
{
    Q_UNUSED( destTrack )
        debug() << "Mountpoint is: " << mountPoint();

    KUrl url = determineURLOnDevice(srcTrack);

    debug() << "Url's path is: " << url.path();

    QFileInfo finfo( url.path() );
    QDir dir = finfo.dir();
    QDir root( QDir::rootPath() );
    // Check if directory exists
    if ( !dir.exists() )
    {
        // If it doesn't exist, make it and the path to it
        if ( !root.mkpath( dir.absolutePath() ) )
        {
            debug() << "Creating directory failed";
            url = "";
        }
        // If fails to create, set its url to blank so the copying will fail
        else
            debug() << "Directory created!";
    }

    debug() << "About to copy from: " << srcTrack->playableUrl().path();
    debug() << "to: " << url;

    m_trackdesturl[ srcTrack ] = url;
}

bool
IpodHandler::libCopyTrack( const Meta::TrackPtr &srcTrack, Meta::MediaDeviceTrackPtr &destTrack )
{
    Q_UNUSED( destTrack )
    DEBUG_BLOCK
//    findPathToCopy( srcTrack );
    KUrl srcurl = KUrl::fromPath( srcTrack->playableUrl().path() );
    m_trackscopying[ srcurl ] = srcTrack;
    return kioCopyTrack( srcurl, m_trackdesturl[ srcTrack ] );
}

void
IpodHandler::writeDatabase()
{
    disconnect( this, SIGNAL( removeTracksDone() ), 0, 0 );
    ThreadWeaver::Weaver::instance()->enqueue( new DBWorkerThread( this ) );
}

void
IpodHandler::addTrackInDB(const Meta::MediaDeviceTrackPtr& track)
{
    DEBUG_BLOCK

    debug() << "Adding " << QString::fromUtf8( m_itdbtrackhash[ track ]->artist) << " - " << QString::fromUtf8( m_itdbtrackhash[ track ]->title );
    itdb_track_add(m_itdb, m_itdbtrackhash[ track ], -1);

    // TODO: podcasts NYI
    // if(podcastInfo)
#if 0
    if( false )
    {
        Itdb_Playlist *podcasts = itdb_playlist_podcasts(m_itdb);
        if(!podcasts)
        {
            podcasts = itdb_playlist_new("Podcasts", false);
            itdb_playlist_add(m_itdb, podcasts, -1);
            itdb_playlist_set_podcasts(podcasts);
        }
        itdb_playlist_add_track(podcasts, ipodtrack, -1);
    }
    else
#endif
    {
        // gtkpod 0.94 does not like if not all songs in the db are on the master playlist
        // but we try anyway
        Itdb_Playlist *mpl = itdb_playlist_mpl(m_itdb);
        if( !mpl )
        {
            mpl = itdb_playlist_new( "iPod", false );
            itdb_playlist_add( m_itdb, mpl, -1 );
            itdb_playlist_set_mpl( mpl );
        }
        itdb_playlist_add_track(mpl, m_itdbtrackhash[ track ], -1);
    }


}

bool
IpodHandler::libDeleteTrackFile( const Meta::MediaDeviceTrackPtr &track )
{
    DEBUG_BLOCK
    Itdb_Track *ipodtrack = m_itdbtrackhash[ track ];

    // delete file
    KUrl url;
    url.setPath( realPath( ipodtrack->ipod_path ) );
    Meta::TrackPtr trackptr = Meta::TrackPtr::staticCast( track );
    m_tracksdeleting[ url ] = trackptr;
    deleteFile( url );

    return true;

}

void
IpodHandler::libDeleteTrack( const Meta::MediaDeviceTrackPtr &track )
{
    DEBUG_BLOCK
    Itdb_Track *ipodtrack = m_itdbtrackhash[ track ];

    m_itdbtrackhash.remove( track );
    m_files.remove( QString(ipodtrack->ipod_path).toLower() );

    itdb_track_remove( ipodtrack );
}

void
IpodHandler::removeTrackFromDB( const Meta::MediaDeviceTrackPtr &track )
{
    DEBUG_BLOCK
    Itdb_Track *ipodtrack = m_itdbtrackhash[ track ];

    removeDBTrack( ipodtrack );
}

inline void
IpodHandler::setDatabaseChanged()
{
    m_dbChanged = true;
}

Meta::TrackList
IpodHandler::staleTracks()
{
    DEBUG_BLOCK
    Meta::TrackList tracklist;

    Meta::TrackMap trackMap = m_memColl->trackMap();

    foreach( Meta::TrackPtr trk, trackMap.values() )
    {
        Meta::MediaDeviceTrackPtr track = Meta::MediaDeviceTrackPtr::staticCast( trk );
        Itdb_Track *ipodtrack =  m_itdbtrackhash.value( track );

        if( !pathExists(QString::fromUtf8( ipodtrack->ipod_path ) ) )
        {
            debug() << "track: " << ipodtrack->artist << " - " << ipodtrack->album << " - " << ipodtrack->title << " is stale: " << ipodtrack->ipod_path << " does not exist" << endl;
            tracklist << trk;
        }
    }

    return tracklist;
}

QStringList
IpodHandler::orphanedTracks()
{
    DEBUG_BLOCK

    QStringList orphanedTracks;
    QString musicpath;
    if (!pathExists( itunesDir( "Music" ), &musicpath ))
    {
        debug() << "Music path not found";
        return QStringList();
    }

    debug() << "Found path for Music";

    QDir dir( musicpath, QString(), QDir::Unsorted, QDir::Dirs );
    for(unsigned i=0; i<dir.count(); i++)
    {
        if(dir[i] == "." || dir[i] == "..")
            continue;

        QString hashpath = musicpath + '/' + dir[i];
        QDir hashdir( hashpath, QString(), QDir::Unsorted, QDir::Files );
        for(unsigned j=0; j<hashdir.count(); j++)
        {
            QString filename = hashpath + '/' + hashdir[j];
            QString ipodPath = itunesDir( "Music:" ) + dir[i] + ':' + hashdir[j];
            Itdb_Track *track = m_files.value( ipodPath.toLower() );
            if(!track)
            {
                debug() << "file: " << filename << " is orphaned" << endl;
                orphanedTracks << ipodPath;
            }
        }
    }

    return orphanedTracks;
}

bool
IpodHandler::removeDBTrack( Itdb_Track *track )
{
    if( !m_itdb || !track )
        return false;

    if( track->itdb != m_itdb )
        return false;

    m_dbChanged = true;

    Itdb_Playlist *mpl = itdb_playlist_mpl(m_itdb);
    while(itdb_playlist_contains_track(mpl, track))
        itdb_playlist_remove_track(mpl, track);

    GList *cur = m_itdb->playlists;
    while(cur)
    {
        Itdb_Playlist *pl = (Itdb_Playlist *)cur->data;
        while(itdb_playlist_contains_track(pl, track))
            itdb_playlist_remove_track(pl, track);
        cur = cur->next;
    }

    return true;
}

bool
IpodHandler::kioCopyTrack( const KUrl &src, const KUrl &dst )
{
    DEBUG_BLOCK

    debug() << "Copying from *" << src << "* to *" << dst << "*";

    KIO::CopyJob *job = KIO::copy( src, dst, KIO::HideProgressInfo );
    m_jobcounter++;

    if( m_jobcounter < 150 )
        copyNextTrackToDevice();


    connect( job, SIGNAL( result( KJob * ) ),
             this,  SLOT( fileTransferred( KJob * ) ), Qt::QueuedConnection );

    connect( job, SIGNAL( copyingDone(KIO::Job*,KUrl,KUrl,time_t,bool,bool)),
             this, SLOT(slotCopyingDone(KIO::Job*,KUrl,KUrl,time_t,bool,bool)) );

    return true;
}

void
IpodHandler::fileTransferred( KJob *job )  //SLOT
{
    DEBUG_BLOCK
    QMutexLocker locker(&m_joblocker);

    m_wait = false;

    m_jobcounter--;

    if ( job->error() )
    {
        m_copyFailed = true;
        debug() << "file transfer failed: " << job->errorText();
        return;
    }

    // Limit max number of jobs to 150, make sure more tracks left
    // to copy
    debug() << "Tracks to copy still remain";
    if( m_jobcounter < 150 )
    {
        debug() << "Jobs: " << m_jobcounter;
        copyNextTrackToDevice();
    }
}

void
IpodHandler::slotCopyingDone( KIO::Job* job, KUrl from, KUrl to, time_t mtime, bool directory, bool renamed)
{
    Q_UNUSED( job )
    Q_UNUSED( to )
    Q_UNUSED( mtime )
    Q_UNUSED( directory )
    Q_UNUSED( renamed )

    DEBUG_BLOCK
    Meta::TrackPtr track = m_trackscopying[from];

    if( !job->error() )
        slotFinalizeTrackCopy( track );
}

void
IpodHandler::deleteFile( const KUrl &url )
{
    DEBUG_BLOCK
    debug() << "deleting " << url.prettyUrl();

    KIO::DeleteJob *job = KIO::del( url, KIO::HideProgressInfo );

    m_jobcounter++;

    if( m_jobcounter < 150 )
        removeNextTrackFromDevice();

    connect( job, SIGNAL( result( KJob * ) ),
             this,  SLOT( fileDeleted( KJob * ) ) );

    return;
}

void
IpodHandler::fileDeleted( KJob *job )  //SLOT
{
    DEBUG_BLOCK
    if( job->error() )
        debug() << "file deletion failed: " << job->errorText();

    m_jobcounter--;

    // Limit max number of jobs to 150, make sure more tracks left
    // to delete
    debug() << "Tracks to delete still remain";
    if( m_jobcounter < 150 )
    {
        debug() << "Jobs: " << m_jobcounter;
        removeNextTrackFromDevice();
    }

    KIO::DeleteJob *djob = reinterpret_cast<KIO::DeleteJob*> (job);

    if( djob )
    {
        KUrl url = djob->urls().first();

        Meta::TrackPtr track = m_tracksdeleting[ url ];

        debug() << "emitting libRemoveTrackDone";

        slotFinalizeTrackRemove( track );
    }
}

KUrl
IpodHandler::determineURLOnDevice( const Meta::TrackPtr &track )
{
    DEBUG_BLOCK
    if( !m_itdb )
    {
        debug() << "m_itdb is NULL";
        return KUrl();
    }

    QString type = track->type();

    QString trackpath;
    QString realpath;
    do
    {
        int num = qrand() % 1000000;
        int music_dirs = itdb_musicdirs_number(m_itdb) > 1 ? itdb_musicdirs_number(m_itdb) : 20;
        int dir = num % music_dirs;
        QString dirname;
        debug() << "itunesDir(): " << itunesDir();
        dirname = QString( "%1Music:F%2" ).arg( "iPod_Control:" ).arg( QString::number( dir, 10 ), 2, QLatin1Char( '0' ) );

        debug() << "Copying to dirname: " << dirname;
        if( !pathExists( dirname ) )
        {
            QString realdir = realPath(dirname.toLatin1());
            QDir qdir( realdir );
            qdir.mkdir( realdir );
        }
        QString filename;
        filename = QString( ":kpod%1.%2" ).arg( QString::number( num, 10 ), 7, QLatin1Char( '0' ) ).arg(type);
        trackpath = dirname + filename;
    }
    while( pathExists( trackpath, &realpath ) );

    return realpath;
}

QString
IpodHandler::ipodPath( const QString &realPath )
{
    if( m_itdb )
    {
        QString mp = QFile::decodeName( itdb_get_mountpoint(m_itdb) );
        if( realPath.startsWith(mp) )
        {
            QString path = realPath;
            path = path.mid(mp.length());
            path = path.replace('/', ":");
            return path;
        }
    }

    return QString();
}

QString
IpodHandler::realPath( const char *ipodPath )
{
    QString path;
    if( m_itdb )
    {
        path = QFile::decodeName(itdb_get_mountpoint(m_itdb));
        path.append(QString(ipodPath).replace(':', "/"));
    }

    return path;
}


QString
IpodHandler::libGetTitle( const Meta::MediaDeviceTrackPtr &track )
{
    return QString::fromUtf8( m_itdbtrackhash[ track ]->title );
}

QString
IpodHandler::libGetAlbum( const Meta::MediaDeviceTrackPtr &track )
{
    return QString::fromUtf8( m_itdbtrackhash[ track ]->album );
}

QString
IpodHandler::libGetArtist( const Meta::MediaDeviceTrackPtr &track )
{
    return QString::fromUtf8( m_itdbtrackhash[ track ]->artist );
}

QString
IpodHandler::libGetComposer( const Meta::MediaDeviceTrackPtr &track )
{
    return QString::fromUtf8( m_itdbtrackhash[ track ]->composer );
}

QString
IpodHandler::libGetGenre( const Meta::MediaDeviceTrackPtr &track )
{
    return QString::fromUtf8( m_itdbtrackhash[ track ]->genre );
}

int
IpodHandler::libGetYear( const Meta::MediaDeviceTrackPtr &track )
{
    return m_itdbtrackhash[ track ]->year;
}

qint64
IpodHandler::libGetLength( const Meta::MediaDeviceTrackPtr &track )
{
    return ( ( m_itdbtrackhash[ track ]->tracklen ) );
}

int
IpodHandler::libGetTrackNumber( const Meta::MediaDeviceTrackPtr &track )
{
    return m_itdbtrackhash[ track ]->track_nr;
}

QString
IpodHandler::libGetComment( const Meta::MediaDeviceTrackPtr &track )
{
    return QString::fromUtf8( m_itdbtrackhash[ track ]->comment );
}

int
IpodHandler::libGetDiscNumber( const Meta::MediaDeviceTrackPtr &track )
{
    return m_itdbtrackhash[ track ]->cd_nr;
}

int
IpodHandler::libGetBitrate( const Meta::MediaDeviceTrackPtr &track )
{
    return m_itdbtrackhash[ track ]->bitrate;
}

int
IpodHandler::libGetSamplerate( const Meta::MediaDeviceTrackPtr &track )
{
    return m_itdbtrackhash[ track ]->samplerate;
}

float
IpodHandler::libGetBpm( const Meta::MediaDeviceTrackPtr &track )
{
    return m_itdbtrackhash[ track ]->BPM;
}
int
IpodHandler::libGetFileSize( const Meta::MediaDeviceTrackPtr &track )
{
    return m_itdbtrackhash[ track ]->size;
}
int
IpodHandler::libGetPlayCount( const Meta::MediaDeviceTrackPtr &track )
{
    return m_itdbtrackhash[ track ]->playcount;
}
uint
IpodHandler::libGetLastPlayed( const Meta::MediaDeviceTrackPtr &track )
{
    return m_itdbtrackhash[ track ]->time_played;
}
int
IpodHandler::libGetRating( const Meta::MediaDeviceTrackPtr &track )
{
    return ( m_itdbtrackhash[ track ]->rating / ITDB_RATING_STEP * 2 );
}
QString
IpodHandler::libGetType( const Meta::MediaDeviceTrackPtr &track )
{
    return QFileInfo( track->playableUrl().path() ).suffix();
}

KUrl
IpodHandler::libGetPlayableUrl( const Meta::MediaDeviceTrackPtr &track )
{
    return KUrl(m_mountPoint + (QString( m_itdbtrackhash[ track ]->ipod_path ).split( ':' ).join( "/" )));
}

float
IpodHandler::usedCapacity() const
{
    if ( !m_filepath.isEmpty() )
        return KDiskFreeSpaceInfo::freeSpaceInfo( m_filepath ).used();
    else
        return 0.0;
}

float
IpodHandler::totalCapacity() const
{
    return m_capacity;
}

/// Sets

void
IpodHandler::libSetTitle( Meta::MediaDeviceTrackPtr& track, const QString& title )
{
    m_itdbtrackhash[ track ]->title = g_strdup( title.toUtf8() );
    setDatabaseChanged();
}
void
IpodHandler::libSetAlbum( Meta::MediaDeviceTrackPtr &track, const QString& album )
{
    m_itdbtrackhash[ track ]->album = g_strdup( album.toUtf8() );
    setDatabaseChanged();
}
void
IpodHandler::libSetArtist( Meta::MediaDeviceTrackPtr &track, const QString& artist )
{
    m_itdbtrackhash[ track ]->artist = g_strdup( artist.toUtf8() );
    setDatabaseChanged();
}
void
IpodHandler::libSetComposer( Meta::MediaDeviceTrackPtr &track, const QString& composer )
{
    m_itdbtrackhash[ track ]->composer = g_strdup( composer.toUtf8() );
    setDatabaseChanged();
}
void
IpodHandler::libSetGenre( Meta::MediaDeviceTrackPtr &track, const QString& genre )
{
    if( genre.startsWith("audiobook", Qt::CaseInsensitive) )
    {
        m_itdbtrackhash[ track ]->remember_playback_position |= 0x01;
        m_itdbtrackhash[ track ]->skip_when_shuffling |= 0x01;
        m_itdbtrackhash[ track ]->mediatype = ITDB_MEDIATYPE_AUDIOBOOK;
    }

    m_itdbtrackhash[ track ]->genre = g_strdup( genre.toUtf8() );
    setDatabaseChanged();
}
void
IpodHandler::libSetYear( Meta::MediaDeviceTrackPtr &track, const QString& year )
{
    bool ok;
    int yr = year.toInt( &ok, 10 );
    if( ok )
    {
        m_itdbtrackhash[ track ]->year = yr;
        setDatabaseChanged();
    }
}
void
IpodHandler::libSetLength( Meta::MediaDeviceTrackPtr &track, int length )
{
    m_itdbtrackhash[ track ]->tracklen = length*1000;
    setDatabaseChanged();
}
void
IpodHandler::libSetTrackNumber( Meta::MediaDeviceTrackPtr &track, int tracknum )
{
    m_itdbtrackhash[ track ]->track_nr = tracknum;
    setDatabaseChanged();
}
void
IpodHandler::libSetComment( Meta::MediaDeviceTrackPtr &track, const QString& comment )
{
    m_itdbtrackhash[ track ]->comment = g_strdup( comment.toUtf8() );
    setDatabaseChanged();
}
void
IpodHandler::libSetDiscNumber( Meta::MediaDeviceTrackPtr &track, int discnum )
{
    m_itdbtrackhash[ track ]->cd_nr = discnum;
    setDatabaseChanged();
}
void
IpodHandler::libSetBitrate( Meta::MediaDeviceTrackPtr &track, int bitrate )
{
    m_itdbtrackhash[ track ]->bitrate = bitrate;
    setDatabaseChanged();
}
void
IpodHandler::libSetSamplerate( Meta::MediaDeviceTrackPtr &track, int samplerate )
{
    m_itdbtrackhash[ track ]->samplerate = samplerate;
    setDatabaseChanged();
}
void
IpodHandler::libSetBpm( Meta::MediaDeviceTrackPtr &track, float bpm )
{
    m_itdbtrackhash[ track ]->BPM = static_cast<int>( bpm );
    setDatabaseChanged();
}
void
IpodHandler::libSetFileSize( Meta::MediaDeviceTrackPtr &track, int filesize )
{
    m_itdbtrackhash[ track ]->size = filesize;
    setDatabaseChanged();
}
void
IpodHandler::libSetPlayCount( Meta::MediaDeviceTrackPtr &track, int playcount )
{
    m_itdbtrackhash[ track ]->playcount = playcount;
    setDatabaseChanged();
}
void
IpodHandler::libSetLastPlayed( Meta::MediaDeviceTrackPtr &track, uint lastplayed)
{
    Q_UNUSED( track )
    Q_UNUSED( lastplayed )
}
void
IpodHandler::libSetRating( Meta::MediaDeviceTrackPtr &track, int rating )
{
    m_itdbtrackhash[ track ]->rating = ( rating * ITDB_RATING_STEP / 2 );
    setDatabaseChanged();
}
void
IpodHandler::libSetType( Meta::MediaDeviceTrackPtr &track, const QString& type )
{
    m_itdbtrackhash[ track ]->mediatype = ITDB_MEDIATYPE_AUDIO;
    bool audiobook = false;

    if( type == "wav" )
        m_itdbtrackhash[ track ]->filetype = g_strdup( "wav" );

    else if( type == "aiff" )
        m_itdbtrackhash[ track ]->filetype = g_strdup( "aiff" );

    else if( type == "mp3" || type == "mpeg")
        m_itdbtrackhash[ track ]->filetype = g_strdup( "mpeg" );

    else if( type == "aac" || type == "m4a" || (!m_supportsVideo && type == "mp4") )
        m_itdbtrackhash[ track ]->filetype = g_strdup( "mp4" );

    else if( type == "m4b" )
    {
        audiobook = true;
        m_itdbtrackhash[ track ]->filetype = g_strdup( "mp4" );
    }
    else if( type == "m4v" || type == "mp4v" || type == "mov" || type == "mpg" || type == "mp4" )
    {
        m_itdbtrackhash[ track ]->filetype = g_strdup( "m4v video" );
        m_itdbtrackhash[ track ]->movie_flag = 0x01; // for videos
        m_itdbtrackhash[ track ]->mediatype = ITDB_MEDIATYPE_MOVIE;
    }
    // TODO: NYI, TagLib calls need to be ported
    /*
    else if(type=="aa")
    {
        audiobook = true;
        m_itdbtrackhash[ track ]->filetype = g_strdup( "audible" );

        TagLib::Audible::File f( QFile::encodeName( url.path() ) );
        TagLib::Audible::Tag *t = f.getAudibleTag();
        if( t )
            m_itdbtrackhash[ track ]->drm_userid = t->userID();
        // libgpod also tries to set those, but this won't work
        m_itdbtrackhash[ track ]->unk126 = 0x01;
        m_itdbtrackhash[ track ]->unk144 = 0x0029;

    }
    */
    else
        m_itdbtrackhash[ track ]->filetype = g_strdup( type.toUtf8() );

    if( audiobook )
    {
        m_itdbtrackhash[ track ]->remember_playback_position |= 0x01;
        m_itdbtrackhash[ track ]->skip_when_shuffling |= 0x01;
        m_itdbtrackhash[ track ]->mediatype = ITDB_MEDIATYPE_AUDIOBOOK;
    }
    setDatabaseChanged();
}

void
IpodHandler::libSetPlayableUrl( Meta::MediaDeviceTrackPtr &destTrack, const Meta::TrackPtr &srcTrack )
{
    KUrl copyurl = m_trackdesturl[ srcTrack ];
    QString pathname = copyurl.path();

    QString type = pathname.section('.', -1).toLower();
    type = type.toLower();

    debug() << "Path before put in ipod_path: " << pathname;

    m_itdbtrackhash[ destTrack ]->ipod_path = g_strdup( ipodPath(pathname).toLatin1() );
    debug() << "on iPod: " << m_itdbtrackhash[ destTrack ]->ipod_path;
    setDatabaseChanged();
}

void
IpodHandler::libCreateTrack( const Meta::MediaDeviceTrackPtr& track )
{
    m_itdbtrackhash[ track ] = itdb_track_new();
}

QString
IpodHandler::ipodArtFilename( const Itdb_Track *ipodtrack ) const
{
    const QString artist = QString::fromUtf8( ipodtrack->artist );
    const QString album  = QString::fromUtf8( ipodtrack->album  );
    KMD5 context( artist.toLower().toLocal8Bit() + album.toLower().toLocal8Bit() );
    const QString imageKey = context.hexDigest();
    return m_tempdir->name() + imageKey + ".png";
}

QPixmap
IpodHandler::libGetCoverArt( const Meta::MediaDeviceTrackPtr &track )
{
#ifdef GDK_FOUND
    Itdb_Track *ipodtrack = m_itdbtrackhash[ track ];
    if( !ipodtrack )
        return QPixmap();

    if( ipodtrack->has_artwork == 0x02 ) // has_artwork: True if set to 0x01, false if set to 0x02.
        return QPixmap();

    const QString filename = ipodArtFilename( ipodtrack );

    if( m_coverArt.contains(filename) )
        return QPixmap(filename);

    GdkPixbuf *pixbuf = (GdkPixbuf*) itdb_artwork_get_pixbuf( ipodtrack->itdb->device, ipodtrack->artwork, -1, -1 );
    if( !pixbuf )
        return QPixmap();

    gdk_pixbuf_save( pixbuf, QFile::encodeName(filename), "png", NULL, (const char*)(NULL));
    gdk_pixbuf_unref( pixbuf );

    m_coverArt.insert( filename );
    return QPixmap( filename );
#else
    Q_UNUSED( track );
    return QPixmap();
#endif
}

void
IpodHandler::libSetCoverArtPath( Meta::MediaDeviceTrackPtr &track, const QString &path )
{
#ifdef GDK_FOUND
    if( path.isEmpty() || !m_supportsArtwork )
        return;

    Itdb_Track *ipodtrack = m_itdbtrackhash[ track ];

    itdb_artwork_remove_thumbnails( ipodtrack->artwork );
    itdb_track_set_thumbnails( ipodtrack, QFile::encodeName(path) );
    ipodtrack->has_artwork = 0x01;

    setDatabaseChanged();
#else
    Q_UNUSED( track );
    Q_UNUSED( path );
#endif
}

void
IpodHandler::libSetCoverArt( Meta::MediaDeviceTrackPtr &track, const QPixmap &image )
{
#ifdef GDK_FOUND
    if( image.isNull() || !m_supportsArtwork )
        return;

    Itdb_Track *ipodtrack = m_itdbtrackhash[ track ];

    const QString filename = ipodArtFilename( ipodtrack );
    // Don't write the file if it already exists (we probably just saved it)
    if( !QFile::exists(filename) )
    {
        bool saved = image.save( filename );
        if( !saved )
            return;
    }

    libSetCoverArtPath( track, filename ); // will call setDatabaseChanged() if required
#else
    Q_UNUSED( track );
    Q_UNUSED( image );
#endif
}

void
IpodHandler::prepareToParseTracks()
{
    m_currtracklist = m_itdb->tracks;
}

bool
IpodHandler::isEndOfParseTracksList()
{
    return (m_currtracklist ? false : true);
}

void
IpodHandler::prepareToParseNextTrack()
{
    m_currtracklist = m_currtracklist->next;
}

void
IpodHandler::nextTrackToParse()
{
    m_currtrack = (Itdb_Track*) m_currtracklist->data;
}

/// Playlist Parsing

void
IpodHandler::prepareToParsePlaylists()
{
    m_currplaylistlist = m_itdb->playlists;
}


bool
IpodHandler::isEndOfParsePlaylistsList()
{
    return (m_currplaylistlist ? false : true);
}


void
IpodHandler::prepareToParseNextPlaylist()
{
    m_currplaylistlist = m_currplaylistlist->next;
}


void
IpodHandler::nextPlaylistToParse()
{
    m_currplaylist = ( Itdb_Playlist * ) m_currplaylistlist->data;
}

bool
IpodHandler::shouldNotParseNextPlaylist()
{
    // NOTE: skip the master playlist
    return ( itdb_playlist_is_mpl( m_currplaylist ) || itdb_playlist_is_podcasts( m_currplaylist ) );
}


void
IpodHandler::prepareToParsePlaylistTracks()
{
    m_currtracklist = m_currplaylist->members;
}


bool
IpodHandler::isEndOfParsePlaylist()
{
    return (m_currtracklist ? false : true );
}


void
IpodHandler::prepareToParseNextPlaylistTrack()
{
    prepareToParseNextTrack();
}


void
IpodHandler::nextPlaylistTrackToParse()
{
    nextTrackToParse();
}

Meta::MediaDeviceTrackPtr
IpodHandler::libGetTrackPtrForTrackStruct()
{
    return m_itdbtrackhash.key( m_currtrack );
}

QString
IpodHandler::libGetPlaylistName()
{
    return QString::fromUtf8( m_currplaylist->name );
}

void
IpodHandler::setAssociatePlaylist( const Meta::MediaDevicePlaylistPtr &playlist )
{
    m_itdbplaylisthash[ playlist ] = m_currplaylist;
}

void
IpodHandler::libSavePlaylist( const Meta::MediaDevicePlaylistPtr &playlist, const QString& name )
{
    DEBUG_BLOCK
    // Make new playlist

    Itdb_Playlist *pl = itdb_playlist_new( name.toUtf8(), 0 );
    itdb_playlist_add( m_itdb, pl, -1 );

    Meta::TrackList tracks = const_cast<Meta::MediaDevicePlaylistPtr&> ( playlist )->tracks();

    foreach( const Meta::TrackPtr track, tracks )
    {
        itdb_playlist_add_track( pl, m_itdbtrackhash[ Meta::MediaDeviceTrackPtr::staticCast( track ) ], -1 );
    }

    m_itdbplaylisthash[ playlist ] = pl;

    setDatabaseChanged();
}

void
IpodHandler::deletePlaylist( const Meta::MediaDevicePlaylistPtr &playlist )
{
    DEBUG_BLOCK
    Itdb_Playlist *pl = m_itdbplaylisthash.value( playlist );

    if( pl )
    {
        debug() << "Playlist removed";
        itdb_playlist_remove( pl );
        m_itdbplaylisthash.remove( playlist );
        setDatabaseChanged();
    }
}

void
IpodHandler::renamePlaylist( const Meta::MediaDevicePlaylistPtr &playlist )
{
    DEBUG_BLOCK
    Itdb_Playlist *pl = m_itdbplaylisthash[ playlist ];

    if( pl )
    {
        debug() << "Playlist renamed";
        pl->name = g_strdup( playlist->name().toUtf8() );
        setDatabaseChanged();
    }
}

void
IpodHandler::setAssociateTrack( const Meta::MediaDeviceTrackPtr track )
{
    m_itdbtrackhash[ track ] = m_currtrack;
    m_files.insert( QString(m_currtrack->ipod_path).toLower(), m_currtrack );
}

QStringList
IpodHandler::supportedFormats()
{
    return ( QStringList() ) << "mp3" << "aac" << "mp4" << "m4a" << "aiff";
}


/* Private Functions */

void
IpodHandler::prepareToCopy()
{
    // Initialize job counter to prepare to keep track of how many
    // copy jobs are going on at once
    //m_jobcounter = 0;

    m_trackdesturl.clear();
    m_trackscopying.clear();
}

void
IpodHandler::prepareToDelete()
{
    // Initialize job counter to prepare to keep track of how many
    // copy jobs are going on at once
    //m_jobcounter = 0;

    m_tracksdeleting.clear();
}

void
IpodHandler::slotDBWriteFailed( ThreadWeaver::Job* job )
{
    Q_UNUSED( job );
    debug() << "Writing to DB failed!";
    slotDatabaseWritten( false );
}

void
IpodHandler::slotDBWriteSucceeded( ThreadWeaver::Job* job )
{
    Q_UNUSED( job );
    if( job->success() )
    {
        debug() << "Writing to DB succeeded!";
        slotDatabaseWritten( true );
    }
    else
        debug() << "Writing to DB did not happen or failed";
}

/// Stale

void
IpodHandler::slotStaleFailed( ThreadWeaver::Job* job )
{
    Q_UNUSED( job );
    debug() << "Finding stale thread failed";
    slotOrphaned();
}

void
IpodHandler::slotStaleSucceeded( ThreadWeaver::Job* job )
{
    if( job->success() )
    {
        debug() << "Stale thread succeeded!";

        QStringList staleList;
        m_staletracksremoved = 0;

        if( m_staletracks.count() > 0 )
        {

            foreach( Meta::TrackPtr track, m_staletracks )
            {
                QString ent;
                QTextStream entry( &ent );
                entry << track->artist()->name()
                        << " - "
                        << track->album()->name()
                        << " - "
                        << track->name();

                staleList << ent;
            }

            bool ok = false;

            QStringList itemList = KInputDialog::getItemList( i18n( "Select Stale Tracks To Delete" ), i18n( "Stale Tracks" ), staleList, staleList, true /*multiple*/, &ok, 0 );

            if( ok )
            {
                Meta::TrackList staleToDelete;
                foreach( QString item, itemList )
                {
                    staleToDelete << m_staletracks[ itemList.indexOf( item ) ];
                }

                m_staletracksremoved = staleToDelete.count();
                // HACK: do through signals/slots, which is how CollectionLocation
                // does deletion of tracks.  Should create protected method to
                // allow Handlers to delete without hackery.
                connect( this, SIGNAL( removeTracksDone() ), SLOT( slotOrphaned() ) );
                removeTrackListFromDevice( staleToDelete );
            }

        } // endif staletracks
        else
        {
            slotOrphaned();
        }

    }
    else
    {
        debug() << "Stale thread failed";
        slotOrphaned();
    }
}

/// Orphaned

void
IpodHandler::slotOrphanedFailed( ThreadWeaver::Job* job )
{
    Q_UNUSED( job );
    debug() << "Finding orphaned thread failed";
}

void
IpodHandler::slotOrphanedSucceeded( ThreadWeaver::Job* job )
{
    DEBUG_BLOCK
    if( job->success() )
    {
        m_orphanedadded = 0;

        debug() << "Number of paths: " << m_orphanedPaths.count();

        if( !m_orphanedPaths.empty() )
        {
            m_statusbar = The::statusBar()->newProgressOperation( this, i18n( "Adding Orphaned Tracks to iPod Database" ) );
            m_statusbar->setMaximum( m_orphanedPaths.count() );

            ThreadWeaver::Weaver::instance()->enqueue( new AddOrphanedWorkerThread( this ) );

        }

    }
    else
    {
        debug() << "failed to find orphaned tracks";
    }
}

/// Add Orphaned

void
IpodHandler::slotAddOrphanedFailed( ThreadWeaver::Job* job )
{
    Q_UNUSED( job );
    debug() << "Adding orphaned thread failed";
    if( m_orphanedPaths.count() )
            ThreadWeaver::Weaver::instance()->enqueue( new AddOrphanedWorkerThread( this ) );
}

void
IpodHandler::slotAddOrphanedSucceeded( ThreadWeaver::Job* job )
{
    if( job->success() )
    {
        emit incrementProgress();

        if( m_orphanedPaths.count() )
            ThreadWeaver::Weaver::instance()->enqueue( new AddOrphanedWorkerThread( this ) );
        else
        {
            writeDatabase();

            const QString orphmsg( i18ncp( "@info", "One orphaned track added to the database.",
                                           "%1 tracks added to the database.", m_orphanedadded ) );

            KMessageBox::information(0,
                                     orphmsg,
                                     i18n("Orphaned Tracks Added") );
        }
    }
    else
    {
        debug() << "failed to add orphaned tracks";
    }
}

/// Capability-related functions

bool
IpodHandler::hasCapabilityInterface( Handler::Capability::Type type ) const
{
    switch( type )
    {
        case Handler::Capability::Artwork:
        case Handler::Capability::Readable:
        case Handler::Capability::Playlist:
        case Handler::Capability::Writable:
            return true;

        default:
            return false;
    }
}

Handler::Capability*
IpodHandler::createCapabilityInterface( Handler::Capability::Type type )
{
    switch( type )
    {
        case Handler::Capability::Artwork:
            return new Handler::IpodArtworkCapability( this );
        case Handler::Capability::Readable:
            return new Handler::IpodReadCapability( this );
        case Handler::Capability::Playlist:
            return new Handler::IpodPlaylistCapability( this );
        case Handler::Capability::Writable:
            return new Handler::IpodWriteCapability( this );

        default:
            return 0;
    }
}

DBWorkerThread::DBWorkerThread( IpodHandler* handler )
    : AbstractIpodWorkerThread( handler )
{
    connect( this, SIGNAL( failed( ThreadWeaver::Job* ) ), m_handler, SLOT( slotDBWriteFailed( ThreadWeaver::Job* ) ), Qt::QueuedConnection );
    connect( this, SIGNAL( done( ThreadWeaver::Job* ) ), m_handler, SLOT( slotDBWriteSucceeded( ThreadWeaver::Job* ) ), Qt::QueuedConnection );
}

void
DBWorkerThread::run()
{
    m_success = m_handler->writeITunesDB( false );
}

// stale

StaleWorkerThread::StaleWorkerThread( IpodHandler* handler )
    : AbstractIpodWorkerThread( handler )
{
    connect( this, SIGNAL( failed( ThreadWeaver::Job* ) ), m_handler, SLOT( slotStaleFailed( ThreadWeaver::Job* ) ), Qt::QueuedConnection );
    connect( this, SIGNAL( done( ThreadWeaver::Job* ) ), m_handler, SLOT( slotStaleSucceeded( ThreadWeaver::Job* ) ), Qt::QueuedConnection );
}

void
StaleWorkerThread::run()
{
    m_success = m_handler->findStale();
}

// Orphaned

OrphanedWorkerThread::OrphanedWorkerThread( IpodHandler* handler )
    : AbstractIpodWorkerThread( handler )
{
    connect( this, SIGNAL( failed( ThreadWeaver::Job* ) ), m_handler, SLOT( slotOrphanedFailed( ThreadWeaver::Job* ) ), Qt::QueuedConnection );
    connect( this, SIGNAL( done( ThreadWeaver::Job* ) ), m_handler, SLOT( slotOrphanedSucceeded( ThreadWeaver::Job* ) ), Qt::QueuedConnection );
}

void
OrphanedWorkerThread::run()
{
    m_success = m_handler->findOrphaned();
}

// Add Orphaned

AddOrphanedWorkerThread::AddOrphanedWorkerThread( IpodHandler* handler )
    : AbstractIpodWorkerThread( handler )
{
    connect( this, SIGNAL( failed( ThreadWeaver::Job* ) ), m_handler, SLOT( slotAddOrphanedFailed( ThreadWeaver::Job* ) ), Qt::QueuedConnection );
    connect( this, SIGNAL( done( ThreadWeaver::Job* ) ), m_handler, SLOT( slotAddOrphanedSucceeded( ThreadWeaver::Job* ) ), Qt::QueuedConnection );
}

void
AddOrphanedWorkerThread::run()
{
    m_success = m_handler->addNextOrphaned();
}

// Sync Artwork

SyncArtworkWorkerThread::SyncArtworkWorkerThread( IpodHandler* handler )
    : AbstractIpodWorkerThread( handler )
{
    connect( this, SIGNAL( failed( ThreadWeaver::Job* ) ), m_handler, SLOT( slotSyncArtworkFailed( ThreadWeaver::Job* ) ), Qt::QueuedConnection );
    connect( this, SIGNAL( done( ThreadWeaver::Job* ) ), m_handler, SLOT( slotSyncArtworkSucceeded( ThreadWeaver::Job* ) ), Qt::QueuedConnection );
}

void
SyncArtworkWorkerThread::run()
{
    m_success = m_handler->syncArtwork();
}

#include "IpodHandler.moc"

