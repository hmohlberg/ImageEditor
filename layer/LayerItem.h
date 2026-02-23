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

#include <QGraphicsPixmapItem>
#include <QUndoStack>
#include <QTransform>
#include <QJsonArray>
#include <QImage>
#include <QPen>

#include "CageMesh.h"
#include "PerspectiveTransform.h"

// ---
class Layer;
class CageOverlayItem;
class CageControlPointItem;
class TransformHandleItem;
class TransformOverlay;
class PerspectiveOverlay;

// ---
class LayerItem : public QGraphicsPixmapItem
{

  public:

    enum LayerType { MainImage, LassoLayer };
    enum OperationMode { None, Info, CageEdit, Translate, Rotate, Scale, Flip, Flop, Perspective, CageWarp, 
                          Select, MovePoint, AddPoint, DeletePoint, TranslatePolygon, SmoothPolygon, ReducePolygon, DeletePolygon };

    LayerItem( const QPixmap& pixmap, QGraphicsItem* parent = nullptr );
    LayerItem( const QImage& image, QGraphicsItem* parent = nullptr );
    
    void paintStrokeSegment( const QPoint& p0, const QPoint &p1, const QColor &color, int radius, float hardness );

    QImage& image( int id=0 );
    const QImage& originalImage();
    void updatePixmap();
    void resetPixmap();
    void resetTotalTransform() { m_totalTransform = QTransform(); }
    void setFileInfo( const QString& filePath );
    void setImage( const QImage &image );
    void setLayer( Layer *layer );
    QString filename() const { return m_filename; }
    QString checksum() const { return m_checksum; }
    QString name() const;
    
    void setMirror( int plane );
    void setImageTransform( const QTransform& transform, bool combine = false );
    // void setCagePoint( int idx, const QPointF& pos );
    void endCageEdit( int idx, const QPointF& pos );
    void setType( LayerType layerType );
    void updateCagePoint( TransformHandleItem*, const QPointF& localPos );
    void commitCageTransform( const QVector<QPointF> &cage );
    void beginCageEdit();
    void setCageVisible( bool isVisible = false );
    bool cageEnabled() const;
    void setCageVisible( LayerItem::OperationMode mode, bool isVisible );
    void setInActive( bool isInActive );
    
    int id() const { return m_index; }
    bool isEditing() const { return m_cageEditing; }
    bool isCageWarp() const { return m_operationMode == OperationMode::CageWarp ? true : false; }
    
    QUndoStack* undoStack() const { return m_undoStack; }
    QWidget* parent() const { return m_parent; }
    void setParent( QWidget *parent ) { m_parent = parent; }
    void setUndoStack( QUndoStack* stack ) { m_undoStack = stack; }
    void setIndex( const int index ) { m_index = index; }
    void setName( const QString& name ) { m_name = name; }
    LayerType getType() const { return m_type; }
    bool hasActiveCage() const { return m_cageOverlay != nullptr ? true : false; } // m_cageEnabled; }
    PerspectiveTransform& perspective() { return m_perspective; }
    
    void applyPerspectiveQuad( const QVector<QPointF>& quad );
    
    int changeNumberOfActiveCagePoints( int step );
    void setNumberOfActiveCagePoints( int nControlPoints );
    void setCageWarpProperty( int type, double value );
    
    void setOperationMode( OperationMode mode );
    OperationMode operationMode() const { return m_operationMode; }
    void setPolygonOperationMode( OperationMode mode );
    OperationMode polygonOperationMode() const { return m_polygonOperationMode; }
    
    OperationMode getPolygonOperationMode();
    
    const CageMesh& cageMesh() const { return m_cageMesh; }
    void setCagePoint( int idx, const QPointF& pos );
    void setCagePoints( const QVector<QPointF>& pts );
    void initCage( const QVector<QPointF>& pts, const QRectF& rect, int rows, int columns );
    QVector<QPointF> cagePoints() const;
    void applyTriangleWarp();
    void applyCageWarp();
    void enableCage( int, int );
    
    void applyPerspective();
    
    void updateHandles();
    void updateOriginalImage();
    void updateImageRegion( const QRect& rect );
    void notifyGeometryChange() {
        prepareGeometryChange();
    }
    
    void printself( bool debugSave = false );

  protected:

    void paint( QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget = nullptr ) override;
    
    void mousePressEvent( QGraphicsSceneMouseEvent* ) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent*) override;
    void mouseReleaseEvent( QGraphicsSceneMouseEvent* ) override;
    void mouseDoubleClickEvent( QGraphicsSceneMouseEvent* ) override;
    
    QImage m_image;
    QImage m_originalImage;
    
    QPointF m_startPos;
    
  private:

    void init();
    bool isValidMouseEventOperation();
    
    int m_index = 0;
    
    OperationMode m_operationMode = LayerItem::Translate;
    OperationMode m_polygonOperationMode = LayerItem::AddPoint;
    LayerType m_type = LayerItem::LassoLayer;
    
    QString m_name = "";
    QString m_filename = "";
    QString m_checksum = "";
    QVector<QGraphicsItem*> m_handles;
    QVector<QPointF> m_originalCage;
    QVector<QPointF> m_cage;
    
    CageMesh m_cageMesh;
    CageOverlayItem* m_cageOverlay = nullptr;
    PerspectiveTransform m_perspective;

	Layer* m_layer = nullptr;
	
	bool m_nogui = false; 
	bool m_lockToBoundingBox = true;
	bool m_showBoundingBox = true;
	bool m_dragging = false;
	bool m_cageEnabled = false;
	bool m_cageEditing = false;
	bool m_mouseOperationActive = false;
	
	QPen m_lassoPen;
	QPen m_selectedPen;
	
	QPointF m_pressScenePos;
    QTransform m_startTransform;
    QTransform m_totalTransform;
	
	QWidget* m_parent = nullptr;
	QUndoStack* m_undoStack = nullptr;
	
};