/***************************************************************************
                          edittransitioncommand.cpp  -  description
                             -------------------
    begin                : 2008
    copyright            : (C) 2008 by Marco Gittler
    email                : g.marco@freenet.de
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "edittransitioncommand.h"
#include "customtrackview.h"

#include <KLocale>

EditTransitionCommand::EditTransitionCommand(CustomTrackView *view, const int track, const GenTime &pos, const QDomElement &oldeffect, const QDomElement &effect, bool doIt, QUndoCommand * parent) :
        QUndoCommand(parent),
        m_view(view),
        m_track(track),
        m_oldeffect(oldeffect),
        m_pos(pos),
        m_doIt(doIt)
{
    m_effect = effect.cloneNode().toElement();
    QString effectName;
    QDomElement namenode = effect.firstChildElement("name");
    if (!namenode.isNull()) effectName = i18n(namenode.text().toUtf8().data());
    else effectName = i18n("effect");
    setText(i18n("Edit transition %1", effectName));
}

// virtual
int EditTransitionCommand::id() const
{
    return 2;
}

// virtual
bool EditTransitionCommand::mergeWith(const QUndoCommand * other)
{
    if (other->id() != id()) return false;
    if (m_track != static_cast<const EditTransitionCommand*>(other)->m_track) return false;
    if (m_pos != static_cast<const EditTransitionCommand*>(other)->m_pos) return false;
    m_effect = static_cast<const EditTransitionCommand*>(other)->m_effect;
    return true;
}

// virtual
void EditTransitionCommand::undo()
{
    m_view->updateTransition(m_track, m_pos, m_effect, m_oldeffect, m_doIt);
}
// virtual
void EditTransitionCommand::redo()
{
    m_view->updateTransition(m_track, m_pos, m_oldeffect, m_effect, m_doIt);
    m_doIt = true;
}

