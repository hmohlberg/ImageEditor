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

#include "PerspectiveOverlay.h"

#include <QPainter>
#include <QTransform>
#include <QPolygonF>

#include "LayerItem.h"
#include "TransformHandleItem.h"
#include "../undo/PerspectiveWarpCommand.h"

PerspectiveOverlay::PerspectiveOverlay( LayerItem* layer, QUndoStack* undoStack )
    : m_layer(layer)
    , m_undoStack(undoStack)
{
    setZValue(999999);
    QRectF r = m_layer->boundingRect();
    m_startQuad = {
        r.topLeft(),
        r.topRight(),
        r.bottomRight(),
        r.bottomLeft()
    };
    m_currentQuad = m_startQuad;
    createHandles();
    updateOverlay();
}

QRectF PerspectiveOverlay::boundingRect() const
{
    return m_rect;
}

void PerspectiveOverlay::paint( QPainter* p, const QStyleOptionGraphicsItem*, QWidget*)
{
    p->setRenderHint(QPainter::Antialiasing);
    QPen pen(Qt::cyan);
    pen.setStyle(Qt::DashLine);
    pen.setWidth(2);
    p->setPen(pen);
    if ( m_currentQuad.size() == 4 )
        p->drawPolygon(QPolygonF(m_currentQuad));
}

void PerspectiveOverlay::updateOverlay()
{
  qDebug() << "PerspectiveOverlay::updateOverlay(): Processing...";
  {
    if ( !m_layer || m_currentQuad.size() != 4 ) return;

    prepareGeometryChange();

    // --- Handles Positionen in Scene-Koordinaten ---
    QVector<QPointF> scenePts;
    for ( int i = 0; i < 4; i++ )
        scenePts.push_back(m_layer->mapToScene(m_currentQuad[i]));

    // --- BoundingRect für Overlay = Polygon der Handles ---
    m_rect = QPolygonF(scenePts).boundingRect();

    // --- Handles setzen ---
    m_handles[PerspectiveCorner::TL]->setPos(scenePts[0]);
    m_handles[PerspectiveCorner::TR]->setPos(scenePts[1]);
    m_handles[PerspectiveCorner::BR]->setPos(scenePts[2]);
    m_handles[PerspectiveCorner::BL]->setPos(scenePts[3]);

    update();
  }
}

void PerspectiveOverlay::createHandles()
{
  qDebug() << "PerspectiveOverlay::createHandles(): Processing...";
  {
    auto add = [&](PerspectiveCorner c)
    {
        auto* h = new TransformHandleItem((HandleType)c, this);
        h->setParentItem(this);
        m_handles[c] = h;
    };
    add(PerspectiveCorner::TL);
    add(PerspectiveCorner::TR);
    add(PerspectiveCorner::BR);
    add(PerspectiveCorner::BL);
  }
}

void PerspectiveOverlay::updateHandlePositions()
{
  qDebug() << "PerspectiveOverlay::updateHandlePositions(): Processing...";
  {
    if ( m_currentQuad.size() != 4 )
        return;
    m_handles[PerspectiveCorner::TL]->setPos(m_currentQuad[0]);
    m_handles[PerspectiveCorner::TR]->setPos(m_currentQuad[1]);
    m_handles[PerspectiveCorner::BR]->setPos(m_currentQuad[2]);
    m_handles[PerspectiveCorner::BL]->setPos(m_currentQuad[3]);
  }
}

void PerspectiveOverlay::beginWarp()
{
  qDebug() << "PerspectiveOverlay::beginWarp(): Processing...";
  {
    if ( !m_layer )
        return;
    m_dragging = true;
    m_startTransform = m_layer->transform();
  /*
    QRectF r = m_layer->boundingRect();
    m_startQuad = {
        r.topLeft(),
        r.topRight(),
        r.bottomRight(),
        r.bottomLeft()
    };
   */
    m_currentQuad = m_startQuad;
  }
}

void PerspectiveOverlay::endWarp()
{
  qDebug() << "PerspectiveOverlay::endWarp(): Processing...";
  {
    if ( !m_layer )
        return;
    if ( !m_dragging ) return;
        m_dragging = false;
    if ( m_currentQuad == m_startQuad )
        return;
    if ( !m_warpCommand ) {
      m_warpCommand = new PerspectiveWarpCommand(m_layer,m_startQuad,m_currentQuad);
      m_undoStack->push(m_warpCommand);
    } else {
      m_warpCommand->setAfterQuad(m_currentQuad);
    }
  }
}

void PerspectiveOverlay::moveCorner( PerspectiveCorner corner, const QPointF& scenePos )
{
  qDebug() << "PerspectiveOverlay::moveCorner(): Processing...";
  {
    if ( !m_layer || !m_dragging ) return;
    int idx = int(corner);
    
    // Scene → Local
    QPointF localPos = m_layer->mapFromScene(scenePos);
    m_currentQuad[idx] = localPos;

    // Transformation: quadToQuad zwischen StartQuad und aktuellem Quad
    QTransform warp;
    QTransform::quadToQuad(m_startQuad, m_currentQuad, warp);
    m_layer->setTransform(warp * m_startTransform); // Vorschau
    updateOverlay();
  }
}
