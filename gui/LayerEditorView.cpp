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

#include "LayerEditorView.h"

#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsPixmapItem>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QSpinBox>
#include <QLabel>
#include <QUndoCommand>
#include <QKeySequence>
#include <QShortcut>
#include <QScrollBar>
#include <QWheelEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPixmap>
#include <QTimer>
#include <QSvgRenderer>
#include <QFileDialog>
#include <QMessageBox>
#include <cmath>

// ---------------------------------------------------------------------------
// Undo command for a single erase stroke
// ---------------------------------------------------------------------------

class EraseCommand : public QUndoCommand
{
public:
    EraseCommand(LayerEditorView* view, QImage before, QImage after)
        : m_view(view), m_before(std::move(before)), m_after(std::move(after))
    {
        setText("Erase");
    }
    void undo() override { m_view->restoreImage(m_before); }
    void redo() override { m_view->restoreImage(m_after);  }

private:
    LayerEditorView* m_view;
    QImage m_before;
    QImage m_after;
};

// ---------------------------------------------------------------------------
// Internal view: wheel zoom, checkerboard, eraser cursor overlay
// ---------------------------------------------------------------------------

class EditorGraphicsView : public QGraphicsView
{
    Q_OBJECT
public:
    explicit EditorGraphicsView(QGraphicsScene* scene, QWidget* parent = nullptr)
        : QGraphicsView(scene, parent)
    {
        setRenderHint(QPainter::SmoothPixmapTransform);
        setDragMode(QGraphicsView::ScrollHandDrag);
        setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
        setResizeAnchor(QGraphicsView::AnchorViewCenter);
        setStyleSheet("QGraphicsView { border: none; }");

        QPixmap pix(32, 32);
        pix.fill(QColor("#3a3a3a"));
        QPainter p(&pix);
        p.fillRect(0,  0,  16, 16, QColor("#2a2a2a"));
        p.fillRect(16, 16, 16, 16, QColor("#2a2a2a"));
        p.end();
        setBackgroundBrush(QBrush(pix));
    }

    void setEraserMode(bool active)
    {
        m_eraserMode   = active;
        m_cursorOnView = false;
        setDragMode(active ? QGraphicsView::NoDrag : QGraphicsView::ScrollHandDrag);
        setCursor(active ? Qt::BlankCursor : Qt::ArrowCursor);
        viewport()->update();
    }

    void setEraserRadius(int r) { m_eraserRadius = r; viewport()->update(); }
    void setPickerMode(bool active)
    {
        m_pickerMode = active;
        if (active)
            setCursor(Qt::CrossCursor);
        else if (!m_eraserMode)
            setCursor(Qt::ArrowCursor);
    }

signals:
    void sceneMousePressed(QPointF scenePos);
    void sceneMouseMoved(QPointF scenePos, Qt::MouseButtons buttons);
    void sceneMouseReleased();
    void scenePointerMoved(QPointF scenePos);  // emitted on every mouse move
    void scaleChanged(double scale);

protected:
    // Draw eraser circle as a foreground overlay in scene coordinates.
    // A cosmetic pen keeps the outline 1 px regardless of zoom.
    void drawForeground(QPainter* painter, const QRectF&) override
    {
        if (!m_eraserMode || !m_cursorOnView) return;

        const qreal r = m_eraserRadius;
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);

        // White solid ring
        QPen white(Qt::white, 1);
        white.setCosmetic(true);
        painter->setPen(white);
        painter->setBrush(Qt::NoBrush);
        painter->drawEllipse(m_cursorScenePos, r, r);

        // Black dashed ring on top for contrast
        QPen black(Qt::black, 1, Qt::DashLine);
        black.setCosmetic(true);
        painter->setPen(black);
        painter->drawEllipse(m_cursorScenePos, r, r);

        painter->restore();
    }

    void wheelEvent(QWheelEvent* e) override
    {
        const double factor = e->angleDelta().y() > 0 ? 1.15 : 1.0 / 1.15;
        scale(factor, factor);
        emit scaleChanged(transform().m11());
        e->accept();
    }

    void mousePressEvent(QMouseEvent* e) override
    {
        // Shift+LMB: pan the canvas regardless of eraser mode
        if (e->button() == Qt::LeftButton && (e->modifiers() & Qt::ShiftModifier)) {
            m_panning     = true;
            m_panLastPos  = e->pos();
            setCursor(Qt::ClosedHandCursor);
            e->accept();
            return;
        }
        if (m_eraserMode && e->button() == Qt::LeftButton) {
            m_cursorScenePos = mapToScene(e->pos());
            emit sceneMousePressed(m_cursorScenePos);
            e->accept();
            return;
        }
        QGraphicsView::mousePressEvent(e);
    }

    void mouseMoveEvent(QMouseEvent* e) override
    {
        // Shift+LMB pan
        if (m_panning && (e->buttons() & Qt::LeftButton)) {
            const QPoint delta = e->pos() - m_panLastPos;
            horizontalScrollBar()->setValue(horizontalScrollBar()->value() - delta.x());
            verticalScrollBar()->setValue(verticalScrollBar()->value() - delta.y());
            m_panLastPos = e->pos();
            e->accept();
            return;
        }
        m_cursorScenePos = mapToScene(e->pos());
        m_cursorOnView   = true;
        emit scenePointerMoved(m_cursorScenePos);   // always — for color picker
        if (m_eraserMode) {
            emit sceneMouseMoved(m_cursorScenePos, e->buttons());
            viewport()->update();
            // Do NOT swallow the event — QGraphicsView must track the cursor
            // position internally for AnchorUnderMouse to work correctly on wheel zoom.
        }
        QGraphicsView::mouseMoveEvent(e);
    }

    void mouseReleaseEvent(QMouseEvent* e) override
    {
        if (m_panning && e->button() == Qt::LeftButton) {
            m_panning = false;
            setCursor(m_eraserMode ? Qt::BlankCursor : Qt::ArrowCursor);
            e->accept();
            return;
        }
        if (m_eraserMode && e->button() == Qt::LeftButton) {
            emit sceneMouseReleased();
            e->accept();
            return;
        }
        QGraphicsView::mouseReleaseEvent(e);
    }

    void enterEvent(QEnterEvent* e) override
    {
        m_cursorOnView = true;
        QGraphicsView::enterEvent(e);
    }

    void leaveEvent(QEvent* e) override
    {
        m_cursorOnView = false;
        viewport()->update();
        QGraphicsView::leaveEvent(e);
    }

private:
    bool    m_eraserMode    = false;
    bool    m_pickerMode    = false;
    bool    m_cursorOnView  = false;
    bool    m_panning       = false;
    int     m_eraserRadius  = 10;
    QPointF m_cursorScenePos;
    QPoint  m_panLastPos;
};

// ---------------------------------------------------------------------------
// LayerEditorView
// ---------------------------------------------------------------------------

LayerEditorView::LayerEditorView(QWidget* parent)
    : QWidget(parent)
    , m_undoStack(new QUndoStack(this))
{
    m_scene = new QGraphicsScene(this);
    m_item  = new QGraphicsPixmapItem;
    m_scene->addItem(m_item);
    auto* editorView = new EditorGraphicsView(m_scene, this);
    m_view = editorView;

    // --- toolbar ---
    auto* toolbar = new QWidget(this);
    toolbar->setFixedHeight(36);
    toolbar->setObjectName("layerEditorToolbar");
    toolbar->setAutoFillBackground(true);
    toolbar->setStyleSheet("#layerEditorToolbar { background-color: #484848; border-bottom: 1px solid #2a2a2a; }");
    auto* tbLayout = new QHBoxLayout(toolbar);
    tbLayout->setContentsMargins(6, 2, 6, 2);
    tbLayout->setSpacing(4);

    auto makeBtn = [&](const QString& label) {
        auto* b = new QPushButton(label, toolbar);
        b->setFixedSize(28, 26);
        return b;
    };

    auto* btnPlus  = makeBtn("+");
    auto* btnMinus = makeBtn("−");
    auto* btnHome  = makeBtn("⌂");

    m_toggleBtn = new QPushButton(tr("Mask"), toolbar);
    m_toggleBtn->setFixedSize(90, 26);
    m_toggleBtn->setCheckable(true);
    m_toggleBtn->setChecked(false);

    auto svgIcon = [](const QString& path) -> QIcon {
        QSvgRenderer renderer(path);
        QPixmap pm(16, 16);
        pm.fill(Qt::transparent);
        QPainter p(&pm);
        renderer.render(&p);
        return QIcon(pm);
    };

    // Eraser button with SVG icon
    m_eraserBtn = new QPushButton(toolbar);
    m_eraserBtn->setFixedSize(28, 26);
    m_eraserBtn->setCheckable(true);
    m_eraserBtn->setToolTip(tr("Eraser (E)"));
    m_eraserBtn->setIcon(svgIcon(":/icons/icons/eraser.svg"));

    // Undo / Redo buttons
    m_undoBtn = new QPushButton(toolbar);
    m_undoBtn->setFixedSize(28, 26);
    m_undoBtn->setToolTip(tr("Undo (Ctrl+Z)"));
    m_undoBtn->setIcon(svgIcon(":/icons/icons/undo.svg"));
    m_undoBtn->setEnabled(false);

    m_redoBtn = new QPushButton(toolbar);
    m_redoBtn->setFixedSize(28, 26);
    m_redoBtn->setToolTip(tr("Redo (Ctrl+Shift+Z)"));
    m_redoBtn->setIcon(svgIcon(":/icons/icons/redo.svg"));
    m_redoBtn->setEnabled(false);

    m_pickerBtn = new QPushButton(toolbar);
    m_pickerBtn->setFixedSize(28, 26);
    m_pickerBtn->setCheckable(true);
    m_pickerBtn->setToolTip(tr("Color Picker"));
    m_pickerBtn->setIcon(svgIcon(":/icons/icons/colorpicker.svg"));

    m_colorSwatch = new QLabel(toolbar);
    m_colorSwatch->setFixedSize(26, 22);
    m_colorSwatch->setStyleSheet("background-color: transparent; border: 1px solid #888;");
    m_colorSwatch->setToolTip(tr("Sampled color"));

    auto* sizeLabel = new QLabel(tr("Size:"), toolbar);
    sizeLabel->setContentsMargins(6, 0, 2, 0);
    sizeLabel->setStyleSheet("background-color: transparent;");
    m_eraserSpin = new QSpinBox(toolbar);
    m_eraserSpin->setRange(1, 300);
    m_eraserSpin->setSingleStep(1);
    m_eraserSpin->setValue(m_eraserSize);
    m_eraserSpin->setSuffix(" px");
    m_eraserSpin->setFixedHeight(26);
    m_eraserSpin->setFixedWidth(72);

    auto* thrLabel = new QLabel(tr("Threshold:"), toolbar);
    thrLabel->setContentsMargins(6, 0, 2, 0);
    thrLabel->setStyleSheet("background-color: transparent;");
    m_thresholdSpin = new QSpinBox(toolbar);
    m_thresholdSpin->setRange(0, 255);
    m_thresholdSpin->setSingleStep(1);
    m_thresholdSpin->setValue(m_threshold);
    m_thresholdSpin->setFixedHeight(26);
    m_thresholdSpin->setFixedWidth(60);
    m_thresholdSpin->setToolTip(tr("Erase only pixels with alpha < threshold"));

    m_updateBtn = new QPushButton(tr("Update"), toolbar);
    m_updateBtn->setFixedHeight(26);
    m_updateBtn->setEnabled(false);

    auto* btnSave = new QPushButton(tr("Save as…"), toolbar);
    btnSave->setFixedHeight(26);

    auto* btnQuit = new QPushButton(tr("Quit"), toolbar);
    btnQuit->setFixedHeight(26);

    tbLayout->addWidget(btnPlus);
    tbLayout->addWidget(btnMinus);
    tbLayout->addWidget(btnHome);
    tbLayout->addSpacing(8);
    tbLayout->addWidget(m_toggleBtn);
    tbLayout->addSpacing(8);
    tbLayout->addWidget(m_eraserBtn);
    tbLayout->addWidget(sizeLabel);
    tbLayout->addWidget(m_eraserSpin);
    tbLayout->addWidget(thrLabel);
    tbLayout->addWidget(m_thresholdSpin);
    tbLayout->addSpacing(8);
    tbLayout->addWidget(m_undoBtn);
    tbLayout->addWidget(m_redoBtn);
    tbLayout->addSpacing(8);
    tbLayout->addWidget(m_pickerBtn);
    tbLayout->addWidget(m_colorSwatch);
    tbLayout->addStretch();
    tbLayout->addWidget(m_updateBtn);
    tbLayout->addSpacing(4);
    tbLayout->addWidget(btnSave);
    tbLayout->addSpacing(4);
    tbLayout->addWidget(btnQuit);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(toolbar);
    layout->addWidget(m_view);

    // --- connections ---
    connect(btnPlus,    &QPushButton::clicked, this, &LayerEditorView::zoomIn);
    connect(btnMinus,   &QPushButton::clicked, this, &LayerEditorView::zoomOut);
    connect(btnHome,    &QPushButton::clicked, this, &LayerEditorView::centerImage);
    connect(m_toggleBtn,&QPushButton::clicked, this, &LayerEditorView::toggleView);
    connect(m_eraserBtn,&QPushButton::toggled, this, &LayerEditorView::toggleEraser);
    connect(m_eraserSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int v) {
        m_eraserSize = v;
        static_cast<EditorGraphicsView*>(m_view)->setEraserRadius(v / 2);
    });
    connect(m_thresholdSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int v) {
        m_threshold = v;
    });
    connect(m_undoBtn,  &QPushButton::clicked,  m_undoStack, &QUndoStack::undo);
    connect(m_redoBtn,  &QPushButton::clicked,  m_undoStack, &QUndoStack::redo);
    connect(m_undoStack,&QUndoStack::canUndoChanged, m_undoBtn, &QPushButton::setEnabled);
    connect(m_undoStack,&QUndoStack::canRedoChanged, m_redoBtn, &QPushButton::setEnabled);
    connect(m_updateBtn,&QPushButton::clicked,  this, &LayerEditorView::onUpdateClicked);
    connect(btnQuit, &QPushButton::clicked, this, [this] {
        if (m_modified) {
            QMessageBox dlg(this);
            dlg.setWindowTitle(tr("Layer Editor"));
            dlg.setText(tr("The layer currently in the workflow has not yet been updated. "
                           "Any edits made will be lost when switching layers if an update has not been "
                           "carried out beforehand. Should an update be carried out now?"));
            dlg.setIcon(QMessageBox::Warning);
            auto* yesBtn    = dlg.addButton(tr("Yes"),         QMessageBox::NoRole);
            auto* skipBtn   = dlg.addButton(tr("Skip update"), QMessageBox::NoRole);
            auto* cancelBtn = dlg.addButton(tr("Cancel"),      QMessageBox::NoRole);
            dlg.setDefaultButton(cancelBtn);
            dlg.exec();
            QAbstractButton* clicked = dlg.clickedButton();
            if (clicked == cancelBtn) return;
            if (clicked == yesBtn) emit updateRequested(m_layerImage);
            Q_UNUSED(skipBtn)
        }
        emit quitRequested();
    });

    connect(btnSave, &QPushButton::clicked, this, [this] {
        if (m_layerImage.isNull()) return;
        const QString path = QFileDialog::getSaveFileName(
            this, tr("Save Mask"),
            QString(),
            tr("PNG Image (*.png);;TIFF Image (*.tif *.tiff);;BMP Image (*.bmp)"));
        if (path.isEmpty()) return;
        if (!m_layerImage.save(path))
            QMessageBox::warning(this, tr("Save Mask"), tr("Could not save image to:\n%1").arg(path));
    });
    connect(editorView, &EditorGraphicsView::scaleChanged, this, &LayerEditorView::scaleChanged);

    // Keyboard shortcut: E toggles eraser
    auto* shortcutE = new QShortcut(QKeySequence("E"), this);
    connect(shortcutE, &QShortcut::activated, this, [this] {
        m_eraserBtn->setChecked(!m_eraserBtn->isChecked());
    });

    // Undo / redo shortcuts
    auto* shortcutUndo = new QShortcut(QKeySequence::Undo, this);
    connect(shortcutUndo, &QShortcut::activated, m_undoStack, &QUndoStack::undo);
    auto* shortcutRedo = new QShortcut(QKeySequence::Redo, this);
    connect(shortcutRedo, &QShortcut::activated, m_undoStack, &QUndoStack::redo);

    // Color picker button + swatch
    connect(m_pickerBtn, &QPushButton::toggled, this, &LayerEditorView::togglePicker);

    connect(editorView, &EditorGraphicsView::scenePointerMoved,
            this, [this](QPointF pos) {
                if (!m_pickerActive) return;

                if (m_layerImage.isNull()) return;
                const int x = qRound(pos.x());
                const int y = qRound(pos.y());
                if (x < 0 || y < 0 || x >= m_layerImage.width() || y >= m_layerImage.height()) return;

                QColor color;
                QString text;

                if (m_showComposite) {
                    // Composite (Image+Mask) mode:
                    // ARGB32 layer: scene shows m_layerImage directly (RGB = main image crop).
                    // Grayscale layer: scene shows m_mainImage composited.
                    const QImage& src2 = (m_layerImage.hasAlphaChannel() || m_mainImage.isNull()
                                          || x >= m_mainImage.width() || y >= m_mainImage.height())
                                         ? m_layerImage : m_mainImage;
                    const QRgb px = src2.pixel(x, y);
                    const int r = qRed(px), g = qGreen(px), b = qBlue(px);
                    color = QColor(r, g, b);
                    text = QString("R:%1 G:%2 B:%3").arg(r).arg(g).arg(b);
                } else {
                    // Mask mode: show mask value (alpha or gray) always as R:x G:x B:x.
                    int grayVal = 0;
                    if (m_layerImage.hasAlphaChannel()) {
                        grayVal = qAlpha(m_layerImage.pixel(x, y));
                    } else if (m_layerImage.format() == QImage::Format_Grayscale8) {
                        grayVal = *(m_layerImage.constScanLine(y) + x);
                    } else {
                        const QRgb px = m_layerImage.pixel(x, y);
                        grayVal = qGray(qRed(px), qGreen(px), qBlue(px));
                    }
                    color = QColor(grayVal, grayVal, grayVal);
                    text = QString("R:%1 G:%2 B:%3").arg(grayVal).arg(grayVal).arg(grayVal);
                }

                m_colorSwatch->setStyleSheet(
                    QString("background-color: rgb(%1,%2,%3); border: 1px solid #888;")
                        .arg(color.red()).arg(color.green()).arg(color.blue()));
                // Coordinates relative to the full image (layer origin offset applied)
                emit pickerSampled(x + m_layerOrigin.x(), y + m_layerOrigin.y(), color, text);
            });

    // Eraser mouse signals
    connect(editorView, &EditorGraphicsView::sceneMousePressed,
            this, [this](QPointF pos) {
                m_erasing         = true;
                m_lastEraserValid = false;
                // Snapshot the image before the stroke begins
                m_strokeSnapshot  = m_layerImage.copy();
                applyEraser(pos);
                m_lastEraserPos   = pos;
                m_lastEraserValid = true;
            });

    connect(editorView, &EditorGraphicsView::sceneMouseMoved,
            this, [this](QPointF pos, Qt::MouseButtons btns) {
                if (!m_erasing || !(btns & Qt::LeftButton)) return;
                if (m_lastEraserValid) {
                    const QPointF delta = pos - m_lastEraserPos;
                    const double dist   = std::sqrt(delta.x()*delta.x() + delta.y()*delta.y());
                    const int steps     = qMax(1, (int)(dist / (m_eraserSize * 0.5)));
                    for (int i = 1; i <= steps; ++i)
                        applyEraser(m_lastEraserPos + delta * (double(i) / steps));
                } else {
                    applyEraser(pos);
                }
                m_lastEraserPos   = pos;
                m_lastEraserValid = true;
            });

    connect(editorView, &EditorGraphicsView::sceneMouseReleased,
            this, [this]() {
                if (!m_erasing) return;
                m_erasing         = false;
                m_lastEraserValid = false;
                // Push one undo command per stroke
                if (m_layerImage != m_strokeSnapshot)
                    m_undoStack->push(new EraseCommand(this, m_strokeSnapshot, m_layerImage));
            });
}

// ---------------------------------------------------------------------------

void LayerEditorView::setImages(const QImage& layerImage, const QImage& mainImage,
                                QPoint layerOrigin, bool preserveViewMode)
{
    m_layerImage  = layerImage;
    m_mainImage   = mainImage;
    m_layerOrigin = layerOrigin;
    m_modified = false;
    m_updateBtn->setEnabled(false);
    m_undoStack->clear();
    if (!preserveViewMode) {
        m_showComposite = false;
        m_toggleBtn->setChecked(false);
        m_toggleBtn->setText(tr("Mask"));
    }
    showCurrentMode();
    QTimer::singleShot(0, this, [this]{ centerImage(); });
}

// ---------------------------------------------------------------------------
// Eraser
// ---------------------------------------------------------------------------

void LayerEditorView::toggleEraser(bool active)
{
    auto* ev = static_cast<EditorGraphicsView*>(m_view);
    ev->setEraserMode(active);
    ev->setEraserRadius(m_eraserSize / 2);
    if (active && m_pickerBtn && m_pickerBtn->isChecked())
        m_pickerBtn->setChecked(false);
}

void LayerEditorView::togglePicker(bool active)
{
    m_pickerActive = active;
    static_cast<EditorGraphicsView*>(m_view)->setPickerMode(active);
    if (!active)
        m_colorSwatch->setStyleSheet("background-color: transparent; border: 1px solid #888;");
    if (active && m_eraserBtn->isChecked())
        m_eraserBtn->setChecked(false);
}

void LayerEditorView::applyEraser(const QPointF& scenePos)
{
    if (m_layerImage.isNull()) return;

    if (m_layerImage.format() != QImage::Format_ARGB32)
        m_layerImage = m_layerImage.convertToFormat(QImage::Format_ARGB32);

    const int cx = qRound(scenePos.x());
    const int cy = qRound(scenePos.y());
    const int r  = m_eraserSize / 2;
    const int w  = m_layerImage.width();
    const int h  = m_layerImage.height();

    for (int dy = -r; dy <= r; ++dy) {
        const int py = cy + dy;
        if (py < 0 || py >= h) continue;
        QRgb* row = reinterpret_cast<QRgb*>(m_layerImage.scanLine(py));
        for (int dx = -r; dx <= r; ++dx) {
            if (dx*dx + dy*dy > r*r) continue;
            const int px = cx + dx;
            if (px < 0 || px >= w) continue;
            if (qGray(qRed(row[px]), qGreen(row[px]), qBlue(row[px])) <= m_threshold)
                row[px] = row[px] & 0x00FFFFFFu;   // clear alpha → transparent
        }
    }

    showCurrentMode();
}

void LayerEditorView::restoreImage(const QImage& img)
{
    m_layerImage = img;
    markModified();
    showCurrentMode();
}

void LayerEditorView::markModified()
{
    m_modified = true;
    m_updateBtn->setEnabled(true);
}

void LayerEditorView::onUpdateClicked()
{
    m_modified = false;
    m_updateBtn->setEnabled(false);
    emit updateRequested(m_layerImage);
}

// ---------------------------------------------------------------------------
// Display helpers
// ---------------------------------------------------------------------------

// Returns ARGB32 where each pixel's alpha (and gray value) equals the mask weight.
// Fully transparent pixels (alpha/mask = 0) show the checkerboard.
QImage LayerEditorView::toMaskGray(const QImage& src)
{
    if (src.isNull()) return src;

    QImage result(src.size(), QImage::Format_ARGB32);
    if (src.hasAlphaChannel()) {
        const QImage a = src.convertToFormat(QImage::Format_ARGB32);
        for (int y = 0; y < a.height(); ++y) {
            const QRgb* row = reinterpret_cast<const QRgb*>(a.constScanLine(y));
            QRgb*       dst = reinterpret_cast<QRgb*>(result.scanLine(y));
            for (int x = 0; x < a.width(); ++x) {
                const uchar v = static_cast<uchar>(qAlpha(row[x]));
                dst[x] = qRgba(v, v, v, v);
            }
        }
    } else {
        const QImage g = src.convertToFormat(QImage::Format_Grayscale8);
        for (int y = 0; y < g.height(); ++y) {
            const uchar* row = g.constScanLine(y);
            QRgb*        dst = reinterpret_cast<QRgb*>(result.scanLine(y));
            for (int x = 0; x < g.width(); ++x)
                dst[x] = qRgba(row[x], row[x], row[x], row[x]);
        }
    }
    return result;
}

QImage LayerEditorView::buildComposite() const
{
    if (m_layerImage.isNull()) return QImage();

    // ARGB32 layer: alpha channel is already the mask
    if (m_layerImage.hasAlphaChannel())
        return m_layerImage.convertToFormat(QImage::Format_ARGB32);

    // Grayscale mask: apply as alpha to the main image
    if (m_mainImage.isNull()) return m_layerImage;

    QImage mask = m_layerImage.convertToFormat(QImage::Format_Grayscale8);
    if (mask.size() != m_mainImage.size())
        mask = mask.scaled(m_mainImage.size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);

    QImage result = m_mainImage.convertToFormat(QImage::Format_ARGB32);
    for (int y = 0; y < result.height(); ++y) {
        QRgb*        dst = reinterpret_cast<QRgb*>(result.scanLine(y));
        const uchar* msk = mask.constScanLine(y);
        for (int x = 0; x < result.width(); ++x) {
            const QRgb px = dst[x];
            dst[x] = qRgba(qRed(px), qGreen(px), qBlue(px), msk[x]);
        }
    }
    return result;
}

void LayerEditorView::showCurrentMode()
{
    const QImage img = m_showComposite ? buildComposite() : toMaskGray(m_layerImage);
    m_item->setPixmap(QPixmap::fromImage(img));
    m_scene->setSceneRect(m_item->boundingRect());
}

// ---------------------------------------------------------------------------

void LayerEditorView::toggleView()
{
    m_showComposite = m_toggleBtn->isChecked();
    m_toggleBtn->setText(m_showComposite ? tr("Image+Mask") : tr("Mask"));
    showCurrentMode();
}

void LayerEditorView::zoomIn()
{
    m_view->scale(1.25, 1.25);
    emit scaleChanged(m_view->transform().m11());
}
void LayerEditorView::zoomOut()
{
    m_view->scale(1.0 / 1.25, 1.0 / 1.25);
    emit scaleChanged(m_view->transform().m11());
}
void LayerEditorView::centerImage()
{
    m_view->resetTransform();
    m_view->fitInView(m_item, Qt::KeepAspectRatio);
    emit scaleChanged(m_view->transform().m11());
}

#include "LayerEditorView.moc"
