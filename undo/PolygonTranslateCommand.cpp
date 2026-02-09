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

#include "PolygonTranslateCommand.h"

#include <iostream>

// ---------------------------- Constructor ----------------------------
PolygonTranslateCommand::PolygonTranslateCommand( EditablePolygon* poly, 
           const QPointF& start, const QPointF& end, QUndoCommand* parent )
    : AbstractCommand(parent)
      , m_poly(poly)
      , m_start(start)
      , m_end(end)
{
  setText(QString("Translate polygon"));
  m_redo = false;
}

// ----------------------------  ----------------------------
void PolygonTranslateCommand::undo()
{
    QPointF delta = m_start-m_end; 
    m_poly->translate(delta);
}

void PolygonTranslateCommand::redo()
{
    if ( m_redo == false ) {
      m_redo = true;
      return;
    }
    QPointF delta = m_end-m_start; 
    m_poly->translate(delta);
}

// ----------------------------  ---------------------------- 
QJsonObject PolygonTranslateCommand::toJson() const
{
    QJsonObject obj = AbstractCommand::toJson();
    obj["type"] = "PolygonTranslate";
    obj["x_start"] = m_start.x();
    obj["y_start"] = m_start.y();
    obj["x_end"] = m_end.x();
    obj["y_end"] = m_end.y();
    return obj;
}

PolygonTranslateCommand* PolygonTranslateCommand::fromJson( const QJsonObject& obj, EditablePolygon* poly )
{
    QPointF start = QPointF(obj["x_start"].toDouble(),obj["y_start"].toDouble());
    QPointF end = QPointF(obj["x_end"].toDouble(),obj["y_end"].toDouble());
    return new PolygonTranslateCommand(poly,start,end);
}