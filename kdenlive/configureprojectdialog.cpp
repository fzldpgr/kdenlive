/***************************************************************************
                          configureprojectdialog  -  description
                             -------------------
    begin                : Sat Nov 15 2003
    copyright            : (C) 2003 by Jason Wood
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
#include "configureprojectdialog.h"

#include <qhbox.h>
#include <qsplitter.h>
#include <qvbox.h>
#include <qlayout.h>

#include <kiconloader.h>
#include <kjanuswidget.h>
#include <klistbox.h>
#include <klocale.h>
#include <kpushbutton.h>

#include "configureproject.h"
#include "exportconfig.h"

ConfigureProjectDialog::ConfigureProjectDialog(QPtrList<AVFileFormatDesc> &formatList, QWidget *parent, const char *name, WFlags f) :
                              KDialogBase(Plain,
                                          i18n("Configure Project"),
                                          Help | Default | Ok | Apply | Cancel,
                                          Ok,
                                          parent, name)
{
	QHBoxLayout *topLayout = new QHBoxLayout( plainPage(), 0, 6 );

	m_hSplitter = new QSplitter( Horizontal, plainPage(), "horizontal splitter" );
	topLayout->addWidget(m_hSplitter);

	m_presetVBox = new QVBox(m_hSplitter, "preset vbox");

	m_presetList = new KListBox(m_presetVBox, "preset list");
	m_addButton = new KPushButton(i18n("Add Preset"), m_presetVBox, "add");
	m_deleteButton = new KPushButton(i18n("Delete Preset"), m_presetVBox, "delete");

	m_tabArea = new KJanusWidget(m_hSplitter, "tabbed area", Tabbed);

	QFrame *m_configPage = m_tabArea->addVBoxPage(i18n("Project Configuration"),
						i18n("Setup the project"),
						KGlobal::instance()->iconLoader()->loadIcon( "piave", KIcon::NoGroup, KIcon::SizeMedium ));
	QFrame *m_exportPage = m_tabArea->addVBoxPage(i18n("Default Export"),
						i18n("Configure the default export setting"),
						KGlobal::instance()->iconLoader()->loadIcon( "piave", KIcon::NoGroup, KIcon::SizeMedium ));

	m_config = new ConfigureProject( m_configPage, "configure page" );
	m_export = new ExportConfig( formatList, m_exportPage, "export page");
}


ConfigureProjectDialog::~ConfigureProjectDialog()
{
}


/** Occurs when the apply button is clicked. */
void ConfigureProjectDialog::slotApply()
{
  //m_renderDlg->writeSettings();
}

/** Called when the ok button is clicked. */
void ConfigureProjectDialog::slotOk()
{
  //m_renderDlg->writeSettings();
  accept();
}

/** Called when the cancel button is clicked. */
void ConfigureProjectDialog::slotCancel()
{
  reject();
}

/** Called when the "Default" button is pressed. */
void ConfigureProjectDialog::slotDefault()
{
  //m_renderDlg->readSettings();
}

#include "configureprojectdialog.moc"
