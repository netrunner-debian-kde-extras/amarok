/****************************************************************************************
 * Copyright (c) 2009-2010 Leo Franchi <lfranchi@kde.org>                               *
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

#include "CustomBias.h"

#define DEBUG_PREFIX "CustomBias"

#include "CustomBiasEntryWidget.h"
#include "core/support/Debug.h"
#include "DynamicModel.h"

QList< Dynamic::CustomBiasEntryFactory* > Dynamic::CustomBias::s_biasFactories = QList< Dynamic::CustomBiasEntryFactory* >();
QList< Dynamic::CustomBias* > Dynamic::CustomBias::s_biases = QList< Dynamic::CustomBias* >();
QMap< QString, Dynamic::CustomBias* > Dynamic::CustomBias::s_failedMap = QMap< QString, Dynamic::CustomBias* >();
QMap< QString, QDomElement > Dynamic::CustomBias::s_failedMapXml = QMap< QString, QDomElement >();

        
Dynamic::CustomBias::CustomBias()
    : m_currentEntry( 0 )
    , m_weight( 0 )
{
      //  debug() << "CREATING NEW CUSTOM BIAS WITH NO ARGS :(!!!";
}

Dynamic::CustomBias::CustomBias( Dynamic::CustomBiasEntry* entry, double weight )
    : m_currentEntry( entry )
    , m_weight( weight )
{

    connect( m_currentEntry, SIGNAL( biasChanged()), this, SLOT( customBiasChanged() ) );
    debug() << "CREATING NEW CUSTOM BIAS!!! with:" << entry << weight;
}

Dynamic::CustomBias::~CustomBias()
{
    delete m_currentEntry;
}


PlaylistBrowserNS::BiasWidget*
Dynamic::CustomBias::widget( QWidget* parent )
{
    DEBUG_BLOCK
    if( m_currentEntry )
        debug() << "type:" << m_currentEntry->pluginName();
    debug() << "custombias with weight: " << m_weight << "returning new widget";
    Dynamic::CustomBiasEntryWidget* w = new Dynamic::CustomBiasEntryWidget( this, parent );
    connect( this, SIGNAL( biasFactoriesChanged() ), w, SLOT( refreshBiasFactories() ) );
    return w;
}

double
Dynamic::CustomBias::energy( const Meta::TrackList& playlist, const Meta::TrackList& context ) const
{
    DEBUG_BLOCK

    Q_UNUSED( context );

    double satisfiedCount = 0;
    if( m_currentEntry )
        satisfiedCount = m_currentEntry->numTracksThatSatisfy( playlist );
    else
        warning() << "WHY is there no set type of BIAS?!";
    
    return  m_weight - (satisfiedCount / (double)playlist.size());
    
}

QDomElement
Dynamic::CustomBias::xml() const
{
    DEBUG_BLOCK

    if( m_currentEntry )
    {

        QDomDocument doc = PlaylistBrowserNS::DynamicModel::instance()->savedPlaylistDoc();

        QDomElement e = doc.createElement( "bias" );
        e.setAttribute( "type", "custom" );
        QDomElement child = doc.createElement( "custombias" );
        debug() << "saving custom bias entry of type:" << m_currentEntry->pluginName();
        child.setAttribute( "name", m_currentEntry->pluginName() );
        child.setAttribute( "weight", m_weight );

        e.appendChild( child );
        child.appendChild( m_currentEntry->xml( doc ) );
        
        return e;
    } else
        return QDomElement();
}

double
Dynamic::CustomBias::reevaluate( double oldEnergy, const Meta::TrackList& oldPlaylist, Meta::TrackPtr newTrack, int newTrackPos, const Meta::TrackList& context ) const
{
    Q_UNUSED( context )

    if( !m_currentEntry ) // wtf it died under us, return oldEnergy
    {
        return oldEnergy;
    }
    
    double offset = 1.0 / (double)oldPlaylist.size();

    bool prevSatisfied = m_currentEntry->trackSatisfies( oldPlaylist[newTrackPos] );
    
    if( m_currentEntry->trackSatisfies( newTrack ) && !prevSatisfied )
    {
//         debug() << "new satisfies and old doesn't:" << oldEnergy - offset;
        return oldEnergy - offset;
    } else if( !m_currentEntry->trackSatisfies( newTrack ) && prevSatisfied )
    {
//         debug() << "new doesn't satisfy and old did:" << oldEnergy + offset;
        return oldEnergy + offset;
    } else
    {
//         debug() << "no change:" << oldEnergy;
        return oldEnergy;
    }
}

bool
Dynamic::CustomBias::hasCollectionFilterCapability()
{
    return m_currentEntry && m_currentEntry->hasCollectionFilterCapability();
}


Dynamic::CollectionFilterCapability*
Dynamic::CustomBias::collectionFilterCapability()
{
    if( m_currentEntry )
        return m_currentEntry->collectionFilterCapability( weight() );
    else
        return 0;
}

void
Dynamic::CustomBias::registerNewBiasFactory( Dynamic::CustomBiasEntryFactory* entry )
{
    DEBUG_BLOCK
    debug() << "new factory of type:" << entry->name() << entry->pluginName();
    if( !s_biasFactories.contains( entry ) )
        s_biasFactories.append( entry );

    foreach( const QString &name, s_failedMap.keys() )
    {
        if( name == entry->pluginName() ) // lazy loading!
        {
            debug() << "found entry loaded without proper custombiasentry. fixing now, with  old weight of" << s_failedMap[ name ]->weight() ;
            //  need to manually set the weight, as we set it on the old widget which is now being thrown away
            Dynamic::CustomBiasEntry* cbe = entry->newCustomBiasEntry( s_failedMapXml[ name ] );
            s_failedMap[ name ]->setCurrentEntry( cbe );
            s_failedMap.remove( name );
            s_failedMapXml.remove( name );
        }
    }
    
    foreach( Dynamic::CustomBias* bias, s_biases )
        bias->refreshWidgets();
}


void
Dynamic::CustomBias::removeBiasFactory( Dynamic::CustomBiasEntryFactory* entry )
{
    DEBUG_BLOCK

    if( s_biasFactories.contains( entry ) )
        s_biasFactories.removeAll( entry );

    foreach( Dynamic::CustomBias* bias, s_biases )
        bias->refreshWidgets();
}

Dynamic::CustomBias*
Dynamic::CustomBias::fromXml(QDomElement e)
{
    DEBUG_BLOCK

    QDomElement biasNode = e.firstChildElement( "custombias" );
    if( !biasNode.isNull() )
    {
        debug() << "ok, got a custom bias node. now to create the right bias type";
        QString pluginName = biasNode.attribute( "name", QString() );
        double weight = biasNode.attribute( "weight", QString() ).toDouble();
        if( !pluginName.isEmpty() )
        {
            debug() << "got custom bias type:" << pluginName << "with weight:" << weight;
            foreach( Dynamic::CustomBiasEntryFactory* factory, s_biasFactories )
            {
                if( factory->pluginName() == pluginName )
                {
                    debug() << "found matching bias type! creating :D";
                    CustomBiasEntry* cbias = factory->newCustomBiasEntry( biasNode.firstChild().toElement() );
                    return createBias( cbias, weight );
                }
            }
            // didn't find a factory for the bias, but we at leasst get a weight, so set that and remember
            debug() << "size of s_failedMap:" << s_failedMap.keys().size();
            Dynamic::CustomBias* b = createBias( 0, weight );
            s_failedMap[ pluginName ] = b;
            s_failedMapXml[ pluginName ] = biasNode.firstChild().toElement();
            return b;
        }
    }
    // couldn't load the xml at all. so instead of crashing we'll return a default custom bias. shouldn't ever be here...
    return createBias();
}


QList< Dynamic::CustomBiasEntryFactory* >
Dynamic::CustomBias::currentFactories()
{
    return s_biasFactories;
}

Dynamic::CustomBias*
Dynamic::CustomBias::createBias()
{
    DEBUG_BLOCK
    Dynamic::CustomBias* newBias = new Dynamic::CustomBias();
    s_biases.append( newBias );
    return newBias;

}

Dynamic::CustomBias*
Dynamic::CustomBias::createBias( Dynamic::CustomBiasEntry* entry, double weight )
{
    DEBUG_BLOCK
    Dynamic::CustomBias* newBias = new Dynamic::CustomBias( entry, weight );
    s_biases.append( newBias );
    return newBias;
}


void Dynamic::CustomBias::customBiasChanged()
{
    emit biasChanged( this );
}


void
Dynamic::CustomBias::setCurrentEntry( Dynamic::CustomBiasEntry* entry )
{
    if( m_currentEntry )
        delete m_currentEntry;
    connect( entry, SIGNAL( biasChanged() ), this, SLOT( customBiasChanged() ) );
    m_currentEntry = entry;
}

Dynamic::CustomBiasEntry*
Dynamic::CustomBias::currentEntry()
{
    return m_currentEntry;
}

void
Dynamic::CustomBias::setWeight( double weight )
{
    m_weight = weight;
}

void Dynamic::CustomBias::refreshWidgets()
{
    emit( biasFactoriesChanged() );
}

#include "CustomBias.moc"
