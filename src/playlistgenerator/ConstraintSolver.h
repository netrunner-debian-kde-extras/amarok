/****************************************************************************************
 * Copyright (c) 2008-2012 Soren Harward <stharward@gmail.com>                          *
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

#ifndef APG_CONSTRAINTSOLVER
#define APG_CONSTRAINTSOLVER

#include "core/meta/Meta.h"

#include <QMutex>
#include <QString>
#include <threadweaver/Job.h>

class ConstraintNode;

namespace Collections {
    class MetaQueryMaker;
}

namespace APG {
    class ConstraintSolver : public ThreadWeaver::Job {
        Q_OBJECT

        public:
            typedef QHash< Meta::TrackList*, double > Population;
            
            static const int QUALITY_RANGE; // allows used to adjust speed/quality tradeoff

            ConstraintSolver( ConstraintNode*, int );
            ~ConstraintSolver();

            Meta::TrackList getSolution() const;
            bool satisfied() const;
            int serial() const { return m_serialNumber; }
            int iterationCount() const { return m_maxGenerations; }

            // overloaded ThreadWeaver::Job functions
            bool canBeExecuted();
            bool success() const;

        public slots:
            void requestAbort();

        signals:
            void readyToRun();
            void incrementProgress();
            void totalSteps( int );
            void endProgressOperation( QObject* );

        protected:
            void run(); // from ThreadWeaver::Job

        private slots:
            void receiveQueryMakerData( Meta::TrackList );
            void receiveQueryMakerDone();

        private:
            int m_serialNumber;                 // a randomly-generated serial number to help with debugging
            double m_satisfactionThreshold;
            double m_finalSatisfaction;

            ConstraintNode* const m_constraintTreeRoot;
            Collections::MetaQueryMaker* m_qm;

            mutable Meta::TrackList m_domain;   // tracks available to solver
            QMutex m_domainMutex;
            bool m_domainReductionFailed;
            Meta::TrackList m_solvedPlaylist;   // playlist generated by solver

            bool m_readyToRun;                  // job execution depends on QueryMaker finishing
            bool m_abortRequested;

            // internal mathematical functions
            void fill_population( Population& );
            Meta::TrackList* find_best( const Population& ) const;
            void select_population( Population&, Meta::TrackList* );
            void mutate_population( Population& );

            Meta::TrackList* crossover( Meta::TrackList*, Meta::TrackList* ) const;
            static bool pop_comp( double, double );
            Meta::TrackPtr random_track_from_domain() const;
            Meta::TrackList sample( Meta::TrackList, const int ) const;

            double rng_gaussian( const double, const double ) const;
            quint32 rng_poisson( const double ) const;
            double rng_uniform() const;

            quint32 playlist_size() const;
            bool select( const double ) const;

            // convenience function
            void dump_population( const Population& ) const;

            // internal mathematical parameters
            quint32 m_maxGenerations;
            quint32 m_populationSize;
            quint32 m_suggestedPlaylistSize;
    };
} // namespace APG

#endif
