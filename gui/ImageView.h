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

#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsPolygonItem>
#include <QGraphicsRectItem>
#include <QImage>
#include <QVector>
#include <QColor>
#include <QPainterPath>
#include <QUndoStack>
#include <QPolygon>
#include <QHash>

#include "../layer/Layer.h"
#include "../layer/LayerItem.h"
#include "../layer/MaskLayerItem.h"
#include "../layer/EditablePolygonItem.h"
#include "../layer/TransformOverlay.h"
#include "../layer/PerspectiveOverlay.h"
#include "../undo/EditablePolygonCommand.h"
#include "../undo/MaskPaintCommand.h"
#include "../undo/CageWarpCommand.h"

// -------------------------------------    ------------------------------------- 
class LayerItem;
class MaskLayer;
class EditablePolygon;
class LassoCutCommand;

struct PixelChange {
    QPoint pos;
    QRgb before;
    QRgb after;
};

// -------------------------------------    ------------------------------------- 
class ImageView : public QGraphicsView
{
    Q_OBJECT
    
public:

    enum MaskTool { None, MaskPaint, MaskErase };
    enum MaskCutTool { Ignore, Mask, OnlyMask, Copy, Inpainting };
    
    explicit ImageView( QWidget* parent = nullptr );

    void cutSelection();
    void clearSelection();
    void clearLayers(); 
    void createLassoLayer();
    void createPolygonLayer();
    void centerOnLayer( Layer* layer );
    void deleteLayer( Layer* layer );
    void enablePipette( bool enabled );
    void setCrosshairVisible( bool visible ) { 
      m_crosshairVisible = visible; 
      viewport()->update(); 
    }
    void setLassoEnabled( bool enabled ) { 
      m_lassoEnabled = enabled; 
      if ( !enabled && m_lassoPreview ) {
        m_scene->removeItem(m_lassoPreview);
        delete m_lassoPreview;
        m_lassoPreview = nullptr;
        m_lassoPolygon.clear();
      }
      viewport()->update(); 
    }
    void finishPolygonDrawing( LayerItem* layer );
    void setPolygonEnabled( bool enabled );
    void setColorTable( const QVector<QRgb>& lut );
    void setImage( const QImage& img ) {
      m_image = img.convertToFormat(QImage::Format_ARGB32);
      QVector<QRgb> lut(256);
      for( int i=0;i<256;i++ ) lut[i] = qRgb(i,i,i);
      setColorTable(lut);
    }
    
    LayerItem::OperationMode getPolygonOperationMode() const { return m_polygonOperationMode; }
    LayerItem::OperationMode getLayerOperationMode() const { return m_layerOperationMode; }
    
    LayerItem* getSelectedItem( bool isActiveCageItem = false );
    LayerItem* currentLayer() const;
    LayerItem* baseLayer();
    EditablePolygonCommand* getPolygonUndoCommand( const QString& name = "", bool isSelected = false );
    QImage& getImage() { return m_image; };
    QGraphicsScene* getScene() const { return m_scene; }
    QUndoStack* undoStack() const { return m_undoStack; }
    QVector<Layer*>& layers() { return m_layers; }
    QColor maskLabelColor( int label ) const {
      if ( !m_maskItem ) return Qt::transparent;
      return m_maskItem->labelColor(label);
    }
    int pushEditablePolygon( EditablePolygon* edtitablePolygon );
    int getMaskCutToolType( const QString& name );
    
    void setBrushRadius( int r ) { m_brushRadius = r; }
    void setMaskBrushRadius( int r ) { m_maskBrushRadius = r; }
    void setBrushColor( const QColor& c ) { m_brushColor = c; }
    void setBrushHardness( qreal h ) { m_brushHardness = qBound(0.0, h, 1.0); }
    void setPaintToolEnabled( bool enabled ) { m_paintToolEnabled = enabled; }
    void setBrushPreviewVisible( bool visible ) { m_showBrushPreview = visible; viewport()->update(); }
    void setMaskOpacity( qreal value ) { if ( m_maskItem ) m_maskItem->setOpacityFactor(value); }
    void setMaskLabel( quint8 index ) { m_currentMaskLabel = index; }
    void setPolygonIndex( quint8 index ) { m_polygonIndex = index; }
    void setActiveCageLayer( LayerItem *item ) { m_selectedCageLayer = item; }
    void setLayerOperationMode( LayerItem::OperationMode mode );
    void setOnlySelectedPolygon( const QString& name );
    void setPolygonOperationMode( LayerItem::OperationMode mode );
    void setNumberOfCageControlPoints( int nControlPoints );
    void setDecreaseNumberOfCageControlPoints();
    void setIncreaseNumberOfCageControlPoints();
    void setCageWarpFixBoundary( bool isChecked );
    void setCageWarpRelaxationSteps( int nRelaxationSteps );
    void setCageWarpStiffness( double stiffness );
    void setCageVisible( LayerItem* layer, LayerItem::OperationMode mode, bool isVisible );
    void setMaskTool( MaskTool t );
    void setMaskCutTool( const QString&, MaskCutTool t );
    void createMaskLayer( const QSize& size );
    void saveMaskImage( const QString& filename );
    void loadMaskImage( const QString& filename );
    void removeOperationsByIdUndoStack( int id = -1 );
    void rebuildUndoStack();
    void forcedUpdate();
    
    void undoPolygonOperation();
    void redoPolygonOperation();
    
    void printself();

signals:

    void cursorColorChanged( const QColor& color );
    void pickedColorChanged( const QColor& color );
    void cursorPositionChanged( int x, int y );
    void scaleChanged( double scale );
    void lassoLayerAdded();
    void layerAdded();

protected:

    void keyPressEvent( QKeyEvent* e ) override;
    void mousePressEvent( QMouseEvent* event ) override;
    void mouseMoveEvent( QMouseEvent* event ) override;
    void mouseReleaseEvent( QMouseEvent* event ) override;
    void mouseDoubleClickEvent( QMouseEvent* event ) override;
    void wheelEvent( QWheelEvent* event ) override;
    void drawForeground( QPainter* painter, const QRectF& rect ) override;

private:

    LassoCutCommand* createNewLayer( const QPolygonF& polygon, const QString& name );
    void setEnableTransformMode( LayerItem* layer );
    void disableTransformMode();
    void setEnablePerspectiveWarp( LayerItem* layer );
    void disablePerspectiveWarp();

    QList<Layer*> m_layers;
    QList<EditablePolygon*> m_editablePolygons;
    QVector<MaskPaintCommand::PixelChange> m_currentMaskStroke;
    
    LayerItem* m_activeLayer = nullptr;
    LayerItem* m_selectedLayer = nullptr;
    LayerItem* m_selectedCageLayer = nullptr;
    LayerItem* m_paintLayer = nullptr;
    
    LayerItem::OperationMode m_layerOperationMode = LayerItem::OperationMode::Translate;
    LayerItem::OperationMode m_polygonOperationMode = LayerItem::OperationMode::MovePoint;
    
    EditablePolygon* m_activePolygon = nullptr;
    EditablePolygonItem* m_activePolygonItem = nullptr;
    
    MaskLayer* m_maskLayer = nullptr;
    MaskLayerItem* m_maskItem = nullptr;
    MaskTool m_maskTool = MaskTool::None;
    MaskCutTool m_maskCutTool = MaskCutTool::Ignore;
    QHash<QString, MaskCutTool> m_maskLabelTypeNames;
    
    TransformOverlay* m_transformOverlay = nullptr;
    PerspectiveOverlay* m_perspectiveOverlay = nullptr;
    
    CageWarpCommand* m_cageWarpCommand = nullptr;

    QWidget* m_parent = nullptr;
    QUndoStack* m_undoStack = nullptr;
    QGraphicsScene* m_scene = nullptr;
    QImage m_image;

    bool m_maskStrokeActive = false;
    bool m_crosshairVisible = true;
    bool m_lassoEnabled = false;
    bool m_selecting = false;
    bool m_panning = false;
    bool m_pipette = false;
    bool m_needColormapUpdate = false;
    bool m_showBrushPreview = true;
    bool m_paintToolEnabled = false;
    bool m_painting = false;
    bool m_maskPainting = false;
    bool m_polygonEnabled = false;
    bool m_maskEraser = false;
    
    quint8 m_currentMaskLabel = 1;
    quint8 m_polygonIndex = 1;
    qreal m_brushHardness = 1.0;
    QColor m_brushColor = Qt::white;
    QColor m_backgroundColor = Qt::white;
    int m_brushRadius = 5;
    int m_maskBrushRadius = 5;
    int m_lassoFeatherRadius = 0;
    int m_lastIndex = 0;
    
    QPointF m_cursorPos;
    QPoint m_lastMousePos;
    QPolygon m_lassoPolygon; 
    QGraphicsPolygonItem* m_lassoPreview = nullptr;
    QGraphicsRectItem* m_lassoBoundingBox = nullptr;
    QPainterPath m_selectionPath;
    QVector<QPoint> m_currentStroke;
    QVector<QPoint> m_maskStrokePoints;
    QVector<QRgb> m_lut;
    QVector<QPointF> m_cageBefore;

};
