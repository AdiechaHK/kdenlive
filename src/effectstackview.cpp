/***************************************************************************
                          effecstackview.cpp  -  description
                             -------------------
    begin                : Feb 15 2008
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


#include "effectstackview.h"
#include "effectslist.h"
#include "clipitem.h"
#include "mainwindow.h"
#include "docclipbase.h"
#include "projectlist.h"
#include "kthumb.h"
#include "monitoreditwidget.h"
#include "monitorscene.h"
#include "core/kdenlivesettings.h"

#include <KDebug>
#include <KLocale>
#include <KMessageBox>
#include <KStandardDirs>
#include <KFileDialog>

#include <QMenu>
#include <QTextStream>
#include <QFile>
#include <QInputDialog>


EffectStackView::EffectStackView(Monitor *monitor, QWidget *parent) :
        QWidget(parent),
        m_monitor(monitor),
        m_clipref(NULL),
        m_trackMode(false),
        m_trackindex(-1)
{
    m_ui.setupUi(this);
    QVBoxLayout *vbox1 = new QVBoxLayout(m_ui.frame);
    //m_effectedit = new EffectStackEdit(monitor, m_ui.frame);
    m_ui.frame->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Preferred);
    vbox1->setContentsMargins(2, 0, 2, 0);
    vbox1->setSpacing(0);
    vbox1->addWidget(m_effectedit);
    m_ui.splitter->setStretchFactor(0, 1);
    m_ui.splitter->setStretchFactor(1, 20);

    //m_ui.region_url->fileDialog()->setFilter(ProjectList::getExtensions());
    //m_ui.effectlist->horizontalHeader()->setVisible(false);
    //m_ui.effectlist->verticalHeader()->setVisible(false);
    int size = style()->pixelMetric(QStyle::PM_SmallIconSize);
    QSize iconSize(size, size);

    m_ui.buttonNew->setIcon(KIcon("document-new"));
    m_ui.buttonNew->setToolTip(i18n("Add new effect"));
    m_ui.buttonNew->setIconSize(iconSize);
    m_ui.buttonUp->setIcon(KIcon("go-up"));
    m_ui.buttonUp->setToolTip(i18n("Move effect up"));
    m_ui.buttonUp->setIconSize(iconSize);
    m_ui.buttonDown->setIcon(KIcon("go-down"));
    m_ui.buttonDown->setToolTip(i18n("Move effect down"));
    m_ui.buttonDown->setIconSize(iconSize);
    m_ui.buttonDel->setIcon(KIcon("edit-delete"));
    m_ui.buttonDel->setToolTip(i18n("Delete effect"));
    m_ui.buttonDel->setIconSize(iconSize);
    m_ui.buttonSave->setIcon(KIcon("document-save"));
    m_ui.buttonSave->setToolTip(i18n("Save effect"));
    m_ui.buttonSave->setIconSize(iconSize);
    m_ui.buttonReset->setIcon(KIcon("view-refresh"));
    m_ui.buttonReset->setToolTip(i18n("Reset effect"));
    m_ui.buttonReset->setIconSize(iconSize);
    m_ui.checkAll->setToolTip(i18n("Enable/Disable all effects"));
    m_ui.buttonShowComments->setIcon(KIcon("help-about"));
    m_ui.buttonShowComments->setToolTip(i18n("Show additional information for the parameters"));

    m_ui.effectlist->setDragDropMode(QAbstractItemView::NoDragDrop); //use internal if drop is recognised right

    m_ui.labelComment->setHidden(true);

    //connect(m_ui.region_url, SIGNAL(urlSelected(const KUrl &)), this , SLOT(slotRegionChanged()));
    //connect(m_ui.region_url, SIGNAL(returnPressed()), this , SLOT(slotRegionChanged()));
    connect(m_ui.effectlist, SIGNAL(itemSelectionChanged()), this , SLOT(slotItemSelectionChanged()));
    connect(m_ui.effectlist, SIGNAL(itemChanged(QListWidgetItem *)), this , SLOT(slotItemChanged(QListWidgetItem *)));
    connect(m_ui.buttonUp, SIGNAL(clicked()), this, SLOT(slotItemUp()));
    connect(m_ui.buttonDown, SIGNAL(clicked()), this, SLOT(slotItemDown()));
    connect(m_ui.buttonDel, SIGNAL(clicked()), this, SLOT(slotItemDel()));
    connect(m_ui.buttonSave, SIGNAL(clicked()), this, SLOT(slotSaveEffect()));
    connect(m_ui.buttonReset, SIGNAL(clicked()), this, SLOT(slotResetEffect()));
    connect(m_ui.checkAll, SIGNAL(stateChanged(int)), this, SLOT(slotCheckAll(int)));
    connect(m_ui.buttonShowComments, SIGNAL(clicked()), this, SLOT(slotShowComments()));
    connect(m_effectedit, SIGNAL(parameterChanged(const QDomElement &, const QDomElement &)), this , SLOT(slotUpdateEffectParams(const QDomElement &, const QDomElement &)));
    connect(m_effectedit, SIGNAL(startFilterJob(QString,QString,QString,QString,QString,QString)), this , SLOT(slotStartFilterJob(QString,QString,QString,QString,QString,QString)));
    connect(m_effectedit, SIGNAL(seekTimeline(int)), this , SLOT(slotSeekTimeline(int)));
    connect(m_effectedit, SIGNAL(displayMessage(const QString&, int)), this, SIGNAL(displayMessage(const QString&, int)));
    connect(m_effectedit, SIGNAL(checkMonitorPosition(int)), this, SLOT(slotCheckMonitorPosition(int)));
    
    connect(monitor, SIGNAL(renderPosition(int)), this, SLOT(slotRenderPos(int)));
    connect(this, SIGNAL(showComments(bool)), m_effectedit, SIGNAL(showComments(bool)));
    m_effectLists["audio"] = &MainWindow::audioEffects;
    m_effectLists["video"] = &MainWindow::videoEffects;
    m_effectLists["custom"] = &MainWindow::customEffects;
    setEnabled(false);
}

EffectStackView::~EffectStackView()
{
    m_effectLists.clear();
    delete m_effectedit;
}

void EffectStackView::updateTimecodeFormat()
{
    m_effectedit->updateTimecodeFormat();
}

void EffectStackView::setMenu(QMenu *menu)
{
    m_ui.buttonNew->setMenu(menu);
}

void EffectStackView::updateProjectFormat(MltVideoProfile profile, Timecode t)
{
    m_effectedit->updateProjectFormat(profile, t);
}

void EffectStackView::slotSaveEffect()
{
    QString name = QInputDialog::getText(this, i18n("Save Effect"), i18n("Name for saved effect: "));
    if (name.isEmpty()) return;
    QString path = KStandardDirs::locateLocal("appdata", "effects/", true);
    path = path + name + ".xml";
    if (QFile::exists(path)) if (KMessageBox::questionYesNo(this, i18n("File %1 already exists.\nDo you want to overwrite it?", path)) == KMessageBox::No) return;

    int i = m_ui.effectlist->currentRow();
    QDomDocument doc;
    QDomElement effect = m_currentEffectList.at(i).cloneNode().toElement();
    doc.appendChild(doc.importNode(effect, true));
    effect = doc.firstChild().toElement();
    effect.removeAttribute("kdenlive_ix");
    effect.setAttribute("id", name);
    effect.setAttribute("type", "custom");
    QDomElement effectname = effect.firstChildElement("name");
    effect.removeChild(effectname);
    effectname = doc.createElement("name");
    QDomText nametext = doc.createTextNode(name);
    effectname.appendChild(nametext);
    effect.insertBefore(effectname, QDomNode());
    QDomElement effectprops = effect.firstChildElement("properties");
    effectprops.setAttribute("id", name);
    effectprops.setAttribute("type", "custom");

    QFile file(path);
    if (file.open(QFile::WriteOnly | QFile::Truncate)) {
        QTextStream out(&file);
        out << doc.toString();
    }
    file.close();
    emit reloadEffects();
}

void EffectStackView::slotUpdateEffectParams(const QDomElement &old, const QDomElement &e)
{
    if (m_trackMode)
        emit updateEffect(NULL, m_trackindex, old, e, m_ui.effectlist->currentRow());
    else if (m_clipref)
        emit updateEffect(m_clipref, -1, old, e, m_ui.effectlist->currentRow());
}

void EffectStackView::slotClipItemSelected(ClipItem* c, int ix)
{
    if (c && !c->isEnabled()) return;
    if (c && c == m_clipref) {
        if (ix == -1) ix = m_ui.effectlist->currentRow();
        //if (ix == -1 || ix == m_ui.effectlist->currentRow()) return;
    } else {
        m_clipref = c;
        if (c) {
            QString cname = m_clipref->clipName();
            if (cname.length() > 30) {
                m_ui.checkAll->setToolTip(i18n("Effects for %1").arg(cname));
                cname.truncate(27);
                m_ui.checkAll->setText(i18n("Effects for %1").arg(cname) + "...");
            } else {
                m_ui.checkAll->setToolTip(QString());
                m_ui.checkAll->setText(i18n("Effects for %1").arg(cname));
            }
            ix = c->selectedEffectIndex();
            QString size = c->baseClip()->getProperty("frame_size");
            double factor = c->baseClip()->getProperty("aspect_ratio").toDouble();
            QPoint p((int)(size.section('x', 0, 0).toInt() * factor + 0.5), size.section('x', 1, 1).toInt());
            m_effectedit->setFrameSize(p);
        } else {
            ix = 0;
        }
    }
    if (m_clipref == NULL) {
        m_ui.effectlist->blockSignals(true);
        m_ui.effectlist->clear();
        ItemInfo info;
        m_effectedit->transferParamDesc(QDomElement(), info);
        //m_ui.region_url->clear();
        m_ui.effectlist->blockSignals(false);
        m_ui.checkAll->setToolTip(QString());
        m_ui.checkAll->setText(QString());
        m_ui.labelComment->setText(QString());
        setEnabled(false);
        return;
    }
    setEnabled(true);
    m_trackMode = false;
    m_currentEffectList = m_clipref->effectList();
    setupListView(ix);
}

void EffectStackView::slotTrackItemSelected(int ix, const TrackInfo info)
{
    m_clipref = NULL;
    m_trackMode = true;
    m_currentEffectList = info.effectsList;
    m_trackInfo = info;
    kDebug() << "// TRACK; " << ix << ", EFFECTS: " << m_currentEffectList.count();
    setEnabled(true);
    m_ui.checkAll->setToolTip(QString());
    m_ui.checkAll->setText(i18n("Effects for track %1").arg(info.trackName.isEmpty() ? QString::number(ix) : info.trackName));
    m_trackindex = ix;
    setupListView(0);
}

void EffectStackView::slotItemChanged(QListWidgetItem *item)
{
    bool disable = item->checkState() == Qt::Unchecked;
    int row = m_ui.effectlist->row(item);
    int activeRow = m_ui.effectlist->currentRow();

    if (row == activeRow) {
        m_ui.buttonReset->setEnabled(!disable || !KdenliveSettings::disable_effect_parameters());
        m_effectedit->updateParameter("disable", QString::number((int) disable));
    }

    if (m_trackMode)
        emit changeEffectState(NULL, m_trackindex, row, disable);
    else
        emit changeEffectState(m_clipref, -1, row, disable);

    slotUpdateCheckAllButton();
}


void EffectStackView::setupListView(int ix)
{
    m_ui.effectlist->blockSignals(true);
    m_ui.effectlist->clear();

    // Issue 238: Add icons for effect type in effectstack.
    KIcon videoIcon("kdenlive-show-video");
    KIcon audioIcon("kdenlive-show-audio");
    KIcon customIcon("kdenlive-custom-effect");
    QListWidgetItem* item;

    for (int i = 0; i < m_currentEffectList.count(); ++i) {
        const QDomElement d = m_currentEffectList.at(i).cloneNode().toElement();
        if (d.isNull()) {
            kDebug() << " . . . . WARNING, NULL EFFECT IN STACK!!!!!!!!!";
            continue;
        }

        /*QDomDocument doc;
        doc.appendChild(doc.importNode(d, true));
        kDebug() << "IMPORTED STK: " << doc.toString();*/

        QDomElement namenode = d.firstChildElement("name");
        if (!namenode.isNull()) {
            // Issue 238: Add icons for effect type in effectstack.
            // Logic more or less copied from initeffects.cpp
            QString type = d.attribute("type", QString());
            if ("audio" == type) {
                item = new QListWidgetItem(audioIcon, i18n(namenode.text().toUtf8().data()), m_ui.effectlist);
            } else if ("custom" == type) {
                item = new QListWidgetItem(customIcon, i18n(namenode.text().toUtf8().data()), m_ui.effectlist);
            } else {
                item = new QListWidgetItem(videoIcon, i18n(namenode.text().toUtf8().data()), m_ui.effectlist);
            }
            item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsDragEnabled | Qt::ItemIsUserCheckable | Qt::ItemIsEnabled);
            if (d.attribute("disable") == "1")
                item->setCheckState(Qt::Unchecked);
            else
                item->setCheckState(Qt::Checked);
        }
    }
    if (m_ui.effectlist->count() == 0) {
        m_ui.buttonDel->setEnabled(false);
        m_ui.buttonSave->setEnabled(false);
        m_ui.buttonReset->setEnabled(false);
        m_ui.buttonUp->setEnabled(false);
        m_ui.buttonDown->setEnabled(false);
        m_ui.checkAll->setEnabled(false);
        m_ui.buttonShowComments->setEnabled(false);
        m_ui.labelComment->setHidden(true);
    } else {
        ix = qBound(0, ix, m_ui.effectlist->count() - 1);
        m_ui.effectlist->setCurrentRow(ix);
        m_ui.checkAll->setEnabled(true);
    }
    m_ui.effectlist->blockSignals(false);
    if (m_ui.effectlist->count() == 0) {
        ItemInfo info;
        m_effectedit->transferParamDesc(QDomElement(), info);
        //m_ui.region_url->clear();
    } else slotItemSelectionChanged(false);
    slotUpdateCheckAllButton();
}

void EffectStackView::slotItemSelectionChanged(bool update)
{
    bool hasItem = m_ui.effectlist->currentItem();
    int activeRow = m_ui.effectlist->currentRow();
    bool isChecked = false;
    if (hasItem && m_ui.effectlist->currentItem()->checkState() == Qt::Checked) isChecked = true;
    QDomElement eff;
    if (hasItem && m_ui.effectlist->currentItem()->isSelected()) {
        eff = m_currentEffectList.at(activeRow);
        if (m_trackMode) {
            // showing track effects
            ItemInfo info;
            info.track = m_trackInfo.type;
            info.cropDuration = GenTime(m_trackInfo.duration, KdenliveSettings::project_fps());
            info.cropStart = GenTime(0);
            info.startPos = GenTime(-1);
            info.track = 0;
            m_effectedit->transferParamDesc(eff, info);
        } else {
            m_effectedit->transferParamDesc(eff, m_clipref->info());
        }
        //m_ui.region_url->setUrl(KUrl(eff.attribute("region")));
        m_ui.labelComment->setText(i18n(eff.firstChildElement("description").firstChildElement("full").text().toUtf8().data()));
    }
    if (!m_trackMode && m_clipref && update) m_clipref->setSelectedEffect(activeRow);
    m_ui.buttonDel->setEnabled(hasItem);
    m_ui.buttonSave->setEnabled(hasItem);
    m_ui.buttonReset->setEnabled(hasItem && (isChecked || !KdenliveSettings::disable_effect_parameters()));
    m_ui.buttonUp->setEnabled(activeRow > 0);
    m_ui.buttonDown->setEnabled((activeRow < m_ui.effectlist->count() - 1) && hasItem);
    m_ui.buttonShowComments->setEnabled(hasItem);

    emit showComments(m_ui.buttonShowComments->isChecked());
    m_ui.labelComment->setVisible(hasItem && m_ui.labelComment->text().count() && (m_ui.buttonShowComments->isChecked() || !eff.elementsByTagName("parameter").count()));
}

void EffectStackView::slotItemUp()
{
    int activeRow = m_ui.effectlist->currentRow();
    if (activeRow <= 0) return;
    if (m_trackMode) emit changeEffectPosition(NULL, m_trackindex, activeRow + 1, activeRow);
    else emit changeEffectPosition(m_clipref, -1, activeRow + 1, activeRow);
}

void EffectStackView::slotItemDown()
{
    int activeRow = m_ui.effectlist->currentRow();
    if (activeRow >= m_ui.effectlist->count() - 1) return;
    if (m_trackMode) emit changeEffectPosition(NULL, m_trackindex, activeRow + 1, activeRow + 2);
    else emit changeEffectPosition(m_clipref, -1, activeRow + 1, activeRow + 2);
}

void EffectStackView::slotItemDel()
{
    int activeRow = m_ui.effectlist->currentRow();
    if (activeRow >= 0) {
        if (m_trackMode)
            emit removeEffect(NULL, m_trackindex, m_currentEffectList.at(activeRow).cloneNode().toElement());
        else
            emit removeEffect(m_clipref, -1, m_clipref->effectAt(activeRow));
        slotUpdateCheckAllButton();
    }
}

void EffectStackView::slotResetEffect()
{
    int activeRow = m_ui.effectlist->currentRow();
    if (activeRow < 0) return;
    QDomElement old = m_currentEffectList.at(activeRow).cloneNode().toElement();
    QDomElement dom;
    QString effectName = m_ui.effectlist->currentItem()->text();
    foreach(const QString &type, m_effectLists.keys()) {
        EffectsList *list = m_effectLists[type];
        if (list->effectNames().contains(effectName)) {
            dom = list->getEffectByName(effectName).cloneNode().toElement();
            break;
        }
    }
    if (!dom.isNull()) {
        dom.setAttribute("kdenlive_ix", old.attribute("kdenlive_ix"));
        if (m_trackMode) {
            EffectsList::setParameter(dom, "in", QString::number(0));
            EffectsList::setParameter(dom, "out", QString::number(m_trackInfo.duration));
            ItemInfo info;
            info.track = m_trackInfo.type;
            info.cropDuration = GenTime(m_trackInfo.duration, KdenliveSettings::project_fps());
            info.cropStart = GenTime(0);
            info.startPos = GenTime(-1);
            info.track = 0;
            m_effectedit->transferParamDesc(dom, info);
            emit updateEffect(NULL, m_trackindex, old, dom, activeRow);
        } else {
            m_clipref->initEffect(dom);
            m_effectedit->transferParamDesc(dom, m_clipref->info());
            //m_ui.region_url->setUrl(KUrl(dom.attribute("region")));
            emit updateEffect(m_clipref, -1, old, dom, activeRow);
        }
    }

    emit showComments(m_ui.buttonShowComments->isChecked());
    m_ui.labelComment->setHidden(!m_ui.buttonShowComments->isChecked() || !m_ui.labelComment->text().count());
}


void EffectStackView::raiseWindow(QWidget* dock)
{
    if ((m_clipref || m_trackMode) && dock)
        dock->raise();
}

void EffectStackView::clear()
{
    m_ui.effectlist->blockSignals(true);
    m_ui.effectlist->clear();
    m_ui.buttonDel->setEnabled(false);
    m_ui.buttonSave->setEnabled(false);
    m_ui.buttonReset->setEnabled(false);
    m_ui.buttonUp->setEnabled(false);
    m_ui.buttonDown->setEnabled(false);
    m_ui.checkAll->setEnabled(false);
    ItemInfo info;
    m_effectedit->transferParamDesc(QDomElement(), info);
    //m_ui.region_url->clear();
    m_clipref = NULL;
    m_ui.buttonShowComments->setEnabled(false);
    m_ui.labelComment->setText(QString());
    m_ui.effectlist->blockSignals(false);
}


void EffectStackView::slotSeekTimeline(int pos)
{
    if (m_trackMode) {
        emit seekTimeline(pos);
    } else if (m_clipref) {
        emit seekTimeline(m_clipref->startPos().frames(KdenliveSettings::project_fps()) + pos);
    }
}

void EffectStackView::slotUpdateCheckAllButton()
{
    bool hasEnabled = false;
    bool hasDisabled = false;
    for (int i = 0; i < m_ui.effectlist->count(); ++i) {
        if (m_ui.effectlist->item(i)->checkState() == Qt::Checked)
            hasEnabled = true;
        else
            hasDisabled = true;
    }

    m_ui.checkAll->blockSignals(true);
    if (hasEnabled && hasDisabled)
        m_ui.checkAll->setCheckState(Qt::PartiallyChecked);
    else if (hasEnabled)
        m_ui.checkAll->setCheckState(Qt::Checked);
    else
        m_ui.checkAll->setCheckState(Qt::Unchecked);
    m_ui.checkAll->blockSignals(false);
}

void EffectStackView::slotCheckAll(int state)
{
    if (state == 1) {
        state = 2;
        m_ui.checkAll->blockSignals(true);
        m_ui.checkAll->setCheckState(Qt::Checked);
        m_ui.checkAll->blockSignals(false);
    }

    bool disabled = (state != 2);
    m_effectedit->updateParameter("disable", QString::number((int) disabled));
    for (int i = 0; i < m_ui.effectlist->count(); ++i) {
        if (m_ui.effectlist->item(i)->checkState() != (Qt::CheckState)state) {
            m_ui.effectlist->item(i)->setCheckState((Qt::CheckState)state);
            if (m_trackMode)
                emit changeEffectState(NULL, m_trackindex, i, disabled);
            else
                emit changeEffectState(m_clipref, -1, i, disabled);
        }
    }
}

/*void EffectStackView::slotRegionChanged()
{
    if (!m_trackMode) emit updateClipRegion(m_clipref, m_ui.effectlist->currentRow(), m_ui.region_url->text());
}*/

void EffectStackView::slotCheckMonitorPosition(int renderPos)
{
    if (m_trackMode || (m_clipref && renderPos >= m_clipref->startPos().frames(KdenliveSettings::project_fps()) && renderPos <= m_clipref->endPos().frames(KdenliveSettings::project_fps()))) {
        if (!m_monitor->getEffectEdit()->getScene()->views().at(0)->isVisible())
            m_monitor->slotShowEffectScene(true);
    } else {
        m_monitor->slotShowEffectScene(false);
    }
}

void EffectStackView::slotRenderPos(int pos)
{
    if (m_effectedit) {
        if (m_trackMode) {
            m_effectedit->slotSyncEffectsPos(pos);
        } else if (m_clipref) {
            m_effectedit->slotSyncEffectsPos(pos - m_clipref->startPos().frames(KdenliveSettings::project_fps()));
        }
    }
}

int EffectStackView::isTrackMode(bool *ok) const
{
    *ok = m_trackMode;
    return m_trackindex;
}

void EffectStackView::slotShowComments()
{
    m_ui.labelComment->setHidden(!m_ui.buttonShowComments->isChecked() || !m_ui.labelComment->text().count());
    emit showComments(m_ui.buttonShowComments->isChecked());
}

void EffectStackView::slotStartFilterJob(const QString&filterName, const QString&filterParams, const QString&finalFilterName, const QString&consumer, const QString&consumerParams, const QString&properties)
{
    if (!m_clipref) return;
    emit startFilterJob(m_clipref->info(), m_clipref->clipProducer(), filterName, filterParams, finalFilterName, consumer, consumerParams, properties);
}

#include "effectstackview.moc"
