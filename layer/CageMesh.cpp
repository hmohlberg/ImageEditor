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
  qDebug() << "CageMesh::create(): cols=" << cols << ", rows=" << rows;
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

void CageMesh::update( const QRectF& bounds, int cols, int rows ) 
{
  if ( cols > m_cols ) {
    refine(bounds,cols,rows);
  } else {
    coarsen(bounds,cols,rows);
  }
}

void CageMesh::coarsen( const QRectF& bounds, int newCols, int newRows )
{
    QVector<QPointF> nextPoints;
    QVector<QPointF> nextOriginalPoints;
    nextPoints.reserve(newCols * newRows);
    nextOriginalPoints.reserve(newCols * newRows);
    for ( int y = 0; y < newRows; ++y ) {
        for ( int x = 0; x < newCols; ++x ) {
            int oldIndex = (y * 2) * m_cols + (x * 2);
            nextPoints.push_back(m_points[oldIndex]);
            nextOriginalPoints.push_back(m_originalPoints[oldIndex]);
        }
    }

    // Daten aktualisieren
    m_points = nextPoints;
    m_originalPoints = nextOriginalPoints;
    m_cols = newCols;
    m_rows = newRows;

    // Wichtig: Federn müssen komplett neu aufgebaut werden!
    rebuildSprings();
}

void CageMesh::refine( const QRectF& bounds, int newCols, int newRows ) 
{
    QVector<QPointF> nextPoints;
    QVector<QPointF> nextOriginalPoints;
    nextPoints.reserve(newCols * newRows);
    nextOriginalPoints.reserve(newCols * newRows);

    // Hilfsfunktion zur Interpolation von m_points
    auto getPoint = [&]( int oldX, int oldY ) {
        return m_points[oldY * m_cols + oldX];
    };
    const qreal dx = bounds.width()  / (newCols - 1);
    const qreal dy = bounds.height() / (newRows - 1);
    for ( int y = 0; y < newRows; ++y ) {
        for ( int x = 0; x < newCols; ++x ) {
        
            // Berechne die Position im alten Gitter (0.0, 0.5, 1.0, 1.5...)
            qreal oldX_f = x / 2.0;
            qreal oldY_f = y / 2.0;
            int x0 = std::floor(oldX_f);
            int x1 = std::ceil(oldX_f);
            int y0 = std::floor(oldY_f);
            int y1 = std::ceil(oldY_f);
            // Bilineare Interpolation für m_points
            qreal tx = oldX_f - x0;
            qreal ty = oldY_f - y0;
            QPointF p00 = getPoint(x0, y0);
            QPointF p10 = getPoint(x1, y0);
            QPointF p01 = getPoint(x0, y1);
            QPointF p11 = getPoint(x1, y1);
            QPointF interpolatedPoint = (1-tx)*(1-ty)*p00 + tx*(1-ty)*p10 + (1-tx)*ty*p01 + tx*ty*p11;
            nextPoints.push_back(interpolatedPoint);
            
            // Für OriginalPoints berechnen wir einfach das neue regelmäßige Gitter
            // (Oder nutzen dieselbe Interpolation auf m_originalPoints)
            QPointF p( bounds.left() + x * dx, bounds.top()  + y * dy );
            nextOriginalPoints.push_back(p);
        }
    }
    
    m_originalPoints = nextOriginalPoints;
    m_points = nextPoints;
    m_cols = newCols;
    m_rows = newRows;
    
    // Federn: horizontal + vertikal
    rebuildSprings();
    
}

void CageMesh::rebuildSprings()
{
    m_springs.clear();
    auto idx = [&]( int x, int y ) { return y * m_cols + x; };
    for ( int y = 0; y < m_rows; ++y ) {
        for ( int x = 0; x < m_cols; ++x ) {
            if ( x + 1 < m_cols ) addSpring(idx(x,y), idx(x+1,y));
            if ( y + 1 < m_rows ) addSpring(idx(x,y), idx(x,y+1));
        }
    }
}

// ----------------- THE DIAGONAL SPRING MODELL -----------------
void CageMesh::addNewSpring(int idxA, int idxB)
{
    // 1. Neues Feder-Objekt erstellen
    CageSpring s;
    s.a = idxA;
    s.b = idxB;

    // 2. Ruhelänge berechnen
    // Wir nutzen m_originalPoints, damit die Feder "weiß", 
    // wie weit die Punkte im Idealzustand voneinander entfernt sind.
    QPointF pA = m_originalPoints[idxA];
    QPointF pB = m_originalPoints[idxB];
    
    QPointF delta = pB - pA;
    
    // Distanz berechnen: sqrt(dx*dx + dy*dy)
    s.restLength = std::hypot(delta.x(), delta.y());

    // 3. In die Liste einfügen
    m_springs.append(s);
}

/*
void CageMesh::rebuildSprings()
{
    m_springs.clear();
    for (int y = 0; y < m_rows; ++y) {
        for (int x = 0; x < m_cols; ++x) {
            int current = y * m_cols + x;
            // --- 1. STRUKTURFEDERN (Nachbarn) ---
            if (x < m_cols - 1) addSpring(current, current + 1); // Horizontal
            if (y < m_rows - 1) addSpring(current, current + m_cols); // Vertikal
            // --- 2. SCHERFEDERN (Diagonalen) ---
            if (x < m_cols - 1 && y < m_rows - 1) {
                addNewSpring(current, (y + 1) * m_cols + (x + 1)); // TL -> BR
                addNewSpring(y * m_cols + (x + 1), (y + 1) * m_cols + x); // TR -> BL
            }
            // --- 3. BIEGEFEDERN (Überspringen einen Punkt) ---
            // Horizontaler Widerstand gegen Knicke
            if (x < m_cols - 2) {
                addNewSpring(current, current + 2);
            }
            // Vertikaler Widerstand gegen Knicke
            if (y < m_rows - 2) {
                addNewSpring(current, (y + 2) * m_cols + x);
            }
        }
    }
}
*/

// ----------------- THE DIAGONAL SPRING MODELL -----------------

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
  qDebug() << "CageMesh::setPoints(): points=" << pts.size();
  {
    if ( pts.size() != m_points.size() ) {
        printself();
        return;
    }
    m_points = pts;
  }
}

bool CageMesh::isBoundaryPoint( int index ) const
{
    int row = index / m_cols;
    int col = index % m_cols;
    return ( row == 0 || row == m_rows - 1 || col == 0 || col == m_cols - 1 );
}

void CageMesh::relax()
{
    // qDebug() << "CageMesh::relax(): iterations =" << m_relaxationSteps 
    //         << "stiffness =" << m_stiffness 
    //         << "fixBoundaries =" << m_fixedBoundaries;

    for ( int it = 0; it < m_relaxationSteps; ++it ) {
        for ( const auto& s : m_springs ) {
            // Referenzen auf die Punkte
            QPointF& p1 = m_points[s.a];
            QPointF& p2 = m_points[s.b];

            // Vektor zwischen den Punkten und aktuelle Distanz
            QPointF delta = p2 - p1;
            qreal dist = std::hypot(delta.x(), delta.y());

            if ( dist < 1e-6 ) continue;

            // Relative Abweichung zur Ruhelänge
            qreal diff = (dist - s.restLength) / dist;
            // Korrekturvektor (0.5, da sich beide Enden aufeinander zu bewegen)
            QPointF corr = delta * 0.5 * m_stiffness * diff;

            // Punkte bewegen, sofern sie keine fixierten Randpunkte sind
            if ( !m_fixedBoundaries ) {
                p1 += corr;
                p2 -= corr;
            } else {
                // Nur bewegen, wenn der Punkt kein Randpunkt ist
                if ( !isBoundaryPoint(s.a) ) p1 += corr;
                if ( !isBoundaryPoint(s.b) ) p2 -= corr;
            }
        }
    }
}

/*
void CageMesh::relax( int iterations )
{
  qDebug() << "CageMesh::relax(): niterations=" << iterations;
  {
    const qreal stiffness = 0.5; // Federhärte
    for ( int it = 0; it < iterations; ++it ) {
        for ( const auto& s : m_springs ) {
            QPointF& p1 = m_points[s.a];
            QPointF& p2 = m_points[s.b];
            QPointF delta = p2 - p1;
            qreal dist = std::hypot(delta.x(), delta.y());
            if ( dist < 1e-6 ) continue;
            qreal diff = (dist - s.restLength) / dist;
            QPointF corr = delta * 0.5 * stiffness * diff;
            p1 += corr;
            p2 -= corr;
        }
    }
  }
}
*/

void CageMesh::enforceConstraints( int idx )
{
  // qDebug() << "CageMesh::enforceConstraints(): index=" << idx;
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
