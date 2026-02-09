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

// --------------------- TriangleWarp Methods ---------------------
namespace TriangleWarp
{

  struct WarpResult {
    QImage image;    // Das fertig transformierte Bild
    QPointF offset;  // Die Verschiebung relativ zum Original-Nullpunkt
  };

  WarpResult warp( const QImage& originalImage, const CageMesh& mesh )
  {
   std::cout << "WarpResult:warp(): Processing..." << std::endl;
   {
    if ( mesh.pointCount() < 4 || !mesh.isActive() ) return { QImage(), QPointF(0,0) };
    // compute bounding box
    QRectF dstBounds;
    for ( int i=0 ; i<mesh.pointCount() ; ++i ) {
      dstBounds = dstBounds.united(QRectF(mesh.point(i), QSizeF(1,1)));
    }
    // create warped image: QImage warped = m_originalImage; // Ausgangsbild
    std::cout << " oldsize=" << originalImage.width() << "x" << originalImage.height() << std::endl;
    std::cout << " newsize=" << int(dstBounds.width()) << "x" << int(dstBounds.height()) << std::endl;
    QImage warped(int(dstBounds.width()), int(dstBounds.height()), QImage::Format_ARGB32);
    warped.fill(Qt::transparent);
    // compute new target image (transparent background)
    int rows = mesh.rows();
    int cols = mesh.cols();
    for ( int y = 0; y + 1 < rows; ++y ) {
        for ( int x = 0; x + 1 < cols; ++x ) {
            int i00 = y*cols + x;
            int i10 = i00 + 1;
            int i01 = i00 + cols;
            int i11 = i01 + 1;
            QVector<QPointF> srcQuad{
                QPointF(x * originalImage.width() / (cols-1), y * originalImage.height() / (rows-1)),
                QPointF((x+1) * originalImage.width() / (cols-1), y * originalImage.height() / (rows-1)),
                QPointF(x * originalImage.width() / (cols-1), (y+1) * originalImage.height() / (rows-1)),
                QPointF((x+1) * originalImage.width() / (cols-1), (y+1) * originalImage.height() / (rows-1))
            };
            QVector<QPointF> dstQuad{
                mesh.point(i00) - dstBounds.topLeft(),
                mesh.point(i10) - dstBounds.topLeft(),
                mesh.point(i01) - dstBounds.topLeft(),
                mesh.point(i11) - dstBounds.topLeft()
            };
            // two tris per quad
            QVector<QPointF> tri1{srcQuad[0], srcQuad[1], srcQuad[2]};
            QVector<QPointF> tri1Dst{dstQuad[0], dstQuad[1], dstQuad[2]};
            QVector<QPointF> tri2{srcQuad[1], srcQuad[2], srcQuad[3]};
            QVector<QPointF> tri2Dst{dstQuad[1], dstQuad[2], dstQuad[3]};
            // triangle 1
            QRectF br1 = QPolygonF(tri1Dst).boundingRect();
            for ( int py = int(br1.top()); py <= int(br1.bottom()); ++py ) {
                for ( int px = int(br1.left()); px <= int(br1.right()); ++px ) {
                    QPointF p(px + 0.5, py + 0.5);
                    if ( !GeometryUtils::pointInTriangle(p, tri1Dst) ) continue;
                    QPointF srcP = GeometryUtils::barycentric(p, tri1Dst, tri1);
                    if ( !originalImage.rect().contains(srcP.toPoint()) ) continue;
                    QColor c = originalImage.pixelColor(int(srcP.x()), int(srcP.y()));
                    warped.setPixelColor(px, py, c);
                }
            }
            // triangle 2
            QRectF br2 = QPolygonF(tri2Dst).boundingRect();
            for ( int py = int(br2.top()); py <= int(br2.bottom()); ++py ) {
                for ( int px = int(br2.left()); px <= int(br2.right()); ++px ) {
                    QPointF p(px + 0.5, py + 0.5);
                    if ( !GeometryUtils::pointInTriangle(p, tri2Dst) ) continue;
                    QPointF srcP = GeometryUtils::barycentric(p, tri2Dst, tri2);
                    if ( !originalImage.rect().contains(srcP.toPoint()) ) continue;
                    QColor c = originalImage.pixelColor(int(srcP.x()), int(srcP.y()));
                    warped.setPixelColor(px, py, c);
                }
            }
        }
    }
    return { warped, QPointF(0,0) };
   }
  }
  
  // --------------------- helper ---------------------
  void drawTriangle( QPainter *painter, const QImage &src, QPointF s1, QPointF s2, QPointF s3,
                             QPointF t1, QPointF t2, QPointF t3 ) 
  {
        QPolygonF sourcePoly; sourcePoly << s1 << s2 << s3;
        QPolygonF targetPoly; targetPoly << t1 << t2 << t3;
        // Berechne die affine Transformation von Source-Dreieck zu Target-Dreieck
        QTransform trans;
        if (QTransform::quadToQuad(sourcePoly, targetPoly, trans)) {
            painter->save();
            painter->setTransform(trans, true);
            // Clipping auf das Zieldreieck, um Überlappungen zu vermeiden
            QPainterPath path;
            path.addPolygon(sourcePoly);
            painter->setClipPath(path);
            painter->drawImage(0, 0, src);
            painter->restore();
        }
    }
  
  WarpResult warp2( const QImage& originalImage, const CageMesh& mesh ) 
  {
    if ( mesh.pointCount() < 4 || !mesh.isActive() ) return { QImage(), QPointF(0,0) };
    {
     // --- prepare ---
     QVector<QPointF> sourceGrid;
     QVector<QPointF> targetGrid = mesh.points();
     int resX = mesh.rows();
     int resY = mesh.cols();
     for ( int y = 0; y < resX; ++y ) {
        for ( int x = 0; x < resY; ++x ) {
            sourceGrid << QPointF(x * originalImage.width() / (resY-1), y * originalImage.height() / (resX-1));
        }
     }
     // --- process ---
     float minX = targetGrid[0].x(), maxX = targetGrid[0].x();
     float minY = targetGrid[0].y(), maxY = targetGrid[0].y();
     for (const QPointF &p : targetGrid) {
        if (p.x() < minX) minX = p.x();
        if (p.x() > maxX) maxX = p.x();
        if (p.y() < minY) minY = p.y();
        if (p.y() > maxY) maxY = p.y();
     }
     QSize newSize(qCeil(maxX - minX) + 2, qCeil(maxY - minY) + 2);
     QPointF offset(-minX + 1, -minY + 1);
     QImage result(newSize, QImage::Format_ARGB32_Premultiplied);
     result.fill(Qt::transparent);
     QPainter painter(&result);
     painter.setRenderHint(QPainter::SmoothPixmapTransform);
     painter.translate(offset);
     for (int y = 0; y < resY - 1; ++y) {
        for (int x = 0; x < resX - 1; ++x) {
            int idx0 = y * resX + x;
            int idx1 = y * resX + (x + 1);
            int idx2 = (y + 1) * resX + x;
            int idx3 = (y + 1) * resX + (x + 1);
            drawTriangle(&painter, originalImage, 
                         sourceGrid[idx0], sourceGrid[idx1], sourceGrid[idx2],
                         targetGrid[idx0], targetGrid[idx1], targetGrid[idx2]);
            drawTriangle(&painter, originalImage, 
                         sourceGrid[idx1], sourceGrid[idx2], sourceGrid[idx3],
                         targetGrid[idx1], targetGrid[idx2], targetGrid[idx3]);
        }
     }
     return { result, -offset };
    }
  }

}