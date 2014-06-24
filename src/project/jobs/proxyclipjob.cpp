/***************************************************************************
 *                                                                         *
 *   Copyright (C) 2011 by Jean-Baptiste Mardelle (jb@kdenlive.org)        *
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

#include "proxyclipjob.h"
#include "kdenlivesettings.h"
#include "doc/kdenlivedoc.h"
#include <QProcess>


#include <KDebug>
#include <KLocalizedString>

ProxyJob::ProxyJob(ClipType cType, const QString &id, const QStringList& parameters)
    : AbstractClipJob(PROXYJOB, cType, id, parameters),
      m_jobDuration(0),
      m_isFfmpegJob(true)
{
    m_jobStatus = JobWaiting;
    description = i18n("proxy");
    m_dest = parameters.at(0);
    m_src = parameters.at(1);
    m_exif = parameters.at(2).toInt();
    m_proxyParams = parameters.at(3);
    m_renderWidth = parameters.at(4).toInt();
    m_renderHeight = parameters.at(5).toInt();
    replaceClip = true;
}

void ProxyJob::startJob()
{
    // Special case: playlist clips (.mlt or .kdenlive project files)
    m_jobDuration = 0;
    if (clipType == Playlist) {
        // change FFmpeg params to MLT format
        m_isFfmpegJob = false;
        QStringList mltParameters;
        mltParameters << m_src;
        mltParameters << QLatin1String("-consumer") << QLatin1String("avformat:") + m_dest;
        QStringList params = m_proxyParams.split(QLatin1Char('-'), QString::SkipEmptyParts);

        foreach(const QString &s, params) {
            QString t = s.simplified();
            if (t.count(QLatin1Char(' ')) == 0) {
                t.append(QLatin1String("=1"));
            }
            else t.replace(QLatin1Char(' '), QLatin1String("="));
            mltParameters << t;
        }
        
        mltParameters.append(QString::fromLatin1("real_time=-1"));

        //TODO: currently, when rendering an xml file through melt, the display ration is lost, so we enforce it manualy
        double display_ratio;
        if (m_src.startsWith(QLatin1String("consumer:"))) display_ratio = KdenliveDoc::getDisplayRatio(m_src.section(QLatin1String(":"), 1));
        else display_ratio = KdenliveDoc::getDisplayRatio(m_src);
        mltParameters << QLatin1String("aspect=") + QLocale().toString(display_ratio);

        // Ask for progress reporting
        mltParameters << QLatin1String("progress=1");

        m_jobProcess = new QProcess;
        m_jobProcess->setProcessChannelMode(QProcess::MergedChannels);
        m_jobProcess->start(KdenliveSettings::rendererpath(), mltParameters);
        m_jobProcess->waitForStarted();
    }
    else if (clipType == Image) {
        m_isFfmpegJob = false;
        // Image proxy
        QImage i(m_src);
        if (i.isNull()) {
            m_errorMessage.append(i18n("Cannot load image %1.", m_src));
            setStatus(JobCrashed);
            return;
        }

        QImage proxy;
        // Images are scaled to profile size.
        //TODO: Make it be configurable?
        if (i.width() > i.height()) proxy = i.scaledToWidth(m_renderWidth);
        else proxy = i.scaledToHeight(m_renderHeight);
        if (m_exif > 1) {
            // Rotate image according to exif data
            QImage processed;
            QMatrix matrix;

            switch ( m_exif ) {
            case 2:
                matrix.scale( -1, 1 );
                break;
            case 3:
                matrix.rotate( 180 );
                break;
            case 4:
                matrix.scale( 1, -1 );
                break;
            case 5:
                matrix.rotate( 270 );
                matrix.scale( -1, 1 );
                break;
            case 6:
                matrix.rotate( 90 );
                break;
            case 7:
                matrix.rotate( 90 );
                matrix.scale( -1, 1 );
                break;
            case 8:
                matrix.rotate( 270 );
                break;
            }
            processed = proxy.transformed( matrix );
            processed.save(m_dest);
        } else {
            proxy.save(m_dest);
        }
        setStatus(JobDone);
        return;
    } else {
        m_isFfmpegJob = true;
        QStringList parameters;
        parameters << QLatin1String("-i") << m_src;
        QString params = m_proxyParams;
        foreach(const QString &s, params.split(QLatin1Char(' ')))
            parameters << s;

        // Make sure we don't block when proxy file already exists
        parameters << QLatin1String("-y");
        parameters << m_dest;
        m_jobProcess = new QProcess;
        m_jobProcess->setProcessChannelMode(QProcess::MergedChannels);
        m_jobProcess->start(KdenliveSettings::ffmpegpath(), parameters, QIODevice::ReadOnly);
        m_jobProcess->waitForStarted();
    }
    while (m_jobProcess->state() != QProcess::NotRunning) {
        processLogInfo();
        if (m_jobStatus == JobAborted) {
            emit cancelRunningJob(m_clipId, cancelProperties());
            m_jobProcess->close();
            m_jobProcess->waitForFinished();
            QFile::remove(m_dest);
        }
        m_jobProcess->waitForFinished(400);
    }
    
    if (m_jobStatus != JobAborted) {
        int result = m_jobProcess->exitStatus();
        if (result == QProcess::NormalExit) {
            if (QFileInfo(m_dest).size() == 0) {
                // File was not created
                processLogInfo();
                m_errorMessage.append(i18n("Failed to create proxy clip."));
                setStatus(JobCrashed);
            }
            else setStatus(JobDone);
        }
        else if (result == QProcess::CrashExit) {
            // Proxy process crashed
            QFile::remove(m_dest);
            setStatus(JobCrashed);
        }
    }
    
    delete m_jobProcess;
    return;
}

void ProxyJob::processLogInfo()
{
    if (!m_jobProcess || m_jobStatus == JobAborted) return;
    QString log = QString::fromUtf8(m_jobProcess->readAll());
    if (!log.isEmpty())
        m_logDetails.append(log + QLatin1Char('\n'));
    else
        return;

    int progress;
    if (m_isFfmpegJob) {
        // Parse FFmpeg output
        if (m_jobDuration == 0) {
            if (log.contains(QLatin1String("Duration:"))) {
                QString data = log.section(QLatin1String("Duration:"), 1, 1).section(QLatin1Char(','), 0, 0).simplified();
                QStringList numbers = data.split(QLatin1Char(':'));
                m_jobDuration = (int) (numbers.at(0).toInt() * 3600 + numbers.at(1).toInt() * 60 + numbers.at(2).toDouble());
            }
        }
        else if (log.contains(QLatin1String("time="))) {
            QString time = log.section(QLatin1String("time="), 1, 1).simplified().section(QLatin1Char(' '), 0, 0);
            if (time.contains(QLatin1Char(':'))) {
                QStringList numbers = time.split(QLatin1Char(':'));
                progress = numbers.at(0).toInt() * 3600 + numbers.at(1).toInt() * 60 + numbers.at(2).toDouble();
            }
            else progress = (int) time.toDouble();
            emit jobProgress(m_clipId, (int) (100.0 * progress / m_jobDuration), jobType);
        }
    }
    else {
        // Parse MLT output
        if (log.contains(QLatin1String("percentage:"))) {
            progress = log.section(QLatin1String("percentage:"), 1).simplified().section(QLatin1Char(' '), 0, 0).toInt();
            emit jobProgress(m_clipId, progress, jobType);
        }
    }
}

ProxyJob::~ProxyJob()
{
}

const QString ProxyJob::destination() const
{
    return m_dest;
}

stringMap ProxyJob::cancelProperties()
{
    QMap <QString, QString> props;
    props.insert(QLatin1String("proxy"), QLatin1String("-"));
    return props;
}

const QString ProxyJob::statusMessage()
{
    QString statusInfo;
    switch (m_jobStatus) {
    case JobWorking:
        statusInfo = i18n("Creating proxy");
        break;
    case JobWaiting:
        statusInfo = i18n("Waiting - proxy");
        break;
    default:
        break;
    }
    return statusInfo;
}


#include "proxyclipjob.moc"
