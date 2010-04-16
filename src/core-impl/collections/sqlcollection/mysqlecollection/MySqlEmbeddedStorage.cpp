/****************************************************************************************
 * Copyright (c) 2008 Edward Toroshchin <edward.hades@gmail.com>                        *
 * Copyright (c) 2009 Jeff Mitchell <mitchell@kde.org>                                  *
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

#include "MySqlEmbeddedStorage.h"

#include "core/support/Amarok.h"
#include "core/support/Debug.h"
#include "amarokconfig.h"

#include <QDir>
#include <QString>
#include <QMutexLocker>
#include <QThreadStorage>
#include <QVarLengthArray>

#include <mysql.h>

MySqlEmbeddedStorage::MySqlEmbeddedStorage( const QString &storageLocation )
    : MySqlStorage()
{

    m_debugIdent = "MySQLe";

    QString defaultsFile;
    QString databaseDir;
    if( storageLocation.isEmpty() )
    {
        defaultsFile = Amarok::config( "MySQLe" ).readEntry( "config", Amarok::saveLocation() + "my.cnf" );
        databaseDir = Amarok::config( "MySQLe" ).readEntry( "data", Amarok::saveLocation() + "mysqle" );
    }
    else
    {
        QDir dir( storageLocation );
        dir.mkpath( "." );  //ensure directory exists
        defaultsFile = QDir::cleanPath( dir.absoluteFilePath( "my.cnf" ) );
        databaseDir = dir.absolutePath() + QDir::separator() + "mysqle";
    }

    char* defaultsLine = qstrdup( QString( "--defaults-file=%1" ).arg( defaultsFile ).toAscii().data() );
    char* databaseLine = qstrdup( QString( "--datadir=%1" ).arg( databaseDir ).toAscii().data() );

    if( !QFile::exists( defaultsFile ) )
    {
        QFile df( defaultsFile );
        if ( !df.open( QIODevice::WriteOnly ) ) {
            error() << "Unable to open " << defaultsFile << " for writing.";
            reportError( "init" );
        }
    }

    if( !QFile::exists( databaseDir ) )
    {
        QDir dir( databaseDir );
        dir.mkpath( "." );
    }

    static const int num_elements = 9;
    char **server_options = new char* [ num_elements + 1 ];
    server_options[0] = const_cast<char*>( "amarokmysqld" );
    server_options[1] = defaultsLine;
    server_options[2] = databaseLine;
    // CAUTION: if we ever change the table type we will need to fix a number of MYISAM specific
    // functions, such as FULLTEXT indexing.
    server_options[3] = const_cast<char*>( "--default-storage-engine=MYISAM" );
    server_options[4] = const_cast<char*>( "--loose-skip-innodb" );
    server_options[5] = const_cast<char*>( "--skip-grant-tables" );
    server_options[6] = const_cast<char*>( "--myisam-recover=FORCE" );
    server_options[7] = const_cast<char*>( "--character-set-server=utf8" );
    server_options[8] = const_cast<char*>( "--collation-server=utf8_bin" );
    server_options[num_elements] = 0;

    char **server_groups = new char* [ 3 ];
    server_groups[0] = const_cast<char*>( "amarokserver" );
    server_groups[1] = const_cast<char*>( "amarokclient" );
    server_groups[2] = 0;

    if( mysql_library_init(num_elements, server_options, server_groups) != 0 )
    {
        error() << "MySQL library initialization failed.";
        reportError( "init" );
        return;
    }

    m_db = mysql_init( NULL );
    delete [] server_options;
    delete [] server_groups;
    delete [] defaultsLine;
    delete [] databaseLine;

    if( !m_db )
    {
        error() << "MySQLe initialization failed";
        return;
    }

    if( mysql_options( m_db, MYSQL_READ_DEFAULT_GROUP, "amarokclient" ) )
        reportError( "Error setting options for READ_DEFAULT_GROUP" );
    if( mysql_options( m_db, MYSQL_OPT_USE_EMBEDDED_CONNECTION, NULL ) )
        reportError( "Error setting option to use embedded connection" );

    if( !mysql_real_connect( m_db, NULL,NULL,NULL, 0, 0,NULL, 0 ) )
    {
        error() << "Could not connect to mysql!";
        reportError( "na" );
        mysql_close( m_db );
        m_db = 0;
    }
    else
    {
        sharedInit( "amarok" );
        debug() << "Connected to MySQL server" << mysql_get_server_info( m_db );
    }

    MySqlStorage::initThreadInitializer();
}

MySqlEmbeddedStorage::~MySqlEmbeddedStorage()
{
    DEBUG_BLOCK
}

QString
MySqlEmbeddedStorage::type() const
{
    return "MySQLe";
}
