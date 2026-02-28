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
 * @brief Logisches Polygon (Model)
 * Wird von Commands verändert und von EditablePolygonItem dargestellt
 */
class EditablePolygon : public QObject
{
    Q_OBJECT
    
public:

    explicit EditablePolygon( const QString& caller, const QString& name = "", QObject* parent = nullptr );

    // --- Access ---
    QString name() const { return m_name; }
    QUndoStack* undoStack() { return &m_undoStack; }
    const QPolygonF& polygon() const { return m_polygon; }
    int pointCount() const { return m_polygon.size(); }
    QPointF point( int idx ) const;

    // --- Modifikation (NUR über Commands aufrufen!) ---
    void smooth();
    void remove();
    void reduce( qreal tolerance = 0.5 );
    void translate( const QPointF& d );
    void addPoint( const QPointF& p );
    void setPoint( int idx, const QPointF& p );
    void insertPoint( int idx, const QPointF& p) ;
    void removePoint( int idx );
    void setPolygon( const QPolygonF& poly );

    // --- Misc ---
    void setName( const QString& name ) { m_name = name; }
    bool isSelected() const { return m_polygonSelected; }
    void setSelected( bool isSelected );
    bool polygonVisible() const { return m_polygonVisible; }
    bool markersVisible() const { return m_markersVisible; }
    QRectF boundingRect() const;
    void setVisible( bool isVisible );
    void printself();
    
    // --- JSON ---
    void undoStackFromJson( const QJsonArray& arr );
    QJsonArray undoStackToJson() const;

    // --- Serialization ---
    QJsonObject toJson() const;
    static EditablePolygon* fromJson( const QJsonObject& obj );

signals:

    void changed();
    void visibilityChanged();
    void selectionChanged();

private:

    QString m_name;

    QPolygonF m_polygon;
    QUndoStack m_undoStack;
    
    bool m_polygonSelected = true;
    bool m_polygonVisible = true;
    bool m_markersVisible = true;
    
};

