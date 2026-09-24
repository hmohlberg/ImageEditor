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

#include <QGraphicsObject>
#include <QUndoStack>
#include <QPolygonF>
#include <QVector>
#include <QJsonObject>
#include <QJsonArray>

class EditablePolygonItem;

/**
 * @brief Model class representing a single editable polygon.
 *
 * Stores the vertex list as a QPolygonF and owns the per-polygon undo stack.
 * All geometry modifications must go through QUndoCommand subclasses so that
 * undo/redo works correctly; never call the mutating methods directly from
 * application code.
 *
 * EditablePolygonItem renders this model inside the QGraphicsScene.
 */
class EditablePolygon : public QObject
{
    Q_OBJECT

 public:

    explicit EditablePolygon( const QString& caller, const QString& name = "", QObject* parent = nullptr );

    // --- Access ---
    /// @brief Returns the display name of the polygon (e.g. "Polygon 1").
    QString name() const { return m_name; }
    /// @brief Returns the per-polygon undo stack (used by polygon command objects).
    QUndoStack* undoStack() { return &m_undoStack; }
    /// @brief Returns the vertex list in scene coordinates.
    const QPolygonF& polygon() const { return m_polygon; }
    /// @brief Returns the number of vertices.
    int pointCount() const { return m_polygon.size(); }
    /// @brief Returns the numeric index extracted from the polygon name.
    int index() const { return m_index; }
    void setIndex( const QString &name );
    /// @brief Returns the vertex at position @p idx in scene coordinates.
    QPointF point( int idx ) const;

    // --- Geometry mutation (call only from QUndoCommand subclasses) ---
    /// @brief Applies one iteration of Laplace smoothing to the vertex list.
    void smooth();
    /// @brief Clears all vertices (marks the polygon as deleted).
    void remove();
    /**
     * @brief Simplifies the polygon using the Douglas–Peucker algorithm.
     * @param tolerance Maximum allowed deviation from the original line (pixels).
     */
    void reduce( qreal tolerance = 2.0 );
    /// @brief Translates all vertices by @p d.
    void translate( const QPointF& d );
    /// @brief Appends a new vertex at the end of the vertex list.
    void addPoint( const QPointF& p );
    /// @brief Replaces the vertex at @p idx with @p p.
    void setPoint( int idx, const QPointF& p );
    /// @brief Inserts a new vertex at position @p idx.
    void insertPoint( int idx, const QPointF& p );
    /// @brief Removes the vertex at @p idx.
    void removePoint( int idx );
    /// @brief Replaces the entire vertex list.
    void setPolygon( const QPolygonF& poly );

    // --- State ---
    void setLayer() { m_hasLayer = true; }
    bool layer() const { return m_hasLayer; }
    void setName( const QString& name );
    /// @brief Returns true if the polygon is currently selected in the scene.
    bool isSelected() const { return m_polygonSelected; }
    void setSelected( bool isSelected );
    bool polygonVisible() const { return m_polygonVisible; }
    bool markersVisible() const { return m_markersVisible; }
    QRectF boundingRect() const;
    void setVisible( bool isVisible );
    void printself();

    /**
     * @brief Returns an HTML table with polygon measurements.
     *
     * Computes area (shoelace formula), perimeter, bounding box, and centroid.
     * All metric values are scaled by @p pixelScaleUm (µm per pixel).
     *
     * @param pixelScaleUm Scale factor in micrometres per pixel (default 20 µm/px).
     * @return HTML string suitable for display in a QLabel or QDialog.
     */
    QString infoHtml(double pixelScaleUm) const;

    // --- Serialization ---
    void undoStackFromJson( const QJsonArray& arr );
    QJsonArray undoStackToJson() const;
    QJsonObject toJson() const;
    static EditablePolygon* fromJson( const QJsonObject& obj );

 signals:

    void changed();
    void visibilityChanged();
    void selectionChanged();

 private:

    QString m_name;
    int m_index;

    QPolygonF m_polygon;
    QUndoStack m_undoStack;
    
    bool m_polygonSelected = true;
    bool m_polygonVisible = true;
    bool m_markersVisible = true;
    bool m_hasLayer = false;
    
};

