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

#include "TransformOverlay.h"

#include <QPainter>
#include <QApplication>
#include <QGraphicsSceneMouseEvent>

#include "LayerItem.h"
#include "TransformHandleItem.h"
#include "CenterHandleItem.h"
#include "../undo/TransformLayerCommand.h"

TransformOverlay::TransformOverlay( LayerItem* layer, QUndoStack* undoStack )
    : m_layer(layer)
    , m_undoStack(undoStack)
{
    setZValue(999999);
    createHandles();
    updateOverlay();
}

QVariant TransformOverlay::itemChange( GraphicsItemChange change, const QVariant &value ) 
{
  // qDebug() << "TransformOverlay::itemChange(): Processing...";
  {
    if ( change == ItemVisibleHasChanged ) {
        bool isVisible = value.toBool();
        // Hier deine Logik einbauen, wenn sich die Sichtbarkeit ändert
    }
    return QGraphicsItem::itemChange(change,value);
  }
}

QRectF TransformOverlay::boundingRect() const
{
    return m_rect.adjusted(-20, -20, 20, 20);
}

void TransformOverlay::paint( QPainter* p, const QStyleOptionGraphicsItem*, QWidget* )
{
    if ( !m_layer )
        return;
    p->setRenderHint(QPainter::Antialiasing);
    QPen pen(Qt::yellow);
    pen.setWidth(1);
    pen.setStyle(Qt::DashLine);
    p->setPen(pen);
    p->setBrush(Qt::NoBrush);
    p->drawRect(m_rect);
}

void TransformOverlay::updateOverlay()
{
    if ( !m_layer )
        return;
    prepareGeometryChange();
    // BoundingBox des Layers (Scene Coordinates)
    QRectF r = m_layer->sceneBoundingRect();
    // Overlay ist selbst in Scene Koordinaten
    m_rect = r;
    updateHandlePositions();
    update();
}

void TransformOverlay::createHandles()
{
    auto addHandle = [&](HandleType type)
    {
        auto* h = new TransformHandleItem(type, this);
        h->setParentItem(this);
        m_handles[type] = h;
    };

    // Corners
    addHandle(HandleType::Corner_TL);
    addHandle(HandleType::Corner_TR);
    addHandle(HandleType::Corner_BR);
    addHandle(HandleType::Corner_BL);

    // Sides
    addHandle(HandleType::Side_Left);
    addHandle(HandleType::Side_Right);
    addHandle(HandleType::Side_Top);
    addHandle(HandleType::Side_Bottom);
    
    // --- Center Handle ---
    m_centerHandle = new CenterHandleItem(this);
    m_centerHandle->setParentItem(this);
}

void TransformOverlay::updateHandlePositions()
{
    QRectF r = m_rect;

    // Corners
    m_handles[HandleType::Corner_TL]->setPos(r.topLeft());
    m_handles[HandleType::Corner_TR]->setPos(r.topRight());
    m_handles[HandleType::Corner_BR]->setPos(r.bottomRight());
    m_handles[HandleType::Corner_BL]->setPos(r.bottomLeft());

    // Sides
    m_handles[HandleType::Side_Left]->setPos(
        QPointF(r.left(), r.center().y()));
    m_handles[HandleType::Side_Right]->setPos(
        QPointF(r.right(), r.center().y()));
    m_handles[HandleType::Side_Top]->setPos(
        QPointF(r.center().x(), r.top()));
    m_handles[HandleType::Side_Bottom]->setPos(
        QPointF(r.center().x(), r.bottom()));
    
    // Center    
    if( m_centerHandle )
        m_centerHandle->setPos(r.center());
}

void TransformOverlay::translateLayer( const QPointF& delta )
{
    if ( !m_layer ) return;
    QTransform t;
    t.translate(delta.x(), delta.y());
    m_layer->setTransform(t * m_layer->transform());
    updateOverlay();
}

void TransformOverlay::beginTransform()
{
    if ( !m_layer )
        return;
    m_startTransform = m_layer->transform();
}

void TransformOverlay::endTransform()
{
 // qDebug() << "TransformOverlay::endTransform(): Processing...";
 {
    if ( !m_layer )
        return;
    QTransform end = m_layer->transform();
    if ( end == m_startTransform )
        return;
    m_initialTransform = m_startTransform;
    m_transformCommand = new TransformLayerCommand(m_layer,m_startTransform,end);
    m_undoStack->push(m_transformCommand);
    updateOverlay();
 }
}

void TransformOverlay::reset()
{
  // qDebug() << "TransformOverlay::reset(): Processing";
  {
    if ( !m_layer || !m_transformCommand ) return; 
    m_layer->resetTotalTransform();
    m_layer->setImageTransform(m_initialTransform);
    m_transformCommand->setTransform(m_initialTransform);
    updateOverlay();
  }
}

void TransformOverlay::applyHandleDrag( HandleType type, const QPointF& delta )
{
  // qDebug() << "TransformOverlay::applyHandleDrag(): Processing...";
  {
    if ( !m_layer ) return;
    
    bool isotropicScaling = true;
    if ( QApplication::keyboardModifiers() & Qt::AltModifier ) {
      isotropicScaling = false;
    }

    QRectF r = m_layer->boundingRect();
    const qreal minSize = 5.0;
    const qreal aspect = r.height() / r.width(); // Das ursprüngliche Verhältnis

    // Variablen für die Skalierung
    qreal sx = 1.0;
    qreal sy = 1.0;
    QPointF fixedPoint;

    // Unterscheidung zwischen Eck- und Seitenhandles
    bool isCorner = ( type == HandleType::Corner_TL || type == HandleType::Corner_TR || 
                        type == HandleType::Corner_BL || type == HandleType::Corner_BR );
    if ( isCorner ) {
        // --- Eckhandles (Logik bleibt weitgehend gleich) ---
        switch(type) {
            case HandleType::Corner_TL: fixedPoint = r.bottomRight(); break;
            case HandleType::Corner_TR: fixedPoint = r.bottomLeft(); break;
            case HandleType::Corner_BL: fixedPoint = r.topRight(); break;
            case HandleType::Corner_BR: fixedPoint = r.topLeft(); break;
            default: break;
        }
        switch(type) {
            case HandleType::Corner_TL:
                sx = (r.right() - (r.left() + delta.x())) / r.width();
                sy = (r.bottom() - (r.top() + delta.y())) / r.height();
                break;
            case HandleType::Corner_TR:
                sx = ((r.right() + delta.x()) - r.left()) / r.width();
                sy = (r.bottom() - (r.top() + delta.y())) / r.height();
                break;
            case HandleType::Corner_BL:
                sx = (r.right() - (r.left() + delta.x())) / r.width();
                sy = ((r.bottom() + delta.y()) - r.top()) / r.height();
                break;
            case HandleType::Corner_BR:
                sx = ((r.right() + delta.x()) - r.left()) / r.width();
                sy = ((r.bottom() + delta.y()) - r.top()) / r.height();
                break;
            default: break;
        }
        if ( isotropicScaling ) {
          qreal s = std::max(sx, sy);
          sx = sy = s;
        }
    } else {
        // --- Seitenhandles mit optionaler isotroper Skalierung ---
        switch( type ) {
            case HandleType::Side_Left:   fixedPoint = QPointF(r.right(), r.center().y()); break;
            case HandleType::Side_Right:  fixedPoint = QPointF(r.left(), r.center().y()); break;
            case HandleType::Side_Top:    fixedPoint = QPointF(r.center().x(), r.bottom()); break;
            case HandleType::Side_Bottom: fixedPoint = QPointF(r.center().x(), r.top()); break;
            default: break;
        }
        // Initiale Skalierung berechnen
        if ( type == HandleType::Side_Left )       sx = (r.right() - (r.left() + delta.x())) / r.width();
        else if ( type == HandleType::Side_Right )  sx = ((r.right() + delta.x()) - r.left()) / r.width();
        else if ( type == HandleType::Side_Top )    sy = (r.bottom() - (r.top() + delta.y())) / r.height();
        else if ( type == HandleType::Side_Bottom ) sy = ((r.bottom() + delta.y()) - r.top()) / r.height();
        // Isotrope Logik für Seitenhandles:
        // Wir verwenden hier Qt::ControlModifier (Strg-Taste)
        if ( isotropicScaling ) {
            if ( type == HandleType::Side_Left || type == HandleType::Side_Right ) {
                sy = sx; // Höhe folgt der Breite
            } else {
                sx = sy; // Breite folgt der Höhe
            }
        }
    }

    // Minimum-Größe absichern
    sx = std::max(sx, minSize / r.width());
    sy = std::max(sy, minSize / r.height());
    fixedPoint = r.center();

    // apply transformation
    QTransform t;
    t.translate(fixedPoint.x(), fixedPoint.y());
    t.scale(sx, sy);
    t.translate(-fixedPoint.x(), -fixedPoint.y());

    m_layer->setTransform(t * m_layer->transform());
    updateOverlay();
  }
}
