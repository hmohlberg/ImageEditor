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
#include "../core/Config.h"
#include "../util/GeometryUtils.h"
#include "../undo/PerspectiveWarpCommand.h"

PerspectiveOverlay::PerspectiveOverlay( LayerItem* layer, QUndoStack* undoStack )
    : m_layer(layer), m_undoStack(undoStack)
{
  qDebug() << "PerspectiveOverlay::PerspectiveOverlay(): Processing...";
  {
    setZValue(999999);
    QRectF r = m_layer->boundingRect();
    m_startQuad = {
        r.topLeft(),
        r.topRight(),
        r.bottomRight(),
        r.bottomLeft()
    };
    qDebug() << "PerspectiveOverlay::PerspectiveOverlay(): startQuad =" << m_startQuad;
    m_currentQuad = m_startQuad;
    m_initialQuad = m_currentQuad;
    m_finalQuad = m_startQuad;
    createHandles();
    updateOverlay();
  }
}

QRectF PerspectiveOverlay::boundingRect() const
{
    return m_rect.adjusted(-4, -4, 4, 4);
}

void PerspectiveOverlay::paint( QPainter* p, const QStyleOptionGraphicsItem*, QWidget* )
{
  {
    p->setRenderHint(QPainter::Antialiasing);
    QPen pen(Qt::cyan);
    pen.setStyle(Qt::DashLine);
    pen.setWidth(2);
    p->setPen(pen);
    p->setBrush(Qt::NoBrush);
    if ( m_handles.size() == 4 ) {
        QPolygonF polygon;
        polygon << m_handles[PerspectiveCorner::TL]->pos()
                << m_handles[PerspectiveCorner::TR]->pos()
                << m_handles[PerspectiveCorner::BR]->pos()
                << m_handles[PerspectiveCorner::BL]->pos();
        p->drawPolygon(polygon);
    }
  }
}

void PerspectiveOverlay::updateOverlay( bool fromStart )
{
  qCDebug(logEditor) << "PerspectiveOverlay::updateOverlay(): fromStart =" << fromStart;
  {
    if ( !m_layer || m_startQuad.size() != 4 || m_currentQuad.size() != 4 ) return;
    prepareGeometryChange();
    // --- Handles Positionen in Scene-Koordinaten ---
    QVector<QPointF> scenePts;
    if ( fromStart ) {
     for ( int i = 0; i < 4; i++ )
        scenePts.push_back(m_layer->mapToScene(m_startQuad[i]));
    } else {
     for ( int i = 0; i < 4; i++ )
        scenePts.push_back(m_layer->mapToScene(m_currentQuad[i]));
    }
    // --- BoundingRect für Overlay = Polygon der Handles ---
    m_rect = QPolygonF(scenePts).boundingRect();
    // --- Handles setzen ---
    m_handles[PerspectiveCorner::TL]->setPos(scenePts[0]);
    m_handles[PerspectiveCorner::TR]->setPos(scenePts[1]);
    m_handles[PerspectiveCorner::BR]->setPos(scenePts[2]);
    m_handles[PerspectiveCorner::BL]->setPos(scenePts[3]);
    // --- Update ---
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
  qDebug() << "PerspectiveOverlay::updateHandlePositions(): currentQuad =" << m_currentQuad;
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
    if ( !m_layer ) return;
    m_dragging = true;
    m_sceneToLocalSnapshot = m_layer->sceneTransform().inverted();
    m_startTransform = m_layer->transform();
    m_currentQuad = m_startQuad;
  }
}

void PerspectiveOverlay::endWarp()
{
  qDebug() << "PerspectiveOverlay::endWarp(): Processing...";
  {
    if ( !m_layer || !m_dragging ) return;
    m_dragging = false;
    if ( m_currentQuad == m_startQuad ) {
      return;
    }
    QVector<QPointF> sceneQuad;
    sceneQuad.reserve(m_startQuad.size());
    for ( const QPointF& point : m_startQuad ) {
        sceneQuad.push_back(m_layer->mapToScene(point));
    }
    m_layer->setTransform(m_startTransform);
    if ( !m_warpCommand ) {
        m_warpCommand = new PerspectiveWarpCommand(m_layer,m_startQuad,m_currentQuad);
        m_undoStack->push(m_warpCommand);
    } else {
        m_warpCommand->setAfterQuadFromScene(sceneQuad);
        m_warpCommand->redo();
    }

    QVector<QPointF> displayQuad = m_warpCommand->displayQuad();
    if ( displayQuad.size() != 4 ) {
        return;
    }
    m_startQuad = displayQuad;
    m_currentQuad = displayQuad;
    m_initialQuad = displayQuad;
    m_finalQuad = displayQuad;
    updateOverlay();
	  }
	}

void PerspectiveOverlay::resetWarp()
{
  qDebug() << "PerspectiveOverlay::resetWarp(): Processing...";
  {
    if ( !m_layer )
        return;
    m_dragging = false;
    if ( !m_warpCommand && m_undoStack ) {
        for ( int i = m_undoStack->count() - 1; i >= 0; --i ) {
            auto* cmd = const_cast<PerspectiveWarpCommand*>(dynamic_cast<const PerspectiveWarpCommand*>(m_undoStack->command(i)));
            if ( cmd && cmd->layer() == m_layer ) {
                m_warpCommand = cmd;
                break;
            }
        }
    }
    if ( m_warpCommand ) {
        m_warpCommand->resetWarp();
        QVector<QPointF> displayQuad = m_warpCommand->displayQuad();
        if ( displayQuad.size() == 4 ) {
            m_startQuad = displayQuad;
            m_currentQuad = displayQuad;
            m_initialQuad = displayQuad;
            m_finalQuad = displayQuad;
        }
    } else {
        QRectF r = m_layer->boundingRect();
        m_startQuad = {
            r.topLeft(),
            r.topRight(),
            r.bottomRight(),
            r.bottomLeft()
        };
        m_currentQuad = m_startQuad;
        m_initialQuad = m_startQuad;
        m_finalQuad = m_startQuad;
    }
    updateOverlay();
  }
}

void PerspectiveOverlay::commitTransformation()
{
  qDebug() << "PerspectiveOverlay::commitTransformation(): Processing...";
  {
    if ( !m_layer || m_currentQuad == m_initialQuad ) return;
    auto* cmd = new PerspectiveWarpCommand(m_layer, m_startQuad, m_currentQuad); // m_initialQuad -> m_startQuad
    m_undoStack->push(cmd);
    m_initialQuad = m_currentQuad; 
  }
}

void PerspectiveOverlay::moveCorner( PerspectiveCorner corner, const QPointF& scenePos )
{
  qCDebug(logEditor) << "PerspectiveOverlay::moveCorner(): corner =" << int(corner) << ", pos =" << scenePos;
  {
    if ( !m_layer || !m_dragging ) return;
    QPointF localPos = m_sceneToLocalSnapshot.map(scenePos);
    m_currentQuad[int(corner)] = localPos;
    m_finalQuad[int(corner)] = localPos;
    QTransform warp;
#if 0
    if ( QTransform::quadToQuad(m_startQuad, m_currentQuad, warp) ) {
      m_layer->setTransform(warp * m_startTransform);
    }
#else
    warp = GeometryUtils::quadToQuad( m_startQuad, m_currentQuad );
    m_layer->setTransform(warp * m_startTransform);
#endif
    updateOverlay(true);
  }
}
