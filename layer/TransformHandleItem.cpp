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

#include "TransformHandleItem.h"
#include "LayerItem.h"
#include "../undo/TransformLayerCommand.h"

#include <QGraphicsSceneMouseEvent>
#include <QtMath>
#include <QCursor>

#include <iostream>

TransformHandleItem::TransformHandleItem( LayerItem* layer, Role role )
    : QGraphicsEllipseItem(-5, -5, 10, 10),
      m_layer(layer),
      m_role(role)
{
std::cout << "TransformHandleItem::TransformHandleItem(): Processing..." << std::endl;
    setBrush(role == Rotate ? Qt::yellow : Qt::white);
    setPen(QPen(Qt::black, 0));
    setFlag(ItemIgnoresTransformations);
    setCursor(Qt::SizeAllCursor);
    setZValue(2000);
}

void TransformHandleItem::mousePressEvent( QGraphicsSceneMouseEvent* e )
{
    m_pressScenePos = e->scenePos();
    m_startTransform = m_layer->transform();
}

void TransformHandleItem::mouseMoveEvent( QGraphicsSceneMouseEvent* event )
{
    QPointF deltaScene = event->scenePos() - m_pressScenePos;
    QPointF deltaLocal =
        m_layer->mapFromScene(deltaScene + m_layer->scenePos())
        - m_layer->mapFromScene(m_layer->scenePos());
    QPointF newPos = m_pressScenePos + deltaLocal;
    setPos(newPos);
    m_layer->updateCagePoint(this, newPos);
    event->accept();

/*
    QPointF delta = e->scenePos() - m_pressScenePos;
    QPointF c = m_layer->boundingRect().center();
    QTransform t = m_startTransform;
    t.translate(c.x(), c.y());
    if ( m_role == Rotate )
        t.rotate(delta.x());
    else {
        qreal s = qMax(0.1, 1.0 + delta.y() * 0.005);
        t.scale(s, s);
    }
    t.translate(-c.x(), -c.y());
    m_layer->setTransform(t);
*/
}

void TransformHandleItem::mouseReleaseEvent( QGraphicsSceneMouseEvent* )
{
std::cout << "TransformHandleItem::mouseReleaseEvent(): Processing..." << std::endl;
    if ( m_layer->undoStack() && m_startTransform != m_layer->transform() ) {
        m_layer->undoStack()->push(
            new TransformLayerCommand(
                m_layer, m_pressScenePos, m_pressScenePos, m_startTransform, m_layer->transform()));
    }
}