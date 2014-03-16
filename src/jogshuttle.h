/***************************************************************************
 *   Copyright (C) 2008 by Jean-Baptiste Mardelle (jb@kdenlive.org)        *
 *   Based on code by Arendt David <admin@prnet.org>                       *
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

#ifndef SHUTTLE_H
#define SHUTTLE_H

#include <QThread>
#include <QObject>
#include <QMap>

#include <media_ctrl/mediactrl.h>


class ShuttleThread : public QThread
{

public:
    virtual void run();
    void init(QObject *parent, const QString &device);
    QString device();
    void stop();

private:
    void handleEvent(const struct media_ctrl_event& ev);
    void jog(const struct media_ctrl_event& ev);
    void shuttle(const struct media_ctrl_event& ev);
    void key(const struct media_ctrl_event& ev);

    QString m_device;
    QObject *m_parent;
    volatile bool m_isRunning;
};


typedef QMap<QString, QString> DeviceMap;
typedef QMap<QString, QString>::iterator DeviceMapIter;

class JogShuttle: public QObject
{
    Q_OBJECT
public:
    explicit JogShuttle(const QString &device, QObject * parent = 0);
    ~JogShuttle();
    void stopDevice();
    void initDevice(const QString &device);
    static QString canonicalDevice(const QString& device);
    static DeviceMap enumerateDevices(const QString& devPath);
    static int keysCount(const QString& devPath);

protected:
    virtual void customEvent(QEvent * e);

private:
    ShuttleThread m_shuttleProcess;

signals:
    void jogBack();
    void jogForward();
    void shuttlePos(int);
    void button(int);
};

#endif
