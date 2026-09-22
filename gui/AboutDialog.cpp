/*
* Copyright 2026 Forschungszentrum Jülich
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*    https://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*
*/

#include "AboutDialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTabWidget>
#include <QLabel>
#include <QTextBrowser>
#include <QDialogButtonBox>
#include <QFile>
#include <QFont>
#include <QApplication>
#include <QPixmap>

#include "core/version.h"

#include <tiffvers.h>
#ifdef HASHDF5
#  include <hdf5.h>
#endif
#ifdef HASITK
#  include <itkConfigure.h>
#endif

#include <QSslSocket>

AboutDialog::AboutDialog( QWidget* parent )
    : QDialog(parent)
{
    setWindowTitle(tr("About ImageEditor"));
    setMinimumSize(720, 480);

    m_tabs = new QTabWidget(this);
    m_tabs->addTab(buildAboutTab(),   tr("About"));
    m_tabs->addTab(buildAuthorsTab(), tr("Authors"));
    m_tabs->addTab(buildLicenseTab(), tr("License"));

    auto* closeBtn = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(closeBtn, &QDialogButtonBox::rejected, this, &QDialog::accept);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(m_tabs);
    layout->addWidget(closeBtn);
}

// ---------------------------------------------------------------------------

QWidget* AboutDialog::buildAboutTab()
{
    auto* w      = new QWidget;
    auto* outer  = new QVBoxLayout(w);
    outer->setContentsMargins(20, 20, 20, 16);
    outer->setSpacing(12);

    // --- App name + version ---
    auto* nameLabel = new QLabel("<b style='font-size:18pt'>ImageEditor</b>");
    nameLabel->setAlignment(Qt::AlignCenter);
    outer->addWidget(nameLabel);

    const QString ver   = APP_VERSION;
    const QString build = QString("%1 %2").arg(__DATE__, __TIME__);
    auto* verLabel = new QLabel(
        QString("<center>Version %1&nbsp;&nbsp;·&nbsp;&nbsp;Build %2</center>")
            .arg(ver, build));
    verLabel->setTextFormat(Qt::RichText);
    outer->addWidget(verLabel);

    outer->addSpacing(8);

    // --- Library versions ---
    auto libRow = [](const QString& label, const QString& value) -> QString {
        return QString("<tr><td align='right'><b>%1</b>&nbsp;</td><td>%2</td></tr>")
            .arg(label, value);
    };
    QString libInfo = "<center><table cellspacing='3'>";
    libInfo += libRow(tr("Qt:"),              QT_VERSION_STR);
    libInfo += libRow(tr("BigTIFF support:"), QString("libtiff %1").arg(TIFFLIB_VERSION_STR_MAJ_MIN_MIC));
#ifdef HASHDF5
    libInfo += libRow(tr("HDF5 support:"),   QString("HDF5 %1").arg(H5_VERSION));
#else
    libInfo += libRow(tr("HDF5 support:"),   tr("not compiled in"));
#endif
#ifdef HASITK
    libInfo += libRow(tr("ITK support:"),
        QString("%1.%2.%3").arg(ITK_VERSION_MAJOR).arg(ITK_VERSION_MINOR).arg(ITK_VERSION_PATCH));
#else
    libInfo += libRow(tr("ITK support:"),    tr("not compiled in"));
#endif
    {
        const QString sslVer = QSslSocket::sslLibraryVersionString();
        libInfo += libRow(tr("SSL:"), sslVer.isEmpty() ? tr("not available") : sslVer);
    }
    libInfo += "</table></center>";
    auto* libLabel = new QLabel(libInfo);
    libLabel->setTextFormat(Qt::RichText);
    libLabel->setAlignment(Qt::AlignCenter);
    outer->addWidget(libLabel);

    outer->addSpacing(16);

    // --- Institutional affiliation ---
    // To add institution logos: replace the placeholder QLabels below with
    // QLabel::setPixmap(QPixmap(":/icons/logo_fzj.png")) once icons are added
    // to resources.qrc.
    auto* affiliationTitle = new QLabel(tr("<b>Institutional Affiliation</b>"));
    affiliationTitle->setAlignment(Qt::AlignCenter);
    outer->addWidget(affiliationTitle);

    // Logo row — add QPixmap labels here as institution icons become available
    auto* logoRow  = new QHBoxLayout;
    logoRow->setSpacing(24);
    logoRow->addStretch();

    auto* fzjLabel = new QLabel;
    fzjLabel->setText("Forschungszentrum Jülich GmbH");
    fzjLabel->setAlignment(Qt::AlignCenter);
    logoRow->addWidget(fzjLabel);

    logoRow->addStretch();
    outer->addLayout(logoRow);

    outer->addStretch();

    // --- Copyright ---
    auto* copyrightLabel = new QLabel(
        "<center><small>Copyright 2026 Forschungszentrum Jülich · "
        "Apache License 2.0</small></center>");
    copyrightLabel->setTextFormat(Qt::RichText);
    outer->addWidget(copyrightLabel);

    return w;
}

QWidget* AboutDialog::buildAuthorsTab()
{
    auto* browser = new QTextBrowser;
    browser->setReadOnly(true);
    browser->setOpenExternalLinks(false);

    QFile f(":/AUTHORS");
    if ( f.open(QIODevice::ReadOnly | QIODevice::Text) )
        browser->setPlainText(QString::fromUtf8(f.readAll()));
    else
        browser->setPlainText(tr("AUTHORS file not available."));

    QFont mono("Courier New");
    mono.setStyleHint(QFont::Monospace);
    browser->setFont(mono);

    return browser;
}

QWidget* AboutDialog::buildLicenseTab()
{
    auto* browser = new QTextBrowser;
    browser->setReadOnly(true);
    browser->setOpenExternalLinks(false);

    QFile f(":/LICENSE.txt");
    if ( f.open(QIODevice::ReadOnly | QIODevice::Text) )
        browser->setPlainText(QString::fromUtf8(f.readAll()));
    else
        browser->setPlainText(tr("License file not available."));

    QFont mono("Courier New");
    mono.setStyleHint(QFont::Monospace);
    browser->setFont(mono);

    return browser;
}
