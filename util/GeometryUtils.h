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

namespace GeometryUtils
{
    // Berechnet eine 3x3 homogene Transformationsmatrix, die quadSrc auf quadDst abbildet
    QTransform quadToQuad( const QPolygonF& src, const QPolygonF& dst )
    {
        if ( src.size() != 4 || dst.size() != 4 )
            return QTransform();
        // Einfacher Ansatz: affine Transformationsmatrix von 4 Punkten
        QTransform t;
        t.setMatrix(
            dst[1].x() - dst[0].x(), dst[2].x() - dst[0].x(), dst[0].x(),
            dst[1].y() - dst[0].y(), dst[2].y() - dst[0].y(), dst[0].y(),
            0, 0, 1
        );
        return t;
    }
    
    // Barycentric Mapping eines Pixels innerhalb eines Dreiecks
    QPointF barycentric( const QPointF& p, const QVector<QPointF>& triSrc, const QVector<QPointF>& triDst )
    {
       Q_ASSERT(triSrc.size() == 3 && triDst.size() == 3);
       // Dreieck in Vektoren
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
    
    // Prüft, ob ein Punkt innerhalb eines Dreiecks liegt
    bool pointInTriangle( const QPointF& p, const QVector<QPointF>& tri )
    {
       QPointF a = tri[0], b = tri[1], c = tri[2];
       double s = a.y()*c.x() - a.x()*c.y() + (c.y() - a.y())*p.x() + (a.x() - c.x())*p.y();
       double t = a.x()*b.y() - a.y()*b.x() + (a.y() - b.y())*p.x() + (b.x() - a.x())*p.y();
       if ((s < 0) != (t < 0)) return false;
       double A = -b.y()*c.x() + a.y()*(c.x() - b.x()) + a.x()*(b.y() - c.y()) + b.x()*c.y();
       return A < 0 ? (s <= 0 && s+t >= A) : (s >= 0 && s+t <= A);
    }
    
    QTransform triangleToTriangle( const QPointF& a1, const QPointF& b1, const QPointF& c1,
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

    // Konvertiert Scene-Koordinaten in Layer-Koordinaten
    inline QPointF sceneToLayer( const QPointF& scenePos, const QTransform& layerTransform )
    {
        return layerTransform.inverted().map(scenePos);
    }
}

