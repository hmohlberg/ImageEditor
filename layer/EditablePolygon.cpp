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

#include <QDebug>

#include <iostream>

// ---------------------------- Constructor ----------------------------
EditablePolygon::EditablePolygon( const QString& name, QObject* parent )
    : QObject(parent)
     , m_name(name)
{
  // std::cout << "EditablePolygon::EditablePolygon(): name=" << m_name.toStdString() << std::endl;
}

void EditablePolygon::setVisible( bool isVisible )
{
    m_polygonVisible = isVisible;
    m_markersVisible = isVisible;
    emit visibilityChanged();
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

// --- reduce number of points (Douglas-Peucker-Algorithmus) ---
void EditablePolygon::reduce( qreal tolerance )
{
  // qDebug() << "EditablePolygon::reduce(): tolerance=" << tolerance;
  {
    if ( m_polygon.size() > 3 ) {
      QPainterPath path;
      path.addPolygon(m_polygon);
      QPainterPath simplifiedPath = path.simplified();
      m_polygon = simplifiedPath.toFillPolygon();
      emit changed();
    }
    // does not work
    if ( m_polygon.size() > 3 ) {
      QPolygonF result;
      result << m_polygon.first();
      for ( int i = 1; i < m_polygon.size(); ++i ) {
        qreal dist = QLineF(result.last(), m_polygon[i]).length();
        if ( dist > tolerance ) {
            result << m_polygon[i];
        }
      }
      m_polygon = result;
      emit changed();
    }
  }
}

void EditablePolygon::remove()
{
     m_polygon = QPolygonF();
     emit changed();
}

QPointF EditablePolygon::point( int idx ) const
{
    if ( idx < 0 || idx >= m_polygon.size() )
        return {};
    return m_polygon[idx];
}

void EditablePolygon::addPoint( const QPointF& p )
{
    m_polygon << p;
    emit changed();
}

void EditablePolygon::setPoint( int idx, const QPointF& p )
{
    if ( idx < 0 || idx >= m_polygon.size() )
        return;
    m_polygon[idx] = p;
    emit changed();
}

void EditablePolygon::insertPoint( int idx, const QPointF& p )
{
    if ( idx < 0 || idx > m_polygon.size() )
        return;
    m_polygon.insert(idx, p);
    emit changed();
}

void EditablePolygon::removePoint( int idx )
{
    if ( idx < 0 || idx >= m_polygon.size() )
        return;
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
  // std::cout << "EditablePolygon::undoStackFromJson(): Processing..." << std::endl;
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
    auto* poly = new EditablePolygon(name);
    QPolygonF polygon;
    QJsonArray arr = obj["points"].toArray();
    for ( const QJsonValue& v : arr ) {
        QJsonObject po = v.toObject();
        polygon << QPointF(po["x"].toDouble(),po["y"].toDouble());
    }
    poly->setPolygon(polygon);
    return poly;
}
