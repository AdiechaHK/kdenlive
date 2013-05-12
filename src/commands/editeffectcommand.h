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


#ifndef EDITEFFECTCOMMAND_H
#define EDITEFFECTCOMMAND_H

#include <QUndoCommand>
#include <KDebug>
#include <gentime.h>
#include <QDomElement>

class CustomTrackView;

class EditEffectCommand : public QUndoCommand
{
public:
    EditEffectCommand(CustomTrackView *view, const int track, GenTime pos, const QDomElement &oldeffect, const QDomElement &effect, int stackPos, bool refreshEffectStack, bool doIt, QUndoCommand *parent = 0);

    virtual int id() const;
    virtual bool mergeWith(const QUndoCommand * command);
    virtual void undo();
    virtual void redo();

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

#endif

