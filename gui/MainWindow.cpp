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

#include "MainWindow.h"
#include "ImageView.h"

#include "../core/ImageLoader.h"
#include "../core/ImageProcessor.h"

#include "../layer/LayerItem.h"
#include "../undo/AbstractCommand.h"
#include "../undo/PaintStrokeCommand.h"
#include "../undo/TransformLayerCommand.h"
#include "../undo/PerspectiveWarpCommand.h"
#include "../undo/LassoCutCommand.h"
#include "../undo/MirrorLayerCommand.h"
#include "../undo/MoveLayerCommand.h"
#include "../undo/CageWarpCommand.h"

#include "../util/MaskUtils.h"
#include "../util/ItemDelegate.h"
#include "../util/QWidgetUtils.h"

#ifdef HASITK
 #include "itkMultiThreaderBase.h"
#endif 

#include <QCloseEvent>
#include <QMessageBox>
#include <QWidgetAction>
#include <QStyledItemDelegate>
#include <QStandardItemModel>
#include <QJsonDocument>
#include <QApplication>
#include <QJsonArray>
#include <QMenuBar>
#include <QToolBar>
#include <QStatusBar>
#include <QAction>
#include <QHBoxLayout>
#include <QFileDialog>
#include <QLabel>
#include <QPixmap>
#include <QPainter>
#include <QComboBox>
#include <QSpinBox>
#include <QSlider>
#include <QPushButton>
#include <QDockWidget>
#include <QColorDialog>
#include <QInputDialog>
#include <QDateTime>
#include <QLineEdit>
#include <QBuffer>

#include <iostream>

/* ============================================================
 * Constructor
 * ============================================================ */
MainWindow::MainWindow( const QJsonObject& options, QWidget* parent ) : QMainWindow(parent)
{
  qCDebug(logEditor) << "MainWindow::MainWindow(): Processing...";
  {
    QString imagePath = options.value("imagePath").toString("");
    QString historyPath = options.value("historyPath").toString("");
    QString outputPath = options.value("outputPath").toString("");
    QString classPath = options.value("classPath").toString("");
    bool useVulkan = options.value("vulkan").toBool();
    Config::verbose = options.value("verbose").toBool();
    
    // setup Gimp style
    // QCoreApplication::setAttribute(Qt::AA_UseStyleSheetPropagationInWidgetStyles);
    QApplication::setStyle("Fusion");
    qApp->setStyleSheet(R"(
      QMainWindow { background-color: #2a2a2a; }
      QWidget { color: #e0e0e0; background-color: #2a2a2a; }
      QToolBar { background-color: #303030; border-bottom: 1px solid #1e1e1e; spacing: 4px; }
      QToolBar::separator { background-color: #a0a0a0; width: 1px; height: 4px; }
      QToolButton { background-color: #3a3a3a; border: 1px solid #1e1e1e; padding: 4px; }
      QToolButton:hover { background-color: #505050; }
      QToolButton:checked { background-color: #6a6a6a; border: 1px solid #909090; font-weight: bold; }
      QToolButton:checked:hover { background-color: #7a7a7a; }
      QStatusBar { background-color: #303030; border-top: 1px solid #1e1e1e; }
      QStatusBar QLabel { color: #e0e0e0; }
      QSlider::groove:horizontal { height: 4px; background: #1e1e1e; }
      QSlider::handle:horizontal { width: 10px; background: #707070; margin: -4px 0; }
      QComboBox { background-color: #3a3a3a; border: 1px solid #1e1e1e; padding: 3px; }
      QComboBox QAbstractItemView { background-color: #2a2a2a; selection-background-color: #505050; }
      QComboBox:item:disabled { color: gray; font-style: italic; }
    )");
    if ( imagePath == "" ) {
      setWindowTitle("ImageEditor - "+historyPath); 
    } else {
      setWindowTitle("ImageEditor - "+imagePath); 
    }
    
    // >>>
    m_imageView = new ImageView(this);
    m_imageView->setStyleSheet("QGraphicsView { border: none; }");
    QGraphicsScene *scene = m_imageView->getScene();
    scene->setBackgroundBrush(QWidgetUtils::createCheckerBrush());
    m_imageView->setScene(scene);
    setCentralWidget(m_imageView);

    // >>>
    createActions();
    createToolbars();
    createStatusbar();
    createDockWidgets();

    // >>>
    bool hasMainImage = false;
    if ( !imagePath.isEmpty() && historyPath.isEmpty() ) {
     hasMainImage = loadImage(imagePath);
    } else if ( imagePath.isEmpty() && !historyPath.isEmpty() ) {
     hasMainImage = loadProject(historyPath, false);
    } else if ( !imagePath.isEmpty() && !historyPath.isEmpty() ) {
     hasMainImage = loadImage(imagePath);
     loadProject(historyPath, true);
    }
    if ( !classPath.isEmpty() ) {
     m_imageView->loadMaskImage(classPath); 
    }
    
    // >>>
    if ( EditorStyle::instance().windowSize() == "maximum" ) {
       this->showMaximized();
    } else if ( EditorStyle::instance().windowSize() == "fullscreen" ) {
       this->showFullScreen();
    } else if ( hasMainImage == false) {
       this->setMinimumSize(800, 600);
    }
  }    
}

MainWindow::~MainWindow() {
  #ifdef HASITK
    std::cout << "MainWindow::~MainWindow(): Delete all..." << std::endl;
    itk::MultiThreaderBase::SetGlobalDefaultNumberOfThreads(1);
    // delete ui;
  #endif
}

// ---------------------- Catch close/exit event ----------------------
bool MainWindow::checkUnsavedData()
{
    if ( m_imageView->undoStack()->isClean() )
      return true;
    auto reply = QMessageBox::question( this, "ImageEditor",
                            tr("There are unsaved changes.\nDo you really want to quit the program?"),
                            QMessageBox::Cancel | QMessageBox::No | QMessageBox::Yes,QMessageBox::Yes);
    return (reply == QMessageBox::Yes);
}

void MainWindow::closeEvent( QCloseEvent *event ) 
{
    if ( checkUnsavedData() ) {
      event->accept();
      return;
    }
    event->ignore();
}

// ---------------------- Load and Save ----------------------
// -> m_imageView->getScene()->addItem(m_layerItem);
bool MainWindow::loadImage( const QString& filePath )
{
  qCDebug(logEditor) << "MainWindow::loadImage(): filePath=" << filePath;
  {
    ImageLoader loader;
    if ( !loader.load(filePath) ) {
      qInfo() << "Warning: Could not load main image " << filePath;
      return false;
    }
    Config::isWhiteBackgroundImage = loader.hasWhiteBackground();
    auto* scene = m_imageView->getScene();
    scene->clear();
    // main image = regular layer
    m_layerItem = new LayerItem(loader.getPixmap());
    m_layerItem->setFileInfo(filePath);
    m_layerItem->setType(LayerItem::MainImage);
    m_layerItem->setParent(this);
    scene->addItem(m_layerItem);
    // set scene size
    scene->setSceneRect(m_layerItem->boundingRect());
    // center
    m_imageView->centerOn(m_layerItem);
    // create main image layer
    rebuildLayerList();
    // ready
    return true;
  }
}

void MainWindow::openImage()
{
    bool isMaskImage = sender() == m_openMaskImageAction ? true : false;
    QString title = isMaskImage ? QString("Open mask image") : QString("Open image");
    QString fileName = QFileDialog::getOpenFileName(this,
                        title, QString(),
                        tr("Images (*.png *.jpg *.bmp)"));
    if ( fileName.isEmpty() )
      return;
    if ( isMaskImage ) {
      m_imageView->loadMaskImage(fileName);
    } else {
      loadImage(fileName);
    }
      
/*
    QPixmap pix(fileName);
    if ( !pix.isNull() ) {
      if ( isMaskImage ) {
       m_imageView->loadMaskImage(fileName);
      } else {
        m_layerItem = new LayerItem(pix);
        m_layerItem->setParent(this);
        m_imageView->getScene()->addItem(m_layerItem);
      }
      fitToWindow();
    }
*/

}

void MainWindow::saveAsImage()
{
  qCDebug(logEditor) << "MainWindow::saveAsImage(): Processing...";
  {
    bool isMaskImage = sender() == m_saveMaskImageAction ? true : false;
    QString title = isMaskImage ? QString("Save Mask Image As...") : QString("Save Image As...");
    QString fileName = QFileDialog::getSaveFileName(this,
                        title, QString(),
                        tr("PNG Image (*.png)"));
    if ( fileName.isEmpty() )
        return;
    // save first main image layer
    if ( isMaskImage ) {
     m_imageView->saveMaskImage(fileName);
    } else {
     if ( !m_imageView->getScene()->items().isEmpty() ) {
       if ( m_layerItem != nullptr ) {
         QImage mainImage = m_layerItem->image();
         // get layers != 0 
         for ( auto* item : m_imageView->getScene()->items(Qt::AscendingOrder) ) {
          auto* layer = dynamic_cast<LayerItem*>(item);
          if ( layer && layer->id() != 0 ) {
            QImage overlayImage = layer->image();
            if ( !overlayImage.isNull() ) {
             int x = layer->pos().x();
             int y = layer->pos().y();
             qCDebug(logEditor) << " + layer=" << layer->name() << ", id=" << layer->id( )<< ", pos=" << layer->pos();
             QPainter painter(&mainImage);
              painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
              painter.drawImage(x,y,overlayImage);
             painter.end();
            }
          }
         }
         if ( mainImage.save(fileName) ) {
           qCDebug(logEditor) << " + saved image as '" << fileName << "'.";
         }
       } else {
         qCDebug(logEditor) << "Warning: No main image available."; 
       }
     } else {
      qCDebug(logEditor) << "Warning: No layers in scene."; 
     }
    }
  }
}

// --------------------------- History ---------------------------

void MainWindow::saveHistory()
{
    QString fileName = QFileDialog::getSaveFileName(this,
                        tr("Save History File As"),QString(),tr("JSON file (*.json)"));
    if ( !fileName.isEmpty() ) {
     saveProject(fileName);
    }
}

bool MainWindow::saveProject( const QString& filePath )
{
    QJsonObject root;
    QUndoStack* undoStack = m_imageView->undoStack();
    
    QJsonArray layerArray;
    // --- Main Layer ---
    if ( m_layerItem != nullptr ) {
     QJsonObject mainObj;
     mainObj["id"] = "0";
     mainObj["name"] = m_layerItem->name();
     QFileInfo fileInfo(m_layerItem->filename());
     mainObj["filename"] = fileInfo.fileName();
     mainObj["pathname"] = fileInfo.absolutePath();
     mainObj["md5checksum"] = m_layerItem->checksum();
     mainObj["filetime"] = fileInfo.lastModified().toString("yyyy-MM-dd HH:mm:ss");
     if ( m_saveImageDataInProjectFile ) {
      QByteArray ba;
      QBuffer buffer(&ba);
      buffer.open(QIODevice::WriteOnly);
      m_layerItem->originalImage().save(&buffer,"PNG");
      mainObj["data"] = QString::fromUtf8(ba.toBase64());
     }
     layerArray.append(mainObj);
    }
    // --- 1. Layers ---
    const auto& layers = m_imageView->layers();
    for ( int i = layers.size()-1; i >= 0; --i ) {
        Layer* layer = layers[i];
        if ( !layer || !layer->m_item ) continue;
        QJsonObject layerObj;
        layerObj["id"] = layer->id();
        layerObj["name"] = layer->name();
        // Image -> Base64
        if ( layer->id() != 0 ) {
         QByteArray ba;
         QBuffer buffer(&ba);
         buffer.open(QIODevice::WriteOnly);
         layer->m_image.save(&buffer, "PNG");
         layerObj["data"] = QString::fromUtf8(ba.toBase64());
        }
        layerObj["opacity"] = layer->opacity();
        layerArray.append(layerObj);
    }
    root["layers"] = layerArray;

    // --- 3. Undo/Redo Stack ---
    QJsonArray undoArray;
    for ( int i = 0; i < undoStack->count(); ++i ) {
        auto* cmd = dynamic_cast<const AbstractCommand*>(undoStack->command(i));
        if ( !cmd ) continue;
        undoArray.append(cmd->toJson());
    }
    root["undoStack"] = undoArray;

    // --- 4. Write JSON to file ---
    QFile f(filePath);
    if (!f.open(QIODevice::WriteOnly)) return false;
    f.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    f.close();
    
    // --- 5. set clean flag in undo stack ---
    m_imageView->undoStack()->setClean();
    
    return true;
}

bool MainWindow::loadProject( const QString& filePath, bool skipMainImage )
{
  qCDebug(logEditor) << "MainWindow::loadProject(): filename=" << filePath << ", skipMainImage=" << skipMainImage;
  {
    QFile f(filePath);
    if ( !f.open(QIODevice::ReadOnly) ) return false;
    QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    f.close();
    if ( !doc.isObject() ) return false;
    QJsonObject root = doc.object();
    
    // --- 1. Clear current scene ---
// **** THIS clears everything **** 
//    m_imageView->getScene()->clear();
//    m_imageView->clearLayers();
// **** ---------------------- ****
    QUndoStack* undoStack = m_imageView->undoStack();
    undoStack->clear();
    
    // --- 2. Parsing layers ---
    QJsonArray layerArray = root["layers"].toArray();
    if ( !skipMainImage ) {
      for ( const QJsonValue& v : layerArray ) {
        QJsonObject layerObj = v.toObject();
        QString name = layerObj["name"].toString();
        int id = layerObj["id"].toInt();
        if ( id == 0 ) {
          QString filename = layerObj["filename"].toString();
          QString pathname = layerObj["pathname"].toString();
          QString fullfilename = pathname+"/"+filename;
          if ( !loadImage(fullfilename) ) {
           qCDebug(logEditor) << "MainWindow::loadProject(): Cannot find '" << fullfilename << "'!";
          }
        }
      }
    }
    int nCreatedLayers = 0;
    for ( const QJsonValue& v : layerArray ) {
      QJsonObject layerObj = v.toObject();
      int id = layerObj["id"].toInt();
      if ( id != 0 ) {
        QString name = layerObj["name"].toString();
        if ( layerObj.contains("data") ) {
         // qDebug() << "MainWindow::loadProject(): Creating new layer " << name << ": id " << id << "...";
         QString imgBase64 = layerObj["data"].toString();
         QByteArray ba = QByteArray::fromBase64(imgBase64.toUtf8());
         QImage image;
         image.loadFromData(ba,"PNG");
         // create new layer
         LayerItem* newLayer = new LayerItem(image);
         newLayer->setIndex(id);
         newLayer->setParent(this);
         newLayer->setUndoStack(m_imageView->undoStack());
         Layer* layer = new Layer(id,image);
         layer->m_name = name;
         layer->m_item = newLayer;
         newLayer->setLayer(layer);
         m_imageView->layers().push_back(layer);
         m_imageView->getScene()->addItem(newLayer);
         nCreatedLayers += 1;
        }
      }
    }
    if ( nCreatedLayers > 0 ) {
      rebuildLayerList();
    }
    
    QList<LayerItem*> layers;
    for ( auto* item : m_imageView->getScene()->items(Qt::DescendingOrder) ) {
      auto* layer = dynamic_cast<LayerItem*>(item);
      if ( layer ) {
        layers << layer;
      }  
    }

    // --- 3. Restore Undo/Redo Stack ---
    EditablePolygonCommand* editablePolyCommand = nullptr;
    QJsonArray undoArray = root["undoStack"].toArray();
    for ( const QJsonValue& v : undoArray ) {
        QJsonObject cmdObj = v.toObject();
        QString type = cmdObj["type"].toString();
        QString text = cmdObj["text"].toString();
        // qDebug() << "MainWindow::loadProject(): Found undo call: type=" << type << ", text=" << text;
        AbstractCommand* cmd = nullptr;
        if ( type == "PaintStrokeCommand" ) {
           cmd = PaintStrokeCommand::fromJson(cmdObj, layers);
        } else if ( type == "LassoCutCommand" ) {
           cmd = LassoCutCommand::fromJson(cmdObj, layers);
           LassoCutCommand* cutCommand = dynamic_cast<LassoCutCommand*>(cmd);
           if ( cutCommand != nullptr ) {
             cutCommand->setController(editablePolyCommand);
           }
        } else if ( type == "MoveLayer" ) {
           cmd = MoveLayerCommand::fromJson(cmdObj, layers);
        } else if ( type == "MirrorLayer" ) {
           cmd = MirrorLayerCommand::fromJson(cmdObj, layers);
        } else if ( type == "CageWarp" ) {
           cmd = CageWarpCommand::fromJson(cmdObj, layers);
        } else if ( type == "TransformLayer" || type == "TransformLayerCommand" ) {
           cmd = TransformLayerCommand::fromJson(cmdObj, layers);
        } else if ( type == "PerspectiveTransform" ) {
           // cmd = PerspectiveTransformCommand::fromJson(cmdObj, layers);
        } else if ( type == "PerspectiveWarp" ) {
           cmd = PerspectiveWarpCommand::fromJson(cmdObj, layers);
        } else if ( type == "EditablePolygonCommand" ) {
           cmd = EditablePolygonCommand::fromJson(cmdObj, layers);
           editablePolyCommand = dynamic_cast<EditablePolygonCommand*>(cmd);
        } else {
           qCDebug(logEditor) << "MainWindow::loadProject(): " << type << " not yet processed.";
        }
        // ggf. weitere Command-Typen hier hinzufügen
        if ( cmd )
            undoStack->push(cmd);
    }
    
    // --- 4. set clean flag in undo stack ---
    m_imageView->undoStack()->setClean();
    
    return true;
  }
}

void MainWindow::loadHistory( const QString& file )
{
    QFile f(file);
    if ( !f.open(QIODevice::ReadOnly) )
        return;
    QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    QJsonArray arr = doc.array();
    m_imageView->undoStack()->clear();
    for ( const auto& v : arr ) {
        auto* cmd = AbstractCommand::fromJson(v.toObject(),m_imageView);
        if ( !cmd ) continue;
        m_imageView->undoStack()->push(cmd);
    }
}

void MainWindow::openHistory()
{
    QString fileName = QFileDialog::getOpenFileName(this,
                        tr("Open JSON history file"), QString(),
                        tr("JSON file (*,json)"));
    if ( fileName.isEmpty() )
      return;
    loadHistory(fileName);
}

// ---------------------- Mask Image ----------------------
void MainWindow::createMaskImage()
{
    LayerItem* item = m_imageView->baseLayer();
    if ( item ) {
      m_imageView->createMaskLayer(item->image().size());
    }
}

// ---------------------- Create ----------------------
void MainWindow::createDockWidgets() 
{
   // layer dock
   m_layerDock = new QDockWidget("Layers", this);
   m_layerDock->setAllowedAreas(Qt::RightDockWidgetArea);
   m_layerList = new QListWidget(m_layerDock);
   m_layerList->setSelectionMode(QAbstractItemView::SingleSelection);
   m_layerList->setDragDropMode(QAbstractItemView::InternalMove);
   m_layerList->setDefaultDropAction(Qt::MoveAction);
   m_layerDock->setWidget(m_layerList);
   m_layerDock->hide();
   addDockWidget(Qt::RightDockWidgetArea, m_layerDock);
   for ( Layer* layer : m_imageView->layers() ) {
    QListWidgetItem* item = new QListWidgetItem(layer->name(), m_layerList);
    item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
    item->setCheckState(layer->m_visible ? Qt::Checked : Qt::Unchecked);
    QIcon eyeIcon(layer->m_visible ? ":/icons/eye_open.png" : ":/icons/eye_closed.png");
    item->setIcon(eyeIcon);
   }
   connect(m_layerList, &QListWidget::itemChanged, this, &MainWindow::toggleLayerVisibility);
   connect(m_imageView, &ImageView::layerAdded, this, &MainWindow::rebuildLayerList);
   connect(m_imageView, &ImageView::lassoLayerAdded, this, &MainWindow::newLassoLayerCreated);
   // Reihenfolge Drag & Drop → Scene aktualisieren
   connect(m_layerList->model(), &QAbstractItemModel::rowsMoved, [this]() {
     const int count = m_layerList->count();
     for ( int i=0 ; i<count ; ++i ) {
       QListWidgetItem* item = m_layerList->item(i);
       Layer* layer = static_cast<Layer*>(item->data(Qt::UserRole).value<void*>());
       if ( layer && layer->m_item ) {
         layer->m_item->setZValue(count - i); // oberste Layer = höchster Z-Wert
       }
     }
     m_imageView->getScene()->update();
   });
   // Kontextmenü für Layer
   m_layerList->setContextMenuPolicy(Qt::CustomContextMenu);
   connect(m_layerList, &QListWidget::customContextMenuRequested,this, &MainWindow::showLayerContextMenu);
   connect(m_layerList, &QListWidget::itemClicked, this, &MainWindow::onLayerItemClicked);

   // history dock
   m_undoView = new QUndoView(m_imageView->undoStack());
   m_undoView->setContextMenuPolicy(Qt::CustomContextMenu);
   m_undoView->setItemDelegate(new DarkHistoryDelegate(m_undoView));
   m_undoView->setWindowTitle("History");
   m_undoView->setStyleSheet("QUndoView { background-color: #1e1e1e; border: none; }");
 
 #ifdef TTT  
   m_undoView->setStyleSheet(
      /* Gesamtansicht */
      "QUndoView {"
      "   background-color: #1e1e1e;" // Dunkles Anthrazit
      "   color: #e0e0e0;"            // Hellgraue Schrift für gute Lesbarkeit
      "   border: 1px solid #333333;"
      "   outline: none;"             // Entfernt den Fokus-Rahmen
      "}"
      /* Einzelne Einträge */
      "QUndoView::item {"
      "   padding: 6px 10px;"
      "   border-bottom: 1px solid #2d2d2d;"
      "}"
      /* Die "Zukunft" (Rückgängig gemachte Befehle) */
      "QUndoView::item:!enabled {"
      "   color: #555555;"           
      "   background-color: #1a1a1a;" 
      "   font-style: italic;"
      "}"
      /* Hover-Effekt (Maus fährt drüber) */
      "QUndoView::item:hover {"
      "   background-color: #333333;"
      "}"
      /* Der aktuell ausgewählte/aktive Stand */
      "QUndoView::item:selected {"
      "   background-color: #094771;" // Dunkelblaues Highlight
      "   color: #ffffff;"
      "   border-left: 3px solid #007acc;" // Akzent-Linie links
      "}"
   );
 #endif
 
   // m_undoView->setAlternatingRowColors(true);
   m_historyDock = new QDockWidget("Undo History", this);
   m_historyDock->setWidget(m_undoView);
   m_historyDock->setAllowedAreas(Qt::RightDockWidgetArea);
   m_historyDock->setFeatures(QDockWidget::DockWidgetClosable | QDockWidget::DockWidgetMovable);
   addDockWidget(Qt::RightDockWidgetArea, m_historyDock);
   m_historyDock->setMinimumWidth(200);
   m_historyDock->hide();
   connect(m_undoView, &QUndoView::activated, this, [this](const QModelIndex &index){
     int i = index.row();
     m_imageView->undoStack()->setIndex(i);
   });
   connect(m_undoView, &QUndoView::customContextMenuRequested, this, [this](const QPoint &pos) {
    QModelIndex index = m_undoView->indexAt(pos);
    if ( !index.isValid() ) return;
    QMenu menu(this);
    QAction *jumpAction = menu.addAction(tr("Jump back to this point"));
    QAction *renameAction = menu.addAction(tr("Rename..."));
    QAction *selectedAction = menu.exec(m_undoView->mapToGlobal(pos));
    if ( selectedAction == jumpAction ) {
        m_imageView->undoStack()->setIndex(index.row()); 
    } else if ( selectedAction == renameAction ) {
        QUndoCommand *cmd = const_cast<QUndoCommand*>(m_imageView->undoStack()->command(index.row() - 1));
        bool ok;
        QString newText = QInputDialog::getText(this, tr("Rename command"),
                                                 tr("New name:"), QLineEdit::Normal,
                                                 cmd->text(), &ok);
        if ( ok && !newText.isEmpty() ) {
            cmd->setText(newText);
            m_undoView->viewport()->update();
        }
    }
   });
   
}

void MainWindow::toggleDocks() 
{
  if ( m_historyDock->isVisible() ) m_historyDock->hide();
  else m_historyDock->show();
  if ( m_layerDock->isVisible() ) m_layerDock->hide();
  else m_layerDock->show();
}

// --------------------------------- Layer tools ---------------------------------
void MainWindow::toggleLayerVisibility(  QListWidgetItem* item )
{
 qCDebug(logEditor) << "MainWindow::toggleLayerVisibility(): Processing...";
 {
    if ( !item ) return;
    if ( m_updatingLayerList ) return; // ⚡ verhindert Rekursion
    Layer* layer = static_cast<Layer*>(item->data(Qt::UserRole).value<void*>());
    if ( !layer || !layer->m_item ) return;
    // Sichtbarkeit umschalten
    layer->m_visible = !layer->m_visible;
    layer->m_item->setVisible(layer->m_visible);
    // Flag setzen, damit wir Änderungen nicht rekursiv triggern
    m_updatingLayerList = true;
    // Update Auge-Icon
    item->setIcon(QIcon(layer->m_visible
        ? ":/icons/eye_open.png"
        : ":/icons/eye_closed.png"));
    // Update CheckState
    item->setCheckState(layer->m_visible ? Qt::Checked : Qt::Unchecked);
    m_updatingLayerList = false;
 }   
}

void MainWindow::updateLayerList()
{
   qDebug() << "MainWindow::updateLayerList(): Processing...";
   rebuildLayerList();
}

void MainWindow::rebuildLayerList()
{
  qDebug() << "MainWindow::rebuildLayerList(): Rebuild layer list...";
  {
    // updating layer list in docks widget
    m_updatingLayerList = true;
    m_layerList->clear();
    const auto& layers = m_imageView->layers();
    for ( int i = layers.size()-1; i >= 0; --i ) {
        Layer* layer = layers[i];
        if ( !layer || !layer->m_item || !layer->m_active ) continue;
        QListWidgetItem* item = new QListWidgetItem(QString("Layer %1").arg(layer->id())); // layer->name());
        item->setCheckState(layer->m_visible ? Qt::Checked : Qt::Unchecked);
        item->setData(Qt::UserRole, QVariant::fromValue<void*>(layer));
        item->setIcon(QIcon(layer->m_visible
            ? ":/icons/eye_open.png"
            : ":/icons/eye_closed.png"));
        m_layerList->addItem(item);
    }
    m_updatingLayerList = false;
    // updating layer list in m_selectLayerItem
    int nItems = 0;
    int currentId = m_selectLayerItem->currentData().toInt();
    m_selectLayerItem->clear();
    for ( int i = layers.size()-1; i >= 0; --i ) {
      Layer* layer = layers[i];
      if ( !layer || !layer->m_item || !layer->m_active ) continue;
      m_selectLayerItem->addItem(QString("Layer %1").arg(layer->id()), layer->id());
      nItems += 1;
    }
    if ( nItems == 0 ) {
       m_selectLayerItem->addItems({"None yet defined"});
    } else {
       int index = m_selectLayerItem->findData(currentId);
       if ( index != -1 ) {
        m_selectLayerItem->setCurrentIndex(index);
       }
    }
  }
}

void MainWindow::setSelectedLayer( const QString &name )
{
  qDebug() << "MainWindow::setSelectedLayer(): name =" << name;
  {
    int index = m_selectLayerItem->findText(name);
    if ( index != -1 ) {
      m_selectLayerItem->setCurrentIndex(index);
    }
  }
}

void MainWindow::layerItemClicked( QListWidgetItem* item )
{
  qCDebug(logEditor) << "MainWindow::layerItemClicked(): Processing...";
  {
    if ( !item ) return;
    toggleLayerVisibility(item);  // Auge/CheckState
    // Layer markieren in der Scene
    Layer* layer = static_cast<Layer*>(item->data(Qt::UserRole).value<void*>());
    if ( !layer || !layer->m_item ) return;
    // Alle Items deselektieren
    for ( Layer* l : m_imageView->layers() ) {
        if ( l->m_item )
            l->m_item->setSelected(false);
    }
    layer->m_item->setSelected(true);
  }
}

void MainWindow::onLayerItemClicked( QListWidgetItem* item )
{
    if ( !item ) return;
    void* ptr = item->data(Qt::UserRole).value<void*>();
    Layer* layer = static_cast<Layer*>(ptr);
    if ( layer ) {
     for ( Layer* l : m_imageView->layers() ) {
       if ( l->m_item ) {
         l->m_item->setSelected(l == layer ? true : false);
       }
     }
    }
}

void MainWindow::showLayerContextMenu( const QPoint& pos )
{
    QListWidgetItem* item = m_layerList->itemAt(pos);
    if ( !item ) return;
    QMenu menu(this);
    menu.addAction("Save Layer as...",  [this, item]() {
        Layer* layer = static_cast<Layer*>(item->data(Qt::UserRole).value<void*>());
        if ( !layer ) return;
        QString fileName = QFileDialog::getSaveFileName(this,
                        tr("Save Layer Image As"),QString(),tr("PNG image file (*.png)"));
        if ( !fileName.isEmpty() ) {
          layer->image().save(fileName, "PNG", 80);
          qDebug() << "Saved layer " << layer->name() << " image as " << fileName;
          qDebug() << "  geometry: " << layer->m_item->boundingRect();
        }
    });
    menu.addAction("Delete Layer", this, &MainWindow::deleteLayer);
    menu.addAction("Merge Layer", this, &MainWindow::mergeLayer);
    menu.addAction("Duplicate Layer", this, &MainWindow::duplicateLayer);
    menu.addAction("Rename Layer", this, &MainWindow::renameLayer);
    menu.addAction("Link to Image", [this, item]() {
        Layer* layer = static_cast<Layer*>(item->data(Qt::UserRole).value<void*>());
        if ( !layer ) return;
        // Layer verknüpfen
        layer->m_linkedToImage = !layer->m_linkedToImage;
        // Icon oder Checkmark optional anpassen
        if ( layer->m_linkedToImage )
            item->setText(layer->name() + " (linked)");
        else
            item->setText(layer->name());
    });
    menu.addAction("Center Layer", [this, item]() {
        Layer* layer = static_cast<Layer*>(item->data(Qt::UserRole).value<void*>());
        if (!layer) return;
        m_imageView->centerOnLayer(layer);
    });
    menu.addAction("Layer Info", [this, item]() {
        Layer* layer = static_cast<Layer*>(item->data(Qt::UserRole).value<void*>());
        if ( !layer || !layer->m_item ) return;
        QRectF bbox = layer->m_item->boundingRect();
        int polygonPoints = 0;
        if ( !layer->m_polygon.isEmpty() )
            polygonPoints = layer->m_polygon.size();
        QSize pixmapSize;
        if ( auto pixmapItem = dynamic_cast<QGraphicsPixmapItem*>(layer->m_item) )
            pixmapSize = pixmapItem->pixmap().size();
        qInfo() << "Layer Info:";
        qInfo() << " Name:" << layer->name();
        qInfo() << " Visible:" << layer->m_visible;
        qInfo() << " Linked to Image:" << layer->m_linkedToImage;
        qInfo() << " Bounding Box:" << bbox;
        qInfo() << " Position:" << layer->m_item->pos();
        qInfo() << " Polygon Points:" << polygonPoints;
        if ( !pixmapSize.isEmpty() )
            qInfo() << " Pixmap Size:" << pixmapSize;
    });
    menu.exec(m_layerList->viewport()->mapToGlobal(pos));
}

void MainWindow::deleteLayer()
{
  qCDebug(logEditor) << "MainWindow::deleteLayer(): Processing...";
  {
    
    QListWidgetItem* item = m_layerList->currentItem();
    if ( !item ) return;
    Layer* layer = static_cast<Layer*>(item->data(Qt::UserRole).value<void*>());
    if ( !layer ) return;
    
    QString labelText = QString("Do you really want to delete the layer? Press the Revoke button to undo all operations "
                    "(all entries will be permanently deleted from the history list) or press Delete to remove "
                    "the layer with the option of restoring it.");
    int result = QWidgetUtils::showIconDialog(this,QString("Delete %1").arg(layer->name()),labelText);

    if ( result == 1 ) {
      // Undo all operations from the stack. Preserve original state
      m_imageView->removeOperationsByIdUndoStack(layer->id());
    } else if ( result == 2 ) {
      m_imageView->deleteLayer(layer);
      // if ( layer->m_item ) 
      //   m_imageView->getScene()->removeItem(layer->m_item);
      // m_imageView->layers().removeOne(layer);
      rebuildLayerList();
      m_imageView->rebuildUndoStack();
    }
  }
}

void MainWindow::duplicateLayer()
{
  qCDebug(logEditor) << "MainWindow::duplicateLayer(): Processing...";
  {
    QListWidgetItem* item = m_layerList->currentItem();
    if ( !item ) return;
    Layer* layer = static_cast<Layer*>(item->data(Qt::UserRole).value<void*>());
    if ( !layer || !layer->m_item ) return;
    Layer* newLayer = new Layer(100);  // !!! WARNING !!!
    newLayer->m_name = layer->name() + " Copy";
    newLayer->m_visible = layer->m_visible;
    newLayer->m_item = m_imageView->getScene()->addPixmap(
        static_cast<QGraphicsPixmapItem*>(layer->m_item)->pixmap());
    newLayer->m_item->setPos(layer->m_item->pos());
    newLayer->m_item->setFlags(QGraphicsItem::ItemIsMovable | QGraphicsItem::ItemIsSelectable);
    m_imageView->layers().append(newLayer);
    rebuildLayerList();
  }
}

void MainWindow::mergeLayer()
{
  qDebug() << "MainWindow::mergeLayer() Processing...";
  {
  
  }
}

void MainWindow::renameLayer()
{
    QListWidgetItem* item = m_layerList->currentItem();
    if ( !item ) return;
    Layer* layer = static_cast<Layer*>(item->data(Qt::UserRole).value<void*>());
    if ( !layer ) return;
    bool ok;
    QString newName = QInputDialog::getText(this, "Rename Layer",
                                            "New name:", QLineEdit::Normal,
                                            layer->name(), &ok);
    if ( ok && !newName.isEmpty() ) {
        layer->m_name = newName;
        item->setText(newName);
    }
}

// --------------------------------- Actions ---------------------------------
void MainWindow::createActions()
{
    m_infoAction = new QAction(tr("Info"), this);
    connect(m_infoAction, &QAction::triggered, this, &MainWindow::info);
    
    m_sortHistoryAction = new QAction(tr("Sort and merge history"), this);
    connect(m_sortHistoryAction, &QAction::triggered, m_imageView, &ImageView::rebuildUndoStack);
    
    m_saveHistoryAction = new QAction(tr("Save history as..."), this);
    connect(m_saveHistoryAction, &QAction::triggered, this, &MainWindow::saveHistory);
    
    m_openHistoryAction = new QAction(tr("Open history file"), this);
    connect(m_openHistoryAction, &QAction::triggered, this, &MainWindow::openHistory);
    
    m_openAction = new QAction(tr("Open"), this);
    connect(m_openAction, &QAction::triggered, this, &MainWindow::openImage);

    m_saveAsAction = new QAction(tr("Save As..."), this);
    connect(m_saveAsAction, &QAction::triggered, this, &MainWindow::saveAsImage);
    
    m_pipetteAction = new QAction("Pipette", this);
    m_pipetteAction->setCheckable(true);
    connect(m_pipetteAction, &QAction::toggled, m_imageView, &ImageView::enablePipette);
    connect(m_pipetteAction, &QAction::toggled, this, &MainWindow::updateButtonState);
    
    m_zoom1to1Action = new QAction("1:1", this);
    connect(m_zoom1to1Action, &QAction::triggered, this, &MainWindow::zoom1to1);
    m_fitAction      = new QAction("Fit", this);
    connect(m_fitAction, &QAction::triggered,  this, &MainWindow::fitToWindow);
    
    m_crosshairAction = new QAction("Crosshair", this);
    m_crosshairAction->setCheckable(true);
    connect(m_crosshairAction, &QAction::toggled, m_imageView, &ImageView::setCrosshairVisible);
    
    m_quitAction = new QAction("Quit Application", this);
    m_quitAction->setToolTip("Exit application");
    connect(m_quitAction, &QAction::triggered, this, [this]() {
      QApplication::quit();
    });
    
    m_undoAction = m_imageView->undoStack()->createUndoAction(this," Undo ");
    m_redoAction = m_imageView->undoStack()->createRedoAction(this," Redo ");
    
    m_paintAction = new QAction(" Paint", this);
    m_paintAction->setCheckable(true);
    connect(m_paintAction, &QAction::toggled, m_imageView, &ImageView::setPaintToolEnabled);
    connect(m_paintAction, &QAction::toggled, this, &MainWindow::updateButtonState);
    
    m_showDockWidgets = new QAction(" Docks", this);
    m_showDockWidgets->setCheckable(true);
    connect(m_showDockWidgets, &QAction::toggled, this, &MainWindow::toggleDocks);
    
    m_lassoAction = new QAction(" Create new lasso", this);
    m_lassoAction->setCheckable(true);
    connect(m_lassoAction, &QAction::toggled, m_imageView, &ImageView::setLassoEnabled);
    connect(m_lassoAction, &QAction::toggled, this, &MainWindow::updateButtonState);
    
    m_polygonAction = new QAction(" Create new polygon", this);
    m_polygonAction->setCheckable(true);
    connect(m_polygonAction, &QAction::toggled, m_imageView, &ImageView::setPolygonEnabled);
    // connect(m_polygonAction, &QAction::toggled, this, &MainWindow::updateButtonState);
    
    // mask image actions
    m_createMaskImageAction = new QAction(" Create new class", this);
    connect(m_createMaskImageAction, &QAction::triggered, this, &MainWindow::createMaskImage);
    m_openMaskImageAction = new QAction(" Open class mask", this);
    connect(m_openMaskImageAction, &QAction::triggered, this, &MainWindow::openImage);
    m_saveMaskImageAction = new QAction(" Save class mask as...", this);
    connect(m_saveMaskImageAction, &QAction::triggered, this, &MainWindow::saveAsImage);
    m_paintMaskImageAction = new QAction("Paint", this);
    m_paintMaskImageAction->setCheckable(true);
    m_eraseMaskImageAction = new QAction("Erase", this);
    m_eraseMaskImageAction->setCheckable(true);
    connect(m_paintMaskImageAction, &QAction::toggled, this, [=](bool on){
        if ( on ) m_eraseMaskImageAction->setChecked(false);
        m_imageView->setMaskTool(on?ImageView::MaskPaint:ImageView::None);
    });
    connect(m_eraseMaskImageAction, &QAction::toggled, this, [=](bool on){
        if ( on ) m_paintMaskImageAction->setChecked(false);
        m_imageView->setMaskTool(on?ImageView::MaskErase:ImageView::None);
    });
    
    // control actions
    m_paintControlAction = new QAction("Paint", this);
    m_paintControlAction->setCheckable(true);
    m_paintControlAction->setChecked(true);
    connect(m_paintControlAction, &QAction::toggled, this, &MainWindow::updateControlButtonState);
    m_maskControlAction = new QAction("Mask classes", this);
    m_maskControlAction->setCheckable(true);
    connect(m_maskControlAction, &QAction::toggled, this, &MainWindow::updateControlButtonState);
    
    m_layerControlAction = new QAction("Layer", this);
    m_layerControlAction->setCheckable(true);
    connect(m_layerControlAction, &QAction::toggled, this, &MainWindow::updateControlButtonState);
    
    m_lassoControlAction = new QAction("Free selection", this);
    m_lassoControlAction->setCheckable(true);
    connect(m_lassoControlAction, &QAction::toggled, this, &MainWindow::updateControlButtonState);
    m_polygonControlAction = new QAction("Polygon", this);
    m_polygonControlAction->setCheckable(true);
    connect(m_polygonControlAction, &QAction::toggled, this, &MainWindow::updateControlButtonState);
}

void MainWindow::updateControlButtonState() 
{
  qCDebug(logEditor) << "MainWindow::updateControlButtonState(): Processing...";
  {
    MainOperationMode operationMode;
    // --- ---
    bool isA = sender() == m_paintControlAction? 1 : 0;
    bool isB = sender() == m_lassoControlAction ? 1 : 0;
    bool isC = sender() == m_maskControlAction ? 1 : 0;
    bool isD = sender() == m_polygonControlAction ? 1 : 0;
    bool isE = sender() == m_layerControlAction ? 1 : 0;
    // --- ---
    bool paintIsChecked = m_paintControlAction->isChecked();
    bool lassoIsChecked = m_lassoControlAction->isChecked();
    bool maskIsChecked = m_maskControlAction->isChecked();
    bool polygonIsChecked = m_polygonControlAction->isChecked();
    bool layerIsChecked = m_layerControlAction->isChecked();
    // --- ---
    if ( isA && paintIsChecked && (lassoIsChecked || maskIsChecked || polygonIsChecked || layerIsChecked) ) {
     m_lassoControlAction->setChecked(false);
     m_maskControlAction->setChecked(false);
     m_polygonControlAction->setChecked(false);
     m_layerControlAction->setChecked(false);
     m_polygonToolbar->setVisible(false);
     m_lassoToolbar->setVisible(false); 
     m_maskToolbar->setVisible(false);
     m_layerToolbar->setVisible(false);
    }
    if ( isB && lassoIsChecked && (paintIsChecked || maskIsChecked || polygonIsChecked || layerIsChecked) ) {
     m_paintControlAction->setChecked(false);
     m_maskControlAction->setChecked(false);
     m_polygonControlAction->setChecked(false);
     m_layerControlAction->setChecked(false);
     m_polygonToolbar->setVisible(false);
     m_editToolbar->setVisible(false);
     m_maskToolbar->setVisible(false);
     m_layerToolbar->setVisible(false);
    }
    if ( isC && maskIsChecked && (paintIsChecked || lassoIsChecked || polygonIsChecked || layerIsChecked) ) {
     m_paintControlAction->setChecked(false);
     m_lassoControlAction->setChecked(false);
     m_polygonControlAction->setChecked(false);
     m_layerControlAction->setChecked(false);
     m_polygonToolbar->setVisible(false);
     m_editToolbar->setVisible(false);
     m_lassoToolbar->setVisible(false);
     m_layerToolbar->setVisible(false);
    }
    if ( isD && polygonIsChecked && (paintIsChecked || lassoIsChecked || maskIsChecked || layerIsChecked) ) {
     m_paintControlAction->setChecked(false);
     m_lassoControlAction->setChecked(false);
     m_maskControlAction->setChecked(false);
     m_layerControlAction->setChecked(false);
     m_editToolbar->setVisible(false);
     m_lassoToolbar->setVisible(false);
     m_maskToolbar->setVisible(false);
     m_layerToolbar->setVisible(false);
    }
    if ( isE && layerIsChecked && (paintIsChecked || lassoIsChecked || maskIsChecked || polygonIsChecked) ) {
     m_paintControlAction->setChecked(false);
     m_lassoControlAction->setChecked(false);
     m_maskControlAction->setChecked(false);
     m_polygonControlAction->setChecked(false);
     m_editToolbar->setVisible(false);
     m_lassoToolbar->setVisible(false);
     m_maskToolbar->setVisible(false);
     m_polygonToolbar->setVisible(false);
    }
    // --- ---
    if ( m_paintControlAction->isChecked() ) {
     m_editToolbar->setVisible(true);
     m_operationMode = MainOperationMode::Paint;
    } else if ( m_lassoControlAction->isChecked() ) {
     m_lassoToolbar->setVisible(true);   
     m_operationMode = MainOperationMode::FreeSelection;
    } else if ( m_maskControlAction->isChecked() ) {
     m_maskToolbar->setVisible(true);   
     m_operationMode = MainOperationMode::Mask;
    } else if ( m_polygonControlAction->isChecked() ) {
     m_polygonToolbar->setVisible(true);   
     m_operationMode = MainOperationMode::Polygon;
    } else if ( m_layerControlAction->isChecked() ) {
     m_layerToolbar->setVisible(true);  
     m_operationMode = MainOperationMode::ImageLayer; 
    }
  }
}

void MainWindow::updateButtonState() 
{
    bool isA = sender() == m_pipetteAction? 1 : 0;
    bool isB = sender() == m_paintAction ? 1 : 0;
    bool isC = sender() == m_lassoAction ? 1 : 0;
    bool pipetteIsChecked = m_pipetteAction->isChecked();
    bool paintIsChecked = m_paintAction->isChecked();
    bool lassoIsChecked = m_lassoAction->isChecked();
    if ( isB && m_paintAction->isChecked() && ( pipetteIsChecked || lassoIsChecked ) ) {
       m_pipetteAction->setChecked(false);
       m_lassoAction->setChecked(false);
    }
    if ( isA && m_pipetteAction->isChecked() && ( paintIsChecked || lassoIsChecked ) ) {
       m_paintAction->setChecked(false);
       m_lassoAction->setChecked(false);
    }
    if ( isC && m_lassoAction->isChecked() && ( paintIsChecked || pipetteIsChecked ) ) {
       m_paintAction->setChecked(false);
       m_pipetteAction->setChecked(false);
    }
}

// --- Default Colors ---
QComboBox* MainWindow::buildDefaultColorComboBox( const QString& name )
{
    QComboBox* colorComboBox = new QComboBox();
    colorComboBox->setItemDelegate(new QStyledItemDelegate(colorComboBox));
    QVector<QColor> colors = defaultMaskColors();
    colorComboBox->setIconSize(QSize(20, 20));
    colorComboBox->setMinimumWidth(110);
    unsigned int i = 0;
    for ( const auto& color : colors ) {
      if ( i!=0 ) {
        QPixmap pixmap(24,24);
        pixmap.fill(Qt::transparent);
        QPainter painter(&pixmap);
         painter.setRenderHint(QPainter::Antialiasing);
         painter.setBrush(color);
         painter.setPen(QPen(Qt::black, 1));
         painter.drawRoundedRect(2, 2, 20, 20, 4, 4);
        painter.end();
        colorComboBox->addItem(QIcon(pixmap),QString("%1 %2").arg(name).arg(i));
      }
      i += 1;
    }
    // --- disable ComboBox entries ---
/*
    QStandardItemModel* model = qobject_cast<QStandardItemModel*>(colorComboBox->model());
    if ( model ) {
      for ( int i = 1; i < 10; ++i ) {
        QStandardItem* item = model->item(i);
        if ( item ) {
            item->setEnabled(false);
            // item->setFlags(Qt::NoItemFlags);
            // item->setForeground(QPalette().brush(QPalette::Disabled, QPalette::Text));
        }
       }
    }
*/
    return colorComboBox;
}

void MainWindow::createToolbars()
{
    // ============================================================
    // create first toolbar
    // ============================================================
    QToolBar *fileToolbar = new QToolBar("View",this);
    fileToolbar->setStyleSheet("QToolBar { background-color:#222222; } QToolButton { color:white; background-color:#222222; }");
    fileToolbar->setAutoFillBackground(true);
    fileToolbar->setMovable(false);
    fileToolbar->setFloatable(false);
    addToolBar(Qt::TopToolBarArea,fileToolbar);
    fileToolbar->addAction(m_openAction);
    fileToolbar->addAction(m_saveAsAction);
    fileToolbar->addAction(m_zoom1to1Action);
    fileToolbar->insertSeparator(m_zoom1to1Action);
    fileToolbar->addAction(m_fitAction);
    fileToolbar->addAction(m_crosshairAction);
    // color tables
    QComboBox* colorTableCombo = new QComboBox();
    colorTableCombo->addItems({"Original","Invert","Red","Green","Blue"});
    fileToolbar->addWidget(colorTableCombo);
    connect(colorTableCombo, &QComboBox::currentTextChanged, m_imageView, [this](const QString& text){
       QVector<QRgb> lut(256);
       if (text=="Original") for(int i=0;i<256;i++) lut[i] = qRgb(i,i,i);
       else if(text=="Invert") for(int i=0;i<256;i++) lut[i] = qRgb(255-i,255-i,255-i);
       else if(text=="Red") for(int i=0;i<256;i++) lut[i] = qRgb(i,0,0);
       else if(text=="Green") for(int i=0;i<256;i++) lut[i] = qRgb(0,i,0);
       else if(text=="Blue") for(int i=0;i<256;i++) lut[i] = qRgb(0,0,i);
       m_imageView->setColorTable(lut);
    });
    fileToolbar->addAction(m_showDockWidgets);
    fileToolbar->insertSeparator(m_showDockWidgets);
    fileToolbar->addAction(m_undoAction);
    fileToolbar->insertSeparator(m_undoAction);
    fileToolbar->addAction(m_redoAction);
    fileToolbar->addAction(m_sortHistoryAction);
    fileToolbar->insertSeparator(m_sortHistoryAction);
    fileToolbar->addAction(m_saveHistoryAction);
    fileToolbar->insertSeparator(m_saveHistoryAction);
    fileToolbar->addAction(m_openHistoryAction);
    fileToolbar->addAction(m_infoAction);
    fileToolbar->insertSeparator(m_infoAction);
    // add spacer
    QWidget* spacer = new QWidget();
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    QWidgetAction* spacerAction = new QWidgetAction(fileToolbar);
    spacerAction->setDefaultWidget(spacer);
    fileToolbar->addAction(spacerAction);
    fileToolbar->addAction(m_quitAction);
    
    // ============================================================
    //  toolbar break
    // ============================================================
    addToolBarBreak(Qt::TopToolBarArea);
    
    // ============================================================
    // create second toolbar
    // ============================================================
    QToolBar* controlToolbar = addToolBar(tr("Control"));
    
    controlToolbar->setStyleSheet(("QToolBar { background-color: #303030; border-bottom: 1px solid #1e1e1e; spacing: 4px; }"));
    
   // controlToolbar->setStyleSheet("QToolBar { background-color:#220022; } QToolButton { color:white; background-color:#222222; }");
    
   // QPalette pal = controlToolbar->palette();
   // pal.setColor(QPalette::Window, Qt::red); // "Window" ist bei Toolbars oft der Hintergrund
   // controlToolbar->setPalette(pal);
   // controlToolbar->setAutoFillBackground(true);

    // controlToolbar->setStyleSheet("QToolBar { background-color:#222222; } QToolButton { color:white; background-color:#222222; }");
    
    controlToolbar->addAction(m_paintControlAction);
    controlToolbar->addAction(m_maskControlAction);
    controlToolbar->addAction(m_lassoControlAction);
    controlToolbar->addAction(m_polygonControlAction);
    controlToolbar->addAction(m_layerControlAction);
    
    // ============================================================
    // create edit toolbar
    // ============================================================
    m_editToolbar = addToolBar(tr("Edit"));
    m_editToolbar->addAction(m_pipetteAction);
    QLabel* pipetteColorLabel = new QLabel(" Color ");
    m_editToolbar->addWidget(pipetteColorLabel);
    QPushButton* pipetteColorButton = new QPushButton();
    pipetteColorButton->setFixedSize(20,20);
     QPixmap whitepixmap(20,20); 
     whitepixmap.fill(Qt::white);
    pipetteColorButton->setIcon(whitepixmap);
    pipetteColorButton->setIconSize(QSize(20,20));
    pipetteColorButton->setFlat(true);
    connect(pipetteColorButton, &QPushButton::clicked, this, [this,pipetteColorButton](){
      QColor c = QColorDialog::getColor(Qt::red,this,"Select Brush Color");
      if( c.isValid() ) {
       m_imageView->setBrushColor(c);
       QPixmap pix(20,20); 
       pix.fill(c); 
       pipetteColorButton->setIcon(pix);
      }
    }); 
    connect(m_imageView, &ImageView::pickedColorChanged, this, [pipetteColorButton](const QColor& c){
     QPixmap pix(20,20); 
     pix.fill(c); 
     pipetteColorButton->setIcon(pix);
    });
    m_editToolbar->addWidget(pipetteColorButton);
    m_editToolbar->addAction(m_paintAction);
    // Brush size
    QLabel* brushLabel = new QLabel(" Brush size:");
    m_editToolbar->addWidget(brushLabel);
    QSpinBox* brushSpin = new QSpinBox();
    brushSpin->setRange(1,50);
    brushSpin->setValue(5);
    m_editToolbar->addWidget(brushSpin);
    connect(brushSpin, QOverload<int>::of(&QSpinBox::valueChanged), m_imageView, &ImageView::setBrushRadius);
    // Brush hardness
    QLabel* hardnessLabel = new QLabel(" Hardness:");
    m_editToolbar->addWidget(hardnessLabel);
    QSlider* hardnessSlider = new QSlider(Qt::Horizontal);
    hardnessSlider->setRange(1,100); // 1..100 %
    hardnessSlider->setValue(100);
    hardnessSlider->setFixedWidth(80);
    m_editToolbar->addWidget(hardnessSlider);
    QLabel* hardnessValueLabel = new QLabel(QString::number(hardnessSlider->value()) + "%");
    m_editToolbar->addWidget(hardnessValueLabel);
    connect(hardnessSlider, &QSlider::valueChanged, m_imageView, [this,hardnessValueLabel](int val){
      m_imageView->setBrushHardness(val/100.0);
      hardnessValueLabel->setText(QString::number(val) + "%");
    });
    
    // ============================================================
    // create layer toolbar
    // ============================================================
    m_layerToolbar = addToolBar(tr("Layer"));
    // --- layer selection ---
    m_selectLayerItem = new QComboBox();
    m_selectLayerItem->addItems({"None yet defined"});
    m_selectLayerItem->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    m_layerToolbar->addWidget(m_selectLayerItem);
    connect(m_selectLayerItem, &QComboBox::currentTextChanged, this, [this](const QString& text){
     std::cout << "MainWindow::createToolbars(): name=" << text.toStdString() << std::endl;
    });
    // --- operation modus ---
    m_transformLayerItem = new QComboBox();
    m_transformLayerItem->addItems({"Translate","Rotate","Scale","Vertical flip","Horizontal flip","Perspective","Cage transform"}); // Perspective
    m_layerToolbar->addWidget(m_transformLayerItem);
    connect(m_transformLayerItem, &QComboBox::currentTextChanged, this, [this](const QString& text){
      LayerItem::OperationMode transformMode = LayerItem::OperationMode::None;
      if ( text.startsWith("Translate") ) transformMode = LayerItem::OperationMode::Translate;
      else if ( text.startsWith("Rotate") ) transformMode = LayerItem::OperationMode::Rotate;
      else if ( text.startsWith("Scale") ) transformMode = LayerItem::OperationMode::Scale;
      else if ( text.startsWith("Vertical") ) transformMode = LayerItem::OperationMode::Flip;
      else if ( text.startsWith("Horizontal") ) transformMode = LayerItem::OperationMode::Flop;
      else if ( text.startsWith("Perspective") ) transformMode = LayerItem::OperationMode::Perspective;
      else if ( text.startsWith("Cage transform") ) transformMode = LayerItem::OperationMode::CageWarp;
      m_imageView->setLayerOperationMode(transformMode);
    });
    // --- cage control points ---
    QLabel* cageControlPointsLabel = new QLabel(" Cage control points:");
    m_layerToolbar->addWidget(cageControlPointsLabel);
    
    QPushButton* btnPlus = new QPushButton("+", this);
    QPushButton* btnMinus = new QPushButton("-", this);
    btnPlus->setStyleSheet("font-size: 20px; font-weight: bold;");
    btnMinus->setStyleSheet("font-size: 20px; font-weight: bold;");
    btnPlus->setFixedSize(18, 18);
    btnMinus->setFixedSize(18, 18);
    m_layerToolbar->addWidget(btnPlus);
    m_layerToolbar->addWidget(btnMinus);
    connect(btnPlus, &QPushButton::clicked, m_imageView, &ImageView::setIncreaseNumberOfCageControlPoints);
    connect(btnMinus, &QPushButton::clicked, m_imageView, &ImageView::setDecreaseNumberOfCageControlPoints);

    m_cageControlPointsSpin = new QSpinBox();
    m_cageControlPointsSpin->setRange(2,30);
    m_cageControlPointsSpin->setValue(1);
    // m_layerToolbar->addWidget(m_cageControlPointsSpin);
    // connect(m_cageControlPointsSpin, QOverload<int>::of(&QSpinBox::valueChanged), m_imageView, &ImageView::setNumberOfCageControlPoints);
    
    
    // --- interpolation ---
    QComboBox* interpolationLayerItem = new QComboBox();
    interpolationLayerItem->addItems({"Nearest neighbor interpolation","Trilinear interpolation"});
    m_layerToolbar->addWidget(interpolationLayerItem);
    // --- relaxation ---
    QLabel* cageRelaxationLabel = new QLabel(" Relaxation:");
    m_layerToolbar->addWidget(cageRelaxationLabel);
    QSpinBox* relaxationSpin = new QSpinBox();
    relaxationSpin->setRange(0,10);
    relaxationSpin->setValue(0);
    m_layerToolbar->addWidget(relaxationSpin);
    connect(relaxationSpin, QOverload<int>::of(&QSpinBox::valueChanged), m_imageView, &ImageView::setCageWarpRelaxationSteps);
    // --- Update for Debugging ---
    QAction *updateAction = new QAction("Update");
    connect(updateAction, &QAction::triggered, this, &MainWindow::forcedUpdate);
    m_layerToolbar->addAction(updateAction);
    
    m_layerToolbar->setVisible(false);
    
    // ============================================================
    // create mask classes toolbar
    // ============================================================
    m_maskToolbar = addToolBar(tr("MaskClasses"));
    m_maskToolbar->setAutoFillBackground(true);
    m_maskToolbar->addAction(m_openMaskImageAction);
    m_maskToolbar->addAction(m_saveMaskImageAction);
    m_maskToolbar->addWidget(QWidgetUtils::getSeparatorLine());
    m_maskToolbar->addAction(m_createMaskImageAction);
    QLabel* maskIndexLabel = new QLabel(" Index:");
    m_maskToolbar->addWidget(maskIndexLabel);
    QComboBox* maskIndexBox = buildDefaultColorComboBox();
    m_maskToolbar->addWidget(maskIndexBox);
    QLabel* classTypeLabel = new QLabel(" Type:");
    m_maskToolbar->addWidget(classTypeLabel);
    QComboBox* applyClassImageItem = new QComboBox();
    applyClassImageItem->addItems({"Ignore mask","Mask labels","Only mask labels","Copy where mask","Inpainting"});
    connect(maskIndexBox, &QComboBox::currentTextChanged, this, [this,applyClassImageItem](const QString& text){
      applyClassImageItem->setCurrentIndex(m_imageView->getMaskCutToolType(text));
      QString numOnly;
      for( auto c : text ) {
       if( c.isDigit() ) numOnly.append(c);
      }
      m_imageView->setMaskLabel(numOnly.toInt());
    });
    connect(applyClassImageItem, &QComboBox::currentTextChanged,this, [this,maskIndexBox](const QString& text){
      QString maskClassName = maskIndexBox->currentText();
      if ( text.startsWith("Ignore") ) {
         m_imageView->setMaskCutTool(maskClassName,ImageView::MaskCutTool::Ignore);
      } else if ( text.startsWith("Mask") ) { 
         m_imageView->setMaskCutTool(maskClassName,ImageView::MaskCutTool::Mask);
      } else if ( text.startsWith("Copy") ) { 
         m_imageView->setMaskCutTool(maskClassName,ImageView::MaskCutTool::Copy);
      } else if ( text.startsWith("Inpainting") ) { 
         m_imageView->setMaskCutTool(maskClassName,ImageView::MaskCutTool::Inpainting);
      } else {
         m_imageView->setMaskCutTool(maskClassName,ImageView::MaskCutTool::OnlyMask);
      }
    });
    m_maskToolbar->addWidget(applyClassImageItem);
    m_maskToolbar->addWidget(QWidgetUtils::getSeparatorLine());
    m_maskToolbar->addAction(m_paintMaskImageAction);
    m_maskToolbar->addAction(m_eraseMaskImageAction);
    QLabel* maskBrushLabel = new QLabel(" Brush size:");
    m_maskToolbar->addWidget(maskBrushLabel);
    QSpinBox* maskBrushSpin = new QSpinBox();
    maskBrushSpin->setRange(1,50);
    maskBrushSpin->setValue(5);
    m_maskToolbar->addWidget(maskBrushSpin);
    connect(maskBrushSpin, QOverload<int>::of(&QSpinBox::valueChanged), m_imageView, &ImageView::setMaskBrushRadius);
    QLabel *maskOpacityLabel = new QLabel("Opacity", this);
    m_maskToolbar->addWidget(maskOpacityLabel);
    QSlider *maskOpacitySlider = new QSlider(Qt::Horizontal, this);
    maskOpacitySlider->setRange(0,100); 
    maskOpacitySlider->setValue(40);  
    maskOpacitySlider->setFixedWidth(100);
    connect(maskOpacitySlider, &QSlider::valueChanged, this, [&](int v){
       m_imageView->setMaskOpacity(v/100.0); // m_maskItem->setOpacityFactor(v / 100.0);
    });
    m_maskToolbar->addWidget(maskOpacitySlider);
    m_maskToolbar->setVisible(false);

    // ============================================================
    // create lasso toolbar
    // ============================================================
    m_lassoToolbar = addToolBar(tr("Lasso"));
    m_lassoToolbar->addAction(m_lassoAction);
    // --- layer mask threshold ---
    QLabel* layerMaskThresholdLabel = new QLabel(" Image threshold:");
    m_lassoToolbar->addWidget(layerMaskThresholdLabel);
    QSpinBox* layerMaskThresholdSpin = new QSpinBox();
    layerMaskThresholdSpin->setRange(0,255);
    layerMaskThresholdSpin->setValue(0);
    m_lassoToolbar->addWidget(layerMaskThresholdSpin);
    // --- Mask image ---
    QComboBox* applyMaskImageItem = new QComboBox();
    applyMaskImageItem->addItems({"Ignore mask","Mask labels","Only mask labels","Copy where mask"});
    connect(applyMaskImageItem, &QComboBox::currentTextChanged,this, [this](const QString& text){
      // std::cout << "applyMaskImageItem(): " << text.toStdString() << std::endl;
      QString name = "Label 0";
      if ( text.startsWith("Ignore") ) {
         m_imageView->setMaskCutTool(name,ImageView::MaskCutTool::Ignore);
      } else if ( text.startsWith("Mask") ) { 
         m_imageView->setMaskCutTool(name,ImageView::MaskCutTool::Mask);
      } else if ( text.startsWith("Copy") ) { 
         m_imageView->setMaskCutTool(name,ImageView::MaskCutTool::Copy);
      } else {
         m_imageView->setMaskCutTool(name,ImageView::MaskCutTool::OnlyMask);
      }
    });
    m_lassoToolbar->addWidget(applyMaskImageItem);
      
    // --- Finalize ---
    m_lassoToolbar->setVisible(false);
    
    // ============================================================
    // create polygon toolbar
    // ============================================================
    m_polygonToolbar = addToolBar(tr("Polygon"));
    m_polygonToolbar->setVisible(false);
    
    m_polygonToolbar->addAction(m_polygonAction);
    
    QLabel* polygonColorLabel = new QLabel("  Index:");
    m_polygonToolbar->addWidget(polygonColorLabel);
    m_polygonIndexBox = buildDefaultColorComboBox("Polygon");
    connect(m_polygonIndexBox, &QComboBox::currentTextChanged, this, [this](const QString& text){
      QString numOnly;
      for( auto c : text ) {
       if( c.isDigit() ) numOnly.append(c);
      }
      m_imageView->setPolygonIndex(numOnly.toInt());
    });
    m_polygonToolbar->addWidget(m_polygonIndexBox);
    
    QLabel* polygonOperationLabel = new QLabel(" Operation mode:");
    m_polygonToolbar->addWidget(polygonOperationLabel);
    m_polygonOperationItem = new QComboBox();
    m_polygonOperationItem->setPlaceholderText("Select operation mode");
    m_polygonOperationItem->setCurrentIndex(-1);
    m_polygonOperationItem->addItems({"Select","Move polygon point","Add new polygon point","Delete polygon point","Translate polygon","Smooth polygon","Reduce polygon","Delete polygon","Information"});  
    m_polygonToolbar->addWidget(m_polygonOperationItem);
    connect(m_polygonOperationItem, &QComboBox::currentTextChanged, this, [this](const QString& text){
      LayerItem::OperationMode polygonOperationMode = LayerItem::OperationMode::Select;
      if ( text.startsWith("Move") ) polygonOperationMode = LayerItem::OperationMode::MovePoint;
      else if ( text.startsWith("Add") ) polygonOperationMode = LayerItem::OperationMode::AddPoint;
      else if ( text.startsWith("Delete polygon point") ) polygonOperationMode = LayerItem::OperationMode::DeletePoint;
      else if ( text.startsWith("Translate") ) polygonOperationMode = LayerItem::OperationMode::TranslatePolygon;
      else if ( text.startsWith("Smooth") ) polygonOperationMode = LayerItem::OperationMode::SmoothPolygon;
      else if ( text.startsWith("Reduce") ) polygonOperationMode = LayerItem::OperationMode::ReducePolygon;
      else if ( text.startsWith("Delete") ) polygonOperationMode = LayerItem::OperationMode::DeletePolygon;
      else if ( text.startsWith("Info") ) polygonOperationMode = LayerItem::OperationMode::Info;
      m_imageView->setPolygonOperationMode(polygonOperationMode);
    });
    
    QAction *polygonUndoAction = new QAction(tr("Undo"), this);
    connect(polygonUndoAction, &QAction::triggered, m_imageView, &ImageView::undoPolygonOperation);
    m_polygonToolbar->addAction(polygonUndoAction);
    QAction *polygonRedoAction =  new QAction(tr("Redo"), this);
    connect(polygonRedoAction, &QAction::triggered, m_imageView, &ImageView::redoPolygonOperation);
    m_polygonToolbar->addAction(polygonRedoAction);
    
    QAction *polygonCreateLayerAction = new QAction(tr("Create new polygon layer"), this);
    connect(polygonCreateLayerAction, &QAction::triggered, m_imageView, &ImageView::createPolygonLayer);
    m_polygonToolbar->addAction(polygonCreateLayerAction);

}

void MainWindow::createStatusbar()
{
	QLabel* scaleLabel = new QLabel(this);
    QLabel* posLabel   = new QLabel(this);
    QLabel* cursorColorLabel = new QLabel(this);
    cursorColorLabel->setFixedSize(24,24);
    QLabel* cursorColorText = new QLabel(this);

    statusBar()->addPermanentWidget(scaleLabel);
    statusBar()->addPermanentWidget(posLabel);
    statusBar()->addPermanentWidget(cursorColorText);
    statusBar()->addPermanentWidget(cursorColorLabel);
    
    connect(m_imageView, &ImageView::scaleChanged, this, [scaleLabel](double scale){
        scaleLabel->setText(QString("Scale: %1×").arg(scale, 0, 'f', 2));
    });

    connect(m_imageView, &ImageView::cursorPositionChanged, this, [posLabel](int x, int y){
        posLabel->setText(QString("|  Pos: %1, %2").arg(x).arg(y));
    });
    
    connect(m_imageView, &ImageView::cursorColorChanged, this, [cursorColorText,cursorColorLabel](const QColor& c){
        QPixmap pix(24,24); 
        pix.fill(c); 
        cursorColorLabel->setPixmap(pix);
        cursorColorText->setText(QString("|  Color: R:%1 G:%2 B:%3").arg(c.red()).arg(c.green()).arg(c.blue()));
        cursorColorLabel->setToolTip(QString("R:%1 G:%2 B:%3 A:%4")
                                 .arg(c.red()).arg(c.green()).arg(c.blue()).arg(c.alpha()));
    });
}

/* =================== Misc =================== */
void MainWindow::info()
{
  qCDebug(logEditor) << "MainWindow::info(): Processing...";
  {
    for ( auto* item : m_imageView->getScene()->items(Qt::DescendingOrder) ) {
      auto* layer = dynamic_cast<LayerItem*>(item);
      if ( layer ) {
        // qDebug() << " Layer " << layer->name() << ", id=" << layer->id();
        layer->printself(true);
      }  
    }
    m_imageView->printself();
  }
}

void MainWindow::setLayerOperationMode( int mode ) 
{
  qDebug() << "MainWindow::setLayerOperationMode(): mode =" << mode;
   m_transformLayerItem->setCurrentIndex(mode-3);
}

void MainWindow::setPolygonOperationMode( int mode ) 
{
   m_polygonOperationItem->setCurrentIndex(mode-10);
}

void MainWindow::setMainOperationMode( MainOperationMode mode )
{
    if ( mode == MainOperationMode::ImageLayer ) {
      m_layerControlAction->setChecked(true);
    } else if ( mode == MainOperationMode::FreeSelection ) {
      m_lassoAction->blockSignals(true);
      m_lassoAction->setChecked(!m_lassoAction->isChecked());
      m_lassoAction->blockSignals(false);
    } else if ( mode == MainOperationMode::CreatePolygon ) {
      m_polygonAction->blockSignals(true);
      m_polygonAction->setChecked(false);
      m_polygonAction->blockSignals(false);
    }
}

int MainWindow::setActivePolygon( const QString& polygonName )
{
  int index = m_polygonIndexBox->findText(polygonName);
  if ( index != -1 ) {
    m_polygonIndexBox->setCurrentIndex(index);
  } 
  return index;
}

/* =================== Signal calls =================== */
void MainWindow::newLassoLayerCreated()
{
  m_lassoAction->setChecked(false);
  rebuildLayerList();
}

/* =================== View Helpers =================== */
void MainWindow::cutSelection() { m_imageView->cutSelection(); }
void MainWindow::zoom1to1() { m_imageView->resetTransform(); }
void MainWindow::fitToWindow() { m_imageView->fitInView(m_layerItem,Qt::KeepAspectRatio); }
void MainWindow::forcedUpdate() { m_imageView->forcedUpdate(); }