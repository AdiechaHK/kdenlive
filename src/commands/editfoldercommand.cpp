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


#include "editfoldercommand.h"
#include "projectlist.h"

#include <KLocalizedString>

EditFolderCommand::EditFolderCommand(ProjectList *view, const QString &newfolderName, const QString &oldfolderName, const QString &clipId, bool doIt, QUndoCommand *parent) :
        QUndoCommand(parent),
        m_view(view),
        m_name(newfolderName),
        m_oldname(oldfolderName),
        m_id(clipId),
        m_doIt(doIt)
{
    setText(i18n("Rename folder"));
}

// virtual
void EditFolderCommand::undo()
{
    m_view->slotAddFolder(m_oldname, m_id, false, true);
}
// virtual
void EditFolderCommand::redo()
{
    if (m_doIt) m_view->slotAddFolder(m_name, m_id, false, true);
    m_doIt = true;
}


