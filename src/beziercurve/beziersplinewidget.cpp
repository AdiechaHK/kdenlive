/***************************************************************************
 *   Copyright (C) 2010 by Till Theato (root@ttill.de)                     *
 *   This file is part of Kdenlive (www.kdenlive.org).                     *
 *                                                                         *
 *   Kdenlive is free software: you can redistribute it and/or modify      *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation, either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   Kdenlive is distributed in the hope that it will be useful,           *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with Kdenlive.  If not, see <http://www.gnu.org/licenses/>.     *
 ***************************************************************************/

#include "beziersplinewidget.h"
#include "colortools.h"
#include "effectstack/dragvalue.h"
#include "kdenlivesettings.h"

#include <QVBoxLayout>

#include <KIcon>
#include <KLocalizedString>


BezierSplineWidget::BezierSplineWidget(const QString& spline, QWidget* parent) :
        QWidget(parent),
        m_mode(ModeRGB),
        m_showPixmap(true)
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->addWidget(&m_edit);
    QWidget *widget = new QWidget(this);
    m_ui.setupUi(widget);
    layout->addWidget(widget);

    m_ui.buttonLinkHandles->setIcon(KIcon(QLatin1String("insert-link")));
    m_ui.buttonZoomIn->setIcon(KIcon(QLatin1String("zoom-in")));
    m_ui.buttonZoomOut->setIcon(KIcon(QLatin1String("zoom-out")));
    m_ui.buttonGridChange->setIcon(KIcon(QLatin1String("view-grid")));
    m_ui.buttonShowPixmap->setIcon(QIcon(QPixmap::fromImage(ColorTools::rgbCurvePlane(QSize(16, 16), ColorTools::COL_Luma, 0.8))));
    m_ui.buttonResetSpline->setIcon(KIcon(QLatin1String("view-refresh")));
    m_ui.buttonShowAllHandles->setIcon(KIcon(QLatin1String("draw-bezier-curves")));
    m_ui.widgetPoint->setEnabled(false);

    m_pX = new DragValue(i18n("In"), 0, 3, 0, 1, -1, QString(), false, this);
    m_pX->setStep(0.001);
    m_pY = new DragValue(i18n("Out"), 0, 3, 0, 1, -1, QString(), false, this);
    m_pY->setStep(0.001);
    m_h1X = new DragValue(i18n("X"), 0, 3, -2, 2, -1, QString(), false, this);
    m_h1X->setStep(0.001);
    m_h1Y = new DragValue(i18n("Y"), 0, 3, -2, 2, -1, QString(), false, this);
    m_h1Y->setStep(0.001);
    m_h2X = new DragValue(i18n("X"), 0, 3, -2, 2, -1, QString(), false, this);
    m_h2X->setStep(0.001);
    m_h2Y = new DragValue(i18n("Y"), 0, 3, -2, 2, -1, QString(), false, this);
    m_h2Y->setStep(0.001);

    m_ui.layoutP->addWidget(m_pX);
    m_ui.layoutP->addWidget(m_pY);
    m_ui.layoutH1->addWidget(new QLabel(i18n("Handle 1:")));
    m_ui.layoutH1->addWidget(m_h1X);
    m_ui.layoutH1->addWidget(m_h1Y);
    m_ui.layoutH2->addWidget(new QLabel(i18n("Handle 2:")));
    m_ui.layoutH2->addWidget(m_h2X);
    m_ui.layoutH2->addWidget(m_h2Y);

    CubicBezierSpline s;
    s.fromString(spline);
    m_edit.setSpline(s);

    connect(&m_edit, SIGNAL(modified()), this, SIGNAL(modified()));
    connect(&m_edit, SIGNAL(currentPoint(BPoint)), this, SLOT(slotUpdatePointEntries(BPoint)));

    connect(m_pX, SIGNAL(valueChanged(double,bool)), this, SLOT(slotUpdatePointP(double,bool)));
    connect(m_pY, SIGNAL(valueChanged(double,bool)), this, SLOT(slotUpdatePointP(double,bool)));
    connect(m_h1X, SIGNAL(valueChanged(double,bool)), this, SLOT(slotUpdatePointH1(double,bool)));
    connect(m_h1Y, SIGNAL(valueChanged(double,bool)), this, SLOT(slotUpdatePointH1(double,bool)));
    connect(m_h2X, SIGNAL(valueChanged(double,bool)), this, SLOT(slotUpdatePointH2(double,bool)));
    connect(m_h2Y, SIGNAL(valueChanged(double,bool)), this, SLOT(slotUpdatePointH2(double,bool)));

    connect(m_ui.buttonLinkHandles, SIGNAL(toggled(bool)), this, SLOT(slotSetHandlesLinked(bool)));
    connect(m_ui.buttonZoomIn, SIGNAL(clicked()), &m_edit, SLOT(slotZoomIn()));
    connect(m_ui.buttonZoomOut, SIGNAL(clicked()), &m_edit, SLOT(slotZoomOut()));
    connect(m_ui.buttonGridChange, SIGNAL(clicked()), this, SLOT(slotGridChange()));
    connect(m_ui.buttonShowPixmap, SIGNAL(toggled(bool)), this, SLOT(slotShowPixmap(bool)));
    connect(m_ui.buttonResetSpline, SIGNAL(clicked()), this, SLOT(slotResetSpline()));
    connect(m_ui.buttonShowAllHandles, SIGNAL(toggled(bool)), this, SLOT(slotShowAllHandles(bool)));

    m_edit.setGridLines(KdenliveSettings::bezier_gridlines());
    m_ui.buttonShowPixmap->setChecked(KdenliveSettings::bezier_showpixmap());
    slotShowPixmap(m_ui.buttonShowPixmap->isChecked());
    m_ui.buttonShowAllHandles->setChecked(KdenliveSettings::bezier_showallhandles());
}

QString BezierSplineWidget::spline() const
{
    return m_edit.spline().toString();
}

void BezierSplineWidget::setMode(BezierSplineWidget::CurveModes mode)
{
    if (m_mode != mode) {
        m_mode = mode;
        if (m_showPixmap)
            slotShowPixmap();
    }
}

void BezierSplineWidget::slotGridChange()
{
    m_edit.setGridLines((m_edit.gridLines() + 1) % 9);
    KdenliveSettings::setBezier_gridlines(m_edit.gridLines());
}

void BezierSplineWidget::slotShowPixmap(bool show)
{
    if (m_showPixmap != show) {
        m_showPixmap = show;
        KdenliveSettings::setBezier_showpixmap(show);
        if (show && (int)m_mode < 6)
            m_edit.setPixmap(QPixmap::fromImage(ColorTools::rgbCurvePlane(m_edit.size(), static_cast<ColorTools::ColorsRGB>(m_mode), 1, palette().background().color().rgb())));
        else if (show && m_mode == ModeHue)
            m_edit.setPixmap(QPixmap::fromImage(ColorTools::hsvCurvePlane(m_edit.size(), QColor::fromHsv(200, 200, 200), ColorTools::COM_H, ColorTools::COM_H)));
        else
            m_edit.setPixmap(QPixmap());
    }
}

void BezierSplineWidget::slotUpdatePointEntries(const BPoint &p)
{
    blockSignals(true);
    if (p == BPoint()) {
        m_ui.widgetPoint->setEnabled(false);
    } else {
        m_ui.widgetPoint->setEnabled(true);
        m_pX->blockSignals(true);
        m_pY->blockSignals(true);
        m_h1X->blockSignals(true);
        m_h1Y->blockSignals(true);
        m_h2X->blockSignals(true);
        m_h2Y->blockSignals(true);
        m_pX->setValue(p.p.x());
        m_pY->setValue(p.p.y());
        m_h1X->setValue(p.h1.x());
        m_h1Y->setValue(p.h1.y());
        m_h2X->setValue(p.h2.x());
        m_h2Y->setValue(p.h2.y());
        m_pX->blockSignals(false);
        m_pY->blockSignals(false);
        m_h1X->blockSignals(false);
        m_h1Y->blockSignals(false);
        m_h2X->blockSignals(false);
        m_h2Y->blockSignals(false);
        m_ui.buttonLinkHandles->setChecked(p.handlesLinked);
    }
    blockSignals(false);
}

void BezierSplineWidget::slotUpdatePointP(double value, bool final)
{
    Q_UNUSED(value)

    BPoint p = m_edit.getCurrentPoint();

    p.setP(QPointF(m_pX->value(), m_pY->value()));

    m_edit.updateCurrentPoint(p, final);
}

void BezierSplineWidget::slotUpdatePointH1(double value, bool final)
{
    Q_UNUSED(value)

    BPoint p = m_edit.getCurrentPoint();

    p.setH1(QPointF(m_h1X->value(), m_h1Y->value()));

    m_edit.updateCurrentPoint(p, final);
}

void BezierSplineWidget::slotUpdatePointH2(double value, bool final)
{
    Q_UNUSED(value)

    BPoint p = m_edit.getCurrentPoint();

    p.setH2(QPointF(m_h2X->value(), m_h2Y->value()));

    m_edit.updateCurrentPoint(p, final);
}

void BezierSplineWidget::slotSetHandlesLinked(bool linked)
{
    BPoint p = m_edit.getCurrentPoint();
    p.handlesLinked = linked;
    m_edit.updateCurrentPoint(p);
}

void BezierSplineWidget::slotResetSpline()
{
    m_edit.setSpline(CubicBezierSpline());
}

void BezierSplineWidget::slotShowAllHandles(bool show)
{
    m_edit.setShowAllHandles(show);
    KdenliveSettings::setBezier_showallhandles(show);
}

#include "beziersplinewidget.moc"
