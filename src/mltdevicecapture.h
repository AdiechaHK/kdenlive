/***************************************************************************
                         mltdevicecapture.h  -  description
                            -------------------
   begin                : Sun May 21 2011
   copyright            : (C) 2011 by Jean-Baptiste Mardelle (jb@kdenlive.org)

***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

/**
 * @class MltDeviceCapture
 * @brief Interface for MLT capture.
 */

#ifndef MLTDEVICECAPTURE_H
#define MLTDEVICECAPTURE_H

#include "gentime.h"
#include "definitions.h"
#include "abstractmonitor.h"

#include <mlt/framework/mlt_types.h>

#include <QtConcurrentRun>
#include <QTimer>
 
namespace Mlt
{
class Consumer;
class Frame;
class Event;
class Producer;
class Profile;
};

class MltDeviceCapture: public AbstractRender
{
Q_OBJECT public:

    enum FailStates { OK = 0,
                      APP_NOEXIST
                    };
    /** @brief Build a MLT Renderer
     *  @param winid The parent widget identifier (required for SDL display). Set to 0 for OpenGL rendering
     *  @param profile The MLT profile used for the capture (default one will be used if empty). */
    MltDeviceCapture(QString profile, VideoSurface *surface, QWidget *parent = 0);

    /** @brief Destroy the MLT Renderer. */
    ~MltDeviceCapture();

    int doCapture;

    /** @brief This property is used to decide if the renderer should convert it's frames to QImage for use in other Kdenlive widgets. */
    bool sendFrameForAnalysis;

    /** @brief Someone needs us to send again a frame. */
    void sendFrameUpdate() {};
    
    void emitFrameUpdated(Mlt::Frame&);
    void emitFrameNumber(double position);
    void emitConsumerStopped();
    void showFrame(Mlt::Frame&);
    void showAudio(Mlt::Frame&);

    void saveFrame(Mlt::Frame& frame);

    /** @brief Starts the MLT Video4Linux process.
     * @param surface The widget onto which the frame should be painted
     */
    bool slotStartCapture(const QString &params, const QString &path, const QString &playlist, bool livePreview, bool xmlPlaylist = true);
    bool slotStartPreview(const QString &producer, bool xmlFormat = false);
    /** @brief A frame arrived from the MLT Video4Linux process. */
    void gotCapturedFrame(Mlt::Frame& frame);
    /** @brief Save current frame to file. */
    void captureFrame(const QString &path);
    
    /** @brief This will add the video clip from path and add it in the overlay track. */
    void setOverlay(const QString &path);

    /** @brief This will add an MLT video effect to the overlay track. */
    void setOverlayEffect(const QString &tag, QStringList parameters);

    /** @brief This will add a horizontal flip effect, easier to work when filming yourself. */
    void mirror(bool activate);
    
    /** @brief True if we are processing an image (yuv > rgb) when recording. */
    bool processingImage;
    
    void pause();

private:
    Mlt::Consumer * m_mltConsumer;
    Mlt::Producer * m_mltProducer;
    Mlt::Profile *m_mltProfile;
    Mlt::Event *m_showFrameEvent;
    QString m_activeProfile;
    int m_droppedFrames;
    /** @brief When true, images will be displayed on monitor while capturing. */
    bool m_livePreview;
    /** @brief Count captured frames, used to display only one in ten images while capturing. */
    int m_frameCount;

    /** @brief The surface onto which the captured frames should be painted. */
    VideoSurface *m_captureDisplayWidget;

    /** @brief A human-readable description of this renderer. */
    int m_winid;

    void uyvy2rgb(unsigned char *yuv_buffer, int width, int height);

    QString m_capturePath;
    
    QTimer m_droppedFramesTimer;
    
    QMutex m_mutex;

    /** @brief Build the MLT Consumer object with initial settings.
     *  @param profileName The MLT profile to use for the consumer */
    void buildConsumer(const QString &profileName = QString());


private slots:
    void slotPreparePreview();
    void slotAllowPreview();
    /** @brief When capturing, check every second for dropped frames. */
    void slotCheckDroppedFrames();
  
signals:
    /** @brief A frame's image has to be shown.
     *
     * Used in Mac OS X. */
    void showImageSignal(QImage);

    void frameSaved(const QString &);
    
    void droppedFrames(int);
    
    void unblockPreview();
    void imageReady(QImage);


public slots:

    /** @brief Stops the consumer. */
    void stop();
    void slotDoRefresh();
};

#endif
