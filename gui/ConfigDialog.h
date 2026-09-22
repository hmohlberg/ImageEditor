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

#pragma once

#include <QDialog>

class QTabWidget;
class QCheckBox;
class QSpinBox;
class QDoubleSpinBox;
class QLineEdit;
class QComboBox;
class QPushButton;
class QWidget;
class QFormLayout;

class ConfigDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ConfigDialog(QWidget* parent = nullptr);
    void loadFromStyle();

    void done(int result) override;

private slots:
    void onSave();
    void onLoad();
    void onDefault();

private:
    void applyToStyle();
    void updateTitle();
    void pickColor(QLineEdit* edit, QPushButton* btn);

    QWidget* buildMainTab();
    QWidget* buildCageTab();
    QWidget* buildScaleTab();
    QWidget* buildLassoTab();
    QWidget* buildPolygonTab();
    QWidget* buildImageLayerTab();

    // ---- Main ----
    QCheckBox*  m_enableLogging     = nullptr;
    QComboBox*  m_windowSize        = nullptr;
    QCheckBox*  m_perspective       = nullptr;
    QCheckBox*  m_binaryMasking     = nullptr;
    QCheckBox*  m_crosshair         = nullptr;
    QCheckBox*  m_showDocksAtStartup = nullptr;
    QSpinBox*   m_cursorSize        = nullptr;
    QLineEdit*  m_cursorFillColor   = nullptr;
    QPushButton* m_cursorFillBtn    = nullptr;
    QLineEdit*  m_cursorBorderColor = nullptr;
    QPushButton* m_cursorBorderBtn  = nullptr;
    QLineEdit*  m_githubBaseUrl     = nullptr;

    // ---- Cage ----
    QCheckBox* m_claudeQuads        = nullptr;
    QCheckBox* m_cageQuads          = nullptr;
    QCheckBox* m_gpu                = nullptr;
    QCheckBox* m_liveWarp           = nullptr;
    QCheckBox* m_noSelfIntersection = nullptr;
    QSpinBox*  m_cpRadius           = nullptr;
    QLineEdit* m_cpColor            = nullptr;
    QPushButton* m_cpColorBtn       = nullptr;
    QLineEdit* m_gridColor          = nullptr;
    QPushButton* m_gridColorBtn     = nullptr;
    QLineEdit* m_cageColor          = nullptr;
    QPushButton* m_cageColorBtn     = nullptr;

    // ---- Scale ----
    QLineEdit*  m_handleColor       = nullptr;
    QPushButton* m_handleColorBtn   = nullptr;
    QSpinBox*   m_handleSize        = nullptr;

    // ---- Lasso ----
    QLineEdit*  m_lassoColor        = nullptr;
    QPushButton* m_lassoColorBtn    = nullptr;
    QSpinBox*   m_lassoWidth        = nullptr;

    // ---- Polygon ----
    QSpinBox*   m_polygonWidth      = nullptr;

    // ---- ImageLayer ----
    QCheckBox*      m_integerMoveOnly    = nullptr;
    QDoubleSpinBox* m_overlayOpacity     = nullptr;
    QDoubleSpinBox* m_rotationStep       = nullptr;
    QDoubleSpinBox* m_handleRadius       = nullptr;
    QComboBox*      m_transformMode      = nullptr;
    QComboBox*      m_interpMode         = nullptr;
};
