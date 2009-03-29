/***************************************************************************
 * copyright     : (C) 2004 Mark Kretschmann <markey@web.de>               *
                   (C) 2007 Dan Meltzer <parallelgrapefruit@gmail.com>   *
 **************************************************************************/

 /***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef VOLUMEWIDGET_H
#define VOLUMEWIDGET_H

#include "SliderWidget.h"
#include "EngineObserver.h"

#include <QPointer>
#include <KHBox>

/*
* A custom widget that serves as our volume slider within Amarok.
*/
class VolumeWidget : public KHBox, public EngineObserver
{
    Q_OBJECT
public:
    VolumeWidget( QWidget * );
    Amarok::Slider* slider() const { return m_slider; }

private slots:
    void engineVolumeChanged( int value );

private:
    QPointer<Amarok::Slider> m_slider;

};

#endif
