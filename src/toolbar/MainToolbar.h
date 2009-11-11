/****************************************************************************************
 * Copyright (c) 2007 Nikolaj Hald Nielsen <nhnFreespirit@gmail.com>                    *
 * Copyright (c) 2009 Mark Kretschmann <kretschmann@kde.org>                            *
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

#ifndef MainToolbarNNG_H
#define MainToolbarNNG_H

#include "EngineObserver.h" //baseclass
#include "SmartPointerList.h"

#include <KHBox>

#include <QToolBar>

class QAction;
class KToolBar;
class MainControlsWidget;
class ProgressWidget;
class VolumeWidget;

/**
    A KHBox based toolbar with a nice svg background and takes care of 
    adding any additional controls needed by individual tracks
*/
class MainToolbar : public QToolBar, public EngineObserver
{

public:
    MainToolbar( QWidget * parent );
    ~MainToolbar();

    virtual void engineStateChanged( Phonon::State state, Phonon::State oldState = Phonon::StoppedState );
    virtual void engineNewMetaData( const QHash<qint64, QString> &newMetaData, bool trackChanged );

protected:
      virtual void resizeEvent( QResizeEvent * event );
      void handleAddActions();
      void centerAddActions();
      virtual bool eventFilter( QObject* object, QEvent* event );

private:
    QWidget            *m_insideBox;
    KToolBar           *m_addControlsToolbar;
    VolumeWidget       *m_volumeWidget;
    MainControlsWidget *m_mainControlsWidget;
    ProgressWidget     *m_progressWidget;

    SmartPointerList<QAction> m_additionalActions;
};

#endif
