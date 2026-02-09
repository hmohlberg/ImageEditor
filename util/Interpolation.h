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

#include <QImage>

namespace Interpolation
{

  static inline float clamp( float v, float a, float b ) {
    return std::max(a, std::min(v, b));
  }

  void dab( QImage& img, const QPoint& center, const QColor& color, int radius, float hardness ) 
  {
    hardness = clamp(hardness, 0.0f, 1.0f);
    const int cx = center.x();
    const int cy = center.y();
    const int r2 = radius * radius;
    const int x0 = std::max(0, cx - radius);
    const int x1 = std::min(img.width()  - 1, cx + radius);
    const int y0 = std::max(0, cy - radius);
    const int y1 = std::min(img.height() - 1, cy + radius);
    for ( int y = y0; y <= y1; ++y ) {
        QRgb* line = reinterpret_cast<QRgb*>(img.scanLine(y));
        for ( int x = x0; x <= x1; ++x ) {
            const int dx = x - cx;
            const int dy = y - cy;
            const int d2 = dx*dx + dy*dy;
            if (d2 > r2)
                continue;
            float dist = std::sqrt(float(d2)) / float(radius);
            float alpha = 1.0f;
            if ( dist > hardness ) {
                float t = (dist - hardness) / (1.0f - hardness);
                alpha = 1.0f - clamp(t, 0.0f, 1.0f);
            }
            if ( alpha <= 0.0f )
                continue;
            QColor dst = QColor::fromRgba(line[x]);
            float a = alpha * (color.alphaF());
            float invA = 1.0f - a;
            int r = int(color.red()   * a + dst.red()   * invA);
            int g = int(color.green() * a + dst.green() * invA);
            int b = int(color.blue()  * a + dst.blue()  * invA);
            int outA = int(255 * (a + dst.alphaF() * invA));
            line[x] = qRgba(r, g, b, outA);
        }
    }
  }
  
}