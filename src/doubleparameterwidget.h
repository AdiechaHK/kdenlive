/***************************************************************************
 *   Copyright (C) 2010 by Till Theato (root@ttill.de)                     *
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


#ifndef DOUBLEPARAMETERWIDGET_H
#define DOUBLEPARAMETERWIDGET_H

#include <QWidget>


class QLabel;
class DragValue;

/**
 * @class DoubleParameterWidget
 * @brief Widget to choose a double parameter (for a effect) with the help of a slider and a spinbox.
 * @author Till Theato
 *
 * The widget does only handle integers, so the parameter has to be converted into the proper double range afterwards.
 */

class DoubleParameterWidget : public QWidget
{
    Q_OBJECT
public:
    /** @brief Sets up the parameter's GUI.
    * @param name Name of the parameter
    * @param value Value of the parameter
    * @param min Minimum value
    * @param max maximum value
    * @param defaultValue Value used when using reset functionality
    * @param comment A comment explaining the parameter. Will be shown for the tooltip.
    * @param suffix (optional) Suffix to display in spinbox
    * @param parent (optional) Parent Widget */
    explicit DoubleParameterWidget(const QString &name, double value, double min, double max, double defaultValue, const QString &comment, int id, const QString &suffix = QString(), int decimals = 0, QWidget* parent = 0);
    ~DoubleParameterWidget();

    /** @brief Gets the parameter's value. */
    double getValue();
    /** @brief Set the inTimeline property to paint widget with other colors. */
    void setInTimelineProperty(bool intimeline);
    /** @brief Returns minimum size for QSpinBox, used to set all spinboxes to the same width. */
    int spinSize();
    void setSpinSize(int width);
    

public slots:
    /** @brief Sets the value to @param value. */
    void setValue(double value);

    /** @brief Sets value to m_default. */
    void slotReset();

private slots:
    /** @brief Shows/Hides the comment label. */
    void slotShowComment(bool show);

    void slotSetValue(double value, bool final);

private:
    DragValue *m_dragVal;
    QLabel *m_commentLabel;
    
signals:
    void valueChanged(double);
    /** @brief User wants to see this parameter in timeline. */
    void setInTimeline(int);
};

#endif
