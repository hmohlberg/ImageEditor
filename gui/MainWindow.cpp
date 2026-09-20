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
#include "ConfigDialog.h"
#include "AboutDialog.h"
#include "LayerEditorView.h"
#include "BigTiffViewer.h"
#ifdef HASHDF5
#include "Hdf5Viewer.h"
#endif

#include "../core/ImageLoader.h"
#include "../core/ImageProcessor.h"
#include "../core/Updater.h"

#include "../layer/LayerItem.h"
#include "../layer/Layer.h"
#include "../undo/AbstractCommand.h"
#include "../undo/PaintStrokeCommand.h"
#include "../undo/TransformLayerCommand.h"
#include "../undo/PerspectiveWarpCommand.h"
#include "../undo/DeleteUndoEntryCommand.h"
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
#include <QRegularExpression>
#include <QStyledItemDelegate>
#include <QStandardItemModel>
#include <QSvgRenderer>
#include <QJsonDocument>
#include <QApplication>
#include <QDesktopServices>
#include <QJsonArray>
#include <QMenuBar>
#include <QToolBar>
#include <QStatusBar>
#include <QAction>
#include <QCheckBox>
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
#include <QTimer>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QEventLoop>
#include <QSslConfiguration>
#include <QVBoxLayout>
#include <QDialog>
#include <QDialogButtonBox>
#include <QTabWidget>

#include <iostream>

/* ============================================================
 * Constructor
 * ============================================================ */
MainWindow::MainWindow( const QJsonObject& options, QWidget* parent ) : QMainWindow(parent)
{
  qCDebug(logEditor) << "MainWindow::MainWindow(): Processing...";
  { 
    // connect
    IMainSystem::setInstance(this);
    
    // setup paths
    QString imagePath = options.value("imagePath").toString("");
    QString historyPath = options.value("historyPath").toString("");
    QString outputPath = options.value("outputPath").toString("");
    QString classPath = options.value("classPath").toString("");
    for ( const auto& v : options.value("fileList").toArray() )
        m_fileList << v.toString();
    bool useVulkan = options.value("vulkan").toBool();
    
    Config::gpuCageWarpProcessing = options.value("gpu").toBool();
    Config::skipValidation = options.value("skipValidation").toBool();
    Config::force = options.value("force").toBool();
    Config::verbose = options.value("verbose").toBool();
    
    // setup Gimp style
    // QCoreApplication::setAttribute(Qt::AA_UseStyleSheetPropagationInWidgetStyles);
    QApplication::setStyle("Fusion");
    qApp->setStyleSheet(R"(
      QMainWindow { background-color: #2a2a2a; }
      QWidget { color: #e0e0e0; background-color: #2a2a2a; }
      QWidget:disabled { color: #606060; }
      QToolBar { background-color: #303030; border-bottom: 1px solid #1e1e1e; spacing: 4px; }
      QToolBar::separator { background-color: #a0a0a0; width: 1px; height: 4px; }
      QToolButton { background-color: #3a3a3a; border: 1px solid #1e1e1e; padding: 4px; }
      QToolButton:hover { background-color: #505050; }
      QToolButton:checked { background-color: #6a6a6a; border: 1px solid #909090; font-weight: bold; }
      QToolButton:checked:hover { background-color: #7a7a7a; }
      QToolButton:disabled { background-color: #2a2a2a; color: #959595; border: 1px solid #222222; }
      QStatusBar { background-color: #303030; border-top: 1px solid #1e1e1e; }
      QStatusBar QLabel { color: #e0e0e0; }
      QSlider::groove:horizontal { height: 4px; background: #1e1e1e; }
      QSlider::handle:horizontal { width: 10px; background: #707070; margin: -4px 0; }
      QComboBox { background-color: #3a3a3a; border: 1px solid #1e1e1e; padding: 3px; }
      QComboBox QAbstractItemView { background-color: #2a2a2a; color: #e0e0e0; border: 
               1px solid #1e1e1e; outline: 0; selection-background-color: #505050; selection-color: #ffffff; }
      QComboBox QAbstractItemView::item { padding: 3px 6px; }
      QListWidget::indicator { width: 12px; height: 12px; border: 1px solid #c0c0c0; border-radius: 3px; background: #222222; }
      QListWidget::indicator:unchecked:hover { border-color: #00cc00; }
      QListWidget::indicator:checked:hover { border-color: #cccc00; background-color: #220000; }
      QListWidget::indicator:checked { image: url(":/icons/icons/check.svg"); }
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

    m_layerEditorView = new LayerEditorView(this);
    connect(m_layerEditorView, &LayerEditorView::quitRequested,
            this, [this]{ m_centralStack->setCurrentIndex(0); });
    connect(m_layerEditorView, &LayerEditorView::updateRequested,
            this, &MainWindow::onLayerUpdateRequested);
    connect(m_layerEditorView, &LayerEditorView::scaleChanged, this, [this](double scale) {
        if (m_statusScaleLabel)
            m_statusScaleLabel->setText(QString("Scale: %1×").arg(scale, 0, 'f', 2));
    });
    connect(m_layerEditorView, &LayerEditorView::pickerSampled,
            this, [this](int x, int y, const QColor& c, const QString& text) {
                if (m_statusPosLabel)
                    m_statusPosLabel->setText(QString("|  Pos: %1, %2").arg(x).arg(y));
                if (m_statusColorSwatch) {
                    QPixmap pix(24, 24);
                    pix.fill(c);
                    m_statusColorSwatch->setPixmap(pix);
                    m_statusColorSwatch->setToolTip(text);
                }
                if (m_statusColorText)
                    m_statusColorText->setText(QString("|  Color: %1").arg(text));
            });

    m_bigTiffViewer = new BigTiffViewer(this);
    connect(m_bigTiffViewer, &BigTiffViewer::closeRequested, this, [this]{
        m_bigTiffViewer->closeTiff();
        m_centralStack->setCurrentIndex(0);
    });

    m_centralStack = new QStackedWidget(this);
    m_centralStack->addWidget(m_imageView);       // index 0 — normal view
    m_centralStack->addWidget(m_layerEditorView); // index 1 — layer editor
    m_centralStack->addWidget(m_bigTiffViewer);   // index 2 — BigTIFF viewer
#ifdef HASHDF5
    m_hdf5Viewer = new Hdf5Viewer(this);
    connect(m_hdf5Viewer, &Hdf5Viewer::closeRequested, this, [this]{
        m_hdf5Viewer->closeFile();
        m_centralStack->setCurrentIndex(0);
    });
    m_centralStack->addWidget(m_hdf5Viewer);      // index 3 — HDF5 viewer
#endif
    setCentralWidget(m_centralStack);

    // >>>
    createActions();
    createStatusbar();
    createToolbars();
    createDockWidgets();

    // >>>
    auto loadAnyImage = [this](const QString& path) -> bool {
        const QString ext = QFileInfo(path).suffix().toLower();
        if (ext == "tif" || ext == "tiff") {
            QTimer::singleShot(0, this, [this, path]{ openBigTiff(path); });
            return false;
        }
#ifdef HASHDF5
        if (ext == "h5" || ext == "hdf5" || ext == "hdf") {
            QTimer::singleShot(0, this, [this, path]{ openHdf5(path); });
            return false;
        }
#endif
        return loadImage(path);
    };

    bool hasMainImage = false;
    if ( !imagePath.isEmpty() && historyPath.isEmpty() ) {
     hasMainImage = loadAnyImage(imagePath);
    } else if ( imagePath.isEmpty() && !historyPath.isEmpty() ) {
     hasMainImage = loadProject(historyPath, false);
    } else if ( !imagePath.isEmpty() && !historyPath.isEmpty() ) {
     hasMainImage = loadAnyImage(imagePath);
     loadProject(historyPath, true);
    }
    // override title when the loaded file was a downloaded URL/filelist entry
    {
     const QString dispName = options.value("imageDisplayName").toString("");
     if ( !dispName.isEmpty() ) {
       m_currentDisplayName = dispName;
       setWindowTitle("ImageEditor - " + dispName);
     }
     // BigTIFF/HDF5 load via QTimer::singleShot → loadImage() not called → m_currentDisplayName empty
     if ( m_currentDisplayName.isEmpty() && !imagePath.isEmpty() )
       m_currentDisplayName = imagePath;
     // ensure the current image appears in the filelist tab
     if ( !m_currentDisplayName.isEmpty() && !m_fileList.contains(m_currentDisplayName) )
       m_fileList.prepend(m_currentDisplayName);
    }
    if ( !classPath.isEmpty() ) {
     m_imageView->loadMaskImage(classPath); 
    }
    
    // >>>
    if ( EditorStyle::instance().windowSize() == "maximum" ) {
       this->showMaximized();
    } else if ( EditorStyle::instance().windowSize() == "fullscreen" ) {
       this->showFullScreen();
    } else if ( EditorStyle::instance().windowSize() == "mni" ) {
       // by CLAUDE - select best screen for highest resolution,
       //             full screen mode. fitToWindow doesn't seem
       //             to work with showFullScreen(), so set screen
       //             size and position explicitly.
       int bestScreen = 0;
       for( int i = 0; i < QGuiApplication::screens().size(); i++ ) {
        if( QGuiApplication::screens().at(i) >
           QGuiApplication::screens().at(bestScreen) ) {
         bestScreen = i;
        }
       }
       QScreen *screen = QGuiApplication::screens().at(bestScreen);
       QSize totalSize = screen->size();
       QRect ScreenGeometry = QGuiApplication::screens().at(bestScreen)->geometry();
       move(ScreenGeometry.topLeft());
       resize(totalSize.width(), totalSize.height());
    } else if ( hasMainImage == false) {
       this->setMinimumSize(800, 600);
    }
    show();
    fitToWindow();

    // Check for new releases on GitHub on startup (silent — no dialog when up to date)
    triggerUpdateCheck(true);
  }
}

MainWindow::~MainWindow() {
  qCDebug(logEditor) << "MainWindow::~MainWindow(): begin " << this
             << "instance =" << IMainSystem::instance() << "this as IMainSystem* =" << static_cast<IMainSystem*>(this);
  {
    #ifdef HASITK
     std::cout << "MainWindow::~MainWindow(): Delete all..." << std::endl;
     itk::MultiThreaderBase::SetGlobalDefaultNumberOfThreads(1);
     // delete ui;
    #endif
    if ( IMainSystem::instance() == static_cast<IMainSystem*>(this) ) {
      IMainSystem::setInstance(nullptr); //  this has also result in a crash even on my mac
    }
  }
}

// --- ---
QString MainWindow::mainOperationModeName( int mode )
{
  switch ( mode ) {
   case None:          return "None";
   case Paint:         return "Paint";
   case Mask:          return "Mask";
   case FreeSelection: return "FreeSelection";
   case Polygon:       return "Polygon";
   case ImageLayer:    return "ImageLayer";
   case CreateLasso:   return "CreateLasso";
   case CreatePolygon: return "CreatePolygon";
  }
  return "Unknown";
}

// ---------------------- Catch events ----------------------
bool MainWindow::eventFilter( QObject *obj, QEvent *event )
{
  if ( obj == m_polygonToolbar && event->type() == QEvent::Show ) {
    if ( m_imageView != nullptr ) {
      int polygon_index = m_imageView->getNextFreePolygonIndex();
      if ( polygon_index > 0 ) {
        activePolygon(QString("Polygon %1").arg(polygon_index));
      }
    } 
  }
  return QMainWindow::eventFilter(obj, event);
}

bool MainWindow::checkUnsavedData( bool isCloseProgram  )
{
  qCDebug(logEditor) << "MainWindow::checkUnsavedData(): closeProgram =" << isCloseProgram;
  {
    if ( m_imageView->undoStack()->isClean() )
      return true;
    QString msg = isCloseProgram ? "quit the program" : "continue";
    auto reply = QMessageBox::question( this, "ImageEditor",
                            tr("There are unsaved changes.\nDo you really want to %1?").arg(msg),
                            QMessageBox::Cancel | QMessageBox::No | QMessageBox::Yes,QMessageBox::Yes);
    return (reply == QMessageBox::Yes);
  }
}

void MainWindow::closeEvent( QCloseEvent *event ) 
{
    if ( checkUnsavedData() ) {
      event->accept();
      return;
    }
    event->ignore();
}

/**
bool MainWindow::focusNextPrevChild( bool next ) 
{
    if ( this->focusWidget() == m_layerList ) {
        return false; 
    }
    return QMainWindow::focusNextPrevChild(next);
}
*/

// ---------------------- Load and Save ----------------------
// -> m_imageView->getScene()->addItem(m_layerItem);
bool MainWindow::loadImage( const QString& filePath, bool askForNewLoad )
{
  qCDebug(logEditor) << "MainWindow::loadImage(): filePath =" << filePath << ", askForNewLoad =" << askForNewLoad;
  {
    ImageLoader loader;
    if ( !loader.load(filePath) ) {
      if ( askForNewLoad ) {
        QString updatedPath;
        if ( QWidgetUtils::handleMissingImage(filePath,updatedPath) == true ) {
          if ( loader.load(updatedPath) ) {
            QFileInfo fileInfo(updatedPath);
            m_mainImageName = fileInfo.fileName();
          } else {
            showMessage(QString("Still invalid main image path '%1'. Loading aborted").arg(updatedPath),1);
            return false;
          }
        } else {
          showMessage(QString("Invalid main image path '%1'. Loading aborted").arg(filePath),1);
          return false;
        }
      } else {
        showMessage(QString("Could not load main image %1").arg(filePath),1);
        return false;
      }
    } else {
      QFileInfo fileInfo(filePath);
      m_mainImageName = fileInfo.fileName();
    }
    m_currentDisplayName = filePath;
    setWindowTitle("ImageEditor - " + filePath);
    Config::isWhiteBackgroundImage = loader.hasWhiteBackground();
    auto* scene = m_imageView->getScene();
    scene->clear();
    // main image = regular layer
    m_layerItem = new LayerItem("MainImage",loader.getPixmap());
    m_layerItem->setFileInfo(filePath);
    m_layerItem->setType(LayerItem::MainImage);
    m_layerItem->setParent(this);
    m_layerItem->setZValue(1);
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
  qCDebug(logEditor) << "MainWindow::openImage(): Processing...";
  {
    const bool isMaskImage = sender() == m_openMaskImageAction;
    if ( !isMaskImage ) {
      if ( !checkUnsavedData(false) ) return;
      m_imageView->undoStack()->clear();
      m_imageView->clearLayers();
      rebuildLayerList();
    }

    // helper: parse a .list file
    // - lines starting with # or empty → skip
    // - http/https lines → URL entries
    // - absolute directory path → becomes search path for subsequent relative names
    // - relative filename → resolved against current search path
    // - absolute file path → used as-is
    auto parseLocalFileList = [](const QString& path) -> QStringList {
        QStringList entries;
        QFile f(path);
        if ( !f.open(QIODevice::ReadOnly | QIODevice::Text) ) return entries;
        const QStringList lines = QString::fromUtf8(f.readAll()).split('\n');
        QString searchPath;
        for ( const auto& line : lines ) {
            const QString t = line.trimmed();
            if ( t.isEmpty() || t.startsWith('#') ) continue;
            if ( t.startsWith("http://") || t.startsWith("https://") ) {
                entries << t;
                continue;
            }
            QFileInfo fi(t);
            if ( fi.isAbsolute() ) {
                if ( fi.isDir() )
                    searchPath = t;
                else
                    entries << t;
            } else {
                entries << ( searchPath.isEmpty() ? t : searchPath + "/" + t );
            }
        }
        return entries;
    };

    // --- dialog with three tabs: local disk / web / filelist ---
    QDialog dlg(this);
    dlg.setWindowTitle(isMaskImage ? tr("Open mask image") : tr("Open image"));
    dlg.setMinimumSize(700, 480);

    const QString fileFilter = isMaskImage
        ? tr("Images (*.png *.jpg *.bmp *.tif *.tiff);;All Files (*)")
        : tr("Images (*.png *.jpg *.bmp *.tif *.tiff *.h5 *.hdf5);;File Lists (*.list);;TIFF Images (*.tif *.tiff);;HDF5 Files (*.h5 *.hdf5);;All Files (*)");

    auto* tabs = new QTabWidget(&dlg);

    // Tab 0: embedded file browser (no separate popup)
    auto* localTab = new QWidget;
    auto* localLay = new QVBoxLayout(localTab);
    localLay->setContentsMargins(0, 0, 0, 0);
    auto* fd = new QFileDialog(localTab);
    fd->setWindowFlags(Qt::Widget);
    fd->setOption(QFileDialog::DontUseNativeDialog, true);
    fd->setFileMode(QFileDialog::ExistingFile);
    fd->setNameFilter(fileFilter);
    for ( auto* bb : fd->findChildren<QDialogButtonBox*>() ) bb->hide();
    localLay->addWidget(fd);
    tabs->addTab(localTab, tr("Open from local disk"));

    // Tab 1: web URL
    auto* webTab = new QWidget;
    auto* webLay = new QVBoxLayout(webTab);
    webLay->setContentsMargins(16, 20, 16, 8);
    auto* urlEdit = new QLineEdit(webTab);
    urlEdit->setPlaceholderText(tr("https://example.com/image.png"));
    webLay->addWidget(new QLabel(tr("URL:"), webTab));
    webLay->addWidget(urlEdit);
    webLay->addStretch();
    tabs->addTab(webTab, tr("Open from web"));

    // Tab 2: filelist
    auto* listTab    = new QWidget;
    auto* listLay    = new QVBoxLayout(listTab);
    listLay->setContentsMargins(8, 8, 8, 4);
    auto* listWidget = new QListWidget(listTab);
    for ( const auto& entry : m_fileList ) {
        auto* item = new QListWidgetItem(entry, listWidget);
        if ( entry == m_currentDisplayName ) {
            QFont f = item->font();
            f.setBold(true);
            item->setFont(f);
            item->setForeground(QColor("#5aabff"));
        }
    }
    auto* browseListBtn = new QPushButton(tr("Browse list file…"), listTab);
    listLay->addWidget(listWidget, 1);
    listLay->addWidget(browseListBtn);
    tabs->addTab(listTab, tr("Open from filelist"));

    auto* bbox = new QDialogButtonBox(QDialogButtonBox::Open | QDialogButtonBox::Cancel, &dlg);
    auto* vlay = new QVBoxLayout(&dlg);
    vlay->addWidget(tabs);
    vlay->addWidget(bbox);

    // Tab 0: double-click in embedded browser → accept immediately
    connect(fd, &QFileDialog::fileSelected, &dlg, &QDialog::accept);
    // Tab 2: browse for a .list file and populate the list widget
    connect(browseListBtn, &QPushButton::clicked, [&]() {
        const QString f = QFileDialog::getOpenFileName(this, tr("Open file list"), QString(),
            tr("File lists (*.list);;All Files (*)"));
        if ( f.isEmpty() ) return;
        const QStringList entries = parseLocalFileList(f);
        listWidget->clear();
        for ( const auto& e : entries ) listWidget->addItem(e);
        m_fileList = entries;
    });
    // Tab 2: double-click on entry → accept immediately
    connect(listWidget, &QListWidget::itemDoubleClicked, &dlg, &QDialog::accept);

    connect(bbox, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(bbox, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    if ( dlg.exec() != QDialog::Accepted ) return;

    QString fileName;
    const int activeTab = tabs->currentIndex();
    if ( activeTab == 0 ) {
      const QStringList sel = fd->selectedFiles();
      fileName = sel.isEmpty() ? QString() : sel.first();
      // if a .list file was selected, parse and use first entry
      if ( QFileInfo(fileName).suffix().toLower() == "list" ) {
          const QStringList entries = parseLocalFileList(fileName);
          if ( entries.isEmpty() ) { showMessage(tr("File list is empty."), 1); return; }
          m_fileList = entries;
          listWidget->clear();
          for ( const auto& e : entries ) listWidget->addItem(e);
          fileName = entries.first();
      }
    } else if ( activeTab == 1 ) {
      fileName = urlEdit->text().trimmed();
    } else {
      // Tab 2: filelist — use double-clicked or currently selected item
      auto* item = listWidget->currentItem();
      fileName = item ? item->text() : QString();
      // keep m_fileList in sync with what's in the widget
      m_fileList.clear();
      for ( int i = 0; i < listWidget->count(); ++i )
          m_fileList << listWidget->item(i)->text();
    }
    if ( fileName.isEmpty() ) return;

    // remember original name before possible URL → tempfile replacement
    const QString displayName = fileName;

    // --- URL download ---
    if ( fileName.startsWith("http://") || fileName.startsWith("https://") ) {
      showMessage(tr("Downloading image…"), 0);
      QApplication::setOverrideCursor(Qt::WaitCursor);
      QNetworkAccessManager nam;
      QUrl qurl(fileName);
      QNetworkRequest req(qurl);
      req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                       QNetworkRequest::NoLessSafeRedirectPolicy);
      req.setSslConfiguration(QSslConfiguration::defaultConfiguration());
      QNetworkReply* reply = nam.get(req);
      connect(reply, &QNetworkReply::sslErrors,
              reply, [reply](const QList<QSslError>&) { reply->ignoreSslErrors(); });
      QEventLoop loop;
      connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
      loop.exec();
      QApplication::restoreOverrideCursor();
      if ( reply->error() != QNetworkReply::NoError ) {
        QMessageBox::critical(this, tr("Download failed"), reply->errorString());
        reply->deleteLater();
        return;
      }
      const QByteArray data = reply->readAll();
      reply->deleteLater();
      const QString tempPath = QDir::tempPath() + "/imageeditor_url_download.png";
      QFile f(tempPath);
      if ( !f.open(QIODevice::WriteOnly) ) {
        QMessageBox::critical(this, tr("Error"), tr("Could not write temporary file."));
        return;
      }
      f.write(data);
      f.close();
      fileName = tempPath;
    }

    if ( isMaskImage ) {
      m_imageView->loadMaskImage(fileName);
    } else {
      const QString ext = QFileInfo(fileName).suffix().toLower();
      if ( ext == "tif" || ext == "tiff" ) {
        openBigTiff(fileName);
        m_currentDisplayName = displayName;
        setWindowTitle("ImageEditor - " + displayName);
        if ( !m_fileList.contains(displayName) )
          m_fileList.prepend(displayName);
#ifdef HASHDF5
      } else if ( ext == "h5" || ext == "hdf5" || ext == "hdf" ) {
        openHdf5(fileName);
        m_currentDisplayName = displayName;
        setWindowTitle("ImageEditor - " + displayName);
        if ( !m_fileList.contains(displayName) )
          m_fileList.prepend(displayName);
#endif
      } else {
        if ( loadImage(fileName) ) {
          m_currentDisplayName = displayName;
          setWindowTitle("ImageEditor - " + displayName);
          if ( !m_fileList.contains(displayName) )
            m_fileList.prepend(displayName);
          QTimer::singleShot(0, this, &MainWindow::fitToWindow);
        }
      }
    }
  }
}

void MainWindow::openBigTiff(const QString& filePath)
{
    if (!m_bigTiffViewer->open(filePath)) {
        showMessage(tr("Could not open TIFF file: %1").arg(filePath), 1);
        return;
    }
    m_centralStack->setCurrentIndex(2);

    // Resize window to match image aspect ratio, up to 80% of available screen
    const QSize imgSize = m_bigTiffViewer->imageSize();
    if (imgSize.isValid()) {
        QScreen* screen = QGuiApplication::primaryScreen();
        const QSize available = screen->availableSize() * 0.8;
        const QSize winContent = imgSize.scaled(available, Qt::KeepAspectRatio);
        // account for toolbars and docks that are already laid out
        const int extraH = height() - m_centralStack->height();
        const int extraW = width()  - m_centralStack->width();
        resize(winContent.width() + extraW, winContent.height() + extraH);
    }
}

#ifdef HASHDF5
void MainWindow::openHdf5(const QString& filePath)
{
    if (!m_hdf5Viewer->open(filePath)) {
        showMessage(tr("Could not open HDF5 file: %1").arg(filePath), 1);
        return;
    }
    m_centralStack->setCurrentIndex(3);

    const QSize imgSize = m_hdf5Viewer->imageSize();
    if (imgSize.isValid()) {
        QScreen* screen = QGuiApplication::primaryScreen();
        const QSize available = screen->availableSize() * 0.8;
        const QSize winContent = imgSize.scaled(available, Qt::KeepAspectRatio);
        const int extraH = height() - m_centralStack->height();
        const int extraW = width()  - m_centralStack->width();
        resize(winContent.width() + extraW, winContent.height() + extraH);
    }
}
#endif

void MainWindow::saveAsImage()
{
  qCDebug(logEditor) << "MainWindow::saveAsImage(): Processing...";
  {
    bool isMaskImage = sender() == m_saveMaskImageAction ? true : false;
    QString title = isMaskImage ? QString("Save Mask Image As...") : QString("Save Image As...");
    QString fileName = QFileDialog::getSaveFileName(this,title, QString(),
                        tr("PNG Image Files (*.png);;All Files (*)"),
                        nullptr, QFileDialog::DontUseNativeDialog);
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
          if ( layer && layer->id() != 0 && !layer->isDeleted() ) {
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
    QFileDialog::Options options;
    options |= QFileDialog::DontUseNativeDialog;
    if ( Config::force ) {
     options |= QFileDialog::DontConfirmOverwrite;
    }
    QString fileName = QFileDialog::getSaveFileName(this,tr("Save JSON History File As..."),
                          m_projectFileName,tr("JSON Files (*.json);;All Files (*)"),
                          nullptr,options);
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
    
    // --- Layers ---
    const auto& layers = m_imageView->layers();
    for ( int i = layers.size()-1; i >= 0; --i ) {
        Layer* layer = layers[i];
        if ( !layer || !layer->m_item || layer->m_deleted ) continue;
        layer->printself();
        QJsonObject layerObj;
        layerObj["id"] = layer->id();
        layerObj["name"] = layer->name();
        // Image -> Base64
        if ( layer->id() != 0 ) {
         bool binaryMasking = layer->m_bounds.isNull() ? false : EditorStyle::instance().binaryMasking();
         QByteArray ba;
         QBuffer buffer(&ba);
         buffer.open(QIODevice::WriteOnly);
         if ( binaryMasking ) {
          if ( !layer->m_binaryMask ) {
            QImage alphaImage = layer->m_image.convertToFormat(QImage::Format_Alpha8);
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
           layer->m_image.save(&buffer, "PNG");
          }
         } else {
          layer->m_image.save(&buffer, "PNG");
          binaryMasking = false;
         }
         layerObj["data"] = QString::fromUtf8(ba.toBase64());
         layerObj["binaryMask"] = binaryMasking;
         if ( binaryMasking == true ) {
           layerObj["x"] = layer->m_bounds.x();
           layerObj["y"] = layer->m_bounds.y();
         }
        }
        layerObj["opacity"] = layer->opacity();
        layerObj["creator"] = layer->creator();
        layerArray.append(layerObj);
    }
    root["layers"] = layerArray;

    // --- Undo/Redo Stack ---
    QJsonArray undoArray;
    for ( int i = 0; i < undoStack->count(); ++i ) {
      auto* cmd = dynamic_cast<const AbstractCommand*>(undoStack->command(i));
      if ( !cmd ) continue;
      undoArray.append(cmd->toJson());
    }
    root["undoStack"] = undoArray;

    // --- Write JSON to file ---
    QFile f(filePath);
    if (!f.open(QIODevice::WriteOnly)) return false;
    f.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    f.close();
    
    // --- Set clean flag in undo stack ---
    m_imageView->undoStack()->setClean();
    
    // --- Misc ---
    m_projectFileName = filePath;
    showMessage(QString("Saved project history file '%1'").arg(filePath));
    
    return true;
}

bool MainWindow::loadProject( const QString& filePath, bool skipMainImage )
{
  qCDebug(logEditor) << "MainWindow::loadProject(): filename=" << filePath << ", skipMainImage =" << skipMainImage;
  {
    QFile f(filePath);
    if ( !f.open(QIODevice::ReadOnly) ) return false;
    m_projectFileName = filePath;
    QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    f.close();
    if ( !doc.isObject() ) return false;
    QJsonObject root = doc.object();
    
    // --- Loading and verify main image ---
    QJsonArray layerArray = root["layers"].toArray();
    if ( !skipMainImage ) {
      bool foundMainImage = false;
      for ( const QJsonValue& v : layerArray ) {
        QJsonObject layerObj = v.toObject();
        QString name = layerObj["name"].toString();
        int id = layerObj["id"].toInt();
        if ( id == 0 ) {
          QString filename = layerObj["filename"].toString();
          QString pathname = layerObj["pathname"].toString();
          QString fullfilename = pathname+"/"+filename;
          if ( !loadImage(fullfilename,true) ) {
           showMessage(QString("Cannot find '%1'").arg(fullfilename),1);
           return false;
          }
          foundMainImage = true;
        }
      }
      if ( foundMainImage == false ) {
        showMessage(QString("The main image is missing from the project file. Use the --file option to specify it."),1);
        return false;
      }
    }
    
    // --- Check whether the main input image and the main project image are identical ---
    for ( const QJsonValue& v : layerArray ) {
       QJsonObject layerObj = v.toObject();
       QString name = layerObj["name"].toString();
       int id = layerObj["id"].toInt();
       if ( id == 0 && !Config::skipValidation ) {
          QString filename = layerObj["filename"].toString();
          if ( filename != m_mainImageName ) {
            if ( QWidgetUtils::showImageMismatchError(m_mainImageName,filename) ) {
             QUndoStack* undoStack = m_imageView->undoStack();
             if ( undoStack != nullptr ) undoStack->clear();
             m_imageView->clearLayers();
             return loadProject(filePath,false);
            } else {
             showMessage(QString("Abort loading '%1'").arg(filePath),1);
             // return false;
            }
          }
       }
    }
    
    // --- Clear current scene ---
    QUndoStack* undoStack = m_imageView->undoStack();
    if ( undoStack != nullptr ) undoStack->clear();
    
    // --- Parsing layers (does not contain layer positions) ---
    int nCreatedLayers = 0;
    for ( const QJsonValue& v : layerArray ) {
      QJsonObject layerObj = v.toObject();
      int id = layerObj["id"].toInt();
      if ( id != 0 ) {
        QString name = layerObj["name"].toString();
        if ( layerObj.contains("data") ) {
         QRect rect;
         LayerItem* newLayer = nullptr;
         QString imgBase64 = layerObj["data"].toString();
         QByteArray ba = QByteArray::fromBase64(imgBase64.toUtf8());
         QImage mask;
         mask.loadFromData(ba,"PNG");
         bool isBinaryMask = layerObj.value("binaryMask").toBool(false);
         int x = layerObj.value("x").toInt(-1);
         int y = layerObj.value("y").toInt(-1);
         // fix by Claude to ensure that m_bounds is always defined correctly
         if ( !(  x > 0 && y > 0 ) ) {
           QJsonArray undoArray = root["undoStack"].toArray();
           for ( const QJsonValue& v : undoArray ) {
             QJsonObject cmdObj = v.toObject();
             QString type = cmdObj["type"].toString();
             if ( type == "LassoCut" || type == "LassoCutCommand" ) {
               if( id == cmdObj["newLayerId"].toInt(-1) ) {
                 QJsonObject r = cmdObj["rect"].toObject();
                 x = r["x"].toInt();
                 y = r["y"].toInt();
               }
             }
           }
         }
         rect = QRect(x,y,mask.width(), mask.height());
         // binary masking
         if ( isBinaryMask ) {
           QImage mainImage = m_layerItem->image();
           QImage subImage = mainImage.copy(x, y, mask.width(), mask.height());
           subImage = subImage.convertToFormat(QImage::Format_ARGB32);
           int backgroundPixelColor = Config::isWhiteBackgroundImage ? 255 : 0;
           for ( int y = 0; y < subImage.height(); ++y ) {
            QRgb *rowData = reinterpret_cast<QRgb*>(subImage.scanLine(y));
            const uchar *maskData = mask.constScanLine(y);
            for ( int x = 0; x < subImage.width(); ++x ) {
             // if ( maskData[x] != 255 ) {
             //  rowData[x] = qRgba(qRed(rowData[x]), qGreen(rowData[x]), qBlue(rowData[x]), 0);
             // }
             if (maskData[x] != 255 || qRed(rowData[x]) == backgroundPixelColor) {
               rowData[x] = qRgba(qRed(rowData[x]), qGreen(rowData[x]), qBlue(rowData[x]), 0);
             }
            }
           }
           newLayer = new LayerItem("SubImage",subImage);
         } else if ( EditorStyle::instance().binaryMasking() ) {
            if ( mask.format() != QImage::Format_ARGB32 && mask.format() != QImage::Format_ARGB32_Premultiplied ) {
              mask = mask.convertToFormat(QImage::Format_ARGB32);
            }
            QImage mainImage = m_layerItem->image();
            QImage subImage = mainImage.copy(x, y, mask.width(), mask.height());
            subImage = subImage.convertToFormat(QImage::Format_ARGB32);
            int backgroundPixelColor = Config::isWhiteBackgroundImage ? 255 : 0;
            for ( int y = 0; y < subImage.height(); ++y ) {
             auto *rowData = reinterpret_cast<QRgb*>(subImage.scanLine(y));
             const auto *maskRowData = reinterpret_cast<const QRgb*>(mask.constScanLine(y));  
             for ( int x = 0; x < subImage.width(); ++x ) {
              // OLD: if ( qRed(rowData[x]) == backgroundPixelColor || qAlpha(maskRowData[x]) != 255 ) {
              if ( qRed(rowData[x]) == backgroundPixelColor && qAlpha(maskRowData[x]) == 255 ) {
               rowData[x] = 0; 
              }
             }
            }
            newLayer = new LayerItem("SubImage",subImage);
            isBinaryMask = false;
         } else {
            newLayer = new LayerItem("MaskImage",mask);
            isBinaryMask = false;
         }
         newLayer->setIndex(id);
         newLayer->setParent(this);
         newLayer->setUndoStack(m_imageView->undoStack());
         Layer* layer = new Layer(id,mask);
         layer->m_binaryMask = isBinaryMask;
         layer->m_name = name;
         layer->m_item = newLayer;
         layer->m_bounds = rect;
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
    QHash<int, QRectF> boundingBoxLayerMap;
    QList<EditablePolygonCommand*> editablePolygonCommands;
    EditablePolygonCommand* editablePolyCommand = nullptr;
    QJsonArray undoArray = root["undoStack"].toArray();
    for ( const QJsonValue& v : undoArray ) {
        QJsonObject cmdObj = v.toObject();
        QString type = cmdObj["type"].toString();
        QString text = cmdObj["text"].toString();
        qCDebug(logEditor) << "MainWindow::loadProject(): Found undo call: type=" << type << ", text=" << text;
        AbstractCommand* cmd = nullptr;
        if ( type == "PaintStroke" || type == "PaintStrokeCommand" ) {
           cmd = PaintStrokeCommand::fromJson(cmdObj, layers);
        } else if ( type == "LassoCut" || type == "LassoCutCommand" ) {
           cmd = LassoCutCommand::fromJson(cmdObj, layers);
           LassoCutCommand* cutCommand = dynamic_cast<LassoCutCommand*>(cmd);
           if ( cutCommand != nullptr ) {
             cutCommand->setController(editablePolyCommand);
           }
           boundingBoxLayerMap.insert(cutCommand->layerId(),cutCommand->rect());
        } else if ( type == "MoveLayer" || type == "MoveLayerCommand" ) {
           cmd = MoveLayerCommand::fromJson(cmdObj, layers);
        } else if ( type == "MirrorLayer" || type == "MirrorLayerCommand" ) {
           cmd = MirrorLayerCommand::fromJson(cmdObj, layers);
        } else if ( type == "CageWarp" || type == "CageWarpCommand" ) {
           cmd = CageWarpCommand::fromJson(cmdObj, layers);
        } else if ( type == "TransformLayer" || type == "TransformLayerCommand" ) {
           cmd = TransformLayerCommand::fromJson(cmdObj, layers);
        } else if ( type == "PerspectiveTransform" || type == "PerspectiveTransformCommand" ) {
           // cmd = PerspectiveTransformCommand::fromJson(cmdObj, layers);
        } else if ( type == "PerspectiveWarp" || type == "PerspectiveWarpCommand" ) {
           cmd = PerspectiveWarpCommand::fromJson(cmdObj, layers);
        } else if ( type == "EditablePolygon" || type == "EditablePolygonCommand" ) {
           cmd = EditablePolygonCommand::fromJson(cmdObj, layers);
           editablePolyCommand = dynamic_cast<EditablePolygonCommand*>(cmd);
           int npolygons = m_imageView->pushEditablePolygon(editablePolyCommand->model());
           if ( editablePolyCommand->childLayerId() == -1 || npolygons < 0 ) {
             editablePolygonCommands.push_back(editablePolyCommand);
           }
        } else if ( type == "DeleteUndoEntry" || type == "DeleteUndoEntryCommand" ) {
            cmd = DeleteUndoEntryCommand::fromJson(undoStack, cmdObj, layers);
        } else {
           qDebug() << LogColor::Red << "MainWindow::loadProject(): " << type << " not yet processed."  << LogColor::Reset;
        }
        // ggf. weitere Command-Typen hier hinzufügen
        if ( cmd )
          undoStack->push(cmd);
    }
    
    // --- 4. Correct misnamed polygon - layer couples ---
    if ( !editablePolygonCommands.isEmpty() ) {
      for ( int i = 0; i < editablePolygonCommands.size(); i++ ) {
        QRectF polyBox = editablePolygonCommands[i]->polygon().boundingRect();
        int bestMatchLayerId = -1;
        double smallestArea = std::numeric_limits<double>::max();
        QHashIterator<int, QRectF> ibt(boundingBoxLayerMap);
        while ( ibt.hasNext() ) {
          ibt.next();
          QRectF layerBox = ibt.value();
          if ( layerBox.contains(polyBox) ) {
            double currentArea = layerBox.width() * layerBox.height();
            if ( currentArea < smallestArea ) {
                smallestArea = currentArea;
                bestMatchLayerId = ibt.key();
            }
          }
        }
        if ( bestMatchLayerId != -1 ) {
            editablePolygonCommands[i]->setChildLayerId(bestMatchLayerId);
            editablePolygonCommands[i]->setName(QString("Polygon %1").arg(bestMatchLayerId));
        } else {
            QRectF polyBox = editablePolygonCommands[i]->polygon().boundingRect();
            qDebug() << "WARNING: No layer match found for polygon " << editablePolygonCommands[i]->text() << ": " << polyBox;
            QHashIterator<int, QRectF> ibt(boundingBoxLayerMap);
            while ( ibt.hasNext() ) {
              ibt.next();
              QRectF layerBox = ibt.value();
              qDebug() << " + layer " << ibt.key() << " rect :" << layerBox;
            }
        }
      }
    }
    
    // --- 5. set clean flag in undo stack ---
    m_imageView->undoStack()->setClean();
    
    return true;
  }
}

void MainWindow::loadHistory( const QString& file )
{
  qCDebug(logEditor) << "MainWindow::loadHistory(): filename =" << file;
  {
    QFile f(file);
    if ( !f.open(QIODevice::ReadOnly) )
        return;
    loadProject(file, m_layerItem == nullptr ? false : true ); 
  }
}

void MainWindow::openHistory()
{
  qCDebug(logEditor) << "MainWindow::openHistory(): Open history...";
  {
    QString fileName = QFileDialog::getOpenFileName(this,
                        tr("Open JSON history file"), QString(),
                        tr("JSON Files (*.json);;All Files (*)"));
    if ( fileName.isEmpty() )
      return;
    loadHistory(fileName);
  }
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
  qCDebug(logEditor) << "MainWindow::createDockWidgets(): Processing...";
  {
   // layer dock
   m_layerDock = new QDockWidget("Layers", this);
   m_layerDock->setAllowedAreas(Qt::RightDockWidgetArea);
   m_layerList = new QListWidget(m_layerDock);
   m_layerList->setFocusPolicy(Qt::NoFocus);
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
   connect(m_layerList->model(), &QAbstractItemModel::rowsMoved, this, [this]() {
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
   m_undoView->setFocusPolicy(Qt::NoFocus);
   m_undoView->setContextMenuPolicy(Qt::CustomContextMenu);
   m_undoViewDelegate = new DarkHistoryDelegate(m_undoView);
   m_undoView->setItemDelegate(m_undoViewDelegate);
   //** m_undoViewDelegate->setHighlightedRows({1, 2});
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
   m_historyDock->setFocusPolicy(Qt::NoFocus);
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
    QAction *renameAction = menu.addAction(tr("Rename command"));
    QAction *deleteAction = menu.addAction(tr("Delete command"));
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
    } else if ( selectedAction == deleteAction ) {
        QUndoCommand *cmd = const_cast<QUndoCommand*>(m_imageView->undoStack()->command(index.row() - 1));
        QString labelText = QString("Do you really want to delete the command? Press the Revoke button to undo all operations "
                    "(all follow-up entries will be permanently deleted from the history list) or press Delete to remove "
                    "the command with the option of restoring it.");
        int result = QWidgetUtils::showIconDialog(this,QString("Delete %1").arg(cmd->text()),labelText);
        if ( result == 1 ) {
           // --- sort required so that the requested layer is at the end ---
           int currentIndex = m_imageView->undoStack()->index();
           m_imageView->removeOperationsByIndexUndoStack(cmd->text(),currentIndex-1);
        } else if ( result == 2 ) {
           QUndoStack *undoStack = m_imageView->undoStack();
           if ( undoStack ) {
             QString name = cmd->text();
             int layerId = QWidgetUtils::getLayerIndexFromString(name);
             undoStack->push(new DeleteUndoEntryCommand(undoStack,name,layerId));
           }
        }
    }
   });
 }  
}

void MainWindow::toggleDocks() 
{
  if ( m_historyDock->isVisible() ) m_historyDock->hide();
  else m_historyDock->show();
  if ( m_layerDock->isVisible() ) m_layerDock->hide();
  else m_layerDock->show();
}

// --------------------------------- Layer tools ---------------------------------
void MainWindow::selectLayerItem( const QString &itemName )
{
  qCDebug(logEditor) << "MainWindow::selectLayerItem(): name =" << itemName;
  {
    if ( !itemName.isEmpty() ) {
      // select active layer
      m_selectedLayerItemName = itemName;
      // update layer list
      QList<QListWidgetItem*> items = m_layerList->findItems(itemName, Qt::MatchExactly);
      if ( !items.isEmpty() ) {
        QListWidgetItem* item = items.first();
        {
         const QSignalBlocker blocker(m_layerList);
         m_layerList->setCurrentItem(item);
         item->setSelected(true);
        }
      }
      // update active layer
      for ( Layer* l : m_imageView->layers() ) {
         if ( l->m_item ) {
          LayerItem *layerItem = dynamic_cast<LayerItem*>(l->m_item);
          if ( layerItem != nullptr ) {
            QString name = l->name().section(' ', -2, -1);
            if ( name == itemName ) {
              layerItem->setIsSelected(6,true);
              layerItem->setZValue(3);
            } else {
              layerItem->setIsSelected(7,false);
              layerItem->setZValue(2);
            }
          }
         }
      }
      // misc
      QString numberString = itemName; 
      numberString.remove(QRegularExpression("\\D"));
      m_imageView->setOnlyCageVisible(numberString.toInt());
      showMessage(QString("Select %1").arg(itemName));
    }
  }
}

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
    // Update message
    showMessage(QString("%1 layer %2").arg(layer->m_visible?"Show":"Hide").arg(layer->id()));
  }   
}

void MainWindow::updateLayerList() {
   rebuildLayerList();
}

void MainWindow::rebuildLayerList()
{
  qCDebug(logEditor) << "MainWindow::rebuildLayerList(): Rebuild layer list...";
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
        //item->setIcon(layer->m_visible?m_iconOpen:m_iconClosed);
        //item->setIcon(QIcon(layer->m_visible
        //    ? ":/icons/eye_open.png"
        //    : ":/icons/eye_closed.png"));
        m_layerList->addItem(item);
    }
    m_updatingLayerList = false;
    // updating layer list in m_selectLayerItem WITHOUT sending a signal 
    {
     const QSignalBlocker blocker(m_selectLayerItem);
     int nItems = 0;
     int currentId = m_selectLayerItem->currentData().toInt();
     m_selectLayerItem->clear();
     for ( int i = layers.size()-1; i >= 0; --i ) {
      Layer* layer = layers[i];
      if ( !layer || !layer->m_item || !layer->m_active ) continue;
      m_selectLayerItem->addItem(QString("Layer %1").arg(layer->id()), layer->id());
      nItems += 1;
     }
     // hide all sub toolbars
     hideAllLayerToolbars();
     // next
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
}

// HINT: Is called frequently
void MainWindow::setSelectedLayer( int caller, const QString &name, bool forcedEnabled )
{
  qCDebug(logEditor) << "MainWindow::setSelectedLayer(): caller =" << caller << ", name =" << name;
  {
    int index = -1;
    if ( !name.isEmpty() ) {
      // set layer index
      index = m_selectLayerItem->findText(name);
      if ( index != m_selectLayerItem->currentIndex() ) {
        m_selectLayerItem->setCurrentIndex(index);
      }
    }
    qCDebug(logEditor) << "MainWindow::setSelectedLayer(): index =" << index;
    bool isEnabled = index == -1 ? false : true;
    isEnabled = forcedEnabled ? true : isEnabled;
    m_translateLayerToolbar->setEnabled(isEnabled);
    m_canvasWarpLayerToolbar->setEnabled(isEnabled);
    m_rotateLayerToolbar->setEnabled(isEnabled);
    m_scaleLayerToolbar->setEnabled(isEnabled);
    m_mirrorLayerToolbar->setEnabled(isEnabled);
    m_perspectiveLayerToolbar->setEnabled(isEnabled);
    m_transformLayerItem->setEnabled(isEnabled);
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

// activated if layer item n is selected
void MainWindow::onLayerItemClicked( QListWidgetItem* item )
{
  if ( !item ) return;

  // If layer editor is active, switch it to the newly selected layer
  if (m_centralStack && m_centralStack->currentIndex() == 1) {
      void* ptr = item->data(Qt::UserRole).value<void*>();
      if (Layer* layer = static_cast<Layer*>(ptr))
          editLayer(layer);
      return;
  }

  bool ok = false;
  int layerNumber = item->text().section(' ', 1, 1).toInt(&ok);
  layerNumber = ok ? layerNumber : -1;
  qDebug() << "MainWindow::onLayerItemClicked(): name =" << item->text() << ", number =" << layerNumber;
  {
    int selectedLayerId = -1;
    void* ptr = item->data(Qt::UserRole).value<void*>();
    Layer* layer = static_cast<Layer*>(ptr);
    if ( layer ) {
     for ( Layer* l : m_imageView->layers() ) {
       if ( l->m_item ) {
         LayerItem *layerItem = m_imageView->getLayerItem(l->name());
         if ( layerItem != nullptr ) {
           if ( l == layer ) {
             l->m_item->setSelected(!l->m_item->isSelected());  // toggle
             if( l->m_item->isSelected() ) {
               setSelectedLayer(1,item->text());
               l->m_item->setZValue(3);
             } else {
               setSelectedLayer(2,"");
               l->m_item->setZValue(2);
             }
             m_selectedLayerItemName = l->name();
             item->setSelected(l->m_item->isSelected());
             selectedLayerId = l->m_item->isSelected() ? layerNumber : -1;
             showMessage(QString("%1 layer %2").arg(layer->m_visible?"Select":"De-select").arg(layer->id()));
           } else {
             l->m_item->setSelected(false);
             l->m_item->setZValue(2);
           }
         }
       }
     }
    }
    QSet<int> selectedItemIdents = {};
    if ( selectedLayerId > 0 ) {
      // get all undo entries with layer N
      int polygonIndex = -1;
      // first run
      const QAbstractItemModel* model = m_undoView->model();
      for ( int row = 0; row < model->rowCount(); ++row ) {
        const QModelIndex index = model->index(row, 0);
        const QString text = model->data(index, Qt::DisplayRole).toString();
        // qDebug().noquote() << QString("%1: %2").arg(row).arg(text);
        if ( text.contains(QString("Layer %1").arg(selectedLayerId)) ) {
         QRegularExpression re("Polygon (\\d+)");
         QRegularExpressionMatch match = re.match(text);
         if ( match.hasMatch()) {
           polygonIndex = match.captured(1).toInt();
         }
         selectedItemIdents.insert(row);
        }
      }
      // second run
      if ( polygonIndex > 0 ) {
        model = m_undoView->model();
        for ( int row = 0; row < model->rowCount(); ++row ) {
          const QString text = model->data(model->index(row, 0), Qt::DisplayRole).toString();
          if ( text.contains(QString("Polygon %1").arg(polygonIndex)) ) {
            selectedItemIdents.insert(row);
          }
        }
      }
    }
    m_undoViewDelegate->setHighlightedRows(selectedItemIdents);
    m_undoView->viewport()->update();
  }
}

void MainWindow::showLayerContextMenu( const QPoint& pos )
{
  qCDebug(logEditor) << "MainWindow::showLayerContextMenu(): pos =" << pos;
  {
    QListWidgetItem* item = m_layerList->itemAt(pos);
    if ( !item ) return;
    QMenu menu(this);
    #ifdef _HAS_LAYER_MASKING
     menu.addAction("Mask Layer", this, [this, item]() {
      Layer* layer = static_cast<Layer*>(item->data(Qt::UserRole).value<void*>());
      if ( !layer ) return;
      qCDebug(logEditor) << "MainWindow::showLayerContextMenu(): Masking layer " << layer->name();
     });
    #endif
    menu.addAction("Save Layer as...",  [this, item]() {
        Layer* layer = static_cast<Layer*>(item->data(Qt::UserRole).value<void*>());
        if ( !layer ) return;
        QString fileName = QFileDialog::getSaveFileName(this,
                        tr("Save Layer Image As"),QString(),tr("PNG Image Files (*.png);All Files (*)"),
                        nullptr, QFileDialog::DontUseNativeDialog);
        if ( !fileName.isEmpty() ) {
          layer->image().save(fileName, "PNG", 80);
          qCDebug(logEditor) << "Saved layer " << layer->name() << " image as " << fileName;
          qCDebug(logEditor) << "  geometry: " << layer->m_item->boundingRect();
        }
    });
    menu.addAction("Edit Layer", [this, item]() {
        Layer* layer = static_cast<Layer*>(item->data(Qt::UserRole).value<void*>());
        if ( !layer ) return;
        editLayer(layer);
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
        if ( !layer ) return;
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
}

void MainWindow::deleteLayer()
{
  qCDebug(logEditor) << "MainWindow::deleteLayer(): Processing...";
  {
    QListWidgetItem* item = m_layerList->currentItem();
    if ( !item ) return;
    Layer* layer = static_cast<Layer*>(item->data(Qt::UserRole).value<void*>());
    if ( !layer ) return;
    QString labelText = QString("Do you really want to delete layer %1? Press the Revoke button to undo all operations "
                    "(all entries will be permanently deleted from the history list), press Delete to remove "
                    "the layer with the option of restoring it or press Destroy to remove the layer image").arg(layer->id());
    int result = QWidgetUtils::showIconDialog(this,QString("Delete %1").arg(layer->name()),labelText);
    if ( result == 1 ) { // Revoke
      // Undo all operations from the stack. Preserve original state
      m_imageView->removeOperationsByIdUndoStack(layer->id());
    } else if ( result == 2 ) { // Delete layer
      m_imageView->deleteLayer(layer);
      // if ( layer->m_item ) 
      //   m_imageView->getScene()->removeItem(layer->m_item);
      // m_imageView->layers().removeOne(layer);
      rebuildLayerList();
      m_imageView->rebuildUndoStack();
    } else if ( result == 3 ) { // Destroy layer
      if ( layer->m_item != nullptr ) {
        layer->m_deleted = true;
        LayerItem* m_item = dynamic_cast<LayerItem*>(layer->m_item);
        if ( m_item != nullptr ) {
          m_item->setVisible(false);
          m_item->setInActive(true);
        } 
        rebuildLayerList();
      }
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
  qDebug() << "MainWindow::mergeLayer() NOT YET CODED";
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
  qCDebug(logEditor) << "MainWindow::createActions(): Processing...";
  {
    m_configAction = new QAction(tr("Config"), this);
    connect(m_configAction, &QAction::triggered, this, &MainWindow::showConfig);
    
    m_sortHistoryAction = new QAction(tr("Sort and merge history"), this);
    connect(m_sortHistoryAction, &QAction::triggered, m_imageView, &ImageView::rebuildUndoStack);
    
    m_saveHistoryAction = new QAction(tr("Save history as..."), this);
    connect(m_saveHistoryAction, &QAction::triggered, this, &MainWindow::saveHistory);
    
    m_openHistoryAction = new QAction(tr("Open history file"), this);
    connect(m_openHistoryAction, &QAction::triggered, this, &MainWindow::openHistory);
    
    m_openAction = new QAction(tr("Open"), this);
    connect(m_openAction, &QAction::triggered, this, &MainWindow::openImage);

    m_saveAsAction = new QAction(tr("Save Image As..."), this);
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
    connect(m_polygonAction, &QAction::toggled, this, &MainWindow::updatePolygonEnabledState);
    
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

    // "Check for updates" lives in the macOS application menu (the one named
    // after the app).  ApplicationSpecificRole tells Qt to move the action
    // there automatically; on non-macOS platforms it ends up in the menu below.
    {
        const QString ver = QApplication::applicationVersion();
        QMenu* appMenu = menuBar()->addMenu(
            QString("ImageEditor %1").arg(ver));

        QAction* checkUpdateAction = new QAction(tr("Check for updates…"), this);
        checkUpdateAction->setMenuRole(QAction::ApplicationSpecificRole);
        connect(checkUpdateAction, &QAction::triggered, this, [this](){
            triggerUpdateCheck(false);
        });
        appMenu->addAction(checkUpdateAction);

        QAction* aboutAction = new QAction(tr("About ImageEditor…"), this);
        aboutAction->setMenuRole(QAction::AboutRole);
        connect(aboutAction, &QAction::triggered, this, &MainWindow::showAboutDialog);
        appMenu->addAction(aboutAction);
    }
  }
}

void MainWindow::hideAllLayerToolbars()
{
  qCDebug(logEditor) << "MainWindow::hideAllLayerToolbars(): Processing...";
  {
    m_translateLayerToolbar->setVisible(false);
    m_canvasWarpLayerToolbar->setVisible(false);
    m_mirrorLayerToolbar->setVisible(false);
    m_rotateLayerToolbar->setVisible(false);
    m_scaleLayerToolbar->setVisible(false);
    m_perspectiveLayerToolbar->setVisible(false);
  }
}

void MainWindow::updatePolygonEnabledState( bool isToggled )
{
  // m_polygonIndexBox->setEnabled(!isToggled);
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
    if ( !isE ) {
      hideAllLayerToolbars();
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
  qCDebug(logEditor) << "MainWindow::createToolbars(): Processing...";
  {
    // ============================================================
    // create first toolbar
    // ============================================================
    QToolBar *fileToolbar = new QToolBar("View",this);
    fileToolbar->setStyleSheet("QToolBar { background-color:#222222; } QToolButton { color:white; background-color:#222222; }");
    fileToolbar->setAutoFillBackground(true);
    fileToolbar->setMovable(false);
    fileToolbar->setFloatable(false);
    fileToolbar->setFixedHeight(34);
    addToolBar(Qt::TopToolBarArea,fileToolbar);
    fileToolbar->addAction(m_openAction);
    fileToolbar->addAction(m_saveAsAction);
    fileToolbar->addAction(m_zoom1to1Action);
    fileToolbar->insertSeparator(m_zoom1to1Action);
    fileToolbar->addAction(m_fitAction);
    fileToolbar->addAction(m_crosshairAction);
    // color tables
    QComboBox* colorTableCombo = new QComboBox();
    colorTableCombo->addItems({
        "Original","Invert",
        "Red","Green","Blue","Magenta","Cyan","Yellow",
        "Hot","Cold","Copper",
        "Jet","Viridis","Plasma","Inferno",
        "Nissl","Myelin"
    });
    fileToolbar->addWidget(colorTableCombo);
    connect(colorTableCombo, &QComboBox::currentTextChanged, m_imageView, [this](const QString& text){
       // helper: build LUT by linear interpolation through anchor stops {index, r, g, b}
       struct Stop { int x; int r,g,b; };
       auto ramp = [](std::initializer_list<Stop> stops) {
           QVector<QRgb> lut(256);
           auto it = stops.begin();
           auto nx = std::next(it);
           for (int i = 0; i < 256; ++i) {
               while (nx != stops.end() && nx->x <= i) { it = nx; ++nx; }
               float t = (nx != stops.end() && nx->x > it->x)
                         ? float(i - it->x) / float(nx->x - it->x) : 0.f;
               lut[i] = qRgb(int(it->r + t*(nx->r - it->r)),
                              int(it->g + t*(nx->g - it->g)),
                              int(it->b + t*(nx->b - it->b)));
           }
           return lut;
       };

       QVector<QRgb> lut(256);
       if      (text=="Original") for(int i=0;i<256;i++) lut[i]=qRgb(i,i,i);
       else if (text=="Invert")   for(int i=0;i<256;i++) lut[i]=qRgb(255-i,255-i,255-i);
       else if (text=="Red")      for(int i=0;i<256;i++) lut[i]=qRgb(i,0,0);
       else if (text=="Green")    for(int i=0;i<256;i++) lut[i]=qRgb(0,i,0);
       else if (text=="Blue")     for(int i=0;i<256;i++) lut[i]=qRgb(0,0,i);
       else if (text=="Magenta")  for(int i=0;i<256;i++) lut[i]=qRgb(i,0,i);
       else if (text=="Cyan")     for(int i=0;i<256;i++) lut[i]=qRgb(0,i,i);
       else if (text=="Yellow")   for(int i=0;i<256;i++) lut[i]=qRgb(i,i,0);
       // ── ramp-based LUTs ──────────────────────────────────────────────────
       else if (text=="Hot")    // black → red → yellow → white
           lut = ramp({{0,0,0,0},{85,255,0,0},{170,255,255,0},{255,255,255,255}});
       else if (text=="Cold")   // black → blue → cyan → white
           lut = ramp({{0,0,0,0},{85,0,0,255},{170,0,255,255},{255,255,255,255}});
       else if (text=="Copper") // black → copper/orange → light copper
           lut = ramp({{0,0,0,0},{200,255,127,80},{255,255,200,160}});
       else if (text=="Jet") {  // blue → cyan → green → yellow → red
           for(int i=0;i<256;i++){
               float v=i/255.f;
               float r=qBound(0.f,1.5f-qAbs(4.f*v-3.f),1.f);
               float g=qBound(0.f,1.5f-qAbs(4.f*v-2.f),1.f);
               float b=qBound(0.f,1.5f-qAbs(4.f*v-1.f),1.f);
               lut[i]=qRgb(int(r*255),int(g*255),int(b*255));
           }
       }
       else if (text=="Viridis") // perceptually uniform blue-green-yellow
           lut = ramp({{0,68,1,84},{32,71,41,122},{64,59,82,139},{96,44,113,142},
                       {128,33,144,141},{160,53,183,121},{192,94,201,98},
                       {224,170,220,50},{255,253,231,37}});
       else if (text=="Plasma")  // perceptually uniform purple-red-yellow
           lut = ramp({{0,13,8,135},{32,84,2,163},{64,139,10,165},{96,185,50,137},
                       {128,219,92,104},{160,244,136,73},{192,254,188,43},
                       {224,239,234,30},{255,240,249,33}});
       else if (text=="Inferno") // perceptually uniform black-purple-orange-yellow
           lut = ramp({{0,0,0,4},{32,40,11,84},{64,101,21,110},{96,159,42,99},
                       {128,212,72,66},{160,245,125,21},{192,252,193,44},
                       {224,252,255,164},{255,252,255,164}});
       // ── histology-specific ───────────────────────────────────────────────
       else if (text=="Nissl")  // dark-purple neural cell staining
           // bright region (low gray) = unstained → white/yellow
           // dark region (high gray)  = Nissl substance → deep violet
           lut = ramp({{0,255,255,240},{64,220,200,230},{128,160,120,200},
                       {192,90,40,150},{255,30,0,80}});
       else if (text=="Myelin") // myelin sheath staining (Luxol fast blue style)
           // dark myelin = deep blue/teal, background = cream/white
           lut = ramp({{0,255,252,230},{64,200,230,210},{128,80,180,170},
                       {192,20,90,140},{255,0,30,100}});

       m_imageView->setColorTable(lut);
       m_bigTiffViewer->setColorTable(lut);
#ifdef HASHDF5
       m_hdf5Viewer->setColorTable(lut);
#endif
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
    fileToolbar->addAction(m_configAction);
    fileToolbar->insertSeparator(m_configAction);
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
    controlToolbar->setFixedHeight(34);

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
    brushSpin->setFocusPolicy(Qt::ClickFocus);
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
    connect(m_selectLayerItem,&QComboBox::currentTextChanged,this,&MainWindow::selectLayerItem);
    connect(m_selectLayerItem, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) {
        if (m_centralStack && m_centralStack->currentIndex() == 1) {
            const int id = m_selectLayerItem->currentData().toInt();
            for (Layer* l : m_imageView->layers()) {
                if (l->id() == id) { editLayer(l); return; }
            }
        }
    });

    auto* editorBtn = new QPushButton(tr("Editor"));
    editorBtn->setFixedHeight(24);
    editorBtn->setFocusPolicy(Qt::NoFocus);
    m_layerToolbar->addWidget(editorBtn);
    connect(editorBtn, &QPushButton::clicked, this, [this] {
        const int id = m_selectLayerItem->currentData().toInt();
        for (Layer* l : m_imageView->layers()) {
            if (l->id() == id) { editLayer(l); return; }
        }
    });

    // --- operation modus ---
    m_transformLayerItem = new QComboBox();
    auto *view = new QListView(m_transformLayerItem);
    view->setMouseTracking(true);
    m_transformLayerItem->setView(view);
    m_transformLayerItem->addItems({"Translate","Rotate","Scale","Mirror","Perspective","Cage warp"});
    if ( !EditorStyle::instance().hasPerspective() ) {
      // disable entries (4=Perpective)
      const int perspectiveIndex = m_transformLayerItem->findText("Perspective");
      auto *model = qobject_cast<QStandardItemModel*>(m_transformLayerItem->model());
      if ( model && perspectiveIndex >= 0 ) {
        if ( QStandardItem *item = model->item(perspectiveIndex) ) {
          item->setFlags(item->flags() & ~(Qt::ItemIsEnabled | Qt::ItemIsSelectable));
        }
      }
      m_transformLayerItem->view()->setItemDelegate(
          new ComboItemDelegate(perspectiveIndex, m_transformLayerItem->view()));
    }
    // >>>
    m_layerToolbar->addWidget(m_transformLayerItem);
    connect(m_transformLayerItem, &QComboBox::currentTextChanged, this, [this](const QString& text){
    
      // --- get transform mode ---
      LayerItem::OperationMode transformMode = LayerItem::OperationMode::None;
      if ( text.startsWith("Translate") ) transformMode = LayerItem::OperationMode::Translate;
      else if ( text.startsWith("Rotate") ) transformMode = LayerItem::OperationMode::Rotate;
      else if ( text.startsWith("Scale") ) transformMode = LayerItem::OperationMode::Scale;
      else if ( text.startsWith("Mirror") ) transformMode = LayerItem::OperationMode::Flip;
      else if ( text.startsWith("Perspective") ) transformMode = LayerItem::OperationMode::Perspective;
      else if ( text.startsWith("Cage warp") ) transformMode = LayerItem::OperationMode::CageWarp;
      
      // --- update visibility of layer sub toolbar ---
      bool translateLayerIsVisible = transformMode == LayerItem::OperationMode::Translate ? true : false;
      m_translateLayerToolbar->setVisible(translateLayerIsVisible);
      bool rotateLayerIsVisible = transformMode == LayerItem::OperationMode::Rotate ? true : false;
      m_rotateLayerToolbar->setVisible(rotateLayerIsVisible);
      bool scaleLayerIsVisible = transformMode == LayerItem::OperationMode::Scale ? true : false;
      m_scaleLayerToolbar->setVisible(scaleLayerIsVisible);
      bool cageWrapLayerIsVisible = transformMode == LayerItem::OperationMode::CageWarp ? true : false;
      m_canvasWarpLayerToolbar->setVisible(cageWrapLayerIsVisible);
      bool mirrorLayerIsVisible = transformMode == LayerItem::OperationMode::Flip ? true : false;
      m_mirrorLayerToolbar->setVisible(mirrorLayerIsVisible);
      bool perspectiveLayerIsVisible = transformMode == LayerItem::OperationMode::Perspective ? true : false;
      m_perspectiveLayerToolbar->setVisible(perspectiveLayerIsVisible);
      
      // --- set operation mode ---
      m_imageView->setLayerOperationMode(transformMode);
      
      // --- set visibility of helpers ---
      m_imageView->setOverlayVisibility(1,false);  // hide transform overlay
      m_imageView->setOverlayVisibility(2,false);  // hidemperepective overlay
       
    });
    
    // --- sub toolbar layer translate ---
    m_translateLayerToolbar = addToolBar(tr("TranslateLayer"));
    QLabel* translateXLayerLabel = new QLabel(" XTranslate:");
    m_translateLayerToolbar->addWidget(translateXLayerLabel);
    m_translateXLayerSpin = new QDoubleSpinBox();
    m_translateXLayerSpin->setReadOnly(true);
    m_translateXLayerSpin->setFocusPolicy(Qt::ClickFocus);
    m_translateXLayerSpin->setRange(-100000.0,100000.0);
    m_translateXLayerSpin->setValue(0.0);
    m_translateLayerToolbar->addWidget(m_translateXLayerSpin);
    QLabel* translateYLayerLabel = new QLabel(" YTranslate:");
    m_translateLayerToolbar->addWidget(translateYLayerLabel);
    m_translateYLayerSpin = new QDoubleSpinBox();
    m_translateYLayerSpin->setReadOnly(true);
    m_translateYLayerSpin->setFocusPolicy(Qt::ClickFocus);
    m_translateYLayerSpin->setRange(-100000.0,100000.0);
    m_translateYLayerSpin->setValue(0.0);
    m_translateLayerToolbar->addWidget(m_translateYLayerSpin);
    m_translateLayerToolbar->setVisible(false);
    
    // --- sub toolbar layer scale ---
    m_scaleLayerToolbar = addToolBar(tr("ScaleLayer"));
    QLabel* scaleXLayerLabel = new QLabel(" XScale:");
    m_scaleLayerToolbar->addWidget(scaleXLayerLabel);
    m_scaleXLayerSpin = new QDoubleSpinBox();
    m_scaleXLayerSpin->setReadOnly(true);
    m_scaleXLayerSpin->setFocusPolicy(Qt::ClickFocus);
    m_scaleXLayerSpin->setRange(-100.0,100.0);
    m_scaleXLayerSpin->setValue(1.0);
    m_scaleLayerToolbar->addWidget(m_scaleXLayerSpin);
    QLabel* scaleYLayerLabel = new QLabel(" YScale:");
    m_scaleLayerToolbar->addWidget(scaleYLayerLabel);
    m_scaleYLayerSpin = new QDoubleSpinBox();
    m_scaleYLayerSpin->setReadOnly(true);
    m_scaleYLayerSpin->setFocusPolicy(Qt::ClickFocus);
    m_scaleYLayerSpin->setRange(-100.0,100.0);
    m_scaleYLayerSpin->setValue(1.0);
    m_scaleLayerToolbar->addWidget(m_scaleYLayerSpin);
    QPushButton *resetScaleAction = new QPushButton("Reset");
    resetScaleAction->setFocusPolicy(Qt::ClickFocus);
    connect(resetScaleAction, &QPushButton::clicked, this, [this]() {
      LayerItem *layerItem = m_imageView->getLayerItem(m_selectLayerItem->currentText());
      if ( layerItem != nullptr ) layerItem->scale(1.0,1.0);
    });
    m_scaleLayerToolbar->addWidget(resetScaleAction);
    m_scaleLayerToolbar->setVisible(false);
    
    // --- sub toolbar layer mirror ---
    m_mirrorLayerToolbar = addToolBar(tr("MirrorLayer"));
    QLabel* mirrorDirectionLabel = new QLabel(" Direction:");
    m_mirrorLayerToolbar->addWidget(mirrorDirectionLabel);
    m_mirrorDirectionCombo = new QComboBox();
    m_mirrorDirectionCombo->addItems({"Horizontal","Vertical"});
    m_mirrorDirectionCombo->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    m_mirrorLayerToolbar->addWidget(m_mirrorDirectionCombo);
    QPushButton *doMirrorAction = new QPushButton("Apply");
    doMirrorAction->setFocusPolicy(Qt::ClickFocus);
    connect(doMirrorAction, &QPushButton::clicked, this, [this]() {
      LayerItem *layerItem = m_imageView->getLayerItem(m_selectLayerItem->currentText());
      if ( layerItem != nullptr ) layerItem->mirror(m_mirrorDirectionCombo->currentText() == "Vertical" ? 1 : 2);
    });
    m_mirrorLayerToolbar->addWidget(doMirrorAction);
    m_mirrorLayerToolbar->setVisible(false);
    
    // --- sub toolbar layer rotate ---
    m_rotateLayerToolbar = addToolBar(tr("RotateLayer"));
    QLabel* rotationLayerAngleLabel = new QLabel(" Angle:");
    m_rotateLayerToolbar->addWidget(rotationLayerAngleLabel);
    m_rotationLayerAngleSpin = new QDoubleSpinBox();
    m_rotationLayerAngleSpin->setFocusPolicy(Qt::ClickFocus);
    m_rotationLayerAngleSpin->setRange(-180.0,180.0);
    m_rotationLayerAngleSpin->setSingleStep(EditorStyle::instance().rotationSingleStep());
    m_rotationLayerAngleSpin->setValue(0.0);
    m_rotationLayerAngleSpin->setWrapping(true);
    connect(m_rotationLayerAngleSpin, &QDoubleSpinBox::valueChanged, m_imageView, [this](double value){
      LayerItem *layer = m_imageView->getSelectedItem();
      if ( layer ) {
        double currentAngle = layer->getRotationAngle();
        layer->setRotationAngle(value-currentAngle);
      } else showMessage("Rotation failed. No image layer selected.",1);
    });
    m_rotateLayerToolbar->addWidget(m_rotationLayerAngleSpin);
    m_rotateLayerToolbar->setVisible(false);
    
    // --- sub toolbar layer perspective ---
    m_perspectiveLayerToolbar = addToolBar(tr("PerspectiveLayer"));
    QPushButton *resetPerspectiveAction = new QPushButton("Reset");
    resetPerspectiveAction->setFocusPolicy(Qt::ClickFocus);
    connect(resetPerspectiveAction, &QPushButton::clicked, this, [this]() {
      if ( m_imageView ) {
        m_imageView->setPerspectiveWarpReset();
      }
    });
    m_perspectiveLayerToolbar->addWidget(resetPerspectiveAction);
    m_perspectiveLayerToolbar->setVisible(false);
    
    // --- sub toolbar layer cage control ---
    m_canvasWarpLayerToolbar = addToolBar(tr("CanvasWarpLayer"));
    // --- cage control points ---
    QLabel* cageControlPointsLabel = new QLabel(" Cage control points:");
    m_canvasWarpLayerToolbar->addWidget(cageControlPointsLabel);
    QPushButton* btnPlus = new QPushButton("+", this);
    btnPlus->setFocusPolicy(Qt::ClickFocus);
    QPushButton* btnMinus = new QPushButton("-", this);
    btnMinus->setFocusPolicy(Qt::ClickFocus);
    btnPlus->setStyleSheet("font-size: 20px; font-weight: bold;");
    btnMinus->setStyleSheet("font-size: 20px; font-weight: bold;");
    btnPlus->setFixedSize(18, 18);
    btnMinus->setFixedSize(18, 18);
    m_canvasWarpLayerToolbar->addWidget(btnPlus);
    m_canvasWarpLayerToolbar->addWidget(btnMinus);
    connect(btnPlus, &QPushButton::clicked, m_imageView, &ImageView::setIncreaseNumberOfCageControlPoints);
    connect(btnMinus, &QPushButton::clicked, m_imageView, &ImageView::setDecreaseNumberOfCageControlPoints);
    // --- fix boundary ---
    QCheckBox* fixBoundaryCheck = new QCheckBox("Fix boundary", this);
    fixBoundaryCheck->setFocusPolicy(Qt::ClickFocus);
    fixBoundaryCheck->setToolTip("Fixed outer mesh warp cage boundaries.");
    fixBoundaryCheck->setChecked(true);
    m_canvasWarpLayerToolbar->addWidget(fixBoundaryCheck);
    connect(fixBoundaryCheck, &QCheckBox::toggled, m_imageView, &ImageView::setCageWarpFixBoundary);
    // --- relaxation ---
    QLabel* cageRelaxationLabel = new QLabel(" Relaxation:");
    m_canvasWarpLayerToolbar->addWidget(cageRelaxationLabel);
    QSpinBox* relaxationSpin = new QSpinBox();
    relaxationSpin->setFocusPolicy(Qt::ClickFocus);
    relaxationSpin->setRange(0,100);
    relaxationSpin->setValue(0);
    m_canvasWarpLayerToolbar->addWidget(relaxationSpin);
    connect(relaxationSpin, QOverload<int>::of(&QSpinBox::valueChanged), m_imageView, &ImageView::setCageWarpRelaxationSteps);
    // --- stiffness ---
    QLabel* cageStiffnessLabel = new QLabel(" Stifness:");
    m_canvasWarpLayerToolbar->addWidget(cageStiffnessLabel);
    QDoubleSpinBox* stiffnessSpin = new QDoubleSpinBox();
    stiffnessSpin->setFocusPolicy(Qt::ClickFocus);
    stiffnessSpin->setRange(0.0,3.0);
    stiffnessSpin->setSingleStep(0.05);
    stiffnessSpin->setValue(0.0);
    m_canvasWarpLayerToolbar->addWidget(stiffnessSpin);
    connect(stiffnessSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), m_imageView, &ImageView::setCageWarpStiffness);
    // --- Reset cage ---
    QPushButton *resetCageAction = new QPushButton("Reset");
    resetCageAction->setFocusPolicy(Qt::ClickFocus);
    connect(resetCageAction, &QPushButton::clicked, m_imageView, &ImageView::setCageWarpReset);
    m_canvasWarpLayerToolbar->addWidget(resetCageAction);
    // --- Update for Debugging ---
    // QPushButton *updateAction = new QPushButton("Update");
    // connect(updateAction, &QPushButton::clicked, this, &MainWindow::forcedUpdate);
    // m_canvasWarpLayerToolbar->addWidget(updateAction);
    // --- add control group to toolbar
    m_canvasWarpLayerToolbar->setVisible(false);
    
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
    connect(applyClassImageItem, &QComboBox::currentTextChanged, this, [this,maskIndexBox](const QString& text){
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
    maskBrushSpin->setFocusPolicy(Qt::ClickFocus);
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
    layerMaskThresholdSpin->setFocusPolicy(Qt::ClickFocus);
    layerMaskThresholdSpin->setRange(0,255);
    layerMaskThresholdSpin->setValue(0);
    m_lassoToolbar->addWidget(layerMaskThresholdSpin);
    // --- Mask image ---
    QComboBox* applyMaskImageItem = new QComboBox();
    applyMaskImageItem->addItems({"Ignore mask","Mask labels","Only mask labels","Copy where mask"});
    connect(applyMaskImageItem, &QComboBox::currentTextChanged, this, [this](const QString& text){
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
    m_polygonToolbar->installEventFilter(this);
    m_polygonToolbar->setVisible(false);
    
    // --- Polygon selection stuff ---
    QLabel* polygonColorLabel = new QLabel("  Index:");
    m_polygonToolbar->addWidget(polygonColorLabel);
    m_polygonIndexBox = buildDefaultColorComboBox("Polygon");
    connect(m_polygonIndexBox, &QComboBox::currentTextChanged, this, [this](const QString& text){
      qCDebug(logEditor) << "MainWindow::createToolbars(): New polygon index name =" << text;
      QString numOnly;
      for( auto c : text ) {
       if( c.isDigit() ) numOnly.append(c);
      }
      m_imageView->setPolygonIndex(numOnly.toInt(),true);
    });
    m_polygonToolbar->addWidget(m_polygonIndexBox);
    
    m_polygonToolbar->addAction(m_polygonAction);
    
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
    
    m_polygonCreateLayerAction = new QAction(tr("Create new polygon layer"), this);
    connect(m_polygonCreateLayerAction, &QAction::triggered, m_imageView, &ImageView::createPolygonLayer);
    connect(m_imageView, &ImageView::polygonHasLayer, this, [this](bool hasLayer){
        m_polygonCreateLayerAction->setText(
            hasLayer ? tr("Update polygon layer") : tr("Create new polygon layer"));
        if ( !hasLayer )
            m_polygonCreateLayerAction->setEnabled(true);
    });
    connect(m_imageView, &ImageView::polygonNeedsUpdate, this, [this](bool needsUpdate){
        m_polygonCreateLayerAction->setEnabled(needsUpdate);
    });
    m_polygonToolbar->addAction(m_polygonCreateLayerAction);
    
  }
}

void MainWindow::showMessage( const QString& message, int msgType )
{
  qCDebug(logEditor) << "MainWindow::showMessage(): message =" << message;
  {
    if ( m_messageLabel != nullptr ) {
      if ( msgType == 1 ) {
        m_messageLabel->setStyleSheet("QLabel { color : red; font-weight: bold; }");
        m_messageLabel->setText(QString("ERROR: %1").arg(message));
      } else {
        m_messageLabel->setStyleSheet("QLabel { color : yellow; font-weight: normal; }");
        m_messageLabel->setText(message);
      }
      // QTimer::singleShot(5000, m_messageLabel, &QLabel::clear);
    }
  }
}

void MainWindow::createStatusbar()
{
  qCDebug(logEditor) << "MainWindow::createStatusbar(): Processing...";
  {
    m_messageLabel = new QLabel("Ready", this);
    m_messageLabel->setFrameStyle(QFrame::NoFrame);
    statusBar()->insertWidget(0, m_messageLabel);
    
    m_statusScaleLabel  = new QLabel(this);
    m_statusPosLabel    = new QLabel(this);
    m_statusColorSwatch = new QLabel(this);
    m_statusColorSwatch->setFixedSize(24, 24);
    m_statusColorText   = new QLabel(this);

    statusBar()->addPermanentWidget(m_statusScaleLabel);
    statusBar()->addPermanentWidget(m_statusPosLabel);
    statusBar()->addPermanentWidget(m_statusColorText);
    statusBar()->addPermanentWidget(m_statusColorSwatch);

    connect(m_imageView, &ImageView::scaleChanged, this, [this](double scale){
        m_statusScaleLabel->setText(QString("Scale: %1×").arg(scale, 0, 'f', 2));
    });

    connect(m_imageView, &ImageView::cursorPositionChanged, this, [this](int x, int y){
        m_statusPosLabel->setText(QString("|  Pos: %1, %2").arg(x).arg(y));
    });

    connect(m_imageView, &ImageView::cursorColorChanged, this, [this](const QColor& c){
        QPixmap pix(24, 24);
        pix.fill(c);
        m_statusColorSwatch->setPixmap(pix);
        m_statusColorText->setText(QString("|  Color: R:%1 G:%2 B:%3").arg(c.red()).arg(c.green()).arg(c.blue()));
        m_statusColorSwatch->setToolTip(QString("R:%1 G:%2 B:%3 A:%4")
                                        .arg(c.red()).arg(c.green()).arg(c.blue()).arg(c.alpha()));
    });
  }
}

/* =================== Misc =================== */

void MainWindow::updateLayerOperationParameter( const QString &aCaller, const QString &layerName, int mode, double value1, double value2 )
{
  qCDebug(logEditor) << "MainWindow::updateLayerOperationParameter(" << aCaller << "): layerName =" << layerName << ", layerOpMode =" << LayerItem::operationModeName(mode) 
                 << ", mainOpMode =" << mainOperationModeName(m_operationMode) << ", value =" << value1;
  {
    if ( m_operationMode == MainOperationMode::ImageLayer ) {
     if ( mode == LayerItem::OperationMode::Rotate ) {
      m_rotationLayerAngleSpin->blockSignals(true);
      m_rotationLayerAngleSpin->setValue(value1);
      m_rotationLayerAngleSpin->blockSignals(false);
     } else if ( mode == LayerItem::OperationMode::Translate ) {
      m_translateXLayerSpin->blockSignals(true);
      m_translateXLayerSpin->setValue(value1);
      m_translateXLayerSpin->blockSignals(false);
      m_translateYLayerSpin->blockSignals(true);
      m_translateYLayerSpin->setValue(value2);
      m_translateYLayerSpin->blockSignals(false);
     } else if ( mode == LayerItem::OperationMode::Scale ) {
      m_scaleXLayerSpin->blockSignals(true);
      m_scaleXLayerSpin->setValue(value1);
      m_scaleXLayerSpin->blockSignals(false);
      m_scaleYLayerSpin->blockSignals(true);
      m_scaleYLayerSpin->setValue(value2);
      m_scaleYLayerSpin->blockSignals(false);
     } else if ( mode == LayerItem::OperationMode::Flip ) {
      m_mirrorDirectionCombo->setCurrentIndex(0);
     } else if ( mode == LayerItem::OperationMode::Flop ) {
      m_mirrorDirectionCombo->setCurrentIndex(1);
     }
    }
  }
}

double MainWindow::getLayerOperationParameter( int mode )
{
  if ( mode == LayerItem::OperationMode::Rotate ) {
    return  m_rotationLayerAngleSpin->value();
  } else if ( mode == LayerItem::OperationMode::Flip ) {
    return m_mirrorDirectionCombo->currentIndex() == 1 ? +1 : -1;
  }
  return 0.0;
}

void MainWindow::showConfig()
{
  qCDebug(logEditor) << "MainWindow::showConfig(): Processing...";
  ConfigDialog dlg(this);
  dlg.exec();
}

void MainWindow::editLayer(Layer* layer)
{
  if (m_centralStack->currentIndex() == 1 && m_layerEditorView->isModified()) {
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
    if (dlg.clickedButton() == cancelBtn) return;
    if (dlg.clickedButton() == yesBtn)
        onLayerUpdateRequested(m_layerEditorView->layerImage());
  }

  m_editingLayer = layer;

  QImage layerImg;
  if (layer->m_item) {
      if (auto* li = dynamic_cast<LayerItem*>(layer->m_item))
          layerImg = li->image();
      else if (auto* pi = dynamic_cast<QGraphicsPixmapItem*>(layer->m_item))
          layerImg = pi->pixmap().toImage();
  }
  if (layerImg.isNull())
      layerImg = layer->image();

  const QImage mainImg = m_layerItem ? m_layerItem->image() : QImage();
  const QPoint origin  = layer->m_item ? layer->m_item->pos().toPoint() : QPoint();
  const bool preserveMode = (m_centralStack->currentIndex() == 1);
  m_layerEditorView->setImages(layerImg, mainImg, origin, preserveMode);
  m_centralStack->setCurrentIndex(1);
}

void MainWindow::onLayerUpdateRequested(const QImage& modifiedImage)
{
  if (!m_editingLayer || !m_editingLayer->m_item) return;
  auto* li = dynamic_cast<LayerItem*>(m_editingLayer->m_item);
  if (!li) return;

  li->setImage(modifiedImage);
  li->updatePixmap();
  m_imageView->update();
}

void MainWindow::setLayerOperationMode( int mode, bool updateMode ) 
{
  qCDebug(logEditor) << "MainWindow::setLayerOperationMode(): layerOpMode =" << LayerItem::operationModeName(mode) << ", mainOpMode =" 
                  << mainOperationModeName(m_operationMode) << ", update =" << updateMode;
  {
    if ( m_operationMode != MainOperationMode::ImageLayer ) 
      return;
    if ( updateMode == false ) { 
      m_transformLayerItem->blockSignals(true);
      hideAllLayerToolbars();
    }
    switch ( mode ) {
      case LayerItem::OperationMode::Translate:   
         m_transformLayerItem->setCurrentIndex(0); 
         m_translateLayerToolbar->setVisible(true);
         break;
      case LayerItem::OperationMode::Rotate:      
         m_transformLayerItem->setCurrentIndex(1);  
         m_rotateLayerToolbar->setVisible(true);
         break;
      case LayerItem::OperationMode::Scale:       
         m_transformLayerItem->setCurrentIndex(2); 
         m_scaleLayerToolbar->setVisible(true); 
         break;
      case LayerItem::OperationMode::Flip:
      case LayerItem::OperationMode::Flop:       
         m_transformLayerItem->setCurrentIndex(3); 
         m_mirrorLayerToolbar->setVisible(true);
         break;
      case LayerItem::OperationMode::Perspective: 
         m_transformLayerItem->setCurrentIndex(4);  
         m_perspectiveLayerToolbar->setVisible(true);
         break;
      case LayerItem::OperationMode::CageWarp:    
         m_transformLayerItem->setCurrentIndex(5);  
         m_canvasWarpLayerToolbar->setVisible(true);
         break;
    }
    if ( updateMode == false ) { 
      m_transformLayerItem->blockSignals(false);
    }
  }
}

void MainWindow::setPolygonOperationMode( int mode ) 
{
  qCDebug(logEditor) << "MainWindow::setPolygonOperationMode(): mode =" << mode;
  {
    if ( mode == -1 ) {
     m_polygonCreateLayerAction->setEnabled(false);
     m_polygonAction->setEnabled(true);
    } else if ( mode == -2 ) {
     m_polygonCreateLayerAction->setEnabled(true);
     m_polygonAction->setEnabled(false);
    } else {
     m_polygonOperationItem->setCurrentIndex(mode-10);
    }
  }
}

void MainWindow::setMainOperationMode( MainOperationMode mode )
{
  qCDebug(logEditor) << "MainWindow::setMainOperationMode(): mode =" << mode;
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
    } else if ( mode == MainOperationMode::Polygon ) {
      m_polygonControlAction->setChecked(true);
    }
  }
}

int MainWindow::activePolygon( const QString& polygonName )
{
  qCDebug(logEditor) << "MainWindow::activePolygon(): polygonName =" << polygonName;
  {
   if ( polygonName.isEmpty() ) {
     return m_polygonIndexBox->currentText().section(' ',1).toInt();;
   }
   int index = m_polygonIndexBox->findText(polygonName);
   if ( index != -1 ) {
    m_polygonIndexBox->setCurrentIndex(index);
   } 
   return index;
  }
}

QString MainWindow::getSelectedLayerItemName() 
{ 
  return m_selectedLayerItemName; 
}

/* =================== Signal calls =================== */

void MainWindow::newLassoLayerCreated()
{
  m_lassoAction->setChecked(false);
  rebuildLayerList();
}

/* =================== View Helpers =================== */

void MainWindow::zoom1to1() { m_imageView->resetTransform(); }
void MainWindow::forcedUpdate() { m_imageView->forcedUpdate(); }

void MainWindow::fitToWindow() {
  if ( m_layerItem != nullptr ) {
     m_imageView->fitInView(m_layerItem,Qt::KeepAspectRatio);
  }
}

/* =================== About Dialog =================== */

void MainWindow::showAboutDialog()
{
    AboutDialog dlg(this);
    dlg.exec();
}

/* =================== Update Checker =================== */

void MainWindow::triggerUpdateCheck(bool silent)
{
    m_silentUpdateCheck = silent;
    if ( !m_updater ) {
        m_updater = new Updater(QApplication::applicationVersion(), this);
        connect(m_updater, &Updater::updateAvailable, this,
                [this](const QString& ver, const QString& url){
            showUpdateAvailableDialog(ver, url);
        });
        connect(m_updater, &Updater::noUpdateAvailable, this, [this](){
            if ( !m_silentUpdateCheck ) {
                const QString ver   = QApplication::applicationVersion();
                const QString build = QString("%1 at %2").arg(__DATE__, __TIME__);
                QMessageBox::information(this, tr("Software up to date"),
                    tr("Your application (Version %1, Build %2) is up to date, "
                       "and no new updates are available.")
                    .arg(ver, build));
            }
        });
        connect(m_updater, &Updater::localVersionNewer, this,
                [this](const QString& localVer, const QString& githubVer){
            if ( !m_silentUpdateCheck ) {
                QMessageBox::warning(this, tr("GitHub repository outdated"),
                    tr("Your installed version (%1) is newer than the latest release "
                       "on GitHub (%2). The GitHub repository is not up to date and "
                       "requires an update.")
                    .arg(localVer, githubVer));
            }
        });
        connect(m_updater, &Updater::checkFailed, this, [this](const QString& err){
            if ( !m_silentUpdateCheck ) {
                QMessageBox::warning(this, tr("Update check failed"), err);
            }
        });
    }
    m_updater->checkForUpdates();
}

void MainWindow::showUpdateAvailableDialog(const QString& newVersion,
                                            const QString& releaseUrl)
{
    const QString installed = QApplication::applicationVersion();
    const QString text = tr("Program is outdated. Latest version in GitHub repository is %1, "
                            "installed version is %2.").arg(newVersion, installed);

    QMessageBox box(this);
    box.setWindowTitle(tr("Update available"));
    box.setIcon(QMessageBox::Information);
    box.setText(text);

    QPushButton* skipBtn   = box.addButton(tr("Skip"),   QMessageBox::RejectRole);
    QPushButton* updateBtn = box.addButton(tr("Update"), QMessageBox::AcceptRole);
    Q_UNUSED(skipBtn)

    box.exec();

    if ( box.clickedButton() == updateBtn )
        QDesktopServices::openUrl(QUrl(releaseUrl));
}
