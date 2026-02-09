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

#include <QVector>
#include <QPointF>
#include <QRectF>

struct CageSpring {
    int a;
    int b;
    qreal restLength;
};

/*
 * CageMesh
 * --------
 * Repräsentiert ein regelmäßiges 2D-Gitter über einem Layer.
 * - speichert Original- und aktuelle Punkte
 * - verhindert Punktüberlappungen (Spring-Constraint)
 * - Grundlage für Piecewise-Affine-Warp (Triangle Warp)
 */
 
class CageMesh
{

public:
    CageMesh();

    void create( const QRectF& bounds, int cols, int rows );
    void update( int cols, int rows );
    
    int cols() const { return m_cols; }
    int rows() const { return m_rows; }
    
    const QVector<QPointF>& points() const { return m_points; }
    const QVector<QPointF>& originalPoints() const { return m_originalPoints; }

    int pointCount() const { return m_points.size(); }
    bool isActive() const { return m_active; }
    void setActive( bool );

    QPointF originalPoint( int i ) const;
    QPointF point( int i ) const;

    void setPoint( int i, const QPointF& pos );
    void relax( int iterations = 5 );
    
    void setPoints( const QVector<QPointF>& pts );
    void enforceConstraints( int idx );
    QRectF boundingRect() const;
    
    void addOffset( qreal x, qreal y ) { m_offset += QPointF(x,y); }
    QPointF getOffset() const { return m_offset; }
    void setOffset( qreal x = -0.5, qreal y = -0.5 ) { m_offset = QPointF(x,y);  }
    
    void printself();

private:

    bool m_active = true;
    
    int m_cols = 0;
    int m_rows = 0;
    
    QPointF m_offset = QPointF(0,0);
    
    QVector<QPointF> m_points;
    QVector<QPointF> m_originalPoints;
    QVector<CageSpring> m_springs;
    
    qreal m_minSpacing = 1.0;

    void addSpring( int a, int b );
    
};
