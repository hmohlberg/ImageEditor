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

#include "../gui/MainWindow.h"
#include "../gui/ImageView.h"
#include "../undo/TransformLayerCommand.h"
#include "../undo/MirrorLayerCommand.h"
#include "../undo/MoveLayerCommand.h"
#include "../util/Interpolation.h"
#include "../util/GeometryUtils.h"
#include "../util/TriangleWarp.h"

#include <QApplication>
#include <QCoreApplication>
#include <QFile>
#include <QCryptographicHash>
#include <QByteArray>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsScene>
#include <QPainter>
#include <iostream>

// ------------------------ LayerItem ------------------------
LayerItem::LayerItem( const QPixmap& pixmap, QGraphicsItem* parent ) : QGraphicsPixmapItem(pixmap,parent)
{ 
  qCDebug(logEditor) << "LayerItem::LayerItem(): Pixmap processing...";
  {
    m_image = pixmap.toImage();
    m_originalImage = pixmap.toImage();
    init();
  }
}

LayerItem::LayerItem( const QImage& image, QGraphicsItem* parent ) 
      : QGraphicsPixmapItem(parent)
      , m_originalImage(image)
      , m_image(image)
{
  qCDebug(logEditor) << "LayerItem::LayerItem(): Image processing...";
  {
   if ( qobject_cast<QApplication*>(qApp) ) {
    setPixmap(QPixmap::fromImage(m_image));
   } else {
    m_nogui = true;
   }
   init();
  }
}

// >>>
void LayerItem::applyPerspective()
{
   QImage warped = m_perspective.apply(m_originalImage);
   setPixmap(QPixmap::fromImage(warped));
}

// per default moveable layer item
// NOTE: QGraphicsItem::ItemIsSelectable expands the size of the QGraphicsPixmapItem by +1+1 !   
void LayerItem::init() 
{
  setFlags(QGraphicsItem::ItemIsSelectable |
             QGraphicsItem::ItemIsMovable |
             QGraphicsItem::ItemSendsGeometryChanges |
             QGraphicsItem::ItemIsFocusable);
  setTransformationMode(Qt::SmoothTransformation);
  setAcceptedMouseButtons(Qt::LeftButton);
  // setShapeMode(QGraphicsPixmapItem::BoundingRectShape);
  setShapeMode(QGraphicsPixmapItem::MaskShape);
  
  // setZValue(0);
  m_showBoundingBox = true;

  // default color  
  m_lassoPen = QPen(Qt::green);
  m_lassoPen.setWidth(0); // immer 1px auf Bildschirm
  m_lassoPen.setStyle(Qt::SolidLine);
  
  // Selektions-Pen
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
  qInfo() << "  + cage: enabled =" << m_cageEnabled << ", edited=" << m_cageEditing;
  qInfo() << "  + cage overlay =" << (m_cageOverlay != nullptr ? "ok" : "null");
  qInfo() << "  + operation mode =" << m_operationMode;
  qInfo() << "  + bounding box =" << m_showBoundingBox;
  qInfo() << "  + parent ="  << ( m_parent != nullptr ? "null" : "ok" );
  // if ( debugSave && m_index == 1 ) {
  //  std::cout << " LayerItem::printself(): Saving image data... " << std::endl;
  //  m_image.save("/tmp/LayerItem_image.png");
  //  m_originalImage.save("/tmp/LayerItem_originalimage.png");
  // }
}

// ------------------------ Getter / Setter ------------------------
QImage& LayerItem::image( int id ) {
    return id == 1 ? m_originalImage : m_image;
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

const QImage& LayerItem::originalImage() {
	if ( m_originalImage.format() != QImage::Format_ARGB32 )
           m_originalImage = m_originalImage.convertToFormat(QImage::Format_ARGB32);
    return m_originalImage;
}

QString LayerItem::name() const {
  if ( m_index == 0 ) return "MainImage";
  if ( m_layer ) return m_layer->name();
  return m_name;
}

// ------------------------ Update ------------------------
void LayerItem::updatePixmap() {
  if ( qobject_cast<QApplication*>(qApp) ) {
    setPixmap(QPixmap::fromImage(m_image));
  }
}

void LayerItem::resetPixmap() {
  if ( qobject_cast<QApplication*>(qApp) ) {
    setPixmap(QPixmap::fromImage(m_originalImage));
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

void LayerItem::updateOriginalImage() {
  m_originalImage = m_image;
}

// ------------------------ Mirror ------------------------
void LayerItem::setMirror( int mirrorPlane ) {
  qCDebug(logEditor) << "LayerItem::setMirror(): mirrorPlane=" << mirrorPlane;
  {
    #if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
     QImage flippedImage = m_image.flipped(mirrorPlane == 1 ? Qt::Vertical : Qt::Horizontal);
    #else
     QImage flippedImage = m_image.mirrored(mirrorPlane == 1 ? Qt::Vertical : Qt::Horizontal);
    #endif
    m_image = flippedImage;
    updatePixmap();
  }
}

// ------------------------ Transform ------------------------
void LayerItem::setImageTransform( const QTransform& transform, bool combine ) {
  qCDebug(logEditor) << "LayerItem::setImageTransform(): combine =" << combine;
  {
    // *** DO NOT USE INTERNAL TRANSFORMATIONS ***
    if ( 1 == 2 && qobject_cast<QApplication*>(qApp) ) {
      qDebug() << " + internal pixmap processing: transform_origin =" << QGraphicsPixmapItem::transformOriginPoint() << ", pos =" << QGraphicsPixmapItem::pos();
      QGraphicsPixmapItem::setTransform(transform,combine);
      return;
    }
    // *** hard QImage transformation ***
    qDebug() << " + alternative process for case where no pixmap is available...";
    // transform image
    QPointF sceneCenter = mapToScene(QRectF(pixmap().rect()).center());
    QPointF imageCenter = QRectF(m_originalImage.rect()).center();
    m_totalTransform.translate(imageCenter.x(), imageCenter.y());
    m_totalTransform *= transform; // This is my actual rotation
    m_totalTransform.translate(-imageCenter.x(), -imageCenter.y());
    m_image = m_originalImage.transformed(m_totalTransform, Qt::SmoothTransformation);
    QPointF newImageCenter(m_image.width() / 2.0, m_image.height() / 2.0);
    setPos(sceneCenter - newImageCenter);
    // reset transform
    setTransform(QTransform());
    updatePixmap();
  }
}

// ------------------------ Paint ------------------------
void LayerItem::paint( QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget )
{
  qCDebug(logEditor) << "LayerItem::paint(): Processing " << (m_layer?m_layer->name():"unknown") << ", selected = " << isSelected();
  {
    QGraphicsPixmapItem::paint(painter,option,widget);
    if ( isSelected() ) {
      painter->setPen(m_selectedPen);
      painter->setBrush(Qt::NoBrush);
      painter->drawRect(boundingRect());
    } else if ( m_showBoundingBox ) {
      painter->setPen(m_lassoPen);
      painter->setBrush(Qt::NoBrush);
      painter->drawRect(boundingRect());
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
void LayerItem::applyTriangleWarp()
{
  qCDebug(logEditor) << "LayerItem::applyTriangleWarp(): meshActive=" << m_cageMesh.isActive() << ",  m_cageEnabled=" << m_cageEnabled << ", m_cageEditing=" << m_cageEditing;
  {
    TriangleWarp::WarpResult warped = TriangleWarp::warp(m_originalImage,m_cageMesh);
    if ( !warped.image.isNull() ) {
     setPixmap(QPixmap::fromImage(warped.image));
     m_image = warped.image;
     QGraphicsPixmapItem::setPos(QGraphicsPixmapItem::pos() + m_cageMesh.getOffset());
     m_cageMesh.setOffset();
    } else {
     qCDebug(logEditor) << "LayerItem::applyTriangleWarp(): WARNING: Image isNull...";
    } 
  }
}

void LayerItem::applyCageWarp()
{
  qDebug() << "DEPRECTAED **** LayerItem::applyCageWarp(): None!";
}

void LayerItem::enableCage( int cols, int rows )
{
  qCDebug(logEditor) << "LayerItem::enableCage(): cols =" << cols << ", rows =" << rows << ", enabled =" << m_cageEnabled;
  {
    m_cageMesh.create(boundingRect(), cols, rows);
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
      m_handles << h;
    }
  }
}

// --- what about the grid ??? ---

void LayerItem::initCage( const QVector<QPointF>& pts, const QRectF &rect, int nrows, int ncolumns )
{
  qCDebug(logEditor) << "LayerItem::initCage(): cageOverlay =" << (m_cageOverlay!=nullptr?"ok":"null") << ", rect =" << rect << ", rows =" << nrows << ", ncolumns =" << ncolumns;
  {
    m_cageMesh.create(rect,nrows,ncolumns);  
    m_cageMesh.setPoints(pts);
    if ( m_cageOverlay == nullptr ) {
      m_cageOverlay = new CageOverlayItem(this);
      m_cageOverlay->setParentItem(this);
    }
  }
}

void LayerItem::setCageVisible( bool isVisible )
{
  qCDebug(logEditor) << "LayerItem::setCageVisible(): name = "  << name() << ", cageOverlay =" << (m_cageOverlay != nullptr?"ok":"none") << ", cageEditing=" << m_cageEditing << "|" << m_cageEnabled << " nControlPoints=" << m_cageMesh.pointCount();
  {
   if ( m_cageOverlay != nullptr ) {
     if ( isVisible  ) {
      m_cageOverlay->setVisible(true);
     } else {
       if ( m_parent != nullptr ) { 
        MainWindow* parent = dynamic_cast<MainWindow*>(m_parent);
        if ( parent && parent->getViewer() != nullptr ) {
         parent->getViewer()->setActiveCageLayer(nullptr);
        }
       }    
       m_cageMesh.setActive(false);
       m_cageEnabled = false;
       m_cageEditing = false;
       for ( int i = 0; i < m_handles.size(); ++i ) {
         m_handles[i]->setVisible(false);
       }
       m_cageOverlay->setVisible(false);
       update();
    }
   }
  }
}

void LayerItem::setCageVisible( LayerItem::OperationMode mode, bool isVisible )
{
  qCDebug(logEditor) << "LayerItem::setCageVisible(): mode =" << mode << ", isVisible =" << isVisible;
  { 
    switch ( mode ) {
      case LayerItem::OperationMode::CageWarp:
        if ( m_cageOverlay != nullptr ) {
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
 }
}

int LayerItem::changeNumberOfActiveCagePoints( int step ) 
{
  qCDebug(logEditor) << "LayerItem::changeNumberOfActiveCagePoints(): cage = " << m_cageMesh.cols() << ", step =" << step;
  {
     int expo = std::log2(m_cageMesh.cols()-1);
     expo -= step > 0 ? 0 : 1;
     int ds = pow(2,expo);
     int columns = m_cageMesh.cols();
     columns += step*ds;
     int rows = m_cageMesh.rows();
     rows += step*ds;
     // update cage
     m_cageMesh.needUpdate();
     m_cageMesh.update(boundingRect(),std::max(3,rows),std::max(3,columns));
     // update handles
     qDeleteAll(m_handles);
     m_handles.clear();
     for ( int i = 0; i < m_cageMesh.pointCount(); ++i ) {
      auto* h = new CageControlPointItem(this, i);
      h->setParentItem(this);
      h->setPos(m_cageMesh.point(i));
      m_handles << h;
     }
     return rows;
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
    m_cageMesh.needUpdate();
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
    update(); // neu zeichnen
  }
}

void LayerItem::setCagePoint( int idx, const QPointF& pos )
{
  qCDebug(logEditor) << "LayerItem::setCagePoint(): index=" << idx << ", pos=(" << pos.x() << ":" << pos.y() << ")...";
  {
    QRectF bounds_before = QPolygonF(m_cageMesh.points()).boundingRect();
    QPointF local = mapFromScene(pos);
    m_cageMesh.setPoint(idx, local);
    QRectF bounds_after = QPolygonF(m_cageMesh.points()).boundingRect();
    qreal dx = bounds_after.x() - bounds_before.x();
    qreal dy = bounds_after.y() - bounds_before.y();
    m_cageMesh.addOffset(dx,dy);
    for ( int i = 0; i < m_handles.size(); ++i )
        m_handles[i]->setPos(m_cageMesh.point(i));
    m_cageMesh.relax();
    update(); 
  } 
}

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
    m_cageEditing = true;
    m_startPos = pos();
    m_startTransform = transform();
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
   qCDebug(logEditor) << "LayerItem::setOperationMode(): index=" << m_index << ", mode=" << m_operationMode << "|" << mode;
   {
     if ( m_operationMode == mode )
      return;
     if ( m_operationMode == OperationMode::CageWarp ) {
      setCageVisible(false);
     }
     m_operationMode = mode;
   }
}

void LayerItem::setRotationAngle( double value )
{
  qCDebug(logEditor) << "LayerItem::setRotationAngle(): value =" << value;
  {
    // --- create undo command ---
    if ( m_transformLayerCommand == nullptr ) {
      QString name = "Rotate Layer";
      TransformLayerCommand::LayerTransformType trafoType = TransformLayerCommand::LayerTransformType::Rotate;
      name += QString(" %1").arg(m_index);
      m_transformLayerCommand = new TransformLayerCommand(this, m_startPos, pos(), m_startTransform, transform(), name, trafoType);
      m_undoStack->push(m_transformLayerCommand);
    }
    // --- update layer ---
    QPointF c = boundingRect().center();
    QTransform t = m_startTransform;
    t.translate(c.x(), c.y());
    t.rotate(value);
    t.translate(-c.x(), -c.y());
    setTransform(t);
    m_currentRotation = value;
    // --- update undo command ---
    m_transformLayerCommand->setRotationAngle(value);
    m_transformLayerCommand->setTransform(t);
    // >>>
    update();
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
  qCDebug(logEditor) << "LayerItem::mouseDoubleClickEvent(): index=" << m_index << ", mode=" << m_operationMode;
  {
    if ( isValidMouseEventOperation() ) {
      if ( m_operationMode == OperationMode::CageWarp ) {
        if ( m_cageEnabled == false ) {
           if ( m_parent != nullptr ) {
            MainWindow* parent = dynamic_cast<MainWindow*>(m_parent);
            if ( parent && parent->getViewer() != nullptr ) {
             parent->getViewer()->setActiveCageLayer(this);
             int nCagePoints = m_cageMesh.rows();
             enableCage(nCagePoints,nCagePoints);
             return;
            }
           }
           enableCage(3,3);
        } else {
           setCageVisible(false);
        }
      } else if ( m_operationMode == OperationMode::Perspective ) {
          qCDebug(logEditor) << " + perpective call +";
      } else if ( m_operationMode == OperationMode::Flip ) {
         m_undoStack->push(new MirrorLayerCommand(this, m_index, 1));
      } else if ( m_operationMode == OperationMode::Flop ) {
         m_undoStack->push(new MirrorLayerCommand(this, m_index, 2));
      }
    }
  }
}

void LayerItem::mousePressEvent( QGraphicsSceneMouseEvent* event )
{
  qCDebug(logEditor) << "LayerItem::mousePressEvent(): layer =" << name() << ", operationMode =" << m_operationMode << ", active=" << m_mouseOperationActive;
  {
    if ( event->button() != Qt::LeftButton ) {
     QGraphicsPixmapItem::mousePressEvent(event);
     return;
    }
    MainWindow* parent = m_parent != nullptr ? dynamic_cast<MainWindow*>(m_parent) : nullptr;
    if ( isValidMouseEventOperation() && parent != nullptr ) {
       if ( m_operationMode == OperationMode::Translate ) {
          m_startPos = pos();
          m_mouseOperationActive = true;
          m_startTransform = transform();
          QGraphicsPixmapItem::mousePressEvent(event);
       } else if ( m_operationMode == OperationMode::Rotate ) {
       
          QPointF clickPos = event->scenePos();
          QPointF center = mapToScene(boundingRect().center());
          double angleRad = std::atan2(clickPos.y() - center.y(), clickPos.x() - center.x());
          m_startMouseAngle = angleRad * 180.0 / M_PI;
          m_startLayerRotation = m_currentRotation;
          
          m_mouseOperationActive = true; 
          m_startPos = pos();
         // m_startTransform = transform();
          QGraphicsPixmapItem::mousePressEvent(event);
       } else {
         // m_operationMode = OperationMode::None;
         m_mouseOperationActive = false;
       }
    }
  }
}

void LayerItem::mouseMoveEvent( QGraphicsSceneMouseEvent* event )
{
  qCDebug(logEditor) << "LayerItem::mouseMoveEvent(): mode =" << m_operationMode;
  {
    if ( m_operationMode == OperationMode::Translate ) {
      QGraphicsPixmapItem::mouseMoveEvent(event);
      return;
    }
    MainWindow* parent = m_parent != nullptr ? dynamic_cast<MainWindow*>(m_parent) : nullptr;
    if ( m_operationMode == OperationMode::Rotate ) {
        QPointF delta = event->scenePos() - event->buttonDownScenePos(Qt::LeftButton);
        QPointF c = boundingRect().center();
        // QTransform t = m_startTransform;
        // t.translate(c.x(), c.y());
        if ( m_operationMode == OperationMode::Rotate ) {
            QPointF center = mapToScene(boundingRect().center());
            QPointF currentPos = event->scenePos();
            double currentMouseAngleDeg = std::atan2(currentPos.y() - center.y(), 
                                                 currentPos.x() - center.x()) * 180.0 / M_PI;
            double angleDelta = currentMouseAngleDeg - m_startMouseAngle;
            double newRotation = m_startLayerRotation + angleDelta;
            setRotationAngle(newRotation);
            parent->updateLayerOperationParameter(LayerItem::OperationMode::Rotate,newRotation); 
        } else {
            QTransform t = m_startTransform;
            t.translate(c.x(), c.y());
            qreal s = qMax(0.1, 1.0 + delta.y() * 0.005);
            t.scale(s, s);
            t.translate(-c.x(), -c.y());
            setTransform(t);
        }
        event->accept();
    }
  }
}

void LayerItem::mouseReleaseEvent( QGraphicsSceneMouseEvent* event )
{
  qCDebug(logEditor) << "LayerItem::mouseReleaseEvent(): index=" << m_index << ", name=" << name();
  {
    if ( !m_undoStack ) {
      QGraphicsPixmapItem::mouseReleaseEvent(event);
      return;
    }
    if ( m_operationMode == OperationMode::Translate ) {
        qCDebug(logEditor) << "LayerItem::mouseReleaseEvent(): MoveLayer " << m_index << " processing...";
        if ( pos() != m_startPos ) {
            // create new (only if really new otherwise it will merge automatically and no new entry will be created)
            m_undoStack->push(
                new MoveLayerCommand(this, m_startPos, pos(), m_index)
            );
        }
    } else if ( m_operationMode == OperationMode::Rotate ) {
       /*
        if ( transform() != m_startTransform ) {
            QString name = "Rotate Layer";
            TransformLayerCommand::LayerTransformType trafoType = TransformLayerCommand::LayerTransformType::Rotate;
            name += QString(" %1").arg(m_index);
            TransformLayerCommand *layerCommand = new TransformLayerCommand(this, m_startPos, pos(), m_startTransform, transform(), name, trafoType);
            m_undoStack->push(layerCommand);
        }
       */
    }
    // m_operationMode = None;
    QGraphicsPixmapItem::mouseReleaseEvent(event);
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