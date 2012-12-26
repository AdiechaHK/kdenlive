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

/**
* @class RecMonitor
* @brief Records video via dvgrab, video4linux and recordmydesktop
* @author Jean-Baptiste Mardelle
*/

#ifndef RECMONITOR_H
#define RECMONITOR_H

#include "abstractmonitor.h"
#include "definitions.h"
#include "ui_recmonitor_ui.h"

#include <QToolBar>
#include <QTimer>
#include <QProcess>
#include <QImage>

#include <KIcon>
#include <KAction>
#include <KRestrictedLine>
#include <KDateTime>
#include <kdeversion.h>
#include <KComboBox>
#include <kcapacitybar.h>

class MonitorManager;
class MltDeviceCapture;
class AbstractRender;

class RecMonitor : public AbstractMonitor, public Ui::RecMonitor_UI
{
    Q_OBJECT

public:
    explicit RecMonitor(Kdenlive::MONITORID name, MonitorManager *manager, QWidget *parent = 0);
    virtual ~RecMonitor();

    AbstractRender *abstractRender();
    void analyseFrames(bool analyse);
    enum CAPTUREDEVICE {FIREWIRE = 0, VIDEO4LINUX = 1, SCREENGRAB = 2, BLACKMAGIC = 3};

protected:
    virtual void mousePressEvent(QMouseEvent * event);
    virtual void mouseDoubleClickEvent(QMouseEvent * event);

private:
    KDateTime m_captureTime;
    /** @brief Provide feedback about dvgrab operations */
    QLabel m_dvinfo;
    
    /** @brief Keeps a brief (max ten items) history of warning or error messages
     * 	(currently only used for BLACKMAGIC). */
    KComboBox m_logger;
    QString m_capturePath;

    KCapacityBar *m_freeSpace;
    QTimer m_spaceTimer;

    KUrl m_captureFile;
    KIcon m_playIcon;
    KIcon m_pauseIcon;

    QProcess *m_captureProcess;
    QProcess *m_displayProcess;
    bool m_isCapturing;
    /** did the user capture something ? */
    bool m_didCapture;
    bool m_isPlaying;
    QStringList m_captureArgs;
    QStringList m_displayArgs;
    QAction *m_recAction;
    QAction *m_playAction;
    QAction *m_fwdAction;
    QAction *m_rewAction;
    QAction *m_stopAction;
    QAction *m_discAction;

    MonitorManager *m_manager;
    MltDeviceCapture *m_captureDevice;
    VideoContainer *m_videoBox;
    QAction *m_addCapturedClip;
    QAction *m_previewSettings;
    
    bool m_analyse;
    void checkDeviceAvailability();
    QPixmap mergeSideBySide(const QPixmap& pix, const QString &txt);
    void manageCapturedFiles();
    /** @brief Build MLT producer for device, using path as profile. */
    void buildMltDevice(const QString &path);
    /** @brief Create string containing an XML playlist for v4l capture. */
    const QString getV4lXmlPlaylist(MltVideoProfile profile, bool *isXml);

private slots:
    void slotStartPreview(bool play = true);
    void slotRecord();
    void slotProcessStatus(QProcess::ProcessState status);
    void slotVideoDeviceChanged(int ix);
    void slotRewind();
    void slotForward();
    void slotDisconnect();
    //void slotStartGrab(const QRect &rect);
    void slotConfigure();
    void slotReadDvgrabInfo();
    void slotUpdateFreeSpace();
    void slotSetInfoMessage(const QString &message);
    void slotDroppedFrames(int dropped);
    /** @brief Change setting for preview while recording. */
    void slotChangeRecordingPreview(bool enable);

public slots:
    void refreshRecMonitor(bool visible);
    void slotPlay();
    void stop();
    void start();
    void slotStopCapture();
    void slotUpdateCaptureFolder(const QString &currentProjectFolder);
    void slotMouseSeek(int eventDelta, bool fast);
    void slotSwitchFullScreen();

signals:
    void renderPosition(int);
    void durationChanged(int);
    void addProjectClip(KUrl);
    void addProjectClipList(KUrl::List);
    void showConfigDialog(int, int);
};

#endif
