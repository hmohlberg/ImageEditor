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

#include "CageControlPointItem.h"
#include "LayerItem.h"

#include "../core/Config.h"
#include "../core/IMainSystem.h"
#include <QGraphicsColorizeEffect>

#include <QGraphicsSceneMouseEvent>
#include <iostream>

CageControlPointItem::CageControlPointItem( LayerItem* layer, int index )
    : QGraphicsRectItem(),
      m_layer(layer), m_index(index)
{
    setZValue(10001);
    refreshStyle();
    setFlag(ItemIsMovable);
    setFlag(ItemSendsScenePositionChanges);
}

void CageControlPointItem::refreshStyle()
{
    const int r = EditorStyle::instance().controlPointRadius();
    prepareGeometryChange();
    setRect(-r, -r, r * 2, r * 2);
    setBrush(EditorStyle::instance().controlPointColor());
    update();
}

void CageControlPointItem::mousePressEvent( QGraphicsSceneMouseEvent *e )
{
  qCDebug(logEditor) << "CageControlPointItem::mousePressEvent((): index =" << m_index;
  {
    if ( m_layer == nullptr ) return;
    // The purpose of this offset is to account for the small
    // distance between the mouse click position and the center of
    // the control point. This gives a smooth motion of the cage.
    if( m_index >=0 && m_index < m_layer->cageMesh().points().size() ) {
      QPointF local = m_layer->mapFromScene(e->scenePos());
      QList<QPointF> pts = m_layer->cageMesh().points();
      m_clickOffset = local - pts[m_index];
    } else {
      std::cout << "CLAUDE: m_index not defined when moving cage control point." << std::endl;
    }
    m_lastPos = e->scenePos();
    if ( m_index >= 0 && m_index < m_layer->cageMesh().points().size() )
      m_startPoint = m_layer->cageMesh().points()[m_index];
    m_layer->setCageEditing(true);
    if ( e->modifiers() & Qt::ControlModifier ) {
      m_layer->setOpacity(EditorStyle::instance().layerOverlayOpacity());
      auto* colorEffect = new QGraphicsColorizeEffect();
      colorEffect->setColor(Qt::red);
      colorEffect->setStrength(1.0);
      m_layer->setGraphicsEffect(colorEffect);
    }
  }
}

void CageControlPointItem::mouseMoveEvent( QGraphicsSceneMouseEvent* e )
{
  qCDebug(logEditor) << "CageControlPointItem::mouseMoveEvent((): Processing...";
  {
    if ( m_layer == nullptr ) return;
    m_layer->setCagePoint(m_index, e->scenePos() - m_clickOffset);
    if ( auto* ms = IMainSystem::instance() ) {
      QPointF localPos = m_layer->mapFromScene(e->scenePos() - m_clickOffset);
      if ( EditorStyle::instance().allowIntegerMoveOnly() )
        localPos = QPointF(qRound(localPos.x()), qRound(localPos.y()));
      QPointF delta = localPos - m_startPoint;
      ms->showMessage(QString("Layer %1: cage point %2 moved by (%3, %4) px")
          .arg(m_layer->name()).arg(m_index)
          .arg(qRound(delta.x())).arg(qRound(delta.y())));
    }
    e->accept();
  }
}

void CageControlPointItem::mouseReleaseEvent( QGraphicsSceneMouseEvent* e )
{
  qCDebug(logEditor) << "CageControlPointItem::mouseReleaseEvent((): Processing...";
  {
    if ( m_layer == nullptr ) return;
    if ( !(e->modifiers() & Qt::ControlModifier) ) {
      m_layer->setOpacity(1.0);
      m_layer->setGraphicsEffect(nullptr);
    }
    m_layer->setCageEditing(false);
    e->accept();
  }
}
