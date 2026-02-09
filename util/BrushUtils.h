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

namespace BrushUtils
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
    // 
    if ( img.format() == QImage::Format_RGB32 ) {
      for ( int y = y0; y <= y1; ++y ) {
        QRgb* line = reinterpret_cast<QRgb*>(img.scanLine(y));
        for ( int x = x0; x <= x1; ++x ) {
            const int dx = x - cx;
            const int dy = y - cy;
            const int d2 = dx*dx + dy*dy;
            if ( d2 > r2 )
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
    } else if ( img.format() == QImage::Format_Grayscale8 ) { 
      int sourceGray = qGray(color.rgb());
      for ( int y = y0; y <= y1; ++y ) {
        uchar* line = img.scanLine(y); // uchar statt QRgb!
        for ( int x = x0; x <= x1; ++x ) {
            float dist = std::sqrt(float((x-cx)*(x-cx) + (y-cy)*(y-cy))) / float(radius);
            if (dist > 1.0f) continue;
            float alpha = (dist > hardness) ? 1.0f - clamp((dist - hardness) / (1.0f - hardness), 0.0f, 1.0f) : 1.0f;
            float a = alpha * color.alphaF();
            if ( a <= 0.0f ) continue;
            float invA = 1.0f - a;
            int oldGray = line[x];
            line[x] = static_cast<uchar>(sourceGray * a + oldGray * invA);
        }
    }
    } else {
      qInfo() << "WARNING: Invalid data format " << img.format();
    }
  }
  
  void strokeSegment( QImage& img, const QPoint& p0, const QPoint& p1, const QColor& color, int radius, float hardness ) {
    if ( radius <= 0 )
        return;
    const float spacing = std::max(1.0f, radius * 0.35f);
    const float dx = p1.x() - p0.x();
    const float dy = p1.y() - p0.y();
    const float dist = std::sqrt(dx*dx + dy*dy);
    if ( dist <= 0.0f ) {
        dab(img, p0, color, radius, hardness);
        return;
    }
    const int steps = std::ceil(dist / spacing);
    for ( int i = 0; i <= steps; ++i ) {
        float t = float(i) / float(steps);
        QPoint p(
            int(p0.x() + t * dx),
            int(p0.y() + t * dy)
        );
        dab(img, p, color, radius, hardness);
    }
  }
  
}