/****************************************************************************************
 * Copyright (c) 2003 Stanislav Karchebny <berkus@users.sf.net>                         *
 * Copyright (c) 2009 Mark Kretschmann <kretschmann@kde.org>                            *
 * Copyright (c) 2009,2010 Kevin Funk <krf@electrostorm.net>                            *
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

#ifndef AMAROK_TRAYICON_H
#define AMAROK_TRAYICON_H

#include "core/engine/EngineObserver.h" // baseclass
#include "core/meta/Meta.h"
#include "core/support/SmartPointerList.h"

#include <KStatusNotifierItem> // baseclass

#include <QAction>
#include <QPointer>


namespace Amarok {

class TrayIcon : public KStatusNotifierItem, public Engine::EngineObserver, public Meta::Observer
{
    Q_OBJECT

public:
    TrayIcon( QObject *parent );

    void setVisible( bool visible );

protected:
    // reimplemented from engineobserver
    virtual void engineStateChanged( Phonon::State state, Phonon::State oldState = Phonon::StoppedState );
    virtual void engineNewTrackPlaying();
    virtual void engineVolumeChanged( int percent );
    virtual void engineMuteStateChanged( bool mute );

    // reimplemented from Meta::Observer
    using Observer::metadataChanged;
    virtual void metadataChanged( Meta::TrackPtr track );
    virtual void metadataChanged( Meta::AlbumPtr album );

private slots:
    void slotActivated();
    void slotScrollRequested( int delta, Qt::Orientation orientation );

private:
    void setupMenu();
    void setupToolTip( bool updateIcon );

    Meta::TrackPtr m_track;

    SmartPointerList<QAction> m_extraActions;
    QPointer<QAction> m_separator;
};

}

#endif // AMAROK_TRAYICON_H
