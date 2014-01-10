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


#include "addeffectcommand.h"
#include "customtrackview.h"

#include <KLocalizedString>

AddEffectCommand::AddEffectCommand(CustomTrackView *view, const int track, const GenTime &pos, const QDomElement &effect, bool doIt, QUndoCommand * parent) :
        QUndoCommand(parent),
        m_view(view),
        m_track(track),
        m_effect(effect.cloneNode().toElement()),
        m_pos(pos),
        m_doIt(doIt)
{
    QString effectName;
    QDomElement namenode = m_effect.firstChildElement(QLatin1String("name"));
    if (!namenode.isNull())
        effectName = i18n(namenode.text().toUtf8().data());
    else
        effectName = i18n("effect");

    if (doIt)
        setText(i18n("Add %1", effectName));
    else
        setText(i18n("Delete %1", effectName));
}


// virtual
void AddEffectCommand::undo()
{
    kDebug() << "----  undoing action";
    if (m_doIt)
        m_view->deleteEffect(m_track, m_pos, m_effect);
    else
        m_view->addEffect(m_track, m_pos, m_effect);
}
// virtual
void AddEffectCommand::redo()
{
    kDebug() << "----  redoing action";
    if (m_doIt)
        m_view->addEffect(m_track, m_pos, m_effect);
    else
        m_view->deleteEffect(m_track, m_pos, m_effect);
}


