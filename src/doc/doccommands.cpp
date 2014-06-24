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

#include "doc/doccommands.h"
#include "doc/kdenlivedoc.h"
#include <KLocalizedString>

AddClipCommand::AddClipCommand(KdenliveDoc *doc, const QDomElement &xml, const QString &id, bool doIt, QUndoCommand * parent) :
        QUndoCommand(parent),
        m_doc(doc),
        m_xml(xml),
        m_id(id),
        m_doIt(doIt)
{
    if (doIt) setText(i18n("Add clip"));
    else setText(i18n("Delete clip"));
}
// virtual
void AddClipCommand::undo()
{
    if (m_doIt)
        m_doc->deleteClip(m_id);
    else
        m_doc->addClip(m_xml, m_id);
}
// virtual
void AddClipCommand::redo()
{
    if (m_doIt)
        m_doc->addClip(m_xml, m_id);
    else
        m_doc->deleteClip(m_id);
}
