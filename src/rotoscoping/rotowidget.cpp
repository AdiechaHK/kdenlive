/***************************************************************************
 *   Copyright (C) 2011 by Till Theato (root@ttill.de)                     *
 *   This file is part of Kdenlive (www.kdenlive.org).                     *
 *                                                                         *
 *   Kdenlive is free software: you can redistribute it and/or modify      *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation, either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   Kdenlive is distributed in the hope that it will be useful,           *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with Kdenlive.  If not, see <http://www.gnu.org/licenses/>.     *
 ***************************************************************************/

#include "rotowidget.h"
#include "monitor/monitor.h"
#include "renderer.h"
#include "monitor/monitorscene.h"
#include "widgets/monitoreditwidget.h"
#include "onmonitoritems/rotoscoping/bpointitem.h"
#include "onmonitoritems/rotoscoping/splineitem.h"
#include "simplekeyframes/simplekeyframewidget.h"
#include "kdenlivesettings.h"

#include <mlt++/Mlt.h>

#include <math.h>

#include <qjson/parser.h>
#include <qjson/serializer.h>

#include <QVBoxLayout>

/** @brief Listener for "tracking-finished" event in MLT rotoscoping filter. */
void tracking_finished(mlt_service *owner, RotoWidget *self, char *data)
{
    Q_UNUSED(owner)

    if (self)
        self->setSpline(QString(data));
}

RotoWidget::RotoWidget(const QString &data, Monitor *monitor, const ItemInfo &info, const Timecode &t, QWidget* parent) :
        QWidget(parent),
        m_monitor(monitor),
        m_in(info.cropStart.frames(KdenliveSettings::project_fps())),
        m_out((info.cropStart + info.cropDuration).frames(KdenliveSettings::project_fps()) - 1),
        m_filter(NULL)
{
    QVBoxLayout *l = new QVBoxLayout(this);
    m_keyframeWidget = new SimpleKeyframeWidget(t, m_out - m_in, this);
    l->addWidget(m_keyframeWidget);

    MonitorEditWidget *edit = monitor->getEffectEdit();
    m_scene = edit->getScene();
    m_scene->cleanup();

    m_item = new SplineItem(QList <BPoint>(), NULL, m_scene);

    connect(m_item, SIGNAL(changed(bool)), this, SLOT(slotUpdateData(bool)));
    connect(m_keyframeWidget, SIGNAL(positionChanged(int)), this, SLOT(slotPositionChanged(int)));
    connect(m_keyframeWidget, SIGNAL(keyframeAdded(int)), this, SLOT(slotAddKeyframe(int)));
    connect(m_keyframeWidget, SIGNAL(keyframeRemoved(int)), this, SLOT(slotRemoveKeyframe(int)));
    connect(m_keyframeWidget, SIGNAL(keyframeMoved(int,int)), this, SLOT(slotMoveKeyframe(int,int)));
    connect(m_scene, SIGNAL(addKeyframe()), this, SLOT(slotAddKeyframe()));

    setSpline(data, false);
    setupTrackingListen(info);
    m_scene->centerView();
}

RotoWidget::~RotoWidget()
{
    if (m_filter)
        mlt_events_disconnect(m_filter->get_properties(), this);

    delete m_keyframeWidget;

    m_scene->removeItem(m_item);
    delete m_item;

    if (m_monitor) {
        MonitorEditWidget *edit = m_monitor->getEffectEdit();
        edit->removeCustomControls();
        m_monitor->slotShowEffectScene(false);
    }
}

void RotoWidget::slotSyncPosition(int relTimelinePos)
{
    relTimelinePos = qBound(0, relTimelinePos, m_out);
    m_keyframeWidget->slotSetPosition(relTimelinePos, false);
    slotPositionChanged(relTimelinePos, false);
}

void RotoWidget::slotUpdateData(int pos, bool editing)
{
    Q_UNUSED(editing)

    int width = m_monitor->render->frameRenderWidth();
    int height = m_monitor->render->renderHeight();

    /*
     * use the position of the on-monitor points to create a storable list
     */
    QList <BPoint> spline = m_item->getPoints();
    QList <QVariant> vlist;
    foreach (const BPoint &point, spline) {
        QList <QVariant> pl;
        for (int i = 0; i < 3; ++i)
            pl << QVariant(QList <QVariant>() << QVariant(point[i].x() / width) << QVariant(point[i].y() / height));
        vlist << QVariant(pl);
    }

    if (m_data.canConvert(QVariant::Map)) {
        QMap <QString, QVariant> map = m_data.toMap();
        // replace or insert at position
        // we have to fill with 0s to maintain the correct order
        map[QString::number((pos < 0 ? m_keyframeWidget->getPosition() : pos) + m_in).rightJustified(log10((double)m_out) + 1, '0')] = QVariant(vlist);
        m_data = QVariant(map);
    } else {
        // timeline update is only required if the first keyframe did not exist yet
        bool update = m_data.isNull();
        m_data = QVariant(vlist);
        if (update) {
            keyframeTimelineFullUpdate();
        }
    }

    emit valueChanged();
}

void RotoWidget::slotUpdateData(bool editing)
{
    slotUpdateData(-1, editing);
}

QString RotoWidget::getSpline()
{
    QJson::Serializer serializer;
    return QString(serializer.serialize(m_data));
}

void RotoWidget::slotPositionChanged(int pos, bool seek)
{
    // do not update while the spline is being edited (points are being dragged)
    if (m_item->editing())
        return;

    m_keyframeWidget->slotSetPosition(pos, false);

    pos += m_in;

    QList <BPoint> p;

    if (m_data.canConvert(QVariant::Map)) {
        QMap <QString, QVariant> map = m_data.toMap();
        QMap <QString, QVariant>::const_iterator i = map.constBegin();
        int keyframe1, keyframe2;
        keyframe1 = keyframe2 = i.key().toInt();
        // find keyframes next to pos
        while (i.key().toInt() < pos && ++i != map.constEnd()) {
            keyframe1 = keyframe2;
            keyframe2 = i.key().toInt();
        }

        if (keyframe1 != keyframe2 && pos < keyframe2) {
            /*
             * in between two keyframes
             * -> interpolate
             */
            QList <BPoint> p1 = getPoints(keyframe1);
            QList <BPoint> p2 = getPoints(keyframe2);
            qreal relPos = (pos - keyframe1) / (qreal)(keyframe2 - keyframe1 + 1);

            // additionaly points are ignored (same behavior as MLT filter)
            int count = qMin(p1.count(), p2.count());
            for (int i = 0; i < count; ++i) {
                BPoint bp;
                for (int j = 0; j < 3; ++j) {
                    if (p1.at(i)[j] != p2.at(i)[j])
                        bp[j] = QLineF(p1.at(i)[j], p2.at(i)[j]).pointAt(relPos);
                    else
                        bp[j] = p1.at(i)[j];
                }
                p.append(bp);
            }

            m_item->setPoints(p);
            m_item->setEnabled(false);
            m_scene->setEnabled(false);
        } else {
            p = getPoints(keyframe2);
            // only update if necessary to preserve the current point selection
            if (p != m_item->getPoints())
                m_item->setPoints(p);
            m_item->setEnabled(pos == keyframe2);
            m_scene->setEnabled(pos == keyframe2);
        }
    } else {
        p = getPoints(-1);
        // only update if necessary to preserve the current point selection
        if (p != m_item->getPoints())
            m_item->setPoints(p);
        m_item->setEnabled(true);
        m_scene->setEnabled(true);
    }

    if (seek)
        emit seekToPos(pos - m_in);
}

QList <BPoint> RotoWidget::getPoints(int keyframe)
{
    int width = m_monitor->render->frameRenderWidth();
    int height = m_monitor->render->renderHeight();
    QList <BPoint> points;
    QList <QVariant> data;
    if (keyframe >= 0)
        data = m_data.toMap()[QString::number(keyframe).rightJustified(log10((double)m_out) + 1, '0')].toList();
    else
        data = m_data.toList();

    // skip tracking flag
    if (data.count() && data.at(0).canConvert(QVariant::String))
        data.removeFirst();

    foreach (const QVariant &bpoint, data) {
        QList <QVariant> l = bpoint.toList();
        BPoint p;
        for (int i = 0; i < 3; ++i)
            p[i] = QPointF(l.at(i).toList().at(0).toDouble() * width, l.at(i).toList().at(1).toDouble() * height);
        points << p;
    }
    return points;
}

void RotoWidget::slotAddKeyframe(int pos)
{
    if (!m_data.canConvert(QVariant::Map)) {
        QVariant data = m_data;
        QMap<QString, QVariant> map;
        map[QString::number(m_in).rightJustified(log10((double)m_out) + 1, '0')] = data;
        m_data = QVariant(map);
    }

    if (pos < 0)
        m_keyframeWidget->addKeyframe();

    slotUpdateData(pos);
    m_item->setEnabled(true);
    m_scene->setEnabled(true);
}

void RotoWidget::slotRemoveKeyframe(int pos)
{
    if (pos < 0)
        pos = m_keyframeWidget->getPosition();

    if (!m_data.canConvert(QVariant::Map) || m_data.toMap().count() < 2)
        return;

    QMap<QString, QVariant> map = m_data.toMap();
    map.remove(QString::number(pos + m_in).rightJustified(log10((double)m_out) + 1, '0'));
    m_data = QVariant(map);

    if (m_data.toMap().count() == 1) {
        // only one keyframe -> switch from map to list again
        m_data = m_data.toMap().begin().value();
    }

    slotPositionChanged(m_keyframeWidget->getPosition(), false);
    emit valueChanged();
}

void RotoWidget::slotMoveKeyframe(int oldPos, int newPos)
{
    if (m_data.canConvert(QVariant::Map)) {
        QMap<QString, QVariant> map = m_data.toMap();
        map[QString::number(newPos + m_in).rightJustified(log10((double)m_out) + 1, '0')] = map.take(QString::number(oldPos + m_in).rightJustified(log10((double)m_out) + 1, '0'));
        m_data = QVariant(map);
    }

    slotPositionChanged(m_keyframeWidget->getPosition(), false);
    emit valueChanged();
}

void RotoWidget::updateTimecodeFormat()
{
    m_keyframeWidget->updateTimecodeFormat();
}

void RotoWidget::keyframeTimelineFullUpdate()
{
    if (m_data.canConvert(QVariant::Map)) {
        QList <int> keyframes;
        QMap <QString, QVariant> map = m_data.toMap();
        QMap <QString, QVariant>::const_iterator i = map.constBegin();
        while (i != map.constEnd()) {
            keyframes.append(i.key().toInt() - m_in);
            ++i;
        }
        m_keyframeWidget->setKeyframes(keyframes);

        /*for (int j = 0; j < keyframes.count(); ++j) {
            // key might already be justified
            if (map.contains(QString::number(keyframes.at(j) + m_in))) {
                QVariant value = map.take(QString::number(keyframes.at(j) + m_in));
                map[QString::number(keyframes.at(j) + m_in).rightJustified(log10((double)m_out) + 1, '0')] = value;
            }
        }
        m_data = QVariant(map);*/
    } else {
        // static (only one keyframe)
        // make sure the first keyframe was already created
        if (m_data.isValid()) {
            m_keyframeWidget->setKeyframes(QList <int>() << 0);
        }
    }
}

void RotoWidget::setupTrackingListen(const ItemInfo &info)
{
    if (info.startPos < GenTime()) {
        // TODO: track effects
        return;
    }

    Mlt::Service service(m_monitor->render->getProducer()->parent().get_service());
    Mlt::Tractor tractor(service);
    Mlt::Producer trackProducer(tractor.track(tractor.count() - info.track - 1));
    Mlt::Playlist trackPlaylist((mlt_playlist) trackProducer.get_service());

    Mlt::Producer *clip = trackPlaylist.get_clip_at((int)info.startPos.frames(KdenliveSettings::project_fps()));
    if (!clip) {
        return;
    }

    int i = 0;
    Mlt::Filter *filter = clip->filter(0);
    while (filter) {
        if (strcmp(filter->get("kdenlive_id"), "rotoscoping") == 0) {
            m_filter = filter;
            filter->listen("tracking-finished", this, (mlt_listener)tracking_finished);
            break;
        }
        filter = clip->filter(++i);
    }

    delete clip;
}

void RotoWidget::setSpline(const QString &spline, bool notify)
{
    QJson::Parser parser;
    bool ok;
    m_data = parser.parse(spline.simplified().toUtf8(), &ok);
    if (!ok) {
        // :(
    }
    keyframeTimelineFullUpdate();
    slotPositionChanged(m_keyframeWidget->getPosition(), false);
    if (notify)
        emit valueChanged();
}


static QVariant interpolate(int position, int in, int out, QVariant *splineIn, QVariant *splineOut)
{
    qreal relPos = (position - in) / (qreal)(out - in + 1);
    QList<QVariant> keyframe1 = splineIn->toList();
    QList<QVariant> keyframe2 = splineOut->toList();
    QList<QVariant> keyframe;
    if (keyframe1.count() && keyframe1.at(0).canConvert(QVariant::String))
        keyframe1.removeFirst();
    if (keyframe2.count() && keyframe2.at(0).canConvert(QVariant::String))
        keyframe2.removeFirst();
    int max = qMin(keyframe1.count(), keyframe2.count());
        
    for (int i = 0; i < max; ++i) {
        QList<QVariant> p1 = keyframe1.at(i).toList();
        QList<QVariant> p2 = keyframe2.at(i).toList();
        QList<QVariant> p;
        for (int j = 0; j < 3; ++j) {
            QPointF middle = QLineF(QPointF(p1.at(j).toList().at(0).toDouble(), p1.at(j).toList().at(1).toDouble()),
                                    QPointF(p2.at(j).toList().at(0).toDouble(), p2.at(j).toList().at(1).toDouble())).pointAt(relPos);
            p.append(QVariant(QList<QVariant>() << QVariant(middle.x()) << QVariant(middle.y())));
        }
        keyframe.append(QVariant(p));
    }
    return QVariant(keyframe);
}

bool adjustRotoDuration(QString* data, int in, int out)
{
    QJson::Parser parser;
    bool ok;
    QVariant splines = parser.parse(data->toUtf8(), &ok);
    if (!ok) {
        *data = QString();
        return true;
    }

    if (!splines.canConvert(QVariant::Map))
        return false;

    QMap<QString, QVariant> newMap;
    QMap<QString, QVariant> map = splines.toMap();
    QMap<QString, QVariant>::iterator i = map.end();
    int lastPos = -1;
    QVariant last = QVariant();

    /*
     * Take care of resize from start
     */
    bool startFound = false;
    while (i-- != map.begin()) {
        if (!startFound && i.key().toInt() < in) {
            startFound = true;
            if (lastPos < 0)
                newMap[QString::number(in).rightJustified(log10((double)out) + 1, '0')] = i.value();
            else
                newMap[QString::number(in).rightJustified(log10((double)out) + 1, '0')] = interpolate(in, i.key().toInt(), lastPos, &i.value(), &last);
        }
        lastPos = i.key().toInt();
        last = i.value();
        if (startFound)
            i = map.erase(i);
    }

    /*
     * Take care of resize from end
     */
    i = map.begin();
    lastPos = -1;
    bool endFound = false;
    while (i != map.end()) {
        if (!endFound && i.key().toInt() > out) {
            endFound = true;
            if (lastPos < 0)
                newMap[QString::number(out)] = i.value();
            else
                newMap[QString::number(out)] = interpolate(out, lastPos, i.key().toInt(), &last, &i.value());
        }
        lastPos = i.key().toInt();
        last = i.value();
        if (endFound)
            i = map.erase(i);
        else
            ++i;
    }

    /*
     * Update key lengths to prevent sorting issues
     */
    i = map.begin();
    while (i != map.end()) {
        newMap[i.key().rightJustified(log10((double)out) + 1, '0', true)] = i.value();
        ++i;
    }

    QJson::Serializer serializer;
    *data = QString(serializer.serialize(QVariant(newMap)));

    if (startFound || endFound)
        return true;
    return false;
}

#include "rotowidget.moc"
