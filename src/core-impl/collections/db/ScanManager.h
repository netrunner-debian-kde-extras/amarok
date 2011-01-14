/****************************************************************************************
 * Copyright (c) 2003-2008 Mark Kretschmann <kretschmann@kde.org>                       *
 * Copyright (c) 2007 Maximilian Kossick <maximilian.kossick@googlemail.com>            *
 * Copyright (c) 2007 Casey Link <unnamedrambler@gmail.com>                             *
 * Copyright (c) 2008-2009 Jeff Mitchell <mitchell@kde.org>                             *
 * Copyright (c) 2010 Ralf Engels <ralf-engels@gmx.de>                                  *
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

#ifndef AMAROK_DATABASE_SCANMANAGER_H
#define AMAROK_DATABASE_SCANMANAGER_H

#include "AmarokProcess.h"
#include "amarok_databasecollection_export.h"
#include "ScanResultProcessor.h"
#include "DatabaseCollection.h"

#include <QObject>
#include <QProcess>
#include <QSet>
#include <QMutex>
#include <QWaitCondition>
#include <QXmlStreamReader>

#include <threadweaver/Job.h>
#include <KUrl>

class ScannerJob;
class QSharedMemory;

class AMAROK_DATABASECOLLECTION_EXPORT_TESTS ScanManager : public QObject
{
    Q_OBJECT

    public:
        ScanManager( Collections::DatabaseCollection *collection, QObject *parent = 0 );
        virtual ~ScanManager();

        /** Aborts a currently running scan and blocks starting new scans if set to true.
         *  After blockScan has been set to false the scan will not be resumed.
         *  Requested scans will be delayed until the blocking has stopped.
         *  If the scan has been blocked twice it needs to be unblocked twice again.
         */
        virtual void blockScan();

        virtual void unblockScan();

        virtual bool isRunning();

    public slots:
        /** Requests the scanner to do a full scan at the next possibility.
         *  The full scan will clear the whole database and re-create all data.
         *  Statistics are not touched.
         */
        virtual void requestFullScan();

        /** Requests the scanner to do a full scan using the given import file.
         */
        virtual void requestImport( QIODevice *input );

        /** Requests the scanner to do an incremental scan.
         *  The incremental scan will check for new files or sub-folders.
         *  @param directory The directory to scan or and empty string if every
         *  collection folder should be checked for changes.
         */
        virtual void requestIncrementalScan( const QString &directory = QString() );

        /** Abort the request and all currently running scans. */
        virtual void abort( const QString &reason = QString() );

    signals:
        /** This are status messages that the scanner emits frequently */
        void message( QString message );

        void succeeded();
        void failed( QString message );

    private slots:
        /** Tries to start the scanner.
            Does nothing if scanning is currently blocked, if another scanner
            is running or if there is nothing to do.
        */
        void startScanner();

        /** This slot is called once to check the scanner version.
            An error message is displayed if the versions don't match.
        */
        void slotCheckScannerVersion();

        /** Slot is called when the check folder timer runs out. */
        void slotWatchFolders();

        /** Called when the scanner job has finished. */
        void slotJobDone();


    private:
        Collections::DatabaseCollection *m_collection;

        ScannerJob *m_scanner;

        int m_restartCount;
        int m_blockCount;

        bool m_fullScanRequested;
        QSet<QString> m_scanDirsRequested;
        QIODevice *m_importRequested;

        /**
           This mutex is protecting the variables:
           m_fullScanRequested, m_scanDirsRequested, m_importRequested, m_scanner
          */
        QMutex m_mutex;
};

/** This is the job that does all the hard work with scanning.
    It will receive new data from the scanner process, parse it and call the
    ScanResultProcessor.
    The job will delete itself when finished or aborted.
*/
class ScannerJob : public ThreadWeaver::Job
{
    Q_OBJECT

    public:
        /** Creates the parse job.
            The constructor itself should be called from the UI thread.
            @param xmlFilePath An optional xml file that is parsed
        */
        ScannerJob( QObject *parent, Collections::DatabaseCollection *collection,
                    QIODevice *input = 0 );

        ScannerJob( QObject *parent, Collections::DatabaseCollection *collection,
                    ScanResultProcessor::ScanType scanType,
                    QStringList scanDirsRequested );

        ~ScannerJob();

        void run();
        void requestAbort();
        void requestAbort(const QString &reason);

        /** Returns the path to the collection scanner */
        static QString scannerPath();

    // note: since this job doesn't have it's own event queue all signals and slots
    // go through the UI event queue
    Q_SIGNALS:
        void totalSteps( const QObject *o, int totalSteps );
        void step( const QObject *o );

        /** This are status messages that the scanner emits frequently */
        void message( QString message );

        void succeeded();
        void failed( QString message );
        // and the ThreadWeaver::Job also emits done

    private Q_SLOTS:
        void directoryProcessed();

    private:
        /** Creates the scanner process.
            @returns true if successfull started
        */
        bool createScannerProcess( bool restart );
        bool tryRestart();

        /** Wait for the scanner to produce some output or die */
        void getScannerOutput();

        /**
           Returns a list of all directories and their modification time from the
           database.
         */
        QList< QPair<QString, uint> > getKnownDirs();

        Collections::DatabaseCollection *m_collection;
        ScanResultProcessor::ScanType m_scanType;
        QStringList m_scanDirsRequested;
        QScopedPointer< QIODevice > m_input;

        int m_restartCount;
        bool m_abortRequested;
        QString m_abortReason;
        QString m_incompleteTagBuffer; // strings received via addNewXmlData but not terminated by either a </directory> or a </scanner>

        AmarokProcess *m_scanner;
        QString m_batchfilePath;
        QSharedMemory *m_scannerStateMemory; // a persistent storage of the current scanner state in case it needs to be restarted.
        QString       m_sharedMemoryKey;

        QXmlStreamReader m_reader;

        QMutex m_mutex; // only protects m_abortRequested and the abort reason

};

#endif
