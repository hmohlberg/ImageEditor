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

#include <QGraphicsRectItem>

class LayerItem;

class CageControlPointItem : public QGraphicsRectItem
{

public:
    CageControlPointItem( LayerItem* layer, int index );

protected:
    void mousePressEvent( QGraphicsSceneMouseEvent* ) override;
    void mouseMoveEvent( QGraphicsSceneMouseEvent* ) override;
    void mouseReleaseEvent( QGraphicsSceneMouseEvent* ) override;

private:
    LayerItem* m_layer;
    QPointF m_startPoint;
    int m_index;

};
