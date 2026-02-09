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

#include "MaskLayer.h"

MaskLayer::MaskLayer( const QSize& size )
    : m_width(size.width()),
      m_height(size.height()),
      m_image(size, QImage::Format_Grayscale8)
{
   m_image.fill(0);
}

MaskLayer::MaskLayer( int width, int height )
    : m_image(width, height, QImage::Format_Grayscale8)
{
    m_image.fill(0); // Alle Pixel = 0
}

void MaskLayer::setImage( const QImage& image ) 
{
   m_image = image;
}

void MaskLayer::setPixel( int x, int y, uchar label )
{
    if ( x < 0 || x >= m_image.width() || y < 0 || y >= m_image.height() )
        return;
    m_image.setPixelColor(x, y, QColor(label, label, label)); // Graustufe für Label
}

uchar MaskLayer::pixel( int x, int y ) const
{
    if ( x < 0 || x >= m_image.width() || y < 0 || y >= m_image.height() )
        return 0;
    return static_cast<uchar>(qGray(m_image.pixel(x, y)));
}

quint8 MaskLayer::labelAt( int x, int y ) const {
    return m_labels[y * m_width + x];
}

void MaskLayer::setLabelAt( int x, int y, quint8 label ) {
    m_labels[y * m_width + x] = label;
}

const QVector<quint8>& MaskLayer::data() const {
    return m_labels;
}

void MaskLayer::clear()
{
    m_image.fill(0);
}