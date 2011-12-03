/***************************************************************************
 *   Copyright (C) 2007 by Jean-Baptiste Mardelle (jb@kdenlive.org)        *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA          *
 ***************************************************************************/

#ifndef MONITOR_H
#define MONITOR_H


#include "gentime.h"
#include "renderer.h"
#include "timecodedisplay.h"
#include "abstractmonitor.h"
#if defined(Q_WS_MAC) || defined(USE_OPEN_GL)
#include "videoglwidget.h"
#endif

#include <QLabel>
#include <QDomElement>
#include <QToolBar>
#include <QSlider>

#include <KIcon>
#include <KAction>
#include <KRestrictedLine>

class SmallRuler;
class DocClipBase;
class AbstractClipItem;
class Transition;
class ClipItem;
class MonitorEditWidget;
class Monitor;
class MonitorManager;

class VideoContainer : public QFrame
{
    Q_OBJECT
public:
    VideoContainer(Monitor *parent = 0);
    void switchFullScreen();

protected:
    virtual void mouseDoubleClickEvent(QMouseEvent * event);
    virtual void mousePressEvent(QMouseEvent * event);
    virtual void mouseReleaseEvent(QMouseEvent *event);
    virtual void mouseMoveEvent(QMouseEvent *event);
    void keyPressEvent(QKeyEvent *event);
    virtual void wheelEvent(QWheelEvent * event);

private:
    Qt::WindowFlags m_baseFlags;
    Monitor *m_monitor;
};

class MonitorRefresh : public QWidget
{
    Q_OBJECT
public:
    MonitorRefresh(QWidget *parent = 0);
    void setRenderer(Render* render);

private:
    Render *m_renderer;
};

class Overlay : public QLabel
{
    Q_OBJECT
public:
    Overlay(QWidget* parent = 0);
    void setOverlayText(const QString &, bool isZone = true);

private:
    bool m_isZone;

protected:
    virtual void mouseDoubleClickEvent ( QMouseEvent * event );
    virtual void mousePressEvent ( QMouseEvent * event );
    virtual void mouseReleaseEvent ( QMouseEvent * event );
    
signals:
    void editMarker();
};

class Monitor : public AbstractMonitor
{
    Q_OBJECT

public:
    Monitor(QString name, MonitorManager *manager, QString profile = QString(), QWidget *parent = 0);
    ~Monitor();
    Render *render;
    AbstractRender *abstractRender();
    void resetProfile(const QString &profile);
    const QString name() const;
    void resetSize();
    bool isActive() const;
    void pause();
    void setupMenu(QMenu *goMenu, QAction *playZone, QAction *loopZone, QMenu *markerMenu = NULL, QAction *loopClip = NULL);
    const QString sceneList();
    DocClipBase *activeClip();
    GenTime position();
    void checkOverlay();
    void updateTimecodeFormat();
    void updateMarkers(DocClipBase *source);
    MonitorEditWidget *getEffectEdit();
    QWidget *container();
    void reloadProducer(const QString &id);
    QFrame *m_volumePopup;

protected:
    virtual void mousePressEvent(QMouseEvent * event);
    virtual void mouseReleaseEvent(QMouseEvent * event);
    virtual void resizeEvent(QResizeEvent *event);

    /** @brief Move to another position on mouse wheel event.
     *
     * Moves towards the end of the clip/timeline on mouse wheel down/back, the
     * opposite on mouse wheel up/forward.
     * Ctrl + wheel moves by a second, without Ctrl it moves by a single frame. */
    virtual void wheelEvent(QWheelEvent * event);
    virtual void mouseMoveEvent(QMouseEvent *event);
    virtual QStringList mimeTypes() const;
    /*virtual void dragMoveEvent(QDragMoveEvent * event);
    virtual Qt::DropActions supportedDropActions() const;*/

    //virtual void resizeEvent(QResizeEvent * event);
    //virtual void paintEvent(QPaintEvent * event);

private:
    QString m_name;
    MonitorManager *m_monitorManager;
    DocClipBase *m_currentClip;
    SmallRuler *m_ruler;
    Overlay *m_overlay;
    double m_scale;
    int m_length;
    bool m_dragStarted;
    MonitorRefresh *m_monitorRefresh;
    KIcon m_playIcon;
    KIcon m_pauseIcon;
    TimecodeDisplay *m_timePos;
    QAction *m_playAction;
    /** Has to be available so we can enable and disable it. */
    QAction *m_loopClipAction;
    QMenu *m_contextMenu;
    QMenu *m_configMenu;
    QMenu *m_playMenu;
    QMenu *m_markerMenu;
    QPoint m_DragStartPosition;
    MonitorEditWidget *m_effectWidget;
    VideoContainer *m_videoBox;
    /** Selected clip/transition in timeline. Used for looping it. */
    AbstractClipItem *m_selectedClip;
    /** true if selected clip is transition, false = selected clip is clip.
     *  Necessary because sometimes we get two signals, e.g. we get a clip and we get selected transition = NULL. */
    bool m_loopClipTransition;

#if defined(Q_WS_MAC) || defined(USE_OPEN_GL)
    VideoGLWidget *m_glWidget;
    bool createOpenGlWidget(QWidget *parent, const QString profile);
#endif

    GenTime getSnapForPos(bool previous);
    Qt::WindowFlags m_baseFlags;
    QToolBar *m_toolbar;
    QWidget *m_volumeWidget;
    QSlider *m_audioSlider;
    QAction *m_editMarker;

private slots:
    void seekCursor(int pos);
    void rendererStopped(int pos);
    void slotExtractCurrentFrame();
    void slotSetThumbFrame();
    void slotSetSizeOneToOne();
    void slotSetSizeOneToTwo();
    void slotSaveZone();
    void slotSeek();
    void setClipZone(QPoint pos);
    void slotSwitchMonitorInfo(bool show);
    void slotSwitchDropFrames(bool show);
    void slotGoToMarker(QAction *action);
    void slotSetVolume(int volume);
    void slotShowVolume();
    void slotEditMarker();

public slots:
    void slotOpenFile(const QString &);
    void slotSetClipProducer(DocClipBase *clip, QPoint zone = QPoint(), bool forceUpdate = false, int position = -1);
    void updateClipProducer(Mlt::Producer *prod);
    void refreshMonitor(bool visible);
    void refreshMonitor();
    void slotSeek(int pos);
    void stop();
    void start();
    bool activateMonitor();
    void slotPlay();
    void slotPlayZone();
    void slotLoopZone();
    /** @brief Loops the selected item (clip or transition). */
    void slotLoopClip();
    void slotForward(double speed = 0);
    void slotRewind(double speed = 0);
    void slotRewindOneFrame(int diff = 1);
    void slotForwardOneFrame(int diff = 1);
    void saveSceneList(QString path, QDomElement info = QDomElement());
    void slotStart();
    void slotEnd();
    void slotSetZoneStart();
    void slotSetZoneEnd();
    void slotZoneStart();
    void slotZoneEnd();
    void slotZoneMoved(int start, int end);
    void slotSeekToNextSnap();
    void slotSeekToPreviousSnap();
    void adjustRulerSize(int length);
    void setTimePos(const QString &pos);
    QStringList getZoneInfo() const;
    void slotEffectScene(bool show = true);
    bool effectSceneDisplayed();

    /** @brief Sets m_selectedClip to @param item. Used for looping it. */
    void slotSetSelectedClip(AbstractClipItem *item);
    void slotSetSelectedClip(ClipItem *item);
    void slotSetSelectedClip(Transition *item);
    void slotMouseSeek(int eventDelta, bool fast);
    void slotSwitchFullScreen();

signals:
    void renderPosition(int);
    void durationChanged(int);
    void refreshClipThumbnail(const QString &, bool);
    void adjustMonitorSize();
    void zoneUpdated(QPoint);
    void saveZone(Render *, QPoint, DocClipBase *);
    /** @brief  Editing transitions / effects over the monitor requires the renderer to send frames as QImage.
     *      This causes a major slowdown, so we only enable it if required */
    void requestFrameForAnalysis(bool);
};

#endif
