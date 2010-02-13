/****************************************************************************************
 * Copyright (c) 2007 Bart Cerneels <bart.cerneels@kde.org>                             *
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

#ifndef SQLPODCASTPROVIDER_H
#define SQLPODCASTPROVIDER_H

#include "PodcastProvider.h"
#include "SqlPodcastMeta.h"

#include <kio/jobclasses.h>
#include <klocale.h>

class PodcastImageFetcher;

class KDialog;
class KUrl;
class PodcastReader;
class SqlStorage;
class QTimer;

/**
	@author Bart Cerneels <bart.cerneels@kde.org>
*/
class SqlPodcastProvider : public PodcastProvider
{
    Q_OBJECT
    public:
        SqlPodcastProvider();
        ~SqlPodcastProvider();

        bool possiblyContainsTrack( const KUrl &url ) const;
        Meta::TrackPtr trackForUrl( const KUrl &url );

        QString prettyName() const { return i18n("Local Podcasts"); }
        KIcon icon() const { return KIcon( "server-database" ); }

        Meta::PlaylistList playlists();

        //PodcastProvider methods
        void addPodcast( const KUrl &url );

        Meta::PodcastChannelPtr addChannel( Meta::PodcastChannelPtr channel );
        Meta::PodcastEpisodePtr addEpisode( Meta::PodcastEpisodePtr episode );

        Meta::PodcastChannelList channels();

        void removeSubscription( Meta::PodcastChannelPtr channel );

        void configureProvider();
        void configureChannel( Meta::PodcastChannelPtr channel );

        QList<QAction *> episodeActions( Meta::PodcastEpisodeList );
        QList<QAction *> channelActions( Meta::PodcastChannelList );

        virtual QList<QAction *> providerActions();

        void completePodcastDownloads();

        //SqlPodcastProvider specific methods
        Meta::SqlPodcastChannelPtr podcastChannelForId( int podcastChannelDbId );

        KUrl baseDownloadDir() const { return m_baseDownloadDir; }

    public slots:
        void updateAll();
        void update( Meta::PodcastChannelPtr channel );
        void downloadEpisode( Meta::PodcastEpisodePtr episode );
        void deleteDownloadedEpisode( Meta::PodcastEpisodePtr episode );
        void slotUpdated();

        void slotReadResult( PodcastReader *podcastReader );
        void update( Meta::SqlPodcastChannelPtr channel );
        void downloadEpisode( Meta::SqlPodcastEpisodePtr episode );
        void deleteDownloadedEpisode( Meta::SqlPodcastEpisodePtr episode );

    private slots:
        void downloadResult( KJob * );
        void addData( KIO::Job * job, const QByteArray & data );
        void redirected( KIO::Job *, const KUrl& );
        void autoUpdate();
        void slotDeleteSelectedEpisodes();
        void slotDownloadEpisodes();
        void slotConfigureChannel();
        void slotRemoveChannels();
        void slotUpdateChannels();
        void slotDownloadProgress( KJob *job, unsigned long percent );
        void slotWriteTagsToFiles();
        void slotConfigChanged();

    signals:
        void updated();
        void totalPodcastDownloadProgress( int progress );

    private slots:
        void channelImageReady( Meta::PodcastChannelPtr, QPixmap );
        void podcastImageFetcherDone( PodcastImageFetcher * );
        void slotConfigureProvider();

    private:
        /** creates all the necessary tables, indexes etc. for the database */
        void createTables() const;
        void loadPodcasts();

        /** return the url as a string. Removes percent encoding if it actually has a non-url guid.
        */
        static QString cleanUrlOrGuid( const KUrl &url );

        void updateDatabase( int fromVersion, int toVersion );
        void fetchImage( Meta::SqlPodcastChannelPtr channel );

        /** shows a modal dialog asking the user if he really wants to unsubscribe
            and if he wants to keep the podcast media */
        QPair<bool, bool> confirmUnsubscribe(Meta::PodcastChannelPtr channel);

        /** remove the episodes in the list from the filesystem */
        void deleteEpisodes( Meta::PodcastEpisodeList & episodes );

        void subscribe( const KUrl &url );
        QFile* createTmpFile ( KJob* job );
        void cleanupDownload( KJob* job, bool downloadFailed );

        /** returns true if the file that is downloaded by 'job' is already locally available */
        bool checkEnclosureLocallyAvailable( KIO::Job *job );

        Meta::SqlPodcastChannelList m_channels;

        QTimer *m_updateTimer;
        int m_autoUpdateInterval; //interval between autoupdate attempts in minutes
        unsigned int m_updatingChannels;
        unsigned int m_maxConcurrentUpdates;
        Meta::PodcastChannelList m_updateQueue;
        QList<KUrl> m_subscribeQueue;

        QHash<KJob *, Meta::SqlPodcastEpisode *> m_downloadJobMap;
        QHash<KJob *, QString> m_fileNameMap;
        QHash<KJob *, QFile*> m_tmpFileMap;

        Meta::SqlPodcastEpisodeList m_downloadQueue;
        int m_maxConcurrentDownloads;
        int m_completedDownloads;

        KUrl m_baseDownloadDir;

        KDialog *m_providerSettingsDialog;

        QList<QAction *> m_providerActions;

        QAction *m_configureChannelAction; //Configure a Channel
        QAction *m_deleteAction; //delete a downloaded Episode
        QAction *m_downloadAction;
        QAction *m_removeAction; //remove a subscription
        QAction *m_renameAction; //rename a Channel or Episode
        QAction *m_updateAction;
        QAction *m_writeTagsAction; //write feed information to downloaded file

        PodcastImageFetcher *m_podcastImageFetcher;
};

#endif
