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
#include "../core/IMainSystem.h"

#include <QCoreApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QMainWindow>
#include <QString>
#include <QUndoView>
#include <QListWidget>
#include <QSpinBox>
#include <QToolBar>
#include <QLabel>

class ImageView;
class LayerItem;

class MainWindow : public QMainWindow, public IMainSystem
{
    Q_OBJECT
    
 public:

    enum MainOperationMode { None, Paint, Mask, FreeSelection, Polygon, ImageLayer, CreateLasso, CreatePolygon };
    
    static QString mainOperationModeName( int mode );
                
    explicit MainWindow( const QJsonObject& options, QWidget* parent = nullptr );
    ~MainWindow();
                
    // --- ---
    void updateLayerList();
    
    // --- global setter and getter ---
    ImageView* getViewer() const { return m_imageView; }
    void setMainOperationMode( MainOperationMode = ImageLayer );
    MainOperationMode getOperationMode() const { return m_operationMode; }
    int activePolygon( const QString& polygonName );
    void setLayerOperationMode( int mode, bool updateMode = true );
    void setPolygonOperationMode( int mode );
    void setSelectedLayer( const QString &name );
    
    // --- override methods ---
    void updateLayerOperationParameter( int mode, double value1, double value2 = 0.0 ) override;
    void showMessage( const QString& message, int msgType=0 ) override;
    double getLayerOperationParameter( int mode ) override;
    QString getSelectedLayerItemName() override;
    
 protected:

    void closeEvent( QCloseEvent *event ) override;
    // bool focusNextPrevChild( bool next ) override;

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
    void toggleDocks();
    void createMaskImage();
    void forcedUpdate();
    void info();
    
    void updateButtonState();
    void updateControlButtonState();
    void updatePolygonEnabledState( bool isToggled );

 private:
    
    bool checkUnsavedData( bool isCloseProgram = true );
    bool loadImage( const QString&, bool askForNewLoad=false );
    void loadHistory( const QString& );
    bool saveProject( const QString& );
    bool loadProject( const QString&, bool );
    
    void createDockWidgets();
    void createActions();
    void createToolbars();
    void createStatusbar();
    
    void hideAllLayerToolbars();
    
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
    
    QToolBar* m_canvasWarpLayerToolbar = nullptr;
    QToolBar* m_rotateLayerToolbar = nullptr;
    QToolBar* m_scaleLayerToolbar = nullptr;
    QToolBar* m_mirrorLayerToolbar = nullptr;
    QToolBar* m_perspectiveLayerToolbar = nullptr;
    QToolBar* m_translateLayerToolbar = nullptr;
    
    MainOperationMode m_operationMode = Paint;
    
    QAction* m_quitAction = nullptr;
    QAction* m_saveHistoryAction = nullptr;
    QAction* m_openHistoryAction = nullptr;
    QAction* m_sortHistoryAction = nullptr;
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
    QAction* m_polygonCreateLayerAction = nullptr;
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
    
    QLabel *m_messageLabel = nullptr;
    
    QComboBox* m_polygonIndexBox = nullptr;
    QComboBox* m_transformLayerItem = nullptr;
    QComboBox* m_polygonOperationItem = nullptr;
    QComboBox* m_selectLayerItem = nullptr;
    QComboBox* m_mirrorDirectionCombo = nullptr;
    
    QDoubleSpinBox* m_rotationLayerAngleSpin = nullptr;
    QDoubleSpinBox* m_scaleXLayerSpin = nullptr;
    QDoubleSpinBox* m_scaleYLayerSpin = nullptr;
    QDoubleSpinBox* m_translateXLayerSpin = nullptr;
    QDoubleSpinBox* m_translateYLayerSpin = nullptr;
    
    QString m_selectedLayerItemName;
    QString m_mainImageName;
    
    bool m_updatingLayerList = false;
    bool m_saveImageDataInProjectFile = false;
    
};