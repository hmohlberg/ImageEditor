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

#include "PolygonDeletePointCommand.h"

// --------------------- Constructor ---------------------
PolygonDeletePointCommand::PolygonDeletePointCommand(
    EditablePolygon* poly, int idx, const QPointF& p, QUndoCommand* parent )
    : AbstractCommand(parent)
      , m_poly(poly)
      , m_idx(idx)
      , m_point(p)
{
  setText(QString("Delete polygon point %1").arg(idx));
}

// ---------------------  ---------------------
void PolygonDeletePointCommand::redo()
{
    m_poly->removePoint(m_idx);
}

void PolygonDeletePointCommand::undo()
{
    m_poly->insertPoint(m_idx, m_point);
}

// ---------------------  ---------------------
QJsonObject PolygonDeletePointCommand::toJson() const
{
    QJsonObject o = AbstractCommand::toJson();
    o["type"] = "PolygonDeletePoint";
    o["idx"] = m_idx;
    o["x"] = m_point.x();
    o["y"] = m_point.y();
    return o;
}

PolygonDeletePointCommand* PolygonDeletePointCommand::fromJson( const QJsonObject& obj, EditablePolygon* poly )
{
    return new PolygonDeletePointCommand(
        poly,
        obj["idx"].toInt(),
        QPointF(obj["x"].toDouble(), obj["y"].toDouble())
    );
}