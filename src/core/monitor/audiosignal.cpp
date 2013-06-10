/***************************************************************************
 *   Copyright (C) 2010 by Marco Gittler (g.marco@freenet.de)              *
 *   Copyright (C) 2012 by Dan Dennedy (dan@dennedy.org)                   *
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

#include "audiosignal.h"

#include <QVBoxLayout>
#include <QLabel>
#include <QAction>
#include <QPainter>
#include <QDebug>
#include <QList>

#include <math.h>

AudioSignal::AudioSignal(QWidget *parent): QWidget(parent)
{
    const QFont& font = QWidget::font();
    const int fontSize = font.pointSize() - (font.pointSize() > 10? 2 : (font.pointSize() > 8? 1 : 0));
    QWidget::setFont(QFont(font.family(), fontSize));
    setBackgroundRole(QPalette::AlternateBase);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    setMinimumHeight(210);
    setMinimumWidth(fontMetrics().width("-60") + 20);
    dbscale << 5 << 0 << -5 << -10 << -15 << -20 << -25 << -30 << -35 << -40 << -50 << -60;
    setContextMenuPolicy(Qt::ActionsContextMenu);
    m_aMonitoringEnabled = new QAction(tr("Monitor Audio Signal"), this);
    m_aMonitoringEnabled->setCheckable(true);
    m_aMonitoringEnabled->setChecked(true);
    connect(m_aMonitoringEnabled, SIGNAL(toggled(bool)), this, SLOT(slotSwitchAudioMonitoring(bool)));
    connect(&m_timer,SIGNAL(timeout()),this,SLOT(slotNoAudioTimeout()));
    addAction(m_aMonitoringEnabled);
}

AudioSignal::~AudioSignal()
{
    delete m_aMonitoringEnabled;
}

bool AudioSignal::monitoringEnabled() const
{
    return m_aMonitoringEnabled->isChecked();
}

//----------------------------------------------------------------------------
// IEC standard dB scaling -- as borrowed from meterbridge (c) Steve Harris

static inline double IEC_Scale(double dB)
{
    double fScale = 1.0f;

    if (dB < -70.0f)
        fScale = 0.0f;
    else if (dB < -60.0f)
        fScale = (dB + 70.0f) * 0.0025f;
    else if (dB < -50.0f)
        fScale = (dB + 60.0f) * 0.005f + 0.025f;
    else if (dB < -40.0)
        fScale = (dB + 50.0f) * 0.0075f + 0.075f;
    else if (dB < -30.0f)
        fScale = (dB + 40.0f) * 0.015f + 0.15f;
    else if (dB < -20.0f)
        fScale = (dB + 30.0f) * 0.02f + 0.3f;
    else if (dB < -0.001f || dB > 0.001f)  /* if (dB < 0.0f) */
        fScale = (dB + 20.0f) * 0.025f + 0.5f;

    return fScale;
}

void AudioSignal::slotAudioLevels(const QVector<double>& channels)
{
    showAudio(channels);
    m_timer.start(1000);
}

void AudioSignal::slotNoAudioTimeout()
{
    peeks.fill(0);
    showAudio(QVector<double>(2, 0));
    m_timer.stop();
}

void AudioSignal::showAudio(const QVector<double> &arr)
{
    channels = arr;
    if (peeks.size() != channels.size()) {
        peeks = QVector<double>(channels.size(), 0);
        peekage = QVector<double>(channels.size(), 0);
    }
    for (int chan = 0; chan < peeks.size(); chan++)
    {
        peekage[chan] = peekage[chan] + 1;
        if (peeks[chan] < channels[chan] || peekage[chan] > 50) {
            peekage[chan] = 0;
            peeks[chan] = channels[chan];
        }
    }
    update();
}

void AudioSignal::paintEvent(QPaintEvent* /*e*/)
{
    if (!m_aMonitoringEnabled->isChecked() || !isVisible())
        return;
    QPainter p(this);
    int numchan = channels.size();
    bool horiz = width() > height();
    int dbsize = fontMetrics().width("-60") + 2;
    bool showdb;
    if (!horiz) showdb = height() > 200;
    else showdb = width() > (dbsize + 2);
    const int h = IEC_Scale(-dbscale[0]) * height() - 2;

    //valpixel=1.0 for 127, 1.0+(1/40) for 1 short oversample, 1.0+(2/40) for longer oversample
    for (int i = 0; i < numchan; ++i) {
        int maxx =  h  * channels[i];
        int xdelta = h / 42 ;
        int y2 = (showdb? width() - dbsize : width()) / numchan - 1;
        int y1 = (showdb? width() - dbsize : width()) *  i / numchan;
        int x2 = maxx >  xdelta ? xdelta - 0 : maxx - 0;
        if (horiz) {
            dbsize = 9;
            showdb = height() > dbsize;
            maxx = width() * channels[i];
            xdelta = width() / 42;
            y2 = (showdb? height() - dbsize : height() ) / numchan - 1;
            y1 = (showdb? height() - dbsize : height() ) * i/numchan;
            x2 = maxx >  xdelta ? xdelta - 1 : maxx - 1;
        }

        for (int segment = 0; segment <= 42; segment++) {
            int x1 = segment * xdelta;
            QColor sig = Qt::green;
            //value of actual painted digit
            double ival = (double) x1 / (double) xdelta / 42.0;
            if (ival > 40.0/42.0)
                sig = Qt::red;
            else if (ival > 37.0/42.0)
                sig = Qt::darkYellow;
            else if (ival > 30.0/42.0)
                sig = Qt::yellow;
            if (maxx > 0) {
                if (horiz)
                    p.fillRect(x1, y1, x2, y2, QBrush(sig, Qt::SolidPattern));
                else
                    p.fillRect(y1 + dbsize, height() - x1, y2,-x2, QBrush(sig, Qt::SolidPattern));
                maxx -= xdelta;
            }
        }
        int xp = peeks[i] * (horiz? width() : h) - 2;
        p.fillRect(horiz? xp : y1 + dbsize, horiz? y1 : height() - xdelta - xp, horiz? 3 : y2, horiz? y2 : 3,
                   QBrush(Qt::black,Qt::SolidPattern));
    }
    if (!horiz) {
        //draw db value at related pixel
        QPen pen(QPalette().dark().color());
        pen.setStyle(Qt::DotLine);
        p.setPen(pen);
        for (int l = 0; l < dbscale.size(); l++) {
            if (!horiz) {
                double xf = IEC_Scale(dbscale[l]) * h;
                if (showdb) p.drawText(0, height() - xf + 2,
                                       QString().sprintf("%s%d", dbscale[l] >= 0? "  " : "", dbscale[l]));
                p.drawLine(dbsize, height() - xf, width(), height() - xf);
            } else {
                double xf = IEC_Scale(dbscale[l]) * (double) width();
                p.drawText(xf * 40.0/42.0 - 10, height() - 2,
                           QString().sprintf("%d", dbscale[l]));
            }
        }
    }
    p.end();
}

void AudioSignal::slotSwitchAudioMonitoring(bool)
{
    emit updateAudioMonitoring();
}

#include "audiosignal.moc"
