/****************************************************************************************
 * Copyright (c) 2009 Simon Esneault <simon.esneault@gmail.com>                         *
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

//Plasma applet for showing video from youtube dailymotion and vimeo in the CV

#ifndef VIDEOCLIP_APPLET_H
#define VIDEOCLIP_APPLET_H

#include "context/Applet.h"
#include "context/DataEngine.h"
#include "core/support/SmartPointerList.h"

#include "VideoclipInfo.h"

#include "ui_videoclipSettings.h"

#include <Phonon/Path>
#include <Phonon/MediaObject>

#include <QWeakPointer>


// Forward declarations
namespace Phonon
{
    class MediaObject;
    class Path;
    class VideoWidget;
}

namespace Plasma
{
    class IconWidget;
    class ScrollWidget;
}

class KConfigDialog;
class QGraphicsWidget;
class QGraphicsLinearLayout;
class QGraphicsSceneResizeEvent;

class CustomVideoWidget;
class VideoItemButton;


 /** VideoclipApplet will display videoclip from the Internet, relative to the current playing song
   * If a video is detected in the playlist, it will also play the video inside the VideoWidget.
   */
class VideoclipApplet : public Context::Applet
{
        Q_OBJECT

    public:
        VideoclipApplet( QObject* parent, const QVariantList& args );
        ~VideoclipApplet();

        virtual void constraintsEvent( Plasma::Constraints constraints = Plasma::AllConstraints );

    public slots:
        virtual void init();

        virtual void dataUpdated( const QString& name, const Plasma::DataEngine::Data& data );
        void connectSource( const QString &source );

        // right click context menu
        void appendVideoClip( VideoInfo *info );
        void queueVideoClip( VideoInfo *info );
        void appendPlayVideoClip( VideoInfo *info );

        void saveSettings();

    protected:
        void createConfigurationInterface(KConfigDialog *parent);
        virtual void resizeEvent( QGraphicsSceneResizeEvent * event );

    private slots:
        void trackPlaying();
        void stateChanged(Phonon::State, Phonon::State );
        void stopped();

    private:
        QWeakPointer<CustomVideoWidget> m_videoWidget;
        SmartPointerList<VideoItemButton> m_videoItemButtons;

        // The two big container, only one who need a resize
        Plasma::ScrollWidget    *m_scroll;
        QGraphicsLinearLayout   *m_scrollLayout;
        QGraphicsLinearLayout   *m_layout;

        QPixmap m_pixYoutube;
        QPixmap m_pixDailymotion;
        QPixmap m_pixVimeo;

        Plasma::IconWidget      *m_settingsIcon;
        Ui::videoclipSettings   ui_Settings;
        bool                    m_youtubeHQ;
};

#endif /* VIDEOCLIP_APPLET_H */
