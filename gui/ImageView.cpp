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

#include "ImageView.h"
#include "MainWindow.h"

#include "../core/Config.h"
#include "../layer/LayerItem.h"
#include "../layer/MaskLayerItem.h"
#include "../layer/EditablePolygon.h"
#include "../layer/EditablePolygonItem.h"
#include "../undo/EditablePolygonCommand.h"
#include "../undo/PaintCommand.h"
#include "../undo/CageWarpCommand.h"
#include "../undo/MaskStrokeCommand.h"
#include "../undo/PaintStrokeCommand.h"
#include "../undo/InvertLayerCommand.h"
#include "../undo/LassoCutCommand.h"
#include "../util/QImageUtils.h"
#include "../util/MaskUtils.h"

#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QGraphicsScene>
#include <QScrollBar>
#include <QWheelEvent>

#include <iostream>

/* ============================================================
 * Helper
 * ============================================================ */
static qreal pointToSegmentDist(const QPointF& p,
                                const QPointF& a,
                                const QPointF& b)
{
    const QPointF ab = b - a;
    const qreal t = std::clamp(
        QPointF::dotProduct(p - a, ab) / QPointF::dotProduct(ab, ab),
        0.0, 1.0
    );
    const QPointF proj = a + t * ab;
    return QLineF(p, proj).length();
}

static qreal distanceToPolygon(const QPointF& p, const QPolygonF& poly)
{
    qreal minDist = std::numeric_limits<qreal>::max();
    for (int i = 0; i < poly.size(); ++i) {
        QPointF a = poly[i];
        QPointF b = poly[(i + 1) % poly.size()];
        minDist = std::min(minDist, pointToSegmentDist(p, a, b));
    }
    return minDist;
}

/* ============================================================
 * Constructor
 * ============================================================ */
ImageView::ImageView( QWidget* parent ) : QGraphicsView(parent),
		m_parent(parent)
{   
    m_scene = new QGraphicsScene(this);
    m_scene->setItemIndexMethod(QGraphicsScene::NoIndex); // Hack to prevent crash
    // setup 
    setMouseTracking(true);
    setRenderHint(QPainter::Antialiasing);
    setViewportUpdateMode(QGraphicsView::FullViewportUpdate);
    setDragMode(QGraphicsView::NoDrag);
    m_undoStack = new QUndoStack(this);
    connect(m_undoStack, &QUndoStack::cleanChanged, this, [this](bool isClean){
      setWindowModified(!isClean);
      setWindowTitle(QString("Mein Editor[*]"));
    });
    
   /** 
    setDragMode(QGraphicsView::NoDrag);
    setMouseTracking(true);
    setScene(new QGraphicsScene(this));
    setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
   */
}

// ------------------------ Self info -------------------------------------
void ImageView::printself() 
{
  qInfo() << " ImageView::printself():";
  qInfo() << "  + Pixmap items in scene:" << getScene()->items().count();
  auto items = getScene()->items();
  for ( QGraphicsItem* item : items ) {
    if ( auto pixmapItem = qgraphicsitem_cast<QGraphicsPixmapItem*>(item) ) {
        qInfo() << "   + Pixmap found: Size =" << pixmapItem->pixmap().size() << ", position =" << item->pos() << ", visible =" << item->isVisible();
    } else if ( auto polyItem = qgraphicsitem_cast<QGraphicsPolygonItem*>(item) ) {
        qInfo() << "   + Polygon found with " << polyItem->polygon().size() << "points, " << ", visible =" << item->isVisible();
    } else if ( auto lineItem = qgraphicsitem_cast<QGraphicsLineItem*>(item) ) {
        qInfo() << "   + Line found";
    } else if ( auto ellipseItem = qgraphicsitem_cast<QGraphicsEllipseItem*>(item) ) {
        QGraphicsEllipseItem* ellipse = qgraphicsitem_cast<QGraphicsEllipseItem*>(item);
        if ( ellipse ) {
          qInfo() << "   + Ellipse found: rect =" << ellipse->rect() << ", position =" << item->pos() << ", visible =" << item->isVisible();
        }
    } else if ( auto pathItem = qgraphicsitem_cast<QGraphicsPathItem*>(item) ) {
        qInfo() << "   + Path found";
    } else if ( auto rectItem = qgraphicsitem_cast<QGraphicsRectItem*>(item) ) {
        qInfo() << "   + Rect found";
    } else if ( auto textItem = qgraphicsitem_cast<QGraphicsTextItem*>(item) ) {
        qInfo() << "   + Text found";
    } else if ( item->type() == 65536 ) {
        QGraphicsObject* obj = item->toGraphicsObject();
        if ( obj ) {
          QString name = obj->objectName().isEmpty() ? "none" : obj->objectName();
          const char* className = typeid(*obj).name();
          qInfo() << "   + QGraphicsObject (65536):" << " name =" << name << ", class (mangled) = " << className
                       << ", position =" << obj->pos() << ", visible =" << item->isVisible();
        }
    } else {
        qInfo() << "   + Unknown item found: " << item->type();
    }
  }
}

// ------------------------ Update -------------------------------------
void ImageView::forcedUpdate()
{
  qCDebug(logEditor) << "ImageView::forcedUpdate(): Processing...";
  {
    if ( m_selectedLayer != nullptr ) {
     m_selectedLayer->disableCage();
    } else {
     qInfo() << " - no selected layer found";
    }
  }
}

void ImageView::rebuildUndoStack()
{
  qCDebug(logEditor) << "ImageView::rebuildUndoStack(): Processing...";
}

// ------------------------ Mask layer -------------------------------------
void ImageView::createMaskLayer( const QSize& size )
{
   qCDebug(logEditor) << "ImageView::createMaskLayer(): size =" << size;
   {
    // delete old mask
    if ( m_maskItem ) {
      QMessageBox::StandardButton reply;
      reply = QMessageBox::question(this, "Please confirm operation", 
                     "A label mask already exists. Are you sure you want to create a new one? The old data will be completely deleted?",
                     QMessageBox::Yes | QMessageBox::No);
      if ( reply == QMessageBox::Yes ) {
        scene()->removeItem(m_maskItem);
        delete m_maskItem;
        m_maskItem = nullptr;
      } else {
       return;
      }
    }
    delete m_maskLayer;
    m_maskLayer = nullptr;
    // --- Mask data ---
    m_maskLayer = new MaskLayer(size);
    // --- Mask overlay ---
    m_maskItem = new MaskLayerItem(m_maskLayer);
    m_maskItem->setZValue(1000);
    m_maskItem->setOpacityFactor(0.4);
    m_maskItem->setLabelColors(defaultMaskColors());
    scene()->addItem(m_maskItem);
   }
}

void ImageView::saveMaskImage( const QString& filename ) {
  if ( m_maskLayer != nullptr ) {
   QImage indexedImage = m_maskLayer->image().convertToFormat(QImage::Format_Indexed8);
   QList<QRgb> colorTable;
   colorTable.reserve(256);
   QVector<QColor> maskColors = defaultMaskColors();
   for ( const QColor &c : maskColors ) {
    colorTable.append(c.rgb());
   }
   for ( int i=maskColors.size() ; i < 256 ; ++i ) {
    colorTable.append(qRgb(i, 255 - i, 0));
   }
   indexedImage.setColorTable(colorTable);
   indexedImage.save(filename,"PNG");
  }
}

void ImageView::loadMaskImage( const QString& filename ) {
  QImage img(filename);
  if ( img.isNull() ) {
   qCDebug(logEditor) << "Error: Could not load image!";
   return;
  }
  QSize size = baseLayer()->image().size();
  if ( img.size() != size ) {
   qCDebug(logEditor) << "Error: Size mismatch could not load file!";
   return;
  }
  if ( m_maskLayer != nullptr ) {
    QMessageBox::StandardButton reply;
    reply = QMessageBox::question(this, "Please confirm operation", 
                     "A label mask already exists. Are you sure you want to load a new one? The old data will be lost?",
                     QMessageBox::Yes | QMessageBox::No);
    if ( reply == QMessageBox::No ) {
      return;
    }
  } else {
   m_maskLayer = new MaskLayer(size);
   m_maskItem = new MaskLayerItem(m_maskLayer);
   m_maskItem->setZValue(1000);
   m_maskItem->setOpacityFactor(0.4);
   scene()->addItem(m_maskItem);
  }
  if ( img.format() != QImage::Format_Indexed8 ) {
   m_maskLayer->setImage(img.convertToFormat(QImage::Format_Grayscale8));
  } else {
   QImage grayImg(img.size(),QImage::Format_Grayscale8);
   for ( int y = 0; y < img.height(); ++y ) {
    const uchar* srcLine = img.scanLine(y);
    uchar* dstLine = grayImg.scanLine(y);
    memcpy(dstLine, srcLine, img.width());
   }
   m_maskLayer->setImage(grayImg);
  }
  viewport()->update();
}

void ImageView::setMaskTool( MaskTool t ) { 
 m_maskTool = t == m_maskTool ? MaskTool::None : t;
 if ( m_maskTool != MaskTool::None && m_maskLayer == nullptr ) {
    createMaskLayer(baseLayer()->image().size());
 }
}

void ImageView::setMaskCutTool( const QString &maskLabelName, MaskCutTool t ) { 
 m_maskCutTool = t == m_maskCutTool ? MaskCutTool::Ignore : t;
 m_maskLabelTypeNames[maskLabelName] = t;
}

int ImageView::getMaskCutToolType( const QString& name )
{
  if ( m_maskLabelTypeNames.contains(name) ) {
    return m_maskLabelTypeNames[name];
  }
  m_maskLabelTypeNames[name] = MaskCutTool::Ignore;
  return 0;
}

// ------------------------ Layer tools -------------------------------------
void ImageView::centerOnLayer( Layer* layer ) {
 if ( !layer || !layer->m_item ) return;
 centerOn(layer->m_item); // QGraphicsView::centerOn
}

LayerItem* ImageView::currentLayer() const {
  auto items = m_scene->items(Qt::DescendingOrder);
  for ( QGraphicsItem* item : items ) {
    if ( auto* layer = dynamic_cast<LayerItem*>(item) )
      return layer;
  }
  return nullptr;
}

// ------------------------ Colortable tools -------------------------------------
void ImageView::setColorTable( const QVector<QRgb> &lut )
{
  qCDebug(logEditor) << "ImageView::setColorTable(): Processing...";
  LayerItem* layer = currentLayer();
  if ( !layer ) return;
  m_undoStack->push(new InvertLayerCommand(layer,lut));
}

void ImageView::enablePipette( bool enabled ) {
    m_pipette = enabled;
    setCursor(enabled ? Qt::CrossCursor : Qt::ArrowCursor);
}

// ------------------------ ------------------------ ------------------------
// ------------------------       Event tools        ------------------------
// ------------------------ ------------------------ ------------------------
void ImageView::keyPressEvent( QKeyEvent* event )
{
  qCDebug(logEditor) << "ImageView::keyPressEvent(): key =" << event->key();
  {
    MainWindow *mainWindow = dynamic_cast<MainWindow*>(m_parent);
    if ( mainWindow != nullptr ) {
      MainWindow::MainOperationMode opMode = mainWindow->getOperationMode();
      if ( opMode == MainWindow::MainOperationMode::Polygon ) {
        if ( m_polygonEnabled && ( event->key() == Qt::Key_Return || event->key() == Qt::Key_Escape ) ) {
         setPolygonEnabled(false);
         return;
        } else if ( event->modifiers() & Qt::ControlModifier ) {
           LayerItem::OperationMode polygonTransformMode = LayerItem::OperationMode::None;
           if ( event->key() == Qt::Key_A ) {
              polygonTransformMode = LayerItem::OperationMode::AddPoint;
           } else if ( event->key() == Qt::Key_D ) {
              polygonTransformMode = LayerItem::OperationMode::DeletePoint;
           } else if ( event->key() == Qt::Key_M ) {
              polygonTransformMode = LayerItem::OperationMode::MovePoint;
           } else if ( event->key() == Qt::Key_R ) {
              polygonTransformMode = LayerItem::OperationMode::ReducePolygon;
           } else if ( event->key() == Qt::Key_S ) {
              polygonTransformMode = LayerItem::OperationMode::SmoothPolygon;
           } else if ( event->key() == Qt::Key_T ) {
              polygonTransformMode = LayerItem::OperationMode::TranslatePolygon;
           } else {
              QGraphicsView::keyPressEvent(event);
              return;
           }
           mainWindow->setPolygonOperationMode(polygonTransformMode);
           setPolygonOperationMode(polygonTransformMode);
        }
      } else if ( opMode == MainWindow::MainOperationMode::ImageLayer ) {
        if ( event->modifiers() & Qt::ControlModifier ) {
          LayerItem::OperationMode transformMode = LayerItem::OperationMode::None;
          if ( event->key() == Qt::Key_T ) {
             transformMode = LayerItem::OperationMode::Translate;  
          } else if ( event->key() == Qt::Key_S ) {
             transformMode = LayerItem::OperationMode::Scale;
          } else if ( event->key() == Qt::Key_R ) {
             transformMode = LayerItem::OperationMode::Rotate;
          } else if ( event->key() == Qt::Key_V ) {
             transformMode = LayerItem::OperationMode::Flip; 
          } else if ( event->key() == Qt::Key_F ) {
             transformMode = LayerItem::OperationMode::Flop; 
          } else if ( event->key() == Qt::Key_W ) {
             transformMode = LayerItem::OperationMode::CageWarp; 
          } else if ( event->key() == Qt::Key_P ) {
             transformMode = LayerItem::OperationMode::Perspective; 
          } else {
             QGraphicsView::keyPressEvent(event);
             return;
          }
          mainWindow->setLayerOperationMode(transformMode);
          setLayerOperationMode(transformMode);
        } 
      }
    }
    QGraphicsView::keyPressEvent(event);
  }
}

void ImageView::mousePressEvent( QMouseEvent* event )
{
  MainWindow *mainWindow = dynamic_cast<MainWindow*>(m_parent);
  if ( mainWindow == nullptr ) return;  
  qCDebug(logEditor) << "ImageView::mousePressEvent(): operationMode = " << mainWindow->getOperationMode() << ", polygonEnabled =" << m_polygonEnabled;
  {
    if ( !scene() )
        return;     
        
    if ( event->button() != Qt::LeftButton && event->button() != Qt::RightButton ) {
        QGraphicsView::mousePressEvent(event);
        return;
    }

    QPointF scenePos = mapToScene(event->pos());
    m_cursorPos = scenePos;

    // --- Shift-Pan: Bild verschieben ---
    if ( event->modifiers() & Qt::ShiftModifier ) {
        m_panning = true;
        m_lastMousePos = event->pos();
        setCursor(Qt::ClosedHandCursor);
        return; // kein weiteres Event behandeln
    }

    // --- Pipette ---
    if ( m_pipette ) {
        auto itemsUnderCursor = scene()->items(scenePos);
        for ( auto* item : itemsUnderCursor ) {
            auto* layer = dynamic_cast<LayerItem*>(item);
            if ( !layer ) continue;
            QPoint localPos = layer->mapFromScene(scenePos).toPoint();
            if ( !layer->image().rect().contains(localPos) ) continue;
            QColor color = layer->image().pixelColor(localPos);
            setBrushColor(color);
            emit pickedColorChanged(color);
            return;
        }
    }
    
    // --- Layer ---
    if ( mainWindow->getOperationMode() == MainWindow::MainOperationMode::ImageLayer ) {
      // --- Check whether a layer has been clicked ---
      LayerItem* clickedItem = nullptr;
      auto itemsUnderCursor = m_scene->items(scenePos);
      for ( auto* item : itemsUnderCursor ) {
        auto* layer = dynamic_cast<LayerItem*>(item);
        if ( layer ) {
            clickedItem = layer;
            break;
        }
      }
      if ( clickedItem ) {
        clickedItem->setOperationMode(m_layerOperationMode);
        if ( clickedItem->isSelected() == true ) {
          clickedItem->setSelected(false);  // NO effect. Why?
        } else {
          for ( auto* item : m_scene->items() ) {
            auto* layer = dynamic_cast<LayerItem*>(item);
            if ( layer && layer != clickedItem )
                layer->setSelected(false);
          }
          clickedItem->setSelected(true);
        }
        if ( clickedItem->isCageWarp() && clickedItem->cageMesh().isActive() ) {
           m_activeLayer = clickedItem;
           m_selectedLayer = clickedItem;
           m_cageBefore = m_activeLayer->cageMesh().points();
        }
      }
    }

    // --- Painting ---
    if ( mainWindow->getOperationMode() == MainWindow::MainOperationMode::Paint ) {
      if ( m_paintToolEnabled ) {
        auto itemsUnderCursor = scene()->items(scenePos);
        for ( auto* item : itemsUnderCursor ) {
            auto* layer = dynamic_cast<LayerItem*>(item);
            if ( !layer ) continue;
            QPoint localPos = layer->mapFromScene(scenePos).toPoint();
            if ( !layer->image().rect().contains(localPos) ) continue;
            m_painting = true;
            m_paintLayer = layer;
            m_currentStroke.clear();
            m_currentStroke << localPos;
            //ALT: m_undoStack->push(new PaintStrokeCommand(layer, localPos, m_brushColor, m_brushRadius, m_brushHardness));
            layer->updateOriginalImage();
            layer->paintStrokeSegment(localPos,localPos,m_brushColor,m_brushRadius,m_brushHardness);
            viewport()->update();
            break;
        }
      }
    }
    
    // --- Mask Painting ---
    if ( mainWindow->getOperationMode() == MainWindow::MainOperationMode::Mask ) {
      if ( m_maskTool != MaskTool::None && (event->button() == Qt::LeftButton || event->button() == Qt::RightButton) ) {
        // NEW: m_maskStrokeActive = true;
        // NEW: m_currentMaskStroke.clear();
        m_maskPainting = true;
        m_maskStrokePoints.clear();
        m_maskStrokePoints << mapToScene(event->pos()).toPoint();
        return;
      }
    }
    // --- Free selection event ---
    if ( mainWindow->getOperationMode() == MainWindow::MainOperationMode::FreeSelection ) {
      if ( m_lassoEnabled ) {
        m_lassoPolygon.clear();
        m_lassoPolygon << scenePos.toPoint();
        if ( !m_lassoPreview ) {
            m_lassoPreview = scene()->addPolygon(QPolygonF(m_lassoPolygon));
            m_lassoPreview->setPen(QPen(EditorStyle::instance().lassoColor(), EditorStyle::instance().lassoWidth()));
            // QColor lassoColor = EditorStyle::instance().lassoColor();
            // m_lassoPreview->setPen(QPen(Qt::yellow, 0));
        } else {
            m_lassoPreview->setPolygon(QPolygonF(m_lassoPolygon));
        }
        QRectF br = QPolygonF(m_lassoPolygon).boundingRect();
        if ( !m_lassoBoundingBox ) {
            QPen pen(Qt::green);
            pen.setWidth(0); // kosmetisch, 1px auf Bildschirm
            pen.setStyle(Qt::SolidLine);
            m_lassoBoundingBox = m_scene->addRect(br, pen);
            m_lassoBoundingBox->setZValue(1000);
        } else {
            m_lassoBoundingBox->setRect(br);
        }
        return;
      }
    }
    
    // --- polygon events ---
    if ( mainWindow->getOperationMode() == MainWindow::MainOperationMode::Polygon ) {
      // --- Add a new point to polygon ---
      if ( m_polygonEnabled && event->button() == Qt::LeftButton ) {
         if ( m_activePolygon != nullptr ) {
           m_activePolygon->addPoint(scenePos);
           return;
         }
      }
    }

    QGraphicsView::mousePressEvent(event);
  
  }
}

void ImageView::mouseDoubleClickEvent( QMouseEvent* event )
{
  qCDebug(logEditor) << "ImageView::mouseDoubleClickEvent(): Processing...";
  {
    MainWindow *mainWindow = dynamic_cast<MainWindow*>(m_parent);
    if ( mainWindow != nullptr && mainWindow->getOperationMode() == MainWindow::MainOperationMode::Polygon ) {
       // check whether a polygon has been double clicked
       QPointF scenePos = mapToScene(event->pos());
       EditablePolygonItem* clickedItem = nullptr;
       auto itemsUnderCursor = m_scene->items(scenePos);
       for ( auto* item : itemsUnderCursor ) {
         auto* editablePolygon = dynamic_cast<EditablePolygonItem*>(item);  //  EditablePolygonItem
         if ( editablePolygon != nullptr  && editablePolygon->polygon() != nullptr ) {
           if ( editablePolygon->hitTestPolygon(scenePos) > 0 ) {
            editablePolygon->polygon()->setSelected(!editablePolygon->polygon()->isSelected());
            if ( editablePolygon->polygon()->isSelected() ) mainWindow->setActivePolygon(editablePolygon->polygon()->name());
            clickedItem = editablePolygon;
            break;
           }
         }
       }
       if ( clickedItem != nullptr ) {
         for ( auto* item : m_scene->items() ) {
           auto* editablePolygon = dynamic_cast<EditablePolygonItem*>(item);
           if ( editablePolygon && editablePolygon != clickedItem )
               editablePolygon->polygon()->setSelected(false);
         }
       }
    }
    QGraphicsView::mouseDoubleClickEvent(event);
  }
}

void ImageView::mouseMoveEvent( QMouseEvent* event )
{
    //  qCDebug(logEditor) << "ImageView::mouseMoveEvent(): m_painting=" << m_maskPainting << ", m_paintToolEnabled " << m_paintToolEnabled;
    if ( !scene() )
        return;
        
    QPointF scenePos = mapToScene(event->pos());
    m_cursorPos = scenePos;

    emit cursorPositionChanged(int(scenePos.x()), int(scenePos.y()));
    emit scaleChanged(transform().m11());

    // --- Cursor-Farbe unter Maus ---
    bool colorFound = false;
    auto itemsUnderCursor = scene()->items(scenePos);
    for ( auto* item : itemsUnderCursor ) {
        auto* layer = dynamic_cast<LayerItem*>(item);
        if ( !layer ) continue;
        QPoint layerPos = layer->mapFromScene(scenePos).toPoint();
        if ( layer->image().rect().contains(layerPos) ) {
            QColor c = layer->image().pixelColor(layerPos);
            emit cursorColorChanged(c);
            colorFound = true;
            break;
        }
    }
    if ( !colorFound )
        emit cursorColorChanged(Qt::transparent);

    // --- Shift-Pan: nur wenn Shift gedrückt & linke Maustaste ---
    // std::cout << "shift-pan: processing: shift=" << (event->modifiers() & Qt::ShiftModifier) << "..." << std::endl;
    if ( (event->modifiers() & Qt::ShiftModifier) && (event->buttons() & Qt::LeftButton) ) {
        QPoint delta = event->pos() - m_lastMousePos;
        horizontalScrollBar()->setValue(horizontalScrollBar()->value() - delta.x());
        verticalScrollBar()->setValue(verticalScrollBar()->value() - delta.y());
        m_lastMousePos = event->pos();
        return;
    }

    // --- Painting ---
    if ( m_painting && m_paintToolEnabled ) {
        auto itemsUnderCursor = m_scene->items(scenePos);
        for ( auto* item : itemsUnderCursor ) {
          auto* layer = dynamic_cast<LayerItem*>(item);
          if ( !layer || !layer->isVisible() ) continue;
          QPoint localPos = layer->mapFromScene(scenePos).toPoint();
          if ( !layer->image().rect().contains(localPos) ) continue;
          // PaintCommand jetzt auf Layer
          if ( m_currentStroke.isEmpty() || m_currentStroke.last() != localPos ) {
            m_currentStroke << localPos;
            layer->paintStrokeSegment(m_currentStroke[m_currentStroke.size()-2],localPos,m_brushColor,m_brushRadius,m_brushHardness);
            viewport()->update();
          }
          //ALT: m_undoStack->push(new PaintStrokeCommand(layer, localPos, m_brushColor, m_brushRadius, m_brushHardness));
          //ALT: viewport()->update();
          break; // nur oberstes Layer malen
        }
        return;
    }
    
    // --- Mask Painting ---
    if ( m_maskPainting && (event->buttons() & Qt::LeftButton || event->buttons() & Qt::RightButton) ) {
     if ( m_maskLayer ) { // NEW: && m_maskStrokeActive
        // m_maskStrokePoints << mapToScene(event->pos()).toPoint();
        // m_maskItem->update();
        bool isRightButton = event->buttons() & Qt::RightButton ? true : false;
        int x = int(scenePos.x());
        int y = int(scenePos.y());
        if( !(x < 0 || y < 0 || x >= m_maskLayer->width() || y >= m_maskLayer->height()) ) {
         int r = m_maskBrushRadius;
         int rr = r*r;
         uchar newValue = isRightButton ? (m_maskTool == MaskTool::MaskErase ? m_currentMaskLabel : 0) : (m_maskTool == MaskTool::MaskErase ? 0 : m_currentMaskLabel);
         for( int dy = -r; dy <= r; ++dy ) {
          for( int dx = -r; dx <= r; ++dx ) {
            int px = x+dx;
            int py = y+dy;
            if( dx*dx + dy*dy > rr ) continue; // Kreis
            // NEW: uchar oldValue = m_maskLayer->pixel(x,y);
            // NEW: if ( oldValue == newValue ) continue;
            // NEW: m_currentMaskStroke.push_back({x,y,oldValue,newValue});
            m_maskLayer->setPixel(px, py, newValue);
          }
         }
         m_maskItem->maskUpdated();
         return;
        }
     }
    }

    // --- Lasso-Zeichnung ---
    if ( m_lassoEnabled && (event->buttons() & Qt::LeftButton) ) {
        m_lassoPolygon << scenePos.toPoint();
        if ( !m_lassoPreview ) {
            m_lassoPreview = scene()->addPolygon(QPolygonF(m_lassoPolygon));
        } else {
            m_lassoPreview->setPolygon(QPolygonF(m_lassoPolygon));
        }
        QRectF br = QPolygonF(m_lassoPolygon).boundingRect();
        if ( !m_lassoBoundingBox ) {
            QPen pen(Qt::green);
            pen.setWidth(0); // kosmetisch
            pen.setStyle(Qt::SolidLine);
            m_lassoBoundingBox = m_scene->addRect(br, pen);
            m_lassoBoundingBox->setZValue(1000);
        } else {
            m_lassoBoundingBox->setRect(br);
        }
        return;
    }

    // --- Freie Auswahl (Path) ---
    if ( m_selecting ) {
        m_selectionPath.lineTo(scenePos);
        viewport()->update();
        return;
    }
    
    // Letzte Mausposition merken
    m_lastMousePos = event->pos();
    viewport()->update();
    
    // >>>
    QGraphicsView::mouseMoveEvent(event);
}

void ImageView::mouseReleaseEvent( QMouseEvent* event )
{
  qCDebug(logEditor) << "ImageView::mouseReleaseEvent(): Processing...";
  {
    if ( !scene() )
        return;
    // --- Cage warp beenden ---
    if ( event->button() == Qt::LeftButton && m_selectedCageLayer ) {
        QVector<QPointF> cageAfter = m_selectedCageLayer->cageMesh().points();
        QVector<QPointF> cageBefore = m_selectedCageLayer->cageMesh().originalPoints();    
        std::cout << "ImageView::mouseReleaseEvent(): layer=" << m_selectedCageLayer->name().toStdString() << ": cageAfter=" << cageAfter.size() << ", cageBefore=" << m_cageBefore.size() << std::endl;
        // Prüfen, ob Cage verändert wurde
        if ( cageAfter != m_cageBefore ) {
         std::cout << " Cage has been modified " << std::endl;
         if ( m_cageWarpCommand == nullptr ) {
          std::cout << "Creating new layer undo/redo instance..." << std::endl;
          int rows = m_selectedCageLayer->cageMesh().rows();
          int columns = m_selectedCageLayer->cageMesh().cols();
          m_cageWarpCommand = new CageWarpCommand(m_selectedLayer, cageBefore, cageAfter, m_selectedLayer->boundingRect(), rows, columns);
          m_undoStack->push(m_cageWarpCommand);
         } else {
          m_cageWarpCommand->pushNewWarpStep(cageAfter);
         } 
         m_selectedCageLayer->applyTriangleWarp();
        }
        m_activeLayer = nullptr;
        m_cageBefore.clear();
    }
    // --- Lasso beenden ---
    if ( m_lassoEnabled && event->button() == Qt::LeftButton ) {
        if ( m_lassoPolygon.size() > 2 ) {
            createLassoLayer(); // erzeugt neues Layer aus Polygon
        }
        // Polygon entfernen
        if ( m_lassoPreview ) {
            scene()->removeItem(m_lassoPreview);
            delete m_lassoPreview;
            m_lassoPreview = nullptr;
        }
        // Bounding Box entfernen
        if ( m_lassoBoundingBox ) {
            scene()->removeItem(m_lassoBoundingBox);
            delete m_lassoBoundingBox;
            m_lassoBoundingBox = nullptr;
        }
        m_lassoPolygon.clear();
        return; // fertig, kein weiteres Event
    }
    // --- Painting beenden ---
    if ( m_painting && event->button() == Qt::LeftButton ) {
       std::cout << "stop painting of " <<  m_currentStroke.size() << " points..." << std::endl;
        if ( m_currentStroke.size() > 1 ) {
            m_undoStack->push(new PaintStrokeCommand(m_paintLayer,m_currentStroke,m_brushColor,
                                    m_brushRadius,m_brushHardness));
        }
        m_paintLayer = nullptr;
        m_currentStroke.clear();
        m_painting = false;
    }
    // --- Misc ---
    if ( event->button() == Qt::LeftButton ) {
        m_painting = false;
        // Shift-Pan beenden
        if ( m_panning ) {
            m_panning = false;
            setCursor(Qt::ArrowCursor);
        }
        // Freie Auswahl beenden
        if ( m_selecting ) {
            m_selecting = false;
            m_selectionPath.closeSubpath();
            viewport()->update();
        }
    }
    // --- Mask Painting beenden ---
    if ( m_maskPainting && (event->button() == Qt::LeftButton || event->button() == Qt::RightButton) ) {
        m_maskStrokeActive = false;
        m_maskPainting = false;
        
        // NEW:
        if ( !m_currentMaskStroke.isEmpty() ) {
          m_undoStack->push(
            new MaskPaintCommand(
                m_maskLayer,
                std::move(m_currentMaskStroke)
            )
          );
        }
       /* **** crash ****
        quint8 label = m_maskEraser ? 0 : m_currentMaskLabel;
        m_undoStack->push(
            new MaskStrokeCommand(
                m_maskLayer,
                m_maskStrokePoints,
                label,
                m_brushRadius
            )
        );
        */
        
        return;
    } 
    QGraphicsView::mouseReleaseEvent(event);
  }
}

// IMPORTANT: Security-Check: Prevents extrem small zoom values (potential crash source!)
//    If newScale will be too small (e.g. < 0.001), the BSP-Tree crashed.
void ImageView::wheelEvent( QWheelEvent* event )
{
  // qCDebug(logEditor) << "ImageView::wheelEvent(): currentScale =" << transform().m11();
  {
    if ( !scene() || scene()->items().isEmpty() ) {
      event->ignore();
      return;
    }
    constexpr qreal zoomFactor = 1.15;
    qreal factor = (event->angleDelta().y() > 0) ? zoomFactor : 1.0 / zoomFactor;
    qreal currentScale = transform().m11();
    qreal newScale = currentScale * factor;
    if ( newScale < 0.01 || newScale > 100.0 ) {
      event->ignore();
      return;
    }
    emit scaleChanged(transform().m11());
    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    scale(factor, factor);
    event->accept();
  }
}

// --------------------------------- drawing ---------------------------------
void ImageView::drawForeground( QPainter* painter, const QRectF& )
{

    if ( !scene() )
        return;
        
    painter->save();

    // ----------------------------
    // LASSO
    // ----------------------------
    if ( m_selecting ) {
        QPen pen(Qt::white, 0, Qt::DashLine);
        painter->setPen(pen);
        painter->setBrush(Qt::NoBrush);
        painter->drawPath(m_selectionPath);
    }

    // ----------------------------
    // CROSSHAIR
    // ----------------------------
    if ( m_crosshairVisible ) {
        QPen pen(Qt::red, 0);   // 0 = kosmetisch
        painter->setPen(pen);
        QRectF r = scene()->sceneRect();
        painter->drawLine(QPointF(m_cursorPos.x(), r.top()),
                          QPointF(m_cursorPos.x(), r.bottom()));
        painter->drawLine(QPointF(r.left(), m_cursorPos.y()),
                          QPointF(r.right(), m_cursorPos.y()));
    }

    // ----------------------------
    // BRUSH-VORSCHAU
    // ----------------------------
    if ( m_showBrushPreview && m_paintToolEnabled ) {
        QPen outerPen(Qt::green, 0);
        outerPen.setCosmetic(true);
        painter->setPen(outerPen);
        painter->setBrush(Qt::NoBrush);
        painter->drawEllipse(m_cursorPos, m_brushRadius, m_brushRadius);
        // Hardness-Ring
        if ( m_brushHardness > 0.0 ) {
            qreal innerRadius = m_brushRadius * m_brushHardness;
            QPen innerPen(Qt::green, 0, Qt::DashLine);
            innerPen.setCosmetic(true);
            painter->setPen(innerPen);
            painter->drawEllipse(m_cursorPos, innerRadius, innerRadius);
        }
    }
    
    painter->restore();
    
}

// ---------------------------- Selectior -----------------------------
void ImageView::clearSelection()
{
    m_selectionPath = QPainterPath();
    viewport()->update();
}

void ImageView::cutSelection()
{
    if ( m_selectionPath.isEmpty() )
        return;

    auto* layer = dynamic_cast<LayerItem*>(m_scene->focusItem());
    if ( !layer ) return;

    QImage& srcImage = layer->image();
    if ( srcImage.isNull() ) return;

    QRectF sceneRect = m_selectionPath.boundingRect();
    QRect imageRect = layer->mapFromScene(sceneRect).boundingRect().toRect()
                          .intersected(srcImage.rect());
    if ( imageRect.isEmpty() ) return;

    QImage cutImage(imageRect.size(), QImage::Format_ARGB32_Premultiplied);
    cutImage.fill(Qt::transparent);

    {
        QPainter p(&cutImage);
        p.setRenderHint(QPainter::Antialiasing);
        QPainterPath localPath = layer->mapFromScene(m_selectionPath);
        p.translate(-imageRect.topLeft());
        p.setClipPath(localPath);
        p.drawImage(0, 0, srcImage,
                    imageRect.x(), imageRect.y(),
                    imageRect.width(), imageRect.height());
    }

    {
        QPainter p(&srcImage);
        p.setCompositionMode(QPainter::CompositionMode_Clear);
        QPainterPath localPath = layer->mapFromScene(m_selectionPath);
        p.drawPath(localPath);
    }

    layer->updatePixmap();

    auto* newLayer = new LayerItem(QPixmap::fromImage(cutImage));
    newLayer->setPos(layer->mapToScene(imageRect.topLeft()));
    newLayer->setFlags(QGraphicsItem::ItemIsMovable |
                       QGraphicsItem::ItemIsSelectable |
                       QGraphicsItem::ItemIsFocusable);

    m_scene->addItem(newLayer);
    newLayer->setSelected(true);
    newLayer->setFocus();

    clearSelection();
}

// ---------------------------- Layer methods -----------------------------
LayerItem* ImageView::getSelectedItem()
{
   for ( auto* item : m_scene->items() ) {
        auto* layer = dynamic_cast<LayerItem*>(item);
        if ( layer && layer->isSelected() ) {
          return layer;
        }
    }
    return nullptr;
}

void ImageView::setPolygonOperationMode( LayerItem::OperationMode mode )
{
  qCDebug(logEditor) << "ImageView::setPolygonOperationMode(): mode =" << mode << ", m_polygonEnabled =" << m_polygonEnabled;
  {
    m_polygonOperationMode = mode; 
  }
}

void ImageView::setLayerOperationMode( LayerItem::OperationMode mode )
{
  qCDebug(logEditor) << "ImageView::setLayerOperationMode(): mode =" << mode << ", m_polygonEnabled =" << m_polygonEnabled;
  {
    m_layerOperationMode = mode; 
    if ( m_polygonEnabled ) {
      setPolygonEnabled(false);
    }
    LayerItem *layer = getSelectedItem();
    if ( layer != nullptr ) {
      layer->setOperationMode(mode);
    } else {
     layer = baseLayer();
     if ( layer != nullptr ) {
       layer->setOperationMode(mode);
     }
    }
  /*
	for ( auto* item : m_scene->items() ) {
        auto* layer = dynamic_cast<LayerItem*>(item);
        if ( layer && layer->isSelected() ) {
          layer->setTransformMode(mode);
        }
    }
   */
  }
}

void ImageView::setNumberOfCageControlPoints( int nControlPoints ) 
{
  // qCDebug(logEditor) << "ImageView::setNumberOfCageControlPoints(): nControlPoints=" << nControlPoints;
  {
    for ( auto* item : m_scene->items(Qt::DescendingOrder) ) {
      auto* layer = dynamic_cast<LayerItem*>(item);
      if ( layer && layer->getType() != LayerItem::MainImage ) {
        if ( layer->hasActiveCage() ) {
         layer->setNumberOfActiveCagePoints(nControlPoints);
         return;
        }
      }
    }
  }
}

void ImageView::setCageWarpRelaxationSteps( int nRelaxationSteps )
{
  // qCDebug(logEditor) << "ImageView::setCageWarpRelaxationSteps(): nRelaxationSteps=" << nRelaxationSteps;
  {
    for ( auto* item : m_scene->items(Qt::DescendingOrder) ) {
      auto* layer = dynamic_cast<LayerItem*>(item);
      if ( layer && layer->getType() != LayerItem::MainImage ) {
        if ( layer->hasActiveCage() ) {
         layer->setCageWarpRelaxationSteps(nRelaxationSteps);
         return;
        }
      }
    }
  }
}

LayerItem* ImageView::baseLayer()
{
    for ( auto* item : m_scene->items(Qt::DescendingOrder) ) {
      auto* layer = dynamic_cast<LayerItem*>(item);
      if ( layer && layer->getType() == LayerItem::MainImage ) {
        return layer;
      }  
    }
    return nullptr;
}

void ImageView::clearLayers()
{
}

void ImageView::createLassoLayer()
{
  createNewLayer(m_lassoPolygon,"Lasso Layer");
  emit lassoLayerAdded();
}

LassoCutCommand* ImageView::createNewLayer( const QPolygonF& polygon, const QString &name )
{
  qCDebug(logEditor) << "ImageView::createNewLayer(): name=" << name << ",  polygon_size=" << polygon.size() << ", operationMode= " << m_layerOperationMode;
  {
    // --- switch to layer operation mode ---
    MainWindow *mainWindow = dynamic_cast<MainWindow*>(m_parent);
    if ( mainWindow != nullptr ) {
      mainWindow->setMainOperationMode(MainWindow::MainOperationMode::ImageLayer);
    }
    // --- get main image layer ---
    LayerItem* base = baseLayer();
    if ( !base || polygon.size() < 3 )
        return nullptr;
    QImage& src = base->image();
    // --- prepare ---
    QColor backgroundColor = Config::isWhiteBackgroundImage ? Qt::white : Qt::black;
    QPolygonF polyF = QPolygonF(polygon.begin(), polygon.end());
    QRectF boundsF = polyF.boundingRect();
    QRect bounds = boundsF.toAlignedRect(); // ganzzahlig
    QImage backup = src.copy(bounds);
    // --- create mask ---
    QImage mask(bounds.size(), QImage::Format_Alpha8);
    mask.fill(0);
    QPainter pm(&mask);
    pm.setRenderHint(QPainter::Antialiasing);
    pm.setBrush(backgroundColor); // Qt::white);
    pm.setPen(Qt::NoPen);
    // Polygon relativ zur Bounding-Box verschieben
    QPolygonF relativePoly = polyF;
    for ( int i=0; i<relativePoly.size(); ++i )
      relativePoly[i] -= bounds.topLeft();
    pm.drawPolygon(relativePoly);
    pm.end();
    // --- lasso fear ---
    if ( m_lassoFeatherRadius > 0 ) {
      mask = QImageUtils::blurAlphaMask(mask,m_lassoFeatherRadius);
    }
    // --- cut layer ---
    QImage cut(bounds.size(), QImage::Format_ARGB32_Premultiplied);
    cut.fill(Qt::transparent);
    if ( m_maskCutTool == MaskCutTool::Mask && m_maskLayer != nullptr ) {
     for ( int y=0; y<bounds.height(); ++y ) {
      uchar* m = mask.scanLine(y);
      unsigned int ypos = bounds.top() + y;
      for ( int x=0; x<bounds.width(); ++x ) {
       unsigned int xpos = bounds.left() + x;
       QPoint imgPos(xpos, ypos);
       QColor c = src.pixelColor(imgPos);
       if ( c != backgroundColor && m[x] > 0 && m_maskLayer->pixel(xpos,ypos) == 0 ) {
         c.setAlpha(m[x]); 
         cut.setPixelColor(x,y,c);
         if ( m[x] == 255 )
            src.setPixelColor(imgPos, backgroundColor);
       }
      }
     }
    } else if ( m_maskCutTool == MaskCutTool::OnlyMask && m_maskLayer != nullptr ) {
     for ( int y=0; y<bounds.height(); ++y ) {
      uchar* m = mask.scanLine(y);
      unsigned int ypos = bounds.top() + y;
      for ( int x=0; x<bounds.width(); ++x ) {
       unsigned int xpos = bounds.left() + x;
       QPoint imgPos(xpos, ypos);
       QColor c = src.pixelColor(imgPos);
       if ( c != backgroundColor && m[x] > 0 && m_maskLayer->pixel(xpos,ypos) != 0 ) {
         c.setAlpha(m[x]); 
         cut.setPixelColor(x,y,c);
         if ( m[x] == 255 )
            src.setPixelColor(imgPos, backgroundColor);
       }
      }
     }
    } else {
     for ( int y=0; y<bounds.height(); ++y ) {
      uchar* m = mask.scanLine(y);
      unsigned int ypos = bounds.top() + y;
      for ( int x=0; x<bounds.width(); ++x ) {
        QPoint imgPos(bounds.left() + x, ypos);
        QColor c = src.pixelColor(imgPos);
        if ( c != backgroundColor && m[x] > 0 ) {
         c.setAlpha(m[x]); 
         cut.setPixelColor(x,y,c);
         // if ( m[x] > 0 )
         //  src.setPixelColor(imgPos, backgroundColor);
        }
      }
     }
     
    }
    // --- Neues LayerItem ---
    int nidx = m_layers.size()+1;
    Layer* layer = new Layer(nidx,cut);
    layer->m_name = QString("%1 %2").arg(name).arg(nidx);
    LayerItem* newLayer = new LayerItem(cut);
    newLayer->setParent(m_parent);
    newLayer->setIndex(nidx);
    newLayer->setLayer(layer);
    newLayer->setUndoStack(m_undoStack);
    LassoCutCommand* cmd = new LassoCutCommand(base, newLayer, bounds, cut, nidx, name);
    m_undoStack->push(cmd);
    newLayer->setPos(base->mapToScene(bounds.topLeft()));
    newLayer->setZValue(base->zValue()+1);
    newLayer->setSelected(true);
    // newLayer->setShowGreenBorder(true); // falls vorhanden
    // ITEM ALREADY IN SCENE: m_scene->addItem(newLayer);
    base->updatePixmap();
    layer->m_item = newLayer;
    m_layers.push_back(layer);
    return cmd;
  }
}

// ---------------------------- --------------- -----------------------------
// ---------------------------- Polygon methods -----------------------------
// ---------------------------- --------------- -----------------------------
EditablePolygonCommand* ImageView::getPolygonUndoCommand( const QString& name )
{
  if ( name == "" ) {
   const QUndoCommand* cmd = m_undoStack->command(m_undoStack->index() - 1);
   EditablePolygonCommand* polyCmd = const_cast<EditablePolygonCommand*>(
        dynamic_cast<const EditablePolygonCommand*>(cmd)
   );
   return polyCmd;
  }
  for ( int i = m_undoStack->count() - 1; i >= 0; --i ) {
    const QUndoCommand* cmd = m_undoStack->command(i);
    if ( cmd->text() == name ) {
       return const_cast<EditablePolygonCommand*>(
           dynamic_cast<const EditablePolygonCommand*>(cmd)
       );
    }
  }
  return nullptr;
}

void ImageView::undoPolygonOperation()
{
    EditablePolygonCommand* polyCmd = ImageView::getPolygonUndoCommand(QString("Editable Polygon %1").arg(m_polygonIndex));
    if ( polyCmd == nullptr )
      return;
    EditablePolygon *polygon = polyCmd->model();
    if ( polygon != nullptr ) {
     polygon->undoStack()->undo();
    }
}

void ImageView::redoPolygonOperation()
{
    EditablePolygonCommand* polyCmd = ImageView::getPolygonUndoCommand(QString("Editable Polygon %1").arg(m_polygonIndex));
    if ( polyCmd == nullptr )
      return;
    EditablePolygon *polygon = polyCmd->model();
    if ( polygon != nullptr ) {
     polygon->undoStack()->redo();
    }
}

void ImageView::createPolygonLayer()
{
  qCDebug(logEditor) << "ImageView::createPolygonLayer(): Processing...";
  {
    if ( m_activePolygon != nullptr ) {
      LayerItem *layer = baseLayer();
      if ( layer != nullptr ) {
        finishPolygonDrawing(layer);
      }
    }
    MainWindow *mainWindow = dynamic_cast<MainWindow*>(m_parent);
    if ( mainWindow != nullptr ) {
      mainWindow->setMainOperationMode(MainWindow::MainOperationMode::ImageLayer);
    }
    EditablePolygonCommand* polyCmd = ImageView::getPolygonUndoCommand();
    if ( polyCmd == nullptr )
      return;
    EditablePolygon *editablePolygon = polyCmd->model();
    if ( editablePolygon != nullptr ) {
     int index = m_layers.size()+1;
     LassoCutCommand *layerCut = createNewLayer(editablePolygon->polygon(),QString("Polygon %1 Layer").arg(index));
     if ( layerCut != nullptr ) {
      layerCut->setController(polyCmd);
      editablePolygon->setVisible(false);
     }
     emit lassoLayerAdded();
    }
  }
}

void ImageView::finishPolygonDrawing( LayerItem* layer )
{
  qCDebug(logEditor) << "ImageView::finishPolygonDrawing(): Processing...";
  {
    if ( !m_activePolygon || m_activePolygon->pointCount() < 3 )
        return;
    MainWindow *mainWindow = dynamic_cast<MainWindow*>(m_parent);
    if ( mainWindow != nullptr ) {
      mainWindow->setMainOperationMode(MainWindow::MainOperationMode::CreatePolygon);
    }
    QPolygonF poly = m_activePolygon->polygon();
    scene()->removeItem(m_activePolygonItem);
    m_activePolygonItem->deleteLater(); // Instead of 'delete m_activePolygonItem;'
    delete m_activePolygon;
    m_activePolygonItem = nullptr;
    m_activePolygon = nullptr;
    m_undoStack->push(new EditablePolygonCommand(layer,scene(),poly,QString("Polygon %1").arg(m_editablePolygons.size())));
  }
}

void ImageView::setPolygonEnabled( bool enabled )
{ 
  qCDebug(logEditor) << "ImageView::setPolygonEnabled(): npolygons=" << m_editablePolygons.size() << ", enabled=" << enabled;
  {
   LayerItem *layer = baseLayer();
   if ( layer != nullptr ) {
    m_polygonEnabled = enabled;
    if ( m_polygonEnabled ) {
     m_activePolygon = new EditablePolygon(QString("Polygon %1").arg(1+m_editablePolygons.size()),this);
     m_editablePolygons.push_back(m_activePolygon);
     m_activePolygonItem = new EditablePolygonItem(m_activePolygon,layer);
     m_activePolygonItem->setColor(QColor(255,0,0)); // set polygon color here
    } else {
     finishPolygonDrawing(layer);
     // m_activePolygonItem = nullptr;
     // m_activePolygon = nullptr;
    }
   }
  }
}