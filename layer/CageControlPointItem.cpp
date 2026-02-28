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

#include <QGraphicsSceneMouseEvent>
#include <iostream>

CageControlPointItem::CageControlPointItem( LayerItem* layer, int index )
    : QGraphicsRectItem(-4, -4, 8, 8),
      m_layer(layer), m_index(index)
{
	setBrush(Qt::red);
    setZValue(10001);
    setFlag(ItemIsMovable);
    setFlag(ItemSendsScenePositionChanges);
}

void CageControlPointItem::mousePressEvent( QGraphicsSceneMouseEvent *e )
{
  qCDebug(logEditor) << "CageControlPointItem::mousePressEvent((): Processing...";
  {
    // m_startPoint = m_layer->mapFromScene(e->scenePos());
    // m_layer->beginCageEdit();
  }
}

void CageControlPointItem::mouseMoveEvent( QGraphicsSceneMouseEvent* e )
{
  qCDebug(logEditor)  << "CageControlPointItem::mouseMoveEvent((): Processing...";
  {
    m_layer->setCagePoint(m_index, e->scenePos());
    e->accept();
    // QPointF scenePos = e->scenePos();
    // QPointF layerLocalPos = m_layer->mapFromScene(scenePos);
    // setPos(layerLocalPos);
    // m_layer->setCagePoint(m_index, layerLocalPos);
    // e->accept();
  }
}

void CageControlPointItem::mouseReleaseEvent( QGraphicsSceneMouseEvent* e )
{
  qCDebug(logEditor)  << "CageControlPointItem::mouseReleaseEvent((): Processing...";
  {
    // m_layer->endCageEdit(m_index,m_startPoint);
    e->accept();
  }
}
