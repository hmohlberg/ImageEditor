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

#include "../core/Config.h"

#include <QCoreApplication>
#include <QComboBox>
#include <QMainWindow>
#include <QString>
#include <QUndoView>
#include <QListWidget>
#include <QSpinBox>
#include <QToolBar>

class ImageView;
class LayerItem;

class MainWindow : public QMainWindow
{
    Q_OBJECT
    
public:

    enum MainOperationMode { None, Paint, Mask, FreeSelection, Polygon, ImageLayer, CreateLasso, CreatePolygon };

    // explicit MainWindow( const QString& imagePath = QString(), const QString& historyPath = QString(), 
    //            bool useVulkan = false, QWidget* parent = nullptr );
                
    explicit MainWindow( const QJsonObject& options, QWidget* parent = nullptr );
                
    // --- ---
    void updateLayerList();
    
    // ----------------- global setter and getter -----------------
    ImageView* getViewer() const { return m_imageView; }
    int getNumberOfCageControlPoints() const { return m_cageControlPointsSpin->value(); }
    void setMainOperationMode( MainOperationMode = ImageLayer );
    MainOperationMode getOperationMode() const { return m_operationMode; }
    void setActivePolygon( const QString& polygonName );
    void setLayerOperationMode( int mode );
    void setPolygonOperationMode( int mode );
    
protected:

    void closeEvent( QCloseEvent *event ) override;

private slots:

    void rebuildLayerList();
    void newLassoLayerCreated();
    void toggleLayerVisibility( QListWidgetItem* item );
    void layerItemClicked( QListWidgetItem* item );
    void onLayerItemClicked( QListWidgetItem* item );
    void showLayerContextMenu( const QPoint& pos );
    
    void deleteLayer();
    void duplicateLayer();
    void renameLayer();
    void mergeLayer();
    
    void zoom1to1();
    void fitToWindow();
    void openImage();
    void saveAsImage();
    void openHistory();
    void saveHistory();
    void cutSelection();
    void toggleDocks();
    void createMaskImage();
    void forcedUpdate();
    void info();
    
    void updateButtonState();
    void updateControlButtonState();

private:
    
    bool loadImage( const QString& );
    void loadHistory( const QString& );
    bool saveProject( const QString& );
    bool loadProject( const QString&, bool );
    
    void createDockWidgets();
    void createActions();
    void createToolbars();
    void createStatusbar();
    
    QComboBox* buildDefaultColorComboBox( const QString& name = "Label" );
    
    ImageView* m_imageView = nullptr;
    LayerItem* m_layerItem = nullptr;
    
    QUndoView* m_undoView;
    QDockWidget* m_layerDock;
    QDockWidget* m_historyDock;
    QListWidget* m_layerList;
    
    QToolBar* m_editToolbar = nullptr;
    QToolBar* m_lassoToolbar = nullptr;
    QToolBar* m_layerToolbar = nullptr;
    QToolBar* m_maskToolbar = nullptr;
    QToolBar* m_polygonToolbar = nullptr;
    
    QSpinBox* m_cageControlPointsSpin = nullptr;
    
    MainOperationMode m_operationMode = Paint;
    
    QAction* m_saveHistoryAction = nullptr;
    QAction* m_openHistoryAction = nullptr;
    QAction* m_createMaskImageAction = nullptr;
    QAction* m_openMaskImageAction = nullptr;
    QAction* m_saveMaskImageAction = nullptr;
    QAction* m_paintMaskImageAction = nullptr;
    QAction* m_eraseMaskImageAction = nullptr;
    QAction* m_paintControlAction = nullptr;
    QAction* m_lassoControlAction = nullptr;
    QAction* m_maskControlAction = nullptr;
    QAction* m_layerControlAction = nullptr;
    QAction* m_polygonControlAction = nullptr;
    QAction* m_openAction = nullptr;
    QAction* m_saveAsAction = nullptr;
    QAction* m_cutAction = nullptr;
    QAction* m_pipetteAction = nullptr;
    QAction* m_zoom1to1Action = nullptr;
    QAction* m_fitAction = nullptr;
    QAction* m_crosshairAction = nullptr;
    QAction* m_undoAction = nullptr;
    QAction* m_redoAction = nullptr;
    QAction* m_paintAction = nullptr;
    QAction* m_showDockWidgets = nullptr;
    QAction* m_lassoAction = nullptr;
    QAction* m_polygonAction = nullptr;
    QAction* m_infoAction = nullptr; 
    
    QComboBox* m_polygonIndexBox = nullptr;
    QComboBox* m_transformLayerItem = nullptr;
    QComboBox* m_polygonOperationItem = nullptr;
    
    bool m_updatingLayerList = false;
    bool m_saveImageDataInProjectFile = false;
    
};

