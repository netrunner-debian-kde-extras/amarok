/****************************************************************************************
 * Copyright (c) 2009 Nikolaj Hald Nielsen <nhnFreespirit@gmail.com>                    *
 *                                                                                      *
 * This program is free software; you can redistribute it and/or modify it under        *
 * the terms of the GNU General Public License as published by the Free Software        *
 * Foundation; either version 2 of the License, or (at your option) any later           *
 * version.                                                                             *
 *                                                                                      *
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY      *
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A      *
 * PARTICULAR PURPOSE. See the GNU General Pulic License for more details.              *
 *                                                                                      *
 * You should have received a copy of the GNU General Public License along with         *
 * this program.  If not, see <http://www.gnu.org/licenses/>.                           *
 ****************************************************************************************/
 
#ifndef AUDIOCDCOLLECTION_H
#define AUDIOCDCOLLECTION_H

#include "Collection.h"
#include "MediaDeviceCollection.h"
#include "MemoryCollection.h"
#include "meta/proxy/MetaProxy.h"

#include <kio/jobclasses.h>
#include <KUrl>

#include <QAction>
#include <QObject>

class AudioCdCollection;
class MediaDeviceInfo;

class AudioCdCollectionFactory : public MediaDeviceCollectionFactory<AudioCdCollection>
{
    Q_OBJECT
public:
    AudioCdCollectionFactory();
    virtual ~AudioCdCollectionFactory() {};

/*    virtual void init();

private slots:
    void audioCdAdded( const QString &uid );
    void deviceRemoved( const QString &uid );

private:

    QString m_currentUid;
    AudioCdCollection * m_collection;*/
    
};


/**
This is a Memorycollection sublclass that uses the KIO audiocd:/ slave to populate itself whenever it detects a cd.

	@author Nikolaj Hald Nielsen <nhnFreespirit@gmail.com>
*/
class AudioCdCollection : public MediaDeviceCollection
{
    Q_OBJECT
public:

    enum { WAV, FLAC, OGG, MP3 } EncodingFormat;

    AudioCdCollection( MediaDeviceInfo* info );
    ~AudioCdCollection();

    QString encodingFormat() const;
    QString copyableBasePath() const;

    void setEncodingFormat( int format ) const;

    virtual QueryMaker * queryMaker();
    virtual QString collectionId() const;
    virtual QString prettyName() const;
    virtual KIcon icon() const;

    virtual CollectionLocation* location() const;

    bool possiblyContainsTrack( const KUrl &url ) const;
    Meta::TrackPtr trackForUrl( const KUrl &url );

    QAction * ejectAction();

    void cdRemoved();

    virtual bool hasCapabilityInterface( Meta::Capability::Type type ) const;
    virtual Meta::Capability* asCapabilityInterface( Meta::Capability::Type type );

    virtual void startFullScan(); //Override this one as I really dont want to do move partsing to the handler atm.
    virtual void startFullScanDevice() { startFullScan(); }

public slots:
    void infoFetchComplete( KJob *job );
    void eject();

private:

    void readAudioCdSettings();
    
    /**
     * Clear collection and read the cd currently in the drive, adding Artist, Album,
     * Genre, Year and whatnot as detected by audiocd using CDDB.
     */
    void readCd();

    void noInfoAvailable();

    void updateProxyTracks();

    KIO::StoredTransferJob * m_cdInfoJob;

    QString m_cdName;
    QString m_discCddbId;
    QString m_udi;
    mutable int m_encodingFormat;

    QAction * m_ejectAction;

    QString m_fileNamePattern;
    QString m_albumNamePattern;

    QMap<KUrl, MetaProxy::Track*> m_proxyMap;


    
};

#endif