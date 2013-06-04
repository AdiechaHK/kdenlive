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


#include "moveeffectcommand.h"
#include "customtrackview.h"

#include <KLocale>
#include <KDebug>

MoveEffectCommand::MoveEffectCommand(CustomTrackView *view, const int track, const GenTime &pos, const QList<int> &oldPos, int newPos, QUndoCommand * parent) :
    QUndoCommand(parent),
    m_view(view),
    m_track(track),
    m_oldindex(oldPos),
    m_pos(pos)
{
    for (int i = 0; i < m_oldindex.count(); ++i) {
        m_newindex << newPos + i;
    }
    /*    QString effectName;
        QDomElement namenode = effect.firstChildElement("name");
        if (!namenode.isNull()) effectName = i18n(namenode.text().toUtf8().data());
        else effectName = i18n("effect");
        setText(i18n("Move effect %1", effectName));*/
    setText(i18n("Move effect"));
}

// virtual
int MoveEffectCommand::id() const
{
    return 2;
}

// virtual
bool MoveEffectCommand::mergeWith(const QUndoCommand * other)
{
    return false;
    if (other->id() != id()) return false;
    if (m_track != static_cast<const MoveEffectCommand*>(other)->m_track) return false;
    if (m_pos != static_cast<const MoveEffectCommand*>(other)->m_pos) return false;
    m_oldindex = static_cast<const MoveEffectCommand*>(other)->m_oldindex;
    m_newindex = static_cast<const MoveEffectCommand*>(other)->m_newindex;
    return true;
}

// virtual
void MoveEffectCommand::undo()
{
    kDebug() << "----  undoing action";
    m_view->moveEffect(m_track, m_pos, m_newindex, m_oldindex);
}
// virtual
void MoveEffectCommand::redo()
{
    kDebug() << "----  redoing action";
    m_view->moveEffect(m_track, m_pos, m_oldindex, m_newindex);
}

