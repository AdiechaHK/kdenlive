/***************************************************************************
 *   Copyright (C) 2008 by Jean-Baptiste Mardelle (jb@kdenlive.org)        *
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


#ifndef DOCUMENTCHECKER_H
#define DOCUMENTCHECKER_H

#include "ui_missingclips_ui.h"

#include <KUrl>

#include <QDir>
#include <QDomElement>


class DocumentChecker: public QObject
{
    Q_OBJECT

public:
    explicit DocumentChecker(const QDomNodeList &infoproducers, const QDomDocument &doc);
    ~DocumentChecker();
    bool hasErrorInClips();

private slots:
    void acceptDialog();
    void slotSearchClips();
    void slotEditItem(QTreeWidgetItem *item, int);
    void slotPlaceholders();
    void slotDeleteSelected();
    QString getProperty(QDomElement effect, const QString &name);
    void setProperty(QDomElement effect, const QString &name, const QString value);
    QString searchLuma(const QDir &dir, const QString &file) const;
    /** @brief Check if images and fonts in this clip exists, returns a list of images that do exist so we don't check twice. */
    void checkMissingImagesAndFonts(QStringList images, QStringList fonts, const QString &id, const QString &baseClip);
    void slotCheckButtons();
    /** @brief Fix duration mismatch issues. */
    void slotFixDuration();

private:
    QDomNodeList m_info;
    QDomDocument m_doc;
    Ui::MissingClips_UI m_ui;
    QDialog *m_dialog;
    QString searchPathRecursively(const QDir &dir, const QString &fileName) const;
    QString searchFileRecursively(const QDir &dir, const QString &matchSize, const QString &matchHash) const;
    void checkStatus();
    QMap <QString, QString> m_missingTitleImages;
    QMap <QString, QString> m_missingTitleFonts;
    QList <QDomElement> m_missingClips;
    QStringList m_safeImages;
    QStringList m_safeFonts;
    
    void fixClipItem(QTreeWidgetItem *child, QDomNodeList producers, QDomNodeList infoproducers, QDomNodeList trans);
};


#endif

