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
#include "../layer/TransformOverlay.h"
#include "../undo/EditablePolygonCommand.h"
#include "../undo/PaintCommand.h"
#include "../undo/CageWarpCommand.h"
#include "../undo/MaskStrokeCommand.h"
#include "../undo/PaintStrokeCommand.h"
#include "../undo/InvertLayerCommand.h"
#include "../undo/DeleteLayerCommand.h"
#include "../undo/LassoCutCommand.h"
#include "../util/QImageUtils.h"
#include "../util/MaskUtils.h"

#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QFileInfo>
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
    // --- history control ---
    connect(m_undoStack, &QUndoStack::indexChanged, this, [this](int currentIndex) {
      qDebug() << "ImageView::ImageView(): lastIndex =" << m_lastIndex << ", currentIndex =" << currentIndex;
      if ( currentIndex > 0 ) {
        const QUndoCommand* justFinishedCommand = m_undoStack->command(currentIndex - 1);
        if ( justFinishedCommand != nullptr ) {
          qDebug() << "Actual command: " << justFinishedCommand->text();
          if ( currentIndex > 1 ) {
            const QUndoCommand* previousActiveCommand = m_undoStack->command(currentIndex - 2);
            if ( previousActiveCommand != nullptr ) {
              qDebug() << "Command before: " << previousActiveCommand->text();
              if ( previousActiveCommand->text().startsWith("Scale Transform") ) {
                if ( m_transformOverlay != nullptr ) {
                  m_transformOverlay->setVisible(false);
                  // cleanupCommand(previousActiveCommand);
                }
              }
            }
          }
          // >>>
          MainWindow *mainWindow = dynamic_cast<MainWindow*>(m_parent);
          QRegularExpression re("(\\d+)");
          QRegularExpressionMatch match = re.match(justFinishedCommand->text());
          if ( match.hasMatch() && mainWindow != nullptr ) {
            int layerId = match.captured(1).toInt();
            mainWindow->setSelectedLayer(QString("Layer %1").arg(layerId));
          }
          if ( justFinishedCommand->text().startsWith("Scale Transform") ) {
            qDebug() << " *** handle scale transform operation ***";
            mainWindow->setLayerOperationMode(LayerItem::OperationMode::Scale);
            if ( m_transformOverlay != nullptr ) {
              m_transformOverlay->setVisible(true);
            }
          } else if ( justFinishedCommand->text().startsWith("Delete Layer") ) {
            if ( mainWindow != nullptr ) {
              mainWindow->updateLayerList();
            }
          } else if ( justFinishedCommand->text().startsWith("Move Layer") ) {
            mainWindow->setLayerOperationMode(LayerItem::OperationMode::Translate);
          } else if ( justFinishedCommand->text().startsWith("Rotate Layer") ) {
            mainWindow->setLayerOperationMode(LayerItem::OperationMode::Rotate);
          } else if ( justFinishedCommand->text().startsWith("Mirror Vertical") ) {
            mainWindow->setLayerOperationMode(LayerItem::OperationMode::Flip);
          } else if ( justFinishedCommand->text().startsWith("Mirror Horizontal") ) {
            mainWindow->setLayerOperationMode(LayerItem::OperationMode::Flop);
          } else if ( justFinishedCommand->text().startsWith("Perspective") ) {
            mainWindow->setLayerOperationMode(LayerItem::OperationMode::Perspective);
          } else if ( justFinishedCommand->text().startsWith("Cage") ) {
            mainWindow->setLayerOperationMode(LayerItem::OperationMode::CageWarp);
          } 
          // Going-back in undo stack actions
          const QUndoCommand* previousCommand = m_undoStack->command(m_lastIndex-1);
          if ( previousCommand != nullptr ) {
           if ( previousCommand->text().startsWith("Delete Layer") ) {
            MainWindow *mainWindow = dynamic_cast<MainWindow*>(m_parent);
            if ( mainWindow != nullptr ) {
              mainWindow->updateLayerList();
            }
           }
          }
        }
      }
      m_lastIndex = currentIndex;
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
  qDebug() << "ImageView::rebuildUndoStack(): Processing...";
  {
    // sort and put a given index (=1) to the end
    int targetIdToEnd = 1;
    struct CommandEntry {
      const QUndoCommand* cmd;
      int id;
      int originalIndex;
    };
    QList<QUndoCommand*> sortedCommands;
    QList<int> sortedLayerIdents;
    std::vector<CommandEntry> entries;
    for ( int i = 0; i < m_undoStack->count(); ++i ) {
      const QUndoCommand* base = m_undoStack->command(i);
      auto* cmd = dynamic_cast<const AbstractCommand*>(base);
      if ( cmd && cmd->layer() ) {
        entries.push_back({base, cmd->layer()->id(), i});
      }  
    }
    std::stable_sort(entries.begin(), entries.end(), [targetIdToEnd](const CommandEntry& a, const CommandEntry& b) {
      if (a.id == targetIdToEnd && b.id != targetIdToEnd) return false;
      if (a.id != targetIdToEnd && b.id == targetIdToEnd) return true;
      return a.id < b.id;
    });
    for ( const auto& entry : entries ) {
      sortedLayerIdents.append(entry.originalIndex);
    }
    for ( int i = 0; i < sortedLayerIdents.count(); ++i ) {
      const QUndoCommand* base = m_undoStack->command(sortedLayerIdents[i]);
      auto* cmd = dynamic_cast<const AbstractCommand*>(base);
      if ( cmd && cmd->layer() ) {
        sortedCommands.append(cmd->clone());
      }
    }
    m_undoStack->clear();
    for ( auto* base : sortedCommands ) {
      auto* cmd = dynamic_cast<AbstractCommand*>(base);
      if ( cmd ) {
        // cmd->setSilent(true);
        m_undoStack->push(cmd);
        // cmd->setSilent(false);
      }
    }
    
  }
}

void ImageView::removeOperationsByIdUndoStack( int id )
{
  qDebug() << "ImageView::removeOperationsByIdUndoStack(): id =" << id;
  {
    QList<QUndoCommand*> allCommands;
    for ( int i = 0; i < m_undoStack->count(); ++i ) {
        allCommands.append(const_cast<QUndoCommand*>(m_undoStack->command(i)));
    }
    for ( int i = allCommands.size() - 1; i >= 0; --i ) {
        auto* cmd = dynamic_cast<AbstractCommand*>(allCommands[i]);
        if ( cmd && cmd->layer() && cmd->layer()->id() == id ) {
            cmd->undo();
        }
    }
    QList<AbstractCommand*> remainingCommands;
    for ( auto* base : allCommands ) {
        auto* cmd = dynamic_cast<AbstractCommand*>(base);
        if ( cmd && (!cmd->layer() || cmd->layer()->id() != id) ) {
            remainingCommands.append(cmd->clone());
        }
    }
    m_undoStack->clear();
    for ( auto* clone : remainingCommands ) {
        // clone->setSilent(true);
        m_undoStack->push(clone);
        // clone->setSilent(false);
    }
  }
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
  qDebug() << "ImageView::saveMaskImage(): filename =" << filename;
  {
    if ( m_maskLayer != nullptr ) {
     // --- save index class file ---
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
     // --- save index file ---
     QFileInfo info(filename);
     QString ext = info.suffix().toLower();
     if ( ext == "png" || ext == "mnc" ) {
       QString infoFileName = info.path() + "/" + info.completeBaseName() + ".json";
       QJsonObject mainObject;
       QHashIterator<QString, MaskCutTool> i(m_maskLabelTypeNames);
       while ( i.hasNext() ) {
        i.next();
        mainObject.insert(i.key(),static_cast<int>(i.value()));
       }
       QJsonDocument doc(mainObject);
       QFile file(infoFileName);
       if ( file.open(QIODevice::WriteOnly) ) {
        file.write(doc.toJson(QJsonDocument::Indented));
        file.close();
       }
     }
    }
  }
}

void ImageView::loadMaskImage( const QString& filename ) {
  qDebug() << "ImageView::loadMaskImage(): maskfile =" << filename;
  {
    // --- load mask image ---
    QImage img(filename);
    if ( img.isNull() ) {
     qInfo() << "Error: Could not load image!";
     return;
    }
    QSize size = baseLayer()->image().size();
    if ( img.size() != size ) {
     qInfo() << "Error: Size mismatch could not load file!";
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
    // --- load json file ---
    QFileInfo info(filename);
    QString infoFileName = info.path() + "/" + info.completeBaseName() + ".json";
    QFile file(infoFileName);
    if ( file.open(QIODevice::ReadOnly) ) {
      QByteArray data = file.readAll();
      file.close();
      QJsonDocument doc = QJsonDocument::fromJson(data);
      if ( !doc.isObject() ) {
        qWarning() << "Error: Invalid JSON format!";
        return;
      }
      QJsonObject mainObject = doc.object();
      m_maskLabelTypeNames.clear();
      for ( auto it = mainObject.begin(); it != mainObject.end(); ++it ) {
        QString key = it.key();
        MaskCutTool toolValue = static_cast<MaskCutTool>(it.value().toInt());
        m_maskLabelTypeNames.insert(key, toolValue);
    }
    }
    // --- update ---
    viewport()->update();
  }
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
  qCDebug(logEditor) << "ImageView::getMaskCutToolType(): name =" << name;
  {
    if ( m_maskLabelTypeNames.contains(name) ) {
      return m_maskLabelTypeNames[name];
    }
    m_maskLabelTypeNames[name] = MaskCutTool::Ignore;
    return 0;
  }
}

// ------------------------ Layer tools -------------------------------------
void ImageView::centerOnLayer( Layer* layer ) 
{
 if ( !layer || !layer->m_item ) return;
 centerOn(layer->m_item); // QGraphicsView::centerOn
}

LayerItem* ImageView::currentLayer() const 
{
  auto items = m_scene->items(Qt::DescendingOrder);
  for ( QGraphicsItem* item : items ) {
    if ( auto* layer = dynamic_cast<LayerItem*>(item) )
      return layer;
  }
  return nullptr;
}

void ImageView::deleteLayer( Layer* layer )
{
   if ( layer == nullptr || layer->m_item == nullptr ) return;
   LayerItem *imageLayer = dynamic_cast<LayerItem*>(layer->m_item);
   m_undoStack->push(new DeleteLayerCommand(imageLayer,imageLayer->pos(),layer->id()));
}

// ------------------------ Colortable tools -------------------------------------
void ImageView::setColorTable( const QVector<QRgb> &lut )
{
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
        } else if ( m_layerOperationMode == LayerItem::OperationMode::Scale && m_transformOverlay != nullptr ) {
          if ( event->key() == Qt::Key_Q ) {
            disableTransformMode();
          } else if ( event->key() == Qt::Key_R ) {
            m_transformOverlay->reset();
          } 
        } else if ( m_layerOperationMode == LayerItem::OperationMode::CageWarp ) {
          if ( event->key() == Qt::Key_Q ) {
            qDebug() << "ImageView::keyPressEvent(): CageWarp quit...";
          } else if ( event->key() == Qt::Key_R ) {
            qDebug() << "ImageView::keyPressEvent(): CageWarp reset...";
          }
        } else if ( m_layerOperationMode == LayerItem::OperationMode::Perspective ) {
           if ( event->key() == Qt::Key_Q ) {
             qDebug() << "ImageView::keyPressEvent(): Perspective quit...";
           } else if ( event->key() == Qt::Key_R ) {
             qDebug() << "ImageView::keyPressEvent(): Perspective reset...";
           }
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
          mainWindow->setSelectedLayer(QString("Layer %1").arg(clickedItem->id()));
        }
        if ( clickedItem->isCageWarp() && clickedItem->cageMesh().isActive() ) {
           m_activeLayer = clickedItem;
           m_selectedLayer = clickedItem;
           m_cageBefore = m_activeLayer->cageMesh().points();
        } else if ( m_layerOperationMode == LayerItem::OperationMode::Scale ||
                     m_layerOperationMode == LayerItem::OperationMode::Perspective ||
                     m_layerOperationMode == LayerItem::OperationMode::CageWarp ) {
           m_activeLayer = clickedItem;
           m_selectedLayer = clickedItem;
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
  // qCDebug(logEditor) << "ImageView::mouseDoubleClickEvent(): Processing...";
  {
    MainWindow *mainWindow = dynamic_cast<MainWindow*>(m_parent);
    if ( mainWindow != nullptr ) {
      if ( mainWindow->getOperationMode() == MainWindow::MainOperationMode::Polygon &&
           ( m_polygonOperationMode == LayerItem::OperationMode::Select || event->modifiers() & Qt::ControlModifier ) ) {
       // check whether a polygon has been double clicked
       QPointF scenePos = mapToScene(event->pos());
       EditablePolygonItem* clickedItem = nullptr;
       auto itemsUnderCursor = m_scene->items(scenePos);
       for ( auto* item : itemsUnderCursor ) {
         auto* editablePolygon = dynamic_cast<EditablePolygonItem*>(item);  //  EditablePolygonItem
         if ( editablePolygon != nullptr  && editablePolygon->polygon() != nullptr ) {
           if ( editablePolygon->hitTestPolygon(scenePos) > 0 ) {
            editablePolygon->polygon()->setSelected(!editablePolygon->polygon()->isSelected());
            if ( editablePolygon->polygon()->isSelected() ) {
              int index = mainWindow->setActivePolygon(editablePolygon->polygon()->name());
              if ( index >= 0 ) {
               QVector<QColor> colors = defaultMaskColors();
               editablePolygon->setColor(colors[(index+1)%colors.length()]);
              }
            } else {
              editablePolygon->setColor(QColor(128,128,128));
            }
            clickedItem = editablePolygon;
            break;
           }
         }
       }
       if ( clickedItem != nullptr ) {
         for ( auto* item : m_scene->items() ) {
           auto* editablePolygon = dynamic_cast<EditablePolygonItem*>(item);
           if ( editablePolygon && editablePolygon != clickedItem ) {
               editablePolygon->polygon()->setSelected(false);
               editablePolygon->setColor(QColor(128,128,128)); // best color for white or black background images ?
           }
         }
       }
      } else if ( mainWindow->getOperationMode() == MainWindow::MainOperationMode::ImageLayer &&
                    m_layerOperationMode == LayerItem::OperationMode::Scale ) {
          setEnableTransformMode(m_selectedLayer);
      } else if ( mainWindow->getOperationMode() == MainWindow::MainOperationMode::ImageLayer &&
                    m_layerOperationMode == LayerItem::OperationMode::Perspective ) {
          setEnablePerspectiveWarp(m_selectedLayer);
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
  qCDebug(logEditor) << "ImageView::wheelEvent(): currentScale =" << transform().m11();
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
  // qCDebug(logEditor) 
  qDebug() << "ImageView::setLayerOperationMode(): mode =" << mode << ", m_polygonEnabled =" << m_polygonEnabled;
  {
    if ( m_layerOperationMode == LayerItem::OperationMode::Scale ) {
      qDebug() << " + clean-up scale mode...";
      disableTransformMode();
    } else if ( m_layerOperationMode == LayerItem::OperationMode::CageWarp ) {
      qDebug() << " + clean-up cage-warp mode...";
    }
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

void ImageView::setIncreaseNumberOfCageControlPoints() 
{
 qDebug() << " m_selectedLayer = " << (m_selectedLayer ? "ok" : "null" );
   if ( !m_selectedLayer || !m_selectedLayer->hasActiveCage() ) return;
   m_selectedLayer->changeNumberOfActiveCagePoints(+1);
}

void ImageView::setDecreaseNumberOfCageControlPoints() 
{
   if ( !m_selectedLayer || !m_selectedLayer->hasActiveCage() ) return;
   m_selectedLayer->changeNumberOfActiveCagePoints(-1);
}

void ImageView::setNumberOfCageControlPoints( int nControlPoints ) 
{
  qCDebug(logEditor) << "ImageView::setNumberOfCageControlPoints(): nControlPoints=" << nControlPoints;
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
       }
      }
     }
    } else if ( m_maskCutTool == MaskCutTool::Copy && m_maskLayer != nullptr ) {
     qDebug() << " *** processing ***";
     // --- testing ---
     // src  = mainImage
     // mask = m = maskImage -> area under polygon
     // m_maskLayer = class data
     // cut = the cut layer itself
     for ( int y=0; y<bounds.height(); ++y ) {
      uchar* m = mask.scanLine(y);
      unsigned int ypos = bounds.top() + y;
      for ( int x=0; x<bounds.width(); ++x ) {
       unsigned int xpos = bounds.left() + x;
       QPoint imgPos(xpos, ypos);
       QColor c = src.pixelColor(imgPos);
       if ( c != backgroundColor && m[x] > 0 ) {
         if ( m_maskLayer->pixel(xpos,ypos) != 0 ) {
          int index = m_maskLayer->pixel(xpos,ypos);
          int alpha = index == 1 ? 0 : ( index > 2 ? 128 : 255 );  // 1 := cut, >2 := copy 
          c.setAlpha(alpha);
          cut.setPixelColor(x,y,c);
         } else {
          c.setAlpha(m[x]); 
          cut.setPixelColor(x,y,c);
         }
       }
      }
     }
     // --- --- --- --- ---
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
EditablePolygonCommand* ImageView::getPolygonUndoCommand( const QString& name, bool isSelected )
{
  if ( name == "" ) {
   if ( isSelected == true ) {
     for ( int i = 0; i < m_undoStack->count(); ++i ) {
        const QUndoCommand* baseCmd = m_undoStack->command(i);
        auto polyCmd = dynamic_cast<const EditablePolygonCommand*>(baseCmd);
        if ( polyCmd && polyCmd->isSelected() ) {
         return const_cast<EditablePolygonCommand*>(polyCmd);
        }
     }
     return nullptr;
   }
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
    // get active polygon
    EditablePolygonCommand* polyCmd = ImageView::getPolygonUndoCommand("",true);
    if ( polyCmd == nullptr )
      return;
    // process polygon
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
    QColor color = QColor(255,0,0);
    MainWindow *mainWindow = dynamic_cast<MainWindow*>(m_parent);
    if ( mainWindow != nullptr ) {
      mainWindow->setMainOperationMode(MainWindow::MainOperationMode::CreatePolygon);
      int index = mainWindow->setActivePolygon(m_activePolygon->name());
      if ( index >= 0 ) {
        QVector<QColor> colors = defaultMaskColors();
        color = colors[(index+1)%colors.length()];
      }
    }
    setOnlySelectedPolygon(m_activePolygon->name());
    QPolygonF poly = m_activePolygon->polygon();
    scene()->removeItem(m_activePolygonItem);
    m_activePolygonItem->deleteLater(); // Instead of 'delete m_activePolygonItem;'
    delete m_activePolygon;
    m_activePolygon = nullptr;
    m_activePolygonItem = nullptr;
    EditablePolygonCommand *polyCmd = new EditablePolygonCommand(layer,scene(),poly,QString("Polygon %1").arg(m_editablePolygons.size()));
    polyCmd->setColor(color);
    m_undoStack->push(polyCmd);
  }
}

void ImageView::setOnlySelectedPolygon( const QString& name )
{
  qCDebug(logEditor) << "ImageView::setOnlySelectedPolygon(): name =" << name;
  {
     for ( int i = m_undoStack->count() - 1; i >= 0; --i ) {
      const QUndoCommand* cmd = m_undoStack->command(i);
      EditablePolygonCommand *polyCmd = const_cast<EditablePolygonCommand*>(dynamic_cast<const EditablePolygonCommand*>(cmd));
      if ( polyCmd != nullptr ) {
        polyCmd->setSelected(cmd->text() == name ? true : false);
      }
     }
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
     QString name = QString("Polygon %1").arg(1+m_editablePolygons.size());
     m_activePolygon = new EditablePolygon(name,this);
     m_editablePolygons.push_back(m_activePolygon);
     m_activePolygonItem = new EditablePolygonItem(m_activePolygon,layer);
     m_activePolygonItem->setColor(QColor(255,0,0)); // set polygon color here
     m_activePolygonItem->setName(name);
    } else {
     finishPolygonDrawing(layer);
     // m_activePolygonItem = nullptr;
     // m_activePolygon = nullptr;
    }
   }
  }
}

// ---------------------------- --------------- -----------------------------
// ---------------------------- Perspective free scale ----------------------
// ---------------------------- --------------- -----------------------------
void ImageView::setCageVisible( LayerItem* layer, LayerItem::OperationMode mode, bool isVisible )
{
  qDebug() << "ImageView::setCageVisible(): mode =" << mode << ", visible =" << isVisible;	
  {
    if ( mode == LayerItem::OperationMode::Scale ) {
      if ( m_transformOverlay == nullptr && isVisible == true ) {
        m_transformOverlay = new TransformOverlay(layer,m_undoStack);
        scene()->addItem(m_transformOverlay);
        m_transformOverlay->updateOverlay();
      }
      if ( m_transformOverlay != nullptr ) {
        m_transformOverlay->setVisible(isVisible);
      }
    } else if ( mode == LayerItem::OperationMode::Perspective ) {
      if ( m_perspectiveOverlay != nullptr ) {
        m_perspectiveOverlay->setVisible(isVisible);
      }
    }
  }
}

// ---------------------------- --------------- -----------------------------
// ---------------------------- Perspective free scale ----------------------
// ---------------------------- --------------- -----------------------------
void ImageView::setEnableTransformMode( LayerItem* layer )
{
  // qCDebug(logEditor) 
  qDebug() << "ImageView::setEnableTransformMode(): layer =" << (layer != nullptr ? layer->name() : "NULL");
  {
    if ( !scene() || !layer )
        return;
    if ( m_transformOverlay ) {
        scene()->removeItem(m_transformOverlay);
        delete m_transformOverlay;
        m_transformOverlay = nullptr;
    }
    m_transformOverlay = new TransformOverlay(layer,m_undoStack);
    scene()->addItem(m_transformOverlay);
    m_transformOverlay->updateOverlay();
  }
}

void ImageView::disableTransformMode()
{
  qDebug() << "ImageView::disableTransformMode(): Processing...";
  {
    if ( !m_transformOverlay )
        return;
    scene()->removeItem(m_transformOverlay);
    delete m_transformOverlay;
    m_transformOverlay = nullptr;
  }
}

// ---------------------------- --------------- -----------------------------
// ---------------------------- Perspective warp -------------------------
// ---------------------------- --------------- -----------------------------

void ImageView::setEnablePerspectiveWarp( LayerItem* layer )
{
  qDebug() << "ImageView::setEnablePerspectiveWarp(): layer =" << (layer != nullptr ? layer->name() : "NULL");
  {
    if ( !scene() || !layer )
        return;
    if ( m_perspectiveOverlay ) {
        scene()->removeItem(m_perspectiveOverlay);
        delete m_perspectiveOverlay;
        m_perspectiveOverlay = nullptr;
    }
    m_perspectiveOverlay = new PerspectiveOverlay(layer, m_undoStack);
    scene()->addItem(m_perspectiveOverlay);
    m_perspectiveOverlay->updateOverlay();
  }
}

void ImageView::disablePerspectiveWarp()
{
    if ( !m_perspectiveOverlay )
        return;
    scene()->removeItem(m_perspectiveOverlay);
    delete m_perspectiveOverlay;
    m_perspectiveOverlay = nullptr;
}