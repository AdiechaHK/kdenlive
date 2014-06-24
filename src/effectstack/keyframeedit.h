/***************************************************************************
                          effectstack/keyframeedit.h  -  description
                             -------------------
    begin                : 22 Jun 2009
    copyright            : (C) 2008 by Jean-Baptiste Mardelle
    email                : jb@kdenlive.org
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef KEYFRAMEEDIT_H
#define KEYFRAMEEDIT_H


#include <QWidget>
#include <QDomElement>
#include <QItemDelegate>
#include <QAbstractItemView>
#include <QSpinBox>

class PositionEdit;

#include "ui_keyframeeditor_ui.h"
#include "definitions.h"
#include "effectstack/keyframehelper.h"

class KeyItemDelegate: public QItemDelegate
{
    Q_OBJECT
public:
    KeyItemDelegate(int min, int max, QAbstractItemView* parent = 0)
        : QItemDelegate(parent), m_min(min), m_max(max) {
    }

    QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const {
        if (index.column() == 1) {
            QSpinBox *spin = new QSpinBox(parent);
            connect(spin, SIGNAL(valueChanged(int)), this, SLOT(commitEditorData()));
            connect(spin, SIGNAL(editingFinished()), this, SLOT(commitAndCloseEditor()));
            return spin;
        } else {
            return QItemDelegate::createEditor(parent, option, index);
        }
    }


    void setEditorData(QWidget *editor, const QModelIndex &index) const {
        if (index.column() == 1) {
            QSpinBox *spin = qobject_cast< QSpinBox* >(editor);
            spin->setRange(m_min, m_max);
            spin->setValue(index.model()->data(index).toInt());
        } else {
            QItemDelegate::setEditorData(editor, index);
        }
    }

private slots:
    void commitAndCloseEditor() {
        QSpinBox *spin = qobject_cast< QSpinBox* >(sender());
        emit closeEditor(spin);
    }

    void commitEditorData() {
        QSpinBox *spin = qobject_cast< QSpinBox* >(sender());
        emit commitData(spin);
    }

private:
    int m_min;
    int m_max;
};

class KeyframeEdit : public QWidget, public Ui::KeyframeEditor_UI
{
    Q_OBJECT
public:
    explicit KeyframeEdit(const QDomElement &e, int minFrame, int maxFrame, const Timecode &tc, int activeKeyframe, QWidget* parent = 0);
    virtual ~KeyframeEdit();
    virtual void addParameter(const QDomElement &e, int activeKeyframe = -1);
    const QString getValue(const QString &name);
    /** @brief Updates the timecode display according to settings (frame number or hh:mm:ss:ff) */
    void updateTimecodeFormat();

    /** @brief Returns true if the parameter @param name should be shown on the clip in timeline. */
    bool isVisibleParam(const QString &name);

    /** @brief Makes the first parameter visible in timeline if no parameter is selected. */
    void checkVisibleParam();

public slots:

    void slotUpdateRange(int inPoint, int outPoint);

protected:
    /** @brief Gets the position of a keyframe from the table.
     * @param row Row of the keyframe in the table */
    int getPos(int row);
    /** @brief Converts a frame value to timecode considering the frames vs. HH:MM:SS:FF setting.
     * @return timecode */
    QString getPosString(int pos);

    void generateAllParams();

    int m_min;
    int m_max;

protected slots:
    void slotAdjustKeyframeInfo(bool seek = true);

private:
    QList <QDomElement> m_params;
    Timecode m_timecode;
    QGridLayout *m_slidersLayout;
    PositionEdit *m_position;

private slots:
    void slotDeleteKeyframe();
    void slotAddKeyframe();
    void slotGenerateParams(int row, int column);
    void slotAdjustKeyframePos(int value);
    void slotAdjustKeyframeValue(double value);
    /** @brief Turns the seek to keyframe position setting on/off.
    * @param seek true = seeking on */
    void slotSetSeeking(bool seek);

    /** @brief Shows the keyframe table and adds a second keyframe. */
    void slotKeyframeMode();

    /** @brief Resets all parameters of the selected keyframe to their default values. */
    void slotResetKeyframe();

    /** @brief Makes the parameter at column @param id the visible (in timeline) one. */
    void slotUpdateVisibleParameter(int id, bool update = true);

signals:
    void parameterChanged();
    void seekToPos(int);
    void showComments(bool show);
};

#endif
