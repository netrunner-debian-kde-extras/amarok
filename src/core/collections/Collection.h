/****************************************************************************************
 * Copyright (c) 2007-2008 Maximilian Kossick <maximilian.kossick@googlemail.com>       *
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

#ifndef AMAROK_COLLECTION_H
#define AMAROK_COLLECTION_H

#include "core/support/Amarok.h"
#include "core/support/PluginFactory.h"
#include "shared/amarok_export.h"
#include "core/capabilities/Capability.h"
#include "core/collections/QueryMaker.h"

#include <QObject>
#include <QSharedData>
#include <QString>

#include <KIcon>
#include <KUrl>
#include <KPluginInfo>

namespace Playlists {
class UserPlaylistProvider;
}

namespace Collections
{

class Collection;
class CollectionLocation;

class AMAROK_CORE_EXPORT CollectionFactory : public Plugins::PluginFactory
{
    Q_OBJECT
    public:
        CollectionFactory( QObject *parent, const QVariantList &args );
        virtual ~CollectionFactory();

        virtual void init() = 0;

    signals:
        void newCollection( Collections::Collection *newCollection );

};

class AMAROK_CORE_EXPORT TrackProvider
{
    public:
        TrackProvider();
        virtual ~TrackProvider();

        /**
            Returns true if this track provider has a chance of providing the
            track specified by @p url.
            This should do a minimal amount of checking, and return quickly.
        */
        virtual bool possiblyContainsTrack( const KUrl &url ) const;
        /**
            Creates a TrackPtr object for url @p url.  Returns a null track Ptr if
            it cannot be done.
            If asynchronysity is desired it is suggested to return a MetaProxy track here
            and have the proxy watch for the real track.
        */
        virtual Meta::TrackPtr trackForUrl( const KUrl &url );
};

class AMAROK_CORE_EXPORT CollectionBase : public QSharedData
{

    public:
        CollectionBase() {}
        virtual ~CollectionBase() {}

        virtual bool hasCapabilityInterface( Capabilities::Capability::Type type ) const;

        virtual Capabilities::Capability* createCapabilityInterface( Capabilities::Capability::Type type );

        /**
         * Retrieves a specialized interface which represents a capability of this
         * MetaBase object.
         *
         * @returns a pointer to the capability interface if it exists, 0 otherwise
         */
        template <class CapIface> CapIface *create()
        {
            Capabilities::Capability::Type type = CapIface::capabilityInterfaceType();
            Capabilities::Capability *iface = createCapabilityInterface(type);
            return qobject_cast<CapIface *>(iface);
        }

        /**
         * Tests if a MetaBase object provides a given capability interface.
         *
         * @returns true if the interface is available, false otherwise
         */
        template <class CapIface> bool is() const
        {
            return hasCapabilityInterface( CapIface::capabilityInterfaceType() );
        }

        private: // no copy allowed
            Q_DISABLE_COPY(CollectionBase)
};

class AMAROK_CORE_EXPORT Collection : public QObject, public TrackProvider, public CollectionBase
{
    Q_OBJECT
    public:

        Collection();
        virtual ~Collection();

        /**
            The collection's querymaker
            @return A querymaker that belongs to this collection.
        */
        virtual QueryMaker * queryMaker() = 0;

        /**
            Checks if the given path is covered by this collection.
            Not all collections cover directories or even know what a path is.
            @returns true if it is covered.
        */
        virtual bool isDirInCollection(const QString &path) { Q_UNUSED( path ); return false; }

        /**
            The protocol of uids coming from this collection.
            @return A string of the protocol, without the ://
        */
        virtual QString uidUrlProtocol() const;
        /**
            @return A unique identifier for this type of collection
        */
        virtual QString collectionId() const = 0;
        /**
            @return a user visible name for this collection, to be displayed in the collectionbrowser and elsewhere
        */
        virtual QString prettyName() const = 0;
        /**
         * @return an icon representing this collection
         */
        virtual KIcon icon() const = 0;

        /**
         * A local collection cannot have a capacity since it may be spread over multiple
         * physical locations (even network components)
         */
        virtual bool hasCapacity() const { return false; }
        virtual float usedCapacity() const { return 0.0; }
        virtual float totalCapacity() const { return 0.0; }

        virtual Collections::CollectionLocation* location();

        //convenience methods so that it is not necessary to create a CollectionLocation
        virtual bool isWritable() const;
        virtual bool isOrganizable() const;

    public slots:
        virtual void collectionUpdated() { emit updated(); }


    signals:
        void remove();

        /** This signal is sent when the collection has changed.
         *  This signal is sent when the collection more than can be detected by
         *  Meta::metaDataChanged.
         *  This is e.g. a new song was added, an old one removed, new device added, ...
         *
         *  Specifically this means that previous done searches can no longer
         *  be considered valid.
         */
        void updated();
};

}

#define AMAROK_EXPORT_COLLECTION( classname, libname ) \
    K_PLUGIN_FACTORY( factory, registerPlugin<classname>(); ) \
            K_EXPORT_PLUGIN( factory( "amarok_collection-" #libname ) )

#endif /* AMAROK_COLLECTION_H */
