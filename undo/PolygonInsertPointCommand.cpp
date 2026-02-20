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

#include "PolygonInsertPointCommand.h"

PolygonInsertPointCommand::PolygonInsertPointCommand( EditablePolygon* poly,
                                                     int idx,
                                                     const QPointF& p,
                                                     QUndoCommand* parent )
    : AbstractCommand(parent)
    , m_poly(poly)
    , m_idx(idx)
    , m_point(p)
{
  setText(QString("Insert polygon point at %1").arg(idx));
}

void PolygonInsertPointCommand::redo()
{
    if ( m_silent ) return;
    m_poly->insertPoint(m_idx, m_point);
}

void PolygonInsertPointCommand::undo()
{
    m_poly->removePoint(m_idx);
}

QJsonObject PolygonInsertPointCommand::toJson() const
{
    QJsonObject obj = AbstractCommand::toJson();
    obj["type"] = "PolygonInsertPoint";
    obj["idx"] = m_idx;
    obj["x"] = m_point.x();
    obj["y"] = m_point.y();
    return obj;
}

PolygonInsertPointCommand* PolygonInsertPointCommand::fromJson( const QJsonObject& obj, EditablePolygon* poly )
{
    return new PolygonInsertPointCommand(
        poly,
        obj["idx"].toInt(),
        QPointF(obj["x"].toDouble(), obj["y"].toDouble())
    );
}
