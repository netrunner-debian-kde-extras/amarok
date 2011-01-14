/****************************************************************************************
 * Copyright (c) 2010 Rick W. Chen <stuffcorpse@archlinux.us>                           *
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

#ifndef RECENTLYPLAYEDLISTWIDGET_H
#define RECENTLYPLAYEDLISTWIDGET_H

#include "core/meta/Meta.h"

#include <KIcon>
#include <Plasma/ScrollWidget>

class QGraphicsLinearLayout;

class RecentlyPlayedListWidget : public Plasma::ScrollWidget
{
    Q_OBJECT

public:
    explicit RecentlyPlayedListWidget( QGraphicsWidget *parent = 0 );
    virtual ~RecentlyPlayedListWidget();

    void clear();

    void startQuery();
    void addTrack( const Meta::TrackPtr &track );

private slots:
    void tracksReturned( QString id, Meta::TrackList );
    void trackChanged( Meta::TrackPtr track );
    void setupTracksData();

private:
    void removeLast();
    void removeItem( QGraphicsLayoutItem *item );

    KIcon m_trackIcon;
    Meta::TrackPtr m_currentTrack;
    Meta::TrackList m_recentTracks;
    QGraphicsLinearLayout *m_layout;
    QMap<uint, QGraphicsLayoutItem*> m_items;
    Q_DISABLE_COPY( RecentlyPlayedListWidget )
};

#endif // RECENTLYPLAYEDLISTWIDGET_H
