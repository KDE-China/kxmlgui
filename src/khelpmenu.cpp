/*
 * This file is part of the KDE Libraries
 * Copyright (C) 1999-2000 Espen Sand (espen@kde.org)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

// I (espen) prefer that header files are included alphabetically

#include "khelpmenu.h"

#include <QTimer>
#include <QAction>
#include <QApplication>
#include <QDialogButtonBox>
#include <QLabel>
#include <QMenu>
#include <QStyle>
#include <QWidget>
#include <QWhatsThis>
#include <QFile>
#include <QDir>
#include <QUrl>
#include <QBoxLayout>
#include <QDesktopServices>
#include <QStandardPaths>

#include "kaboutapplicationdialog.h"
#include "kaboutkdedialog_p.h"
#include "kbugreport.h"
#include "kswitchlanguagedialog_p.h"

#include <kaboutdata.h>
#include <kauthorized.h>
#include <kiconloader.h>
#include <klocalizedstring.h>
#include <kstandardaction.h>

using namespace KDEPrivate;

class KHelpMenuPrivate
{
public:
    KHelpMenuPrivate()
        : mSwitchApplicationLanguage(nullptr),
          mActionsCreated(false),
          mSwitchApplicationLanguageAction(nullptr),
          mAboutData(KAboutData::applicationData())
    {
        mMenu = nullptr;
        mAboutApp = nullptr;
        mAboutKDE = nullptr;
        mBugReport = nullptr;
        mDonateAction = nullptr;
        mHandBookAction = nullptr;
        mWhatsThisAction = nullptr;
        mReportBugAction = nullptr;
        mAboutAppAction = nullptr;
        mAboutKDEAction = nullptr;
    }
    ~KHelpMenuPrivate()
    {
        delete mMenu;
        delete mAboutApp;
        delete mAboutKDE;
        delete mBugReport;
        delete mSwitchApplicationLanguage;
    }

    void createActions(KHelpMenu *q);

    QMenu *mMenu;
    QDialog *mAboutApp;
    KAboutKdeDialog *mAboutKDE;
    KBugReport *mBugReport;
    QAction *mDonateAction;
    KSwitchLanguageDialog *mSwitchApplicationLanguage;

// TODO evaluate if we use static_cast<QWidget*>(parent()) instead of mParent to win that bit of memory
    QWidget *mParent;
    QString mAboutAppText;

    bool mShowWhatsThis;
    bool mActionsCreated;

    QAction *mHandBookAction, *mWhatsThisAction;
    QAction *mReportBugAction, *mSwitchApplicationLanguageAction, *mAboutAppAction, *mAboutKDEAction;

    KAboutData mAboutData;
};

KHelpMenu::KHelpMenu(QWidget *parent, const QString &aboutAppText,
                     bool showWhatsThis)
    : QObject(parent), d(new KHelpMenuPrivate)
{
    d->mAboutAppText = aboutAppText;
    d->mShowWhatsThis = showWhatsThis;
    d->mParent = parent;
    d->createActions(this);
}

KHelpMenu::KHelpMenu(QWidget *parent, const KAboutData &aboutData,
                     bool showWhatsThis)
    : QObject(parent), d(new KHelpMenuPrivate)
{
    d->mShowWhatsThis = showWhatsThis;
    d->mParent = parent;
    d->mAboutData = aboutData;
    d->createActions(this);
}

KHelpMenu::~KHelpMenu()
{
    delete d;
}

void KHelpMenuPrivate::createActions(KHelpMenu *q)
{
    if (mActionsCreated) {
        return;
    }
    mActionsCreated = true;

    if (KAuthorized::authorizeAction(QStringLiteral("help_contents"))) {
        mHandBookAction = KStandardAction::helpContents(q, SLOT(appHelpActivated()), q);
    }
    if (mShowWhatsThis && KAuthorized::authorizeAction(QStringLiteral("help_whats_this"))) {
        mWhatsThisAction = KStandardAction::whatsThis(q, SLOT(contextHelpActivated()), q);
    }

    if (KAuthorized::authorizeAction(QStringLiteral("help_report_bug")) && !mAboutData.bugAddress().isEmpty()) {
        mReportBugAction = KStandardAction::reportBug(q, SLOT(reportBug()), q);
    }

    if (KAuthorized::authorizeAction(QStringLiteral("help_donate"))
        && mAboutData.bugAddress() == QStringLiteral("submit@bugs.kde.org")) {
        mDonateAction = KStandardAction::donate(q, &KHelpMenu::donate, q);
    }

    if (KAuthorized::authorizeAction(QStringLiteral("switch_application_language"))) {
        mSwitchApplicationLanguageAction = KStandardAction::create(KStandardAction::SwitchApplicationLanguage, q, SLOT(switchApplicationLanguage()), q);
    }

    if (KAuthorized::authorizeAction(QStringLiteral("help_about_app"))) {
        mAboutAppAction = KStandardAction::aboutApp(q, SLOT(aboutApplication()), q);
    }

    if (KAuthorized::authorizeAction(QStringLiteral("help_about_kde"))) {
        mAboutKDEAction = KStandardAction::aboutKDE(q, SLOT(aboutKDE()), q);
    }
}

// Used in the non-xml-gui case, like kfind or ksnapshot's help button.
QMenu *KHelpMenu::menu()
{
    if (!d->mMenu) {
        d->mMenu = new QMenu(d->mParent);
        connect(d->mMenu, SIGNAL(destroyed()), this, SLOT(menuDestroyed()));

        d->mMenu->setTitle(i18n("&Help"));

        d->createActions(this);

        bool need_separator = false;
        if (d->mHandBookAction) {
            d->mMenu->addAction(d->mHandBookAction);
            need_separator = true;
        }

        if (d->mWhatsThisAction) {
            d->mMenu->addAction(d->mWhatsThisAction);
            need_separator = true;
        }

        if (d->mReportBugAction) {
            if (need_separator) {
                d->mMenu->addSeparator();
            }
            d->mMenu->addAction(d->mReportBugAction);
            need_separator = true;
        }

        if (d->mDonateAction) {
            if (need_separator) {
                d->mMenu->addSeparator();
            }
            d->mMenu->addAction(d->mDonateAction);
            need_separator = true;
        }

        if (d->mSwitchApplicationLanguageAction) {
            if (need_separator) {
                d->mMenu->addSeparator();
            }
            d->mMenu->addAction(d->mSwitchApplicationLanguageAction);
            need_separator = true;
        }

        if (need_separator) {
            d->mMenu->addSeparator();
        }

        if (d->mAboutAppAction) {
            d->mMenu->addAction(d->mAboutAppAction);
        }

        if (d->mAboutKDEAction) {
            d->mMenu->addAction(d->mAboutKDEAction);
        }
    }

    return d->mMenu;
}

QAction *KHelpMenu::action(MenuId id) const
{
    switch (id) {
    case menuHelpContents:
        return d->mHandBookAction;

    case menuWhatsThis:
        return d->mWhatsThisAction;

    case menuReportBug:
        return d->mReportBugAction;

    case menuSwitchLanguage:
        return d->mSwitchApplicationLanguageAction;

    case menuAboutApp:
        return d->mAboutAppAction;

    case menuAboutKDE:
        return d->mAboutKDEAction;

    case menuDonate:
        return d->mDonateAction;
    }

    return nullptr;
}

void KHelpMenu::appHelpActivated()
{
    QDesktopServices::openUrl(QUrl(QStringLiteral("help:/")));
}

void KHelpMenu::aboutApplication()
{
    if (receivers(SIGNAL(showAboutApplication())) > 0) {
        emit showAboutApplication();
    } else { // if (d->mAboutData)
        if (!d->mAboutApp) {
            d->mAboutApp = new KAboutApplicationDialog(d->mAboutData, d->mParent);
            connect(d->mAboutApp, SIGNAL(finished(int)), this, SLOT(dialogFinished()));
        }
        d->mAboutApp->show();
    }
#if 0 // KF5: when can this happen?
    else {
        if (!d->mAboutApp) {
            d->mAboutApp = new QDialog(d->mParent, Qt::Dialog);
            QString caption = QGuiApplication::applicationDisplayName();
            if (caption.isEmpty()) {
                caption = QCoreApplication::applicationName();
            }
            d->mAboutApp->setWindowTitle(i18n("About %1", caption));
            d->mAboutApp->setObjectName(QStringLiteral("about"));
            connect(d->mAboutApp, SIGNAL(finished(int)), this, SLOT(dialogFinished()));

            const int spacingHint = d->mAboutApp->style()->pixelMetric(QStyle::PM_DefaultLayoutSpacing);
            const int marginHint = d->mAboutApp->style()->pixelMetric(QStyle::PM_DefaultChildMargin);

            QVBoxLayout *vbox = new QVBoxLayout;
            d->mAboutApp->setLayout(vbox);

            QHBoxLayout *hbox = new QHBoxLayout;
            hbox->setSpacing(spacingHint * 3);
            hbox->setMargin(marginHint * 1);

            const int size = IconSize(KIconLoader::Dialog);
            QLabel *label1 = new QLabel(d->mAboutApp);
            label1->setPixmap(qApp->windowIcon().pixmap(size, size));
            QLabel *label2 = new QLabel(d->mAboutApp);
            label2->setText(d->mAboutAppText);

            hbox->addWidget(label1);
            hbox->addWidget(label2);

            vbox->addLayout(hbox);

            QDialogButtonBox *buttonBox = new QDialogButtonBox(d->mAboutApp);
            buttonBox->setStandardButtons(QDialogButtonBox::Close);
            connect(buttonBox, SIGNAL(accepted()), d->mAboutApp, SLOT(accept()));
            connect(buttonBox, SIGNAL(rejected()), d->mAboutApp, SLOT(reject()));
            vbox->addWidget(buttonBox);
        }
        d->mAboutApp->show();
    }
#endif
}

void KHelpMenu::aboutKDE()
{
    if (!d->mAboutKDE) {
        d->mAboutKDE = new KAboutKdeDialog(d->mParent);
        connect(d->mAboutKDE, SIGNAL(finished(int)), this, SLOT(dialogFinished()));
    }
    d->mAboutKDE->show();
}

void KHelpMenu::reportBug()
{
    if (!d->mBugReport) {
        d->mBugReport = new KBugReport(d->mAboutData, d->mParent);
        connect(d->mBugReport, SIGNAL(finished(int)), this, SLOT(dialogFinished()));
    }
    d->mBugReport->show();
}

void KHelpMenu::switchApplicationLanguage()
{
    if (!d->mSwitchApplicationLanguage) {
        d->mSwitchApplicationLanguage = new KSwitchLanguageDialog(d->mParent);
        connect(d->mSwitchApplicationLanguage, SIGNAL(finished(int)), this, SLOT(dialogFinished()));
    }
    d->mSwitchApplicationLanguage->show();
}

void KHelpMenu::donate()
{
    QDesktopServices::openUrl(QUrl(QStringLiteral("https://www.kde.org/donate?app=%1").arg(d->mAboutData.componentName())));
}

void KHelpMenu::dialogFinished()
{
    QTimer::singleShot(0, this, SLOT(timerExpired()));
}

void KHelpMenu::timerExpired()
{
    if (d->mAboutKDE && !d->mAboutKDE->isVisible()) {
        delete d->mAboutKDE; d->mAboutKDE = nullptr;
    }

    if (d->mBugReport && !d->mBugReport->isVisible()) {
        delete d->mBugReport; d->mBugReport = nullptr;
    }

    if (d->mSwitchApplicationLanguage && !d->mSwitchApplicationLanguage->isVisible()) {
        delete d->mSwitchApplicationLanguage; d->mSwitchApplicationLanguage = nullptr;
    }

    if (d->mAboutApp && !d->mAboutApp->isVisible()) {
        delete d->mAboutApp; d->mAboutApp = nullptr;
    }
}

void KHelpMenu::menuDestroyed()
{
    d->mMenu = nullptr;
}

void KHelpMenu::contextHelpActivated()
{
    QWhatsThis::enterWhatsThisMode();
}

