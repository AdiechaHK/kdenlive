/***************************************************************************
                          movetransitioncommand.h  -  description
                             -------------------
    begin                : Mar 15 2008
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

#ifndef MOVETRANSITIONCOMMAND_H
#define MOVETRANSITIONCOMMAND_H

#include "definitions.h"

#include <QUndoCommand>

class CustomTrackView;

class MoveTransitionCommand : public QUndoCommand
{
public:
    MoveTransitionCommand(CustomTrackView *view, const ItemInfo &start, const ItemInfo &end, bool doIt, QUndoCommand * parent = 0);
    void undo();
    void redo();

private:
    CustomTrackView *m_view;
    ItemInfo m_startPos;
    ItemInfo m_endPos;
    bool m_doIt;
    bool m_refresh;
};

#endif

