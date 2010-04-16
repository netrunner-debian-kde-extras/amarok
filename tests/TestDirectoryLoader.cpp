/***************************************************************************
 *   Copyright (c) 2009 Sven Krohlas <sven@getamarok.com>                  *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.         *
 ***************************************************************************/

#include "TestDirectoryLoader.h"

#include "playlist/PlaylistController.h"
#include "playlist/PlaylistModelStack.h"

#include <QtTest/QTest>

/* This one is a bit ugly, as the results of the methods in DirectoryLoader can't *
 * be checked directly there but only in the playlist.                            */

TestDirectoryLoader::TestDirectoryLoader( const QStringList args, const QString &logPath )
    : TestBase( "DirectoryLoader" )
{
    QStringList combinedArgs = args;
    addLogging( combinedArgs, logPath );

    m_testArgumentList = combinedArgs;
    m_finishedLoaders = 0;

    The::playlistController()->clear(); // we need a clear playlist for those tests

    DirectoryLoader *loader1 = new DirectoryLoader;
    DirectoryLoader *loader2 = new DirectoryLoader;
    QList<QUrl> testList;
    QUrl testUrl;

    testUrl = QUrl::fromLocalFile( dataPath( "amarok/testdata/audio/" ) );
    testList.append( testUrl );

    connect( loader1, SIGNAL( finished( Meta::TrackList ) ), this, SLOT( loadersFinished() ) );
    connect( loader2, SIGNAL( finished( Meta::TrackList ) ), this, SLOT( loadersFinished() ) );

    loader1->insertAtRow( 1 ); // TODO: negative values always seem to append at the beginning. is that correct?
    loader1->init( testList );
    loader2->insertAtRow( 4 );
    loader2->init( testList );
}

void TestDirectoryLoader::loadersFinished()
{
    m_mutex.lock();
    m_finishedLoaders++;

    if( m_finishedLoaders == 2 )
    {
        m_mutex.unlock();
        QTest::qExec( this, m_testArgumentList );
        delete this;
    }
    else
        m_mutex.unlock();
}


void TestDirectoryLoader::testInitAndInsertAtRow()
{
    QCOMPARE( Playlist::ModelStack::instance()->bottom()->rowCount(), 20 );

    QCOMPARE( Playlist::ModelStack::instance()->bottom()->trackAt( 0 )->prettyName(), QString( "Platz 01" ) );
    QCOMPARE( Playlist::ModelStack::instance()->bottom()->trackAt( 4 )->prettyName(), QString( "Platz 01" ) );
    QCOMPARE( Playlist::ModelStack::instance()->bottom()->trackAt( 5 )->prettyName(), QString( "Platz 02" ) );
    QCOMPARE( Playlist::ModelStack::instance()->bottom()->trackAt( 14 )->prettyName(), QString( "Platz 05" ) );
    QCOMPARE( Playlist::ModelStack::instance()->bottom()->trackAt( 19 )->prettyName(), QString( "Platz 10" ) );
}
