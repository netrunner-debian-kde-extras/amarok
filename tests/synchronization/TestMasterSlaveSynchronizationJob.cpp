/****************************************************************************************
 * Copyright (c) 2010 Maximilian Kossick <maximilian.kossick@googlemail.com>       *
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

#include "TestMasterSlaveSynchronizationJob.h"

#include "Debug.h"
#include "collection/CollectionLocation.h"
#include "collection/CollectionLocationDelegate.h"
#include "Components.h"
#include "synchronization/MasterSlaveSynchronizationJob.h"

#include "CollectionTestImpl.h"
#include "collection/MockCollectionLocationDelegate.h"
#include "mocks/MockTrack.h"
#include "mocks/MockAlbum.h"
#include "mocks/MockArtist.h"

#include <KCmdLineArgs>
#include <KGlobal>

#include <qtest_kde.h>

#include <gmock/gmock.h>

QTEST_KDEMAIN_CORE( TestMasterSlaveSynchronizationJob )

using ::testing::Return;
using ::testing::AnyNumber;
using ::testing::_;

static int trackCopyCount;
static int trackRemoveCount;

class MyCollectionLocation : public CollectionLocation
{
public:
    CollectionTestImpl *coll;

    QString prettyLocation() const { return "foo"; }
    bool isWritable() const { return true; }

    void removeUrlsFromCollection( const Meta::TrackList &sources )
    {
        trackRemoveCount += sources.count();
        coll->acquireWriteLock();
        TrackMap map = coll->trackMap();
        foreach( const Meta::TrackPtr &track, sources )
            map.remove( track->uidUrl() );
        coll->setTrackMap( map );
        coll->releaseLock();
        slotRemoveOperationFinished();
    }

    void copyUrlsToCollection(const QMap<Meta::TrackPtr, KUrl> &sources)
    {
        trackCopyCount = sources.count();
        foreach( const Meta::TrackPtr &track, sources.keys() )
        {
            coll->addTrack( track );
        }
    }
};

class MyCollectionTestImpl : public CollectionTestImpl
{
public:
    MyCollectionTestImpl( const QString &id ) : CollectionTestImpl( id ) {}

    CollectionLocation* location() const
    {
        MyCollectionLocation *r = new MyCollectionLocation();
        r->coll = const_cast<MyCollectionTestImpl*>( this );
        return r;
    }
};

void addMockTrack( CollectionTestImpl *coll, const QString &trackName, const QString &artistName, const QString &albumName )
{
    Meta::MockTrack *track = new Meta::MockTrack();
    ::testing::Mock::AllowLeak( track );
    Meta::TrackPtr trackPtr( track );
    EXPECT_CALL( *track, name() ).Times( AnyNumber() ).WillRepeatedly( Return( trackName ) );
    EXPECT_CALL( *track, uidUrl() ).Times( AnyNumber() ).WillRepeatedly( Return( trackName + "_" + artistName + "_" + albumName ) );
    EXPECT_CALL( *track, isPlayable() ).Times( AnyNumber() ).WillRepeatedly( Return( true ) );
    EXPECT_CALL( *track, playableUrl() ).Times( AnyNumber() ).WillRepeatedly( Return( KUrl( '/' + track->uidUrl() ) ) );
    coll->addTrack( trackPtr );

    Meta::AlbumPtr albumPtr = coll->albumMap().value( albumName );
    Meta::MockAlbum *album;
    Meta::TrackList albumTracks;
    if( albumPtr )
    {
        album = dynamic_cast<Meta::MockAlbum*>( albumPtr.data() );
        if( !album )
        {
            QFAIL( "expected a Meta::MockAlbum" );
            return;
        }
        albumTracks = albumPtr->tracks();
    }
    else
    {
        album = new Meta::MockAlbum();
        ::testing::Mock::AllowLeak( album );
        albumPtr = Meta::AlbumPtr( album );
        EXPECT_CALL( *album, name() ).Times( AnyNumber() ).WillRepeatedly( Return( albumName ) );
        EXPECT_CALL( *album, hasAlbumArtist() ).Times( AnyNumber() ).WillRepeatedly( Return( false ) );
        coll->addAlbum( albumPtr );
    }
    albumTracks << trackPtr;
    EXPECT_CALL( *album, tracks() ).Times( AnyNumber() ).WillRepeatedly( Return( albumTracks ) );

    EXPECT_CALL( *track, album() ).Times( AnyNumber() ).WillRepeatedly( Return( albumPtr ) );

    Meta::ArtistPtr artistPtr = coll->artistMap().value( artistName );
    Meta::MockArtist *artist;
    Meta::TrackList artistTracks;
    if( artistPtr )
    {
        artist = dynamic_cast<Meta::MockArtist*>( artistPtr.data() );
        if( !artist )
        {
            QFAIL( "expected a Meta::MockArtist" );
            return;
        }
        artistTracks = artistPtr->tracks();
    }
    else
    {
        artist = new Meta::MockArtist();
        ::testing::Mock::AllowLeak( artist );
        artistPtr = Meta::ArtistPtr( artist );
        EXPECT_CALL( *artist, name() ).Times( AnyNumber() ).WillRepeatedly( Return( artistName ) );
        coll->addArtist( artistPtr );
    }
    artistTracks << trackPtr;
    EXPECT_CALL( *artist, tracks() ).Times( AnyNumber() ).WillRepeatedly( Return( artistTracks ) );
    EXPECT_CALL( *track, artist() ).Times( AnyNumber() ).WillRepeatedly( Return( artistPtr ) );
}

TestMasterSlaveSynchronizationJob::TestMasterSlaveSynchronizationJob()
{
    KCmdLineArgs::init( KGlobal::activeComponent().aboutData() );
    ::testing::InitGoogleMock( &KCmdLineArgs::qtArgc(), KCmdLineArgs::qtArgv() );
    qRegisterMetaType<Meta::TrackList>();
    qRegisterMetaType<Meta::AlbumList>();
    qRegisterMetaType<Meta::ArtistList>();
}

void
TestMasterSlaveSynchronizationJob::init()
{
    trackCopyCount = 0;
    trackRemoveCount = 0;
}

void
TestMasterSlaveSynchronizationJob::testAddTracksToEmptySlave()
{
    CollectionTestImpl *master = new MyCollectionTestImpl( "master" );
    CollectionTestImpl *slave = new MyCollectionTestImpl( "slave" );

    //setup master
    addMockTrack( master, "track1", "artist1", "album1" );
    QCOMPARE( master->trackMap().count(), 1 );
    QCOMPARE( slave->trackMap().count(), 0 );
    QCOMPARE( trackCopyCount, 0 );
    QCOMPARE( trackRemoveCount, 0 );

    MasterSlaveSynchronizationJob *job = new MasterSlaveSynchronizationJob();
    job->setMaster( master );
    job->setSlave( slave );
    job->synchronize();
    QTest::kWaitForSignal( job, SIGNAL(destroyed()), 1000 );

    QCOMPARE( trackCopyCount, 1 );
    QCOMPARE( trackRemoveCount, 0 );
    QCOMPARE( master->trackMap().count(), 1 );
    QCOMPARE( slave->trackMap().count(), 1 );
    delete master;
    delete slave;
}

void
TestMasterSlaveSynchronizationJob::testAddSingleTrack()
{
    CollectionTestImpl *master = new MyCollectionTestImpl( "master" );
    CollectionTestImpl *slave = new MyCollectionTestImpl( "slave" );

    //setup
    addMockTrack( master, "track1", "artist1", "album1" );
    addMockTrack( slave, "track1", "artist1", "album1" );
    addMockTrack( master, "track2", "artist1", "album1" );

    QCOMPARE( master->trackMap().count(), 2 );
    QCOMPARE( slave->trackMap().count(), 1 );
    QCOMPARE( trackCopyCount, 0 );
    QCOMPARE( trackRemoveCount, 0 );

    //test
    MasterSlaveSynchronizationJob *job = new MasterSlaveSynchronizationJob();
    job->setMaster( master );
    job->setSlave( slave );
    job->synchronize();
    QTest::kWaitForSignal( job, SIGNAL(destroyed()), 1000 );

    //verify
    QCOMPARE( trackCopyCount, 1 );
    QCOMPARE( trackRemoveCount, 0 );
    QCOMPARE( master->trackMap().count(), 2 );
    QCOMPARE( slave->trackMap().count(), 2 );

    delete master;
    delete slave;
}

void
TestMasterSlaveSynchronizationJob::testAddAlbum()
{
    CollectionTestImpl *master = new MyCollectionTestImpl( "master" );
    CollectionTestImpl *slave = new MyCollectionTestImpl( "slave" );

    //setup
    addMockTrack( master, "track1", "artist1", "album1" );
    addMockTrack( slave, "track1", "artist1", "album1" );
    addMockTrack( master, "track1", "artist1", "album2" );

    QCOMPARE( master->trackMap().count(), 2 );
    QCOMPARE( slave->trackMap().count(), 1 );
    QCOMPARE( trackCopyCount, 0 );
    QCOMPARE( trackRemoveCount, 0 );

    //test
    MasterSlaveSynchronizationJob *job = new MasterSlaveSynchronizationJob();
    job->setMaster( master );
    job->setSlave( slave );
    job->synchronize();
    QTest::kWaitForSignal( job, SIGNAL(destroyed()), 1000 );

    //verify
    QCOMPARE( trackCopyCount, 1 );
    QCOMPARE( trackRemoveCount, 0 );
    QCOMPARE( master->trackMap().count(), 2 );
    QCOMPARE( slave->trackMap().count(), 2 );

    delete master;
    delete slave;
}

void
TestMasterSlaveSynchronizationJob::testAddArtist()
{
    CollectionTestImpl *master = new MyCollectionTestImpl( "master" );
    CollectionTestImpl *slave = new MyCollectionTestImpl( "slave" );

    //setup
    addMockTrack( master, "track1", "artist1", "album1" );
    addMockTrack( slave, "track1", "artist1", "album1" );
    addMockTrack( master, "track1", "artist2", "album1" );

    QCOMPARE( master->trackMap().count(), 2 );
    QCOMPARE( slave->trackMap().count(), 1 );
    QCOMPARE( trackCopyCount, 0 );
    QCOMPARE( trackRemoveCount, 0 );

    //test
    MasterSlaveSynchronizationJob *job = new MasterSlaveSynchronizationJob();
    job->setMaster( master );
    job->setSlave( slave );
    job->synchronize();
    QTest::kWaitForSignal( job, SIGNAL(destroyed()), 1000 );

    //verify
    QCOMPARE( trackCopyCount, 1 );
    QCOMPARE( trackRemoveCount, 0 );
    QCOMPARE( master->trackMap().count(), 2 );
    QCOMPARE( slave->trackMap().count(), 2 );

    delete master;
    delete slave;
}

void
TestMasterSlaveSynchronizationJob::testRemoveSingleTrack()
{
    CollectionTestImpl *master = new MyCollectionTestImpl( "master" );
    CollectionTestImpl *slave = new MyCollectionTestImpl( "slave" );

    MockCollectionLocationDelegate *delegate = new MockCollectionLocationDelegate();
    EXPECT_CALL( *delegate, reallyDelete( _, _) ).Times( AnyNumber() ).WillRepeatedly( Return( true ) );
    Amarok::Components::setCollectionLocationDelegate( delegate );

    //setup
    addMockTrack( master, "track1", "artist1", "album1" );
    addMockTrack( slave, "track1", "artist1", "album1" );
    addMockTrack( slave, "track2", "artist1", "album1" );

    QCOMPARE( master->trackMap().count(), 1 );
    QCOMPARE( slave->trackMap().count(), 2 );
    QCOMPARE( trackCopyCount, 0 );
    QCOMPARE( trackRemoveCount, 0 );

    //test
    MasterSlaveSynchronizationJob *job = new MasterSlaveSynchronizationJob();
    job->setMaster( master );
    job->setSlave( slave );
    job->synchronize();
    QTest::kWaitForSignal( job, SIGNAL(destroyed()), 1000 );

    //verify
    QCOMPARE( trackCopyCount, 0 );
    QCOMPARE( trackRemoveCount, 1 );
    QCOMPARE( master->trackMap().count(), 1 );
    QCOMPARE( slave->trackMap().count(), 1 );

    delete master;
    delete slave;
    delete Amarok::Components::setCollectionLocationDelegate( 0 );
}

void
TestMasterSlaveSynchronizationJob::testRemoveAlbum()
{
    CollectionTestImpl *master = new MyCollectionTestImpl( "master" );
    CollectionTestImpl *slave = new MyCollectionTestImpl( "slave" );

    MockCollectionLocationDelegate *delegate = new MockCollectionLocationDelegate();
    EXPECT_CALL( *delegate, reallyDelete( _, _) ).Times( AnyNumber() ).WillRepeatedly( Return( true ) );
    Amarok::Components::setCollectionLocationDelegate( delegate );

    //setup
    addMockTrack( master, "track1", "artist1", "album1" );
    addMockTrack( slave, "track1", "artist1", "album1" );
    addMockTrack( slave, "track1", "artist1", "album2" );

    QCOMPARE( master->trackMap().count(), 1 );
    QCOMPARE( slave->trackMap().count(), 2 );
    QCOMPARE( trackCopyCount, 0 );
    QCOMPARE( trackRemoveCount, 0 );

    //test
    MasterSlaveSynchronizationJob *job = new MasterSlaveSynchronizationJob();
    job->setMaster( master );
    job->setSlave( slave );
    job->synchronize();
    QTest::kWaitForSignal( job, SIGNAL(destroyed()), 1000 );

    //verify
    QCOMPARE( trackCopyCount, 0 );
    QCOMPARE( trackRemoveCount, 1 );
    QCOMPARE( master->trackMap().count(), 1 );
    QCOMPARE( slave->trackMap().count(), 1 );

    delete master;
    delete slave;
    delete Amarok::Components::setCollectionLocationDelegate( 0 );
}

void
TestMasterSlaveSynchronizationJob::testRemoveArtist()
{
    CollectionTestImpl *master = new MyCollectionTestImpl( "master" );
    CollectionTestImpl *slave = new MyCollectionTestImpl( "slave" );

    MockCollectionLocationDelegate *delegate = new MockCollectionLocationDelegate();
    EXPECT_CALL( *delegate, reallyDelete( _, _) ).Times( AnyNumber() ).WillRepeatedly( Return( true ) );
    Amarok::Components::setCollectionLocationDelegate( delegate );

    //setup
    addMockTrack( master, "track1", "artist1", "album1" );
    addMockTrack( slave, "track1", "artist1", "album1" );
    addMockTrack( slave, "track1", "artist2", "album1" );

    QCOMPARE( master->trackMap().count(), 1 );
    QCOMPARE( slave->trackMap().count(), 2 );
    QCOMPARE( trackCopyCount, 0 );
    QCOMPARE( trackRemoveCount, 0 );

    //test
    MasterSlaveSynchronizationJob *job = new MasterSlaveSynchronizationJob();
    job->setMaster( master );
    job->setSlave( slave );
    job->synchronize();
    QTest::kWaitForSignal( job, SIGNAL(destroyed()), 1000 );

    //verify
    QCOMPARE( trackCopyCount, 0 );
    QCOMPARE( trackRemoveCount, 1 );
    QCOMPARE( master->trackMap().count(), 1 );
    QCOMPARE( slave->trackMap().count(), 1 );

    delete master;
    delete slave;
    delete Amarok::Components::setCollectionLocationDelegate( 0 );
}

void
TestMasterSlaveSynchronizationJob::testEmptyMaster()
{
    CollectionTestImpl *master = new MyCollectionTestImpl( "master" );
    CollectionTestImpl *slave = new MyCollectionTestImpl( "slave" );

    MockCollectionLocationDelegate *delegate = new MockCollectionLocationDelegate();
    EXPECT_CALL( *delegate, reallyDelete( _, _) ).Times( AnyNumber() ).WillRepeatedly( Return( true ) );
    Amarok::Components::setCollectionLocationDelegate( delegate );

    //setup master
    addMockTrack( slave, "track1", "artist1", "album1" );
    QCOMPARE( master->trackMap().count(), 0 );
    QCOMPARE( slave->trackMap().count(), 1 );
    QCOMPARE( trackCopyCount, 0 );
    QCOMPARE( trackRemoveCount, 0 );

    MasterSlaveSynchronizationJob *job = new MasterSlaveSynchronizationJob();
    job->setMaster( master );
    job->setSlave( slave );
    job->synchronize();
    QTest::kWaitForSignal( job, SIGNAL(destroyed()), 1000 );

    QCOMPARE( trackCopyCount, 0 );
    QCOMPARE( trackRemoveCount, 1 );
    QCOMPARE( master->trackMap().count(), 0 );
    QCOMPARE( slave->trackMap().count(), 0 );
    delete master;
    delete slave;
    delete Amarok::Components::setCollectionLocationDelegate( 0 );
}
