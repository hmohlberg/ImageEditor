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

PerspectiveHandleItem::PerspectiveHandleItem(
    int index,
    PerspectiveTransform* transform)
    : QGraphicsEllipseItem(-5, -5, 10, 10)
    , m_index(index)
    , m_transform(transform)
{
    setBrush(Qt::red);
    setFlag(ItemIsMovable);
    setFlag(ItemSendsScenePositionChanges);
}

void PerspectiveHandleItem::mouseMoveEvent( QGraphicsSceneMouseEvent* e )
{
    m_transform->setTargetPoint(m_index, e->scenePos());
    /*
    auto dst = m_transform->targetQuad();
    dst[m_index] = scenePos();
    m_transform->setTargetQuad(dst);
    QGraphicsEllipseItem::mouseMoveEvent(e);
    */
}
