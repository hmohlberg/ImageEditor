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
#include <QRectF>
#include <QPainter>
#include <QtMath>

#include "../layer/CageMesh.h"
#include "GeometryUtils.h"

#include <iostream>

// --------------------- QuadWarp Methods ---------------------

namespace QuadWarp
{

  struct WarpResult {
    QImage image;    // Das fertig transformierte Bild
    QPointF offset;  // Die Verschiebung relativ zum Original-Nullpunkt
  };
  
  WarpResult warp( const QImage& originalImage, const CageMesh& cageMesh )
  {
   qDebug() << "QuadWarp:warp(): useQuads =" << EditorStyle::instance().useCageQuads() << ", pointId =" << cageMesh.activeCagePointId();
   {
    if ( cageMesh.pointCount() < 4 ) {
      return { QImage(), QPointF(0,0) }; 
    }
    // compute bounding box
    QRectF dstBounds;
    for ( int i=0 ; i<cageMesh.pointCount() ; ++i ) {
      dstBounds = dstBounds.united(QRectF(cageMesh.point(i), QSizeF(1,1)));
    }
    QImage warped(int(dstBounds.width()), int(dstBounds.height()), QImage::Format_ARGB32);
    warped.fill(Qt::transparent);
    // compute new target image (transparent background)
    int rows = cageMesh.rows();
    int cols = cageMesh.cols();
    int gx = cageMesh.activeCagePointId() % cols;
    int gy = cageMesh.activeCagePointId() / rows;
    
    int xStart = std::max(0, gx - 1);
    int xEnd   = std::min(cols - 2, gx);
    int yStart = std::max(0, gy - 1);
    int yEnd   = std::min(rows - 2, gy);

    for ( int y = yStart; y <= yEnd; ++y ) {
      for ( int x = xStart; x <= xEnd; ++x ) {
        int i00 = y * cols + x;
        int i10 = i00 + 1;
        int i01 = i00 + cols;
        int i11 = i01 + 1;
        float srcL = float(x) * originalImage.width() / (cols - 1);
        float srcR = float(x + 1) * originalImage.width() / (cols - 1);
        float srcT = float(y) * originalImage.height() / (rows - 1);
        float srcB = float(y + 1) * originalImage.height() / (rows - 1);
        QVector<QPointF> dstQuad{
            cageMesh.point(i00) - dstBounds.topLeft(),
            cageMesh.point(i10) - dstBounds.topLeft(),
            cageMesh.point(i11) - dstBounds.topLeft(),
            cageMesh.point(i01) - dstBounds.topLeft()
        };
        QRect br = QPolygonF(dstQuad).boundingRect().toAlignedRect();
        br &= warped.rect();
        for ( int py = br.top(); py <= br.bottom(); ++py ) {
            QRgb* scanline = reinterpret_cast<QRgb*>(warped.scanLine(py));
            for ( int px = br.left(); px <= br.right(); ++px ) {
                QPointF p(px, py);
                if ( !GeometryUtils::pointInQuad(p, dstQuad) )
                    continue;
                QPointF uv = GeometryUtils::getBilinearUV(p, dstQuad);
                float srcX = srcL + uv.x() * (srcR - srcL);
                float srcY = srcT + uv.y() * (srcB - srcT);
                if ( srcX >= 0 && srcX < originalImage.width() &&
                    srcY >= 0 && srcY < originalImage.height() ) {
                    scanline[px] = originalImage.pixel(int(srcX), int(srcY));
                }
            }
        }
      }
    }
    return { warped, QPointF(0,0) };
   }
  }
  
} 