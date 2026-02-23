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

#include <QGraphicsItem>
#include <QPointF>

class TransformOverlay;

class CenterHandleItem : public QGraphicsItem
{
   public:
   
    CenterHandleItem(TransformOverlay* overlay);

    QRectF boundingRect() const override;
    void paint( QPainter* painter,
               const QStyleOptionGraphicsItem*,
               QWidget*) override ;

  protected:
  
    void mousePressEvent( QGraphicsSceneMouseEvent* e ) override;
    void mouseMoveEvent( QGraphicsSceneMouseEvent* e ) override;
    void mouseReleaseEvent( QGraphicsSceneMouseEvent* e ) override;

  private:
   
    TransformOverlay* m_overlay = nullptr;
    QPointF m_lastPos;
    
};