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

#include "ConfigDialog.h"
#include "../core/Config.h"
#include "../util/GpuInfo.h"

#include <QTabWidget>
#include <QCheckBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QLineEdit>
#include <QComboBox>
#include <QPushButton>
#include <QFormLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QLabel>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QMessageBox>
#include <QColorDialog>
#include <QColor>
#include <QFileInfo>

// ---------------------------------------------------------------------------
// helpers
// ---------------------------------------------------------------------------

static void styleColorButton(QPushButton* btn, const QString& colorName)
{
    QColor c(colorName);
    if (!c.isValid()) c = Qt::gray;
    btn->setStyleSheet(QString("QPushButton { background-color:%1; border:1px solid #555; }")
                       .arg(c.name()));
    btn->setText(QString());
    btn->setFixedSize(40, 22);
}

// ---------------------------------------------------------------------------
// constructor
// ---------------------------------------------------------------------------

ConfigDialog::ConfigDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Configuration"));
    setMinimumWidth(800);

    setStyleSheet(
        "QCheckBox::indicator {"
        "  width: 13px; height: 13px;"
        "  border: 2px solid #888888;"
        "  border-radius: 3px;"
        "  background-color: #2a2a2a;"
        "}"
        "QCheckBox::indicator:unchecked:hover {"
        "  border-color: #aaaaaa;"
        "  background-color: #3a3a3a;"
        "}"
        "QCheckBox::indicator:checked {"
        "  border-color: #aaaaaa;"
        "  background-color: #2a2a2a;"
        "  image: url(:/icons/icons/check.svg);"
        "}"
        "QCheckBox::indicator:checked:hover {"
        "  border-color: #cccccc;"
        "}"
    );

    QTabWidget* tabs = new QTabWidget(this);
    tabs->addTab(buildMainTab(),       tr("Main"));
    tabs->addTab(buildCageTab(),       tr("Cage"));
    tabs->addTab(buildScaleTab(),      tr("Scale"));
    tabs->addTab(buildLassoTab(),      tr("Lasso"));
    tabs->addTab(buildPolygonTab(),    tr("Polygon"));
    tabs->addTab(buildImageLayerTab(), tr("ImageLayer"));

    // button row
    QPushButton* loadBtn    = new QPushButton(tr("Load"),    this);
    QPushButton* saveBtn    = new QPushButton(tr("Save As"), this);
    QPushButton* defaultBtn = new QPushButton(tr("Default"), this);
    QPushButton* closeBtn   = new QPushButton(tr("Close"),   this);

    QHBoxLayout* btnRow = new QHBoxLayout;
    btnRow->addWidget(loadBtn);
    btnRow->addWidget(saveBtn);
    btnRow->addWidget(defaultBtn);
    btnRow->addStretch();
    btnRow->addWidget(closeBtn);

    QVBoxLayout* main = new QVBoxLayout(this);
    main->addWidget(tabs);
    main->addLayout(btnRow);

    connect(saveBtn,    &QPushButton::clicked, this, &ConfigDialog::onSave);
    connect(loadBtn,    &QPushButton::clicked, this, &ConfigDialog::onLoad);
    connect(defaultBtn, &QPushButton::clicked, this, &ConfigDialog::onDefault);
    connect(closeBtn,   &QPushButton::clicked, this, &QDialog::accept);

    loadFromStyle();
    updateTitle();
}

// ---------------------------------------------------------------------------
// tab builders
// ---------------------------------------------------------------------------

QWidget* ConfigDialog::buildMainTab()
{
    QWidget* w = new QWidget;
    QFormLayout* f = new QFormLayout(w);
    f->setRowWrapPolicy(QFormLayout::WrapLongRows);

    auto row = [&](const QString& label, QWidget* widget, const QString& tip) {
        auto* lbl = new QLabel(label);
        lbl->setToolTip(tip);
        widget->setToolTip(tip);
        f->addRow(lbl, widget);
    };

    m_enableLogging = new QCheckBox;
    row(tr("Enable logging"), m_enableLogging,
        tr("Enables debug logging output to the console."));

    m_windowSize = new QComboBox;
    m_windowSize->addItems({"default","maximum","fullscreen","mni"});
    row(tr("Window size"), m_windowSize,
        tr("Initial window size at startup: default, maximum, fullscreen, or predefined mni layout."));

    m_perspective = new QCheckBox;
    row(tr("Perspective mode"), m_perspective,
        tr("Enables perspective transformation mode for image layers."));

    m_binaryMasking = new QCheckBox;
    row(tr("Binary masking"), m_binaryMasking,
        tr("Restricts mask values to 0 or 1 (binary on/off) instead of continuous values."));

    m_crosshair = new QCheckBox;
    row(tr("Crosshair"), m_crosshair,
        tr("Shows a crosshair overlay at the cursor position in the image view."));

    m_showDocksAtStartup = new QCheckBox;
    row(tr("Show docks at startup"), m_showDocksAtStartup,
        tr("Show all dock panels automatically when the application starts."));

    m_cursorSize = new QSpinBox; m_cursorSize->setRange(0, 128);
    row(tr("Cursor size"), m_cursorSize,
        tr("Radius of the brush preview circle displayed at the cursor position, in pixels."));

    // color rows
    m_cursorFillColor = new QLineEdit;
    m_cursorFillBtn   = new QPushButton;
    connect(m_cursorFillBtn, &QPushButton::clicked, this, [this]{ pickColor(m_cursorFillColor, m_cursorFillBtn); });
    {
        QHBoxLayout* hl = new QHBoxLayout; hl->setContentsMargins(0,0,0,0); hl->addWidget(m_cursorFillColor); hl->addWidget(m_cursorFillBtn);
        QWidget* cw = new QWidget; cw->setLayout(hl);
        row(tr("Cursor fill color"), cw, tr("Fill color of the brush preview cursor circle."));
    }

    m_cursorBorderColor = new QLineEdit;
    m_cursorBorderBtn   = new QPushButton;
    connect(m_cursorBorderBtn, &QPushButton::clicked, this, [this]{ pickColor(m_cursorBorderColor, m_cursorBorderBtn); });
    {
        QHBoxLayout* hl = new QHBoxLayout; hl->setContentsMargins(0,0,0,0); hl->addWidget(m_cursorBorderColor); hl->addWidget(m_cursorBorderBtn);
        QWidget* cw = new QWidget; cw->setLayout(hl);
        row(tr("Cursor border color"), cw, tr("Border color of the brush preview cursor circle."));
    }

    m_githubBaseUrl = new QLineEdit;
    m_githubBaseUrl->setPlaceholderText("https://raw.githubusercontent.com/...");
    row(tr("GitHub base URL (github://)"), m_githubBaseUrl,
        tr("Base URL used to resolve github:// protocol paths when loading remote resources."));

    return w;
}

QWidget* ConfigDialog::buildCageTab()
{
    QWidget* w = new QWidget;
    QFormLayout* f = new QFormLayout(w);
    f->setRowWrapPolicy(QFormLayout::WrapLongRows);

    auto row = [&](const QString& label, QWidget* widget, const QString& tip) {
        auto* lbl = new QLabel(label);
        lbl->setToolTip(tip);
        widget->setToolTip(tip);
        f->addRow(lbl, widget);
    };

    m_claudeQuads = new QCheckBox;
    row(tr("Claude quads"), m_claudeQuads,
        tr("Uses Claude's quad subdivision algorithm for cage warping."));

    m_cageQuads = new QCheckBox;
    row(tr("Cage quads"), m_cageQuads,
        tr("Activates quad-based cage control instead of triangulation."));

    m_gpu = new QCheckBox;
    row(tr("Use GPU"), m_gpu,
        tr("Enables GPU-accelerated cage warp rendering via OpenGL."));

    m_gpuCatmullRom = new QCheckBox;
    row(tr("GPU Catmull-Rom interpolation"), m_gpuCatmullRom,
        tr("Uses Catmull-Rom spline interpolation on the GPU for smoother warp results."));

    m_liveWarp = new QCheckBox;
    row(tr("Live warp (GPU only)"), m_liveWarp,
        tr("Updates the warp result interactively while dragging control points (requires GPU)."));

    m_noSelfIntersection = new QCheckBox;
    row(tr("No self-intersection"), m_noSelfIntersection,
        tr("Prevents cage control points from crossing each other during warping."));

    m_cpRadius = new QSpinBox; m_cpRadius->setRange(1, 32);
    row(tr("Control point radius"), m_cpRadius,
        tr("Display radius of cage control point handles in pixels."));

    auto makeColorRow = [&](const QString& label, const QString& tip, QLineEdit*& edit, QPushButton*& btn) {
        edit = new QLineEdit; btn = new QPushButton;
        connect(btn, &QPushButton::clicked, this, [this, edit, btn]{ pickColor(edit, btn); });
        QHBoxLayout* hl = new QHBoxLayout; hl->setContentsMargins(0,0,0,0); hl->addWidget(edit); hl->addWidget(btn);
        QWidget* cw = new QWidget; cw->setLayout(hl);
        row(label, cw, tip);
    };
    makeColorRow(tr("Control point color"), tr("Color used to draw cage control point handles."),
                 m_cpColor, m_cpColorBtn);
    makeColorRow(tr("Grid color"), tr("Color of the cage grid lines."),
                 m_gridColor, m_gridColorBtn);
    makeColorRow(tr("Cage warp color"), tr("Color of the cage warp boundary outline."),
                 m_cageColor, m_cageColorBtn);

    m_cageGridCols = new QSpinBox; m_cageGridCols->setRange(3, 33); m_cageGridCols->setSingleStep(1);
    row(tr("Grid columns"), m_cageGridCols,
        tr("Number of columns in the cage control grid."));

    m_squareCageQuads = new QCheckBox;
    row(tr("Square quads (auto rows)"), m_squareCageQuads,
        tr("Automatically sets the number of rows so each cage quad is approximately square."));

    if ( !GpuInfo::query().available ) {
        const QString noGpuTip = tr("No OpenGL context available on this system");
        for ( QWidget* gw : { (QWidget*)m_gpu, (QWidget*)m_gpuCatmullRom, (QWidget*)m_liveWarp } ) {
            gw->setEnabled(false);
            gw->setToolTip(noGpuTip);
        }
    }

    return w;
}

QWidget* ConfigDialog::buildScaleTab()
{
    QWidget* w = new QWidget;
    QFormLayout* f = new QFormLayout(w);

    auto row = [&](const QString& label, QWidget* widget, const QString& tip) {
        auto* lbl = new QLabel(label);
        lbl->setToolTip(tip);
        widget->setToolTip(tip);
        f->addRow(lbl, widget);
    };

    m_handleColor    = new QLineEdit;
    m_handleColorBtn = new QPushButton;
    connect(m_handleColorBtn, &QPushButton::clicked, this, [this]{ pickColor(m_handleColor, m_handleColorBtn); });
    {
        QHBoxLayout* hl = new QHBoxLayout; hl->setContentsMargins(0,0,0,0); hl->addWidget(m_handleColor); hl->addWidget(m_handleColorBtn);
        QWidget* cw = new QWidget; cw->setLayout(hl);
        row(tr("Handle color"), cw, tr("Color of the scale and rotate transform handles."));
    }

    m_handleSize = new QSpinBox; m_handleSize->setRange(1, 64);
    row(tr("Handle size"), m_handleSize,
        tr("Size of the scale and rotate transform handles in pixels."));

    return w;
}

QWidget* ConfigDialog::buildLassoTab()
{
    QWidget* w = new QWidget;
    QFormLayout* f = new QFormLayout(w);

    auto row = [&](const QString& label, QWidget* widget, const QString& tip) {
        auto* lbl = new QLabel(label);
        lbl->setToolTip(tip);
        widget->setToolTip(tip);
        f->addRow(lbl, widget);
    };

    m_lassoColor    = new QLineEdit;
    m_lassoColorBtn = new QPushButton;
    connect(m_lassoColorBtn, &QPushButton::clicked, this, [this]{ pickColor(m_lassoColor, m_lassoColorBtn); });
    {
        QHBoxLayout* hl = new QHBoxLayout; hl->setContentsMargins(0,0,0,0); hl->addWidget(m_lassoColor); hl->addWidget(m_lassoColorBtn);
        QWidget* cw = new QWidget; cw->setLayout(hl);
        row(tr("Lasso color"), cw, tr("Color of the freehand lasso selection outline."));
    }

    m_lassoWidth = new QSpinBox; m_lassoWidth->setRange(0, 20);
    row(tr("Lasso width"), m_lassoWidth,
        tr("Line width of the lasso selection outline in pixels."));

    return w;
}

QWidget* ConfigDialog::buildPolygonTab()
{
    QWidget* w = new QWidget;
    QFormLayout* f = new QFormLayout(w);

    auto row = [&](const QString& label, QWidget* widget, const QString& tip) {
        auto* lbl = new QLabel(label);
        lbl->setToolTip(tip);
        widget->setToolTip(tip);
        f->addRow(lbl, widget);
    };

    m_polygonWidth = new QSpinBox; m_polygonWidth->setRange(0, 50);
    row(tr("Polygon width"), m_polygonWidth,
        tr("Line width of the polygon outline in pixels."));

    m_polygonHandleSize = new QSpinBox; m_polygonHandleSize->setRange(1, 64);
    row(tr("Handle size"), m_polygonHandleSize,
        tr("Size of polygon vertex handles in pixels."));

    m_polygonHandleColor    = new QLineEdit;
    m_polygonHandleColorBtn = new QPushButton;
    connect(m_polygonHandleColorBtn, &QPushButton::clicked, this, [this]{ pickColor(m_polygonHandleColor, m_polygonHandleColorBtn); });
    {
        QHBoxLayout* hl = new QHBoxLayout; hl->setContentsMargins(0,0,0,0); hl->addWidget(m_polygonHandleColor); hl->addWidget(m_polygonHandleColorBtn);
        QWidget* cw = new QWidget; cw->setLayout(hl);
        row(tr("Handle color"), cw, tr("Color of the polygon vertex handles."));
    }
    return w;
}

QWidget* ConfigDialog::buildImageLayerTab()
{
    QWidget* w = new QWidget;
    QFormLayout* f = new QFormLayout(w);
    f->setRowWrapPolicy(QFormLayout::WrapLongRows);

    auto row = [&](const QString& label, QWidget* widget, const QString& tip) {
        auto* lbl = new QLabel(label);
        lbl->setToolTip(tip);
        widget->setToolTip(tip);
        f->addRow(lbl, widget);
    };

    m_integerMoveOnly = new QCheckBox;
    row(tr("Integer move only"), m_integerMoveOnly,
        tr("Restricts layer movement to whole pixel positions only."));

    m_overlayOpacity = new QDoubleSpinBox;
    m_overlayOpacity->setRange(0.0, 1.0); m_overlayOpacity->setSingleStep(0.05); m_overlayOpacity->setDecimals(2);
    row(tr("Overlay opacity"), m_overlayOpacity,
        tr("Opacity of the semi-transparent red overlay shown when Ctrl+dragging a layer."));

    m_rotationStep = new QDoubleSpinBox;
    m_rotationStep->setRange(0.01, 90.0); m_rotationStep->setSingleStep(0.5); m_rotationStep->setDecimals(2);
    row(tr("Rotation single step"), m_rotationStep,
        tr("Angle increment per step when rotating a layer with the rotation handle, in degrees."));

    m_handleRadius = new QDoubleSpinBox;
    m_handleRadius->setRange(1.0, 50.0); m_handleRadius->setSingleStep(0.5); m_handleRadius->setDecimals(1);
    row(tr("Handle radius"), m_handleRadius,
        tr("Radius of the rotation and scale transform handles in pixels."));

    m_transformMode = new QComboBox;
    m_transformMode->addItems({"fast", "smooth"});
    row(tr("Transformation mode"), m_transformMode,
        tr("Rendering quality when transforming a layer: fast (nearest-neighbour) or smooth (bilinear)."));

    m_interpMode = new QComboBox;
    m_interpMode->addItems({"nearest", "linear", "bicubic"});
    row(tr("Interpolation mode"), m_interpMode,
        tr("Pixel interpolation used when scaling or rotating layers: nearest, linear, or bicubic."));

    return w;
}

// ---------------------------------------------------------------------------
// load / apply
// ---------------------------------------------------------------------------

void ConfigDialog::loadFromStyle()
{
    const EditorStyle& s = EditorStyle::instance();

    // Main
    m_enableLogging->setChecked(s.isLoggingEnabled());
    {
        int idx = m_windowSize->findText(s.windowSize());
        m_windowSize->setCurrentIndex(idx >= 0 ? idx : 0);
    }
    m_perspective->setChecked(s.hasPerspective());
    m_binaryMasking->setChecked(s.binaryMasking());
    m_crosshair->setChecked(s.crosshair());
    m_showDocksAtStartup->setChecked(s.showDocksAtStartup());
    m_cursorSize->setValue(s.cursorSize());
    m_cursorFillColor->setText(s.cursorFillColor().name());
    styleColorButton(m_cursorFillBtn, s.cursorFillColor().name());
    m_cursorBorderColor->setText(s.cursorBorderColor().name());
    styleColorButton(m_cursorBorderBtn, s.cursorBorderColor().name());
    m_githubBaseUrl->setText(s.githubBaseUrl());

    // Cage
    m_claudeQuads->setChecked(s.useClaudeQuads());
    m_cageQuads->setChecked(s.useCageQuads());
    m_gpu->setChecked(s.useGPU());
    m_gpuCatmullRom->setChecked(s.gpuCatmullRom());
    m_liveWarp->setChecked(s.liveWarp());
    m_noSelfIntersection->setChecked(s.noSelfIntersection());
    m_cageGridCols->setValue(s.cageGridCols());
    m_squareCageQuads->setChecked(s.squareCageQuads());
    m_cpRadius->setValue(s.controlPointRadius());
    m_cpColor->setText(s.controlPointColor().name());
    styleColorButton(m_cpColorBtn,   s.controlPointColor().name());
    m_gridColor->setText(s.cageGridColor().name());
    styleColorButton(m_gridColorBtn, s.cageGridColor().name());
    m_cageColor->setText(s.cageWarpColor().name());
    styleColorButton(m_cageColorBtn, s.cageWarpColor().name());

    // Scale
    m_handleColor->setText(s.getHandleColor().name());
    styleColorButton(m_handleColorBtn, s.getHandleColor().name());
    m_handleSize->setValue(s.getHandleSize());

    // Lasso
    m_lassoColor->setText(s.lassoColor().name());
    styleColorButton(m_lassoColorBtn, s.lassoColor().name());
    m_lassoWidth->setValue(s.lassoWidth());

    // Polygon
    m_polygonWidth->setValue(s.polygonWidth());
    m_polygonHandleSize->setValue(s.polygonHandleSize());
    m_polygonHandleColor->setText(s.polygonHandleColor().name());
    styleColorButton(m_polygonHandleColorBtn, s.polygonHandleColor().name());

    // ImageLayer
    m_integerMoveOnly->setChecked(s.allowIntegerMoveOnly());
    m_overlayOpacity->setValue(s.layerOverlayOpacity());
    m_rotationStep->setValue(s.rotationSingleStep());
    m_handleRadius->setValue(s.handleRadius());
    {
        int idx = m_transformMode->findText(s.transformationMode() == Qt::FastTransformation ? "fast" : "smooth");
        m_transformMode->setCurrentIndex(idx >= 0 ? idx : 0);
    }
    {
        QString im;
        switch (s.interpolationMode()) {
            case EditorStyle::InterpolationMode::Nearest: im = "nearest"; break;
            case EditorStyle::InterpolationMode::Bicubic: im = "bicubic"; break;
            default:                                      im = "linear";  break;
        }
        int idx = m_interpMode->findText(im);
        m_interpMode->setCurrentIndex(idx >= 0 ? idx : 1);
    }
}

void ConfigDialog::applyToStyle()
{
    EditorStyle& s = EditorStyle::instance();

    // Main
    s.setLoggingEnabled(m_enableLogging->isChecked());
    s.setWindowSize(m_windowSize->currentText());
    s.setHasPerspective(m_perspective->isChecked());
    s.setBinaryMasking(m_binaryMasking->isChecked());
    s.setCrosshair(m_crosshair->isChecked());
    s.setShowDocksAtStartup(m_showDocksAtStartup->isChecked());
    s.setCursorSize(m_cursorSize->value());
    { QColor c(m_cursorFillColor->text());   if (c.isValid()) s.setCursorFillColor(c); }
    { QColor c(m_cursorBorderColor->text()); if (c.isValid()) s.setCursorBorderColor(c); }
    if (!m_githubBaseUrl->text().isEmpty()) s.setGithubBaseUrl(m_githubBaseUrl->text());

    // Cage
    s.setUseClaudeQuads(m_claudeQuads->isChecked());
    s.setUseCageQuads(m_cageQuads->isChecked());
    s.setUseGPU(m_gpu->isChecked());
    s.setGpuCatmullRom(m_gpuCatmullRom->isChecked());
    s.setLiveWarp(m_liveWarp->isChecked());
    s.setNoSelfIntersection(m_noSelfIntersection->isChecked());
    s.setCageGridCols(m_cageGridCols->value());
    s.setSquareCageQuads(m_squareCageQuads->isChecked());
    s.setControlPointRadius(m_cpRadius->value());
    { QColor c(m_cpColor->text());   if (c.isValid()) s.setControlPointColor(c); }
    { QColor c(m_gridColor->text()); if (c.isValid()) s.setCageGridColor(c); }
    { QColor c(m_cageColor->text()); if (c.isValid()) s.setCageWarpColor(c); }

    // Scale
    { QColor c(m_handleColor->text()); if (c.isValid()) s.setHandleColor(c); }
    s.setHandleSize(m_handleSize->value());

    // Lasso
    { QColor c(m_lassoColor->text()); if (c.isValid()) s.setLassoColor(c); }
    s.setLassoWidth(m_lassoWidth->value());

    // Polygon
    s.setPolygonWidth(m_polygonWidth->value());
    s.setPolygonHandleSize(m_polygonHandleSize->value());
    { QColor c(m_polygonHandleColor->text()); if (c.isValid()) s.setPolygonHandleColor(c); }

    // ImageLayer
    s.setAllowIntegerMoveOnly(m_integerMoveOnly->isChecked());
    s.setLayerOverlayOpacity(m_overlayOpacity->value());
    s.setRotationSingleStep(m_rotationStep->value());
    s.setHandleRadius(m_handleRadius->value());
    s.setTransformationMode(m_transformMode->currentText() == "fast"
                            ? Qt::FastTransformation : Qt::SmoothTransformation);
    {
        const QString im = m_interpMode->currentText();
        if (im == "nearest")      s.setInterpolationMode(EditorStyle::InterpolationMode::Nearest);
        else if (im == "bicubic") s.setInterpolationMode(EditorStyle::InterpolationMode::Bicubic);
        else                      s.setInterpolationMode(EditorStyle::InterpolationMode::Linear);
    }
}

// ---------------------------------------------------------------------------
// slots
// ---------------------------------------------------------------------------

void ConfigDialog::done(int result)
{
    applyToStyle();
    QDialog::done(result);
}

void ConfigDialog::updateTitle()
{
    const QString path = EditorStyle::instance().path();
    if (path.isEmpty())
        setWindowTitle(tr("Configuration"));
    else
        setWindowTitle(tr("Configuration — %1").arg(QFileInfo(path).fileName()));
}

void ConfigDialog::onSave()
{
    applyToStyle();

    const QString current = EditorStyle::instance().path();
    const QString path = QFileDialog::getSaveFileName(
        this, tr("Save config file"), current, tr("INI files (*.ini);;All files (*)"));
    if (path.isEmpty()) return;
    EditorStyle::instance().setPath(path);
    EditorStyle::instance().save();
    updateTitle();
    QMessageBox::information(this, tr("Config saved"), tr("Configuration saved to:\n%1").arg(path));
}

void ConfigDialog::onDefault()
{
    EditorStyle::instance().resetToDefaults();
    loadFromStyle();
}

void ConfigDialog::onLoad()
{
    const QString current = EditorStyle::instance().path();
    const QString path = QFileDialog::getOpenFileName(
        this, tr("Load config file"), current, tr("INI files (*.ini);;All files (*)"));
    if (path.isEmpty()) return;
    EditorStyle::instance().load(path);
    loadFromStyle();
    updateTitle();
}

// ---------------------------------------------------------------------------
// color picker helper
// ---------------------------------------------------------------------------

void ConfigDialog::pickColor(QLineEdit* edit, QPushButton* btn)
{
    QColor initial(edit->text());
    if (!initial.isValid()) initial = Qt::white;
    QColor c = QColorDialog::getColor(initial, this, tr("Choose color"));
    if (c.isValid()) {
        edit->setText(c.name());
        styleColorButton(btn, c.name());
    }
}
