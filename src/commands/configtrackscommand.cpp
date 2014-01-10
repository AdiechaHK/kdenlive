/***************************************************************************
 *   Copyright (C) 2010 by Till Theato (root@ttill.de)                     *
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


#include "configtrackscommand.h"
#include "customtrackview.h"


ConfigTracksCommand::ConfigTracksCommand(CustomTrackView* view, const QList<TrackInfo> &oldInfos, const QList<TrackInfo> &newInfos, QUndoCommand* parent) :
        QUndoCommand(parent),
        m_view(view),
        m_oldInfos(oldInfos),
        m_newInfos(newInfos)
{
    setText(i18n("Configure Tracks"));
}

// virtual
void ConfigTracksCommand::undo()
{
    m_view->configTracks(m_oldInfos);
}

// virtual
void ConfigTracksCommand::redo()
{
    m_view->configTracks(m_newInfos);
}
