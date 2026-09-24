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
#include "../undo/CageWarpCommand.h"

// ---
class Layer;
class CageOverlayItem;
class CageControlPointItem;
class CageWarpRenderer;
class TransformHandleItem;
class TransformOverlay;
class PerspectiveOverlay;
class TransformLayerCommand;
class ImageViewer;

/**
 * @brief QGraphicsItem that displays and manages a single image layer.
 *
 * LayerItem is the central class of the layer system.  It wraps a QImage
 * (and a cached QPixmap for display) and supports:
 * - Free-hand painting and erasing via paintStrokeSegment()
 * - Geometric transforms: translate, rotate, scale, mirror, perspective
 * - Cage-warp deformation using a grid of control points (CageMesh)
 * - An independent QUndoStack for all layer-level operations
 *
 * The current interaction mode is controlled by setOperationMode() and
 * setPolygonOperationMode(); the active mode determines how mouse events
 * on the item are interpreted.
 *
 * Layer ownership: each LayerItem belongs to a Layer (the JSON-serialisable
 * model) and is displayed inside an ImageView (QGraphicsView).
 */
class LayerItem : public QGraphicsPixmapItem
{

  public:

    /// @brief Distinguishes the original load-time image from a cage-warped copy.
    enum ImageType { Unknown, Original, Warped };
    /// @brief Distinguishes a regular image layer from a lasso (free-selection) layer.
    enum LayerType { MainImage, LassoLayer };
    /**
     * @brief Interaction modes for the layer and its polygons.
     *
     * The active mode is set via setOperationMode() (for the layer itself)
     * or setPolygonOperationMode() (for the polygon tool).
     *
     * | Mode              | Description                                     |
     * |-------------------|-------------------------------------------------|
     * | None              | No interaction                                  |
     * | Info              | Display polygon measurements on double-click    |
     * | CageEdit          | Drag individual cage control points             |
     * | Translate         | Drag the layer to a new position                |
     * | Rotate            | Rotate the layer around its centre              |
     * | Scale             | Scale the layer using corner handles            |
     * | Flip / Flop       | Mirror horizontally or vertically               |
     * | Perspective       | Adjust perspective via four corner handles      |
     * | CageWarp          | Apply cage-based free-form deformation          |
     * | Select            | Select the active polygon                       |
     * | MovePoint         | Drag a polygon vertex                           |
     * | AddPoint          | Double-click a polygon edge to insert a vertex  |
     * | DeletePoint       | Double-click a vertex to remove it              |
     * | TranslatePolygon  | Drag the whole polygon                          |
     * | SmoothPolygon     | Apply Laplace smoothing on double-click         |
     * | ReducePolygon     | Apply Douglas–Peucker simplification            |
     * | DeletePolygon     | Remove the polygon on double-click              |
     */
    enum OperationMode { None, Info, CageEdit, Translate, Rotate, Scale, Flip, Flop, Perspective, CageWarp,
                          Select, MovePoint, AddPoint, DeletePoint, TranslatePolygon, SmoothPolygon, ReducePolygon, DeletePolygon };

    /// @brief Returns the human-readable name for an OperationMode value.
    static QString operationModeName( int mode );

    LayerItem( const QString& name, const QPixmap& pixmap, QGraphicsItem* parent = nullptr );
    LayerItem( const QString& name, const QImage& image, QGraphicsItem* parent = nullptr );
    ~LayerItem();

    QRectF boundingRect() const override;

    /**
     * @brief Paints a single brush stroke segment directly into the layer image.
     * @param p0       Start point in layer (pixel) coordinates.
     * @param p1       End point in layer (pixel) coordinates.
     * @param color    Stroke colour (alpha is respected).
     * @param radius   Brush radius in pixels.
     * @param hardness Edge hardness [0.0 = soft, 1.0 = hard].
     */
    void paintStrokeSegment( const QPoint& p0, const QPoint &p1, const QColor &color, int radius, float hardness );

    /// @brief Returns the layer's working image (id=0 is the primary image).
    QImage& image( int id=0 );
    /// @brief Returns the alpha mask as a Base64-encoded or raw PNG byte string.
    QString getAlphaMaskData( bool base64Encoding = true );
    /// @brief Stores @p originalImage as the layer's source image for reset/revert.
    void setOriginalImage( const QImage& originalImage, ImageType imageType = ImageType::Original );
    /// @brief Returns the unmodified source image (before any cage warp was applied).
    const QImage& originalImage();
    /// @brief Rebuilds the display pixmap from the current working image.
    void updatePixmap();
    /// @brief Restores the display pixmap from the stored original image.
    void resetPixmap();
    /// @brief Resets the cumulative transform to the identity matrix.
    void resetTotalTransform();
    /// @brief Stores the file path and computes an MD5 checksum for the loaded file.
    void setFileInfo( const QString& filePath );
    void setImage( const QImage &image );
    void setLayer( Layer *layer );
    /// @brief Returns the file name (without path) of the source image.
    QString filename() const { return m_filename; }
    /// @brief Returns the MD5 checksum of the source file (for duplicate detection).
    QString checksum() const { return m_checksum; }
    QString name() const;

    /// @brief Returns the cumulative transform applied to this layer (position × rotation × scale).
    QTransform totalTransform() const { return m_totalTransform; }
    void setTotalTransform( const QTransform &transform ) {
      m_totalTransform = transform;
    }

    void setIsSelected( int caller, bool isSelected );
    /// @brief Applies a mirror preset: 0 = none, 1 = horizontal, 2 = vertical.
    void setMirror( int plane );
    void mirror( int plane );
    /// @brief Moves the layer by @p aShift pixels relative to its current position.
    void shift( const QPointF& aShift );
    /// @brief Moves the layer so that its top-left corner is at @p aPosition.
    void shiftTo( const QPointF& aPosition );
    /// @brief Scales the layer image by independent x and y factors.
    void scale( double xscale, double yscale );
    void setRotationAngle( double value );
    double getRotationAngle() const { return m_currentRotation; }
    void setImageRect( const QRectF& rect );
    /**
     * @brief Applies an additional transform to the layer.
     * @param transform  The transform to apply.
     * @param combine    If true, multiplies with the existing transform; if false, replaces it.
     */
    void setImageTransform( const QTransform& transform, bool combine = true );
    void reapplyImageTransform();
    void resetImageState( const QImage& image, const QPointF& position, const QTransform& transform );

    // --- Cage warp ---
    /// @brief Updates a single cage control point after an interactive drag.
    void endCageEdit( int idx, const QPointF& pos );
    void setType( LayerType layerType );
    void updateCagePoint( TransformHandleItem*, const QPointF& localPos );
    /// @brief Commits the current cage state and bakes the warp into the image.
    void commitCageTransform( const QVector<QPointF> &cage );
    /// @brief Enters cage-edit mode and shows the control-point overlay.
    void beginCageEdit();
    void setCageVisible( int caller, bool isVisible = false );
    bool cageEnabled() const { return m_cageEnabled; }
    void setCageVisible( LayerItem::OperationMode mode, bool isVisible, bool pushBackImage = false );
    void setInActive( bool isInActive );
    CageWarpCommand* getCageWarpCommand() { return m_cageWarpCommand; }
    void setCageWarpCommand( CageWarpCommand * cmd ) { m_cageWarpCommand = cmd; }

    /// @brief Returns the numeric layer index (assigned by the layer manager).
    int id() const { return m_index; }
    bool isDeleted() const { return m_isDeleted; }
    void setMultiSelected( bool v ) { m_isMultiSelected = v; update(); }
    bool isEditing() const { return m_cageEditing; }
    bool isCageWarp() const { return m_operationMode == OperationMode::CageWarp ? true : false; }

    /// @brief Returns the undo stack for layer-level operations (transform, paint, etc.).
    QUndoStack* undoStack() const { return m_undoStack; }
    QWidget* parent() const { return m_parent; }
    /// @brief Returns the ImageView that contains this layer, or nullptr.
    ImageView* getParentImageView();
    void setCageEditing( bool isEditing ) { m_cageEditing = isEditing; }

    // Cage edit undo/redo (separate per-layer stack, does not close the cage session)
    QUndoStack* cageEditStack();
    void snapshotCageState();
    void restoreCageState( const QVector<QPointF>& pts, const QVector<QPointF>& origPts, int cols, int rows,
                           const QPointF& scenePos );
    const QVector<QPointF>& cageSnapPts()     const { return m_cageSnapPts; }
    const QVector<QPointF>& cageSnapOrigPts() const { return m_cageSnapOrigPts; }
    int     cageSnapCols() const { return m_cageSnapCols; }
    int     cageSnapRows() const { return m_cageSnapRows; }
    QPointF cageSnapPos()  const { return m_cageSnapPos; }
    void setParent( QWidget *parent ) { m_parent = parent; }
    void setUndoStack( QUndoStack* stack );
    QPointF dragStartPos() const { return m_startPos; }
    void resetDragStartPos() { m_startPos = pos(); }
    void setIndex( const int index ) { m_index = index; }
    void setName( const QString& name ) { m_name = name; }
    LayerType getType() const { return m_type; }
    bool hasActiveCage() const { return m_cageOverlay != nullptr ? true : false; }
    PerspectiveTransform& perspective() { return m_perspective; }

    /**
     * @brief Applies the four-corner perspective quad to the layer image.
     * @param quad Four corner points in scene coordinates (top-left, top-right, bottom-right, bottom-left).
     */
    void applyPerspectiveQuad( const QVector<QPointF>& quad );

    /// @brief Changes the number of active cage rows/cols by @p step and returns the new count.
    int changeNumberOfActiveCagePoints( int step );
    void setNumberOfActiveCagePoints( int nControlPoints );
    /**
     * @brief Sets a cage warp property (relaxation, stiffness, etc.).
     * @param type  Property identifier (solver-specific).
     * @param value New property value.
     */
    void setCageWarpProperty( int type, double value );

    /// @brief Sets the layer-level interaction mode (translate, rotate, scale, cage, …).
    void setOperationMode( OperationMode mode );
    OperationMode operationMode() const { return m_operationMode; }
    /// @brief Sets the polygon sub-tool mode (add/delete/move point, smooth, info, …).
    void setPolygonOperationMode( OperationMode mode );
    OperationMode polygonOperationMode() const { return m_polygonOperationMode; }

    OperationMode getPolygonOperationMode();

    const CageMesh& cageMesh() const { return m_cageMesh; }
    void setCagePoint( int idx, const QPointF& pos );
    void setCagePoints( const QVector<QPointF>& pts );
    void initCage( const QVector<QPointF>& pts, const QRectF& rect, int rows, int columns );
    void resetCageToPixmap();
    QVector<QPointF> cagePoints() const;
    /// @brief Applies the current cage mesh to the image and returns the warped result.
    QImage applyCageWarp( const QString& caller = "unknown" );
    void enableCage( int cols = -1, int nrows = -1 );

    void applyPerspective();

    void updateHandles();
    void updateOriginalImage();
    /// @brief Repaints only the changed rectangle @p rect inside the layer image.
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
    ImageType m_originalImageType = ImageType::Unknown;
    
    QPointF m_startPos;
    
  private:

    void init();
    bool isValidMouseEventOperation();
    
    int m_index = 0;
    
    double m_currentRotation = 0.0;
    double m_startMouseAngle = 0.0;
    double m_startLayerRotation = 0.0;
    
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
    CageWarpCommand* m_cageWarpCommand = nullptr;
    CageWarpRenderer* m_cageWarpRenderer = nullptr;

    QUndoStack*      m_cageEditStack       = nullptr;
    QVector<QPointF> m_cageSnapPts;
    QVector<QPointF> m_cageSnapOrigPts;
    int              m_cageSnapCols        = 0;
    int              m_cageSnapRows        = 0;
    QPointF          m_cageSnapPos;
    // Initial cage state at start of editing session (for Reset)
    QVector<QPointF> m_cageInitialPts;
    QVector<QPointF> m_cageInitialOrigPts;
    int              m_cageInitialCols     = 0;
    int              m_cageInitialRows     = 0;
    QPointF          m_cageInitialPos;

    Layer* m_layer = nullptr;
	
    bool m_nogui = false; 
    bool m_lockToBoundingBox = true;
    bool m_showBoundingBox = true;
    bool m_dragging = false;
    bool m_cageEnabled = false;
    bool m_cageEditing = false;
    bool m_cageApplied = false;
    bool m_mouseOperationActive = false;
    bool m_isDeleted = false;
    bool m_isMultiSelected = false;
    bool m_redOverlay = false;
    QPixmap m_redOverlayPixmap;
	
    QPen m_lassoPen;
    QPen m_selectedPen;
	
    QPointF m_pressScenePos;
    QTransform m_startTransform;
    QTransform m_totalTransform;
	
    QWidget* m_parent = nullptr;
    QUndoStack* m_undoStack = nullptr;
	
};