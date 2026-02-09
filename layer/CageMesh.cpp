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

#include "CageMesh.h"

#include <QDebug>
#include <QtMath>
#include <QLine>

#include <iostream>
 
// ------------------------------  ------------------------------
CageMesh::CageMesh() {}

// ------------------------------  ------------------------------
void CageMesh::printself()
{
  qDebug() << "CageMesh::printself(): size =" << m_cols << "x" << m_rows;
  qDebug() << m_points;
  qDebug() << m_originalPoints;
}

// ------------------------------  ------------------------------
void CageMesh::create( const QRectF& bounds, int cols, int rows )
{
  std::cout << "CageMesh::create(): cols=" << cols << ", rows=" << rows << std::endl;
  {
    m_cols = cols;
    m_rows = rows;
    m_points.clear();
    m_originalPoints.clear();
    m_springs.clear();
    if ( cols < 2 || rows < 2 )
      return;
    const qreal dx = bounds.width()  / (cols - 1);
    const qreal dy = bounds.height() / (rows - 1);
    for ( int y = 0; y < rows; ++y ) {
        for ( int x = 0; x < cols; ++x ) {
            QPointF p(
                bounds.left() + x * dx,
                bounds.top()  + y * dy
            );
            m_points.push_back(p);
            m_originalPoints.push_back(p);
        }
    }
    // Federn: horizontal + vertikal
    auto idx = [&]( int x, int y ) { return y * cols + x; };
    for ( int y = 0; y < rows; ++y ) {
        for ( int x = 0; x < cols; ++x ) {
            if (x + 1 < cols) addSpring(idx(x,y), idx(x+1,y));
            if (y + 1 < rows) addSpring(idx(x,y), idx(x,y+1));
        }
    }
  }
}

void CageMesh::update( int cols, int rows ) 
{
  std::cout << "CageMesh::update(): cols=" << cols << ", rows=" << rows << std::endl;
  {
    
  }
}

void CageMesh::setActive( bool active )
{
	m_active = active;
}

void CageMesh::addSpring(int a, int b)
{
    qreal len = QLineF(m_points[a], m_points[b]).length();
    m_springs.push_back({a, b, len});
}

QPointF CageMesh::point( int idx ) const
{
    return (idx >= 0 && idx < m_points.size())
           ? m_points[idx]
           : QPointF();
}

QPointF CageMesh::originalPoint( int idx ) const
{
    return (idx >= 0 && idx < m_originalPoints.size())
           ? m_originalPoints[idx]
           : QPointF();
}

void CageMesh::setPoint( int i, const QPointF& pos )
{
    if ( i < 0 || i >= m_points.size() )
      return;
    m_points[i] = pos;
    enforceConstraints(i);
}

void CageMesh::setPoints( const QVector<QPointF>& pts )
{
  std::cout << "CageMesh::setPoints(): points=" << pts.size() << std::endl;
  {
    if ( pts.size() != m_points.size() ) {
        printself();
        return;
    }
    m_points = pts;
  }
}

void CageMesh::relax( int iterations )
{
  // std::cout << "CageMesh::relax(): niterations=" << iterations << std::endl;
  {
    const qreal stiffness = 0.5; // Federhärte
    for ( int it = 0; it < iterations; ++it ) {
        for ( const auto& s : m_springs ) {
            QPointF& p1 = m_points[s.a];
            QPointF& p2 = m_points[s.b];
            QPointF delta = p2 - p1;
            qreal dist = std::hypot(delta.x(), delta.y());
            if (dist < 1e-6) continue;
            qreal diff = (dist - s.restLength) / dist;
            QPointF corr = delta * 0.5 * stiffness * diff;
            p1 += corr;
            p2 -= corr;
        }
    }
  }
}

void CageMesh::enforceConstraints( int idx )
{
  // std::cout << "CageMesh::enforceConstraints(): index=" << idx << std::endl;
  {
    const QPointF p = m_points[idx];
    const int x = idx % m_cols;
    const int y = idx / m_cols;
    auto constrainNeighbor = [&](int nx, int ny) {
        if ( nx < 0 || ny < 0 || nx >= m_cols || ny >= m_rows )
            return;
        int nIdx = ny * m_cols + nx;
        QPointF d = m_points[nIdx] - p;
        qreal len = std::hypot(d.x(), d.y());
        if ( len < m_minSpacing && len > 1e-4 ) {
            QPointF dir = d / len;
            m_points[nIdx] = p + dir * m_minSpacing;
        }
    };
    constrainNeighbor(x - 1, y);
    constrainNeighbor(x + 1, y);
    constrainNeighbor(x, y - 1);
    constrainNeighbor(x, y + 1);
  }
}

QRectF CageMesh::boundingRect() const
{
    QRectF r;
    for ( const auto& p : m_points )
        r |= QRectF(p, QSizeF(1,1));
    return r;
}
