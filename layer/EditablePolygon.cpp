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

#include "EditablePolygon.h"

#include "../undo/AbstractCommand.h"
#include "../undo/PolygonTranslateCommand.h"
#include "../undo/PolygonDeletePointCommand.h"
#include "../undo/PolygonTranslateCommand.h"
#include "../undo/PolygonInsertPointCommand.h"
#include "../undo/PolygonMovePointCommand.h"
#include "../undo/PolygonSmoothCommand.h"
#include "../undo/PolygonReduceCommand.h"

#include "../gui/MainWindow.h"
#include "../core/Config.h"

#include <QDebug>
#include <QString>
#include <cmath>

#include <iostream>

// ---------------------------- Constructor ----------------------------
EditablePolygon::EditablePolygon( const QString &caller, const QString& name, QObject* parent )
    : QObject(parent)
     , m_name(name)
{
  setIndex(name);
}

void EditablePolygon::setName( const QString& name ) 
{ 
  m_name = name; 
  setIndex(name);
}

void EditablePolygon::setIndex( const QString &name )
{
  QRegularExpression re("\\d+");
  QRegularExpressionMatch match = re.match(name);
  if ( match.hasMatch() ) {
    QString numberStr = match.captured(0);
    m_index = numberStr.toInt();
  }
}

void EditablePolygon::setVisible( bool isVisible )
{
  qCDebug(logEditor) << "EditablePolygon::setVisible(): name =" << m_name << ", visible =" << isVisible;
  {
    m_polygonVisible = isVisible;
    m_markersVisible = isVisible;
    emit visibilityChanged();
  }
}

void EditablePolygon::setSelected( bool isSelected )
{
    m_polygonSelected = isSelected;
    emit selectionChanged();
}

// ---------------------------- Tools ----------------------------
void EditablePolygon::translate( const QPointF& delta )
{
  m_polygon = m_polygon.translated(delta);
  emit changed();
}

// --- smooth edges (Cubic splines / bezier-curves) ---
void EditablePolygon::smooth()
{
  QPainterPath path;
  if ( !m_polygon.isEmpty() ) {
     path.moveTo(m_polygon.at(0));
     int np = m_polygon.size();
     for ( int i = 1; i <= m_polygon.size(); ++i ) {
        int i1 = (i-1) % np;
        int i2 = i % np;
        QPointF midPoint = (m_polygon.at(i1) + m_polygon.at(i2)) / 2;
        path.quadTo(m_polygon.at(i1), midPoint);
     }
     m_polygon = path.toFillPolygon();
  }
  emit changed();
}

// Douglas-Peucker helpers (file-local)
namespace {

static qreal dpDist( const QPointF& p, const QPointF& a, const QPointF& b )
{
    const qreal dx = b.x() - a.x();
    const qreal dy = b.y() - a.y();
    const qreal len2 = dx*dx + dy*dy;
    if ( len2 < 1e-12 )
        return QLineF(p, a).length();
    const qreal t = qBound(0.0, ((p.x()-a.x())*dx + (p.y()-a.y())*dy) / len2, 1.0);
    return QLineF(p, QPointF(a.x()+t*dx, a.y()+t*dy)).length();
}

static void dpRecurse( const QPolygonF& pts, int first, int last, qreal eps, QVector<bool>& keep )
{
    if ( last <= first + 1 ) return;
    qreal dmax = 0.0;
    int   idx  = first;
    for ( int i = first + 1; i < last; ++i ) {
        qreal d = dpDist(pts[i], pts[first], pts[last]);
        if ( d > dmax ) { dmax = d; idx = i; }
    }
    if ( dmax > eps ) {
        keep[idx] = true;
        dpRecurse(pts, first, idx,  eps, keep);
        dpRecurse(pts, idx,   last, eps, keep);
    }
}

} // anonymous namespace

// --- reduce number of points (Douglas-Peucker) ---
void EditablePolygon::reduce( qreal tolerance )
{
    qCDebug(logEditor) << "EditablePolygon::reduce(): tolerance=" << tolerance;
    if ( m_polygon.size() < 4 ) return;

    // Work on open polygon (remove duplicate closing point if present)
    QPolygonF pts = m_polygon;
    const bool wasClosed = (pts.first() == pts.last());
    if ( wasClosed )
        pts.removeLast();
    const int sz = pts.size();
    if ( sz < 3 ) return;

    // For a closed polygon we split at the two endpoints of the longest chord
    // so neither seam endpoint distorts the simplification.
    int splitAt = sz / 2;
    {
        qreal best = 0.0;
        for ( int i = 1; i < sz; ++i ) {
            qreal d = QLineF(pts[0], pts[i]).length();
            if ( d > best ) { best = d; splitAt = i; }
        }
    }

    // Build a linearised sequence: [splitAt .. end] + [0 .. splitAt]
    QPolygonF seq;
    seq.reserve(sz + 1);
    for ( int i = splitAt; i < sz; ++i ) seq << pts[i];
    for ( int i = 0;       i <= splitAt; ++i ) seq << pts[i];
    // seq[0] == seq[sz] == pts[splitAt]

    QVector<bool> keep(sz + 1, false);
    keep[0]  = true;
    keep[sz] = true;
    dpRecurse(seq, 0, sz, tolerance, keep);

    QPolygonF result;
    for ( int i = 0; i <= sz; ++i )
        if ( keep[i] ) result << seq[i];

    // Remove the duplicate junction point at the seam
    if ( result.size() > 1 && result.first() == result.last() )
        result.removeLast();

    if ( wasClosed && !result.isEmpty() )
        result << result.first();

    m_polygon = result;
    emit changed();
}

void EditablePolygon::remove()
{
     m_polygon = QPolygonF();
     emit changed();
}

QPointF EditablePolygon::point( int idx ) const
{
    if ( idx < 0 || idx >= m_polygon.size() ) return {};
    return m_polygon[idx];
}

void EditablePolygon::addPoint( const QPointF& p )
{
    m_polygon << p;
    emit changed();
}

void EditablePolygon::setPoint( int idx, const QPointF& p )
{
    if ( idx < 0 || idx >= m_polygon.size() ) return;
    m_polygon[idx] = p;
    emit changed();
}

void EditablePolygon::insertPoint( int idx, const QPointF& p )
{
    if ( idx < 0 || idx > m_polygon.size() ) return;
    if ( auto *ms = IMainSystem::instance() ) {
      IMainSystem::instance()->showMessage(QString("Inserted point (%1:%2) at position %3").arg(p.x()).arg(p.y()).arg(idx));
    }
    m_polygon.insert(idx, p);
    emit changed();
}

void EditablePolygon::removePoint( int idx )
{
    if ( idx < 0 || idx >= m_polygon.size() ) return;
    m_polygon.removeAt(idx);
    emit changed();
}

void EditablePolygon::setPolygon( const QPolygonF& poly )
{
    m_polygon = poly;
    emit changed();
}

QRectF EditablePolygon::boundingRect() const
{
    return m_polygon.boundingRect();
}

void EditablePolygon::printself()
{
  qInfo() << "EditablePolygon::printself(): polygon =" << m_polygon; 
  qInfo() << " + undoStack: size =" << m_undoStack.count();
  for ( int i = 0; i < m_undoStack.count(); ++i ) {
        auto* cmd = m_undoStack.command(i);
        qInfo() << "  + text =" << cmd->text();
  }
}

QString EditablePolygon::infoHtml(double pixelScaleUm) const
{
    const int n = m_polygon.size();
    if (n < 3) {
        return QString("<b>%1</b><br><br>Not enough vertices to compute measurements (%2 points).")
                   .arg(m_name).arg(n);
    }

    // Signed shoelace → area and centroid
    double signedArea = 0.0;
    double perimeter  = 0.0;
    double cx = 0.0, cy = 0.0;
    for (int i = 0; i < n; ++i) {
        const QPointF& a = m_polygon[i];
        const QPointF& b = m_polygon[(i + 1) % n];
        const double cross = a.x() * b.y() - b.x() * a.y();
        signedArea += cross;
        cx += (a.x() + b.x()) * cross;
        cy += (a.y() + b.y()) * cross;
        const double dx = b.x() - a.x(), dy = b.y() - a.y();
        perimeter += std::sqrt(dx*dx + dy*dy);
    }
    signedArea /= 2.0;
    const double area = std::abs(signedArea);
    if (area > 0.0) { cx /= 6.0 * signedArea; cy /= 6.0 * signedArea; }

    const QRectF bb = m_polygon.boundingRect();
    const double s  = pixelScaleUm;
    const double s2 = s * s;

    // Format helpers
    auto fmtPx  = [](double v, int d = 1){ return QString::number(v, 'f', d); };
    auto fmtLen = [](double um){
        return um >= 1000.0 ? QString("%1 mm").arg(um / 1000.0, 0, 'f', 3)
                            : QString("%1 µm").arg(um, 0, 'f', 1);
    };
    auto fmtArea = [](double um2){
        return um2 >= 1.0e6 ? QString("%1 mm²").arg(um2 / 1.0e6, 0, 'f', 4)
                            : QString("%1 µm²").arg(um2, 0, 'f', 1);
    };

    return QString(
        "<b>%1</b><br><br>"
        "<table cellspacing='4'>"
        "<tr><td><b>Vertices</b></td><td>%2</td></tr>"
        "<tr><td><b>Bounding box</b></td><td>%3 × %4 px &nbsp;(%5 × %6)</td></tr>"
        "<tr><td><b>Top-left corner</b></td><td>(%7, %8) px</td></tr>"
        "<tr><td><b>Centroid</b></td><td>(%9, %10) px</td></tr>"
        "<tr><td><b>Perimeter</b></td><td>%11 px &nbsp;(%12)</td></tr>"
        "<tr><td><b>Enclosed area</b></td><td>%13 px² &nbsp;(%14)</td></tr>"
        "<tr><td colspan='2'><hr></td></tr>"
        "<tr><td><b>Scale</b></td><td>1 pixel = %15 µm</td></tr>"
        "</table>"
    )
    .arg(m_name)
    .arg(n)
    .arg(fmtPx(bb.width())).arg(fmtPx(bb.height()))
    .arg(fmtLen(bb.width() * s)).arg(fmtLen(bb.height() * s))
    .arg(fmtPx(bb.left())).arg(fmtPx(bb.top()))
    .arg(fmtPx(cx)).arg(fmtPx(cy))
    .arg(fmtPx(perimeter)).arg(fmtLen(perimeter * s))
    .arg(fmtPx(area, 0)).arg(fmtArea(area * s2))
    .arg(pixelScaleUm, 0, 'f', 1);
}

// ---------------- Serialization ----------------

QJsonArray EditablePolygon::undoStackToJson() const
{
    QJsonArray arr;
    for ( int i = 0; i < m_undoStack.count(); ++i ) {
        auto* cmd = dynamic_cast<const AbstractCommand*>(m_undoStack.command(i));
        if ( !cmd ) continue;
        arr.append(cmd->toJson());
    }
    return arr;
}

void EditablePolygon::undoStackFromJson( const QJsonArray& arr )
{
  qCDebug(logEditor) << "EditablePolygon::undoStackFromJson(): Processing...";
  {
    m_undoStack.clear();
    for ( const QJsonValue& v : arr ) {
        QJsonObject o = v.toObject();
        QString type = o["type"].toString();
        AbstractCommand* cmd = nullptr;
        if ( type == "PolygonMovePoint" )
            cmd = PolygonMovePointCommand::fromJson(o, this);
        else if ( type == "PolygonInsertPoint" )
            cmd = PolygonInsertPointCommand::fromJson(o, this);
        else if ( type == "PolygonDeletePoint" )
            cmd = PolygonDeletePointCommand::fromJson(o, this);
        else if ( type == "TranslatePolygon" )
        	cmd = PolygonTranslateCommand::fromJson(o, this);
        else if ( type == "SmoothPolygon" )
        	cmd = PolygonSmoothCommand::fromJson(o, this);
        else if ( type == "ReducePolygon" )
        	cmd = PolygonReduceCommand::fromJson(o, this);
        else if ( type == "PolygonTranslate" )
        	cmd = PolygonTranslateCommand::fromJson(o, this);
        else if ( type == "PolygonSmooth" )
            cmd = PolygonSmoothCommand::fromJson(o, this);
        else if ( type == "PolygonReduce" )
            cmd = PolygonReduceCommand::fromJson(o, this);
        else qDebug() << "EditablePolygon::undoStackFromJson(): " << type << " not found.";
        if ( cmd ) 
            m_undoStack.push(cmd);
    }
  }
}

QJsonObject EditablePolygon::toJson() const
{
    QJsonObject obj;
    obj["name"] = m_name;
    QJsonArray arr;
    for ( const QPointF& p : m_polygon ) {
        QJsonObject po;
        po["x"] = p.x();
        po["y"] = p.y();
        arr.append(po);
    }
    obj["points"] = arr;
    obj["undo"] = undoStackToJson();
    return obj;
}

EditablePolygon* EditablePolygon::fromJson( const QJsonObject& obj )
{
    QString name = obj.value("name").toString("Unknown");
    auto* poly = new EditablePolygon("EditablePolygon::fromJson()",name);
    QPolygonF polygon;
    QJsonArray arr = obj["points"].toArray();
    for ( const QJsonValue& v : arr ) {
        QJsonObject po = v.toObject();
        polygon << QPointF(po["x"].toDouble(),po["y"].toDouble());
    }
    poly->setPolygon(polygon);
    return poly;
}
