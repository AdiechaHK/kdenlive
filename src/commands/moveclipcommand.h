/***************************************************************************
 *   Copyright (C) 2007 by Jean-Baptiste Mardelle (jb@kdenlive.org)        *
 *                 2012    Simon A. Eugster <simon.eu@gmail.com>           *
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


#ifndef MOVECLIPCOMMAND_H
#define MOVECLIPCOMMAND_H


#include "definitions.h"
#include <QUndoCommand>

class CustomTrackView;

class MoveClipCommand : public QUndoCommand
{
public:
    MoveClipCommand(CustomTrackView *view, const ItemInfo &start, const ItemInfo &end, bool doIt, QUndoCommand * parent = 0);
    virtual void undo();
    virtual void redo();

private:
    CustomTrackView *m_view;
    const ItemInfo m_startPos;
    ItemInfo m_endPos;
    bool m_doIt;
    bool m_refresh;
    bool m_success;
};

#endif

