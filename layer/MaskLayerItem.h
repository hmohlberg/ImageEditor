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
#include "MaskLayer.h"

class MaskLayerItem : public QGraphicsItem {

  public:
    explicit MaskLayerItem( MaskLayer* layer );

    void setOpacityFactor( qreal o );
    void setLabelColors( const QVector<QColor>& colors );

    QRectF boundingRect() const override;
    QColor labelColor( int index ) {
      return m_labelColors.at(index);
    }
    void maskUpdated();
    
  protected:
    void paint( QPainter* p, const QStyleOptionGraphicsItem*, QWidget* ) override;

  private:
    MaskLayer* m_layer = nullptr;
    qreal m_opacityFactor = 0.4;
    QImage m_cachedImage; 
    bool m_dirty = true;
    QVector<QColor> m_labelColors; // size 10
    
};