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
#include <QPolygonF>
#include <QVector>
#include <QJsonObject>
#include <QJsonArray>

#include "EditablePolygon.h"
#include "LayerItem.h"

class EditablePolygonItem : public QGraphicsObject
{
    Q_OBJECT
    
  public:
  
    explicit EditablePolygonItem( EditablePolygon* poly, QGraphicsItem* parent = nullptr );

    // --- QGraphicsItem ---
    QRectF boundingRect() const override;
    void paint( QPainter* p,
               const QStyleOptionGraphicsItem* opt,
               QWidget* widget ) override;

    // --- Interaction ---
    void pointMoved( int idx, const QPointF& scenePos );
    
    // --- Misc ---
    EditablePolygon* polygon() const { return m_poly; }
    int hitTestPolygon( const QPointF& scenePos ) const;
    void setColor( const QColor& color ) { m_lineColor = color; }
    void setName( const QString& name ) { m_name = name; } 

  signals:
  
    // → wird vom ImageView abgefangen und in UndoCommands umgesetzt
    void requestMovePoint( int idx, const QPointF& from, const QPointF& to );
    void requestInsertPoint( int idx, const QPointF& scenePos );
    void requestRemovePoint( int idx );

  protected:
  
    // --- mouse-Events ---
    void mousePressEvent( QGraphicsSceneMouseEvent* e ) override;
    void mouseMoveEvent( QGraphicsSceneMouseEvent* e ) override;
    void mouseReleaseEvent( QGraphicsSceneMouseEvent* e ) override;
    void mouseDoubleClickEvent( QGraphicsSceneMouseEvent* e ) override;

  private slots:
  
    void updateGeometry();
    void onVisibilityChanged();
    void onSelelctionChanged();

  private:

    // --- Hilfsfunktionen ---
    int hitTestPoint( const QPointF& scenePos ) const;
    int hitTestEdge( const QPointF& scenePos ) const;
    void rebuildHandles();

  private:

    EditablePolygon* m_poly = nullptr;
    LayerItem* m_layer = nullptr;

    // Dragging-State
    int     m_activePoint = -1;
    QPointF m_dragStartPos;
    QPointF m_dragMousePressPos;

    // Darstellung
    QVector<QGraphicsEllipseItem*> m_handles;
    qreal   m_handleRadius = 4.0;

    // Styling
    QColor m_lineColor   = QColor(0, 255, 0);
    QColor m_fillColor   = QColor(0, 255, 0, 40);
    QColor m_handleColor = QColor(255, 0, 0);

    QString m_name = "";
    bool m_editable = true;
};
