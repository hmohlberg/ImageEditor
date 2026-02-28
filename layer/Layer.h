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
#include <QImage>

class Layer {

  public:
  
    Layer( int index = -1, const QImage& image = QImage() ) : m_id(index), m_image(image) {}
  
    int id() const { return m_id; }
    float opacity() const { return m_opacity; }
    QPointF pos() const { return m_item != nullptr ? m_item->pos() : QPointF(0,0); }
    QString name() const { return m_name; }
    QString creator() const { return m_creator; }
    QImage image() const { return m_image; }

  public:
  
    int m_id = -1;
    float m_opacity = 1.0;
    
    QString m_name;
    QString m_creator;
    
    bool m_visible = true;
    bool m_active = true;
    
    QImage m_image;
    QPolygon m_polygon;
    QGraphicsItem* m_item = nullptr;
    bool m_linkedToImage = false;
    
    Layer() = default;
    
};