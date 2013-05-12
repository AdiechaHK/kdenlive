/***************************************************************************
                          effectslist.h  -  description
                             -------------------
    begin                : Sat Aug 10 2002
    copyright            : (C) 2002 by Jason Wood
    email                : jasonwood@blueyonder.co.uk
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
 * @class EffectsList
 * @brief List for effects objects.
 * @author Jason Wood
 *
 * This is a list of DocClipBase objects, to be used instead of
 * QList<DocClipBase> to enable sorting lists correctly. It also contains the
 * ability to set a "master clip", which can be used by a number of operations
 * where there is the need of one clip to act as a reference for what happens to
 * all clips.
 */

#ifndef EFFECTSLIST_H
#define EFFECTSLIST_H

#include <QDomDocument>

namespace Kdenlive {
  enum EFFECTTYPE { simpleEffect, groupEffect };
}

class EffectsList: public QDomDocument
{
public:
    EffectsList(bool indexRequired = false);
    ~EffectsList();

    /** @brief Returns the XML element of an effect.
     * @param name name of the effect to be returned */
    QDomElement getEffectByName(const QString & name) const;
    QDomElement getEffectByTag(const QString & tag, const QString & id) const;

    /** @brief Checks the existance of an effect.
     * @param tag effect tag
     * @param id effect id
     * @return effect index if the effect exists, -1 otherwise */
    int hasEffect(const QString & tag, const QString & id) const;

    /** @brief Lists the core properties of an effect.
     * @param ix effect index
     * @return list of name, tag and id of an effect */
    QStringList effectIdInfo(const int ix) const;

    /** @brief Lists effects names. */
    QStringList effectNames();
    QString getInfo(const QString & tag, const QString & id) const;
    QString getInfoFromIndex(const int ix) const;
    QString getEffectInfo(const QDomElement effect) const;
    void clone(const EffectsList &original);
    QDomElement append(QDomElement e);
    bool isEmpty() const;
    int count() const;
    const QDomElement at(int ix) const;
    void removeAt(int ix);
    QDomElement itemFromIndex(int ix) const;
    QDomElement insert(QDomElement effect);
    void updateEffect(QDomElement effect);
    static bool hasKeyFrames(QDomElement effect);
    static bool hasSimpleKeyFrames(QDomElement effect);
    static bool hasGeometryKeyFrames(QDomElement effect);
    static void setParameter(QDomElement effect, const QString &name, const QString &value);
    static QString parameter(QDomElement effect, const QString &name);
    /** @brief Change the value of a 'property' element from the effect node. */
    static void setProperty(QDomElement effect, const QString &name, const QString &value);
    /** @brief Rename a 'property' element from the effect node. */
    static void renameProperty(QDomElement effect, const QString &oldName, const QString &newName);
    /** @brief Get the value of a 'property' element from the effect node. */
    static QString property(QDomElement effect, const QString &name);
    /** @brief Delete a 'property' element from the effect node. */
    static void removeProperty(QDomElement effect, const QString &name);
    /** @brief Remove all 'meta.*' properties from a producer, used when replacing proxy producers in xml for rendering. */
    static void removeMetaProperties(QDomElement producer);
    void clearList();
    /** @brief Get am effect with effect index equal to ix. */
    QDomElement effectFromIndex(QDomNodeList effects, int ix);
    /** @brief Update all effects indexes to make sure they are 1, 2, 3, ... */
    void updateIndexes(QDomNodeList effects, int startIndex);
    /** @brief Enable / disable a list of effects */
    void enableEffects(QList <int> indexes, bool disable);

private:
    QDomElement m_baseElement;
    bool m_useIndex;
    
    /** @brief Init effect default parameter values. */
    void initEffect(const QDomElement &effect) const;

};

#endif
