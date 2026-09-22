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

    m_enableLogging = new QCheckBox; f->addRow(tr("Enable logging"),     m_enableLogging);
    m_windowSize    = new QComboBox; m_windowSize->addItems({"default","maximum","fullscreen","mni"});
    f->addRow(tr("Window size"), m_windowSize);
    m_perspective   = new QCheckBox; f->addRow(tr("Perspective mode"),  m_perspective);
    m_binaryMasking = new QCheckBox; f->addRow(tr("Binary masking"),    m_binaryMasking);
    m_crosshair     = new QCheckBox; f->addRow(tr("Crosshair"),         m_crosshair);
    m_showDocksAtStartup = new QCheckBox; f->addRow(tr("Show docks at startup"), m_showDocksAtStartup);
    m_cursorSize    = new QSpinBox;  m_cursorSize->setRange(0, 128);
    f->addRow(tr("Cursor size"), m_cursorSize);

    // color rows
    m_cursorFillColor   = new QLineEdit;
    m_cursorFillBtn     = new QPushButton;
    connect(m_cursorFillBtn, &QPushButton::clicked, this, [this]{ pickColor(m_cursorFillColor, m_cursorFillBtn); });
    {
        QHBoxLayout* hl = new QHBoxLayout; hl->setContentsMargins(0,0,0,0); hl->addWidget(m_cursorFillColor); hl->addWidget(m_cursorFillBtn);
        QWidget* cw = new QWidget; cw->setLayout(hl);
        f->addRow(tr("Cursor fill color"), cw);
    }
    m_cursorBorderColor = new QLineEdit;
    m_cursorBorderBtn   = new QPushButton;
    connect(m_cursorBorderBtn, &QPushButton::clicked, this, [this]{ pickColor(m_cursorBorderColor, m_cursorBorderBtn); });
    {
        QHBoxLayout* hl = new QHBoxLayout; hl->setContentsMargins(0,0,0,0); hl->addWidget(m_cursorBorderColor); hl->addWidget(m_cursorBorderBtn);
        QWidget* cw = new QWidget; cw->setLayout(hl);
        f->addRow(tr("Cursor border color"), cw);
    }

    m_githubBaseUrl = new QLineEdit;
    m_githubBaseUrl->setPlaceholderText("https://raw.githubusercontent.com/...");
    f->addRow(tr("GitHub base URL (github://)"), m_githubBaseUrl);

    return w;
}

QWidget* ConfigDialog::buildCageTab()
{
    QWidget* w = new QWidget;
    QFormLayout* f = new QFormLayout(w);
    f->setRowWrapPolicy(QFormLayout::WrapLongRows);

    m_claudeQuads = new QCheckBox; f->addRow(tr("Claude quads"),           m_claudeQuads);
    m_cageQuads   = new QCheckBox; f->addRow(tr("Cage quads"),             m_cageQuads);
    m_gpu         = new QCheckBox; f->addRow(tr("Use GPU"),                m_gpu);
    m_liveWarp           = new QCheckBox; f->addRow(tr("Live warp (GPU only)"),       m_liveWarp);
    m_noSelfIntersection = new QCheckBox; f->addRow(tr("No self-intersection"),        m_noSelfIntersection);
    m_cpRadius    = new QSpinBox;  m_cpRadius->setRange(1, 32);
    f->addRow(tr("Control point radius"), m_cpRadius);

    auto makeColorRow = [&](const QString& label, QLineEdit*& edit, QPushButton*& btn) {
        edit = new QLineEdit; btn = new QPushButton;
        connect(btn, &QPushButton::clicked, this, [this, edit, btn]{ pickColor(edit, btn); });
        QHBoxLayout* hl = new QHBoxLayout; hl->setContentsMargins(0,0,0,0); hl->addWidget(edit); hl->addWidget(btn);
        QWidget* cw = new QWidget; cw->setLayout(hl);
        f->addRow(label, cw);
    };
    makeColorRow(tr("Control point color"), m_cpColor,    m_cpColorBtn);
    makeColorRow(tr("Grid color"),           m_gridColor,  m_gridColorBtn);
    makeColorRow(tr("Cage warp color"),      m_cageColor,  m_cageColorBtn);
    return w;
}

QWidget* ConfigDialog::buildScaleTab()
{
    QWidget* w = new QWidget;
    QFormLayout* f = new QFormLayout(w);

    m_handleColor  = new QLineEdit;
    m_handleColorBtn = new QPushButton;
    connect(m_handleColorBtn, &QPushButton::clicked, this, [this]{ pickColor(m_handleColor, m_handleColorBtn); });
    {
        QHBoxLayout* hl = new QHBoxLayout; hl->setContentsMargins(0,0,0,0); hl->addWidget(m_handleColor); hl->addWidget(m_handleColorBtn);
        QWidget* cw = new QWidget; cw->setLayout(hl);
        f->addRow(tr("Handle color"), cw);
    }
    m_handleSize = new QSpinBox; m_handleSize->setRange(1, 64);
    f->addRow(tr("Handle size"), m_handleSize);
    return w;
}

QWidget* ConfigDialog::buildLassoTab()
{
    QWidget* w = new QWidget;
    QFormLayout* f = new QFormLayout(w);

    m_lassoColor    = new QLineEdit;
    m_lassoColorBtn = new QPushButton;
    connect(m_lassoColorBtn, &QPushButton::clicked, this, [this]{ pickColor(m_lassoColor, m_lassoColorBtn); });
    {
        QHBoxLayout* hl = new QHBoxLayout; hl->setContentsMargins(0,0,0,0); hl->addWidget(m_lassoColor); hl->addWidget(m_lassoColorBtn);
        QWidget* cw = new QWidget; cw->setLayout(hl);
        f->addRow(tr("Lasso color"), cw);
    }
    m_lassoWidth = new QSpinBox; m_lassoWidth->setRange(0, 20);
    f->addRow(tr("Lasso width"), m_lassoWidth);
    return w;
}

QWidget* ConfigDialog::buildPolygonTab()
{
    QWidget* w = new QWidget;
    QFormLayout* f = new QFormLayout(w);

    m_polygonWidth = new QSpinBox; m_polygonWidth->setRange(0, 50);
    f->addRow(tr("Polygon width"), m_polygonWidth);
    return w;
}

QWidget* ConfigDialog::buildImageLayerTab()
{
    QWidget* w = new QWidget;
    QFormLayout* f = new QFormLayout(w);
    f->setRowWrapPolicy(QFormLayout::WrapLongRows);

    m_integerMoveOnly = new QCheckBox; f->addRow(tr("Integer move only"), m_integerMoveOnly);

    m_overlayOpacity = new QDoubleSpinBox;
    m_overlayOpacity->setRange(0.0, 1.0); m_overlayOpacity->setSingleStep(0.05); m_overlayOpacity->setDecimals(2);
    f->addRow(tr("Overlay opacity"), m_overlayOpacity);

    m_rotationStep = new QDoubleSpinBox;
    m_rotationStep->setRange(0.01, 90.0); m_rotationStep->setSingleStep(0.5); m_rotationStep->setDecimals(2);
    f->addRow(tr("Rotation single step"), m_rotationStep);

    m_handleRadius = new QDoubleSpinBox;
    m_handleRadius->setRange(1.0, 50.0); m_handleRadius->setSingleStep(0.5); m_handleRadius->setDecimals(1);
    f->addRow(tr("Handle radius"), m_handleRadius);

    m_transformMode = new QComboBox;
    m_transformMode->addItems({"fast", "smooth"});
    f->addRow(tr("Transformation mode"), m_transformMode);

    m_interpMode = new QComboBox;
    m_interpMode->addItems({"nearest", "linear", "bicubic"});
    f->addRow(tr("Interpolation mode"), m_interpMode);

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
    m_liveWarp->setChecked(s.liveWarp());
    m_noSelfIntersection->setChecked(s.noSelfIntersection());
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
    s.setLiveWarp(m_liveWarp->isChecked());
    s.setNoSelfIntersection(m_noSelfIntersection->isChecked());
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
