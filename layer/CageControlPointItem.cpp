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

#include <QGraphicsSceneMouseEvent>
#include <QGraphicsSceneHoverEvent>
#include <QPainter>
#include <iostream>

CageControlPointItem::CageControlPointItem( LayerItem* layer, int index )
    : QGraphicsRectItem(),
      m_layer(layer), m_index(index)
{
    setZValue(10001);
    setAcceptHoverEvents(true);
    refreshStyle();
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
      QList<QPointF> pts = m_layer->cageMesh().points();
      // Store scene position of cage point at drag start (used for status message delta).
      m_clickOffset = m_layer->mapToScene(pts[m_index]);
    } else {
      std::cout << "CLAUDE: m_index not defined when moving cage control point." << std::endl;
    }
    m_lastPos = e->scenePos();
    if ( m_index >= 0 && m_index < m_layer->cageMesh().points().size() )
      m_startPoint = m_layer->cageMesh().points()[m_index];
    m_layer->snapshotCageState();
    m_layer->setCageEditing(true);
    if ( e->modifiers() & Qt::ControlModifier ) {
      m_layer->setOpacity(EditorStyle::instance().layerOverlayOpacity());
      m_layer->setRedOverlay(true);
    }
  }
}

void CageControlPointItem::mouseMoveEvent( QGraphicsSceneMouseEvent* e )
{
  qCDebug(logEditor) << "CageControlPointItem::mouseMoveEvent((): Processing...";
  {
    if ( m_layer == nullptr ) return;
    const QVector<QPointF>& pts = m_layer->cageMesh().points();
    if (m_index >= 0 && m_index < pts.size()) {
      // Incremental delta avoids stale m_clickOffset after setCagePoint normalization
      // shifts the item pos (happens when left/top edge points go negative).
      QPointF delta = e->scenePos() - m_lastPos;
      m_lastPos = e->scenePos();
      QPointF newScenePos = m_layer->mapToScene(pts[m_index]) + delta;
      m_layer->setCagePoint(m_index, newScenePos);
      if ( auto* ms = IMainSystem::instance() ) {
        QPointF disp = newScenePos - m_clickOffset;  // m_clickOffset = scene pos at drag start
        if ( EditorStyle::instance().allowIntegerMoveOnly() )
          disp = QPointF(qRound(disp.x()), qRound(disp.y()));
        ms->showMessage(QString("Layer %1: cage point %2 moved by (%3, %4) px")
            .arg(m_layer->name()).arg(m_index)
            .arg(qRound(disp.x())).arg(qRound(disp.y())));
      }
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
      m_layer->setRedOverlay(false);
    }
    m_layer->setCageEditing(false);
    e->accept();
  }
}

void CageControlPointItem::paint( QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget )
{
    if ( !m_hovered ) {
        QGraphicsRectItem::paint(painter, option, widget);
        return;
    }
    const int r = EditorStyle::instance().controlPointRadius();
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    painter->setPen(pen());
    painter->setBrush(brush());
    painter->drawRect(-r * 2, -r * 2, r * 4, r * 4);
    painter->setPen(QPen(QColor(255, 220, 0, 200), 1.5));
    painter->setBrush(Qt::NoBrush);
    painter->drawEllipse(QPointF(0, 0), r * 4.0, r * 4.0);
    painter->restore();
}

void CageControlPointItem::hoverEnterEvent( QGraphicsSceneHoverEvent* e )
{
    m_hovered = true;
    const int r = EditorStyle::instance().controlPointRadius();
    prepareGeometryChange();
    setRect(-r * 4, -r * 4, r * 8, r * 8);
    e->accept();
}

void CageControlPointItem::hoverLeaveEvent( QGraphicsSceneHoverEvent* e )
{
    m_hovered = false;
    refreshStyle();
    e->accept();
}
