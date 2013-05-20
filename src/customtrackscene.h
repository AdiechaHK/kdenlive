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
 * @class CustomTrackScene
 * @author Jean-Baptiste Mardelle
 * @brief Holds all scene properties that need to be used by clip items.
 */

#ifndef CUSTOMTRACKSCENE_H
#define CUSTOMTRACKSCENE_H

#include <QList>
#include <QGraphicsScene>
#include <QPixmap>

#include "gentime.h"

class KdenliveDoc;
class MltVideoProfile;

enum EDITMODE { NORMALEDIT = 0 , OVERWRITEEDIT = 1 , INSERTEDIT = 2 };

class CustomTrackScene : public QGraphicsScene
{
    Q_OBJECT

public:
    explicit CustomTrackScene(KdenliveDoc *doc, QObject *parent = 0);
    ~CustomTrackScene();
    void setSnapList(const QList <GenTime>& snaps);
    GenTime previousSnapPoint(const GenTime &pos) const;
    GenTime nextSnapPoint(const GenTime &pos) const;
    double getSnapPointForPos(double pos, bool doSnap = true);
    void setScale(double scale, double vscale);
    QPointF scale() const;
    int tracksCount() const;
    MltVideoProfile profile() const;
    void setEditMode(EDITMODE mode);
    EDITMODE editMode() const;

private:
    KdenliveDoc *m_document;
    QList <GenTime> m_snapPoints;
    QPointF m_scale;
    EDITMODE m_editMode;
};

#endif

