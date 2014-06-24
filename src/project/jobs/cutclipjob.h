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

#ifndef CUTCLIPJOB
#define CUTCLIPJOB

#include <QObject>

#include "abstractclipjob.h"


class CutClipJob : public AbstractClipJob
{
    Q_OBJECT

public:
    CutClipJob(ClipType cType, const QString &id, const QStringList &parameters);
    virtual ~ CutClipJob();
    const QString destination() const;
    void startJob();
    stringMap cancelProperties();
    void processLogInfo();
    const QString statusMessage();
    bool isExclusive();
    
private:
    QString m_dest;
    QString m_src;
    QString m_start;
    QString m_end;
    QString m_cutExtraParams;
    int m_jobDuration;   
};

#endif
