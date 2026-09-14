/* 
* Copyright 2026 Forschungszentrum J?lich
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
  
  // --- --- ---
  void transformQuad( int x, int y, int cols, int rows, const QImage& originalImage, QImage& warped, 
                   const CageMesh& cageMesh, const QRectF& dstBounds ) {
    // Bounds-Check: Existiert das Quad (x, y) bis (x+1, y+1)?
    if ( x < 0 || x + 1 >= cols || y < 0 || y + 1 >= rows ) return;

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
            if ( !GeometryUtils::pointInQuad(p, dstQuad) ) continue;
            QPointF uv = GeometryUtils::getBilinearUV(p,dstQuad);
            float srcX = srcL + uv.x() * (srcR - srcL);
            float srcY = srcT + uv.y() * (srcB - srcT);
            if ( srcX >= 0 && srcX < originalImage.width() && 
                srcY >= 0 && srcY < originalImage.height() ) {
                scanline[px] = originalImage.pixel(int(srcX), int(srcY));
            }
        }
    }
    
  }

  WarpResult warp( QImage & currentImage, const QImage& originalImage, const CageMesh& cageMesh )
  {
   qCDebug(logEditor) << "TriangleWarp:warp(): useQuads =" << EditorStyle::instance().useCageQuads();
   {
    if ( cageMesh.pointCount() < 4 ) {
      return { QImage(), QPointF(0,0) }; 
    }

    // compute bounding box
    QRectF dstBounds;
    for ( int i=0 ; i<cageMesh.pointCount() ; ++i ) {
      dstBounds = dstBounds.united(QRectF(cageMesh.point(i), QSizeF(1,1)));
    }
    // create warped image: QImage warped = m_originalImage; // Ausgangsbild

    QImage warped(int(dstBounds.width()), int(dstBounds.height()), QImage::Format_ARGB32);
    warped.fill(Qt::transparent);  // this is QColor(0,0,0,0) = transparent black color

    // compute new target image (transparent background)
    int rows = cageMesh.rows();
    int cols = cageMesh.cols();

    if ( EditorStyle::instance().useCageQuads() == true ) {

     // --- QUAD WARP (default) 
     if ( EditorStyle::instance().useClaudeQuads() == true ) {

      int count = 0;
      int numcalls = 0;

      // -- always use GeometryUtils::barycentric, which uses iterations and is more "exact".
      for ( int y = 0; y + 1 < rows; ++y ) {
       for ( int x = 0; x + 1 < cols; ++x ) {
        int i00 = y * cols + x;
        int i10 = i00 + 1;
        int i01 = i00 + cols;
        int i11 = i01 + 1;

        int updateQuad = true;
        int pointID = cageMesh.activeCagePointId();
        if( pointID != -1 ) {
          updateQuad = false;
          if( pointID == i00 || pointID == i10 || pointID == i01 || pointID == i11 ) {
            updateQuad = true;
            std::cout << "Updating quad " << i00 << ", " << i10 << ", " << i01 << ", " << i11 << std::endl;
          }
        }
// CLAUDE: added qreal cast to avoid truncating to int (cageMesh is done like this).
        QVector<QPointF> srcQuad{
            QPointF(x * (qreal)originalImage.width() / (cols - 1), y * (qreal)originalImage.height() / (rows - 1)),
            QPointF((x + 1) * (qreal)originalImage.width() / (cols - 1), y * (qreal)originalImage.height() / (rows - 1)),
            QPointF((x + 1) * (qreal)originalImage.width() / (cols - 1), (y + 1) * (qreal)originalImage.height() / (rows - 1)),
            QPointF(x * (qreal)originalImage.width() / (cols - 1), (y + 1) * (qreal)originalImage.height() / (rows - 1))
        };
        QVector<QPointF> dstQuad{
            cageMesh.point(i00) - dstBounds.topLeft(),
            cageMesh.point(i10) - dstBounds.topLeft(),
            cageMesh.point(i11) - dstBounds.topLeft(),
            cageMesh.point(i01) - dstBounds.topLeft()
        };
        QRectF br = QPolygonF(dstQuad).boundingRect();
        QPointF start(-1,-1);
        double firstY = -1.0;

        // Note for below: QPointF.toPoint() rounds to the nearest integer point,
        // which can be wrong. For example, if we have x=47.8, this point is in
        // pixel 47 but after rounding it becomes in pixel 48, which may be outside
        // the bounding box. While point 47 is valid and inside the bounding box,
        // it will not be drawn (and left black) if it's been set to belong to
        // point 48. So we should avoid toPoint() and use std::floor(x) instead.

        for ( int py = int(br.top()); py <= int(br.bottom()); ++py ) {
            start.setX(-1);  // reset at start of row
            start.setY(firstY);  // reset at start of row
            int saveY = 1;
            for ( int px = int(br.left()); px <= int(br.right()); ++px ) {
                QPointF p(px, py);

                if ( !GeometryUtils::pointInQuad(p, dstQuad) ) continue;

                if( updateQuad ) {

                  numcalls++;
                  QPointF srcP = GeometryUtils::barycentric(p, dstQuad, srcQuad, &count, start );
                  if( saveY ) {
                    firstY = start.y();  // first Y of the row remembered for next row
                    saveY = 0;
                  }

                  // if ( originalImage.rect().contains(srcP.toPoint()) ) {  this is rounding.
                  if( originalImage.rect().contains( QPoint( std::floor(srcP.x()), std::floor(srcP.y()) ) ) ) {

#if 1
                    // nearest neighbour. Now this is an issue too. On the original image,
                    // with no displacement, barycentric() may return a floating point with
                    // "noise", like 3.0 +/- 1.0e-6. This can cause randomness in selecting
                    // the "good" voxel. So here use toPoint() for consistent rounding.

                    QRgb c = ( originalImage.rect().contains(srcP.toPoint()) ) ?
                             originalImage.pixel(srcP.toPoint()) :
                             originalImage.pixel( QPoint( std::floor(srcP.x()), std::floor(srcP.y()) ) );
                    warped.setPixel(px, py, c );

#else
                    // bilinear interpolation
                    qreal dx = srcP.x() - std::floor( srcP.x() );
                    qreal dy = srcP.y() - std::floor( srcP.y() );

                    QPoint p4( std::floor(srcP.x()), std::floor(srcP.y()) );
                    QRgb c4 = originalImage.pixel( p4 );
                    QPoint p3( std::floor(srcP.x()+1.0), std::floor(srcP.y()) );
                    QRgb c3 = ( originalImage.rect().contains(p3) ) ? originalImage.pixel( p3 ) : c4;
                    QPoint p2( std::floor(srcP.x()+1.0), std::floor(srcP.y()+1.0) );
                    QRgb c2 = ( originalImage.rect().contains(p2) ) ? originalImage.pixel( p2 ) : c4;
                    QPoint p1( std::floor(srcP.x()), std::floor(srcP.y()+1.0) );
                    QRgb c1 = ( originalImage.rect().contains(p1) ) ? originalImage.pixel( p1 ) : c4;

                    if( qAlpha(c1) < 10 ) c1 = c4;  // this is the pixel for nearest neighbour
                    if( qAlpha(c2) < 10 ) c2 = c4;
                    if( qAlpha(c3) < 10 ) c3 = c4;

                    int red = ( 1.0 - dx ) * dy * qRed( c1 ) +
                              dx * dy * qRed( c2 ) +
                              dx * ( 1.0 - dy ) * qRed( c3 ) +
                              ( 1.0 - dx ) * ( 1.0 - dy ) * qRed( c4 );
                    int green = ( 1.0 - dx ) * dy * qGreen( c1 ) +
                                dx * dy * qGreen( c2 ) +
                                dx * ( 1.0 - dy ) * qGreen( c3 ) +
                                ( 1.0 - dx ) * ( 1.0 - dy ) * qGreen( c4 );
                    int blue = ( 1.0 - dx ) * dy * qBlue( c1 ) +
                               dx * dy * qBlue( c2 ) +
                               dx * ( 1.0 - dy ) * qBlue( c3 ) +
                               ( 1.0 - dx ) * ( 1.0 - dy ) * qBlue( c4 );
                    int alpha = ( 1.0 - dx ) * dy * qAlpha( c1 ) +
                               dx * dy * qAlpha( c2 ) +
                               dx * ( 1.0 - dy ) * qAlpha( c3 ) +
                               ( 1.0 - dx ) * ( 1.0 - dy ) * qAlpha( c4 );

                    alpha = ( alpha > 10 ) ? 255 : 0;
                    warped.setPixel(px, py, qRgba( red, green, blue, alpha ) );
                    // warped.setPixel(px, py, qRgba( qAlpha(c), 0, 0, qAlpha(c) ) );

#endif
                  } else {
                    // This voxel is inside the new cage but outside the 
                    // bounding box of the original layer. The cage after 
                    // should map perfectly to the cage before, minus some
                    // rounding errors. This will occur along the top row
                    // and left column of the cage where floor() interpolation
                    // can cause the (top/left) corner of the voxel to be
                    // outside. Clamp the local coordinates "start" to project
                    // onto the border of the cage.

                    if ( GeometryUtils::pointInQuad(srcP, srcQuad) ) {
                      qreal xi = ( start.x() <= -1.0 ) ? -1.0 : ( start.x() > 1.0 ? 1.0 : start.x() );
                      qreal eta = ( start.y() <= -1.0 ) ? -1.0 : ( start.y() > 1.0 ? 1.0 : start.y() );
                      srcP.setX( 0.25 * ( (1.0-xi)*(1.0-eta)*srcQuad[0].x() +
                                          (1.0+xi)*(1.0-eta)*srcQuad[1].x() +
                                          (1.0+xi)*(1.0+eta)*srcQuad[2].x() +
                                          (1.0-xi)*(1.0+eta)*srcQuad[3].x() ) );
                      srcP.setY( 0.25 * ( (1.0-xi)*(1.0-eta)*srcQuad[0].y() +
                                          (1.0+xi)*(1.0-eta)*srcQuad[1].y() +
                                          (1.0+xi)*(1.0+eta)*srcQuad[2].y() +
                                          (1.0-xi)*(1.0+eta)*srcQuad[3].y() ) );

                      if( originalImage.rect().contains( srcP.toPoint() ) ) {
                        warped.setPixel(px, py, originalImage.pixel(srcP.toPoint()) );
                      }
                    }

                  }
                } else {
 
                  // This quad does not need to be updated, so simply copy the
                  // image from the previous (currrent) image to the new one (warped).

                  QPointF pp(px, py);
                  pp += cageMesh.getOffset(1);

#if 1
                  if( currentImage.rect().contains(pp.toPoint()) ) {
                    QRgb c = currentImage.pixel(pp.toPoint() );   // WRONG: toPoint() is rounding, not flooring
                    warped.setPixel(px, py, c );
                  }
#else
                  if( currentImage.rect().contains( QPoint( std::floor(pp.x()), std::floor(pp.y()) ) ) ) {
                    QRgb c = currentImage.pixel( QPoint( std::floor(pp.x()), std::floor(pp.y()) ) );
                    warped.setPixel(px, py, c );
                  }
#endif
                }
            }
        }
       }
      }

     } else {
        
      for ( int y = 0; y + 1 < rows; ++y ) {
       for ( int x = 0; x + 1 < cols; ++x ) {
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
                if ( !GeometryUtils::pointInQuad(p, dstQuad) ) continue;
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
     
     }
    
    } else {
    
     // --- TRIANGLE WARP ---
     int count = 0;
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
                cageMesh.point(i00) - dstBounds.topLeft(),
                cageMesh.point(i10) - dstBounds.topLeft(),
                cageMesh.point(i01) - dstBounds.topLeft(),
                cageMesh.point(i11) - dstBounds.topLeft()
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
                    QPointF p(px, py);
                    QPointF start(0,0);
                    if ( !GeometryUtils::pointInTriangle(p, tri1Dst) ) continue;
                    QPointF srcP = GeometryUtils::barycentric(p, tri1Dst, tri1, &count, start );
                    if ( !originalImage.rect().contains(srcP.toPoint()) ) continue;
                    QColor c = originalImage.pixelColor(int(srcP.x()), int(srcP.y()));
                    warped.setPixelColor(px, py, c);
                }
            }
            // triangle 2
            QRectF br2 = QPolygonF(tri2Dst).boundingRect();
            for ( int py = int(br2.top()); py <= int(br2.bottom()); ++py ) {
                for ( int px = int(br2.left()); px <= int(br2.right()); ++px ) {
                    QPointF p(px, py);
                    QPointF start(0,0);
                    if ( !GeometryUtils::pointInTriangle(p, tri2Dst) ) continue;
                    QPointF srcP = GeometryUtils::barycentric(p, tri2Dst, tri2, &count, start );
                    if ( !originalImage.rect().contains(srcP.toPoint()) ) continue;
                    QColor c = originalImage.pixelColor(int(srcP.x()), int(srcP.y()));
                    warped.setPixelColor(px, py, c);
                }
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
            // Clipping auf das Zieldreieck, um ?berlappungen zu vermeiden
            QPainterPath path;
            path.addPolygon(sourcePoly);
            painter->setClipPath(path);
            painter->drawImage(0, 0, src);
            painter->restore();
        }
  }
  
  // --- OLD CODE ---
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
