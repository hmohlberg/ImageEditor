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

#include <QImage>
#include <QPointF>
#include <QPolygonF>
#include <QTransform>
#include <QGraphicsPixmapItem>

namespace GeometryUtils
{
    
    // >>>
    inline QString getGeometryString( QGraphicsPixmapItem *item ) {
       if ( !item ) return QString();
       int w = item->pixmap().width();
       int h = item->pixmap().height();
       int x = static_cast<int>(item->scenePos().x());
       int y = static_cast<int>(item->scenePos().y());
       return QString("%1x%2+%3+%4").arg(w).arg(h).arg(x).arg(y);
    }
    
    // >>>
    inline QPointF getBilinearCoords(const QPointF &p, const QVector<QPointF> &dstQuad, const QVector<QPointF> &srcQuad) {
       // Relative Koordinaten u, v im Einheitsquadrat berechnen (0 bis 1)
       // Wir nutzen hier die dstQuad Eckpunkte: p00, p10, p11, p01
       QPointF p00 = dstQuad[0];
       QPointF p10 = dstQuad[1];
       QPointF p11 = dstQuad[2];
       QPointF p01 = dstQuad[3];
       // Vektoren für die Gleichung
       QPointF e = p10 - p00;
       QPointF f = p01 - p00;
       QPointF g = p11 - p10 - p01 + p00;
       QPointF h = p - p00;
       // Koeffizienten der quadratischen Gleichung für 'v'
       double k2 = g.x() * f.y() - g.y() * f.x();
       double k1 = e.x() * f.y() - e.y() * f.x() + h.x() * g.y() - h.y() * g.x();
       double k0 = h.x() * e.y() - h.y() * e.x();
       double v = 0;
       if (qAbs(k2) < 1e-6) {
        // Lineares Case (Trapez oder Parallelogramm)
        v = -k0 / k1;
       } else {
        // Quadratisches Case (Allgemeines Viereck)
        double delta = k1 * k1 - 4.0 * k2 * k0;
        v = (-k1 + qSqrt(qAbs(delta))) / (2.0 * k2);
       }
       // Basierend auf v berechnen wir u
       double u = (h.x() - f.x() * v) / (e.x() + g.x() * v);
       // Falls Division durch Null droht (Vektor e.x fast 0), nutze y-Komponente
       if (qAbs(e.x() + g.x() * v) < 1e-6) {
        u = (h.y() - f.y() * v) / (e.y() + g.y() * v);
       }
       // Jetzt interpolieren wir im Quell-Rechteck (srcQuad) mit u und v
       // Bilineare Interpolation: 
       // P_src = (1-u)(1-v)P00 + u(1-v)P10 + uvP11 + (1-u)vP01
       QPointF res = (1-u)*(1-v)*srcQuad[0] + u*(1-v)*srcQuad[1] + u*v*srcQuad[2] + (1-u)*v*srcQuad[3];
       return res;
    }
    
    // Calculates a 3x3 homogeneous transformation matrix that maps quadSrc to quadDst.
    inline QTransform quadToQuad( const QPolygonF& src, const QPolygonF& dst )
    {
        if ( src.size() != 4 || dst.size() != 4 )
            return QTransform();
        QTransform t;
        if ( !QTransform::quadToQuad(src, dst, t) ) {
          // fall back to old solution. not the best but better than nothing
          t.setMatrix(
             dst[1].x() - dst[0].x(), dst[2].x() - dst[0].x(), dst[0].x(),
             dst[1].y() - dst[0].y(), dst[2].y() - dst[0].y(), dst[0].y(),
             0, 0, 1
          );
        }
        return t;
    }
    
    // Barycentric mapping of a pixel within a triangle
    inline QPointF barycentric( const QPointF& p, const QVector<QPointF>& triSrc, const QVector<QPointF>& triDst )
    {
       // Q_ASSERT(triSrc.size() == 3 && triDst.size() == 3);
       if( triSrc.size() == 3 && triDst.size() == 3 ) {
         QPointF v0 = triSrc[1] - triSrc[0];
         QPointF v1 = triSrc[2] - triSrc[0];
         QPointF v2 = p - triSrc[0];
         double d00 = QPointF::dotProduct(v0, v0);
         double d01 = QPointF::dotProduct(v0, v1);
         double d11 = QPointF::dotProduct(v1, v1);
         double d20 = QPointF::dotProduct(v2, v0);
         double d21 = QPointF::dotProduct(v2, v1);
         double denom = d00 * d11 - d01 * d01;
         double v = (d11 * d20 - d01 * d21) / denom;
         double w = (d00 * d21 - d01 * d20) / denom;
         double u = 1.0 - v - w;
         QPointF mapped = u*triDst[0] + v*triDst[1] + w*triDst[2];
         return mapped;
       }
       if( triSrc.size() == 4 && triDst.size() == 4 ) {  // ok, it's a quad
         QPointF Q(0,0);
         QPointF v0 = triSrc[0] + triSrc[1] + triSrc[2] + triSrc[3];
         QPointF v1 = -triSrc[0] + triSrc[1] + triSrc[2] - triSrc[3];
         QPointF v2 = -triSrc[0] - triSrc[1] + triSrc[2] + triSrc[3];
         QPointF v3 = triSrc[0] - triSrc[1] + triSrc[2] - triSrc[3];
         for ( int iter = 0 ; iter < 10; iter++ ) {
           QPointF rhs = 4.0 * p - v0 - Q.x() * v1 - Q.y() * v2 - Q.x() * Q.y() * v3;
           double res = rhs.x() * rhs.x() + rhs.y() * rhs.y();
           if( res <= 1.0e-10 ) {
             break;
           }
           QPointF A0 = v1 + v3 * Q.y();
           QPointF A1 = v2 + v3 * Q.x();
           // matrix is  [ A0.x()  A1.x() ]
           //            [ A0.y()  A1.y() ]
           double det = A0.x() * A1.y() - A1.x() * A0.y();
           if( det >= 0.0 ) return p;  // in this orientation, need det < 0.  
           QPointF delta = { ( A1.y() * rhs.x() - A1.x() * rhs.y() ) / det,
                             ( A0.x() * rhs.y() - A0.y() * rhs.x() ) / det };
           Q += delta;
         }
         double weights[4] = { 0.25 * ( 1 - Q.x() ) * ( 1 - Q.y() ),
                               0.25 * ( 1 + Q.x() ) * ( 1 - Q.y() ),
                               0.25 * ( 1 + Q.x() ) * ( 1 + Q.y() ),
                               0.25 * ( 1 - Q.x() ) * ( 1 + Q.y() ) };
         QPointF mapped = weights[0] * triDst[0] + weights[1] * triDst[1] +
                          weights[2] * triDst[2] + weights[3] * triDst[3];
         return mapped;
       }
       return p;

    }

    // Checks whether a point lies within a triangle
    inline bool pointInTriangle( const QPointF& p, const QVector<QPointF>& tri )
    {
       QPointF a = tri[0], b = tri[1], c = tri[2];
       double s = a.y()*c.x() - a.x()*c.y() + (c.y() - a.y())*p.x() + (a.x() - c.x())*p.y();
       double t = a.x()*b.y() - a.y()*b.x() + (a.y() - b.y())*p.x() + (b.x() - a.x())*p.y();
       if ((s < 0) != (t < 0)) return false;
       double A = -b.y()*c.x() + a.y()*(c.x() - b.x()) + a.x()*(b.y() - c.y()) + b.x()*c.y();
       return A < 0 ? (s <= 0 && s+t >= A) : (s >= 0 && s+t <= A);
    }
    
    inline bool pointInQuad(const QPointF &p, const QVector<QPointF> &quad) {
       if ( quad.size() < 4 ) return false;
       QVector<QPointF> tri1 = {quad[0], quad[1], quad[2]};
       QVector<QPointF> tri2 = {quad[0], quad[2], quad[3]};  
       return pointInTriangle(p, tri1) || pointInTriangle(p, tri2);
    }
    
    inline QTransform triangleToTriangle( const QPointF& a1, const QPointF& b1, const QPointF& c1,
                                     const QPointF& a2, const QPointF& b2, const QPointF& c2 ) 
    {
		QTransform t1, t2;
        t1.setMatrix(
        	a1.x(), b1.x(), c1.x(),
        	a1.y(), b1.y(), c1.y(),
        	1, 1, 1
    	);
    	t2.setMatrix(
        	a2.x(), b2.x(), c2.x(),
        	a2.y(), b2.y(), c2.y(),
        	1, 1, 1
    	);
    	return t2 * t1.inverted();                                 
    }

    // Converts scene coordinates to layer coordinates
    inline QPointF sceneToLayer( const QPointF& scenePos, const QTransform& layerTransform )
    {
        return layerTransform.inverted().map(scenePos);
    }
}
