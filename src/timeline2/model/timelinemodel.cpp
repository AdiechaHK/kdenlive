/***************************************************************************
 *   Copyright (C) 2017 by Nicolas Carion                                  *
 *   This file is part of Kdenlive. See www.kdenlive.org.                  *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) version 3 or any later version accepted by the       *
 *   membership of KDE e.V. (or its successor approved  by the membership  *
 *   of KDE e.V.), which shall act as a proxy defined in Section 14 of     *
 *   version 3 of the license.                                             *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 ***************************************************************************/

#include "timelinemodel.hpp"
#include "assets/model/assetparametermodel.hpp"
#include "bin/projectclip.h"
#include "bin/projectitemmodel.h"
#include "clipmodel.hpp"
#include "compositionmodel.hpp"
#include "core.h"
#include "doc/docundostack.hpp"
#include "effects/effectsrepository.hpp"
#include "groupsmodel.hpp"
#include "kdenlivesettings.h"
#include "logger.hpp"
#include "snapmodel.hpp"
#include "timelinefunctions.hpp"
#include "trackmodel.hpp"

#include <QDebug>
#include <QModelIndex>
#include <klocalizedstring.h>
#include <mlt++/MltConsumer.h>
#include <mlt++/MltField.h>
#include <mlt++/MltProfile.h>
#include <mlt++/MltTractor.h>
#include <mlt++/MltTransition.h>
#include <queue>

#include "macros.hpp"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#pragma GCC diagnostic ignored "-Wfloat-equal"
#pragma GCC diagnostic ignored "-Wshadow"
#pragma GCC diagnostic ignored "-Wpedantic"
#include <rttr/registration>
#pragma GCC diagnostic pop
RTTR_REGISTRATION
{
    using namespace rttr;
    registration::class_<TimelineModel>("TimelineModel")
        .method("requestClipMove", select_overload<bool(int, int, int, bool, bool, bool)>(&TimelineModel::requestClipMove))(
            parameter_names("clipId", "trackId", "position", "updateView", "logUndo", "invalidateTimeline"))
        .method("requestCompositionMove", select_overload<bool(int, int, int, bool, bool)>(&TimelineModel::requestCompositionMove))(
            parameter_names("compoId", "trackId", "position", "updateView", "logUndo"))
        .method("requestClipInsertion", select_overload<bool(const QString &, int, int, int &, bool, bool, bool)>(&TimelineModel::requestClipInsertion))(
            parameter_names("binClipId", "trackId", "position", "id", "logUndo", "refreshView", "useTargets"))
        .method("requestItemDeletion", select_overload<bool(int, bool)>(&TimelineModel::requestItemDeletion))(parameter_names("clipId", "logUndo"))
        .method("requestGroupMove", select_overload<bool(int, int, int, int, bool, bool)>(&TimelineModel::requestGroupMove))(
            parameter_names("clipId", "groupId", "delta_track", "delta_pos", "updateView", "logUndo"))
        .method("requestGroupDeletion", select_overload<bool(int, bool)>(&TimelineModel::requestGroupDeletion))(parameter_names("clipId", "logUndo"))
        .method("requestItemResize", select_overload<int(int, int, bool, bool, int, bool)>(&TimelineModel::requestItemResize))(
            parameter_names("itemId", "size", "right", "logUndo", "snapDistance", "allowSingleResize"))
        .method("requestClipsGroup", select_overload<int(const std::unordered_set<int> &, bool, GroupType)>(&TimelineModel::requestClipsGroup))(
            parameter_names("itemIds", "logUndo", "type"))
        .method("requestClipUngroup", select_overload<bool(int, bool)>(&TimelineModel::requestClipUngroup))(parameter_names("itemId", "logUndo"))
        .method("requestClipsUngroup", &TimelineModel::requestClipsUngroup)(parameter_names("itemIds", "logUndo"))
        .method("requestTrackInsertion", select_overload<bool(int, int &, const QString &, bool)>(&TimelineModel::requestTrackInsertion))(
            parameter_names("pos", "id", "trackName", "audioTrack"))
        .method("requestTrackDeletion", select_overload<bool(int)>(&TimelineModel::requestTrackDeletion))(parameter_names("trackId"));
}

int TimelineModel::next_id = 0;
int TimelineModel::seekDuration = 30000;

TimelineModel::TimelineModel(Mlt::Profile *profile, std::weak_ptr<DocUndoStack> undo_stack)
    : QAbstractItemModel_shared_from_this()
    , m_tractor(new Mlt::Tractor(*profile))
    , m_snaps(new SnapModel())
    , m_undoStack(std::move(undo_stack))
    , m_profile(profile)
    , m_blackClip(new Mlt::Producer(*profile, "color:black"))
    , m_lock(QReadWriteLock::Recursive)
    , m_timelineEffectsEnabled(true)
    , m_id(getNextId())
    , m_temporarySelectionGroup(-1)
    , m_overlayTrackCount(-1)
    , m_audioTarget(-1)
    , m_videoTarget(-1)
    , m_editMode(TimelineMode::NormalEdit)
    , m_blockRefresh(false)
{
    // Create black background track
    m_blackClip->set("id", "black_track");
    m_blackClip->set("mlt_type", "producer");
    m_blackClip->set("aspect_ratio", 1);
    m_blackClip->set("length", INT_MAX);
    m_blackClip->set("set.test_audio", 0);
    m_blackClip->set("length", INT_MAX);
    m_blackClip->set_in_and_out(0, TimelineModel::seekDuration);
    m_tractor->insert_track(*m_blackClip, 0);

    TRACE_CONSTR(this);
}

TimelineModel::~TimelineModel()
{
    std::vector<int> all_ids;
    for (auto tracks : m_iteratorTable) {
        all_ids.push_back(tracks.first);
    }
    for (auto tracks : all_ids) {
        deregisterTrack_lambda(tracks, false)();
    }
    for (const auto &clip : m_allClips) {
        clip.second->deregisterClipToBin();
    }
}

int TimelineModel::getTracksCount() const
{
    READ_LOCK();
    int count = m_tractor->count();
    if (m_overlayTrackCount > -1) {
        count -= m_overlayTrackCount;
    }
    Q_ASSERT(count >= 0);
    // don't count the black background track
    Q_ASSERT(count - 1 == static_cast<int>(m_allTracks.size()));
    return count - 1;
}

int TimelineModel::getTrackIndexFromPosition(int pos) const
{
    Q_ASSERT(pos >= 0 && pos < (int)m_allTracks.size());
    READ_LOCK();
    auto it = m_allTracks.begin();
    while (pos > 0) {
        it++;
        pos--;
    }
    return (*it)->getId();
}

int TimelineModel::getClipsCount() const
{
    READ_LOCK();
    int size = int(m_allClips.size());
    return size;
}

int TimelineModel::getCompositionsCount() const
{
    READ_LOCK();
    int size = int(m_allCompositions.size());
    return size;
}

int TimelineModel::getClipTrackId(int clipId) const
{
    READ_LOCK();
    Q_ASSERT(m_allClips.count(clipId) > 0);
    const auto clip = m_allClips.at(clipId);
    return clip->getCurrentTrackId();
}

int TimelineModel::getCompositionTrackId(int compoId) const
{
    Q_ASSERT(m_allCompositions.count(compoId) > 0);
    const auto trans = m_allCompositions.at(compoId);
    return trans->getCurrentTrackId();
}

int TimelineModel::getItemTrackId(int itemId) const
{
    READ_LOCK();
    Q_ASSERT(isClip(itemId) || isComposition(itemId));
    if (isComposition(itemId)) {
        return getCompositionTrackId(itemId);
    }
    return getClipTrackId(itemId);
}

int TimelineModel::getClipPosition(int clipId) const
{
    READ_LOCK();
    Q_ASSERT(m_allClips.count(clipId) > 0);
    const auto clip = m_allClips.at(clipId);
    int pos = clip->getPosition();
    return pos;
}

double TimelineModel::getClipSpeed(int clipId) const
{
    READ_LOCK();
    Q_ASSERT(m_allClips.count(clipId) > 0);
    return m_allClips.at(clipId)->getSpeed();
}

int TimelineModel::getClipSplitPartner(int clipId) const
{
    READ_LOCK();
    Q_ASSERT(m_allClips.count(clipId) > 0);
    return m_groups->getSplitPartner(clipId);
}

int TimelineModel::getClipIn(int clipId) const
{
    READ_LOCK();
    Q_ASSERT(m_allClips.count(clipId) > 0);
    const auto clip = m_allClips.at(clipId);
    return clip->getIn();
}

PlaylistState::ClipState TimelineModel::getClipState(int clipId) const
{
    READ_LOCK();
    Q_ASSERT(m_allClips.count(clipId) > 0);
    const auto clip = m_allClips.at(clipId);
    return clip->clipState();
}

const QString TimelineModel::getClipBinId(int clipId) const
{
    READ_LOCK();
    Q_ASSERT(m_allClips.count(clipId) > 0);
    const auto clip = m_allClips.at(clipId);
    QString id = clip->binId();
    return id;
}

int TimelineModel::getClipPlaytime(int clipId) const
{
    READ_LOCK();
    Q_ASSERT(isClip(clipId));
    const auto clip = m_allClips.at(clipId);
    int playtime = clip->getPlaytime();
    return playtime;
}

QSize TimelineModel::getClipFrameSize(int clipId) const
{
    READ_LOCK();
    Q_ASSERT(isClip(clipId));
    const auto clip = m_allClips.at(clipId);
    return clip->getFrameSize();
}

int TimelineModel::getTrackClipsCount(int trackId) const
{
    READ_LOCK();
    Q_ASSERT(isTrack(trackId));
    int count = getTrackById_const(trackId)->getClipsCount();
    return count;
}

int TimelineModel::getClipByPosition(int trackId, int position) const
{
    READ_LOCK();
    Q_ASSERT(isTrack(trackId));
    return getTrackById_const(trackId)->getClipByPosition(position);
}

int TimelineModel::getCompositionByPosition(int trackId, int position) const
{
    READ_LOCK();
    Q_ASSERT(isTrack(trackId));
    return getTrackById_const(trackId)->getCompositionByPosition(position);
}

int TimelineModel::getTrackPosition(int trackId) const
{
    READ_LOCK();
    Q_ASSERT(isTrack(trackId));
    auto it = m_allTracks.begin();
    int pos = (int)std::distance(it, (decltype(it))m_iteratorTable.at(trackId));
    return pos;
}

int TimelineModel::getTrackMltIndex(int trackId) const
{
    READ_LOCK();
    // Because of the black track that we insert in first position, the mlt index is the position + 1
    return getTrackPosition(trackId) + 1;
}

int TimelineModel::getTrackSortValue(int trackId, bool separated) const
{
    if (separated) {
        return getTrackPosition(trackId) + 1;
    }
    auto it = m_allTracks.end();
    int aCount = 0;
    int vCount = 0;
    bool isAudio = false;
    int trackPos = 0;
    while (it != m_allTracks.begin()) {
        --it;
        bool audioTrack = (*it)->isAudioTrack();
        if (audioTrack) {
            aCount++;
        } else {
            vCount++;
        }
        if (trackId == (*it)->getId()) {
            isAudio = audioTrack;
            trackPos = audioTrack ? aCount : vCount;
        }
    }
    int trackDiff = aCount - vCount;
    if (trackDiff > 0) {
        // more audio tracks
        if (!isAudio) {
            trackPos -= trackDiff;
        } else if (trackPos > vCount) {
            return -trackPos;
        }
    }
    return isAudio ? ((aCount * trackPos) - 1) : (vCount + 1 - trackPos) * 2;
}

QList<int> TimelineModel::getLowerTracksId(int trackId, TrackType type) const
{
    READ_LOCK();
    Q_ASSERT(isTrack(trackId));
    QList<int> results;
    auto it = m_iteratorTable.at(trackId);
    while (it != m_allTracks.begin()) {
        --it;
        if (type == TrackType::AnyTrack) {
            results << (*it)->getId();
            continue;
        }
        bool audioTrack = (*it)->isAudioTrack();
        if (type == TrackType::AudioTrack && audioTrack) {
            results << (*it)->getId();
        } else if (type == TrackType::VideoTrack && !audioTrack) {
            results << (*it)->getId();
        }
    }
    return results;
}

int TimelineModel::getPreviousVideoTrackIndex(int trackId) const
{
    READ_LOCK();
    Q_ASSERT(isTrack(trackId));
    auto it = m_iteratorTable.at(trackId);
    while (it != m_allTracks.begin()) {
        --it;
        if (it != m_allTracks.begin() && !(*it)->isAudioTrack()) {
            break;
        }
    }
    return it == m_allTracks.begin() ? 0 : (*it)->getId();
}

int TimelineModel::getPreviousVideoTrackPos(int trackId) const
{
    READ_LOCK();
    Q_ASSERT(isTrack(trackId));
    auto it = m_iteratorTable.at(trackId);
    while (it != m_allTracks.begin()) {
        --it;
        if (it != m_allTracks.begin() && !(*it)->isAudioTrack()) {
            break;
        }
    }
    return it == m_allTracks.begin() ? 0 : getTrackMltIndex((*it)->getId());
}

int TimelineModel::getMirrorVideoTrackId(int trackId) const
{
    READ_LOCK();
    Q_ASSERT(isTrack(trackId));
    auto it = m_iteratorTable.at(trackId);
    if (!(*it)->isAudioTrack()) {
        // we expected an audio track...
        return -1;
    }
    int count = 0;
    if (it != m_allTracks.end()) {
        ++it;
    }
    while (it != m_allTracks.end()) {
        if ((*it)->isAudioTrack()) {
            count++;
        } else {
            if (count == 0) {
                return (*it)->getId();
            }
            count--;
        }
        ++it;
    }
    if (it != m_allTracks.end() && !(*it)->isAudioTrack() && count == 0) {
        return (*it)->getId();
    }
    return -1;
}

int TimelineModel::getMirrorTrackId(int trackId) const
{
    if (isAudioTrack(trackId)) {
        return getMirrorVideoTrackId(trackId);
    }
    return getMirrorAudioTrackId(trackId);
}

int TimelineModel::getMirrorAudioTrackId(int trackId) const
{
    READ_LOCK();
    Q_ASSERT(isTrack(trackId));
    auto it = m_iteratorTable.at(trackId);
    if ((*it)->isAudioTrack()) {
        // we expected a video track...
        return -1;
    }
    int count = 0;
    if (it != m_allTracks.begin()) {
        --it;
    }
    while (it != m_allTracks.begin()) {
        if (!(*it)->isAudioTrack()) {
            count++;
        } else {
            if (count == 0) {
                return (*it)->getId();
            }
            count--;
        }
        --it;
    }
    if ((*it)->isAudioTrack() && count == 0) {
        return (*it)->getId();
    }
    return -1;
}

void TimelineModel::setEditMode(TimelineMode::EditMode mode)
{
    m_editMode = mode;
}

bool TimelineModel::normalEdit() const
{
    return m_editMode == TimelineMode::NormalEdit;
}

bool TimelineModel::fakeClipMove(int clipId, int trackId, int position, bool updateView, bool invalidateTimeline, Fun &undo, Fun &redo)
{
    Q_UNUSED(updateView);
    Q_UNUSED(invalidateTimeline);
    Q_UNUSED(undo);
    Q_UNUSED(redo);
    Q_ASSERT(isClip(clipId));
    m_allClips[clipId]->setFakePosition(position);
    bool trackChanged = false;
    if (trackId > -1) {
        if (trackId != m_allClips[clipId]->getFakeTrackId()) {
            if (getTrackById_const(trackId)->trackType() == m_allClips[clipId]->clipState()) {
                m_allClips[clipId]->setFakeTrackId(trackId);
                trackChanged = true;
            }
        }
    }
    QModelIndex modelIndex = makeClipIndexFromID(clipId);
    if (modelIndex.isValid()) {
        QVector<int> roles{FakePositionRole};
        if (trackChanged) {
            roles << FakeTrackIdRole;
        }
        notifyChange(modelIndex, modelIndex, roles);
        return true;
    }
    return false;
}

bool TimelineModel::requestClipMove(int clipId, int trackId, int position, bool updateView, bool invalidateTimeline, Fun &undo, Fun &redo)
{
    // qDebug() << "// FINAL MOVE: " << invalidateTimeline << ", UPDATE VIEW: " << updateView;
    if (trackId == -1) {
        return false;
    }
    Q_ASSERT(isClip(clipId));
    if (m_allClips[clipId]->clipState() == PlaylistState::Disabled) {
        if (getTrackById_const(trackId)->trackType() == PlaylistState::AudioOnly && !m_allClips[clipId]->canBeAudio()) {
            return false;
        }
        if (getTrackById_const(trackId)->trackType() == PlaylistState::VideoOnly && !m_allClips[clipId]->canBeVideo()) {
            return false;
        }
    } else if (getTrackById_const(trackId)->trackType() != m_allClips[clipId]->clipState()) {
        // Move not allowed (audio / video mismatch)
        qDebug() << "// CLIP MISMATCH: " << getTrackById_const(trackId)->trackType() << " == " << m_allClips[clipId]->clipState();
        return false;
    }
    std::function<bool(void)> local_undo = []() { return true; };
    std::function<bool(void)> local_redo = []() { return true; };
    bool ok = true;
    int old_trackId = getClipTrackId(clipId);
    bool notifyViewOnly = false;
    bool localUpdateView = updateView;
    // qDebug()<<"MOVING CLIP FROM: "<<old_trackId<<" == "<<trackId;
    Fun update_model = []() { return true; };
    if (old_trackId == trackId) {
        // Move on same track, simply inform the view
        localUpdateView = false;
        notifyViewOnly = true;
        update_model = [clipId, this, invalidateTimeline]() {
            QModelIndex modelIndex = makeClipIndexFromID(clipId);
            notifyChange(modelIndex, modelIndex, StartRole);
            if (invalidateTimeline) {
                int in = getClipPosition(clipId);
                emit invalidateZone(in, in + getClipPlaytime(clipId));
            }
            return true;
        };
    }
    if (old_trackId != -1) {
        if (notifyViewOnly) {
            PUSH_LAMBDA(update_model, local_undo);
        }
        ok = getTrackById(old_trackId)->requestClipDeletion(clipId, localUpdateView, invalidateTimeline, local_undo, local_redo);
        if (!ok) {
            bool undone = local_undo();
            Q_ASSERT(undone);
            return false;
        }
    }
    ok = ok & getTrackById(trackId)->requestClipInsertion(clipId, position, localUpdateView, invalidateTimeline, local_undo, local_redo);
    if (!ok) {
        qDebug() << "-------------\n\nINSERTION FAILED, REVERTING\n\n-------------------";
        bool undone = local_undo();
        Q_ASSERT(undone);
        return false;
    }
    update_model();
    if (notifyViewOnly) {
        PUSH_LAMBDA(update_model, local_redo);
    }
    UPDATE_UNDO_REDO(local_redo, local_undo, undo, redo);
    return true;
}

bool TimelineModel::requestFakeClipMove(int clipId, int trackId, int position, bool updateView, bool logUndo, bool invalidateTimeline)
{
    QWriteLocker locker(&m_lock);
    Q_ASSERT(m_allClips.count(clipId) > 0);
    if (m_allClips[clipId]->getPosition() == position && getClipTrackId(clipId) == trackId) {
        return true;
    }
    if (m_groups->isInGroup(clipId)) {
        // element is in a group.
        int groupId = m_groups->getRootId(clipId);
        int current_trackId = getClipTrackId(clipId);
        int track_pos1 = getTrackPosition(trackId);
        int track_pos2 = getTrackPosition(current_trackId);
        int delta_track = track_pos1 - track_pos2;
        int delta_pos = position - m_allClips[clipId]->getPosition();
        return requestFakeGroupMove(clipId, groupId, delta_track, delta_pos, updateView, logUndo);
    }
    std::function<bool(void)> undo = []() { return true; };
    std::function<bool(void)> redo = []() { return true; };
    bool res = fakeClipMove(clipId, trackId, position, updateView, invalidateTimeline, undo, redo);
    if (res && logUndo) {
        PUSH_UNDO(undo, redo, i18n("Move clip"));
    }
    return res;
}

bool TimelineModel::requestClipMove(int clipId, int trackId, int position, bool updateView, bool logUndo, bool invalidateTimeline)
{
    QWriteLocker locker(&m_lock);
    TRACE(clipId, trackId, position, updateView, logUndo, invalidateTimeline);
    Q_ASSERT(m_allClips.count(clipId) > 0);
    if (m_allClips[clipId]->getPosition() == position && getClipTrackId(clipId) == trackId) {
        TRACE_RES(true);
        return true;
    }
    if (m_groups->isInGroup(clipId)) {
        // element is in a group.
        int groupId = m_groups->getRootId(clipId);
        int current_trackId = getClipTrackId(clipId);
        int track_pos1 = getTrackPosition(trackId);
        int track_pos2 = getTrackPosition(current_trackId);
        int delta_track = track_pos1 - track_pos2;
        int delta_pos = position - m_allClips[clipId]->getPosition();
        return requestGroupMove(clipId, groupId, delta_track, delta_pos, updateView, logUndo);
    }
    std::function<bool(void)> undo = []() { return true; };
    std::function<bool(void)> redo = []() { return true; };
    bool res = requestClipMove(clipId, trackId, position, updateView, invalidateTimeline, undo, redo);
    if (res && logUndo) {
        PUSH_UNDO(undo, redo, i18n("Move clip"));
    }
    TRACE_RES(res);
    return res;
}

bool TimelineModel::requestClipMoveAttempt(int clipId, int trackId, int position)
{
    QWriteLocker locker(&m_lock);
    Q_ASSERT(m_allClips.count(clipId) > 0);
    if (m_allClips[clipId]->getPosition() == position && getClipTrackId(clipId) == trackId) {
        return true;
    }
    std::function<bool(void)> undo = []() { return true; };
    std::function<bool(void)> redo = []() { return true; };
    bool res = true;
    if (m_groups->isInGroup(clipId)) {
        // element is in a group.
        int groupId = m_groups->getRootId(clipId);
        int current_trackId = getClipTrackId(clipId);
        int track_pos1 = getTrackPosition(trackId);
        int track_pos2 = getTrackPosition(current_trackId);
        int delta_track = track_pos1 - track_pos2;
        int delta_pos = position - m_allClips[clipId]->getPosition();
        res = requestGroupMove(clipId, groupId, delta_track, delta_pos, false, false, undo, redo, false);
    } else {
        res = requestClipMove(clipId, trackId, position, false, false, undo, redo);
    }
    if (res) {
        undo();
    }
    return res;
}

int TimelineModel::suggestItemMove(int itemId, int trackId, int position, int cursorPosition, int snapDistance)
{
    if (isClip(itemId)) {
        return suggestClipMove(itemId, trackId, position, cursorPosition, snapDistance);
    }
    return suggestCompositionMove(itemId, trackId, position, cursorPosition, snapDistance);
}

int TimelineModel::suggestClipMove(int clipId, int trackId, int position, int cursorPosition, int snapDistance, bool allowViewUpdate)
{
    QWriteLocker locker(&m_lock);
    Q_ASSERT(isClip(clipId));
    Q_ASSERT(isTrack(trackId));
    int currentPos = getClipPosition(clipId);
    int sourceTrackId = getClipTrackId(clipId);
    if (sourceTrackId > -1 && getTrackById_const(trackId)->isAudioTrack() != getTrackById_const(sourceTrackId)->isAudioTrack()) {
        // Trying move on incompatible track type, stay on same track
        trackId = sourceTrackId;
    }
    if (currentPos == position && sourceTrackId == trackId) {
        return position;
    }
    bool after = position > currentPos;
    if (snapDistance > 0) {
        // For snapping, we must ignore all in/outs of the clips of the group being moved
        std::vector<int> ignored_pts;
        std::unordered_set<int> all_items = {clipId};
        if (m_groups->isInGroup(clipId)) {
            int groupId = m_groups->getRootId(clipId);
            all_items = m_groups->getLeaves(groupId);
        }
        for (int current_clipId : all_items) {
            if (getItemTrackId(current_clipId) != -1) {
                int in = getItemPosition(current_clipId);
                int out = in + getItemPlaytime(current_clipId);
                ignored_pts.push_back(in);
                ignored_pts.push_back(out);
            }
        }

        int snapped = requestBestSnapPos(position, m_allClips[clipId]->getPlaytime(), m_editMode == TimelineMode::NormalEdit ? ignored_pts : std::vector<int>(),
                                         cursorPosition, snapDistance);
        // qDebug() << "Starting suggestion " << clipId << position << currentPos << "snapped to " << snapped;
        if (snapped >= 0) {
            position = snapped;
        }
    }
    // we check if move is possible
    bool possible = m_editMode == TimelineMode::NormalEdit ? requestClipMove(clipId, trackId, position, true, false, false)
                                                           : requestFakeClipMove(clipId, trackId, position, true, false, false);
    /*} else {
        possible = requestClipMoveAttempt(clipId, trackId, position);
    }*/
    if (possible) {
        return position;
    }
    // Find best possible move
    if (!m_groups->isInGroup(clipId)) {
        // Try same track move
        if (trackId != sourceTrackId) {
            qDebug() << "// TESTING SAME TRACVK MOVE: " << trackId << " = " << sourceTrackId;
            trackId = sourceTrackId;
            possible = requestClipMove(clipId, trackId, position, true, false, false);
            if (!possible) {
                qDebug() << "CANNOT MOVE CLIP : " << clipId << " ON TK: " << trackId << ", AT POS: " << position;
            } else {
                return position;
            }
        }

        int blank_length = getTrackById(trackId)->getBlankSizeNearClip(clipId, after);
        qDebug() << "Found blank" << blank_length;
        if (blank_length < INT_MAX) {
            if (after) {
                position = currentPos + blank_length;
            } else {
                position = currentPos - blank_length;
            }
        } else {
            return currentPos;
        }
        possible = requestClipMove(clipId, trackId, position, true, false, false);
        return possible ? position : currentPos;
    }
    // find best pos for groups
    int groupId = m_groups->getRootId(clipId);
    std::unordered_set<int> all_items = m_groups->getLeaves(groupId);
    QMap<int, int> trackPosition;

    // First pass, sort clips by track and keep only the first / last depending on move direction
    for (int current_clipId : all_items) {
        int clipTrack = getItemTrackId(current_clipId);
        if (clipTrack == -1) {
            continue;
        }
        int in = getItemPosition(current_clipId);
        if (trackPosition.contains(clipTrack)) {
            if (after) {
                // keep only last clip position for track
                int out = in + getItemPlaytime(current_clipId);
                if (trackPosition.value(clipTrack) < out) {
                    trackPosition.insert(clipTrack, out);
                }
            } else {
                // keep only first clip position for track
                if (trackPosition.value(clipTrack) > in) {
                    trackPosition.insert(clipTrack, in);
                }
            }
        } else {
            trackPosition.insert(clipTrack, after ? in + getItemPlaytime(current_clipId) : in);
        }
    }
    // Now check space on each track
    QMapIterator<int, int> i(trackPosition);
    int blank_length = -1;
    while (i.hasNext()) {
        i.next();
        int track_space;
        if (!after) {
            // Check space before the position
            track_space = i.value() - getTrackById(i.key())->getBlankStart(i.value() - 1);
            if (blank_length == -1 || blank_length > track_space) {
                blank_length = track_space;
            }
        } else {
            // Check space after the position
            track_space = getTrackById(i.key())->getBlankEnd(i.value() + 1) - i.value() - 1;
            if (blank_length == -1 || blank_length > track_space) {
                blank_length = track_space;
            }
        }
    }
    if (blank_length != 0) {
        int updatedPos = currentPos + (after ? blank_length : -blank_length);
        possible = requestClipMove(clipId, trackId, updatedPos, true, false, false);
        if (possible) {
            return updatedPos;
        }
    }
    return currentPos;
}

int TimelineModel::suggestCompositionMove(int compoId, int trackId, int position, int cursorPosition, int snapDistance)
{
    QWriteLocker locker(&m_lock);
    Q_ASSERT(isComposition(compoId));
    Q_ASSERT(isTrack(trackId));
    int currentPos = getCompositionPosition(compoId);
    int currentTrack = getCompositionTrackId(compoId);
    if (getTrackById_const(trackId)->isAudioTrack()) {
        // Trying move on incompatible track type, stay on same track
        trackId = currentTrack;
    }
    if (currentPos == position && currentTrack == trackId) {
        return position;
    }

    if (snapDistance > 0) {
        // For snapping, we must ignore all in/outs of the clips of the group being moved
        std::vector<int> ignored_pts;
        if (m_groups->isInGroup(compoId)) {
            int groupId = m_groups->getRootId(compoId);
            auto all_items = m_groups->getLeaves(groupId);
            for (int current_compoId : all_items) {
                // TODO: fix for composition
                int in = getItemPosition(current_compoId);
                int out = in + getItemPlaytime(current_compoId);
                ignored_pts.push_back(in);
                ignored_pts.push_back(out);
            }
        } else {
            int in = currentPos;
            int out = in + getCompositionPlaytime(compoId);
            qDebug() << " * ** IGNORING SNAP PTS: " << in << "-" << out;
            ignored_pts.push_back(in);
            ignored_pts.push_back(out);
        }

        int snapped = requestBestSnapPos(position, m_allCompositions[compoId]->getPlaytime(), ignored_pts, cursorPosition, snapDistance);
        qDebug() << "Starting suggestion " << compoId << position << currentPos << "snapped to " << snapped;
        if (snapped >= 0) {
            position = snapped;
        }
    }
    // we check if move is possible
    bool possible = requestCompositionMove(compoId, trackId, position, true, false);
    qDebug() << "Original move success" << possible;
    if (possible) {
        return position;
    }
    /*bool after = position > currentPos;
    int blank_length = getTrackById(trackId)->getBlankSizeNearComposition(compoId, after);
    qDebug() << "Found blank" << blank_length;
    if (blank_length < INT_MAX) {
        if (after) {
            return currentPos + blank_length;
        }
        return currentPos - blank_length;
    }
    return position;*/
    return currentPos;
}

bool TimelineModel::requestClipCreation(const QString &binClipId, int &id, PlaylistState::ClipState state, double speed, Fun &undo, Fun &redo)
{
    qDebug() << "requestClipCreation " << binClipId;
    QString bid = binClipId;
    if (binClipId.contains(QLatin1Char('/'))) {
        bid = binClipId.section(QLatin1Char('/'), 0, 0);
    }
    if (!pCore->projectItemModel()->hasClip(bid)) {
        qDebug() << " / / / /MASTER CLIP NOT FOUND";
        return false;
    }
    std::shared_ptr<ProjectClip> master = pCore->projectItemModel()->getClipByBinID(bid);
    if (!master->isReady() || !master->isCompatible(state)) {
        qDebug()<<"// CLIP NOT READY OR NOT COMPATIBLE: "<<state;
        return false;
    }
    int clipId = TimelineModel::getNextId();
    id = clipId;
    Fun local_undo = deregisterClip_lambda(clipId);
    ClipModel::construct(shared_from_this(), bid, clipId, state, speed);
    auto clip = m_allClips[clipId];
    Fun local_redo = [clip, this, state]() {
        // We capture a shared_ptr to the clip, which means that as long as this undo object lives, the clip object is not deleted. To insert it back it is
        // sufficient to register it.
        registerClip(clip, true);
        clip->refreshProducerFromBin(state);
        return true;
    };

    if (binClipId.contains(QLatin1Char('/'))) {
        int in = binClipId.section(QLatin1Char('/'), 1, 1).toInt();
        int out = binClipId.section(QLatin1Char('/'), 2, 2).toInt();
        int initLength = m_allClips[clipId]->getPlaytime();
        bool res = true;
        if (in != 0) {
            res = requestItemResize(clipId, initLength - in, false, true, local_undo, local_redo);
        }
        res = res && requestItemResize(clipId, out - in + 1, true, true, local_undo, local_redo);
        if (!res) {
            bool undone = local_undo();
            Q_ASSERT(undone);
            return false;
        }
    }
    UPDATE_UNDO_REDO(local_redo, local_undo, undo, redo);
    return true;
}

bool TimelineModel::requestClipInsertion(const QString &binClipId, int trackId, int position, int &id, bool logUndo, bool refreshView, bool useTargets)
{
    QWriteLocker locker(&m_lock);
    TRACE(binClipId, trackId, position, id, logUndo, refreshView, useTargets);
    Fun undo = []() { return true; };
    Fun redo = []() { return true; };
    bool result = requestClipInsertion(binClipId, trackId, position, id, logUndo, refreshView, useTargets, undo, redo);
    if (result && logUndo) {
        PUSH_UNDO(undo, redo, i18n("Insert Clip"));
    }
    TRACE_RES(result);
    return result;
}

bool TimelineModel::requestClipInsertion(const QString &binClipId, int trackId, int position, int &id, bool logUndo, bool refreshView, bool useTargets,
                                         Fun &undo, Fun &redo)
{
    std::function<bool(void)> local_undo = []() { return true; };
    std::function<bool(void)> local_redo = []() { return true; };
    qDebug() << "requestClipInsertion " << binClipId << " "
             << " " << trackId << " " << position;
    bool res = false;
    ClipType::ProducerType type = ClipType::Unknown;
    QString bid = binClipId.section(QLatin1Char('/'), 0, 0);
    // dropType indicates if we want a normal drop (disabled), audio only or video only drop
    PlaylistState::ClipState dropType = PlaylistState::Disabled;
    if (bid.startsWith(QLatin1Char('A'))) {
        dropType = PlaylistState::AudioOnly;
        bid = bid.remove(0, 1);
    } else if (bid.startsWith(QLatin1Char('V'))) {
        dropType = PlaylistState::VideoOnly;
        bid = bid.remove(0, 1);
    }
    if (!pCore->projectItemModel()->hasClip(bid)) {
        return false;
    }
    std::shared_ptr<ProjectClip> master = pCore->projectItemModel()->getClipByBinID(bid);
    type = master->clipType();
    if (useTargets && m_audioTarget == -1 && m_videoTarget == -1) {
        useTargets = false;
    }
    if (dropType == PlaylistState::Disabled && (type == ClipType::AV || type == ClipType::Playlist)) {
        if (m_audioTarget >= 0 && m_videoTarget == -1 && useTargets) {
            // If audio target is set but no video target, only insert audio
            trackId = m_audioTarget;
        }
        bool audioDrop = getTrackById_const(trackId)->isAudioTrack();
        res = requestClipCreation(binClipId, id, getTrackById_const(trackId)->trackType(), 1.0, local_undo, local_redo);
        res = res && requestClipMove(id, trackId, position, refreshView, logUndo, local_undo, local_redo);
        int target_track = audioDrop ? m_videoTarget : m_audioTarget;
        qDebug() << "CLIP HAS A+V: " << master->hasAudioAndVideo();
        int mirror = getMirrorTrackId(trackId);
        bool canMirrorDrop = !useTargets && mirror > -1;
        if (res && (canMirrorDrop || target_track > -1) && master->hasAudioAndVideo()) {
            if (!useTargets) {
                target_track = mirror;
            }
            // QList<int> possibleTracks = m_audioTarget >= 0 ? QList<int>() << m_audioTarget : getLowerTracksId(trackId, TrackType::AudioTrack);
            QList<int> possibleTracks;
            qDebug() << "CREATING SPLIT " << target_track << " usetargets" << useTargets;
            if (target_track >= 0 && !getTrackById_const(target_track)->isLocked()) {
                possibleTracks << target_track;
            }
            if (possibleTracks.isEmpty()) {
                // No available audio track for splitting, abort
                pCore->displayMessage(i18n("No available track for split operation"), ErrorMessage);
                res = false;
            } else {
                std::function<bool(void)> audio_undo = []() { return true; };
                std::function<bool(void)> audio_redo = []() { return true; };
                int newId;
                res = requestClipCreation(binClipId, newId, audioDrop ? PlaylistState::VideoOnly : PlaylistState::AudioOnly, 1.0, audio_undo, audio_redo);
                if (res) {
                    bool move = false;
                    while (!move && !possibleTracks.isEmpty()) {
                        int newTrack = possibleTracks.takeFirst();
                        move = requestClipMove(newId, newTrack, position, true, false, audio_undo, audio_redo);
                    }
                    // use lazy evaluation to group only if move was successful
                    res = res && move && requestClipsGroup({id, newId}, audio_undo, audio_redo, GroupType::AVSplit);
                    if (!res || !move) {
                        pCore->displayMessage(i18n("Audio split failed: no viable track"), ErrorMessage);
                        bool undone = audio_undo();
                        Q_ASSERT(undone);
                    } else {
                        UPDATE_UNDO_REDO(audio_redo, audio_undo, local_undo, local_redo);
                    }
                } else {
                    pCore->displayMessage(i18n("Audio split failed: impossible to create audio clip"), ErrorMessage);
                    bool undone = audio_undo();
                    Q_ASSERT(undone);
                }
            }
        }
    } else {
        std::shared_ptr<ProjectClip> binClip = pCore->projectItemModel()->getClipByBinID(bid);
        if (dropType == PlaylistState::Disabled) {
            dropType = getTrackById_const(trackId)->trackType();
        } else if (dropType != getTrackById_const(trackId)->trackType()) {
            qDebug() << "// INCORRECT DRAG, ABORTING";
            return false;
        }
        QString normalisedBinId = binClipId;
        if (normalisedBinId.startsWith(QLatin1Char('A')) || normalisedBinId.startsWith(QLatin1Char('V'))) {
            normalisedBinId.remove(0, 1);
        }
        res = requestClipCreation(normalisedBinId, id, dropType, 1.0, local_undo, local_redo);
        res = res && requestClipMove(id, trackId, position, refreshView, logUndo, local_undo, local_redo);
    }
    if (!res) {
        bool undone = local_undo();
        Q_ASSERT(undone);
        id = -1;
        return false;
    }
    UPDATE_UNDO_REDO(local_redo, local_undo, undo, redo);
    return true;
}

bool TimelineModel::requestItemDeletion(int clipId, Fun &undo, Fun &redo)
{
    QWriteLocker locker(&m_lock);
    if (m_groups->isInGroup(clipId)) {
        return requestGroupDeletion(clipId, undo, redo);
    }
    return requestClipDeletion(clipId, undo, redo);
}

bool TimelineModel::requestItemDeletion(int itemId, bool logUndo)
{
    QWriteLocker locker(&m_lock);
    TRACE(itemId, logUndo);
    Q_ASSERT(isClip(itemId) || isComposition(itemId));
    if (m_groups->isInGroup(itemId)) {
        bool res = requestGroupDeletion(itemId, logUndo);
        TRACE_RES(res);
        return res;
    }
    Fun undo = []() { return true; };
    Fun redo = []() { return true; };
    bool res = false;
    QString actionLabel;
    if (isClip(itemId)) {
        actionLabel = i18n("Delete Clip");
        res = requestClipDeletion(itemId, undo, redo);
    } else {
        actionLabel = i18n("Delete Composition");
        res = requestCompositionDeletion(itemId, undo, redo);
    }
    if (res && logUndo) {
        PUSH_UNDO(undo, redo, actionLabel);
    }
    TRACE_RES(res);
    return res;
}

bool TimelineModel::requestClipDeletion(int clipId, Fun &undo, Fun &redo)
{
    int trackId = getClipTrackId(clipId);
    if (trackId != -1) {
        bool res = getTrackById(trackId)->requestClipDeletion(clipId, true, true, undo, redo);
        if (!res) {
            undo();
            return false;
        }
    }
    auto operation = deregisterClip_lambda(clipId);
    auto clip = m_allClips[clipId];
    Fun reverse = [this, clip]() {
        // We capture a shared_ptr to the clip, which means that as long as this undo object lives, the clip object is not deleted. To insert it back it is
        // sufficient to register it.
        registerClip(clip, true);
        return true;
    };
    if (operation()) {
        emit removeFromSelection(clipId);
        UPDATE_UNDO_REDO(operation, reverse, undo, redo);
        return true;
    }
    undo();
    return false;
}

bool TimelineModel::requestCompositionDeletion(int compositionId, Fun &undo, Fun &redo)
{
    int trackId = getCompositionTrackId(compositionId);
    if (trackId != -1) {
        bool res = getTrackById(trackId)->requestCompositionDeletion(compositionId, true, true, undo, redo);
        if (!res) {
            undo();
            return false;
        } else {
            unplantComposition(compositionId);
        }
    }
    Fun operation = deregisterComposition_lambda(compositionId);
    auto composition = m_allCompositions[compositionId];
    Fun reverse = [this, composition]() {
        // We capture a shared_ptr to the composition, which means that as long as this undo object lives, the composition object is not deleted. To insert it
        // back it is sufficient to register it.
        registerComposition(composition);
        return true;
    };
    if (operation()) {
        emit removeFromSelection(compositionId);
        UPDATE_UNDO_REDO(operation, reverse, undo, redo);
        return true;
    }
    undo();
    return false;
}

std::unordered_set<int> TimelineModel::getItemsInRange(int trackId, int start, int end, bool listCompositions)
{
    Q_UNUSED(listCompositions)

    std::unordered_set<int> allClips;
    if (trackId == -1) {
        for (const auto &track : m_allTracks) {
            std::unordered_set<int> clipTracks = getItemsInRange(track->getId(), start, end, listCompositions);
            allClips.insert(clipTracks.begin(), clipTracks.end());
        }
    } else {
        std::unordered_set<int> clipTracks = getTrackById(trackId)->getClipsInRange(start, end);
        allClips.insert(clipTracks.begin(), clipTracks.end());
        if (listCompositions) {
            std::unordered_set<int> compoTracks = getTrackById(trackId)->getCompositionsInRange(start, end);
            allClips.insert(compoTracks.begin(), compoTracks.end());
        }
    }
    return allClips;
}

bool TimelineModel::requestFakeGroupMove(int clipId, int groupId, int delta_track, int delta_pos, bool updateView, bool logUndo)
{
    std::function<bool(void)> undo = []() { return true; };
    std::function<bool(void)> redo = []() { return true; };
    bool res = requestFakeGroupMove(clipId, groupId, delta_track, delta_pos, updateView, logUndo, undo, redo);
    if (res && logUndo) {
        PUSH_UNDO(undo, redo, i18n("Move group"));
    }
    return res;
}

bool TimelineModel::requestFakeGroupMove(int clipId, int groupId, int delta_track, int delta_pos, bool updateView, bool finalMove, Fun &undo, Fun &redo,
                                         bool allowViewRefresh)
{
    Q_UNUSED(updateView);
    Q_UNUSED(finalMove);
    Q_UNUSED(undo);
    Q_UNUSED(redo);
    Q_UNUSED(allowViewRefresh);
    QWriteLocker locker(&m_lock);
    Q_ASSERT(m_allGroups.count(groupId) > 0);
    bool ok = true;
    auto all_items = m_groups->getLeaves(groupId);
    Q_ASSERT(all_items.size() > 1);
    Fun local_undo = []() { return true; };
    Fun local_redo = []() { return true; };

    // Moving groups is a two stage process: first we remove the clips from the tracks, and then try to insert them back at their calculated new positions.
    // This way, we ensure that no conflict will arise with clips inside the group being moved

    Fun update_model = []() { return true; };

    // Check if there is a track move

    // First, remove clips
    std::unordered_map<int, int> old_track_ids, old_position, old_forced_track;
    for (int item : all_items) {
        int old_trackId = getItemTrackId(item);
        old_track_ids[item] = old_trackId;
        if (old_trackId != -1) {
            if (isClip(item)) {
                old_position[item] = m_allClips[item]->getPosition();
            } else {
                old_position[item] = m_allCompositions[item]->getPosition();
                old_forced_track[item] = m_allCompositions[item]->getForcedTrack();
            }
        }
    }

    // Second step, calculate delta
    int audio_delta, video_delta;
    audio_delta = video_delta = delta_track;

    if (getTrackById(old_track_ids[clipId])->isAudioTrack()) {
        // Master clip is audio, so reverse delta for video clips
        video_delta = -delta_track;
    } else {
        audio_delta = -delta_track;
    }
    bool trackChanged = false;

    // Reverse sort. We need to insert from left to right to avoid confusing the view
    for (int item : all_items) {
        int current_track_id = old_track_ids[item];
        int current_track_position = getTrackPosition(current_track_id);
        int d = getTrackById(current_track_id)->isAudioTrack() ? audio_delta : video_delta;
        int target_track_position = current_track_position + d;
        if (target_track_position >= 0 && target_track_position < getTracksCount()) {
            auto it = m_allTracks.cbegin();
            std::advance(it, target_track_position);
            int target_track = (*it)->getId();
            int target_position = old_position[item] + delta_pos;
            if (isClip(item)) {
                qDebug() << "/// SETTING FAKE CLIP: " << target_track << ", POSITION: " << target_position;
                m_allClips[item]->setFakePosition(target_position);
                if (m_allClips[item]->getFakeTrackId() != target_track) {
                    trackChanged = true;
                }
                m_allClips[item]->setFakeTrackId(target_track);
            } else {
            }
        } else {
            qDebug() << "// ABORTING; MOVE TRIED ON TRACK: " << target_track_position << "..\n..\n..";
            ok = false;
        }
        if (!ok) {
            bool undone = local_undo();
            Q_ASSERT(undone);
            return false;
        }
    }
    QModelIndex modelIndex;
    QVector<int> roles{FakePositionRole};
    if (trackChanged) {
        roles << FakeTrackIdRole;
    }
    for (int item : all_items) {
        if (isClip(item)) {
            modelIndex = makeClipIndexFromID(item);
        } else {
            modelIndex = makeCompositionIndexFromID(item);
        }
        notifyChange(modelIndex, modelIndex, roles);
    }
    return true;
}

bool TimelineModel::requestGroupMove(int clipId, int groupId, int delta_track, int delta_pos, bool updateView, bool logUndo)
{
    QWriteLocker locker(&m_lock);
    TRACE(clipId, groupId, delta_track, delta_pos, updateView, logUndo);
    std::function<bool(void)> undo = []() { return true; };
    std::function<bool(void)> redo = []() { return true; };
    bool res = requestGroupMove(clipId, groupId, delta_track, delta_pos, updateView, logUndo, undo, redo);
    if (res && logUndo) {
        PUSH_UNDO(undo, redo, i18n("Move group"));
    }
    TRACE_RES(res);
    return res;
}

bool TimelineModel::requestGroupMove(int clipId, int groupId, int delta_track, int delta_pos, bool updateView, bool finalMove, Fun &undo, Fun &redo,
                                     bool allowViewRefresh)
{
    QWriteLocker locker(&m_lock);
    Q_ASSERT(m_allGroups.count(groupId) > 0);
    bool ok = true;
    auto all_items = m_groups->getLeaves(groupId);
    Q_ASSERT(all_items.size() > 1);
    Fun local_undo = []() { return true; };
    Fun local_redo = []() { return true; };

    // Sort clips. We need to delete from right to left to avoid confusing the view, and compositions from top to bottom
    std::vector<int> sorted_clips(all_items.begin(), all_items.end());
    std::sort(sorted_clips.begin(), sorted_clips.end(), [this, delta_track](int clipId1, int clipId2) {
        int p1 = isClip(clipId1) ? m_allClips[clipId1]->getPosition()
                                 : delta_track < 0 ? getTrackMltIndex(m_allCompositions[clipId1]->getCurrentTrackId())
                                                   : delta_track > 0 ? -getTrackMltIndex(m_allCompositions[clipId1]->getCurrentTrackId())
                                                                     : m_allCompositions[clipId1]->getPosition();
        int p2 = isClip(clipId2) ? m_allClips[clipId2]->getPosition()
                                 : delta_track < 0 ? getTrackMltIndex(m_allCompositions[clipId2]->getCurrentTrackId())
                                                   : delta_track > 0 ? -getTrackMltIndex(m_allCompositions[clipId2]->getCurrentTrackId())
                                                                     : m_allCompositions[clipId2]->getPosition();
        return p2 <= p1;
    });

    // Moving groups is a two stage process: first we remove the clips from the tracks, and then try to insert them back at their calculated new positions.
    // This way, we ensure that no conflict will arise with clips inside the group being moved

    Fun update_model = []() { return true; };

    // Check if there is a track move
    bool updatePositionOnly = false;
    if (delta_track == 0 && updateView) {
        updateView = false;
        allowViewRefresh = false;
        updatePositionOnly = true;
        update_model = [sorted_clips, this]() {
            QModelIndex modelIndex;
            QVector<int> roles{StartRole};
            for (int item : sorted_clips) {
                if (isClip(item)) {
                    modelIndex = makeClipIndexFromID(item);
                } else {
                    modelIndex = makeCompositionIndexFromID(item);
                }
                notifyChange(modelIndex, modelIndex, roles);
            }
            return true;
        };
    }

    // First, remove clips
    std::unordered_map<int, int> old_track_ids, old_position, old_forced_track;
    for (int item : sorted_clips) {
        int old_trackId = getItemTrackId(item);
        old_track_ids[item] = old_trackId;
        if (old_trackId != -1) {
            bool updateThisView = allowViewRefresh;
            if (isClip(item)) {
                ok = ok && getTrackById(old_trackId)->requestClipDeletion(item, updateThisView, finalMove, local_undo, local_redo);
                old_position[item] = m_allClips[item]->getPosition();
            } else {
                // ok = ok && getTrackById(old_trackId)->requestCompositionDeletion(item, updateThisView, finalMove, local_undo, local_redo);
                old_position[item] = m_allCompositions[item]->getPosition();
                old_forced_track[item] = m_allCompositions[item]->getForcedTrack();
            }
            if (!ok) {
                bool undone = local_undo();
                Q_ASSERT(undone);
                return false;
            }
        }
    }

    // Second step, reinsert clips at correct positions
    int audio_delta, video_delta;
    audio_delta = video_delta = delta_track;

    if (getTrackById(old_track_ids[clipId])->isAudioTrack()) {
        // Master clip is audio, so reverse delta for video clips
        video_delta = -delta_track;
    } else {
        audio_delta = -delta_track;
    }

    // Reverse sort. We need to insert from left to right to avoid confusing the view
    std::reverse(std::begin(sorted_clips), std::end(sorted_clips));
    for (int item : sorted_clips) {
        int current_track_id = old_track_ids[item];
        int current_track_position = getTrackPosition(current_track_id);
        int d = getTrackById(current_track_id)->isAudioTrack() ? audio_delta : video_delta;
        int target_track_position = current_track_position + d;
        bool updateThisView = allowViewRefresh;

        if (target_track_position >= 0 && target_track_position < getTracksCount()) {
            auto it = m_allTracks.cbegin();
            std::advance(it, target_track_position);
            int target_track = (*it)->getId();
            int target_position = old_position[item] + delta_pos;
            if (isClip(item)) {
                ok = ok && requestClipMove(item, target_track, target_position, updateThisView, finalMove, local_undo, local_redo);
            } else {
                ok = ok &&
                     requestCompositionMove(item, target_track, old_forced_track[item], target_position, updateThisView, finalMove, local_undo, local_redo);
            }
        } else {
            qDebug() << "// ABORTING; MOVE TRIED ON TRACK: " << target_track_position << "..\n..\n..";
            ok = false;
        }
        if (!ok) {
            bool undone = local_undo();
            Q_ASSERT(undone);
            return false;
        }
    }
    if (updatePositionOnly) {
        update_model();
        PUSH_LAMBDA(update_model, local_redo);
        PUSH_LAMBDA(update_model, local_undo);
    }
    UPDATE_UNDO_REDO(local_redo, local_undo, undo, redo);
    return true;
}

bool TimelineModel::requestGroupDeletion(int clipId, bool logUndo)
{
    QWriteLocker locker(&m_lock);
    TRACE(clipId, logUndo);
    Fun undo = []() { return true; };
    Fun redo = []() { return true; };
    bool res = requestGroupDeletion(clipId, undo, redo);
    if (res && logUndo) {
        PUSH_UNDO(undo, redo, i18n("Remove group"));
    }
    TRACE_RES(res);
    return res;
}

bool TimelineModel::requestGroupDeletion(int clipId, Fun &undo, Fun &redo)
{
    // we do a breadth first exploration of the group tree, ungroup (delete) every inner node, and then delete all the leaves.
    std::queue<int> group_queue;
    group_queue.push(m_groups->getRootId(clipId));
    std::unordered_set<int> all_items;
    std::unordered_set<int> all_compositions;
    while (!group_queue.empty()) {
        int current_group = group_queue.front();
        if (m_temporarySelectionGroup == current_group) {
            m_temporarySelectionGroup = -1;
        }
        group_queue.pop();
        Q_ASSERT(isGroup(current_group));
        auto children = m_groups->getDirectChildren(current_group);
        int one_child = -1; // we need the id on any of the indices of the elements of the group
        for (int c : children) {
            if (isClip(c)) {
                all_items.insert(c);
                one_child = c;
            } else if (isComposition(c)) {
                all_compositions.insert(c);
                one_child = c;
            } else {
                Q_ASSERT(isGroup(c));
                one_child = c;
                group_queue.push(c);
            }
        }
        if (one_child != -1) {
            bool res = m_groups->ungroupItem(one_child, undo, redo);
            if (!res) {
                undo();
                return false;
            }
        }
    }
    for (int clip : all_items) {
        bool res = requestClipDeletion(clip, undo, redo);
        if (!res) {
            undo();
            return false;
        }
    }
    for (int compo : all_compositions) {
        bool res = requestCompositionDeletion(compo, undo, redo);
        if (!res) {
            undo();
            return false;
        }
    }
    return true;
}

int TimelineModel::requestItemResize(int itemId, int size, bool right, bool logUndo, int snapDistance, bool allowSingleResize)
{
    if (logUndo) {
        qDebug() << "---------------------\n---------------------\nRESIZE W/UNDO CALLED\n++++++++++++++++\n++++";
    }
    QWriteLocker locker(&m_lock);
    TRACE(itemId, size, right, logUndo, snapDistance, allowSingleResize);
    Q_ASSERT(isClip(itemId) || isComposition(itemId));
    if (size <= 0) {
        TRACE_RES(-1);
        return -1;
    }
    int in = getItemPosition(itemId);
    int out = in + getItemPlaytime(itemId);
    if (snapDistance > 0) {
        Fun temp_undo = []() { return true; };
        Fun temp_redo = []() { return true; };
        int proposed_size = m_snaps->proposeSize(in, out, size, right, snapDistance);
        if (proposed_size > 0) {
            // only test move if proposed_size is valid
            bool success = false;
            if (isClip(itemId)) {
                success = m_allClips[itemId]->requestResize(proposed_size, right, temp_undo, temp_redo, false);
            } else {
                success = m_allCompositions[itemId]->requestResize(proposed_size, right, temp_undo, temp_redo, false);
            }
            if (success) {
                temp_undo(); // undo temp move
                size = proposed_size;
            }
        }
    }
    Fun undo = []() { return true; };
    Fun redo = []() { return true; };
    std::unordered_set<int> all_items;
    if (!allowSingleResize && m_groups->isInGroup(itemId)) {
        int groupId = m_groups->getRootId(itemId);
        auto items = m_groups->getLeaves(groupId);
        for (int id : items) {
            if (id == itemId) {
                all_items.insert(id);
                continue;
            }
            int start = getItemPosition(id);
            int end = in + getItemPlaytime(id);
            if (right) {
                if (out == end) {
                    all_items.insert(id);
                }
            } else if (start == in) {
                all_items.insert(id);
            }
        }
    } else {
        all_items.insert(itemId);
    }
    bool result = true;
    for (int id : all_items) {
        result = result && requestItemResize(id, size, right, logUndo, undo, redo);
    }
    if (!result) {
        bool undone = undo();
        Q_ASSERT(undone);
        TRACE_RES(-1);
        return -1;
    }
    if (result && logUndo) {
        if (isClip(itemId)) {
            PUSH_UNDO(undo, redo, i18n("Resize clip"));
        } else {
            PUSH_UNDO(undo, redo, i18n("Resize composition"));
        }
    }
    int res = result ? size : -1;
    TRACE_RES(res);
    return res;
}

bool TimelineModel::requestItemResize(int itemId, int size, bool right, bool logUndo, Fun &undo, Fun &redo, bool blockUndo)
{
    Fun local_undo = []() { return true; };
    Fun local_redo = []() { return true; };
    Fun update_model = [itemId, right, logUndo, this]() {
        Q_ASSERT(isClip(itemId) || isComposition(itemId));
        if (getItemTrackId(itemId) != -1) {
            qDebug() << "++++++++++\nRESIZING ITEM: " << itemId << "\n+++++++";
            QModelIndex modelIndex = isClip(itemId) ? makeClipIndexFromID(itemId) : makeCompositionIndexFromID(itemId);
            notifyChange(modelIndex, modelIndex, !right, true, logUndo);
        }
        return true;
    };
    bool result = false;
    if (isClip(itemId)) {
        result = m_allClips[itemId]->requestResize(size, right, local_undo, local_redo, logUndo);
    } else {
        Q_ASSERT(isComposition(itemId));
        result = m_allCompositions[itemId]->requestResize(size, right, local_undo, local_redo, logUndo);
    }
    if (result) {
        if (!blockUndo) {
            PUSH_LAMBDA(update_model, local_undo);
        }
        PUSH_LAMBDA(update_model, local_redo);
        update_model();
        UPDATE_UNDO_REDO(local_redo, local_undo, undo, redo);
    }
    return result;
}

int TimelineModel::requestClipsGroup(const std::unordered_set<int> &ids, bool logUndo, GroupType type)
{
    QWriteLocker locker(&m_lock);
    TRACE(ids, logUndo, type);
    Fun undo = []() { return true; };
    Fun redo = []() { return true; };
    if (m_temporarySelectionGroup > -1) {
        m_groups->destructGroupItem(m_temporarySelectionGroup);
        // We don't log in undo the selection changes
        // int firstChild = *m_groups->getDirectChildren(m_temporarySelectionGroup).begin();
        // requestClipUngroup(firstChild, undo, redo);
        m_temporarySelectionGroup = -1;
    }
    int result = requestClipsGroup(ids, undo, redo, type);
    if (type == GroupType::Selection) {
        m_temporarySelectionGroup = result;
    }
    if (result > -1 && logUndo && type != GroupType::Selection) {
        PUSH_UNDO(undo, redo, i18n("Group clips"));
    }
    TRACE_RES(result);
    return result;
}

int TimelineModel::requestClipsGroup(const std::unordered_set<int> &ids, Fun &undo, Fun &redo, GroupType type)
{
    QWriteLocker locker(&m_lock);
    for (int id : ids) {
        if (isClip(id)) {
            if (getClipTrackId(id) == -1) {
                return -1;
            }
        } else if (isComposition(id)) {
            if (getCompositionTrackId(id) == -1) {
                return -1;
            }
        } else if (!isGroup(id)) {
            return -1;
        }
    }
    if (type == GroupType::Selection && ids.size() == 1) {
        // only one element selected, no group created
        return -1;
    }
    int groupId = m_groups->groupItems(ids, undo, redo, type);
    return groupId;
}

bool TimelineModel::requestClipsUngroup(const std::unordered_set<int> &itemIds, bool logUndo)
{
    QWriteLocker locker(&m_lock);
    TRACE(itemIds, logUndo);
    Fun undo = []() { return true; };
    Fun redo = []() { return true; };
    bool result = true;
    int old_selection = m_temporarySelectionGroup;
    if (m_temporarySelectionGroup != -1) {
        // Delete selection group without undo
        Fun tmp_undo = []() { return true; };
        Fun tmp_redo = []() { return true; };
        requestClipUngroup(m_temporarySelectionGroup, tmp_undo, tmp_redo);
        m_temporarySelectionGroup = -1;
    }
    std::unordered_set<int> roots;
    for (int itemId : itemIds) {
        int root = m_groups->getRootId(itemId);
        if (root != old_selection) {
            roots.insert(root);
        }
    }
    for (int root : roots) {
        result = result && requestClipUngroup(root, undo, redo);
    }
    if (!result) {
        bool undone = undo();
        Q_ASSERT(undone);
    }
    if (result && logUndo) {
        PUSH_UNDO(undo, redo, i18n("Ungroup clips"));
    }
    TRACE_RES(result);
    return result;
}

bool TimelineModel::requestClipUngroup(int itemId, bool logUndo)
{
    QWriteLocker locker(&m_lock);
    TRACE(itemId, logUndo);
    Fun undo = []() { return true; };
    Fun redo = []() { return true; };
    bool result = true;
    if (itemId == m_temporarySelectionGroup) {
        // Delete selection group without undo
        Fun tmp_undo = []() { return true; };
        Fun tmp_redo = []() { return true; };
        requestClipUngroup(itemId, tmp_undo, tmp_redo);
        m_temporarySelectionGroup = -1;
    } else {
        result = requestClipUngroup(itemId, undo, redo);
    }
    if (result && logUndo) {
        PUSH_UNDO(undo, redo, i18n("Ungroup clips"));
    }
    TRACE_RES(result);
    return result;
}

bool TimelineModel::requestClipUngroup(int itemId, Fun &undo, Fun &redo)
{
    QWriteLocker locker(&m_lock);
    return m_groups->ungroupItem(itemId, undo, redo);
}

bool TimelineModel::requestTrackInsertion(int position, int &id, const QString &trackName, bool audioTrack)
{
    QWriteLocker locker(&m_lock);
    TRACE(position, id, trackName, audioTrack);
    Fun undo = []() { return true; };
    Fun redo = []() { return true; };
    bool result = requestTrackInsertion(position, id, trackName, audioTrack, undo, redo);
    if (result) {
        PUSH_UNDO(undo, redo, i18n("Insert Track"));
    }
    TRACE_RES(result);
    return result;
}

bool TimelineModel::requestTrackInsertion(int position, int &id, const QString &trackName, bool audioTrack, Fun &undo, Fun &redo, bool updateView)
{
    // TODO: make sure we disable overlayTrack before inserting a track
    if (position == -1) {
        position = (int)(m_allTracks.size());
    }
    if (position < 0 || position > (int)m_allTracks.size()) {
        return false;
    }
    int trackId = TimelineModel::getNextId();
    id = trackId;
    Fun local_undo = deregisterTrack_lambda(trackId, true);
    TrackModel::construct(shared_from_this(), trackId, position, trackName, audioTrack);
    auto track = getTrackById(trackId);
    Fun local_redo = [track, position, updateView, this]() {
        // We capture a shared_ptr to the track, which means that as long as this undo object lives, the track object is not deleted. To insert it back it is
        // sufficient to register it.
        registerTrack(track, position, updateView);
        return true;
    };
    UPDATE_UNDO_REDO(local_redo, local_undo, undo, redo);
    return true;
}

bool TimelineModel::requestTrackDeletion(int trackId)
{
    // TODO: make sure we disable overlayTrack before deleting a track
    QWriteLocker locker(&m_lock);
    TRACE(trackId);
    Fun undo = []() { return true; };
    Fun redo = []() { return true; };
    bool result = requestTrackDeletion(trackId, undo, redo);
    if (result) {
        if (m_videoTarget == trackId) {
            m_videoTarget = -1;
        }
        if (m_audioTarget == trackId) {
            m_audioTarget = -1;
        }
        PUSH_UNDO(undo, redo, i18n("Delete Track"));
    }
    TRACE_RES(result);
    return result;
}

bool TimelineModel::requestTrackDeletion(int trackId, Fun &undo, Fun &redo)
{
    Q_ASSERT(isTrack(trackId));
    std::vector<int> clips_to_delete;
    for (const auto &it : getTrackById(trackId)->m_allClips) {
        clips_to_delete.push_back(it.first);
    }
    Fun local_undo = []() { return true; };
    Fun local_redo = []() { return true; };
    for (int clip : clips_to_delete) {
        bool res = true;
        while (res && m_groups->isInGroup(clip)) {
            res = requestClipUngroup(clip, local_undo, local_redo);
        }
        if (res) {
            res = requestClipDeletion(clip, local_undo, local_redo);
        }
        if (!res) {
            bool u = local_undo();
            Q_ASSERT(u);
            return false;
        }
    }
    int old_position = getTrackPosition(trackId);
    auto operation = deregisterTrack_lambda(trackId, true);
    std::shared_ptr<TrackModel> track = getTrackById(trackId);
    Fun reverse = [this, track, old_position]() {
        // We capture a shared_ptr to the track, which means that as long as this undo object lives, the track object is not deleted. To insert it back it is
        // sufficient to register it.
        registerTrack(track, old_position);
        return true;
    };
    if (operation()) {
        UPDATE_UNDO_REDO(operation, reverse, local_undo, local_redo);
        UPDATE_UNDO_REDO(local_redo, local_undo, undo, redo);
        return true;
    }
    local_undo();
    return false;
}

void TimelineModel::registerTrack(std::shared_ptr<TrackModel> track, int pos, bool doInsert, bool reloadView)
{
    // qDebug() << "REGISTER TRACK" << track->getId() << pos;
    int id = track->getId();
    if (pos == -1) {
        pos = static_cast<int>(m_allTracks.size());
    }
    Q_ASSERT(pos >= 0);
    Q_ASSERT(pos <= static_cast<int>(m_allTracks.size()));

    // effective insertion (MLT operation), add 1 to account for black background track
    if (doInsert) {
        int error = m_tractor->insert_track(*track, pos + 1);
        Q_ASSERT(error == 0); // we might need better error handling...
    }

    // we now insert in the list
    auto posIt = m_allTracks.begin();
    std::advance(posIt, pos);
    auto it = m_allTracks.insert(posIt, std::move(track));
    // it now contains the iterator to the inserted element, we store it
    Q_ASSERT(m_iteratorTable.count(id) == 0); // check that id is not used (shouldn't happen)
    m_iteratorTable[id] = it;
    if (reloadView) {
        // don't reload view on each track load on project opening
        _resetView();
    }
}

void TimelineModel::registerClip(const std::shared_ptr<ClipModel> &clip, bool registerProducer)
{
    int id = clip->getId();
    qDebug() << " // /REQUEST TL CLP REGSTR: " << id << "\n--------\nCLIPS COUNT: " << m_allClips.size();
    Q_ASSERT(m_allClips.count(id) == 0);
    m_allClips[id] = clip;
    clip->registerClipToBin(clip->getProducer(), registerProducer);
    m_groups->createGroupItem(id);
    clip->setTimelineEffectsEnabled(m_timelineEffectsEnabled);
}

void TimelineModel::registerGroup(int groupId)
{
    Q_ASSERT(m_allGroups.count(groupId) == 0);
    m_allGroups.insert(groupId);
}

Fun TimelineModel::deregisterTrack_lambda(int id, bool updateView)
{
    return [this, id, updateView]() {
        // qDebug() << "DEREGISTER TRACK" << id;
        auto it = m_iteratorTable[id];                        // iterator to the element
        int index = getTrackPosition(id);                     // compute index in list
        m_tractor->remove_track(static_cast<int>(index + 1)); // melt operation, add 1 to account for black background track
        // send update to the model
        m_allTracks.erase(it);     // actual deletion of object
        m_iteratorTable.erase(id); // clean table
        if (updateView) {
            _resetView();
        }
        return true;
    };
}

Fun TimelineModel::deregisterClip_lambda(int clipId)
{
    return [this, clipId]() {
        // qDebug() << " // /REQUEST TL CLP DELETION: " << clipId << "\n--------\nCLIPS COUNT: " << m_allClips.size();
        clearAssetView(clipId);
        Q_ASSERT(m_allClips.count(clipId) > 0);
        Q_ASSERT(getClipTrackId(clipId) == -1); // clip must be deleted from its track at this point
        Q_ASSERT(!m_groups->isInGroup(clipId)); // clip must be ungrouped at this point
        auto clip = m_allClips[clipId];
        m_allClips.erase(clipId);
        clip->deregisterClipToBin();
        m_groups->destructGroupItem(clipId);
        return true;
    };
}

void TimelineModel::deregisterGroup(int id)
{
    Q_ASSERT(m_allGroups.count(id) > 0);
    m_allGroups.erase(id);
}

std::shared_ptr<TrackModel> TimelineModel::getTrackById(int trackId)
{
    Q_ASSERT(m_iteratorTable.count(trackId) > 0);
    return *m_iteratorTable[trackId];
}

const std::shared_ptr<TrackModel> TimelineModel::getTrackById_const(int trackId) const
{
    Q_ASSERT(m_iteratorTable.count(trackId) > 0);
    return *m_iteratorTable.at(trackId);
}

bool TimelineModel::addTrackEffect(int trackId, const QString &effectId)
{
    Q_ASSERT(m_iteratorTable.count(trackId) > 0);
    if ((*m_iteratorTable.at(trackId))->addEffect(effectId) == false) {
        QString effectName = EffectsRepository::get()->getName(effectId);
        pCore->displayMessage(i18n("Cannot add effect %1 to selected track", effectName), InformationMessage, 500);
        return false;
    }
    return true;
}

bool TimelineModel::copyTrackEffect(int trackId, const QString &sourceId)
{
    QStringList source = sourceId.split(QLatin1Char('-'));
    Q_ASSERT(m_iteratorTable.count(trackId) > 0 && source.count() == 3);
    int itemType = source.at(0).toInt();
    int itemId = source.at(1).toInt();
    int itemRow = source.at(2).toInt();
    std::shared_ptr<EffectStackModel> effectStack = pCore->getItemEffectStack(itemType, itemId);
    if ((*m_iteratorTable.at(trackId))->copyEffect(effectStack, itemRow) == false) {
        pCore->displayMessage(i18n("Cannot paste effect to selected track"), InformationMessage, 500);
        return false;
    }
    return true;
}

std::shared_ptr<ClipModel> TimelineModel::getClipPtr(int clipId) const
{
    Q_ASSERT(m_allClips.count(clipId) > 0);
    return m_allClips.at(clipId);
}

bool TimelineModel::addClipEffect(int clipId, const QString &effectId, bool notify)
{
    Q_ASSERT(m_allClips.count(clipId) > 0);
    bool result = m_allClips.at(clipId)->addEffect(effectId);
    if (!result && notify) {
        QString effectName = EffectsRepository::get()->getName(effectId);
        pCore->displayMessage(i18n("Cannot add effect %1 to selected clip", effectName), InformationMessage, 500);
    }
    return result;
}

bool TimelineModel::removeFade(int clipId, bool fromStart)
{
    Q_ASSERT(m_allClips.count(clipId) > 0);
    return m_allClips.at(clipId)->removeFade(fromStart);
}

std::shared_ptr<EffectStackModel> TimelineModel::getClipEffectStack(int itemId)
{
    Q_ASSERT(m_allClips.count(itemId));
    return m_allClips.at(itemId)->m_effectStack;
}

bool TimelineModel::copyClipEffect(int clipId, const QString &sourceId)
{
    QStringList source = sourceId.split(QLatin1Char('-'));
    Q_ASSERT(m_allClips.count(clipId) && source.count() == 3);
    int itemType = source.at(0).toInt();
    int itemId = source.at(1).toInt();
    int itemRow = source.at(2).toInt();
    std::shared_ptr<EffectStackModel> effectStack = pCore->getItemEffectStack(itemType, itemId);
    return m_allClips.at(clipId)->copyEffect(effectStack, itemRow);
}

bool TimelineModel::adjustEffectLength(int clipId, const QString &effectId, int duration, int initialDuration)
{
    Q_ASSERT(m_allClips.count(clipId));
    Fun undo = []() { return true; };
    Fun redo = []() { return true; };
    bool res = m_allClips.at(clipId)->adjustEffectLength(effectId, duration, initialDuration, undo, redo);
    if (res && initialDuration > 0) {
        PUSH_UNDO(undo, redo, i18n("Adjust Fade"));
    }
    return res;
}

std::shared_ptr<CompositionModel> TimelineModel::getCompositionPtr(int compoId) const
{
    Q_ASSERT(m_allCompositions.count(compoId) > 0);
    return m_allCompositions.at(compoId);
}

int TimelineModel::getNextId()
{
    return TimelineModel::next_id++;
}

bool TimelineModel::isClip(int id) const
{
    return m_allClips.count(id) > 0;
}

bool TimelineModel::isComposition(int id) const
{
    return m_allCompositions.count(id) > 0;
}

bool TimelineModel::isTrack(int id) const
{
    return m_iteratorTable.count(id) > 0;
}

bool TimelineModel::isGroup(int id) const
{
    return m_allGroups.count(id) > 0;
}

void TimelineModel::updateDuration()
{
    int current = m_blackClip->get_playtime() - TimelineModel::seekDuration;
    int duration = 0;
    for (const auto &tck : m_iteratorTable) {
        auto track = (*tck.second);
        duration = qMax(duration, track->trackDuration());
    }
    if (duration != current) {
        // update black track length
        m_blackClip->set_in_and_out(0, duration + TimelineModel::seekDuration);
        emit durationUpdated();
    }
}

int TimelineModel::duration() const
{
    return m_tractor->get_playtime() - TimelineModel::seekDuration;
}

std::unordered_set<int> TimelineModel::getGroupElements(int clipId)
{
    int groupId = m_groups->getRootId(clipId);
    return m_groups->getLeaves(groupId);
}

Mlt::Profile *TimelineModel::getProfile()
{
    return m_profile;
}

bool TimelineModel::requestReset(Fun &undo, Fun &redo)
{
    std::vector<int> all_ids;
    for (const auto &track : m_iteratorTable) {
        all_ids.push_back(track.first);
    }
    bool ok = true;
    for (int trackId : all_ids) {
        ok = ok && requestTrackDeletion(trackId, undo, redo);
    }
    return ok;
}

void TimelineModel::setUndoStack(std::weak_ptr<DocUndoStack> undo_stack)
{
    m_undoStack = std::move(undo_stack);
}

int TimelineModel::suggestSnapPoint(int pos, int snapDistance)
{
    int snapped = m_snaps->getClosestPoint(pos);
    return (qAbs(snapped - pos) < snapDistance ? snapped : pos);
}

int TimelineModel::requestBestSnapPos(int pos, int length, const std::vector<int> &pts, int cursorPosition, int snapDistance)
{
    if (!pts.empty()) {
        m_snaps->ignore(pts);
    }
    m_snaps->addPoint(cursorPosition);
    int snapped_start = m_snaps->getClosestPoint(pos);
    int snapped_end = m_snaps->getClosestPoint(pos + length);
    m_snaps->unIgnore();
    m_snaps->removePoint(cursorPosition);

    int startDiff = qAbs(pos - snapped_start);
    int endDiff = qAbs(pos + length - snapped_end);
    if (startDiff < endDiff && startDiff <= snapDistance) {
        // snap to start
        return snapped_start;
    }
    if (endDiff <= snapDistance) {
        // snap to end
        return snapped_end - length;
    }
    return -1;
}

int TimelineModel::requestNextSnapPos(int pos)
{
    return m_snaps->getNextPoint(pos);
}

int TimelineModel::requestPreviousSnapPos(int pos)
{
    return m_snaps->getPreviousPoint(pos);
}

void TimelineModel::addSnap(int pos)
{
    return m_snaps->addPoint(pos);
}

void TimelineModel::removeSnap(int pos)
{
    return m_snaps->removePoint(pos);
}

void TimelineModel::registerComposition(const std::shared_ptr<CompositionModel> &composition)
{
    int id = composition->getId();
    Q_ASSERT(m_allCompositions.count(id) == 0);
    m_allCompositions[id] = composition;
    m_groups->createGroupItem(id);
}

bool TimelineModel::requestCompositionInsertion(const QString &transitionId, int trackId, int position, int length, std::unique_ptr<Mlt::Properties> transProps,
                                                int &id, bool logUndo)
{
    QWriteLocker locker(&m_lock);
    Fun undo = []() { return true; };
    Fun redo = []() { return true; };
    bool result = requestCompositionInsertion(transitionId, trackId, -1, position, length, std::move(transProps), id, undo, redo, logUndo);
    if (result && logUndo) {
        PUSH_UNDO(undo, redo, i18n("Insert Composition"));
    }
    return result;
}

bool TimelineModel::requestCompositionInsertion(const QString &transitionId, int trackId, int compositionTrack, int position, int length,
                                                std::unique_ptr<Mlt::Properties> transProps, int &id, Fun &undo, Fun &redo, bool finalMove)
{
    qDebug() << "Inserting compo track" << trackId << "pos" << position << "length" << length;
    int compositionId = TimelineModel::getNextId();
    id = compositionId;
    Fun local_undo = deregisterComposition_lambda(compositionId);
    CompositionModel::construct(shared_from_this(), transitionId, compositionId, std::move(transProps));
    auto composition = m_allCompositions[compositionId];
    Fun local_redo = [composition, this]() {
        // We capture a shared_ptr to the composition, which means that as long as this undo object lives, the composition object is not deleted. To insert it
        // back it is sufficient to register it.
        registerComposition(composition);
        return true;
    };
    bool res = requestCompositionMove(compositionId, trackId, compositionTrack, position, true, finalMove, local_undo, local_redo);
    qDebug() << "trying to move" << trackId << "pos" << position << "success " << res;
    if (res) {
        res = requestItemResize(compositionId, length, true, true, local_undo, local_redo, true);
        qDebug() << "trying to resize" << compositionId << "length" << length << "success " << res;
    }
    if (!res) {
        bool undone = local_undo();
        Q_ASSERT(undone);
        id = -1;
        return false;
    }
    UPDATE_UNDO_REDO(local_redo, local_undo, undo, redo);
    return true;
}

Fun TimelineModel::deregisterComposition_lambda(int compoId)
{
    return [this, compoId]() {
        Q_ASSERT(m_allCompositions.count(compoId) > 0);
        Q_ASSERT(!m_groups->isInGroup(compoId)); // composition must be ungrouped at this point
        clearAssetView(compoId);
        m_allCompositions.erase(compoId);
        m_groups->destructGroupItem(compoId);
        return true;
    };
}

int TimelineModel::getCompositionPosition(int compoId) const
{
    Q_ASSERT(m_allCompositions.count(compoId) > 0);
    const auto trans = m_allCompositions.at(compoId);
    return trans->getPosition();
}

int TimelineModel::getCompositionPlaytime(int compoId) const
{
    READ_LOCK();
    Q_ASSERT(m_allCompositions.count(compoId) > 0);
    const auto trans = m_allCompositions.at(compoId);
    int playtime = trans->getPlaytime();
    return playtime;
}

int TimelineModel::getItemPosition(int itemId) const
{
    if (isClip(itemId)) {
        return getClipPosition(itemId);
    }
    return getCompositionPosition(itemId);
}

int TimelineModel::getItemPlaytime(int itemId) const
{
    if (isClip(itemId)) {
        return getClipPlaytime(itemId);
    }
    return getCompositionPlaytime(itemId);
}

int TimelineModel::getTrackCompositionsCount(int trackId) const
{
    Q_ASSERT(isTrack(trackId));
    return getTrackById_const(trackId)->getCompositionsCount();
}

bool TimelineModel::requestCompositionMove(int compoId, int trackId, int position, bool updateView, bool logUndo)
{
    QWriteLocker locker(&m_lock);
    Q_ASSERT(isComposition(compoId));
    if (m_allCompositions[compoId]->getPosition() == position && getCompositionTrackId(compoId) == trackId) {
        return true;
    }
    if (m_groups->isInGroup(compoId)) {
        // element is in a group.
        int groupId = m_groups->getRootId(compoId);
        int current_trackId = getCompositionTrackId(compoId);
        int track_pos1 = getTrackPosition(trackId);
        int track_pos2 = getTrackPosition(current_trackId);
        int delta_track = track_pos1 - track_pos2;
        int delta_pos = position - m_allCompositions[compoId]->getPosition();
        return requestGroupMove(compoId, groupId, delta_track, delta_pos, updateView, logUndo);
    }
    std::function<bool(void)> undo = []() { return true; };
    std::function<bool(void)> redo = []() { return true; };
    int min = getCompositionPosition(compoId);
    int max = min + getCompositionPlaytime(compoId);
    int tk = getCompositionTrackId(compoId);
    bool res = requestCompositionMove(compoId, trackId, m_allCompositions[compoId]->getForcedTrack(), position, updateView, logUndo, undo, redo);
    if (tk > -1) {
        min = qMin(min, getCompositionPosition(compoId));
        max = qMax(max, getCompositionPosition(compoId));
    } else {
        min = getCompositionPosition(compoId);
        max = min + getCompositionPlaytime(compoId);
    }

    if (res && logUndo) {
        PUSH_UNDO(undo, redo, i18n("Move composition"));
        checkRefresh(min, max);
    }
    return res;
}

bool TimelineModel::isAudioTrack(int trackId) const
{
    READ_LOCK();
    Q_ASSERT(isTrack(trackId));
    auto it = m_iteratorTable.at(trackId);
    return (*it)->isAudioTrack();
}

bool TimelineModel::requestCompositionMove(int compoId, int trackId, int compositionTrack, int position, bool updateView, bool finalMove, Fun &undo, Fun &redo)
{
    QWriteLocker locker(&m_lock);
    Q_ASSERT(isComposition(compoId));
    Q_ASSERT(isTrack(trackId));
    if (compositionTrack == -1 || (compositionTrack > 0 && trackId == getTrackIndexFromPosition(compositionTrack - 1))) {
        // qDebug() << "// compo track: " << trackId << ", PREVIOUS TK: " << getPreviousVideoTrackPos(trackId);
        compositionTrack = getPreviousVideoTrackPos(trackId);
    }
    if (compositionTrack == -1) {
        // it doesn't make sense to insert a composition on the last track
        qDebug() << "Move failed because of last track";
        return false;
    }
    qDebug() << "Requesting composition move" << trackId << "," << position << " ( " << compositionTrack << " / "
             << (compositionTrack > 0 ? getTrackIndexFromPosition(compositionTrack - 1) : 0);

    Fun local_undo = []() { return true; };
    Fun local_redo = []() { return true; };
    bool ok = true;
    int old_trackId = getCompositionTrackId(compoId);
    bool notifyViewOnly = false;
    Fun update_model = []() { return true; };
    if (updateView && old_trackId == trackId) {
        // Move on same track, only send view update
        updateView = false;
        notifyViewOnly = true;
        update_model = [compoId, this]() {
            QModelIndex modelIndex = makeCompositionIndexFromID(compoId);
            notifyChange(modelIndex, modelIndex, StartRole);
            return true;
        };
    }
    if (old_trackId != -1) {
        Fun delete_operation = []() { return true; };
        Fun delete_reverse = []() { return true; };
        if (old_trackId != trackId) {
            delete_operation = [this, compoId]() {
                bool res = unplantComposition(compoId);
                if (res) m_allCompositions[compoId]->setATrack(-1, -1);
                return res;
            };
            int oldAtrack = m_allCompositions[compoId]->getATrack();
            delete_reverse = [this, compoId, oldAtrack, updateView]() {
                m_allCompositions[compoId]->setATrack(oldAtrack, oldAtrack <= 0 ? -1 : getTrackIndexFromPosition(oldAtrack - 1));
                return replantCompositions(compoId, updateView);
            };
        }
        ok = delete_operation();
        if (!ok) qDebug() << "Move failed because of first delete operation";

        if (ok) {
            if (notifyViewOnly) {
                PUSH_LAMBDA(update_model, local_undo);
            }
            UPDATE_UNDO_REDO(delete_operation, delete_reverse, local_undo, local_redo);
            ok = getTrackById(old_trackId)->requestCompositionDeletion(compoId, updateView, finalMove, local_undo, local_redo);
        }
        if (!ok) {
            qDebug() << "Move failed because of first deletion request";
            bool undone = local_undo();
            Q_ASSERT(undone);
            return false;
        }
    }
    ok = getTrackById(trackId)->requestCompositionInsertion(compoId, position, updateView, finalMove, local_undo, local_redo);
    if (!ok) qDebug() << "Move failed because of second insertion request";
    if (ok) {
        Fun insert_operation = []() { return true; };
        Fun insert_reverse = []() { return true; };
        if (old_trackId != trackId) {
            insert_operation = [this, compoId, compositionTrack, updateView]() {
                qDebug() << "-------------- ATRACK ----------------\n" << compositionTrack << " = " << getTrackIndexFromPosition(compositionTrack);
                m_allCompositions[compoId]->setATrack(compositionTrack, compositionTrack <= 0 ? -1 : getTrackIndexFromPosition(compositionTrack - 1));
                return replantCompositions(compoId, updateView);
            };
            insert_reverse = [this, compoId]() {
                bool res = unplantComposition(compoId);
                if (res) m_allCompositions[compoId]->setATrack(-1, -1);
                return res;
            };
        }
        ok = insert_operation();
        if (!ok) qDebug() << "Move failed because of second insert operation";
        if (ok) {
            if (notifyViewOnly) {
                PUSH_LAMBDA(update_model, local_redo);
            }
            UPDATE_UNDO_REDO(insert_operation, insert_reverse, local_undo, local_redo);
        }
    }
    if (!ok) {
        bool undone = local_undo();
        Q_ASSERT(undone);
        return false;
    }
    update_model();
    UPDATE_UNDO_REDO(local_redo, local_undo, undo, redo);
    return true;
}

bool TimelineModel::replantCompositions(int currentCompo, bool updateView)
{
    // We ensure that the compositions are planted in a decreasing order of b_track.
    // For that, there is no better option than to disconnect every composition and then reinsert everything in the correct order.
    std::vector<std::pair<int, int>> compos;
    for (const auto &compo : m_allCompositions) {
        int trackId = compo.second->getCurrentTrackId();
        if (trackId == -1 || compo.second->getATrack() == -1) {
            continue;
        }
        // Note: we need to retrieve the position of the track, that is its melt index.
        int trackPos = getTrackMltIndex(trackId);
        compos.emplace_back(trackPos, compo.first);
        if (compo.first != currentCompo) {
            unplantComposition(compo.first);
        }
    }
    // sort by decreasing b_track
    std::sort(compos.begin(), compos.end(), [](const std::pair<int, int> &a, const std::pair<int, int> &b) { return a.first > b.first; });
    // replant
    QScopedPointer<Mlt::Field> field(m_tractor->field());
    field->lock();

    // Unplant track compositing
    mlt_service nextservice = mlt_service_get_producer(field->get_service());
    mlt_properties properties = MLT_SERVICE_PROPERTIES(nextservice);
    QString resource = mlt_properties_get(properties, "mlt_service");

    mlt_service_type mlt_type = mlt_service_identify(nextservice);
    QList<Mlt::Transition *> trackCompositions;
    while (mlt_type == transition_type) {
        Mlt::Transition transition((mlt_transition)nextservice);
        nextservice = mlt_service_producer(nextservice);
        int internal = transition.get_int("internal_added");
        if (internal > 0 && resource != QLatin1String("mix")) {
            trackCompositions << new Mlt::Transition(transition);
            field->disconnect_service(transition);
            transition.disconnect_all_producers();
        }
        if (nextservice == nullptr) {
            break;
        }
        mlt_type = mlt_service_identify(nextservice);
        properties = MLT_SERVICE_PROPERTIES(nextservice);
        resource = mlt_properties_get(properties, "mlt_service");
    }
    // Sort track compositing
    std::sort(trackCompositions.begin(), trackCompositions.end(), [](Mlt::Transition *a, Mlt::Transition *b) { return a->get_b_track() < b->get_b_track(); });

    for (const auto &compo : compos) {
        int aTrack = m_allCompositions[compo.second]->getATrack();
        Q_ASSERT(aTrack != -1 && aTrack < m_tractor->count());

        int ret = field->plant_transition(*m_allCompositions[compo.second].get(), aTrack, compo.first);
        qDebug() << "Planting composition " << compo.second << "in " << aTrack << "/" << compo.first << "IN = " << m_allCompositions[compo.second]->getIn()
                 << "OUT = " << m_allCompositions[compo.second]->getOut() << "ret=" << ret;

        Mlt::Transition &transition = *m_allCompositions[compo.second].get();
        transition.set_tracks(aTrack, compo.first);
        mlt_service consumer = mlt_service_consumer(transition.get_service());
        Q_ASSERT(consumer != nullptr);
        if (ret != 0) {
            field->unlock();
            return false;
        }
    }
    // Replant last tracks compositing
    while (!trackCompositions.isEmpty()) {
        Mlt::Transition *firstTr = trackCompositions.takeFirst();
        field->plant_transition(*firstTr, firstTr->get_a_track(), firstTr->get_b_track());
    }
    field->unlock();
    if (updateView) {
        QModelIndex modelIndex = makeCompositionIndexFromID(currentCompo);
        notifyChange(modelIndex, modelIndex, ItemATrack);
    }
    return true;
}

bool TimelineModel::unplantComposition(int compoId)
{
    qDebug() << "Unplanting" << compoId;
    Mlt::Transition &transition = *m_allCompositions[compoId].get();
    mlt_service consumer = mlt_service_consumer(transition.get_service());
    Q_ASSERT(consumer != nullptr);
    QScopedPointer<Mlt::Field> field(m_tractor->field());
    field->lock();
    field->disconnect_service(transition);
    int ret = transition.disconnect_all_producers();

    mlt_service nextservice = mlt_service_get_producer(transition.get_service());
    // mlt_service consumer = mlt_service_consumer(transition.get_service());
    Q_ASSERT(nextservice == nullptr);
    // Q_ASSERT(consumer == nullptr);
    field->unlock();
    return ret != 0;
}

bool TimelineModel::checkConsistency()
{
    for (const auto &tck : m_iteratorTable) {
        auto track = (*tck.second);
        // Check parent/children link for tracks
        if (auto ptr = track->m_parent.lock()) {
            if (ptr.get() != this) {
                qDebug() << "Wrong parent for track" << tck.first;
                return false;
            }
        } else {
            qDebug() << "NULL parent for track" << tck.first;
            return false;
        }
        // check consistency of track
        if (!track->checkConsistency()) {
            qDebug() << "Consistency check failed for track" << tck.first;
            return false;
        }
    }

    // We store all in/outs of clips to check snap points
    std::map<int, int> snaps;
    // Check parent/children link for clips
    for (const auto &cp : m_allClips) {
        auto clip = (cp.second);
        // Check parent/children link for tracks
        if (auto ptr = clip->m_parent.lock()) {
            if (ptr.get() != this) {
                qDebug() << "Wrong parent for clip" << cp.first;
                return false;
            }
        } else {
            qDebug() << "NULL parent for clip" << cp.first;
            return false;
        }
        if (getClipTrackId(cp.first) != -1) {
            snaps[clip->getPosition()] += 1;
            snaps[clip->getPosition() + clip->getPlaytime()] += 1;
        }
        if (!clip->checkConsistency()) {
            qDebug() << "Consistency check failed for clip" << cp.first;
            return false;
        }
    }
    for (const auto &cp : m_allCompositions) {
        auto clip = (cp.second);
        // Check parent/children link for tracks
        if (auto ptr = clip->m_parent.lock()) {
            if (ptr.get() != this) {
                qDebug() << "Wrong parent for compo" << cp.first;
                return false;
            }
        } else {
            qDebug() << "NULL parent for compo" << cp.first;
            return false;
        }
        if (getCompositionTrackId(cp.first) != -1) {
            snaps[clip->getPosition()] += 1;
            snaps[clip->getPosition() + clip->getPlaytime()] += 1;
        }
    }
    // Check snaps
    auto stored_snaps = m_snaps->_snaps();
    if (snaps.size() != stored_snaps.size()) {
        qDebug() << "Wrong number of snaps: " << snaps.size() << " == " << stored_snaps.size();
        return false;
    }
    for (auto i = snaps.begin(), j = stored_snaps.begin(); i != snaps.end(); ++i, ++j) {
        if (*i != *j) {
            qDebug() << "Wrong snap info at point" << (*i).first;
            return false;
        }
    }

    // We check consistency with bin model
    auto binClips = pCore->projectItemModel()->getAllClipIds();
    // First step: all clips referenced by the bin model exist and are inserted
    for (const auto &binClip : binClips) {
        auto projClip = pCore->projectItemModel()->getClipByBinID(binClip);
        for (const auto &insertedClip : projClip->m_registeredClips) {
            if (auto ptr = insertedClip.second.lock()) {
                if (ptr.get() == this) { // check we are talking of this timeline
                    if (!isClip(insertedClip.first)) {
                        qDebug() << "Bin model registers a bad clip ID" << insertedClip.first;
                        return false;
                    }
                }
            } else {
                qDebug() << "Bin model registers a clip in a NULL timeline" << insertedClip.first;
                return false;
            }
        }
    }

    // Second step: all clips are referenced
    for (const auto &clip : m_allClips) {
        auto binId = clip.second->m_binClipId;
        auto projClip = pCore->projectItemModel()->getClipByBinID(binId);
        if (projClip->m_registeredClips.count(clip.first) == 0) {
            qDebug() << "Clip " << clip.first << "not registered in bin";
            return false;
        }
    }

    // We now check consistency of the compositions. For that, we list all compositions of the tractor, and see if we have a matching one in our
    // m_allCompositions
    std::unordered_set<int> remaining_compo;
    for (const auto &compo : m_allCompositions) {
        if (getCompositionTrackId(compo.first) != -1 && m_allCompositions[compo.first]->getATrack() != -1) {
            remaining_compo.insert(compo.first);

            // check validity of the consumer
            Mlt::Transition &transition = *m_allCompositions[compo.first].get();
            mlt_service consumer = mlt_service_consumer(transition.get_service());
            Q_ASSERT(consumer != nullptr);
        }
    }
    QScopedPointer<Mlt::Field> field(m_tractor->field());
    field->lock();

    mlt_service nextservice = mlt_service_get_producer(field->get_service());
    mlt_service_type mlt_type = mlt_service_identify(nextservice);
    while (nextservice != nullptr) {
        if (mlt_type == transition_type) {
            auto tr = (mlt_transition)nextservice;
            int currentTrack = mlt_transition_get_b_track(tr);
            int currentATrack = mlt_transition_get_a_track(tr);

            int currentIn = (int)mlt_transition_get_in(tr);
            int currentOut = (int)mlt_transition_get_out(tr);

            qDebug() << "looking composition IN: " << currentIn << ", OUT: " << currentOut << ", TRACK: " << currentTrack << " / " << currentATrack;
            int foundId = -1;
            // we iterate to try to find a matching compo
            for (int compoId : remaining_compo) {
                if (getTrackMltIndex(getCompositionTrackId(compoId)) == currentTrack && m_allCompositions[compoId]->getATrack() == currentATrack &&
                    m_allCompositions[compoId]->getIn() == currentIn && m_allCompositions[compoId]->getOut() == currentOut) {
                    foundId = compoId;
                    break;
                }
            }
            if (foundId == -1) {
                qDebug() << "Error, we didn't find matching composition IN: " << currentIn << ", OUT: " << currentOut << ", TRACK: " << currentTrack << " / "
                         << currentATrack;
                field->unlock();
                return false;
            }
            qDebug() << "Found";

            remaining_compo.erase(foundId);
        }
        nextservice = mlt_service_producer(nextservice);
        if (nextservice == nullptr) {
            break;
        }
        mlt_type = mlt_service_identify(nextservice);
    }
    field->unlock();

    if (!remaining_compo.empty()) {
        qDebug() << "Error: We found less compositions than expected. Compositions that have not been found:";
        for (int compoId : remaining_compo) {
            qDebug() << compoId;
        }
        return false;
    }

    // We check consistency of groups
    if (!m_groups->checkConsistency(true, true)) {
        qDebug() << "== ERROR IN GROUP CONSISTENCY";
        return false;
    }
    return true;
}

void TimelineModel::setTimelineEffectsEnabled(bool enabled)
{
    m_timelineEffectsEnabled = enabled;
    // propagate info to clips
    for (const auto &clip : m_allClips) {
        clip.second->setTimelineEffectsEnabled(enabled);
    }

    // TODO if we support track effects, they should be disabled here too
}

std::shared_ptr<Mlt::Producer> TimelineModel::producer()
{
    return std::make_shared<Mlt::Producer>(tractor());
}

void TimelineModel::checkRefresh(int start, int end)
{
    if (m_blockRefresh) {
        return;
    }
    int currentPos = tractor()->position();
    if (currentPos >= start && currentPos < end) {
        emit requestMonitorRefresh();
    }
}

void TimelineModel::clearAssetView(int itemId)
{
    emit requestClearAssetView(itemId);
}

std::shared_ptr<AssetParameterModel> TimelineModel::getCompositionParameterModel(int compoId) const
{
    READ_LOCK();
    Q_ASSERT(isComposition(compoId));
    return std::static_pointer_cast<AssetParameterModel>(m_allCompositions.at(compoId));
}

std::shared_ptr<EffectStackModel> TimelineModel::getClipEffectStackModel(int clipId) const
{
    READ_LOCK();
    Q_ASSERT(isClip(clipId));
    return std::static_pointer_cast<EffectStackModel>(m_allClips.at(clipId)->m_effectStack);
}

std::shared_ptr<EffectStackModel> TimelineModel::getTrackEffectStackModel(int trackId)
{
    READ_LOCK();
    Q_ASSERT(isTrack(trackId));
    return getTrackById(trackId)->m_effectStack;
}

QStringList TimelineModel::extractCompositionLumas() const
{
    QStringList urls;
    for (const auto &compo : m_allCompositions) {
        QString luma = compo.second->getProperty(QStringLiteral("resource"));
        if (!luma.isEmpty()) {
            urls << QUrl::fromLocalFile(luma).toLocalFile();
        }
    }
    urls.removeDuplicates();
    return urls;
}

void TimelineModel::adjustAssetRange(int clipId, int in, int out)
{
    Q_UNUSED(clipId)
    Q_UNUSED(in)
    Q_UNUSED(out)
    // pCore->adjustAssetRange(clipId, in, out);
}

void TimelineModel::requestClipReload(int clipId)
{
    std::function<bool(void)> local_undo = []() { return true; };
    std::function<bool(void)> local_redo = []() { return true; };

    // in order to make the producer change effective, we need to unplant / replant the clip in int track
    int old_trackId = getClipTrackId(clipId);
    int oldPos = getClipPosition(clipId);
    int oldOut = getClipIn(clipId) + getClipPlaytime(clipId);

    // Check if clip out is longer than actual producer duration (if user forced duration)
    std::shared_ptr<ProjectClip> binClip = pCore->projectItemModel()->getClipByBinID(getClipBinId(clipId));
    bool refreshView = oldOut > (int)binClip->frameDuration();
    if (old_trackId != -1) {
        getTrackById(old_trackId)->requestClipDeletion(clipId, refreshView, true, local_undo, local_redo);
    }    
    if (old_trackId != -1) {
        m_allClips[clipId]->refreshProducerFromBin();
        getTrackById(old_trackId)->requestClipInsertion(clipId, oldPos, refreshView, true, local_undo, local_redo);
    }
}

void TimelineModel::replugClip(int clipId)
{
    int old_trackId = getClipTrackId(clipId);
    if (old_trackId != -1) {
        getTrackById(old_trackId)->replugClip(clipId);
    }
}

void TimelineModel::requestClipUpdate(int clipId, const QVector<int> &roles)
{
    QModelIndex modelIndex = makeClipIndexFromID(clipId);
    if (roles.contains(TimelineModel::ReloadThumbRole)) {
        m_allClips[clipId]->forceThumbReload = !m_allClips[clipId]->forceThumbReload;
    }
    notifyChange(modelIndex, modelIndex, roles);
}

bool TimelineModel::requestClipTimeWarp(int clipId, double speed, Fun &undo, Fun &redo)
{
    QWriteLocker locker(&m_lock);
    if (qFuzzyCompare(speed, m_allClips[clipId]->getSpeed())) {
        return true;
    }
    std::function<bool(void)> local_undo = []() { return true; };
    std::function<bool(void)> local_redo = []() { return true; };
    int oldPos = getClipPosition(clipId);
    // in order to make the producer change effective, we need to unplant / replant the clip in int track
    bool success = true;
    int trackId = getClipTrackId(clipId);
    if (trackId != -1) {
        success = success && getTrackById(trackId)->requestClipDeletion(clipId, true, true, local_undo, local_redo);
    }
    if (success) {
        success = m_allClips[clipId]->useTimewarpProducer(speed, local_undo, local_redo);
    }
    if (trackId != -1) {
        success = success && getTrackById(trackId)->requestClipInsertion(clipId, oldPos, true, true, local_undo, local_redo);
    }
    if (!success) {
        local_undo();
        return false;
    }
    UPDATE_UNDO_REDO(local_redo, local_undo, undo, redo);
    return success;
}

bool TimelineModel::requestClipTimeWarp(int clipId, double speed)
{
    Fun undo = []() { return true; };
    Fun redo = []() { return true; };
    // Get main clip info
    int trackId = getClipTrackId(clipId);
    bool result = true;
    if (trackId != -1) {
        // Check if clip has a split partner
        int splitId = m_groups->getSplitPartner(clipId);
        if (splitId > -1) {
            result = requestClipTimeWarp(splitId, speed / 100.0, undo, redo);
        }
        if (result) {
            result = requestClipTimeWarp(clipId, speed / 100.0, undo, redo);
        } else {
            pCore->displayMessage(i18n("Change speed failed"), ErrorMessage);
            undo();
            return false;
        }
    } else {
        // If clip is not inserted on a track, we just change the producer
        m_allClips[clipId]->useTimewarpProducer(speed, undo, redo);
    }
    if (result) {
        PUSH_UNDO(undo, redo, i18n("Change clip speed"));
        return true;
    }
    return false;
}

const QString TimelineModel::getTrackTagById(int trackId) const
{
    READ_LOCK();
    Q_ASSERT(isTrack(trackId));
    bool isAudio = getTrackById_const(trackId)->isAudioTrack();
    int count = 1;
    int totalAudio = 2;
    auto it = m_allTracks.begin();
    bool found = false;
    while ((isAudio || !found) && it != m_allTracks.end()) {
        if ((*it)->isAudioTrack()) {
            totalAudio++;
            if (isAudio && !found) {
                count++;
            }
        } else if (!isAudio) {
            count++;
        }
        if ((*it)->getId() == trackId) {
            found = true;
        }
        it++;
    }
    return isAudio ? QStringLiteral("A%1").arg(totalAudio - count) : QStringLiteral("V%1").arg(count - 1);
}

void TimelineModel::updateProfile(Mlt::Profile *profile)
{
    m_profile = profile;
    m_tractor->set_profile(*m_profile);
}

int TimelineModel::getBlankSizeNearClip(int clipId, bool after) const
{
    READ_LOCK();
    Q_ASSERT(m_allClips.count(clipId) > 0);
    int trackId = getClipTrackId(clipId);
    if (trackId != -1) {
        return getTrackById_const(trackId)->getBlankSizeNearClip(clipId, after);
    }
    return 0;
}

int TimelineModel::getPreviousTrackId(int trackId)
{
    READ_LOCK();
    Q_ASSERT(isTrack(trackId));
    auto it = m_iteratorTable.at(trackId);
    bool audioWanted = (*it)->isAudioTrack();
    while (it != m_allTracks.begin()) {
        --it;
        if (it != m_allTracks.begin() && (*it)->isAudioTrack() == audioWanted) {
            break;
        }
    }
    return it == m_allTracks.begin() ? trackId : (*it)->getId();
}

int TimelineModel::getNextTrackId(int trackId)
{
    READ_LOCK();
    Q_ASSERT(isTrack(trackId));
    auto it = m_iteratorTable.at(trackId);
    bool audioWanted = (*it)->isAudioTrack();
    while (it != m_allTracks.end()) {
        ++it;
        if (it != m_allTracks.end() && (*it)->isAudioTrack() == audioWanted) {
            break;
        }
    }
    return it == m_allTracks.end() ? trackId : (*it)->getId();
}
