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

#pragma once

#include <QGraphicsEllipseItem>

class LayerItem;
class QUndoStack;

class TransformHandleItem : public QGraphicsEllipseItem
{
public:

    enum Role { Scale, Rotate };

    TransformHandleItem( LayerItem* layer, Role role );

protected:

    void mousePressEvent( QGraphicsSceneMouseEvent* ) override;
    void mouseMoveEvent( QGraphicsSceneMouseEvent* ) override;
    void mouseReleaseEvent( QGraphicsSceneMouseEvent* ) override;

private:

    LayerItem* m_layer;
    Role m_role;
    QPointF m_pressScenePos;
    QTransform m_startTransform;

};