/*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#include "projectmanager.h"
#include "core.h"
#include "mainwindow.h"
#include "kdenlivesettings.h"
#include "monitor/monitormanager.h"
#include "kdenlivedoc.h"
#include "trackview.h"
#include "projectsettings.h"
#include "projectlist.h"
#include "customtrackview.h"
#include "transitionsettings.h"
#include "widgets/archivewidget.h"
#include "effectstack/effectstackview2.h"
#include <KTabWidget>
#include <KFileDialog>
#include <KActionCollection>
#include <KAction>
#include <KMessageBox>
#include <KIO/NetAccess>
#include <KProgressDialog>
#include <QCryptographicHash>


ProjectManager::ProjectManager(QObject* parent) :
    QObject(parent),
    m_project(0)
{
    m_fileRevert = KStandardAction::revert(this, SLOT(slotRevert()), pCore->window()->actionCollection());
    m_fileRevert->setEnabled(false);

    KStandardAction::open(this,                   SLOT(openFile()),               pCore->window()->actionCollection());
    KStandardAction::saveAs(this,                 SLOT(saveFileAs()),             pCore->window()->actionCollection());
    KStandardAction::openNew(this,                SLOT(newFile()),                pCore->window()->actionCollection());
//     m_closeAction = KStandardAction::close(this,  SLOT(closeCurrentDocument()),   pCore->window()->actionCollection());
}

ProjectManager::~ProjectManager()
{
}

void ProjectManager::init(const KUrl& projectUrl, const QString& clipList)
{

    // Open or create a file.  Command line argument passed in Url has
    // precedence, then "openlastproject", then just a plain empty file.
    // If opening Url fails, openlastproject will _not_ be used.
    if (!projectUrl.isEmpty()) {
        // delay loading so that the window shows up
        m_startUrl = projectUrl;
        QTimer::singleShot(500, this, SLOT(openFile()));
    } else if (KdenliveSettings::openlastproject()) {
        QTimer::singleShot(500, this, SLOT(openLastFile()));
    } else {
        newFile(false);
    }

    if (!clipList.isEmpty() && m_project) {
        QStringList list = clipList.split(',');
        QList <QUrl> urls;
        foreach(const QString &path, list) {
            kDebug() << QDir::current().absoluteFilePath(path);
            urls << QUrl::fromLocalFile(QDir::current().absoluteFilePath(path));
        }
        pCore->window()->m_projectList->slotAddClip(urls);
    }
}

bool ProjectManager::queryClose()
{
    // warn the user to save if document is modified and we have clips in our project list
    if (m_project && m_project->isModified() &&
            ((pCore->window()->m_projectList->documentClipList().isEmpty() && !m_project->url().isEmpty()) ||
             !pCore->window()->m_projectList->documentClipList().isEmpty())) {
        pCore->window()->raise();
        pCore->window()->activateWindow();
        QString message;
        if (m_project->url().fileName().isEmpty()) {
            message = i18n("Save changes to document?");
        } else {
            message = i18n("The project <b>\"%1\"</b> has been changed.\nDo you want to save your changes?", m_project->url().fileName());
        }
        switch (KMessageBox::warningYesNoCancel(pCore->window(), message)) {
        case KMessageBox::Yes :
            // save document here. If saving fails, return false;
            return saveFile();
        case KMessageBox::No :
            // User does not want to save the changes, clear recovery files
            m_project->m_autosave->resize(0);
            return true;
        default: // cancel
            return false;
        }
    }
    return true;
}


void ProjectManager::newFile(bool showProjectSettings, bool force)
{
    if (!pCore->window()->m_timelineArea->isEnabled() && !force) {
        return;
    }
    m_fileRevert->setEnabled(false);
    QString profileName = KdenliveSettings::default_profile();
    KUrl projectFolder = KdenliveSettings::defaultprojectfolder();
    QMap <QString, QString> documentProperties;
    QMap <QString, QString> documentMetadata;
    QPoint projectTracks(KdenliveSettings::videotracks(), KdenliveSettings::audiotracks());
    if (!showProjectSettings) {
        if (!KdenliveSettings::activatetabs()) {
            if (!closeCurrentDocument()) {
                return;
            }
        }
    } else {
        QPointer<ProjectSettings> w = new ProjectSettings(NULL, QMap <QString, QString> (), QStringList(), projectTracks.x(), projectTracks.y(), KdenliveSettings::defaultprojectfolder(), false, true, pCore->window());
        if (w->exec() != QDialog::Accepted) {
            delete w;
            return;
        }
        if (!KdenliveSettings::activatetabs()) {
            if (!closeCurrentDocument()) {
                delete w;
                return;
            }
        }
        if (KdenliveSettings::videothumbnails() != w->enableVideoThumbs()) {
            pCore->window()->slotSwitchVideoThumbs();
        }
        if (KdenliveSettings::audiothumbnails() != w->enableAudioThumbs()) {
            pCore->window()->slotSwitchAudioThumbs();
        }
        profileName = w->selectedProfile();
        projectFolder = w->selectedFolder();
        projectTracks = w->tracks();
        documentProperties.insert("enableproxy", QString::number((int) w->useProxy()));
        documentProperties.insert("generateproxy", QString::number((int) w->generateProxy()));
        documentProperties.insert("proxyminsize", QString::number(w->proxyMinSize()));
        documentProperties.insert("proxyparams", w->proxyParams());
        documentProperties.insert("proxyextension", w->proxyExtension());
        documentProperties.insert("generateimageproxy", QString::number((int) w->generateImageProxy()));
        documentProperties.insert("proxyimageminsize", QString::number(w->proxyImageMinSize()));
        documentMetadata = w->metadata();
        delete w;
    }
    pCore->window()->m_timelineArea->setEnabled(true);
    pCore->window()->m_projectList->setEnabled(true);
    bool openBackup;
    KdenliveDoc *doc = new KdenliveDoc(KUrl(), projectFolder, pCore->window()->m_commandStack, profileName, documentProperties, documentMetadata, projectTracks, pCore->monitorManager()->projectMonitor()->render, pCore->window()->m_notesWidget, &openBackup, pCore->window());
    doc->m_autosave = new KAutoSaveFile(KUrl(), doc);
    bool ok;
    TrackView *trackView = new TrackView(doc, pCore->window()->m_tracksActionCollection->actions(), &ok, pCore->window());
    pCore->window()->m_timelineArea->addTab(trackView, KIcon("kdenlive"), doc->description());
    if (!ok) {
        // MLT is broken
        //pCore->window()->m_timelineArea->setEnabled(false);
        //pCore->window()->m_projectList->setEnabled(false);
        pCore->window()->slotPreferences(6);
        return;
    }
    if (pCore->window()->m_timelineArea->count() == 1) {
        connectDocumentInfo(doc);
        pCore->window()->connectDocument(trackView, doc, &m_project);
    } else {
        pCore->window()->m_timelineArea->setTabBarHidden(false);
    }
    pCore->monitorManager()->activateMonitor(Kdenlive::ClipMonitor);
//     m_closeAction->setEnabled(pCore->window()->m_timelineArea->count() > 1);
}

void ProjectManager::activateDocument()
{
    if (pCore->window()->m_timelineArea->currentWidget() == NULL || !pCore->window()->m_timelineArea->isEnabled()) {
        return;
    }
    TrackView *currentTab = (TrackView *) pCore->window()->m_timelineArea->currentWidget();
    KdenliveDoc *currentDoc = currentTab->document();
    connectDocumentInfo(currentDoc);
    pCore->window()->connectDocument(currentTab, currentDoc, &m_project);
}

bool ProjectManager::closeCurrentDocument(bool saveChanges)
{
    QWidget *w = pCore->window()->m_timelineArea->currentWidget();
    if (!w) {
        return true;
    }
    // closing current document
    int ix = pCore->window()->m_timelineArea->currentIndex() + 1;
    if (ix == pCore->window()->m_timelineArea->count()) {
        ix = 0;
    }
    pCore->window()->m_timelineArea->setCurrentIndex(ix);
    TrackView *tabToClose = (TrackView *) w;
    KdenliveDoc *docToClose = tabToClose->document();
    if (docToClose && docToClose->isModified() && saveChanges) {
        QString message;
        if (m_project->url().fileName().isEmpty()) {
            message = i18n("Save changes to document?");
        } else {
            message = i18n("The project <b>\"%1\"</b> has been changed.\nDo you want to save your changes?", m_project->url().fileName());
        }

        switch (KMessageBox::warningYesNoCancel(pCore->window(), message)) {
        case KMessageBox::Yes :
            // save document here. If saving fails, return false;
            if (saveFile() == false) return false;
            break;
        case KMessageBox::Cancel :
            return false;
            break;
        default:
            break;
        }
    }

    pCore->window()->slotTimelineClipSelected(NULL, false);
    pCore->monitorManager()->clipMonitor()->slotSetClipProducer(NULL);
    pCore->window()->m_projectList->slotResetProjectList();
    pCore->window()->m_timelineArea->removeTab(pCore->window()->m_timelineArea->indexOf(w));
    if (pCore->window()->m_timelineArea->count() == 1) {
        pCore->window()->m_timelineArea->setTabBarHidden(true);
//         m_closeAction->setEnabled(false);
    }

    if (docToClose == m_project) {
        delete m_project;
        m_project = NULL;
        pCore->monitorManager()->setDocument(m_project);
        pCore->window()->m_effectStack->clear();
        pCore->window()->m_transitionConfig->slotTransitionItemSelected(NULL, 0, QPoint(), false);
    } else {
        delete docToClose;
    }

    if (w == pCore->window()->m_activeTimeline) {
        delete pCore->window()->m_activeTimeline;
        pCore->window()->m_activeTimeline = NULL;
    } else {
        delete w;
    }

    return true;
}

bool ProjectManager::saveFileAs(const QString &outputFileName)
{
    pCore->monitorManager()->stopActiveMonitor();

    if (m_project->saveSceneList(outputFileName, pCore->monitorManager()->projectMonitor()->sceneList(), pCore->window()->m_projectList->expandedFolders()) == false) {
        return false;
    }

    // Save timeline thumbnails
    pCore->window()->m_activeTimeline->projectView()->saveThumbnails();
    m_project->setUrl(KUrl(outputFileName));
    QByteArray hash = QCryptographicHash::hash(KUrl(outputFileName).encodedPath(), QCryptographicHash::Md5).toHex();
    if (m_project->m_autosave == NULL) {
        m_project->m_autosave = new KAutoSaveFile(KUrl(hash), this);
    } else {
        m_project->m_autosave->setManagedFile(KUrl(hash));
    }

    pCore->window()->setCaption(m_project->description());
    pCore->window()->m_timelineArea->setTabText(pCore->window()->m_timelineArea->currentIndex(), m_project->description());
    pCore->window()->m_timelineArea->setTabToolTip(pCore->window()->m_timelineArea->currentIndex(), m_project->url().path());
    m_project->setModified(false);
    pCore->window()->m_fileOpenRecent->addUrl(KUrl(outputFileName));
    m_fileRevert->setEnabled(true);
    pCore->window()->m_undoView->stack()->setClean();

    return true;
}

bool ProjectManager::saveFileAs()
{
    QString outputFile = KFileDialog::getSaveFileName(m_project->projectFolder(), getMimeType(false));
    if (outputFile.isEmpty()) {
        return false;
    }

    if (QFile::exists(outputFile)) {
        // Show the file dialog again if the user does not want to overwrite the file
        if (KMessageBox::questionYesNo(pCore->window(), i18n("File %1 already exists.\nDo you want to overwrite it?", outputFile)) == KMessageBox::No) {
            return saveFileAs();
        }
    }

    return saveFileAs(outputFile);
}

bool ProjectManager::saveFile()
{
    if (!m_project) {
        return true;
    }

    if (m_project->url().isEmpty()) {
        return saveFileAs();
    } else {
        bool result = saveFileAs(m_project->url().path());
        m_project->m_autosave->resize(0);
        return result;
    }
}

void ProjectManager::openFile()
{
    if (!m_startUrl.isEmpty()) {
        openFile(m_startUrl);
        m_startUrl = KUrl();
        return;
    }
    KUrl url = KFileDialog::getOpenUrl(KUrl("kfiledialog:///projectfolder"), getMimeType());
    if (url.isEmpty()) {
        return;
    }

    pCore->window()->m_fileOpenRecent->addUrl(url);
    openFile(url);
}

void ProjectManager::openLastFile()
{
    if (pCore->window()->m_fileOpenRecent->selectableActionGroup()->actions().isEmpty()) {
        // No files in history
        newFile(false);
        return;
    }

    QAction *firstUrlAction = pCore->window()->m_fileOpenRecent->selectableActionGroup()->actions().last();
    if (firstUrlAction) {
        firstUrlAction->trigger();
    } else {
        newFile(false);
    }
}

void ProjectManager::openFile(const KUrl &url)
{
    // Make sure the url is a Kdenlive project file
    KMimeType::Ptr mime = KMimeType::findByUrl(url);
    if (mime.data()->is("application/x-compressed-tar")) {
        // Opening a compressed project file, we need to process it
        kDebug()<<"Opening archive, processing";
        QPointer<ArchiveWidget> ar = new ArchiveWidget(url);
        if (ar->exec() == QDialog::Accepted) {
            openFile(KUrl(ar->extractedProjectFile()));
        } else if (!m_startUrl.isEmpty()) {
            // we tried to open an invalid file from command line, init new project
            newFile(false);
        }
        delete ar;
        return;
    }

    if (!url.fileName().endsWith(".kdenlive")) {
        // This is not a Kdenlive project file, abort loading
        KMessageBox::sorry(pCore->window(), i18n("File %1 is not a Kdenlive project file", url.path()));
        if (!m_startUrl.isEmpty()) {
            // we tried to open an invalid file from command line, init new project
            newFile(false);
        }
        return;
    }

    // Check if the document is already opened
    const int ct = pCore->window()->m_timelineArea->count();
    bool isOpened = false;
    int i;
    for (i = 0; i < ct; ++i) {
        TrackView *tab = (TrackView *) pCore->window()->m_timelineArea->widget(i);
        KdenliveDoc *doc = tab->document();
        if (doc->url() == url) {
            isOpened = true;
            break;
        }
    }

    if (isOpened) {
        pCore->window()->m_timelineArea->setCurrentIndex(i);
        return;
    }

    if (!KdenliveSettings::activatetabs()) {
        if (!closeCurrentDocument()) {
            return;
        }
    }

    // Check for backup file
    QByteArray hash = QCryptographicHash::hash(url.encodedPath(), QCryptographicHash::Md5).toHex();
    QList<KAutoSaveFile *> staleFiles = KAutoSaveFile::staleFiles(KUrl(hash));
    if (!staleFiles.isEmpty()) {
        if (KMessageBox::questionYesNo(pCore->window(),
                                       i18n("Auto-saved files exist. Do you want to recover them now?"),
                                       i18n("File Recovery"),
                                       KGuiItem(i18n("Recover")), KGuiItem(i18n("Don't recover"))) == KMessageBox::Yes) {
            recoverFiles(staleFiles, url);
            return;
        } else {
            // remove the stale files
            foreach(KAutoSaveFile * stale, staleFiles) {
                stale->open(QIODevice::ReadWrite);
                delete stale;
            }
        }
    }
    pCore->window()->m_messageLabel->setMessage(i18n("Opening file %1", url.path()), InformationMessage);
    pCore->window()->m_messageLabel->repaint();
    doOpenFile(url, NULL);
}

void ProjectManager::doOpenFile(const KUrl &url, KAutoSaveFile *stale)
{
    if (!pCore->window()->m_timelineArea->isEnabled()) return;
    m_fileRevert->setEnabled(true);

    // Recreate stopmotion widget on document change
    if (pCore->window()->m_stopmotion) {
        delete pCore->window()->m_stopmotion;
        pCore->window()->m_stopmotion = NULL;
    }

//     m_timer.start();
    KProgressDialog progressDialog(pCore->window(), i18n("Loading project"), i18n("Loading project"));
    progressDialog.setAllowCancel(false);
    progressDialog.progressBar()->setMaximum(4);
    progressDialog.show();
    progressDialog.progressBar()->setValue(0);

    bool openBackup;
    KdenliveDoc *doc = new KdenliveDoc(stale ? KUrl(stale->fileName()) : url, KdenliveSettings::defaultprojectfolder(), pCore->window()->m_commandStack, KdenliveSettings::default_profile(), QMap <QString, QString> (), QMap <QString, QString> (), QPoint(KdenliveSettings::videotracks(), KdenliveSettings::audiotracks()), pCore->monitorManager()->projectMonitor()->render, pCore->window()->m_notesWidget, &openBackup, pCore->window(), &progressDialog);

    progressDialog.progressBar()->setValue(1);
    progressDialog.progressBar()->setMaximum(4);
    progressDialog.setLabelText(i18n("Loading project"));
    progressDialog.repaint();

    if (stale == NULL) {
        QByteArray hash = QCryptographicHash::hash(url.encodedPath(), QCryptographicHash::Md5).toHex();
        stale = new KAutoSaveFile(KUrl(hash), doc);
        doc->m_autosave = stale;
    } else {
        doc->m_autosave = stale;
        doc->setUrl(url);//stale->managedFile());
        doc->setModified(true);
        stale->setParent(doc);
    }
    connectDocumentInfo(doc);

    progressDialog.progressBar()->setValue(2);
    progressDialog.repaint();

    bool ok;
    TrackView *trackView = new TrackView(doc, pCore->window()->m_tracksActionCollection->actions(), &ok, pCore->window());
    pCore->window()->connectDocument(trackView, doc, &m_project);
    progressDialog.progressBar()->setValue(3);
    progressDialog.repaint();

    pCore->window()->m_timelineArea->setCurrentIndex(pCore->window()->m_timelineArea->addTab(trackView, KIcon("kdenlive"), doc->description()));
    if (!ok) {
        pCore->window()->m_timelineArea->setEnabled(false);
        pCore->window()->m_projectList->setEnabled(false);
        KMessageBox::sorry(pCore->window(), i18n("Cannot open file %1.\nProject is corrupted.", url.path()));
        pCore->window()->slotGotProgressInfo(QString(), -1);
        newFile(false, true);
        return;
    }
    pCore->window()->m_timelineArea->setTabToolTip(pCore->window()->m_timelineArea->currentIndex(), doc->url().path());
    trackView->setDuration(trackView->duration());

    if (pCore->window()->m_timelineArea->count() > 1) {
        pCore->window()->m_timelineArea->setTabBarHidden(false);
    }

    pCore->window()->slotGotProgressInfo(QString(), -1);
    pCore->monitorManager()->projectMonitor()->adjustRulerSize(trackView->duration());
    pCore->monitorManager()->projectMonitor()->slotZoneMoved(trackView->inPoint(), trackView->outPoint());
    progressDialog.progressBar()->setValue(4);
    if (openBackup) {
        pCore->window()->slotOpenBackupDialog(url);
    }
}


void ProjectManager::recoverFiles(const QList<KAutoSaveFile *> &staleFiles, const KUrl &originUrl)
{
    foreach(KAutoSaveFile * stale, staleFiles) {
        /*if (!stale->open(QIODevice::QIODevice::ReadOnly)) {
                  // show an error message; we could not steal the lockfile
                  // maybe another application got to the file before us?
                  delete stale;
                  continue;
        }*/
        kDebug() << "// OPENING RECOVERY: " << stale->fileName() << "\nMANAGED: " << stale->managedFile().path();
        // the stalefiles also contain ".lock" files so we must ignore them... bug in KAutoSaveFile?
        if (!stale->fileName().endsWith(".lock")) {
            doOpenFile(originUrl, stale);
        } else {
            KIO::NetAccess::del(KUrl(stale->fileName()), pCore->window());
        }
    }
}

void ProjectManager::slotRevert()
{
    if (KMessageBox::warningContinueCancel(pCore->window(), i18n("This will delete all changes made since you last saved your project. Are you sure you want to continue?"), i18n("Revert to last saved version")) == KMessageBox::Cancel) return;
    KUrl url = m_project->url();
    if (closeCurrentDocument(false))
        doOpenFile(url, NULL);
}

void ProjectManager::connectDocumentInfo(KdenliveDoc *doc)
{
    if (m_project) {
        if (m_project == doc)
            return;
        disconnect(m_project, SIGNAL(progressInfo(QString,int)), pCore->window(), SLOT(slotGotProgressInfo(QString,int)));
    }
    connect(doc, SIGNAL(progressInfo(QString,int)), pCore->window(), SLOT(slotGotProgressInfo(QString,int)));
}

QString ProjectManager::getMimeType(bool open)
{
    QString mimetype = "application/x-kdenlive";
    KMimeType::Ptr mime = KMimeType::mimeType(mimetype);
    if (!mime) {
        mimetype = "*.kdenlive";
        if (open) mimetype.append(" *.tar.gz");
    }
    else if (open) mimetype.append(" application/x-compressed-tar");
    return mimetype;
}


KdenliveDoc* ProjectManager::current()
{
    return m_project;
}

#include "project/projectmanager.moc"
