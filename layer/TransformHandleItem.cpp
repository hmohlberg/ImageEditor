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
  qCDebug(logEditor) << "TransformHandleItem::TransformHandleItem(): Processing...";
  {
    setBrush(role == Rotate ? Qt::yellow : Qt::white);
    setPen(QPen(Qt::black, 0));
    setFlag(ItemIgnoresTransformations);
    setCursor(Qt::SizeAllCursor);
    setZValue(2000);
  }
}

TransformHandleItem::TransformHandleItem( HandleType type, TransformOverlay* overlay )
    : QGraphicsEllipseItem(-6, -6, 12, 12)
    , m_type(type)
    , m_overlay(overlay)
{
     setBrush(Qt::red);
     QPen pen(Qt::black);
     pen.setWidth(1);
     setPen(pen);
     setZValue(9999999);
     setFlag(QGraphicsItem::ItemIsMovable, false);
     setAcceptedMouseButtons(Qt::LeftButton);
     switch ( m_type ) {
        case HandleType::Side_Left:
        case HandleType::Side_Right:
            setCursor(Qt::SizeHorCursor);
            break;
        case HandleType::Side_Top:
        case HandleType::Side_Bottom:
            setCursor(Qt::SizeVerCursor);
            break;
        case HandleType::Corner_BL:
        case HandleType::Corner_TR:
           setCursor(Qt::SizeBDiagCursor);
           break;
        default:
            setCursor(Qt::SizeFDiagCursor);
            break;
    }
}

TransformHandleItem::TransformHandleItem( HandleType type, PerspectiveOverlay* overlay )
    : QGraphicsEllipseItem(-6, -6, 12, 12)
    , m_type(type)
    , m_perspectiveOverlay(overlay)
{
    setBrush(Qt::cyan);
    setPen(QPen(Qt::black, 1));
    setZValue(9999999);
    setFlag(QGraphicsItem::ItemIsMovable, false);
    setAcceptedMouseButtons(Qt::LeftButton);
    switch ( m_type ) {
        case HandleType::Corner_BL:
        case HandleType::Corner_TR:
           setCursor(Qt::SizeBDiagCursor);
           break;
        default:
           setCursor(Qt::SizeFDiagCursor);
           break;
    }
}

// --------------------- Mouse events ---------------------
void TransformHandleItem::mousePressEvent( QGraphicsSceneMouseEvent* e )
{
 qCDebug(logEditor) << "TransformHandleItem::mousePressEvent(): overlay=" << (m_overlay?"ok":"null") 
                 << ", perspective=" << (m_perspectiveOverlay?"ok":"null")
                 << ", layer=" << (m_layer?"ok":"null");
 {
   m_pressScenePos = e->scenePos();
   if ( m_overlay ) {
    m_overlay->beginTransform();
   } else if ( m_perspectiveOverlay ) {
    m_perspectiveOverlay->beginWarp();
   } else if ( m_layer ) {
    m_startTransform = m_layer->transform();
   }
   e->accept();
 }
}

void TransformHandleItem::mouseMoveEvent( QGraphicsSceneMouseEvent* e )
{
  qCDebug(logEditor) << "TransformHandleItem::mouseMoveEvent(): Processing...";
  {
   QPointF delta = e->scenePos() - m_pressScenePos;
   if ( m_overlay ) {
    m_pressScenePos = e->scenePos();
    m_overlay->applyHandleDrag(m_type, delta);
   } else if ( m_perspectiveOverlay ) {
    m_pressScenePos = e->scenePos();
    switch ( m_type ) {
            case HandleType::Corner_TL:
                m_perspectiveOverlay->moveCorner(PerspectiveCorner::TL, e->scenePos());
                break;
            case HandleType::Corner_TR:
                m_perspectiveOverlay->moveCorner(PerspectiveCorner::TR, e->scenePos());
                break;
            case HandleType::Corner_BR:
                m_perspectiveOverlay->moveCorner(PerspectiveCorner::BR, e->scenePos());
                break;
            case HandleType::Corner_BL:
                m_perspectiveOverlay->moveCorner(PerspectiveCorner::BL, e->scenePos());
                break;
            default:
                break;
    }
   } else if ( m_layer ) {
    QPointF deltaLocal =
        m_layer->mapFromScene(delta + m_layer->scenePos())
        - m_layer->mapFromScene(m_layer->scenePos());
    QPointF newPos = m_pressScenePos + deltaLocal;
    setPos(newPos);
    m_layer->updateCagePoint(this, newPos);
   }
   e->accept();
  }
}

void TransformHandleItem::mouseReleaseEvent( QGraphicsSceneMouseEvent *e )
{
  qCDebug(logEditor) << "TransformHandleItem::mouseReleaseEvent(): Processing...";
  {
    if ( m_overlay ) {
      m_overlay->endTransform();
      e->accept();
    } else if ( m_perspectiveOverlay ) {
      m_perspectiveOverlay->endWarp();
      e->accept();
    } else if ( m_layer ) {
      if ( m_layer->undoStack() && m_startTransform != m_layer->transform() ) {
        m_layer->undoStack()->push(
            new TransformLayerCommand(
                m_layer, m_pressScenePos, m_pressScenePos, m_startTransform, m_layer->transform()));
      }
    }
  }
}