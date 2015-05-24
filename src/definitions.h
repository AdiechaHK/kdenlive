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


#ifndef DEFINITIONS_H
#define DEFINITIONS_H

#include "gentime.h"
#include "effectslist/effectslist.h"

#include <QDebug>

#include <QTreeWidgetItem>
#include <QString>

const int MAXCLIPDURATION = 15000;

namespace Kdenlive {

enum MonitorId {
    NoMonitor,
    ClipMonitor,
    ProjectMonitor,
    RecordMonitor,
    StopMotionMonitor,
    DvdMonitor
};

const int DefaultThumbHeight = 100;
/*
const QString clipMonitor("clipMonitor");
const QString recordMonitor("recordMonitor");
const QString projectMonitor("projectMonitor");
const QString stopmotionMonitor("stopmotionMonitor");
*/

}

enum OperationType {
    None = 0,
    MoveOperation,
    ResizeStart,
    ResizeEnd,
    FadeIn,
    FadeOut,
    TransitionStart,
    TransitionEnd,
    MoveGuide,
    KeyFrame,
    Seek,
    Spacer,
    RubberSelection,
    ScrollTimeline,
    ZoomTimeline
};

enum ClipType {
    Unknown = 0,
    Audio = 1,
    Video = 2,
    AV = 3,
    Color = 4,
    Image = 5,
    Text = 6,
    SlideShow = 7,
    Virtual = 8,
    Playlist = 9
};

enum ProjectItemType {
    ProjectClipType = QTreeWidgetItem::UserType,
    ProjectFoldeType,
    ProjectSubclipType
};

enum GraphicsRectItem {
    AVWidget = 70000,
    LabelWidget,
    TransitionWidget,
    GroupWidget
};

enum ProjectTool {
    SelectTool = 0,
    RazorTool = 1,
    SpacerTool = 2
};

enum TransitionType {
    /** TRANSITIONTYPE: between 0-99: video trans, 100-199: video+audio trans, 200-299: audio trans */
    LumaTransition = 0,
    CompositeTransition = 1,
    PipTransition = 2,
    LumaFileTransition = 3,
    MixTransition = 200
};

enum MessageType {
    DefaultMessage,
    ProcessingJobMessage,
    OperationCompletedMessage,
    InformationMessage,
    ErrorMessage,
    MltError
};

enum TrackType {
    AudioTrack = 0,
    VideoTrack = 1
};

enum ClipJobStatus {
    NoJob = 0,
    JobWaiting = -1,
    JobWorking = -2,
    JobDone = -3,
    JobCrashed = -4,
    JobAborted = -5
};

class TrackInfo {

public:
    TrackType type;
    QString trackName;
    bool isMute;
    bool isBlind;
    bool isLocked;
    EffectsList effectsList;
    int duration;
    TrackInfo() :
        type(VideoTrack),
        isMute(0),
        isBlind(0),
        isLocked(0),
        duration(0) {}
};

typedef QMap<QString, QString> stringMap;
typedef QMap <int, QMap <int, QByteArray> > audioByteArray;
typedef QVector<qint16> audioShortVector;

class ItemInfo {
public:
    /** startPos is the position where the clip starts on the track */
    GenTime startPos;
    /** endPos is the duration where the clip ends on the track */
    GenTime endPos;
    /** cropStart is the position where the sub-clip starts, relative to the clip's 0 position */
    GenTime cropStart;
    /** cropDuration is the duration of the clip */
    GenTime cropDuration;
    int track;
    ItemInfo() : track(0) {}
};

class TransitionInfo {
public:
    /** startPos is the position where the clip starts on the track */
    GenTime startPos;
    /** endPos is the duration where the clip ends on the track */
    GenTime endPos;
    /** the track on which the transition is (b_track)*/
    int b_track;
    /** the track on which the transition is applied (a_track)*/
    int a_track;
    /** Does the user request for a special a_track */
    bool forceTrack;
    TransitionInfo() :
        b_track(0),
        a_track(0),
        forceTrack(0) {}
};

class MltVideoProfile {
public:
    QString path;
    QString description;
    int frame_rate_num;
    int frame_rate_den;
    int width;
    int height;
    bool progressive;
    int sample_aspect_num;
    int sample_aspect_den;
    int display_aspect_num;
    int display_aspect_den;
    int colorspace;
    MltVideoProfile();
    bool operator==(const MltVideoProfile& point) const;
    bool operator!=(const MltVideoProfile &other) const;
};

/**)
 * @class EffectInfo
 * @brief A class holding some meta info for effects widgets, like state (collapsed or not, ...)
 * @author Jean-Baptiste Mardelle
 */

class EffectInfo
{
public:
    EffectInfo();
    bool isCollapsed;
    bool groupIsCollapsed;
    int groupIndex;
    QString groupName;
    QString toString() const;
    void fromString(QString value);
};

class EffectParameter
{
public:
    EffectParameter(const QString &name, const QString &value);
    QString name()   const;
    QString value() const;
    void setValue(const QString &value);

private:
    QString m_name;
    QString m_value;
};

/** Use our own list for effect parameters so that they are not sorted in any ways, because
    some effects like sox need a precise order
*/
class EffectsParameterList: public QList < EffectParameter >
{
public:
    EffectsParameterList();
    bool hasParam(const QString &name) const;

    QString paramValue(const QString &name, const QString &defaultValue = QString()) const;
    void addParam(const QString &name, const QString &value);
    void removeParam(const QString &name);
};

class CommentedTime
{
public:
    CommentedTime();
    CommentedTime(const GenTime &time, const QString& comment, int markerType = 0);

    QString comment()   const;
    GenTime time() const;
    void    setComment(const QString &comm);
    void setMarkerType(int t);
    int markerType() const;
    static QColor markerColor(int type);

    /* Implementation of > operator; Works identically as with basic types. */
    bool operator>(CommentedTime op) const;
    /* Implementation of < operator; Works identically as with basic types. */
    bool operator<(CommentedTime op) const;
    /* Implementation of >= operator; Works identically as with basic types. */
    bool operator>=(CommentedTime op) const;
    /* Implementation of <= operator; Works identically as with basic types. */
    bool operator<=(CommentedTime op) const;
    /* Implementation of == operator; Works identically as with basic types. */
    bool operator==(CommentedTime op) const;
    /* Implementation of != operator; Works identically as with basic types. */
    bool operator!=(CommentedTime op) const;

private:
    GenTime t;
    QString c;
    int type;
};

QDebug operator << (QDebug qd, const ItemInfo &info);

#endif
