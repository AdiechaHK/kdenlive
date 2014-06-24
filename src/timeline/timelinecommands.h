/***************************************************************************
 *   Copyright (C) 2007-2012 by Jean-Baptiste Mardelle (jb@kdenlive.org)   *
 *   Copyright (C) 2008 by Marco Gittler                                   *
 *   Copyright (C) 2010 by Till Theato (root@ttill.de)                     *
 *   Copyright (C) 2014 by Vincent Pinon (vpinon@april.org)                *
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


#ifndef TIMELINECOMMANDS_H
#define TIMELINECOMMANDS_H

#include <QUndoCommand>
#include <QDomElement>
#include "definitions.h"
#include "effectslist.h"
class GenTime;
class CustomTrackView;

class AddEffectCommand : public QUndoCommand
{
public:
    AddEffectCommand(CustomTrackView *view, const int track, const GenTime &pos, const QDomElement &effect, bool doIt, QUndoCommand * parent = 0);
    void undo();
    void redo();
private:
    CustomTrackView *m_view;
    int m_track;
    QDomElement m_effect;
    GenTime m_pos;
    bool m_doIt;
};

class AddExtraDataCommand : public QUndoCommand
{
public:
    AddExtraDataCommand(CustomTrackView *view, const QString&id, const QString&key, const QString &oldData, const QString &newData, QUndoCommand * parent = 0);
    void undo();
    void redo();
private:
    CustomTrackView *m_view;
    QString m_oldData;
    QString m_newData;
    QString m_key;
    QString m_id;
};

class AddMarkerCommand : public QUndoCommand
{
public:
    AddMarkerCommand(CustomTrackView *view, const CommentedTime &oldMarker, const CommentedTime &newMarker, const QString &id, QUndoCommand * parent = 0);
    void undo();
    void redo();
private:
    CustomTrackView *m_view;
    CommentedTime m_oldMarker;
    CommentedTime m_newMarker;
    QString m_id;
};

class AddTimelineClipCommand : public QUndoCommand
{
public:
    AddTimelineClipCommand(CustomTrackView *view, const QDomElement &xml, const QString &clipId, const ItemInfo &info, const EffectsList &effects, bool overwrite, bool push, bool doIt, bool doRemove, QUndoCommand * parent = 0);
    void undo();
    void redo();
private:
    CustomTrackView *m_view;
    ItemInfo m_clipInfo;
    EffectsList m_effects;
    QString m_clipId;
    QDomElement m_xml;
    bool m_doIt;
    bool m_remove;
    bool m_overwrite;
    bool m_push;
};

class AddTrackCommand : public QUndoCommand
{
public:
    AddTrackCommand(CustomTrackView *view, int ix, const TrackInfo &info, bool addTrack, QUndoCommand * parent = 0);
    void undo();
    void redo();
private:
    CustomTrackView *m_view;
    int m_ix;
    bool m_addTrack;
    TrackInfo m_info;
};

class AddTransitionCommand : public QUndoCommand
{
public:
    AddTransitionCommand(CustomTrackView *view, const ItemInfo &info, int transitiontrack, const QDomElement &params, bool remove, bool doIt, QUndoCommand * parent = 0);
    void undo();
    void redo();
private:
    CustomTrackView *m_view;
    ItemInfo m_info;
    QDomElement m_params;
    int m_track;
    bool m_doIt;
    bool m_remove;
    bool m_refresh;
};

class ChangeClipTypeCommand : public QUndoCommand
{
public:
    ChangeClipTypeCommand(CustomTrackView *view, const int track, const GenTime &pos, bool videoOnly, bool audioOnly, bool originalVideo, bool originalAudio, QUndoCommand * parent = 0);
    void undo();
    void redo();
private:
    CustomTrackView *m_view;
    const GenTime m_pos;
    const int m_track;
    bool m_videoOnly;
    bool m_audioOnly;
    bool m_originalVideoOnly;
    bool m_originalAudioOnly;
};

class ChangeEffectStateCommand : public QUndoCommand
{
public:
    ChangeEffectStateCommand(CustomTrackView *view, const int track, const GenTime &pos, const QList <int>& effectIndexes, bool disable, bool refreshEffectStack, bool doIt, QUndoCommand *parent = 0);
    void undo();
    void redo();
private:
    CustomTrackView *m_view;
    const int m_track;
    QList <int> m_effectIndexes;
    const GenTime m_pos;
    bool m_disable;
    bool m_doIt;
    bool m_refreshEffectStack;
};

class ChangeSpeedCommand : public QUndoCommand
{
public:
    ChangeSpeedCommand(CustomTrackView *view, const ItemInfo &info, const ItemInfo &speedIndependantInfo, double old_speed, double new_speed, int old_strobe, int new_strobe, const QString &clipId, QUndoCommand * parent = 0);
    void undo();
    void redo();
private:
    CustomTrackView *m_view;
    ItemInfo m_clipInfo;
    ItemInfo m_speedIndependantInfo;
    QString m_clipId;
    double m_old_speed;
    double m_new_speed;
    int m_old_strobe;
    int m_new_strobe;
};

class ConfigTracksCommand : public QUndoCommand
{
public:
    ConfigTracksCommand(CustomTrackView *view, const QList <TrackInfo> &oldInfos, const QList <TrackInfo> &newInfos, QUndoCommand * parent = 0);
    void undo();
    void redo();
private:
    CustomTrackView *m_view;
    QList <TrackInfo> m_oldInfos;
    QList <TrackInfo> m_newInfos;
};

class EditEffectCommand : public QUndoCommand
{
public:
    EditEffectCommand(CustomTrackView *view, const int track, const GenTime &pos, const QDomElement &oldeffect, const QDomElement &effect, int stackPos, bool refreshEffectStack, bool doIt, QUndoCommand *parent = 0);
    virtual int id() const;
    virtual bool mergeWith(const QUndoCommand * command);
    void undo();
    void redo();
private:
    CustomTrackView *m_view;
    const int m_track;
    QDomElement m_oldeffect;
    QDomElement m_effect;
    const GenTime m_pos;
    int m_stackPos;
    bool m_doIt;
    bool m_refreshEffectStack;
};

class EditGuideCommand : public QUndoCommand
{
public:
    EditGuideCommand(CustomTrackView *view, const GenTime &oldPos, const QString &oldcomment, const GenTime &pos, const QString &comment, bool doIt, QUndoCommand * parent = 0);
    void undo();
    void redo();
private:
    CustomTrackView *m_view;
    QString m_oldcomment;
    QString m_comment;
    GenTime m_oldPos;
    GenTime m_pos;
    bool m_doIt;
};

class EditTransitionCommand : public QUndoCommand
{
public:
    EditTransitionCommand(CustomTrackView *view, const int track, const GenTime &pos, const QDomElement &oldeffect, const QDomElement &effect, bool doIt, QUndoCommand * parent = NULL);
    virtual int id() const;
    virtual bool mergeWith(const QUndoCommand * command);
    virtual void undo();
    virtual void redo();
private:
    CustomTrackView *m_view;
    const int m_track;
    QDomElement m_effect;
    QDomElement m_oldeffect;
    const GenTime m_pos;
    bool m_doIt;
};

class GroupClipsCommand : public QUndoCommand
{
public:
    GroupClipsCommand(CustomTrackView *view, const QList <ItemInfo> &clipInfos, const QList <ItemInfo> &transitionInfos, bool group, QUndoCommand * parent = 0);
    void undo();
    void redo();

private:
    CustomTrackView *m_view;
    const QList <ItemInfo> m_clips;
    const QList <ItemInfo> m_transitions;
    bool m_group;
};

class InsertSpaceCommand : public QUndoCommand
{
public:
    InsertSpaceCommand(CustomTrackView *view, const QList<ItemInfo> &clipsToMove, const QList<ItemInfo> &transToMove, int track, const GenTime &duration, bool doIt, QUndoCommand * parent = 0);
    void undo();
    void redo();
private:
    CustomTrackView *m_view;
    QList<ItemInfo> m_clipsToMove;
    QList<ItemInfo> m_transToMove;
    GenTime m_duration;
    int m_track;
    bool m_doIt;
};

class LockTrackCommand : public QUndoCommand
{
public:
    LockTrackCommand(CustomTrackView *view, int ix, bool lock, QUndoCommand * parent = 0);
    void undo();
    void redo();
private:
    CustomTrackView *m_view;
    int m_ix;
    bool m_lock;
};

class MoveClipCommand : public QUndoCommand
{
public:
    MoveClipCommand(CustomTrackView *view, const ItemInfo &start, const ItemInfo &end, bool doIt, QUndoCommand * parent = 0);
    void undo();
    void redo();
private:
    CustomTrackView *m_view;
    const ItemInfo m_startPos;
    ItemInfo m_endPos;
    bool m_doIt;
    bool m_refresh;
    bool m_success;
};

class MoveEffectCommand : public QUndoCommand
{
public:
    MoveEffectCommand(CustomTrackView *view, const int track, const GenTime &pos, const QList <int> &oldPos, int newPos, QUndoCommand * parent = 0);
    virtual int id() const;
    virtual bool mergeWith(const QUndoCommand * command);
    void undo();
    void redo();
private:
    CustomTrackView *m_view;
    int m_track;
    QList <int> m_oldindex;
    QList <int> m_newindex;
    GenTime m_pos;
};

class MoveGroupCommand : public QUndoCommand
{
public:
    MoveGroupCommand(CustomTrackView *view, const QList <ItemInfo> &startClip, const QList <ItemInfo> &startTransition, const GenTime &offset, const int trackOffset, bool doIt, QUndoCommand * parent = 0);
    void undo();
    void redo();
private:
    CustomTrackView *m_view;
    const QList <ItemInfo> m_startClip;
    const QList <ItemInfo> m_startTransition;
    const GenTime m_offset;
    const int m_trackOffset;
    bool m_doIt;
};

class MoveTransitionCommand : public QUndoCommand
{
public:
    MoveTransitionCommand(CustomTrackView *view, const ItemInfo &start, const ItemInfo &end, bool doIt, QUndoCommand * parent = 0);
    void undo();
    void redo();
private:
    CustomTrackView *m_view;
    ItemInfo m_startPos;
    ItemInfo m_endPos;
    bool m_doIt;
    bool m_refresh;
};

class RazorClipCommand : public QUndoCommand
{
public:
    RazorClipCommand(CustomTrackView *view, const ItemInfo &info, EffectsList stack, const GenTime &cutTime, bool doIt = true, QUndoCommand * parent = 0);
    void undo();
    void redo();
private:
    CustomTrackView *m_view;
    ItemInfo m_info;
    EffectsList m_originalStack;
    GenTime m_cutTime;
    bool m_doIt;
};
/*
class RazorGroupCommand : public QUndoCommand
{
public:
    RazorGroupCommand(CustomTrackView *view, QList <ItemInfo> clips1, QList <ItemInfo> transitions1, QList <ItemInfo> clipsCut, QList <ItemInfo> transitionsCut, QList <ItemInfo> clips2, QList <ItemInfo> transitions2, GenTime cutPos, QUndoCommand * parent = 0);
    virtual void undo();
    virtual void redo();
private:
    CustomTrackView *m_view;
    QList <ItemInfo> m_clips1;
    QList <ItemInfo> m_transitions1;
    QList <ItemInfo> m_clipsCut;
    QList <ItemInfo> m_transitionsCut;
    QList <ItemInfo> m_clips2;
    QList <ItemInfo> m_transitions2;
    GenTime m_cutPos;
};
*/
class RebuildGroupCommand : public QUndoCommand
{
public:
    RebuildGroupCommand(CustomTrackView *view, int childTrack, const GenTime &childPos, QUndoCommand* parent = 0);
    void undo();
    void redo();
private:
    CustomTrackView *m_view;
    int m_childTrack;
    GenTime m_childPos;
};

class RefreshMonitorCommand : public QUndoCommand
{
public:
    RefreshMonitorCommand(CustomTrackView *view, bool execute, bool refreshOnUndo, QUndoCommand * parent = 0);
    void undo();
    void redo();
private:
    CustomTrackView *m_view;
    bool m_exec;
    bool m_execOnUndo;
};

class ResizeClipCommand : public QUndoCommand
{
public:
    ResizeClipCommand(CustomTrackView *view, const ItemInfo &start, const ItemInfo &end, bool doIt, bool dontWorry, QUndoCommand * parent = 0);
    void undo();
    void redo();
private:
    CustomTrackView *m_view;
    ItemInfo m_startPos;
    ItemInfo m_endPos;
    bool m_doIt;
    bool m_dontWorry;
};

class SplitAudioCommand : public QUndoCommand
{
public:
    SplitAudioCommand(CustomTrackView *view, const int track, const GenTime &pos, const EffectsList &effects, QUndoCommand * parent = 0);
    void undo();
    void redo();
private:
    CustomTrackView *m_view;
    const GenTime m_pos;
    const int m_track;
    EffectsList m_effects;
};

#endif

