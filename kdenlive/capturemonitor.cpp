/***************************************************************************
                          capturemonitor  -  description
                             -------------------
    begin                : Sun Jun 12 2005
    copyright            : (C) 2005 by Jason Wood
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
#include "capturemonitor.h"

#include <qtoolbutton.h>

#include <kled.h>
#include <klocale.h>
#include <kio/netaccess.h>
#include <kstandarddirs.h>
#include <klineeditdlg.h>
#include <kurlrequester.h>
#include <klistview.h>
#include <kiconloader.h>
#include <kmessagebox.h>

#include "kdenlivedoc.h"
#include "kmmrecpanel.h"


namespace Gui {

    CaptureMonitor::CaptureMonitor(KdenliveApp * app, QWidget * parent,
	const char *name):KMonitor(parent, name),
	m_app(app), m_screenHolder(new QVBox(this,name)), m_screen(new QWidget(m_screenHolder, name)),
	m_recPanel(new KMMRecPanel(app->getDocument(), this, name)), captureProcess(0), hasCapturedFiles(false), m_tmpFolder(QString::null)
    {
	m_fileNumber = 0;
	m_screen->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Expanding);
	m_screen->setBackgroundMode(Qt::PaletteDark);
	slotCheckCaptureStatus();
	m_recPanel->capture_format->setCurrentItem(KdenliveSettings::captureformat());

	connect(m_recPanel, SIGNAL(activateMonitor()), this,  SLOT(activateMonitor()));
	connect(m_recPanel, SIGNAL(stopDevice()), this, SLOT(slotStop()));
	connect(m_recPanel, SIGNAL(playDevice()), this, SLOT(slotPlay()));
	connect(m_recPanel, SIGNAL(pauseDevice()), this, SLOT(slotPause()));
	connect(m_recPanel, SIGNAL(recDevice()), this, SLOT(slotRec()));
	connect(m_recPanel, SIGNAL(forwardDevice()), this, SLOT(slotFastForward()));
	connect(m_recPanel, SIGNAL(stepForwardDevice()), this, SLOT(slotForward()));
	connect(m_recPanel, SIGNAL(rewindDevice()), this, SLOT(slotRewind()));
	connect(m_recPanel, SIGNAL(stepRewindDevice()), this, SLOT(slotReverse()));
    } 

    CaptureMonitor::~CaptureMonitor() {
	if (captureProcess) delete captureProcess;
    }

    void CaptureMonitor::slotCheckCaptureStatus() {
	QObject *label = m_screenHolder->child("warn_message");
	if (label) delete label;
	label = m_screenHolder->child("warn_button");
	if (label) delete label;
	QString warnMessage;
	if (KStandardDirs::findExe("dvgrab") == QString::null ||KStandardDirs::findExe("ffplay") == QString::null) {
		warnMessage = i18n("<b>The programs dvgrab or ffplay are missing</b>.<br>Firewire capture will be disabled until you install them.");
		m_readyForCapture = false;
	}
	else if (!KIO::NetAccess::exists(KURL("/dev/raw1394"), false, this)) {
		if (!warnMessage.isEmpty()) warnMessage += "<br>";
		warnMessage += i18n("<b>Device not detected. Make sure that the camcorder is turned on. Please check that the kernel module raw1394 is loaded and that you have write permission to /dev/raw1394 or equivalent.");
		m_readyForCapture = false;
	}
	else m_readyForCapture = true;
	m_recPanel->setEnabled(m_readyForCapture);
	if (!m_readyForCapture) {
		QLabel *warningLabel = new QLabel(warnMessage, m_screenHolder, "warn_message");
		QPushButton *pb = new QPushButton(i18n("Check again"), m_screenHolder, "warn_button");
		connect(pb, SIGNAL(clicked()), this, SLOT(slotCheckCaptureStatus()));
		warningLabel->setPaletteBackgroundColor(Qt::red);
		warningLabel->setMargin(5);
		warningLabel->show();
		pb->show();
	}
    }
    
    void CaptureMonitor::exportCurrentFrame(KURL url, bool notify) const {
	// TODO FIXME
    } 

    KMMEditPanel *CaptureMonitor::editPanel() const {
	// TODO FIXME
	return 0;
    } 
    
    KMMScreen *CaptureMonitor::screen() const {
	// TODO FIXME
	return 0;
    } 

    void CaptureMonitor::activateMonitor() {
	m_app->activateMonitor(this);
    }

    void CaptureMonitor::slotSetActive() {
        m_recPanel->rendererConnected();
    }
    
    DocClipRef *CaptureMonitor::clip() const {
	return 0;
    } 
    
    void CaptureMonitor::slotSetupScreen() {
	//m_screen->setCapture();
    }

    void CaptureMonitor::slotRewind() {
	if (!captureProcess) slotInit();
	captureProcess->writeStdin("a", 1);
    }

    void CaptureMonitor::slotReverse() {
	if (!captureProcess) slotInit();
	captureProcess->writeStdin("j", 1);
    }


void CaptureMonitor::displayCapturedFiles()
{
	if (m_tmpFolder.isEmpty()) return;
	KDialogBase *dia = new KDialogBase(  KDialogBase::Swallow, i18n("Captured Clips"), KDialogBase::Ok | KDialogBase::Cancel, KDialogBase::Ok, this, "captured_clips", true);
	dia->setButtonOKText (i18n("Process captured files"));
	dia->setButtonCancelText (i18n("Continue capture..."));
	QVBox *page = new QVBox(dia);
	KListView *lv = new KListView(page);
	lv->addColumn(i18n("Add"));
	lv->addColumn("original_name",0);
	lv->addColumn(i18n("Clip Name"));
	lv->setItemsRenameable(true);
	lv->setResizeMode(QListView::LastColumn);
	lv->setRenameable(2, true);
	lv->setRenameable(0, false);
	lv->setAllColumnsShowFocus(true);

	QHBox *box1 = new QHBox(page);
	QLabel *lab = new QLabel(i18n("Move selected files to:"), box1);
	KURLRequester *urlreq = new KURLRequester(box1);
	urlreq->setMode(KFile::Directory);
	urlreq->setURL(KdenliveSettings::currentdefaultfolder());

	QStringList more;
    	QStringList::Iterator it;

        QDir dir( KURL(m_tmpFolder).path() );
        more = dir.entryList( QDir::Files );
        for ( it = more.begin() ; it != more.end() ; ++it ){
		KURL url;
		url.setPath(m_tmpFolder);
		url.addPath(*it);
		QPixmap p = m_app->getDocument()->renderer()->getVideoThumbnail(url.path(), 1, 60, 40);
		if (!p.isNull()) {
		    QCheckListItem *item = new QCheckListItem(lv, QString::null, QCheckListItem::CheckBox);
		    item->setPixmap(0, p);
		    item->setText(1, (*it));
		    item->setText(2, (*it));
		    ((QCheckListItem*)item)->setOn(true);
		}
	    }
	lv->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	dia->setMainWidget(page);
	dia->setMinimumSize(400, 240);
	dia->adjustSize();
	if (dia->exec() == QDialog::Accepted) {
	    QListViewItemIterator it( lv );
    	    for ( ; it.current(); ++it )
                if ( ( (QCheckListItem*)it.current() )->isOn() ) {
		    bool ok = true;
		    // move selected files to our project folder
                    QString source = m_tmpFolder + it.current()->text( 1 );
		    QString dest = urlreq->url() + "/" + it.current()->text( 2 );
		    while (KIO::NetAccess::exists(KURL(dest), true, this) && ok) {
			dest = KLineEditDlg::getText(i18n("File exists"), i18n("File already exists, enter a new name"), dest, &ok);
		    }
		    if (ok) {
			KIO::NetAccess::move(KURL(source), KURL(dest), this);
		        m_app->insertClipFromUrl(dest);
		    }
	        }
	    KIO::NetAccess::del(m_tmpFolder, this);
	    m_tmpFolder = QString::null;
	    hasCapturedFiles = false;
	}
	delete lab;
	delete urlreq;
	delete box1;
	delete lv;
	delete page;
	delete dia;
}

    void CaptureMonitor::slotProcessStopped(KProcess *) {
	KMessageBox::detailedSorry(this, i18n("The capture stopped, see details for more info."), m_errorLog);
	slotStop();
    }

    void CaptureMonitor::receivedStderr(KProcess *, char *buffer, int len)
    {
	QCString res(buffer, len);
	m_errorLog.append(res + "\n");
    }

    void CaptureMonitor::slotStop() {
	if (captureProcess) {
	 //if (!captureProcess->normalExit()) 
        if (m_screen) {
	    delete m_screen;
	    m_screen = new QWidget(m_screenHolder, name());
	    m_screen->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Expanding);
	    m_screen->setBackgroundMode(Qt::PaletteDark);
	    m_screen->show();
	}
	    captureProcess->writeStdin("q", 1);
	    if (captureProcess) delete captureProcess;
	    captureProcess = 0;
	}
	m_recPanel->rendererDisconnected();
	m_recPanel->capture_format->setEnabled(true);
	if (hasCapturedFiles) displayCapturedFiles();
	m_recPanel->unsetRecording();
    }

    void CaptureMonitor::slotInit() {
	if (captureProcess) slotStop();
	m_errorLog = QString::null;
	captureProcess = new KProcess();
	//if (!m_tmpFolder.isEmpty()) KIO::NetAccess::del(m_tmpFolder, this);
	m_tmpFolder = KdenliveSettings::currenttmpfolder() + "/dvcapture/";
	KIO::NetAccess::mkdir(KURL(m_tmpFolder), this);
	if (!KIO::NetAccess::exists(m_tmpFolder, false, this)) {
		// creation of custom tmp folder failed, default to system tmp resource
		m_tmpFolder = locateLocal("tmp", "dvcapture/", true);
	}
	captureProcess->setWorkingDirectory(m_tmpFolder);
        captureProcess->setUseShell(true);
        captureProcess->setEnvironment("SDL_WINDOWID", QString::number(m_screen->winId()));
	*captureProcess<<"dvgrab";

	bool isHdv = false;

	switch (m_recPanel->capture_format->currentItem()){
	    case 0:
 		*captureProcess<<"--format"<<"dv1";
		break;
	    case 1:
 		*captureProcess<<"--format"<<"dv2";
		break;
	    case 3:
 		*captureProcess<<"--format"<<"hdv";
		isHdv = true;
		break;
	    default:
        	*captureProcess<<"--format"<<"raw";
		break;
	}

	if (KdenliveSettings::autosplit()) *captureProcess<<"--autosplit";
	if (KdenliveSettings::timestamp()) *captureProcess<<"--timestamp";
	*captureProcess<<"-i"<<"capture"<<"-";
        if (isHdv)
	    *captureProcess<<"|"<<"ffplay"<<"-f"<<"mpegts"<<"-x"<<QString::number(m_screen->width())<<"-y"<<QString::number(m_screen->height())<<"-";
	else 
	    *captureProcess<<"|"<<"ffplay"<<"-f"<<"dv"<<"-x"<<QString::number(m_screen->width())<<"-y"<<QString::number(m_screen->height())<<"-";
        connect(captureProcess, SIGNAL(processExited(KProcess *)), this, SLOT(slotProcessStopped(KProcess *)));
    	connect(captureProcess, SIGNAL(receivedStderr (KProcess *, char *, int )), this, SLOT(receivedStderr(KProcess *, char *, int)));
	captureProcess->start(KProcess::NotifyOnExit, KProcess::Communication(KProcess::Stdin | KProcess::Stderr));
	m_recPanel->capture_format->setEnabled(false);
	m_recPanel->rendererConnected();
    }

    void CaptureMonitor::slotPause() {
	if (!captureProcess) slotInit();
	captureProcess->writeStdin("\e", 2);
	m_recPanel->unsetRecording();
    }

    void CaptureMonitor::slotPlay() {
	if (!captureProcess) slotInit();
	captureProcess->writeStdin(" ", 1);
    }

    void CaptureMonitor::slotRec() {
        QDir dir( KURL(m_tmpFolder).path() );
        m_fileNumber = dir.entryList( QDir::Files ).count();
	if (!captureProcess) slotInit();
	captureProcess->writeStdin("c\n", 3);
	hasCapturedFiles = true;
	QTimer::singleShot( 1500, this, SLOT(checkCapture()) );
    }

    void CaptureMonitor::checkCapture() {
	QDir dir( KURL(m_tmpFolder).path() );
        int fileNumber = dir.entryList( QDir::Files ).count();
	if (fileNumber > m_fileNumber) m_recPanel->setRecording();
    }

    void CaptureMonitor::slotForward() {
	if (!captureProcess) slotInit();
	captureProcess->writeStdin("l", 1);
    }

    void CaptureMonitor::slotFastForward() {
	if (!captureProcess) slotInit();
	captureProcess->writeStdin("z", 1);
    }

}

