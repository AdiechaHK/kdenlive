/***************************************************************************
                          timeline/transition.h  -  description
                             -------------------
    begin                : Tue Jan 24 2006
    copyright            : (C) 2006 by Jean-Baptiste Mardelle
    email                : jb@ader.ch
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

/**
 * @class Transition
 * @author Jean-Baptiste Mardelle
 * @brief Describes a transition with a name, parameters, keyframes, etc.
 */

#ifndef TRANSITION_H
#define TRANSITION_H

#include <QString>
#include <QGraphicsRectItem>
#include <QPixmap>
#include <QDomElement>
#include <QMap>

#include "gentime.h"
#include "definitions.h"
#include "timeline/abstractclipitem.h"

class ClipItem;

class Transition : public AbstractClipItem
{
    Q_OBJECT
public:

    Transition(const ItemInfo &info, int transitiontrack, double fps, const QDomElement &params = QDomElement(), bool automaticTransition = false);
    virtual ~Transition();
    virtual void paint(QPainter *painter,
                       const QStyleOptionGraphicsItem *option,
                       QWidget *widget);
    virtual int type() const;

    /** @brief Returns an XML representation of this transition. */
    QDomElement toXML();

    /** @brief Returns the track number of the transition in the playlist. */
    int transitionEndTrack() const;
    QString transitionTag() const;
    QStringList transitionInfo() const;
    OperationType operationMode(const QPointF &pos);
    static int itemHeight();
    static int itemOffset();
    //const QMap < QString, QString > transitionParameters() const;
    void setTransitionParameters(const QDomElement &params);
    void setTransitionTrack(int track);

    /** @brief Links the transition to another track.
     *
     * This happens only if the current track is not forced. */
    void updateTransitionEndTrack(int newtrack);
    void setForcedTrack(bool force, int track);
    bool forcedTrack() const;
    Transition *clone();
    bool isAutomatic() const;
    void setAutomatic(bool automatic);
    bool hasGeometry();
    int defaultZValue() const;
    /** @brief When a transition is resized, check if keyframes are out of the transition and fix if necessary. 
     * @param oldEnd the previous transition end, so that when we expand the transition, if there is a keyframe at end we move it
     */
    bool updateKeyframes(int oldEnd);

protected:
    virtual QVariant itemChange(GraphicsItemChange change, const QVariant &value);

private:
    QString m_name;
    bool m_forceTransitionTrack;

    /** @brief True if the transition is attached to its clip. */
    bool m_automaticTransition;

    /** @brief Contains the transition parameters. */
    QDomElement m_parameters;

    int m_transitionTrack;

    /** @brief Returns the display name for a transition type. */
    QString getTransitionName(const TransitionType & type);

    /** @brief Returns the transition type for a given name. */
    TransitionType getTransitionForName(const QString & type);
};

#endif
