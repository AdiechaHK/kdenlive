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


#include "addtrackcommand.h"
#include "customtrackview.h"

#include <KLocalizedString>
#include <KDebug>
AddTrackCommand::AddTrackCommand(CustomTrackView *view, int ix, const TrackInfo &info, bool addTrack, QUndoCommand * parent) :
        QUndoCommand(parent),
        m_view(view),
        m_ix(ix),
        m_addTrack(addTrack),
        m_info(info)
{
    if (addTrack)
        setText(i18n("Add track"));
    else
        setText(i18n("Delete track"));
}


// virtual
void AddTrackCommand::undo()
{
// kDebug()<<"----  undoing action";
    if (m_addTrack)
        m_view->removeTrack(m_ix);
    else
        m_view->addTrack(m_info, m_ix);
}
// virtual
void AddTrackCommand::redo()
{
    kDebug() << "----  redoing action";
    if (m_addTrack)
        m_view->addTrack(m_info, m_ix);
    else
        m_view->removeTrack(m_ix);
}

