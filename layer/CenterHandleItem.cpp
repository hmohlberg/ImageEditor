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

#include "CenterHandleItem.h"
#include "TransformOverlay.h"

#include <QPainter>
#include <QGraphicsSceneMouseEvent>
#include <QCursor>

CenterHandleItem::CenterHandleItem( TransformOverlay* overlay ) : m_overlay(overlay)
{
    setAcceptedMouseButtons(Qt::LeftButton);
    setZValue(9999999);
    setCursor(Qt::SizeAllCursor); // ↔↕ für verschieben
}

QRectF CenterHandleItem::boundingRect() const
{
    return QRectF(-10,-10,20,20);
}

void CenterHandleItem::paint( QPainter* painter,
                             const QStyleOptionGraphicsItem*,
                             QWidget* )
{
    painter->setPen(QPen(Qt::yellow, 1));
    painter->drawLine(QPointF(-10,0), QPointF(10,0));
    painter->drawLine(QPointF(0,-10), QPointF(0,10));
}

void CenterHandleItem::mousePressEvent( QGraphicsSceneMouseEvent* e )
{
    m_lastPos = e->scenePos();
    if(m_overlay) m_overlay->beginTransform();
    e->accept();
}

void CenterHandleItem::mouseMoveEvent( QGraphicsSceneMouseEvent* e )
{
    QPointF delta = e->scenePos() - m_lastPos;
    m_lastPos = e->scenePos();
    if(m_overlay) m_overlay->translateLayer(delta);
    e->accept();
}

void CenterHandleItem::mouseReleaseEvent( QGraphicsSceneMouseEvent* e )
{
    if(m_overlay) m_overlay->endTransform();
    e->accept();
}