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
#include "../undo/CageWarpCommand.h"
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
    m_originalImageType = ImageType::Original; 
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
   m_originalImageType = ImageType::Original; 
   m_nogui = (qobject_cast<QApplication*>(qApp) == nullptr);
   if ( !m_nogui ) {
     setPixmap(QPixmap::fromImage(m_image));
     init();
   }
  }
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
      return pixmapRect.united(cageRect);
   }
   return pixmapRect;
}

// per default moveable layer item
// NOTE: QGraphicsItem::ItemIsSelectable expands the size of the QGraphicsPixmapItem by +1+1 !   
void LayerItem::init() 
{
  setFlags(QGraphicsItem::ItemIsSelectable |
             QGraphicsItem::ItemIsMovable |
             QGraphicsItem::ItemSendsGeometryChanges |
             QGraphicsItem::ItemIsFocusable);
  setTransformationMode(EditorStyle::instance().transformationMode());
  setAcceptedMouseButtons(Qt::LeftButton);
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

void LayerItem::setUndoStack( QUndoStack* stack ) {
  m_undoStack = stack; 
}

void LayerItem::setImageRect( const QRectF& rect ) {
  prepareGeometryChange();
}

void LayerItem::setOriginalImage( const QImage& aImage, ImageType imageType) {
  qCDebug(logEditor) << "LayerItem::setOriginalImage(): Processing...";
  {
    m_originalImage = aImage;
    m_originalImageType = imageType;
    m_cageApplied = imageType == ImageType::Warped ? true : false;
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

// ------------------------ Selected ------------------------
void LayerItem::setIsSelected( int caller, bool isSelected ) 
{
  qDebug() << "LayerItem::setIsSelected(" << caller << "): name =" << name() 
                  << ", select =" << isSelected << ", operationMode =" << m_operationMode;
  {
    MainWindow* parent = m_parent != nullptr ? dynamic_cast<MainWindow*>(m_parent) : nullptr;
    if ( parent != nullptr ) {
      if ( isSelected ) {
        parent->getViewer()->setSelectedLayer(1,this);
        parent->setLayerOperationMode(m_operationMode,false);
      }
      parent->updateLayerOperationParameter(LayerItem::OperationMode::Rotate,m_currentRotation);
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
      parent->updateLayerOperationParameter(LayerItem::OperationMode::Scale,xscale,yscale);
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
    setPos(position);
    m_image = image;
    m_totalTransform = transform;
    updatePixmap();
  }
}

void LayerItem::setImageTransform( const QTransform& transform, bool combine ) {
  qDebug() << "LayerItem::setImageTransform(): position =" << pos() << ", combine =" 
                        << combine << ", originalImageType =" << m_originalImageType << ", transform =" << transform;
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
     m_originalImage = m_image.copy();
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
    m_image = m_originalImage.transformed(m_totalTransform,Qt::SmoothTransformation);
    QPointF newImageCenter(m_image.width() / 2.0, m_image.height() / 2.0);
    setPos(sceneCenter - newImageCenter);
    // reset transform
    setTransform(QTransform());
    // >>>
    updatePixmap();
  }
}

// ------------------------ Paint ------------------------
void LayerItem::paint( QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget )
{
#if 0
  qCDebug(logEditor) << "LayerItem::paint(): Processing " << (m_layer?m_layer->name():"unknown") << ", selected =" << isSelected();
#endif
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

QImage LayerItem::applyCageWarp( const QString &caller )
{
  qDebug() << "LayerItem::applyCageWarp(" << caller << "): name =" << name() << ", meshActive =" << m_cageMesh.isActive()
        << ", activeCagePointId =" << m_cageMesh.activeCagePointId()
        << ", cageEnabled =" << m_cageEnabled << ", cageEditing =" << m_cageEditing << ", intialized =" << m_cageMesh.isInitialized();
  {
    #if 0
     // removed by CLAUDE
     if ( !m_cageEditing ) {
      return m_cageMesh.image(); // muss das original image sein vor jedem warp !!!
     }
    #endif
    if ( !m_cageMesh.isInitialized(true) ) {
      m_cageMesh.setImage(m_image);
    }
    TriangleWarp::WarpResult warped = TriangleWarp::warp(m_cageMesh.image(),m_cageMesh);
    if ( !warped.image.isNull() ) {
     setPixmap(QPixmap::fromImage(warped.image));
     m_image = warped.image;
     QGraphicsPixmapItem::setPos(QGraphicsPixmapItem::pos() + m_cageMesh.getOffset());
     m_cageApplied = true;
     return m_image.copy();
    } else {
     qCritical() << "CRITICAL - LayerItem::applyCageWarp(): WARNING: Image isNull.";
    }
    return QImage(); 
  }
}

void LayerItem::enableCage( int cols, int rows )
{
  qCDebug(logEditor) << "LayerItem::enableCage(): cols =" << cols << ", rows =" << rows 
                  << ", initialized =" << m_cageMesh.isInitialized() << ", enabled =" << m_cageEnabled;
  {
    if ( m_cageMesh.isInitialized() ) return;
    int ncols = cols == -1 ? m_cageMesh.cols() : cols;
    int nrows = rows == -1 ? m_cageMesh.rows() : rows;
    m_cageMesh.create(boundingRect(), ncols, nrows);
    if ( !m_cageMesh.isInitialized() ) m_cageMesh.setImage(m_image);
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
      int row = i / m_cageMesh.rows();
      int dx = ( col == 0 ) ? 0 : ( ( col == m_cageMesh.cols()-1 ) ? -2*m_cageMesh.getRadius() : -m_cageMesh.getRadius() );
      int dy = ( row == 0 ) ? 0 : ( ( row == m_cageMesh.rows()-1 ) ? -2*m_cageMesh.getRadius() : -m_cageMesh.getRadius() );
      h->setRect(dx, dy, 2*m_cageMesh.getRadius(), 2*m_cageMesh.getRadius() );
      m_handles << h;
    }
  }
}

// --- what about the grid ??? ---

void LayerItem::initCage( const QVector<QPointF>& pts, const QRectF &rect, int nrows, int ncolumns )
{
  qCDebug(logEditor) << "LayerItem::initCage(): cageOverlay =" << (m_cageOverlay!=nullptr?"ok":"null") 
                          << ", rect =" << rect << ", rows =" << nrows << ", ncolumns =" << ncolumns;
  {
    m_cageMesh.setIsInitialized();
    m_cageMesh.create(rect,nrows,ncolumns);  
    // m_cageMesh.setImage(m_image);  // ADDED by CLAUDE 
    //   04.01.2026: REMOVED by Hartmut: 
    //       Multiple undo/redo operations result in cumulative distortions
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
     int expo = std::log2(m_cageMesh.cols()-1);
     expo -= step > 0 ? 0 : 1;
     int ds = pow(2,expo);
     int columns = m_cageMesh.cols();
     columns += step*ds;
     int rows = m_cageMesh.rows();
     rows += step*ds;
     if ( rows >= 3 && columns >= 3 ) {
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
       int col = i % m_cageMesh.cols();
       int row = i / m_cageMesh.cols();
       int dx = ( col == 0 ) ? 0 : ( ( col == m_cageMesh.cols()-1 ) ? -2*m_cageMesh.getRadius() : -m_cageMesh.getRadius() );
       int dy = ( row == 0 ) ? 0 : ( ( row == m_cageMesh.rows()-1 ) ? -2*m_cageMesh.getRadius() : -m_cageMesh.getRadius() );
       h->setRect(dx, dy, 2*m_cageMesh.getRadius(), 2*m_cageMesh.getRadius() );
       m_handles << h;
      }
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
    update();
  }
}

void LayerItem::resetCageToPixmap()
{
  qCDebug(logEditor) << "LayerItem::resetCageToPixmap(): size =" << m_handles.size();
  {
    prepareGeometryChange();
    qreal w = pixmap().width();
    qreal h = pixmap().height();
    setOffset(0, 0);
    QList<QPointF> resetPoints;
    int nrc = qSqrt(m_handles.size());
    qreal dx = w/(nrc-1);
    qreal dy = h/(nrc-1);
    for ( int i=0 ; i<nrc ; i++ ) {
      qreal y = i*dy;
      for ( int j=0 ; j<nrc ; j++ ) {
        qreal x = j*dx;
        resetPoints << QPointF(x, y);
      }
    }
    m_cageMesh.setPoints(resetPoints);
    for ( int i = 0; i < m_handles.size(); ++i ) {
        if ( i < resetPoints.size() ) {
            m_handles[i]->setPos(resetPoints[i]);
        }
    }
    applyCageWarp("LayerItem");
  }
}

/** BEST */

void LayerItem::setCagePoint( int idx, const QPointF& pos )
{
  qCDebug(logEditor) << "LayerItem::setCagePoint(): index =" << idx << ", point =" << pos;
  {
    QPointF localPos = mapFromScene(pos);
    m_cageMesh.setPoint(idx,localPos);
    QRectF newBounds = QPolygonF(m_cageMesh.points()).boundingRect();
    if ( newBounds.x() != 0 || newBounds.y() != 0 ) {
        qreal dx = newBounds.x();
        qreal dy = newBounds.y();
        prepareGeometryChange();
        setPos(mapToScene(QPointF(dx, dy)));
        m_cageMesh.addOffset(dx,dy);
        // setOffset(offset() - QPointF(dx, dy));
        QList<QPointF> pts = m_cageMesh.points();
        for ( int i = 0; i < pts.size(); ++i ) {
            pts[i] -= QPointF(dx, dy);
        }
        m_cageMesh.setPoints(pts);
    }
    for ( int i = 0; i < m_handles.size(); ++i ) {
        m_handles[i]->setPos(m_cageMesh.point(i));
    }
    m_cageMesh.setActiveCagePointId(idx);
    m_cageMesh.relax();
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
     if ( m_operationMode == mode )
      return;
     if ( m_operationMode == OperationMode::CageWarp ) {
      setCageVisible(2,false);
      // setOriginalImage(m_image);
     }
     m_operationMode = mode;
     if ( m_operationMode == OperationMode::CageWarp ) {
       setCageVisible(3,true);
     }
   }
}

void LayerItem::setRotationAngle( double angleDelta )
{
  qCDebug(logEditor) << "LayerItem::setRotationAngle(): deltaRotationAngle =" << angleDelta << ", currentRotation =" << m_currentRotation;
  {
    QString name = "Rotate Layer";
    TransformLayerCommand::LayerTransformType trafoType = TransformLayerCommand::LayerTransformType::Rotate;
    name += QString(" %1").arg(m_index);
    QPointF c = boundingRect().center();
    QTransform t;
    t.translate(c.x(), c.y());
    t.rotate(angleDelta);
    t.translate(-c.x(), -c.y());
    m_currentRotation += angleDelta;
    m_undoStack->push(
       new TransformLayerCommand(this, m_startPos, pos(), m_currentRotation, m_startTransform, t, name, trafoType)
    );
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
      if ( m_operationMode == OperationMode::CageWarp ) {
        if ( m_cageEnabled == false ) {
          MainWindow* parent = m_parent != nullptr ? dynamic_cast<MainWindow*>(m_parent) : nullptr;
          if ( parent && parent->getViewer() != nullptr ) {
            parent->getViewer()->setActiveCageLayer(this);
            enableCage(m_cageMesh.rows(),m_cageMesh.cols());
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
  qDebug() << "LayerItem::mousePressEvent(): layer =" << name() << ", selected =" 
               << isSelected() << ", zValue =" << zValue() << ", operationMode =" << m_operationMode 
               << ", active =" << m_mouseOperationActive;
  {
    if ( event->button() != Qt::LeftButton ) {
      QGraphicsPixmapItem::mousePressEvent(event);
      return;
    }
    if ( isSelected() || event->modifiers() & Qt::AltModifier || event->modifiers() & Qt::MetaModifier ) {
      MainWindow* parent = m_parent != nullptr ? dynamic_cast<MainWindow*>(m_parent) : nullptr;
      if ( isValidMouseEventOperation() && parent != nullptr ) {
        parent->updateLayerOperationParameter(LayerItem::OperationMode::Rotate,m_currentRotation);
        if ( m_operationMode == OperationMode::Translate ) {
          m_startPos = pos();
          m_mouseOperationActive = true;
          m_startTransform = transform();
          QGraphicsPixmapItem::mousePressEvent(event);
        } else if ( m_operationMode == OperationMode::Rotate ) {
          m_startLayerRotation = m_currentRotation;
          m_mouseOperationActive = true; 
          m_startPos = pos();
          QGraphicsPixmapItem::mousePressEvent(event);
        } else {
         // m_operationMode = OperationMode::None;
         m_mouseOperationActive = false;
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
       QGraphicsPixmapItem::mouseMoveEvent(event);
      } else if ( m_operationMode == OperationMode::Rotate ) {
       MainWindow* parent = m_parent != nullptr ? dynamic_cast<MainWindow*>(m_parent) : nullptr;
       if ( parent != nullptr ) {
        QPointF delta = event->scenePos() - event->buttonDownScenePos(Qt::LeftButton);
        double angleDelta = m_startLayerRotation + delta.x()/20.0 - m_currentRotation;
        setRotationAngle(angleDelta);
        parent->updateLayerOperationParameter(LayerItem::OperationMode::Rotate,m_currentRotation); 
        event->accept();
       }
      }
    }
  }
}

void LayerItem::mouseReleaseEvent( QGraphicsSceneMouseEvent* event )
{
  qCDebug(logEditor) << "LayerItem::mouseReleaseEvent(): index =" << m_index << ", name =" << name();
  {
    if ( !isSelected() ) return;
    if ( !m_undoStack ) {
      QGraphicsPixmapItem::mouseReleaseEvent(event);
      return;
    }
    if ( m_operationMode == OperationMode::Translate ) {
        if ( pos() != m_startPos ) {
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
           // TransformLayerCommand *layerCommand = new TransformLayerCommand(this, m_startPos, pos(), m_startTransform, transform(), name, trafoType);
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