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
#include <QTransform>
#include <QPainter>
#include <cmath>
#include <algorithm>

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
  
  // for gui only
  QImage transformWithHighQuality(const QImage &originalImg, const QTransform &matrix)
  {
    QRectF transformedRect = matrix.mapRect(QRectF(originalImg.rect()));
    QImage transformedImg(transformedRect.size().toSize(), QImage::Format_ARGB32_Premultiplied);
    transformedImg.fill(Qt::transparent);
    QPainter painter(&transformedImg);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter.translate(-transformedRect.topLeft());
    painter.setTransform(matrix, true);
    painter.drawImage(0, 0, originalImg);
    painter.end();
    return transformedImg;
  }
  
  // universal High-Quality Non-GUI transformations
  inline float bicubicKernel( float x ) {
    x = std::abs(x);
    if (x <= 1.0f) return (1.5f * x - 2.5f) * x * x + 1.0f;
    if (x < 2.0f)  return ((-0.5f * x + 2.5f) * x - 4.0f) * x + 2.0f;
    return 0.0f;
  }

  QImage transformBicubic( const QImage &src, const QTransform &matrix ) {
    bool invertible;
    QTransform invMatrix = matrix.inverted(&invertible);
    if ( !invertible ) return QImage();

    // Ensure ARGB32 so scanLine gives QRgb* directly (no per-pixel format conversion)
    const QImage srcImg = (src.format() == QImage::Format_ARGB32)
                          ? src : src.convertToFormat(QImage::Format_ARGB32);
    const int srcW = srcImg.width(), srcH = srcImg.height();

    QRectF destRect = matrix.mapRect(QRectF(srcImg.rect()));
    const int destWidth  = (int)std::ceil(destRect.width());
    const int destHeight = (int)std::ceil(destRect.height());
    QImage dest(destWidth, destHeight, QImage::Format_ARGB32);
    dest.fill(Qt::transparent);

    const float offsetX = (float)destRect.left();
    const float offsetY = (float)destRect.top();

    for ( int dy = 0; dy < destHeight; ++dy ) {
        QRgb* destLine = reinterpret_cast<QRgb*>(dest.scanLine(dy));
        for ( int dx = 0; dx < destWidth; ++dx ) {
            qreal srcXReal, srcYReal;
            invMatrix.map((double)(dx + offsetX), (double)(dy + offsetY), &srcXReal, &srcYReal);
            if ( srcXReal < -1.0 || srcXReal >= srcW + 1 ||
                 srcYReal < -1.0 || srcYReal >= srcH + 1 ) continue;

            const int ix = (int)std::floor(srcXReal);
            const int iy = (int)std::floor(srcYReal);

            // Precompute the 4 kernel weights for x and y separately
            float wx[4], wy[4];
            for ( int k = 0; k < 4; ++k ) {
                wx[k] = bicubicKernel((float)srcXReal - (float)(ix + k - 1));
                wy[k] = bicubicKernel((float)srcYReal - (float)(iy + k - 1));
            }

            float r = 0, g = 0, b = 0, a = 0, totalWeight = 0;
            for ( int m = 0; m < 4; ++m ) {
                const int ky = iy + m - 1;
                if ( ky < 0 || ky >= srcH ) continue;
                const QRgb* srcLine = reinterpret_cast<const QRgb*>(srcImg.constScanLine(ky));
                for ( int n = 0; n < 4; ++n ) {
                    const int kx = ix + n - 1;
                    if ( kx < 0 || kx >= srcW ) continue;
                    const float w = wx[n] * wy[m];
                    const QRgb px = srcLine[kx];
                    r += qRed(px)   * w;
                    g += qGreen(px) * w;
                    b += qBlue(px)  * w;
                    a += qAlpha(px) * w;
                    totalWeight += w;
                }
            }
            if ( totalWeight > 0.0f ) {
                destLine[dx] = qRgba(
                    std::clamp((int)(r / totalWeight), 0, 255),
                    std::clamp((int)(g / totalWeight), 0, 255),
                    std::clamp((int)(b / totalWeight), 0, 255),
                    std::clamp((int)(a / totalWeight), 0, 255));
            }
        }
    }
    return dest;
  }
  
}