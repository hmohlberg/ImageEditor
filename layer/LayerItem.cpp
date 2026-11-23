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
 
#include "LayerItem.h"
#include "Layer.h"
#include "TransformHandleItem.h"
#include "CageControlPointItem.h"
#include "CageOverlayItem.h"
#include "CageMesh.h"

#include "../core/IMainSystem.h"
#include "../gui/MainWindow.h"
#include "../gui/ImageView.h"
#include "../undo/TransformLayerCommand.h"
#include "../undo/MirrorLayerCommand.h"
#include "../undo/MoveLayerCommand.h"
#include "../undo/SetPivotCommand.h"
#include "../undo/CageWarpCommand.h"
#include "../undo/CageEditCommand.h"
#include "../util/Interpolation.h"
#include "../util/GeometryUtils.h"
#include "../util/TriangleWarp.h"
#include "../util/CageWarp.h"
#include "../util/CagePerspectiveWarp.h"

#include <QApplication>
#include <QCoreApplication>
#include <QFile>
#include <QCryptographicHash>
#include <QByteArray>
#include <QGraphicsColorizeEffect>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsScene>
#include <QPainter>
#include <QtMath>
#include <QBuffer>
#include <cmath>
#include <iostream>

// QImage::flipped(Qt::Orientations) was added in Qt 6.13.
// On older Qt use mirrored(bool h, bool v) which is identical.
// QImage::flipped(Qt::Orientations) was introduced in Qt 6.9 as a replacement
// for the then-deprecated mirrored(bool, bool). On Qt < 6.9 mirrored() is the
// correct (non-deprecated) API, so the version guard is exactly 6.9.
#if QT_VERSION >= QT_VERSION_CHECK(6, 9, 0)
#  define QIMAGE_FLIP(img, dirs) (img).flipped(dirs)
#else
static inline QImage qImageFlip(const QImage& img, Qt::Orientations dirs) {
    return img.mirrored(dirs & Qt::Horizontal, dirs & Qt::Vertical);
}
#  define QIMAGE_FLIP(img, dirs) qImageFlip(img, dirs)
#endif

// Returns a Format_Alpha8 image from an ARGB32 source when any pixel is non-opaque,
// or a null QImage when the image is fully opaque (the common case for loaded images).
static QImage extractAlphaMask(const QImage& argb32)
{
    const int w = argb32.width(), h = argb32.height();
    bool hasTransparency = false;
    for (int y = 0; y < h && !hasTransparency; ++y) {
        const QRgb* row = reinterpret_cast<const QRgb*>(argb32.constScanLine(y));
        for (int x = 0; x < w; ++x)
            if (qAlpha(row[x]) < 255) { hasTransparency = true; break; }
    }
    if (!hasTransparency) return QImage();
    QImage alpha(w, h, QImage::Format_Alpha8);
    for (int y = 0; y < h; ++y) {
        const QRgb* src = reinterpret_cast<const QRgb*>(argb32.constScanLine(y));
        uint8_t* dst = alpha.scanLine(y);
        for (int x = 0; x < w; ++x)
            dst[x] = (uint8_t)qAlpha(src[x]);
    }
    return alpha;
}

// ------------------------ LayerItem ------------------------
LayerItem::LayerItem( const QString& name, const QPixmap& pixmap, QGraphicsItem* parent ) : QGraphicsPixmapItem(pixmap,parent)
{
  qCDebug(logEditor) << "LayerItem::LayerItem(): Pixmap processing of layer " << name << "...";
  {
    m_name = name;
    m_image = pixmap.toImage();
    const QImage argb = (m_image.format() == QImage::Format_ARGB32)
                        ? m_image : m_image.convertToFormat(QImage::Format_ARGB32);
    m_alphaMask = extractAlphaMask(argb);
    m_originalImage = argb.convertToFormat(QImage::Format_Grayscale8);
    m_originalImageType = ImageType::Original;
    init();
  }
}

LayerItem::LayerItem( const QString& name, const QImage& image, QGraphicsItem* parent )
      : QGraphicsPixmapItem(parent)
      , m_name(name)
      , m_image(image)
{
  qCDebug(logEditor) << "LayerItem::LayerItem(): Image (size =" << image.size() << ") processing of layer " << name << "...";
  {
   if (image.format() == QImage::Format_Grayscale8) {
       m_originalImage = image;
   } else {
       const QImage argb = (image.format() == QImage::Format_ARGB32)
                           ? image : image.convertToFormat(QImage::Format_ARGB32);
       m_alphaMask = extractAlphaMask(argb);
       m_originalImage = argb.convertToFormat(QImage::Format_Grayscale8);
   }
   m_originalImageType = ImageType::Original;
   m_nogui = (qobject_cast<QApplication*>(qApp) == nullptr);
   if ( !m_nogui ) {
     setPixmap(QPixmap::fromImage(m_image));
     init();
   }
  }
}

LayerItem::~LayerItem()
{
    delete m_cageEditStack;
}

// >>>
QString LayerItem::operationModeName( int mode )
{
  switch ( mode ) {
   case None:             return "None";
   case Info:             return "Info";
   case CageEdit:         return "CageEdit";
   case Translate:        return "Translate";
   case Rotate:           return "Rotate";
   case Scale:            return "Scale";
   case Flip:             return "Flip";
   case Flop:             return "Flop";
   case Perspective:      return "Perspective";
   case CageWarp:         return "CageWarp";
   case Select:           return "Select";
   case MovePoint:        return "MovePoint";
   case AddPoint:         return "AddPoint";
   case DeletePoint:      return "DeletePoint";
   case TranslatePolygon: return "TranslatePolygon";
   case SmoothPolygon:    return "SmoothPolygon";
   case ReducePolygon:    return "ReducePolygon";
   case DeletePolygon:    return "DeletePolygon";
  }
  return "Unknown";
}

// >>>
void LayerItem::applyPerspective()
{
   QImage warped = m_perspective.apply(m_originalImage);
   // OLD: setPixmap(QPixmap::fromImage(warped));
   m_image = warped;
   if ( !m_nogui && qobject_cast<QApplication*>(qApp) ) {
     setPixmap(QPixmap::fromImage(warped));
   }
}

// returns unified rect of the pixmap AND the m_cageMesh
QRectF LayerItem::boundingRect() const
{
   QSize s = pixmap().isNull() ? m_image.size() : pixmap().size();
   QRectF pixmapRect(offset(),s);
   if ( m_cageMesh.isActive() ) {
      QRectF cageRect = QPolygonF(m_cageMesh.points()).boundingRect();
      QRectF united = pixmapRect.united(cageRect);
      return united;
   }
   // Expand to include the pivot cross when it lies outside the pixmap
   if ( m_hasPivot && m_operationMode == OperationMode::Rotate ) {
      const QPointF p = mapFromScene(m_pivot);
      const qreal   r = 16.0;
      pixmapRect = pixmapRect.united(QRectF(p.x() - r, p.y() - r, r * 2, r * 2));
   }
   return pixmapRect;
}

QPainterPath LayerItem::shape() const
{
    QPainterPath path = QGraphicsPixmapItem::shape();
    if ( m_hasPivot && m_operationMode == OperationMode::Rotate ) {
        const QPointF p = mapFromScene(m_pivot);
        const qreal   r = 16.0;
        path.addRect(QRectF(p.x() - r, p.y() - r, r * 2, r * 2));
    }
    return path;
}

// QGraphicsPixmapItem with BoundingRectShape bypasses virtual shape() in its contains()
// and tests against the raw pixmap rect directly.  Override contains() so that hits in
// the pivot area are recognised even when the pivot lies outside the pixmap.
bool LayerItem::contains(const QPointF& point) const
{
    const bool base = QGraphicsPixmapItem::contains(point);
    if ( base )
        return true;
    if ( m_hasPivot && m_operationMode == OperationMode::Rotate ) {
        const QPointF p = mapFromScene(m_pivot);
        const qreal   r = 16.0;
        return QRectF(p.x() - r, p.y() - r, r * 2, r * 2).contains(point);
    }
    return false;
}

// Called by ImageView::mousePressEvent to bypass Qt's internal itemsAtPosition
// dispatch (which uses a 1×1 rect query and may miss the pivot area).  Sets up
// the pivot drag state and returns true if scenePos hits the pivot marker.
bool LayerItem::beginPivotDragIfHit(const QPointF& scenePos)
{
    if ( m_operationMode != OperationMode::Rotate || !isSelected() )
        return false;
    const QPointF itemPos = mapFromScene(scenePos);
    if ( QLineF(itemPos, pivot()).length() > 12.0 )
        return false;
    m_pivotAtDragStart     = pivotScene();
    m_pivot                = m_pivotAtDragStart;
    m_pivotDragging        = true;
    m_pivotSelected        = true;
    m_pivotWasSet          = m_hasPivot;
    m_mouseOperationActive = true;
    m_startPos             = pos();
    update();
    return true;
}

void LayerItem::updatePivotTo(const QPointF& scenePos)
{
    prepareGeometryChange();
    m_pivot    = scenePos;
    m_hasPivot = true;
    update();
}

void LayerItem::finishPivotDrag()
{
    const QPointF newPivotScene = m_pivot;
    m_pivotDragging = false;
    m_pivotSelected = false;
    update();
    if ( m_undoStack && QLineF(newPivotScene, m_pivotAtDragStart).length() > 0.5 )
        m_undoStack->push(new SetPivotCommand(this, m_pivotAtDragStart, newPivotScene, m_pivotWasSet));
    setSelected(true);
}

bool LayerItem::beginPivotTranslateIfHit(const QPointF& scenePos)
{
    if ( m_operationMode != OperationMode::Rotate || !isSelected() )
        return false;
    const QPointF itemPos = mapFromScene(scenePos);
    if ( QLineF(itemPos, pivot()).length() > 12.0 )
        return false;
    m_startPos               = pos();
    m_pivotAtDragStart       = pivotScene();
    m_pivotWasSet            = m_hasPivot;
    m_lastPivotTransScenePos = scenePos;
    m_pivotTranslating       = true;
    m_pivotHovered           = false;
    m_mouseOperationActive   = true;
    update();
    return true;
}

void LayerItem::updatePivotTranslateTo(const QPointF& scenePos)
{
    const QPointF delta = scenePos - m_lastPivotTransScenePos;
    m_lastPivotTransScenePos = scenePos;
    setPos(pos() + delta);
    if ( m_hasPivot )
        m_pivot += delta;
    update();
}

void LayerItem::finishPivotTranslate()
{
    m_pivotTranslating     = false;
    m_mouseOperationActive = false;
    update();
    if ( !m_undoStack )
        return;
    const QPointF newPos = EditorStyle::instance().allowIntegerMoveOnly()
                           ? QPointF(qRound(pos().x()), qRound(pos().y())) : pos();
    if ( EditorStyle::instance().allowIntegerMoveOnly() )
        setPos(newPos);
    const bool moved = (newPos - m_startPos).manhattanLength() > 0.5;
    if ( !moved )
        return;
    const QString cmdText = QString("Rotate Layer %1 (pivot move)").arg(id());
    if ( m_pivotWasSet ) {
        m_undoStack->beginMacro(cmdText);
        m_undoStack->push(new MoveLayerCommand(this, m_startPos, newPos, id(), OperationMode::Rotate));
        m_undoStack->push(new SetPivotCommand(this, m_pivotAtDragStart, m_pivot, m_pivotWasSet));
        m_undoStack->endMacro();
    } else {
        auto* cmd = new MoveLayerCommand(this, m_startPos, newPos, id(), OperationMode::Rotate);
        cmd->setText(cmdText);
        m_undoStack->push(cmd);
    }
    setSelected(true);
}

void LayerItem::hoverMoveEvent(QGraphicsSceneHoverEvent* event)
{
    if ( m_operationMode == OperationMode::Rotate && isSelected() ) {
        const bool near = QLineF(event->pos(), pivot()).length() <= 12.0;
        if ( near != m_pivotHovered ) {
            m_pivotHovered = near;
            update();
        }
    } else if ( m_pivotHovered ) {
        m_pivotHovered = false;
        update();
    }
    QGraphicsPixmapItem::hoverMoveEvent(event);
}

void LayerItem::hoverLeaveEvent(QGraphicsSceneHoverEvent* event)
{
    if ( m_pivotHovered ) {
        m_pivotHovered = false;
        update();
    }
    QGraphicsPixmapItem::hoverLeaveEvent(event);
}

// per default moveable layer item
// NOTE: QGraphicsItem::ItemIsSelectable expands the size of the QGraphicsPixmapItem by +1+1 !
void LayerItem::init() 
{
  setFlags(QGraphicsItem::ItemIsSelectable |
             QGraphicsItem::ItemIsMovable |
             QGraphicsItem::ItemSendsGeometryChanges |
             QGraphicsItem::ItemIsFocusable);
  setTransformationMode(EditorStyle::instance().transformationMode()); // for on screen visualization
  setAcceptedMouseButtons(Qt::LeftButton);
  setAcceptHoverEvents(true);
  setShapeMode(QGraphicsPixmapItem::BoundingRectShape);
  // setShapeMode(QGraphicsPixmapItem::MaskShape); // the better choice but complex
  
  setZValue(2);
  m_showBoundingBox = true;

  // Lasso pen
  m_lassoPen = QPen(Qt::green);
  m_lassoPen.setWidth(0);
  m_lassoPen.setStyle(Qt::SolidLine);
  
  // Selection pen
  m_selectedPen = QPen(Qt::red);
  m_selectedPen.setWidth(0);
  m_selectedPen.setStyle(Qt::SolidLine);
  
}

// ------------------------ Self info ------------------------
void LayerItem::printself( bool debugSave )
{
  qInfo() << " LayerItem::printself(): name =" << name() << ", id =" << m_index << ", visible =" << isVisible();
  QRectF rect = m_nogui ? m_image.rect() : boundingRect();
  QString geometry = QString("%1x%2+%3+%4").arg(rect.width()).arg(rect.height()).arg(pos().x()).arg(pos().y());
  qInfo() << "  + position =" << pos();
  qInfo() << "  + geometry =" << geometry;
  qInfo() << "  + originalImageType =" << m_originalImageType;
  qInfo() << "  + selected =" << isSelected();
  qInfo() << "  + zValue =" << zValue();
  qInfo() << "  + cage: enabled =" << m_cageEnabled << ", edited =" << m_cageEditing;
  qInfo() << "  + cage overlay =" << (m_cageOverlay != nullptr ? "ok" : "null");
  qInfo() << "  + operation mode =" << m_operationMode;
  qInfo() << "  + bounding box =" << m_showBoundingBox;
  qInfo() << "  + parent ="  << ( m_parent != nullptr ? "null" : "ok" );
}

// ------------------------ Getter / Setter ------------------------
ImageView* LayerItem::getParentImageView() {
  MainWindow* parent = m_parent != nullptr ? dynamic_cast<MainWindow*>(m_parent) : nullptr;
  if ( parent != nullptr ) return parent->getViewer();
  return nullptr;
}

QImage& LayerItem::image( int id ) {
    return id == 1 ? m_originalImage : m_image;
}

QString LayerItem::getAlphaMaskData( bool base64Encoding )
{
  QByteArray ba;
  QBuffer buffer(&ba);
  buffer.open(QIODevice::WriteOnly);
  if ( 1 == 1 ) {
   QImage alphaImage = m_image.convertToFormat(QImage::Format_Alpha8);
   QImage indexedImage = alphaImage.convertToFormat(QImage::Format_Indexed8);
   QList<QRgb> palette;
   for ( int i = 0; i < 256; ++i ) {
     if ( i == 0 ) 
       palette.append(qRgba(0, 0, 0, 255));
     else 
       palette.append(qRgba(255, 255, 255, 255));
   }
   indexedImage.setColorTable(palette);
   for ( int y = 0; y < indexedImage.height(); ++y ) {
     uchar *pixels = indexedImage.scanLine(y);
     for ( int x = 0; x < indexedImage.width(); ++x ) {
       pixels[x] = (pixels[x] > 0) ? 255 : 0;
     }
   }
   indexedImage.save(&buffer, "PNG");
  } else {
   m_image.save(&buffer, "PNG");
  }
  return QString::fromUtf8(ba.toBase64());
}

void LayerItem::setInActive( bool isInActive )
{
  if ( !m_layer ) return;
  m_layer->m_active = !isInActive;
}

void LayerItem::setImage( const QImage &image ) {
    m_image = image;
    updatePixmap();
}

void LayerItem::setLayer( Layer *layer ) {
    m_layer = layer;
}

void LayerItem::setType( LayerType layerType ) {
    m_type = layerType;
    if ( m_type == LayerType::MainImage ) {
      setFlag(QGraphicsItem::ItemIsMovable, false);
      setAcceptedMouseButtons(Qt::NoButton);
    } else {
      setFlag(QGraphicsItem::ItemIsMovable, true);
      setAcceptedMouseButtons(Qt::LeftButton);
      setAcceptedMouseButtons(Qt::RightButton);
    }
}

void LayerItem::setFileInfo( const QString &filePath )
{
  // set filename
  m_filename = filePath;
  // compute checksum
  QFile file(filePath);
  if ( file.open(QIODevice::ReadOnly) ) {
   QCryptographicHash hash(QCryptographicHash::Md5);
   while ( !file.atEnd() ) {
     hash.addData(file.read(8192));
   }
   m_checksum = hash.result().toHex();
  }
}

void LayerItem::setUndoStack( GuardedUndoStack* stack ) {
  m_undoStack = stack; 
}

void LayerItem::setImageRect( const QRectF& rect ) {
  prepareGeometryChange();
}

void LayerItem::setOriginalImage( const QImage& aImage, ImageType imageType) {
  qCDebug(logEditor) << "LayerItem::setOriginalImage(): Processing...";
  {
    if (aImage.format() == QImage::Format_Grayscale8) {
        m_originalImage = aImage;
        m_alphaMask = QImage();
    } else {
        const QImage argb = aImage.convertToFormat(QImage::Format_ARGB32);
        m_alphaMask = extractAlphaMask(argb);
        m_originalImage = argb.convertToFormat(QImage::Format_Grayscale8);
    }
    if (imageType == ImageType::Warped)
        m_totalTransform = QTransform();
    m_originalImageType = imageType;
    m_cageApplied = imageType == ImageType::Warped ? true : false;
  }
}

const QImage& LayerItem::originalImage() {
    return m_originalImage;
}

QString LayerItem::name() const {
  if ( m_index == 0 ) return "MainImage";
  if ( m_layer ) return m_layer->name();
  return m_name;
}

// ------------------------ Update ------------------------
void LayerItem::updatePixmap() {
  if ( m_suppressPixmapUpdate ) return;
  m_displayScale = -1.0; // invalidate mipmap cache
  if ( qobject_cast<QApplication*>(qApp) ) {
    setPixmap(QPixmap::fromImage(m_image));
    if ( m_redOverlay ) {
      const QPixmap& px = pixmap();
      if ( !px.isNull() ) {
        m_redOverlayPixmap = QPixmap(px.size());
        m_redOverlayPixmap.fill(QColor(255, 0, 0, 100));
        QPainter op(&m_redOverlayPixmap);
        op.setCompositionMode(QPainter::CompositionMode_DestinationIn);
        op.drawPixmap(0, 0, px);
      }
    }
  }
}

void LayerItem::setRedOverlay( bool enabled )
{
    m_redOverlay = enabled;
    if ( enabled ) {
        const QPixmap& px = pixmap();
        if ( !px.isNull() ) {
            m_redOverlayPixmap = QPixmap(px.size());
            m_redOverlayPixmap.fill(QColor(255, 0, 0, 100));
            QPainter op(&m_redOverlayPixmap);
            op.setCompositionMode(QPainter::CompositionMode_DestinationIn);
            op.drawPixmap(0, 0, px);
        }
    } else {
        m_redOverlayPixmap = QPixmap();
    }
    update();
}

void LayerItem::resetPixmap() {
  if ( m_suppressPixmapUpdate ) return;
  if ( qobject_cast<QApplication*>(qApp) ) {
    // m_image must be the display image (ARGB32, rotation baked in) so the cage
    // warp physical path receives a source that matches physArgb in size.
    if (m_originalImage.format() == QImage::Format_Grayscale8) {
        if (!m_activeLut.isEmpty()) {
            applyLutWithTransform(m_activeLut);  // rebuilds m_image with rotation
        } else if (!m_totalTransform.isIdentity()) {
            // No LUT but rotation present: build m_image from rotated original
            // so sizesCompatible passes in applyCageWarp's physical path.
            // Must use transformWithHighQuality (same as applyCageWarp's physArgb
            // computation) — transformBicubic uses a different vImage code path and
            // can produce output 1px smaller, causing a size mismatch that shifts the
            // cage warp result visually (the "jump" bug for Rotate+CageWarp without LUT).
            m_image = Interpolation::transformWithHighQuality(
                m_originalImage.convertToFormat(QImage::Format_ARGB32),
                m_totalTransform);
            if (m_cageEnabled && !m_image.isNull())
                m_cageMesh.setImage(m_image);
            updatePixmap();
        } else {
            // No LUT, no rotation: identity case
            m_image = m_originalImage.convertToFormat(QImage::Format_ARGB32);
            if (m_cageEnabled && !m_image.isNull())
                m_cageMesh.setImage(m_image);
            setPixmap(QPixmap::fromImage(m_image));
        }
    } else {
        m_image = m_originalImage;
        setPixmap(QPixmap::fromImage(m_originalImage));
    }
  }
}

void LayerItem::updateImageRegion( const QRect& rect ) {
  if ( rect.isEmpty() )
    return;
  QPixmap pm = pixmap();
  if ( pm.isNull() )
    return;
  QPainter painter(&pm);
   painter.setCompositionMode(QPainter::CompositionMode_Source);
   painter.drawImage(rect.topLeft(),m_image,rect);
  painter.end();
  setPixmap(pm);
  update(rect);
}

void LayerItem::applyActiveLutToRegion( const QRect& rect )
{
    if ( m_activeLut.isEmpty() || m_originalImage.isNull() ) return;
    if ( m_originalImage.format() != QImage::Format_Grayscale8 ) return;
    const QRect r = rect & m_image.rect();
    if ( r.isEmpty() ) return;
    for ( int y = r.top(); y <= r.bottom(); ++y ) {
        const uint8_t* src = m_originalImage.constScanLine(y);
        QRgb*          dst = reinterpret_cast<QRgb*>(m_image.scanLine(y));
        for ( int x = r.left(); x <= r.right(); ++x )
            dst[x] = m_activeLut[src[x]];
    }
    if (!m_alphaMask.isNull()) {
        for ( int y = r.top(); y <= r.bottom(); ++y ) {
            QRgb*          dst = reinterpret_cast<QRgb*>(m_image.scanLine(y));
            const uint8_t* a   = m_alphaMask.constScanLine(y);
            for ( int x = r.left(); x <= r.right(); ++x )
                dst[x] = (dst[x] & 0x00FFFFFF) | ((uint32_t)a[x] << 24);
        }
    }
}

void LayerItem::updateOriginalImage() {
  m_originalImage = (m_image.format() == QImage::Format_Grayscale8)
                    ? m_image
                    : m_image.convertToFormat(QImage::Format_Grayscale8);
}

QImage LayerItem::transformForDisplay(const QImage& image, const QTransform& transform) const
{
    if (transform.isIdentity())
        return image;

    const bool isPureFlip = qFuzzyIsNull(transform.m12())
        && qFuzzyIsNull(transform.m21())
        && qFuzzyIsNull(transform.m13())
        && qFuzzyIsNull(transform.m23())
        && qFuzzyCompare(qAbs(transform.m11()), 1.0)
        && qFuzzyCompare(qAbs(transform.m22()), 1.0);
    if (isPureFlip) {
        Qt::Orientations directions;
        if (transform.m11() < 0) directions |= Qt::Horizontal;
        if (transform.m22() < 0) directions |= Qt::Vertical;
        return QIMAGE_FLIP(image, directions);
    }

    const auto mode = EditorStyle::instance().interpolationMode();
    if (!m_nogui && mode == EditorStyle::InterpolationMode::System)
        return Interpolation::transformWithHighQuality(image, transform);
    if (mode == EditorStyle::InterpolationMode::Nearest)
        return image.transformed(transform, Qt::FastTransformation);
    if (mode == EditorStyle::InterpolationMode::Bicubic || m_nogui)
        return Interpolation::transformBicubic(image, transform);
    return image.transformed(transform, Qt::SmoothTransformation);
}

void LayerItem::applyLutWithTransform( const QVector<QRgb>& lut )
{
    const QImage& original = originalImage();
    if ( original.isNull() || original.format() != QImage::Format_Grayscale8 ) return;

    QImage lutApplied(original.size(), QImage::Format_ARGB32);
    if (lutApplied.isNull())
        return;

    const QRgb* colors = lut.constData();
    const bool masked = !m_alphaMask.isNull() && m_alphaMask.size() == original.size();
    if (masked) {
        for (int y = 0; y < original.height(); ++y) {
            const uchar* gray = original.constScanLine(y);
            const uchar* mask = m_alphaMask.constScanLine(y);
            auto* dest = reinterpret_cast<QRgb*>(lutApplied.scanLine(y));
            for (int x = 0; x < original.width(); ++x) {
                const QRgb color = colors[gray[x]];
                // The mask may reduce the LUT opacity, including for transparent LUT entries.
                const int alpha = qMin(qAlpha(color), int(mask[x]));
                dest[x] = (color & 0x00FFFFFF) | (QRgb(alpha) << 24);
            }
        }
    } else {
        for (int y = 0; y < original.height(); ++y) {
            const uchar* gray = original.constScanLine(y);
            auto* dest = reinterpret_cast<QRgb*>(lutApplied.scanLine(y));
            for (int x = 0; x < original.width(); ++x)
                dest[x] = colors[gray[x]];
        }
    }

    m_image = transformForDisplay(lutApplied, m_totalTransform);
    setActiveLut(lut);
    updatePixmap();
    // Keep the live preview source in sync with the displayed image. Committed
    // warps use the separate pre-warp grayscale and alpha data.
    if (m_cageEnabled && !m_image.isNull()) {
        const bool sizeChanged = m_cageMesh.image().size() != m_image.size();
        m_cageMesh.setImage(m_image);
        if (m_cageWarpRenderer) {
            if (sizeChanged) {
                delete m_cageWarpRenderer;
                m_cageWarpRenderer = nullptr;
            } else {
                // Source content changed (new LUT or post-commit warp) but size is the same.
                // Update the renderer's GPU texture so the next drag samples the current
                // display, not the stale pre-commit source.
                m_cageWarpRenderer->setSourceImage(m_cageMesh.image());
            }
        }
        // Source changed in either case: invalidate the partial-warp base so the next
        // drag does a full warp from the new source rather than compositing with stale
        // non-dirty regions from the previous display state.
        m_partialWarpBase = QImage();
    }
}

// ------------------------ Selected ------------------------
void LayerItem::setIsSelected( int caller, bool isSelected ) 
{
 qCDebug(logEditor) << "LayerItem::setIsSelected(" << caller << "): name =" << name() 
                  << ", select =" << isSelected << ", operationMode =" << m_operationMode;
  {
    MainWindow* parent = m_parent != nullptr ? dynamic_cast<MainWindow*>(m_parent) : nullptr;
    if ( parent != nullptr ) {
      if ( isSelected ) {
        parent->getViewer()->setSelectedLayer(1,this);
        parent->setLayerOperationMode(m_operationMode,false);
        parent->updateLayerOperationParameter("LayerItem::setIsSelected",m_name,LayerItem::OperationMode::Rotate,m_currentRotation);
      }
    }
    if ( !isSelected ) setCageVisible(1,false);
    QGraphicsItem::setSelected(isSelected);
  }
}

// ------------------------ Mirror ------------------------
void LayerItem::mirror( int plane ) 
{
  qCDebug(logEditor) << "LayerItem::mirror(): name =" << name();
  { 
    if ( m_undoStack != nullptr ) {
      m_undoStack->push(new MirrorLayerCommand(this, m_index, plane));
    } else {
      qWarning() << "CRITICAL - LayerItem::mirror(): m_undoStack is null";
    }
  }
}

void LayerItem::setMirror( int mirrorPlane ) {
  qCDebug(logEditor) << "LayerItem::setMirror(): mirrorPlane =" << mirrorPlane;
  {
    if ( mirrorPlane > 0 ) {
      QTransform transform;
      transform.scale(mirrorPlane == 1 ? +1 : -1,mirrorPlane == 1 ? -1 : +1);
      setImageTransform(transform);
    }
  }
}

// ------------------------ Transform ------------------------
void LayerItem::shift( const QPointF &aShift ) {
  setPos(pos()+aShift);
}
void LayerItem::shiftTo( const QPointF &aPosition ) {
  setPos(aPosition);
}

void LayerItem::scale( double xscale, double yscale ) 
{
  qCDebug(logEditor) << "LayerItem::scale(): name =" << name() << ", scaleX =" << xscale << ", scaleY =" << yscale;
  {
    MainWindow* parent = m_parent != nullptr ? dynamic_cast<MainWindow*>(m_parent) : nullptr;
    if ( parent != nullptr && m_undoStack != nullptr ) {
      if ( m_undoStack->count() > 0 ) {
        QString scaleLayerName = QString("Scale Layer %1").arg(id());
        const QUndoCommand* newestCmd = m_undoStack->command(m_undoStack->count() - 1);
        if ( newestCmd != nullptr ) {
          if ( scaleLayerName == newestCmd->text() ) {
            const TransformLayerCommand *layerCmd = static_cast<const TransformLayerCommand*>(newestCmd);
            if ( layerCmd != nullptr ) {
              QTransform layerTransform = layerCmd->transform();
              m_undoStack->push(new TransformLayerCommand(this, transform(), layerTransform.inverted()));
              parent->getViewer()->setCageVisible(this,LayerItem::OperationMode::Scale,true);
            } else {
              qWarning() << "LayerItem::scale(): static_cast failure for last undo command.";
            }
          } else {
            qWarning() << "LayerItem::scale(): Name mismatch: " << scaleLayerName << " not " << newestCmd->text();
          }
        }
      }
      parent->updateLayerOperationParameter("LayerItem::scale",m_name,LayerItem::OperationMode::Scale,xscale,yscale);
    }
  }
}

void LayerItem::resetTotalTransform()
{
  qCDebug(logEditor) << "LayerItem::resetTotalTransform(): Processing...";
  {
    m_totalTransform = QTransform();
    setImageTransform(m_totalTransform);
  }
}

void LayerItem::resetImageState( const QImage& image, const QPointF& position, const QTransform& transform )
{
  qCDebug(logEditor) << "LayerItem::resetImageState(): name =" << name() << ", position =" << position;
  {
    prepareGeometryChange();
    setTransform(QTransform());
    const QPointF p = EditorStyle::instance().allowIntegerMoveOnly()
        ? QPointF(qRound(position.x()), qRound(position.y())) : position;
    setPos(p);
	    m_image = image;
	    m_totalTransform = transform;
	    updatePixmap();
	  }
	}

void LayerItem::setImageTransform( const QTransform& transform, bool combine ) {
  qCDebug(logEditor) << "LayerItem::setImageTransform(): nonGUI =" << m_nogui << ", position =" << pos() << ", combine =" 
                        << combine << ", originalImageType =" << m_originalImageType;
  {
    // *** DO NOT USE INTERNAL TRANSFORMATIONS ***
    if ( 1 == 2 && !m_nogui && qobject_cast<QApplication*>(qApp) ) {
      qDebug() << " + internal pixmap processing: transform_origin =" << QGraphicsPixmapItem::transformOriginPoint() << ", pos =" << QGraphicsPixmapItem::pos();
      m_totalTransform = transform;
      m_image = m_originalImage.transformed(m_totalTransform, Qt::SmoothTransformation);
      prepareGeometryChange();
      QPointF imageCenter = QRectF(m_originalImage.rect()).center();
      QPointF sceneCenter = mapToScene(imageCenter);
      QPointF newImageCenter(m_image.width() / 2.0, m_image.height() / 2.0);
      setPixmap(QPixmap::fromImage(m_image));
      setPos(sceneCenter - newImageCenter);
      // QGraphicsPixmapItem::setTransform(transform,combine);
      return;
    }
    // *** hard QImage transformation ***
    qCDebug(logEditor) << "LayerItem::setImageTransform(): ALTERNATIVE PROCESS FOR CASE WHERE NO PIXMAP IS AVAILABLE";
    // check whether original image is-up-to-date
    if ( m_cageApplied && m_originalImageType == ImageType::Original ) {
     // qDebug() << "LayerItem::setImageTransform(): Need image update";
     m_originalImageType = ImageType::Warped;
     const QImage argb32 = (m_image.format() == QImage::Format_ARGB32)
                           ? m_image : m_image.convertToFormat(QImage::Format_ARGB32);
     m_alphaMask = extractAlphaMask(argb32);
     m_originalImage = argb32.convertToFormat(QImage::Format_Grayscale8);
    }
    // transform image
    prepareGeometryChange();
    QPointF sceneCenter = qobject_cast<QApplication*>(qApp) ? mapToScene(QRectF(pixmap().rect()).center()) : mapToScene(QRectF(m_originalImage.rect()).center());
    QPointF imageCenter = QRectF(m_originalImage.rect()).center();
    if ( combine ) {
      m_totalTransform.translate(imageCenter.x(), imageCenter.y());
      m_totalTransform *= transform;
      m_totalTransform.translate(-imageCenter.x(), -imageCenter.y());
    } else {
      m_totalTransform = QTransform();
      m_totalTransform.translate(imageCenter.x(), imageCenter.y());
      m_totalTransform *= transform;
      m_totalTransform.translate(-imageCenter.x(), -imageCenter.y());
    }
    // Fast path for pure pixel flip (horizontal/vertical mirror).
    // When m_totalTransform is a pure axis-aligned flip (|m11|=1, |m22|=1, no shear or
    // projection), QImage::mirrored() gives the same result as transformWithHighQuality
    // but is orders of magnitude faster — no QPainter, no resampling, just a memcpy.
    const bool isPureFlip = (m_originalImage.format() == QImage::Format_ARGB32
                              || m_originalImage.format() == QImage::Format_Grayscale8)
        && qFuzzyIsNull(m_totalTransform.m12())
        && qFuzzyIsNull(m_totalTransform.m21())
        && qFuzzyIsNull(m_totalTransform.m13())
        && qFuzzyIsNull(m_totalTransform.m23())
        && qFuzzyCompare(qAbs(m_totalTransform.m11()), 1.0)
        && qFuzzyCompare(qAbs(m_totalTransform.m22()), 1.0);
    // the interpolation step (Qt only supports nearest neighbor und linear interpolation)
    if ( isPureFlip ) {
      Qt::Orientations dirs;
      if ( m_totalTransform.m11() < 0 ) dirs |= Qt::Horizontal;
      if ( m_totalTransform.m22() < 0 ) dirs |= Qt::Vertical;
      m_image = QIMAGE_FLIP(m_originalImage, dirs);
    } else if ( !m_nogui && EditorStyle::instance().interpolationMode() == EditorStyle::InterpolationMode::System ) {
      // !!! only in gui mode !!!
      m_image = Interpolation::transformWithHighQuality(m_originalImage, m_totalTransform);
    } else if ( EditorStyle::instance().interpolationMode() == EditorStyle::InterpolationMode::Bicubic ) {
      // this use external bicubic interpolation
      m_image = Interpolation::transformBicubic(m_originalImage, m_totalTransform);
    } else if ( EditorStyle::instance().interpolationMode() == EditorStyle::InterpolationMode::Nearest ) {
      m_image = m_originalImage.transformed(m_totalTransform,Qt::FastTransformation);
    } else if ( m_nogui ) {
      m_image = Interpolation::transformBicubic(m_originalImage, m_totalTransform);
    } else {
      m_image = m_originalImage.transformed(m_totalTransform,Qt::SmoothTransformation);
    }
    // end
    QPointF newImageCenter(m_image.width() / 2.0, m_image.height() / 2.0);
    setPos(sceneCenter - newImageCenter);
    // reset transform
    setTransform(QTransform());
    // >>>
    updatePixmap();
  }
}

// Re-applies the stored m_totalTransform to an updated m_originalImage without
// modifying the transform itself.  For an identity transform the image is used
// as-is.  In either case the item's scene position is adjusted so its visual
// centre stays at the same scene point (same centre-alignment as setImageTransform).
void LayerItem::reapplyImageTransform()
{
  prepareGeometryChange();
  QPointF sceneCenter = qobject_cast<QApplication*>(qApp)
      ? mapToScene(QRectF(pixmap().rect()).center())
      : mapToScene(QRectF(m_originalImage.rect()).center());
  if ( m_totalTransform.isIdentity() ) {
    m_image = m_originalImage;
  } else if ( !m_nogui && EditorStyle::instance().interpolationMode() == EditorStyle::InterpolationMode::System ) {
    m_image = Interpolation::transformWithHighQuality(m_originalImage, m_totalTransform);
  } else if ( EditorStyle::instance().interpolationMode() == EditorStyle::InterpolationMode::Bicubic ) {
    m_image = Interpolation::transformBicubic(m_originalImage, m_totalTransform);
  } else if ( EditorStyle::instance().interpolationMode() == EditorStyle::InterpolationMode::Nearest ) {
    m_image = m_originalImage.transformed(m_totalTransform, Qt::FastTransformation);
  } else if ( m_nogui ) {
    m_image = Interpolation::transformBicubic(m_originalImage, m_totalTransform);
  } else {
    m_image = m_originalImage.transformed(m_totalTransform, Qt::SmoothTransformation);
  }
  QPointF newImageCenter(m_image.width() / 2.0, m_image.height() / 2.0);
  setPos(sceneCenter - newImageCenter);
  setTransform(QTransform());
  updatePixmap();
}

void LayerItem::restoreTransformState( const QTransform& totalTransform, const QPointF& restoredPos )
{
  prepareGeometryChange();
  m_totalTransform = totalTransform;
  if ( m_totalTransform.isIdentity() ) {
    m_image = m_originalImage;
  } else if ( !m_nogui && EditorStyle::instance().interpolationMode() == EditorStyle::InterpolationMode::System ) {
    m_image = Interpolation::transformWithHighQuality(m_originalImage, m_totalTransform);
  } else if ( EditorStyle::instance().interpolationMode() == EditorStyle::InterpolationMode::Bicubic ) {
    m_image = Interpolation::transformBicubic(m_originalImage, m_totalTransform);
  } else if ( EditorStyle::instance().interpolationMode() == EditorStyle::InterpolationMode::Nearest ) {
    m_image = m_originalImage.transformed(m_totalTransform, Qt::FastTransformation);
  } else if ( m_nogui ) {
    m_image = Interpolation::transformBicubic(m_originalImage, m_totalTransform);
  } else {
    m_image = m_originalImage.transformed(m_totalTransform, Qt::SmoothTransformation);
  }
  setPos(restoredPos);
  setTransform(QTransform());
  updatePixmap();
}

void LayerItem::restoreOriginalImageState( const QImage& origImg, const QImage& alphaMask,
                                            ImageType imageType, bool cageApplied )
{
  m_originalImage    = origImg;
  m_alphaMask        = alphaMask;
  m_originalImageType = imageType;
  m_cageApplied      = cageApplied;
}

// ------------------------ Paint ------------------------
void LayerItem::paint( QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget )
{
  {
    const QTransform world = painter->worldTransform();
    // Use the length of the transformed unit vector so rotation alone keeps the cached mipmap.
    const qreal viewScale = std::hypot(world.m11(), world.m12());
    constexpr qreal kMipmapThreshold = 0.5;
    if ( viewScale < kMipmapThreshold && !m_image.isNull() ) {
      // Progressive halving (area-averaging) for extreme zoom-out.
      // Regenerate only when scale changes by more than 10%.
      if ( m_displayScale < 0.0 || qAbs(viewScale - m_displayScale) > m_displayScale * 0.1 ) {
        const qreal targetScale = qMax(viewScale * 2.0, 0.02);
        const QSize targetSz( qMax(1, qRound(m_image.width()  * targetScale)),
                               qMax(1, qRound(m_image.height() * targetScale)) );
        QImage img = m_image;
        while ( img.width() / 2 >= targetSz.width() && img.width() > 1 )
          img = img.scaled(img.size() / 2, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        if ( img.size() != targetSz )
          img = img.scaled(targetSz, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        m_displayPixmap = QPixmap::fromImage(img);
        m_displayScale  = viewScale;
      }
      painter->save();
      painter->setRenderHint(QPainter::SmoothPixmapTransform, true);
      painter->drawPixmap(QRectF(offset(), QSizeF(m_image.size())),
                          m_displayPixmap, QRectF(m_displayPixmap.rect()));
      painter->restore();
    } else {
      if ( m_displayScale >= 0.0 ) { m_displayPixmap = QPixmap(); m_displayScale = -1.0; }
      QGraphicsPixmapItem::paint(painter,option,widget);
    }
    if ( m_redOverlay && !m_redOverlayPixmap.isNull() ) {
      painter->drawPixmap(QPointF(0, 0), m_redOverlayPixmap);
    }
    if ( m_operationMode == OperationMode::Perspective ) {
      return;
    }
    if ( isSelected() || m_isMultiSelected ) {
      QPen selPen(m_selectedPen);
      selPen.setWidth(EditorStyle::instance().layerBoundingBoxWidth());
      painter->setPen(selPen);
      painter->setBrush(Qt::NoBrush);
      painter->drawRect(boundingRect());
    } else if ( m_showBoundingBox ) {
      QPen bbPen(m_lassoPen);
      bbPen.setWidth(EditorStyle::instance().layerBoundingBoxWidth());
      painter->setPen(bbPen);
      painter->setBrush(Qt::NoBrush);
      painter->drawRect(boundingRect());
    }
    // rotation centre marker — drawn in viewport space so pos() rounding
    // in setImageTransform() does not cause the cross to drift visually.
    if ( m_operationMode == OperationMode::Rotate && ( isSelected() || m_isMultiSelected ) ) {
      const QPointF pivSc = m_hasPivot ? m_pivot : mapToScene(boundingRect().center());
      const qreal   viewScale = painter->worldTransform().m11();
      const qreal   arm = 8.0 * qMax(1.0, viewScale);
      const QPointF cVp = painter->worldTransform().map(mapFromScene(pivSc));
      painter->save();
      painter->resetTransform();
      QPen crossPen(m_pivotSelected || m_pivotHovered || m_pivotTranslating ? Qt::yellow : Qt::red, 1.5);
      painter->setPen(crossPen);
      painter->setBrush(Qt::NoBrush);
      painter->drawLine(cVp + QPointF(-arm, 0), cVp + QPointF(arm, 0));
      painter->drawLine(cVp + QPointF(0, -arm), cVp + QPointF(0, arm));
      painter->drawEllipse(cVp, 4.0 * qMax(1.0, viewScale), 4.0 * qMax(1.0, viewScale));
      painter->restore();
    }
    // render cage
    if ( m_cageEnabled ) {
     painter->setPen(QPen(Qt::red,0));
     painter->setBrush(Qt::NoBrush);
     painter->drawPolygon(m_cage.data(), m_cage.size());
    }
  }
}

void LayerItem::paintStrokeSegment( const QPoint& p0, const QPoint& p1, const QColor &color, int radius, float hardness )
{
  qCDebug(logEditor) << "LayerItem::paintStrokeSegment(): Processing...";
  if ( radius < 0.0 ) return;
  {
    const float spacing = std::max(1.0f, radius * 0.35f);
    const float dx = p1.x() - p0.x();
    const float dy = p1.y() - p0.y();
    const float dist = std::sqrt(dx*dx + dy*dy);
    if ( dist <= 0.0f ) {
     Interpolation::dab(m_image, p0, color, radius, hardness);
     return;
    }
    const int steps = std::ceil(dist / spacing);
    for ( int i = 0; i <= steps; ++i ) {
        float t = float(i) / float(steps);
        QPoint p(
            int(p0.x() + t * dx),
            int(p0.y() + t * dy)
        );
        Interpolation::dab(m_image, p, color, radius, hardness);
    }
    // ---
    QRect dirtyRect = QRect(p0, QSize(1,1));
    dirtyRect |= QRect(p1, QSize(1,1));
    int pad = radius + 2;
    dirtyRect.adjust(-pad, -pad, pad, pad);
    dirtyRect &= m_image.rect();
    updateImageRegion(dirtyRect);
    // ---
    // setPixmap(QPixmap::fromImage(m_image));
  }
}

// ------------------------ Cage ------------------------

QImage LayerItem::cageWarpSource() const
{
    // The cage points are cumulative and always map from the pre-warp image.
    const bool hasInitialImage = !m_cageInitialImage.isNull();
    const QImage& gray = hasInitialImage ? m_cageInitialImage : m_originalImage;
    const QImage& alpha = hasInitialImage ? m_cageInitialAlphaMask : m_alphaMask;
    const QTransform& transform = hasInitialImage ? m_cageInitialTransform : m_totalTransform;
    QImage source = gray.convertToFormat(QImage::Format_ARGB32);
    if (source.isNull())
        return {};

    if (!alpha.isNull() && alpha.size() == source.size()) {
        for (int y = 0; y < source.height(); ++y) {
            auto* pixels = reinterpret_cast<QRgb*>(source.scanLine(y));
            const uchar* mask = alpha.constScanLine(y);
            for (int x = 0; x < source.width(); ++x)
                pixels[x] = (pixels[x] & 0x00FFFFFF) | (QRgb(mask[x]) << 24);
        }
    }

    source = transformForDisplay(source, transform);

    // Match the raster into which the initial grid was placed. Later warps may
    // expand the displayed image, while the source coordinates stay fixed.
    const QSize frameSize = hasInitialImage ? m_cageInitialFrameSize : m_image.size();
    if (!source.isNull() && !frameSize.isEmpty() && source.size() != frameSize) {
        QImage framed(frameSize, QImage::Format_ARGB32);
        if (framed.isNull())
            return {};
        framed.fill(Qt::transparent);
        QPainter painter(&framed);
        painter.drawImage(0, 0, source);
        painter.end();
        source = framed;
    }
    return source.convertToFormat(QImage::Format_ARGB32);
}

QImage LayerItem::commitCageWarp(const QString& caller, const QImage& cachedPhysical)
{
    QImage warped;
    if (!m_nogui && !cachedPhysical.isNull()) {
        warped = cachedPhysical;
        m_cageMesh.setActiveCagePointId(-1);
        m_cageMesh.setOffset(0, 0);
        setOffset(QPointF());
    } else {
        warped = applyCageWarp(caller, true);
    }
    if (warped.isNull())
        return {};

    setOriginalImage(warped, ImageType::Warped);
    updateCageDisplay();
    return warped;
}

void LayerItem::updateCageDisplay()
{
    QVector<QRgb> lut = m_activeLut;
    if (lut.isEmpty()) {
        lut.resize(256);
        for (int i = 0; i < lut.size(); ++i)
            lut[i] = qRgb(i, i, i);
    }
    applyLutWithTransform(lut);
}

QImage LayerItem::applyCageWarp(const QString& caller, bool physical)
{
    qCDebug(logEditor) << "Applying cage warp" << caller << name() << "physical:" << physical;
    const bool preview = caller == "setCagePoint";
    const QRectF bounds = QPolygonF(m_cageMesh.points()).boundingRect();
    const QSize outputSize = CagePerspectiveWarp::outputSize(bounds);
    if (outputSize.isEmpty())
        return {};
    if (!preview) {
        m_cageMesh.setActiveCagePointId(-1);
        m_cageMesh.setOffset(0, 0);
    }
    const QImage source = physical ? cageWarpSource() : QImage();
    if (physical && source.isNull())
        return {};

    if ( !m_cageMesh.isInitialized(true) ) {
      m_cageMesh.setImage(m_image);
    }
    if (!Config::noOpenGL && (Config::gpuCageWarpProcessing || EditorStyle::instance().useGPU())) {
      qCDebug(logEditor) << "LayerItem::applyCageWarp(): GPU cage warp processing...";
      const bool needsSource = !m_cageWarpRenderer || physical;
      if (!m_cageWarpRenderer)
          m_cageWarpRenderer = new CageWarpRenderer();
      const QImage& gpuSource = physical ? source : m_cageMesh.image();
      const bool sourceReady = !needsSource || m_cageWarpRenderer->setSourceImage(gpuSource);
      m_cageWarpRenderer->setFixedOutputRect(bounds);
      const bool gridReady = sourceReady && m_cageWarpRenderer->setGridSize(m_cageMesh.cols(), m_cageMesh.rows());
      CageWarpRenderer::WarpOptions options;
      const bool catmullRomMode = EditorStyle::instance().gpuCatmullRom();
      options.cageInterpolation = catmullRomMode
          ? CageWarpRenderer::CageInterpolation::CatmullRom
          : CageWarpRenderer::CageInterpolation::Bilinear;
      options.inverseMapping = CageWarpRenderer::InverseMapping::Newton;
      options.inverseIterations = 10;

      // Build dirty-quad list for partial rendering.
      const int ptIdxForPartial = m_cageMesh.activeCagePointId();
      QVector<int> dirtyQuadIndices;
      if ( ptIdxForPartial >= 0 ) {
        const int dcols = m_cageMesh.cols();
        const int drows = m_cageMesh.rows();
        const int drow  = ptIdxForPartial / dcols;
        const int dcol  = ptIdxForPartial % dcols;
        if ( catmullRomMode ) {
          for (int r = qMax(0, drow - 2); r <= qMin(drows - 2, drow + 1); ++r)
            for (int c = qMax(0, dcol - 2); c <= qMin(dcols - 2, dcol + 1); ++c)
              dirtyQuadIndices.append(r * dcols + c);
        } else {
          if (drow < drows-1 && dcol < dcols-1) dirtyQuadIndices.append( drow    * dcols + dcol    );
          if (drow < drows-1 && dcol > 0)       dirtyQuadIndices.append( drow    * dcols + dcol - 1);
          if (drow > 0       && dcol > 0)       dirtyQuadIndices.append((drow-1) * dcols + dcol - 1);
          if (drow > 0       && dcol < dcols-1) dirtyQuadIndices.append((drow-1) * dcols + dcol    );
        }
      }

      QImage warped;
      if (gridReady) {
          if (!dirtyQuadIndices.isEmpty() && m_partialWarpBase.size() == bounds.toAlignedRect().size()) {
              warped = m_cageWarpRenderer->warpPartial(m_cageMesh.points(), dirtyQuadIndices,
                  m_partialWarpBase, m_partialWarpPrevPoints, nullptr, options);
          }
          if (warped.isNull())
              warped = m_cageWarpRenderer->warp(m_cageMesh.points(), nullptr, options);
      }

      if (physical) {
          delete m_cageWarpRenderer;
          m_cageWarpRenderer = nullptr;
          m_partialWarpBase = QImage();
          if (!warped.isNull()) {
              if (warped.format() == QImage::Format_ARGB32_Premultiplied)
                  warped.reinterpretAsFormat(QImage::Format_ARGB32);
              else if (warped.format() == QImage::Format_RGBA8888_Premultiplied)
                  warped.reinterpretAsFormat(QImage::Format_RGBA8888);
              setOffset(QPointF());
              return warped;
          }
      } else if (!warped.isNull()) {
        // The GPU inverse-field warp processes the entire output bounding box, which can
        // produce opaque output in areas that are transparent in the source (alpha=0).
        // Apply a CPU alpha mask using the cage mesh forward mapping — same approach as
        // TriangleWarp — to restore correct transparency.
        if ( !warped.isNull() ) {
          warped = warped.convertToFormat(QImage::Format_ARGB32);
          const QImage srcA = m_cageMesh.image().convertToFormat(QImage::Format_ARGB32);
          const QVector<QPointF>& pts = m_cageMesh.points();
          const int gcols = m_cageMesh.cols();
          const int grows = m_cageMesh.rows();
          if ( !srcA.isNull() && pts.size() == gcols * grows && gcols >= 2 && grows >= 2 ) {
            // Compute output origin = bounding box min of all warped cage points
            double minX = pts[0].x(), minY = pts[0].y();
            for (const QPointF& p : pts) { minX = qMin(minX, p.x()); minY = qMin(minY, p.y()); }
            const QPointF origin(std::floor(minX), std::floor(minY));
            const double srcW = srcA.width(), srcH = srcA.height();
            // Build alpha mask by scanning each cage cell in output (target) space
            QVector<bool> alphaMask(warped.width() * warped.height(), false);
            for ( int cy = 0; cy + 1 < grows; ++cy ) {
              for ( int cx = 0; cx + 1 < gcols; ++cx ) {
                const QVector<QPointF> dstQuad = {
                  pts[ cy      * gcols + cx    ] - origin,
                  pts[ cy      * gcols + cx + 1] - origin,
                  pts[(cy + 1) * gcols + cx + 1] - origin,
                  pts[(cy + 1) * gcols + cx    ] - origin
                };
                const QVector<QPointF> srcQuad = {
                  QPointF( cx      * srcW / (gcols - 1),  cy      * srcH / (grows - 1)),
                  QPointF((cx + 1) * srcW / (gcols - 1),  cy      * srcH / (grows - 1)),
                  QPointF((cx + 1) * srcW / (gcols - 1), (cy + 1) * srcH / (grows - 1)),
                  QPointF( cx      * srcW / (gcols - 1), (cy + 1) * srcH / (grows - 1))
                };
                const QRectF bbox = QPolygonF(dstQuad).boundingRect();
                const int yMin = qMax(0,                 int(bbox.top()));
                const int yMax = qMin(warped.height()-1, int(bbox.bottom()) + 1);
                const int xMin = qMax(0,                 int(bbox.left()));
                const int xMax = qMin(warped.width()-1,  int(bbox.right())  + 1);
                for ( int py = yMin; py <= yMax; ++py ) {
                  QPointF start(-1, -1);
                  for (int px = xMin; px <= xMax; ++px) {
                    const QPointF p(px, py);
                    if (!GeometryUtils::pointInQuad(p, dstQuad)) continue;
                    int cnt = 0;
                    const QPointF srcP = GeometryUtils::barycentric(p, dstQuad, srcQuad, &cnt, start);
                    const int sx = qBound(0, int(srcP.x()), srcA.width()  - 1);
                    const int sy = qBound(0, int(srcP.y()), srcA.height() - 1);
                    if ( qAlpha(srcA.pixel(sx, sy)) > 0 )
                      alphaMask[py * warped.width() + px] = true;
                  }
                }
              }
            }
          
            // Zero out pixels not covered by the alpha mask
            for ( int py = 0; py < warped.height(); ++py ) {
              int pyww = py * warped.width();
              QRgb* row = reinterpret_cast<QRgb*>(warped.scanLine(py));
              for ( int px = 0; px < warped.width(); ++px ) {
                if ( !alphaMask[pyww + px] )
                  row[px] = qRgba(0, 0, 0, 0);
              }
            }
          }
        }
        // Save the preview for the next partial update.
        m_partialWarpBase       = warped;
        m_partialWarpPrevPoints = m_cageMesh.points();

        m_cageMesh.setActiveCagePointId(-1);

        setOffset(QPointF(0, 0));   // reset pixmap offset accumulated during drag normalization
        if ( !m_suppressPixmapUpdate ) {
          setPixmap(QPixmap::fromImage(warped));
          if (m_redOverlay) setRedOverlay(true);  // rebuild overlay from new pixmap
        }
        m_image = warped;
        QGraphicsPixmapItem::setPos(QGraphicsPixmapItem::pos() + m_cageMesh.getOffset());
        m_cageMesh.setOffset(0, 0);   // consume accumulated drag offset; mirrors CPU path cleanup
        m_cageApplied = true;
        return m_image;
      }
    }

    if (!m_cageMesh.needUpdate() && !physical)
        return {};

    QImage rawSource = physical ? source : (preview ? QImage() : cageWarpSource());
    QImage warped;
    if (!rawSource.isNull()) {
        if (EditorStyle::instance().quadPerspectiveWarp())
            warped = CagePerspectiveWarp::warpByCage(rawSource, m_cageMesh);
        if (warped.isNull())
            warped = TriangleWarp::warp(rawSource, rawSource, m_cageMesh).image;
    }
    if (physical && warped.isNull())
        return {};

    if (warped.isNull()) {
        warped = TriangleWarp::warp(m_image, m_cageMesh.image(), m_cageMesh).image;
    } else if (!physical && !m_activeLut.isEmpty()) {
        // Render a transient preview from the uncolored result without baking it.
        warped = warped.convertToFormat(QImage::Format_ARGB32);
        for (int y = 0; y < warped.height(); ++y) {
            auto* row = reinterpret_cast<QRgb*>(warped.scanLine(y));
            for (int x = 0; x < warped.width(); ++x) {
                const QRgb color = m_activeLut[qGray(row[x])];
                const int alpha = qMin(qAlpha(color), qAlpha(row[x]));
                row[x] = (color & 0x00FFFFFF) | (QRgb(alpha) << 24);
            }
        }
    }

    m_cageMesh.setActiveCagePointId(-1);
    m_cageMesh.setOffset(0, 0);
    if (warped.isNull())
        return {};

    // The legacy renderer includes an extra pixel on the far edge.
    if (warped.size() != outputSize)
        warped = warped.copy(QRect(QPoint(), outputSize));

    // The mesh is already rebased to its new origin. The old pixmap offset
    // only kept the preview in place during dragging.
    setOffset(QPointF());
    if (!physical) {
        m_image = warped;
        m_cageApplied = true;
        updatePixmap();
    }
    return warped;
}

void LayerItem::captureCageSource()
{
    if (!m_cageInitialImage.isNull())
        return;

    m_cageInitialImage = m_originalImage;
    m_cageInitialTransform = m_totalTransform;
    m_cageInitialAlphaMask = m_alphaMask;
    m_cageInitialFrameSize = m_image.size();
    m_cageInitialPts = m_cageMesh.points();
    m_cageInitialOrigPts = m_cageMesh.originalPoints();
    m_cageInitialCols = m_cageMesh.cols();
    m_cageInitialRows = m_cageMesh.rows();
    m_cageInitialPos = pos();
}

void LayerItem::enableCage( int cols, int rows )
{
  qCDebug(logEditor) << "LayerItem::enableCage(): cols =" << cols << ", rows =" << rows 
                  << ", initialized =" << m_cageMesh.isInitialized() << ", enabled =" << m_cageEnabled;
  {
    int ncols = cols == -1 ? m_cageMesh.cols() : cols;
    int nrows = rows == -1 ? m_cageMesh.rows() : rows;
    if ( m_cageMesh.isInitialized() && m_cageMesh.cols() == ncols && m_cageMesh.rows() == nrows ) return;
    const bool firstInit = !m_cageMesh.isInitialized();
    m_cageMesh.create(QRectF(offset(), QSizeF(m_image.size())), ncols, nrows);
    m_cageMesh.setImage(m_image);
    // Grid changed → partial warp state from old grid is incompatible; discard it.
    m_partialWarpBase = QImage();
    m_partialWarpPrevPoints.clear();
    if (firstInit) {
        captureCageSource();
        m_partialWarpBase = m_image;
        m_partialWarpPrevPoints = m_cageMesh.originalPoints();
    }
    m_cageEnabled = true;     
    if ( !scene() )
      return;
    if ( !m_cageOverlay ) {
      m_cageOverlay = new CageOverlayItem(this);
      m_cageOverlay->setParentItem(this);
    }
    m_cageOverlay->setVisible(true);
    // Handles
    qDeleteAll(m_handles);
    m_handles.clear();
    for ( int i = 0; i < m_cageMesh.pointCount(); ++i ) {
      auto* h = new CageControlPointItem(this, i);
      h->setParentItem(this);
      h->setPos(m_cageMesh.point(i));
      int col = i % m_cageMesh.cols();
      int row = i / m_cageMesh.cols();
      int dx = ( col == 0 ) ? 0 : ( ( col == m_cageMesh.cols()-1 ) ? -2*m_cageMesh.getRadius() : -m_cageMesh.getRadius() );
      int dy = ( row == 0 ) ? 0 : ( ( row == m_cageMesh.rows()-1 ) ? -2*m_cageMesh.getRadius() : -m_cageMesh.getRadius() );
      h->setRect(dx, dy, 2*m_cageMesh.getRadius(), 2*m_cageMesh.getRadius() );
      m_handles << h;
    }
    m_cageOverlay->update();
  }
}

// --- what about the grid ??? ---

void LayerItem::initCage( const QVector<QPointF>& pts, const QRectF &rect, int nrows, int ncolumns )
{
  qCDebug(logEditor) << "LayerItem::initCage(): cageOverlay =" << (m_cageOverlay!=nullptr?"ok":"null") 
                          << ", rect =" << rect << ", rows =" << nrows << ", ncolumns =" << ncolumns;
  {
    m_cageMesh.setIsInitialized();
    m_cageMesh.create(rect, ncolumns, nrows);
    captureCageSource();
    m_cageMesh.setPoints(pts);
    if ( m_cageOverlay == nullptr ) {
      m_cageOverlay = new CageOverlayItem(this);
      m_cageOverlay->setParentItem(this);
    }
  }
}

void LayerItem::setCageVisible( int caller, bool isVisible )
{
  qCDebug(logEditor) << "LayerItem::setCageVisible(): caller =" << caller << ", name =" << name() << ", visible =" << isVisible 
        << ", cageOverlay =" << (m_cageOverlay != nullptr?"ok":"none") << ", cageEditing =" << m_cageEditing << "|" << m_cageEnabled 
        << ", nControlPoints =" << m_cageMesh.pointCount();
  {
   ImageView *viewer = getParentImageView();
   if ( m_cageOverlay != nullptr && viewer != nullptr ) {
     viewer->enableCageLayer(this,isVisible);
     if ( isVisible  ) {
       m_cageMesh.setActive(true);
       m_cageEnabled = true;
       m_cageEditing = true;
       for ( int i = 0; i < m_handles.size(); ++i ) {
         m_handles[i]->setVisible(true);
       }
       m_cageOverlay->setVisible(true);
     } else {
#if 0
       // CLAUDE says: This is potentially wrong. This will reset
       // m_selectedCageLayer to null in MainWindow, which is not
       // necessarily what we want to do when making other inactive
       // cages invisible. Let the parent MainWindow decide about this.

       if ( m_parent != nullptr ) { 
        MainWindow* parent = dynamic_cast<MainWindow*>(m_parent);
        if ( parent && parent->getViewer() != nullptr ) {
         parent->getViewer()->setActiveCageLayer(nullptr);
        }
       }    
#endif
       m_cageMesh.setActive(false);
       m_cageEnabled = false;
       m_cageEditing = false;
       for ( int i = 0; i < m_handles.size(); ++i ) {
         m_handles[i]->setVisible(false);
       }
       m_cageOverlay->setVisible(false);
    }
    update();
   } else {
    // qCritical() << "CRITICAL: LayerItem::setCageVisible(): No Cage Overlay!";
   }
  }
}

void LayerItem::setCageVisible( LayerItem::OperationMode mode, bool isVisible, bool pushBackImage )
{
  qCDebug(logEditor) << "LayerItem::setCageVisible(): name =" << name() << ", mode =" << mode << ", isVisible =" 
             << isVisible << ", pushBackImage =" << pushBackImage << ", cageOverlay =" << (m_cageOverlay != nullptr?"ok":"none");
  { 
    switch ( mode ) {
      case LayerItem::OperationMode::CageWarp:
        if ( m_cageOverlay != nullptr ) {
          if( m_handles.size() == 0 ) {
            // create handles if they have not been created (when read from .json)
            // CLAUDE -- this should be moved to a more convenient place.
            qDeleteAll(m_handles);
            m_handles.clear();
            for ( int i = 0; i < m_cageMesh.pointCount(); ++i ) {
              auto* h = new CageControlPointItem(this, i);
              h->setParentItem(this);
              h->setPos(m_cageMesh.point(i));
              int col = i % m_cageMesh.cols();
              int row = i / m_cageMesh.cols();
              int dx = ( col == 0 ) ? 0 : ( ( col == m_cageMesh.cols()-1 ) ? -2*m_cageMesh.getRadius() : -m_cageMesh.getRadius() );
              int dy = ( row == 0 ) ? 0 : ( ( row == m_cageMesh.rows()-1 ) ? -2*m_cageMesh.getRadius() : -m_cageMesh.getRadius() );
              h->setRect(dx, dy, 2*m_cageMesh.getRadius(), 2*m_cageMesh.getRadius() );
              m_handles << h;
            }
          }
          for ( int i = 0; i < m_handles.size(); ++i ) {
            m_handles[i]->setVisible(isVisible);
          }
          m_cageOverlay->setVisible(isVisible);
        } else {
          qCDebug(logEditor) << "WARNING: m_cageOverlay is null."; 
        }
        break;
      case LayerItem::OperationMode::Scale:
      case LayerItem::OperationMode::Perspective:
        if ( m_parent != nullptr ) {
           MainWindow* parent = dynamic_cast<MainWindow*>(m_parent);
           if ( parent && parent->getViewer() != nullptr ) {
             parent->getViewer()->setCageVisible(this,mode,isVisible);
           }
        }
        break;
      default:
        qDebug() << " + unprocessed mode " << mode;
    }
    if ( isVisible == false && pushBackImage == true ) {
      setOriginalImage(m_cageMesh.image());
    }
  }
}

int LayerItem::changeNumberOfActiveCagePoints( int step ) 
{
  qCDebug(logEditor) << "LayerItem::changeNumberOfActiveCagePoints(): cage =" << m_cageMesh.cols() << ", step =" << step;
  {
     int expo_c = std::log2(m_cageMesh.cols()-1);
     expo_c -= step > 0 ? 0 : 1;
     int ds_c = pow(2, expo_c);
     int columns = m_cageMesh.cols() + step * ds_c;
     int rows;
     if (EditorStyle::instance().squareCageQuads()) {
       const QRectF br = boundingRect();
       rows = (br.width() > 0 && columns > 1)
            ? qMax(3, qRound(br.height() * (columns - 1) / br.width()) + 1)
            : qMax(3, m_cageMesh.rows() + step * ds_c);
     } else {
       int expo_r = std::log2(qMax(2, m_cageMesh.rows()) - 1);
       expo_r -= step > 0 ? 0 : 1;
       int ds_r = pow(2, expo_r);
       rows = m_cageMesh.rows() + step * ds_r;
     }
     if ( rows >= 3 && columns >= 3 ) {
      const CageState before = cageState();

      // resize() uses general bilinear interpolation and handles arbitrary size
      // changes (including non-power-of-2 steps for squareCageQuads).
      m_cageMesh.setNeedUpdate();
      m_cageMesh.update(boundingRect(), std::max(3,columns), std::max(3,rows));
      // update handles
      qDeleteAll(m_handles);
      m_handles.clear();
      for ( int i = 0; i < m_cageMesh.pointCount(); ++i ) {
       auto* h = new CageControlPointItem(this, i);
       h->setParentItem(this);
       h->setPos(m_cageMesh.point(i));
       int col = i % m_cageMesh.cols();
       int row = i / m_cageMesh.cols();
       int dx = ( col == 0 ) ? 0 : ( ( col == m_cageMesh.cols()-1 ) ? -2*m_cageMesh.getRadius() : -m_cageMesh.getRadius() );
       int dy = ( row == 0 ) ? 0 : ( ( row == m_cageMesh.rows()-1 ) ? -2*m_cageMesh.getRadius() : -m_cageMesh.getRadius() );
       h->setRect(dx, dy, 2*m_cageMesh.getRadius(), 2*m_cageMesh.getRadius() );
       m_handles << h;
      }
      if ( m_cageOverlay ) m_cageOverlay->update();
      cageEditStack()->push(new CageEditCommand(this, before, cageState(),
          step > 0 ? "Increase cage grid" : "Decrease cage grid"));
      return rows;
     }
     return 0;
  }
}

void LayerItem::setNumberOfActiveCagePoints( int nControlPoints ) {
  enableCage(nControlPoints,nControlPoints);
}

void LayerItem::setCageWarpProperty( int type, double value ) {
    if ( type == 1 ) {
       m_cageMesh.setNumberOfRelaxationsSteps(int(value));
    } else if ( type == 2 ) {
       m_cageMesh.setStiffness(value);
    } else if ( type == 3 ) {
       m_cageMesh.setFixedBoundaries(value>0?true:false);
    } else {
       return;
    }
    m_cageMesh.setNeedUpdate();
}

void LayerItem::setCagePoints( const QVector<QPointF>& pts ) {
    m_cageMesh.setPoints(pts);
    update();
}

QVector<QPointF> LayerItem::cagePoints() const {
    return m_cageMesh.points();
}

void LayerItem::updateCagePoint( TransformHandleItem* handle, const QPointF& localPos )
{
  qCDebug(logEditor) << "LayerItem::updateCagePoint(): Processing...";
  {
    int idx = m_handles.indexOf(handle);
    if ( idx < 0 ) return;
    m_cage[idx] = localPos;
    update();
  }
}

// -------------- cage edit undo/redo (separate per-layer stack) --------------

QUndoStack* LayerItem::cageEditStack()
{
    if ( !m_cageEditStack ) {
        m_cageEditStack = new QUndoStack();
        // Only capture initial state here if enableCage() has not already done so
        // (m_cageInitialCols > 0 means enableCage() already captured the uniform grid).
        // Overwriting here would record post-drag (deformed) points as the Reset target.
        if ( m_cageInitialCols == 0 ) {
            m_cageInitialPts     = m_cageMesh.points();
            m_cageInitialOrigPts = m_cageMesh.originalPoints();
            m_cageInitialCols    = m_cageMesh.cols();
            m_cageInitialRows    = m_cageMesh.rows();
            m_cageInitialPos     = pos();
        }
    }
    return m_cageEditStack;
}

LayerItem::CageState LayerItem::cageState() const
{
    return {m_cageMesh.points(), m_cageMesh.originalPoints(), m_cageMesh.cols(),
            m_cageMesh.rows(), pos(), m_originalImageType != ImageType::Warped};
}

void LayerItem::snapshotCageState()
{
    m_cageSnapshot = cageState();
}

void LayerItem::restoreCageState(const CageState& state)
{
    const int cols = state.columns;
    const int rows = state.rows;
    prepareGeometryChange();
    setOffset(QPointF());
    setPos(state.position);
    m_cageMesh.restore(state.points, state.originalPoints, cols, rows);

    // Rebuild cage control point handles (same pattern as changeNumberOfActiveCagePoints)
    const int radius = m_cageMesh.getRadius();
    qDeleteAll(m_handles);
    m_handles.clear();
    for ( int i = 0; i < m_cageMesh.pointCount(); ++i ) {
        auto* h = new CageControlPointItem(this, i);
        h->setParentItem(this);
        h->setPos(m_cageMesh.point(i));
        int col = i % cols;
        int row = i / cols;
        int dx = (col == 0) ? 0 : ((col == cols - 1) ? -2 * radius : -radius);
        int dy = (row == 0) ? 0 : ((row == rows - 1) ? -2 * radius : -radius);
        h->setRect(dx, dy, 2 * radius, 2 * radius);
        m_handles << h;
    }

    QImage warped;
    if (state.unwarped && !m_cageInitialImage.isNull()) {
        setOriginalImage(m_cageInitialImage, ImageType::Original);
        m_alphaMask = m_cageInitialAlphaMask;
        m_totalTransform = m_cageInitialTransform;
        updateCageDisplay();
    } else {
        warped = commitCageWarp("restoreCageState");
        if (warped.isNull())
            return;
    }
    if (m_cageWarpCommand) {
        m_cageWarpCommand->restoreEditState(pos(), state.points, cols, rows, warped);
        if (m_undoStack)
            m_undoStack->resetClean();
    }
}

// ----------------------------------------------------------------------------

void LayerItem::resetCageToPixmap()
{
  qCDebug(logEditor) << "LayerItem::resetCageToPixmap(): size =" << m_handles.size();
  {
    if (m_cageInitialCols > 0) {
        restoreCageState({m_cageInitialPts, m_cageInitialOrigPts,
                          m_cageInitialCols, m_cageInitialRows, m_cageInitialPos, true});
    } else {
        // No edit history yet: reset all points to a uniform grid over the pixmap.
        prepareGeometryChange();
        const qreal w = pixmap().width();
        const qreal h = pixmap().height();
        setOffset(0, 0);
        const int ncols = m_cageMesh.cols();
        const int nrows = m_cageMesh.rows();
        const qreal dx = w / (ncols - 1);
        const qreal dy = h / (nrows - 1);
        QList<QPointF> resetPoints;
        for ( int i = 0; i < nrows; ++i ) {
            for ( int j = 0; j < ncols; ++j )
                resetPoints << QPointF(j * dx, i * dy);
        }
        m_cageMesh.setPoints(resetPoints);
        for ( int i = 0; i < m_handles.size(); ++i ) {
            if ( i < resetPoints.size() )
                m_handles[i]->setPos(resetPoints[i]);
        }
        applyCageWarp("LayerItem");
    }
    // Clear edit history — the cage is back to its starting state.
    // Keep m_cageInitialCols so repeated Reset presses are idempotent.
    if ( m_cageEditStack )
        m_cageEditStack->clear();
  }
}

/** BEST */

void LayerItem::setCagePoint( int idx, const QPointF& pos )
{
  qCDebug(logEditor) << "LayerItem::setCagePoint(): index =" << idx << ", point =" << pos;
  {
    QPointF localPos = mapFromScene(pos);
    if ( EditorStyle::instance().allowIntegerMoveOnly() )
        localPos = QPointF(qRound(localPos.x()), qRound(localPos.y()));

    if ( EditorStyle::instance().noSelfIntersection() ) {
        const QVector<QPointF>& pts = m_cageMesh.points();
        const int cols = m_cageMesh.cols();
        const int rows = m_cageMesh.rows();
        const int row  = idx / cols;
        const int col  = idx % cols;

        // True if segments AB and CD properly intersect (strict, not just touching)
        auto segmentsIntersect = [](const QPointF& a, const QPointF& b,
                                    const QPointF& c, const QPointF& d) -> bool {
            auto cr = [](const QPointF& u, const QPointF& v){ return u.x()*v.y() - u.y()*v.x(); };
            double d1 = cr(d-c, a-c), d2 = cr(d-c, b-c);
            double d3 = cr(b-a, c-a), d4 = cr(b-a, d-a);
            return ((d1>0&&d2<0)||(d1<0&&d2>0)) && ((d3>0&&d4<0)||(d3<0&&d4>0));
        };
        // Signed area × 2 via shoelace — positive = valid TL→TR→BR→BL winding
        auto signedArea2 = [](const QPointF& p0, const QPointF& p1,
                               const QPointF& p2, const QPointF& p3) -> double {
            return p0.x()*(p1.y()-p3.y()) + p1.x()*(p2.y()-p0.y()) +
                   p2.x()*(p3.y()-p1.y()) + p3.x()*(p0.y()-p2.y());
        };
        // Quad is invalid if opposite edges cross (horizontal/vertical escape)
        // OR if the overall winding inverts (diagonal escape)
        auto isSelfIntersecting = [&](const QPointF& p0, const QPointF& p1,
                                      const QPointF& p2, const QPointF& p3) -> bool {
            return segmentsIntersect(p0, p1, p2, p3) ||
                   segmentsIntersect(p1, p2, p3, p0) ||
                   signedArea2(p0, p1, p2, p3) <= 0;
        };
        // Check all quads the moving point (at candidate pos) participates in
        auto quadsOk = [&](const QPointF& candidate) -> bool {
            // Temporarily override pt() for this candidate
            auto ptC = [&](int r, int c) -> QPointF {
                int i = r * cols + c;
                return (i == idx) ? candidate : pts[i];
            };
            if ( row < rows-1 && col < cols-1 )
                if ( isSelfIntersecting(ptC(row,col), ptC(row,col+1), ptC(row+1,col+1), ptC(row+1,col)) ) return false;
            if ( row < rows-1 && col > 0 )
                if ( isSelfIntersecting(ptC(row,col-1), ptC(row,col), ptC(row+1,col), ptC(row+1,col-1)) ) return false;
            if ( row > 0 && col > 0 )
                if ( isSelfIntersecting(ptC(row-1,col-1), ptC(row-1,col), ptC(row,col), ptC(row,col-1)) ) return false;
            if ( row > 0 && col < cols-1 )
                if ( isSelfIntersecting(ptC(row-1,col), ptC(row-1,col+1), ptC(row,col+1), ptC(row,col)) ) return false;
            return true;
        };

        const QPointF current = pts[idx];
        if ( quadsOk(localPos) ) {
            // full move OK — nothing to do, localPos stays
        } else if ( quadsOk(QPointF(localPos.x(), current.y())) ) {
            localPos = QPointF(localPos.x(), current.y());   // slide along X only
        } else if ( quadsOk(QPointF(current.x(), localPos.y())) ) {
            localPos = QPointF(current.x(), localPos.y());   // slide along Y only
        } else {
            return;  // no valid partial move either
        }
    }

    prepareGeometryChange();
    m_cageMesh.setPoint(idx,localPos);
    m_cageMesh.setActiveCagePointId(idx);
    m_cageMesh.relax();
    const QRectF newBounds = QPolygonF(m_cageMesh.points()).boundingRect();
    if ( newBounds.x() != 0 || newBounds.y() != 0 ) {
        qreal dx = newBounds.x();
        qreal dy = newBounds.y();
        m_partialWarpBase = QImage();
        m_partialWarpPrevPoints.clear();
        setPos(mapToScene(QPointF(dx, dy)));
        m_cageMesh.addOffset(dx,dy);
        setOffset(offset() - QPointF(dx, dy));  // counter-shift pixmap to keep image visually stable during drag
        QList<QPointF> pts = m_cageMesh.points();
        for ( int i = 0; i < pts.size(); ++i ) {
            pts[i] -= QPointF(dx, dy);
        }
        m_cageMesh.setPoints(pts);
    }
    for ( int i = 0; i < m_handles.size(); ++i ) {
        m_handles[i]->setPos(m_cageMesh.point(i));
    }
    if ( EditorStyle::instance().useGPU() && EditorStyle::instance().liveWarp() ) {
      applyCageWarp("setCagePoint");
    }
    update();
  }
}

/** */

void LayerItem::commitCageTransform( const QVector<QPointF> &cage )
{
  qCDebug(logEditor) << "LayerItem::commitCageTransform(): Processing...";
  {
    if ( m_cage.size() < 4 || m_originalCage.size() < 4 )
        return;
    QTransform t;
    if ( QTransform::quadToQuad(m_originalCage.mid(0, 4),cage.mid(0, 4),t) ) {
        setTransform(t, false);
    }
    // Neue Basis für weitere Transformationen
    m_cage = cage;
    m_originalCage = m_cage;
    update();
  }
}

void LayerItem::beginCageEdit()
{
  qCDebug(logEditor) << "LayerItem::beginCageEdit(): Processing...";
  {
    m_cageEditing = true;
    m_startPos = pos();
    m_startTransform = transform();
  }
}

void LayerItem::endCageEdit( int idx, const QPointF& startPos )
{
  qCDebug(logEditor) << "LayerItem::endCageEdit(): Processing...";
  {
    QVector<QPointF> cage;
    for ( int i=0 ; i<m_cage.size() ; i++ ) {
      cage.append(QPointF(m_cage[i].x(),m_cage[i].y()));
    }
    cage[idx] = startPos;
    commitCageTransform(cage);
    m_cageEditing = false;
   // if ( undoStack() && m_startTransform != transform() )
   //   undoStack()->push(new TransformLayerCommand(this, m_startPos, pos(), m_startTransform, transform()));
  }
}

LayerItem::OperationMode LayerItem::getPolygonOperationMode()
{
    if ( m_parent != nullptr ) {
      MainWindow* parent = dynamic_cast<MainWindow*>(m_parent);
      if ( parent != nullptr ) {
        return parent->getViewer()->getPolygonOperationMode();
      }
    }
    return LayerItem::OperationMode::None;
}

void LayerItem::setPolygonOperationMode( OperationMode mode )
{
   if ( m_polygonOperationMode == mode )
      return;
   m_polygonOperationMode = mode;
}

void LayerItem::setOperationMode( OperationMode mode ) 
{
   qCDebug(logEditor) << "LayerItem::setOperationMode(): name =" << name() << ", index =" 
                             << m_index << ", mode =" << m_operationMode << "|" << mode;
   {
     if ( m_operationMode == mode ) {
      return;
     }
     if ( m_operationMode == OperationMode::CageWarp ) {
      setCageVisible(2,false);
      // setOriginalImage(m_image);
     }
     // boundingRect() changes when m_hasPivot toggles the pivot-cross expansion
     if ( m_hasPivot ) {
         prepareGeometryChange();
     }
     m_operationMode = mode;
     if ( m_operationMode == OperationMode::CageWarp ) {
       setCageVisible(3,true);
     }
     update();
   }
}

void LayerItem::setRotationAngle( double angleDelta )
{
  qCDebug(logEditor) << "LayerItem::setRotationAngle(): deltaRotationAngle =" << angleDelta << ", currentRotation =" << m_currentRotation;
  {
    QString name = "Rotate Layer";
    TransformLayerCommand::LayerTransformType trafoType = TransformLayerCommand::LayerTransformType::Rotate;
    name += QString(" %1").arg(m_index);

    // setImageTransform() always rotates around imageCenter regardless of what transform
    // is passed (translate/rotate mix due to Qt's pre/post-multiply asymmetry cancels out).
    // A pure rotation R is therefore correct here.
    QTransform t;
    t.rotate(angleDelta);

    // When a custom pivot is set, compute a scene-space position adjustment so the
    // rotation visually happens around the pivot point instead of the image center.
    QPointF posAdj;
    if ( m_hasPivot ) {
      const QPointF pivotScene  = m_pivot;   // already scene coords
      const QPointF centerScene = mapToScene(boundingRect().center());
      const double  rad  = qDegreesToRadians(angleDelta);
      const double  cosA = qCos(rad), sinA = qSin(rad);
      const QPointF off  = centerScene - pivotScene;
      const QPointF rotOff(cosA * off.x() - sinA * off.y(),
                            sinA * off.x() + cosA * off.y());
      posAdj = (pivotScene + rotOff) - centerScene;
    }

    m_currentRotation += angleDelta;
    auto* cmd = new TransformLayerCommand(this, m_startPos, pos(), m_currentRotation, m_startTransform, t, name, trafoType);
    if ( !posAdj.isNull() )
      cmd->setPositionAdjust(posAdj);
    m_undoStack->push(cmd);
  }
}


// ------------------------ Mouse events ------------------------
bool LayerItem::isValidMouseEventOperation()
{
    if ( m_parent != nullptr ) {
      MainWindow* parent = dynamic_cast<MainWindow*>(m_parent);
      MainWindow::MainOperationMode opMode = parent != nullptr ? parent->getOperationMode(): MainWindow::MainOperationMode::None;
      return opMode == MainWindow::MainOperationMode::ImageLayer ? true : false;
    }
    return false;
}

void LayerItem::mouseDoubleClickEvent( QGraphicsSceneMouseEvent* event )
{
  qCDebug(logEditor) << "LayerItem::mouseDoubleClickEvent(): index =" << m_index << ", mode =" << m_operationMode;
  {
    if ( isSelected() && isValidMouseEventOperation() ) {
      if ( m_operationMode == OperationMode::Rotate ) {
        // Accept without action: prevents QGraphicsScene from calling clearSelection()
        // when the double-click lands in the extended bounding rect (pivot area outside pixmap).
        event->accept();
        return;
      }
      if ( m_operationMode == OperationMode::CageWarp ) {
        if ( m_cageEnabled == false ) {
          MainWindow* parent = m_parent != nullptr ? dynamic_cast<MainWindow*>(m_parent) : nullptr;
          if ( parent && parent->getViewer() != nullptr ) {
            parent->getViewer()->setActiveCageLayer(this);
            enableCage(m_cageMesh.cols(),m_cageMesh.rows());
            return;
          }
        } else {
          setCageVisible(4,false);
        }
      } else if ( m_operationMode == OperationMode::Perspective ) {
          qCDebug(logEditor) << " + perpective call +";
      } else if ( m_operationMode == OperationMode::Flip ) {
         mirror(IMainSystem::instance()->getLayerOperationParameter(LayerItem::OperationMode::Flip) > 0 ? 1 : 2);
      }
    }
  }
}

void LayerItem::mousePressEvent( QGraphicsSceneMouseEvent* event )
{
  qCDebug(logEditor) << "LayerItem::mousePressEvent(): layer =" << name() << ", selected ="
               << isSelected() << ", zValue =" << zValue() << ", operationMode =" << m_operationMode
               << ", active =" << m_mouseOperationActive << ", event_modifiers =" << event->modifiers();
  {
    if ( event->button() != Qt::LeftButton ) {
      QGraphicsPixmapItem::mousePressEvent(event);
      return;
    }
    if ( event->modifiers() & Qt::AltModifier ) {
      // In Rotate mode, Alt+click near the pivot starts pivot dragging — don't multi-select.
      const bool nearPivot = ( m_operationMode == OperationMode::Rotate && isSelected()
                                && QLineF(event->pos(), pivot()).length() <= 12.0 );
      if ( !nearPivot ) {
        // Multi-select (Alt/Option): toggle this layer in/out of the co-move group
        ImageView* view = getParentImageView();
        if ( view ) {
          if ( view->isInLayerSelection(this) )
            view->removeLayerFromSelection(this);
          else
            view->addLayerToSelection(this);
          for ( LayerItem* l : view->selectedLayers() )
            l->resetDragStartPos();
        }
        QGraphicsPixmapItem::mousePressEvent(event);
        return;
      }
    }

    // If a different layer is currently active and the click also falls within it,
    // yield to that layer so the active layer always has priority over overlapping
    // inactive layers regardless of Z-order.
    if ( !isSelected() && !(event->modifiers() & Qt::ControlModifier) ) {
      if ( ImageView* view = getParentImageView() ) {
        LayerItem* active = dynamic_cast<LayerItem*>(view->getSelectedItem());
        if ( active && active != this
             && active->contains(active->mapFromScene(event->scenePos())) ) {
          event->ignore();
          return;
        }
      }
    }

    if ( isSelected() || event->modifiers() & Qt::ControlModifier ) {
      MainWindow* parent = m_parent != nullptr ? dynamic_cast<MainWindow*>(m_parent) : nullptr;
      if ( isValidMouseEventOperation() && parent != nullptr ) {
        parent->updateLayerOperationParameter("LayerItem::mousePressEvent",m_name,LayerItem::OperationMode::Rotate,m_currentRotation);
        if ( m_operationMode == OperationMode::Translate ) {
          m_startPos = pos();
          m_mouseOperationActive = true;
          m_startTransform = transform();
          QGraphicsPixmapItem::mousePressEvent(event);
          // Clear multi-selection group on normal drag start (primary layer tracked via isSelected())
          ImageView* view = getParentImageView();
          if ( view )
            view->clearLayerSelection();
        } else if ( m_operationMode == OperationMode::Rotate ) {
          // Check whether the click lands on the pivot marker (12 px hit tolerance in item coords).
          // Pivot dragging requires Alt to be held; without it the click starts rotation.
          const QPointF clickItemPos = event->pos();
          const bool altHeld = event->modifiers() & Qt::AltModifier;
          if ( altHeld && QLineF(clickItemPos, pivot()).length() <= 12.0 ) {
            m_pivotAtDragStart  = pivotScene();   // scene coords
            m_pivot             = m_pivotAtDragStart;  // ensure release without drag doesn't corrupt m_pivot
            m_pivotDragging     = true;
            m_pivotSelected     = true;
            m_pivotWasSet       = m_hasPivot;
            m_mouseOperationActive = true;
            m_startPos          = pos();
            event->accept();
            update();
          } else if ( !altHeld && QLineF(clickItemPos, pivot()).length() <= 12.0 ) {
            // Drag on pivot without Alt → translate the layer
            m_startPos               = pos();
            m_pivotAtDragStart       = pivotScene();
            m_pivotWasSet            = m_hasPivot;
            m_lastPivotTransScenePos = event->scenePos();
            m_pivotTranslating       = true;
            m_mouseOperationActive   = true;
            event->accept();
            update();
          } else {
            m_pivotDragging    = false;
            m_pivotSelected    = false;
            // If the click is in the extended bounding rect (around the pivot)
            // but outside the actual pixmap, consume it silently.  Without this,
            // the base-class call below would ignore() the event (BoundingRectShape
            // only covers the pixmap rect), causing propagation to MainImage which
            // would deselect this layer.
            const QSizeF sz = pixmap().isNull() ? QSizeF(m_image.size()) : QSizeF(pixmap().size());
            if ( !QRectF(offset(), sz).contains(event->pos()) ) {
                event->accept();
                return;
            }
            m_startLayerRotation = m_currentRotation;
            m_rotationPressScreenPos = event->screenPos();
            m_mouseOperationActive = true;
            m_startPos         = pos();
            if ( ImageView* view = getParentImageView() )
                view->setSuppressIndexChangedAdjustments(true);
            QGraphicsPixmapItem::mousePressEvent(event);
          }
        } else {
         m_mouseOperationActive = false;
        }
        // --- updating ---
        parent->setSelectedLayer(7,QString("Layer %1").arg(m_index));
        // to ensure that message is shown even layer is already selected
        parent->showMessage(QString("Select layer %1").arg(m_index));
        // Ctrl: transparent red overlay (see-through mode)
        if ( event->modifiers() & Qt::ControlModifier ) {
          setOpacity(EditorStyle::instance().layerOverlayOpacity());
          // Build alpha-masked red overlay once (reused every frame during drag)
          const QPixmap& px = pixmap();
          if ( !px.isNull() ) {
            m_redOverlayPixmap = QPixmap(px.size());
            m_redOverlayPixmap.fill(QColor(255, 0, 0, 100));
            QPainter op(&m_redOverlayPixmap);
            op.setCompositionMode(QPainter::CompositionMode_DestinationIn);
            op.drawPixmap(0, 0, px);
          }
          m_redOverlay = true;
          update();
        }
      }
    }
  }
}

void LayerItem::mouseMoveEvent( QGraphicsSceneMouseEvent* event )
{
  qCDebug(logEditor) << "LayerItem::mouseMoveEvent(): mode =" << m_operationMode;
  {
    if ( isSelected() ) {
      if ( m_operationMode == OperationMode::Translate ) {
       QPointF posBefore = pos();
       QGraphicsPixmapItem::mouseMoveEvent(event);
       QPointF delta = pos() - posBefore;
       if ( !delta.isNull() ) {
         ImageView* view = getParentImageView();
         if ( view ) {
           for ( LayerItem* l : view->selectedLayers() )
             if ( l != this )
               l->setPos( l->pos() + delta );
         }
       }
      } else if ( m_operationMode == OperationMode::Rotate ) {
       if ( m_pivotTranslating ) {
         const QPointF scenePos = event->scenePos();
         const QPointF delta    = scenePos - m_lastPivotTransScenePos;
         m_lastPivotTransScenePos = scenePos;
         setPos(pos() + delta);
         if ( m_hasPivot )
             m_pivot += delta;
         update();
         event->accept();
       } else if ( m_pivotDragging ) {
         prepareGeometryChange();
         m_pivot    = mapToScene(event->pos());   // store in scene coords
         m_hasPivot = true;
         update();
         event->accept();
       } else {
         MainWindow* parent = m_parent != nullptr ? dynamic_cast<MainWindow*>(m_parent) : nullptr;
         if ( parent != nullptr ) {
           // Ensure adjustments are suppressed for the entire drag, even when Alt was
           // held at press time (which takes an early-return path in mousePressEvent).
           if ( ImageView* view = getParentImageView() )
               view->setSuppressIndexChangedAdjustments(true);
           QPointF screenDelta = QPointF(event->screenPos()) - QPointF(m_rotationPressScreenPos);
           double previewAngle = m_startLayerRotation + screenDelta.x() * EditorStyle::instance().rotationSingleStep() / 10.0;
           // Visual-only preview using Qt's built-in item rotation — no pixel resampling.
           // The real setImageTransform() bake happens once on mouse release.
           QPointF origin = m_hasPivot ? mapFromScene(m_pivot) : boundingRect().center();
           setTransformOriginPoint(origin);
           setRotation(previewAngle - m_startLayerRotation);
           parent->updateLayerOperationParameter("LayerItem::mouseMoveEvent",m_name,LayerItem::OperationMode::Rotate,previewAngle);
           event->accept();
         }
       }
      }
    }
  }
}

void LayerItem::mouseReleaseEvent( QGraphicsSceneMouseEvent* event )
{
  qCDebug(logEditor) << "LayerItem::mouseReleaseEvent(): index =" << m_index << ", name =" << name();
  {
    setOpacity(1.0);
    if ( m_redOverlay ) {
      m_redOverlay = false;
      m_redOverlayPixmap = QPixmap();
      update();
    }

    if ( !isSelected() ) return;
    
    if ( !m_undoStack ) {
      QGraphicsPixmapItem::mouseReleaseEvent(event);
      return;
    }
    if ( m_operationMode == OperationMode::Translate &&
         !( m_layer && m_layer->creator() == "Inpainting" ) ) {
        // Build group: primary (this) + co-selected layers
        ImageView* view = getParentImageView();
        QList<LayerItem*> group;
        group.append(this);
        if ( view ) {
            for ( LayerItem* l : view->selectedLayers() )
                if ( l != this ) group.append(l);
        }

        struct Move { LayerItem* layer; QPointF from; QPointF to; };
        QList<Move> moves;
        for ( LayerItem* l : group ) {
            QPointF from = l->dragStartPos();
            QPointF to = EditorStyle::instance().allowIntegerMoveOnly() ? QPointF(qRound(l->pos().x()), qRound(l->pos().y())) : l->pos();
            if ( to != from )
                moves.append(Move{l, from, to});
        }

        if ( !moves.isEmpty() ) {
            if ( moves.size() > 1 ) m_undoStack->beginMacro("Move Layers");
            for ( const auto& mv : moves )
                m_undoStack->push(new MoveLayerCommand(mv.layer, mv.from, mv.to, mv.layer->id()));
            if ( moves.size() > 1 ) m_undoStack->endMacro();
        }
    } else if ( m_operationMode == OperationMode::Rotate ) {
        if ( m_pivotTranslating ) {
            finishPivotTranslate();
            // Skip base-class release to prevent spurious clearSelection().
            if ( ImageView* view = getParentImageView() )
                view->setSuppressIndexChangedAdjustments(false);
            setSelected(true);
            return;
        }
        const bool pivotWasDragging = m_pivotDragging;
        if ( m_pivotDragging ) {
            const QPointF newPivotScene = m_pivot;   // scene coords
            m_pivotDragging  = false;
            m_pivotSelected  = false;
            update();
            // Only push command when the pivot actually moved
            if ( QLineF(newPivotScene, m_pivotAtDragStart).length() > 0.5 ) {
                m_undoStack->push(new SetPivotCommand(this, m_pivotAtDragStart, newPivotScene, m_pivotWasSet));
            }
        } else {
            ImageView* const view = getParentImageView();
            // Suppress viewport repaints during the entire bake sequence.
            // Without this, Qt can render a frame between setRotation(0) and
            // the new resampled pixmap being set, causing a visible flash of
            // the old unrotated image.
            if (view) view->viewport()->setUpdatesEnabled(false);

            setRotation(0.0);
            setTransformOriginPoint({0.0, 0.0});
            // Bake the final rotation once with full-quality resampling.
            QPointF screenDelta = QPointF(event->screenPos()) - QPointF(m_rotationPressScreenPos);
            double finalAngle = m_startLayerRotation + screenDelta.x() * EditorStyle::instance().rotationSingleStep() / 10.0;
            double angleDelta = finalAngle - m_currentRotation;
            if (view) {
                view->rotateLayer(*this, angleDelta);
                view->setSuppressIndexChangedAdjustments(false);
            } else if (qAbs(angleDelta) > 1e-6) {
                setRotationAngle(angleDelta);
            }
        }
        if ( !m_pivotDragging && EditorStyle::instance().allowIntegerMoveOnly() && m_undoStack->index() > 0 ) {
            const QPointF snapped(qRound(pos().x()), qRound(pos().y()));
            const QPointF adj = snapped - pos();
            if ( !adj.isNull() ) {
                auto* cmd = const_cast<QUndoCommand*>(m_undoStack->command(m_undoStack->index() - 1));
                if ( auto* rotCmd = dynamic_cast<TransformLayerCommand*>(cmd) )
                    rotCmd->setPositionAdjust(adj);
                setPos(snapped);
            }
        }
        if ( pivotWasDragging ) {
            // For pivot press/drag: skip the base-class release to prevent
            // clearSelection() firing on click-without-drag, which would emit
            // selectionChanged() and trigger app-level layer deactivation before
            // our setSelected(true) can restore it.
            setSelected(true);
            return;
        }
    }
    // m_operationMode = None;
    // Safety: ensure suppress flag is always cleared on any release path.
    if ( ImageView* view = getParentImageView() )
        view->setSuppressIndexChangedAdjustments(false);
    QGraphicsPixmapItem::mouseReleaseEvent(event);
    // Qt's default handler deselects the item on click-without-drag (toggle behavior).
    // We always want the layer to stay selected after an active press.
    setSelected(true);
  }
}

// ------------------------ Handles ------------------------
void LayerItem::updateHandles() 
{
  qCDebug(logEditor) << "LayerItem::updateHandles(): Processing...";
  {
    qDeleteAll(m_handles);
    m_handles.clear();
    if ( !isSelected() )
        return;
    QRectF r = boundingRect();
    auto addScale = [&](QPointF p) {
        auto* h = new TransformHandleItem(this, TransformHandleItem::Scale);
        h->setParentItem(this);
        h->setPos(p);
        m_handles << h;
    };
    addScale(r.topLeft());
    addScale(r.topRight());
    addScale(r.bottomLeft());
    addScale(r.bottomRight());
    auto* rot = new TransformHandleItem(this, TransformHandleItem::Rotate);
    rot->setParentItem(this);
    rot->setPos(QPointF(r.center().x(), r.top() - 30));
    m_handles << rot;
  }
}

// -----
void LayerItem::applyPerspectiveQuad( const QVector<QPointF>& quad )
{
  qCDebug(logEditor) << "LayerItem::applyPerspectiveQuad(): Processing...";
  {
    QRectF r = boundingRect();
    QVector<QPointF> src =
    {
        r.topLeft(),
        r.topRight(),
        r.bottomRight(),
        r.bottomLeft()
    };
    QTransform warp;
    if ( QTransform::quadToQuad(src, quad, warp) ) {
      this->setTransform(warp);
    }
  }
}
