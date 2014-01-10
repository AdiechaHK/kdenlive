/***************************************************************************
                          editkeyframecommand.cpp  -  description
                             -------------------
    begin                : 2008
    copyright            : (C) 2008 by Jean-Baptiste Mardelle
    email                : jb@kdenlive.org
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/


#include "editkeyframecommand.h"
#include "customtrackview.h"

#include <KLocalizedString>

EditKeyFrameCommand::EditKeyFrameCommand(CustomTrackView *view, const int track, const GenTime &pos, const int effectIndex, const QString& oldkeyframes, const QString& newkeyframes, bool doIt) :
    QUndoCommand(),
    m_view(view),
    m_oldkfr(oldkeyframes),
    m_newkfr(newkeyframes),
    m_track(track),
    m_index(effectIndex),
    m_pos(pos),
    m_doIt(doIt)
{
    int prev = m_oldkfr.split(QLatin1Char(';'), QString::SkipEmptyParts).count();
    int next = m_newkfr.split(QLatin1Char(';'), QString::SkipEmptyParts).count();
    if (prev == next)
        setText(i18n("Edit keyframe"));
    else if (prev > next)
        setText(i18n("Delete keyframe"));
    else
        setText(i18n("Add keyframe"));
    //kDebug() << "///  CREATE GUIDE COMMAND, TIMES: " << m_oldPos.frames(25) << "x" << m_pos.frames(25);
}


// virtual
void EditKeyFrameCommand::undo()
{
    m_view->editKeyFrame(m_pos, m_track, m_index, m_oldkfr);
    m_doIt = true;
}
// virtual
void EditKeyFrameCommand::redo()
{
    if (m_doIt) {
        m_view->editKeyFrame(m_pos, m_track, m_index, m_newkfr);
    }
    m_doIt = true;
}

