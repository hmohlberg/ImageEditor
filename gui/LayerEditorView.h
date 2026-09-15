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

#include <QWidget>
#include <QImage>
#include <QPoint>
#include <QUndoStack>

class QGraphicsView;
class QGraphicsScene;
class QGraphicsPixmapItem;
class QPushButton;
class QSpinBox;
class QLabel;

class LayerEditorView : public QWidget
{
    Q_OBJECT

public:
    explicit LayerEditorView(QWidget* parent = nullptr);
    void setImages(const QImage& layerImage, const QImage& mainImage,
                   QPoint layerOrigin = {}, bool preserveViewMode = false);

    bool isModified() const { return m_modified; }
    QImage layerImage() const { return m_layerImage; }

    // Called by internal undo commands to restore a previous image state
    void restoreImage(const QImage& img);

signals:
    void quitRequested();
    void updateRequested(const QImage& modifiedImage);
    void pickerSampled(int x, int y, const QColor& color, const QString& text);
    void scaleChanged(double scale);

private slots:
    void zoomIn();
    void zoomOut();
    void centerImage();
    void toggleView();
    void toggleEraser(bool active);
    void togglePicker(bool active);
    void onUpdateClicked();

private:
    static QImage toMaskGray(const QImage& src);
    QImage buildComposite() const;
    void   showCurrentMode();
    void   applyEraser(const QPointF& scenePos);
    void   markModified();

    QGraphicsView*       m_view       = nullptr;
    QGraphicsScene*      m_scene      = nullptr;
    QGraphicsPixmapItem* m_item       = nullptr;
    QPushButton*         m_toggleBtn  = nullptr;
    QPushButton*         m_updateBtn  = nullptr;
    QPushButton*         m_eraserBtn  = nullptr;
    QPushButton*         m_undoBtn    = nullptr;
    QPushButton*         m_redoBtn    = nullptr;
    QPushButton*         m_pickerBtn   = nullptr;
    QLabel*              m_colorSwatch = nullptr;
    QSpinBox*            m_eraserSpin     = nullptr;
    QSpinBox*            m_thresholdSpin  = nullptr;

    QUndoStack* m_undoStack  = nullptr;

    QImage m_layerImage;
    QImage m_mainImage;
    QImage m_strokeSnapshot;   // image state at the start of an erase stroke

    bool   m_showComposite    = false;
    bool   m_modified         = false;
    bool   m_erasing          = false;
    bool   m_pickerActive     = false;
    int    m_eraserSize       = 2;
    int    m_threshold        = 255;
    QPoint m_layerOrigin;
    QPointF m_lastEraserPos;
    bool    m_lastEraserValid = false;
};
