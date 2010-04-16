/****************************************************************************************
 * Copyright (c) 2004 Mark Kretschmann <kretschmann@kde.org>                            *
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

#define DEBUG_PREFIX "PluginManager"

#include "core/plugins/PluginManager.h"

#include "core/support/Debug.h"
#include "core/plugins/Plugin.h"

#include <KLibLoader>
#include <KLocale>
#include <KMessageBox>

#include <QFile>

using std::vector;

vector<Plugins::PluginManager::StoreItem>
Plugins::PluginManager::m_store;


/////////////////////////////////////////////////////////////////////////////////////
// PUBLIC INTERFACE
/////////////////////////////////////////////////////////////////////////////////////

KService::List
Plugins::PluginManager::query( const QString& constraint )
{
    // Add versioning constraint
    QString
    str  = "[X-KDE-Amarok-framework-version] == ";
    str += QString::number( Plugins::PluginFrameworkVersion );
    if ( !constraint.trimmed().isEmpty() )
        str += " and " + constraint;
    str += " and ";
    str += "[X-KDE-Amarok-rank] > 0";

    debug() << "Plugin trader constraint: " << str;

    return KServiceTypeTrader::self()->query( "Amarok/Plugin", str );
}


Plugins::Plugin*
Plugins::PluginManager::createFromQuery( const QString &constraint )
{
    Debug::Block block( __PRETTY_FUNCTION__ );

    KService::List offers = query( constraint );

    if ( offers.isEmpty() ) {
        warning() << "No matching plugin found.\n";
        return 0;
    }

    // Select plugin with highest rank
    int rank = 0;
    uint current = 0;
    for ( int i = 0; i < offers.count(); i++ ) {
        if ( offers[i]->property( "X-KDE-Amarok-rank" ).toInt() > rank )
            current = i;
    }

    return createFromService( offers[current] );
}


Plugins::Plugin*
Plugins::PluginManager::createFromService( const KService::Ptr service )
{
    DEBUG_BLOCK
    debug() << "Trying to load: " << service->library();

    //get the library loader instance
    KLibLoader *loader = KLibLoader::self();
    //try to load the specified library
    KLibrary *lib = loader->library( QFile::encodeName( service->library() ), QLibrary::ExportExternalSymbolsHint );

    if ( !lib )
    {
        warning() << "KLibLoader could not load the plugin: " << service->library();
        warning() << "Error Message: " << loader->lastErrorMessage();

        return 0;
    }

    //look up address of init function and cast it to pointer-to-function
    Plugin* (*create_plugin)() = ( Plugin* (*)() ) lib->resolveSymbol( "create_plugin" );

    if ( !create_plugin ) {
        warning() << "create_plugin == NULL\n";
        return 0;
    }
    //create plugin on the heap
    Plugins::Plugin* plugin = create_plugin();

    //put plugin into store
    StoreItem item;
    item.plugin = plugin;
    item.library = lib;
    item.service = service;
    m_store.push_back( item );

    dump( service );
    return plugin;
}


void
Plugins::PluginManager::unload( Plugins::Plugin* plugin )
{
    DEBUG_FUNC_INFO

    vector<StoreItem>::iterator iter = lookupPlugin( plugin );

    if ( iter != m_store.end() ) {
        delete (*iter).plugin;
        debug() << "Unloading library: "<< (*iter).service->library();
        (*iter).library->unload();

        m_store.erase( iter );
    }
    else
        warning() << "Could not unload plugin (not found in store).\n";
}


KService::Ptr
Plugins::PluginManager::getService( const Plugins::Plugin* plugin )
{
    if ( !plugin ) {
        warning() << "pointer == NULL\n";
        return KService::Ptr(0);
    }

    //search plugin in store
    vector<StoreItem>::const_iterator iter = lookupPlugin( plugin );

    if ( iter == m_store.end() ) {
        warning() << "Plugin not found in store.\n";
	return KService::Ptr(0);
    }

    return (*iter).service;
}


void
Plugins::PluginManager::showAbout( const QString &constraint )
{
    KService::List offers = query( constraint );

    if ( offers.isEmpty() )
        return;

    KService::Ptr s = offers.front();

    const QString body = "<tr><td>%1</td><td>%2</td></tr>";

    QString str  = "<html><body><table width=\"100%\" border=\"1\">";

    str += body.arg( i18nc( "Title, as in: the title of this item", "Name" ),                s->name() );
    str += body.arg( i18n( "Library" ),             s->library() );
    str += body.arg( i18n( "Authors" ),             s->property( "X-KDE-Amarok-authors" ).toStringList().join( "\n" ) );
    str += body.arg( i18nc( "Property, belonging to the author of this item", "Email" ),               s->property( "X-KDE-Amarok-email" ).toStringList().join( "\n" ) );
    str += body.arg( i18n( "Version" ),             s->property( "X-KDE-Amarok-version" ).toString() );
    str += body.arg( i18n( "Framework Version" ),   s->property( "X-KDE-Amarok-framework-version" ).toString() );

    str += "</table></body></html>";

    KMessageBox::information( 0, str, i18n( "Plugin Information" ) );
}


void
Plugins::PluginManager::dump( const KService::Ptr service )
{
    #define ENDLI endl << Debug::indent()

    debug()
      << ENDLI
      << "PluginManager Service Info:" << ENDLI
      << "---------------------------" << ENDLI
      << "name                          :" << service->name() << ENDLI
      << "library                       :" << service->library() << ENDLI
      << "desktopEntryPath              :" << service->entryPath() << ENDLI
      << "X-KDE-Amarok-plugintype       :" << service->property( "X-KDE-Amarok-plugintype" ).toString() << ENDLI
      << "X-KDE-Amarok-name             :" << service->property( "X-KDE-Amarok-name" ).toString() << ENDLI
      << "X-KDE-Amarok-authors          :" << service->property( "X-KDE-Amarok-authors" ).toStringList() << ENDLI
      << "X-KDE-Amarok-rank             :" << service->property( "X-KDE-Amarok-rank" ).toString() << ENDLI
      << "X-KDE-Amarok-version          :" << service->property( "X-KDE-Amarok-version" ).toString() << ENDLI
      << "X-KDE-Amarok-framework-version:" << service->property( "X-KDE-Amarok-framework-version" ).toString()
      << endl
     ;

    #undef ENDLI
}


/////////////////////////////////////////////////////////////////////////////////////
// PRIVATE INTERFACE
/////////////////////////////////////////////////////////////////////////////////////

vector<Plugins::PluginManager::StoreItem>::iterator
Plugins::PluginManager::lookupPlugin( const Plugins::Plugin* plugin )
{
    vector<StoreItem>::iterator iter;

    //search plugin pointer in store
    vector<StoreItem>::iterator iterEnd(m_store.end() );
    for ( iter = m_store.begin(); iter != iterEnd; ++iter ) {
        if ( (*iter).plugin == plugin )
            break;
    }

    return iter;
}


