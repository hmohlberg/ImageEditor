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

/**
 * @brief QGraphicsItem that renders a semantic segmentation mask overlay.
 *
 * Holds a cached ARGB image built from the MaskLayer's label data.
 * Each label index is mapped to a configurable colour; the resulting
 * image is composited over the scene at a configurable opacity.
 *
 * Call maskUpdated() whenever the underlying MaskLayer data changes
 * to invalidate the cache and schedule a repaint.
 */
class MaskLayerItem : public QGraphicsItem {

  public:
    explicit MaskLayerItem( MaskLayer* layer );

    /// @brief Sets the overall opacity multiplier (0.0 = invisible, 1.0 = opaque).
    void setOpacityFactor( qreal o );
    /**
     * @brief Assigns the colour for each label index.
     * @param colors Vector of colours; index 0 is the background class.
     */
    void setLabelColors( const QVector<QColor>& colors );

    QRectF boundingRect() const override;
    /// @brief Returns the display colour for label @p index.
    QColor labelColor( int index ) {
      return m_labelColors.at(index);
    }
    /// @brief Invalidates the cached image so the overlay is redrawn on the next paint call.
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