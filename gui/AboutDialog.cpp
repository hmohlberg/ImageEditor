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
#ifdef HASLAMA
#  include <onnxruntime_c_api.h>
#endif

#include <QSslSocket>

#include "../util/GpuInfo.h"

AboutDialog::AboutDialog( QWidget* parent )
    : QDialog(parent)
{
    setWindowTitle(tr("About ImageEditor"));
    setMinimumSize(720, 480);

    m_tabs = new QTabWidget(this);
    m_tabs->addTab(buildAboutTab(),       tr("About"));
    m_tabs->addTab(buildShortcutsTab(),  tr("Shortcuts"));
    m_tabs->addTab(buildThirdPartyTab(), tr("Third-party"));
    m_tabs->addTab(buildAuthorsTab(),    tr("Authors"));
    m_tabs->addTab(buildLicenseTab(),    tr("License"));

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
#ifdef TIFFLIB_VERSION_STR_MAJ_MIN_MIC
    libInfo += libRow(tr("BigTIFF support:"), QString("libtiff %1").arg(TIFFLIB_VERSION_STR_MAJ_MIN_MIC));
#else
    libInfo += libRow(tr("BigTIFF support:"), QString("libtiff %1").arg(TIFFLIB_VERSION_STR));
#endif
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
    libInfo += libRow(tr("GPU:"), GpuInfo::query().htmlSummary());
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

QWidget* AboutDialog::buildShortcutsTab()
{
    auto* browser = new QTextBrowser;
    browser->setReadOnly(true);
    browser->setOpenExternalLinks(false);

    // Helper lambdas for HTML generation
    auto section = [](const QString& title) -> QString {
        return QString("<tr><td colspan='2' style='padding-top:10px'>"
                       "<b style='font-size:10pt'>%1</b></td></tr>").arg(title);
    };
    auto row = [](const QString& keys, const QString& desc) -> QString {
        return QString("<tr>"
                       "<td style='padding:2px 12px 2px 4px; white-space:nowrap'>"
                       "<b>%1</b>"
                       "</td>"
                       "<td style='padding:2px 0'>%2</td>"
                       "</tr>").arg(keys, desc);
    };
    auto mouseRow = [](const QString& action, const QString& desc) -> QString {
        return QString("<tr>"
                       "<td style='padding:2px 12px 2px 4px; white-space:nowrap'>"
                       "<i>%1</i>"
                       "</td>"
                       "<td style='padding:2px 0'>%2</td>"
                       "</tr>").arg(action, desc);
    };

    QString html =
        "<html><body style='font-size:9pt; margin:8px'>"
        "<table cellspacing='0' cellpadding='0' width='100%'>";

    // ── Global ────────────────────────────────────────────────────────────────
    html += section(tr("Global"));
    html += row("Ctrl+Z",           tr("Undo"));
    html += row("Ctrl+Y",           tr("Redo"));
    html += row("Tab",              tr("Cycle to next visible layer"));
    html += row("+",                tr("Zoom in"));
    html += row("−",                tr("Zoom out"));
    html += row("Shift + Drag",     tr("Pan the view"));

    // ── Mouse – Navigation ─────────────────────────────────────────────────
    html += section(tr("Navigation (Mouse)"));
    html += mouseRow(tr("Scroll wheel"),         tr("Zoom in / out (anchored under cursor)"));
    html += mouseRow(tr("Shift + Left drag"),    tr("Pan the view"));
    html += mouseRow(tr("Overview – Left drag"), tr("Scroll main view to clicked position"));
    html += mouseRow(tr("Overview – Dbl-click"), tr("Jump main view to clicked position"));

    // ── Image Layer mode ──────────────────────────────────────────────────
    html += section(tr("Image Layer Mode"));
    html += row("Ctrl+T",    tr("Switch to Translate mode"));
    html += row("Ctrl+S",    tr("Switch to Scale mode"));
    html += row("Ctrl+R",    tr("Switch to Rotate mode"));
    html += row("Ctrl+M",    tr("Switch to Flip (mirror) mode"));
    html += row("Ctrl+W",    tr("Switch to Cage Warp mode"));
    html += row("Ctrl+P",    tr("Switch to Perspective Warp mode"));
    html += row("Ctrl+V / Alt+V", tr("Make all layers visible"));
    html += row("Ctrl+I / Alt+I", tr("Toggle visibility of selected layer"));
    html += row("Q",         tr("Exit / disable current transform mode (Scale)"));
    html += row("R",         tr("Reset transform overlay (Scale)"));
    html += mouseRow(tr("Left drag on layer"),      tr("Move / rotate layer (depends on mode)"));
    html += mouseRow(tr("Alt + Left click layer"),  tr("Add / remove layer from co-move group"));
    html += mouseRow(tr("Dbl-click layer (Scale)"), tr("Enable scale transform overlay"));
    html += mouseRow(tr("Dbl-click layer (Perspective)"), tr("Enable perspective warp"));
    html += mouseRow(tr("Dbl-click layer (Cage, off)"),   tr("Enable cage warp overlay"));
    html += mouseRow(tr("Dbl-click layer (Cage, on)"),    tr("Hide cage warp overlay"));
    html += mouseRow(tr("Dbl-click layer (Flip)"),        tr("Apply mirror to layer"));
    html += mouseRow(tr("Drag transform handle"),   tr("Scale / perspective warp"));
    html += mouseRow(tr("Drag centre handle"),      tr("Translate layer via overlay"));
    html += mouseRow(tr("Drag cage point"),         tr("Deform cage warp"));

    // ── Paint mode ────────────────────────────────────────────────────────
    html += section(tr("Paint Mode"));
    html += mouseRow(tr("Left drag on image"),  tr("Paint with brush colour"));
    html += mouseRow(tr("Right drag on image"), tr("Paint with background (border-averaged) colour"));
    html += mouseRow(tr("Pipette + Left click"), tr("Sample pixel colour as brush colour"));

    // ── Mask mode ─────────────────────────────────────────────────────────
    html += section(tr("Mask Mode"));
    html += mouseRow(tr("Left drag on image"),  tr("Paint mask label (selected class)"));
    html += mouseRow(tr("Right drag on image"), tr("Erase mask label"));

    // ── Lasso / Free Selection mode ───────────────────────────────────────
    html += section(tr("Lasso / Free Selection Mode"));
    html += mouseRow(tr("Left drag on image"), tr("Draw freehand lasso selection path"));
    html += mouseRow(tr("Release after ≥3 points"), tr("Cut selection to new layer"));

    // ── Polygon mode ──────────────────────────────────────────────────────
    html += section(tr("Polygon Mode"));
    html += row("Return / Escape",  tr("Finish / close active polygon"));
    html += row("Ctrl+A",           tr("Switch to Add Point mode"));
    html += row("Ctrl+D",           tr("Switch to Delete Point mode"));
    html += row("Ctrl+M",           tr("Switch to Move Point mode"));
    html += row("Ctrl+R",           tr("Switch to Reduce Polygon mode"));
    html += row("Ctrl+S",           tr("Switch to Smooth Polygon mode"));
    html += row("Ctrl+T",           tr("Switch to Translate Polygon mode"));
    html += mouseRow(tr("Left click on canvas"),        tr("Add new polygon vertex"));
    html += mouseRow(tr("Dbl-click polygon (Add)"),     tr("Insert vertex on polygon edge"));
    html += mouseRow(tr("Dbl-click polygon (Delete)"),  tr("Remove vertex"));
    html += mouseRow(tr("Dbl-click polygon (Reduce)"),  tr("Simplify polygon"));
    html += mouseRow(tr("Dbl-click polygon (Smooth)"),  tr("Smooth polygon"));
    html += mouseRow(tr("Dbl-click polygon (Select)"),  tr("Toggle polygon selection / cycle colour"));
    html += mouseRow(tr("Drag polygon point"),          tr("Move vertex (Move Point mode)"));
    html += mouseRow(tr("Drag polygon (Translate)"),    tr("Translate entire polygon"));

    html += "</table></body></html>";

    browser->setHtml(html);
    return browser;
}

QWidget* AboutDialog::buildThirdPartyTab()
{
    auto* browser = new QTextBrowser;
    browser->setReadOnly(true);
    browser->setOpenExternalLinks(true);

    // Helper: one library entry
    auto entry = [](const QString& name,
                    const QString& version,
                    const QString& license,
                    const QString& url,
                    const QString& note = QString()) -> QString
    {
        QString s =
            QString("<tr valign='top'>"
                    "<td style='padding:6px 14px 6px 4px; white-space:nowrap'>"
                    "<b>%1</b></td>"
                    "<td style='padding:6px 14px 6px 0; white-space:nowrap'>%2</td>"
                    "<td style='padding:6px 14px 6px 0; white-space:nowrap'>%3</td>"
                    "<td style='padding:6px 0'>")
            .arg(name, version, license);
        if ( !url.isEmpty() )
            s += QString("<a href='%1'>%1</a>").arg(url);
        if ( !note.isEmpty() )
            s += (url.isEmpty() ? QString() : QString("<br>"))
                 + QString("<small>%1</small>").arg(note);
        s += "</td></tr>";
        return s;
    };

    QString html =
        "<html><body style='font-size:9pt; margin:8px'>"
        "<p>This application uses the following third-party libraries and models. "
        "Their licenses are listed below; full license texts are available at the "
        "URLs shown or in the accompanying source tree.</p>"
        "<table cellspacing='0' cellpadding='0' width='100%'>"
        "<tr>"
        "<td style='padding:4px 14px 4px 4px; border-bottom:1px solid #888'><b>Library / Model</b></td>"
        "<td style='padding:4px 14px 4px 0; border-bottom:1px solid #888'><b>Version</b></td>"
        "<td style='padding:4px 14px 4px 0; border-bottom:1px solid #888'><b>License</b></td>"
        "<td style='padding:4px 0;           border-bottom:1px solid #888'><b>Source</b></td>"
        "</tr>";

    // Qt
    html += entry("Qt",
                  QT_VERSION_STR,
                  "LGPL 3.0",
                  "https://www.qt.io",
                  "Used under the open-source LGPL 3.0 terms.");

    // libtiff
#ifdef TIFFLIB_VERSION_STR_MAJ_MIN_MIC
    html += entry("libtiff", TIFFLIB_VERSION_STR_MAJ_MIN_MIC,
#else
    html += entry("libtiff", TIFFLIB_VERSION_STR,
#endif
                  "LibTIFF License (BSD-like)",
                  "http://libtiff.gitlab.io/libtiff/",
                  "BigTIFF read/write support.");

    // HDF5
#ifdef HASHDF5
    html += entry("HDF5", H5_VERSION,
                  "BSD-style (HDF Group)",
                  "https://www.hdfgroup.org/solutions/hdf5/",
                  "HDF5 file format support.");
#endif

    // ONNX Runtime
#ifdef HASLAMA
    html += entry("ONNX Runtime", QString("API %1").arg(ORT_API_VERSION),
                  "MIT",
                  "https://onnxruntime.ai",
                  "C++ inference engine for LaMa AI inpainting.");

    // LaMa model
    html += entry("LaMa (big-lama ONNX)",
                  "—",
                  "Apache 2.0",
                  "https://github.com/advimman/lama",
                  "Resolution-robust Large Mask Inpainting with Fourier Convolutions.<br>"
                  "Suvorov et al., Samsung Research, WACV 2022.<br>"
                  "Model weights not bundled — placed separately by the user.");
#endif

    // ITK
#ifdef HASITK
    html += entry("ITK",
                  QString("%1.%2.%3")
                      .arg(ITK_VERSION_MAJOR).arg(ITK_VERSION_MINOR).arg(ITK_VERSION_PATCH),
                  "Apache 2.0",
                  "https://itk.org");
#endif

    // OpenSSL (reported via Qt)
    {
        const QString sslVer = QSslSocket::sslLibraryVersionString();
        if ( !sslVer.isEmpty() )
            html += entry("OpenSSL", sslVer, "Apache 2.0",
                          "https://www.openssl.org",
                          "TLS/SSL support (via Qt Network).");
    }

    html += "</table>"
            "<p style='margin-top:12px'><small>"
            "LaMa citation: R. Suvorov et al., &ldquo;Resolution-robust Large Mask Inpainting "
            "with Fourier Convolutions&rdquo;, WACV 2022."
            "</small></p>"
            "</body></html>";

    browser->setHtml(html);
    return browser;
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
