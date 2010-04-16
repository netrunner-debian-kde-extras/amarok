/****************************************************************************************
 * Copyright (c) 2009 Téo Mrnjavac <teo@kde.org>                                        *
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

#include "PlaylistModelStack.h"

#include "core/support/Debug.h"
#include "PlaylistActions.h"
#include "amarokconfig.h"


namespace The
{
    AMAROK_EXPORT Playlist::AbstractModel* playlist()
    {
        return Playlist::ModelStack::instance()->top();
    }

    AMAROK_EXPORT Playlist::Controller* playlistController()
    {
        return Playlist::ModelStack::instance()->controller();
    }
}

namespace Playlist
{

ModelStack* ModelStack::s_instance = 0;

ModelStack*
ModelStack::instance()
{
    if( s_instance == 0 )
        s_instance = new ModelStack();
    return s_instance;
}

void
ModelStack::destroy()
{
    if( s_instance )
    {
        delete s_instance;
        s_instance = 0;
    }
}

ModelStack::ModelStack()
    : QObject()
{
    DEBUG_BLOCK
    m_model = new Model( this );
    m_sortfilter = new SortFilterProxy( m_model, this );
    m_search = new SearchProxy( m_sortfilter, this );
    m_grouping = new GroupingProxy( m_search, this );

    m_controller = new Controller( m_model, m_grouping, this );
}

ModelStack::~ModelStack()
{
    // Delete everything in reverse order of the constructor.
    delete m_controller;

    delete m_grouping;
    delete m_search;
    delete m_sortfilter;
    delete m_model;
}

Playlist::AbstractModel *
ModelStack::top()
{
    return m_grouping;
}

GroupingProxy *
ModelStack::groupingProxy()
{
    return m_grouping;
}

SortFilterProxy *
ModelStack::sortProxy()
{
    return m_sortfilter;
}

SortFilterProxy *
ModelStack::filterProxy()
{
    return m_sortfilter;
}

Playlist::Model *
ModelStack::bottom()
{
    return m_model;
}

Controller *
ModelStack::controller()
{
    return m_controller;
}

}   //namespace Playlist
