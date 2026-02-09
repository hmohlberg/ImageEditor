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

#include "PolygonSmoothCommand.h"

// ---------------------------- Constructor ----------------------------
PolygonSmoothCommand::PolygonSmoothCommand( EditablePolygon* poly, QUndoCommand* parent )
    : AbstractCommand(parent),
      m_poly(poly)
{
  m_before = m_poly->polygon();
  setText(QString("Smooth polygon"));
}

// ----------------------------  ----------------------------
void PolygonSmoothCommand::undo()
{
    if ( !m_poly )
      return; 
    m_poly->setPolygon(m_before);  
}

void PolygonSmoothCommand::redo()
{
    if ( !m_poly )
      return; 
    m_poly->smooth();
}

// ----------------------------  ---------------------------- 
QJsonObject PolygonSmoothCommand::toJson() const
{
    QJsonObject obj = AbstractCommand::toJson();
    obj["type"] = "PolygonSmooth";
    obj["layerId"] = 0;
    QJsonArray pts;
    for ( const QPointF& p : m_before ) {
        QJsonObject jp;
        jp["x"] = p.x();
        jp["y"] = p.y();
        pts.append(jp);
    }
    obj["points"] = pts;
    return obj;
}

PolygonSmoothCommand* PolygonSmoothCommand::fromJson( const QJsonObject& obj, EditablePolygon* poly )
{
    if ( poly == nullptr ) 
      return nullptr;
    const int layerId = obj["layerId"].toInt(-1);
    QPolygonF polygon;
    QJsonArray pts = obj["points"].toArray();
    for ( const QJsonValue& v : pts ) {
        QJsonObject jp = v.toObject();
        polygon << QPointF(jp["x"].toDouble(), jp["y"].toDouble());
    }
    poly->setPolygon(polygon);
    return new PolygonSmoothCommand(poly);
}