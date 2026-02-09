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

#include "PerspectiveTransformCommand.h"

// -------------- Constructor -------------- 
PerspectiveTransformCommand::PerspectiveTransformCommand(
    LayerItem* layer,
    const QVector<QPointF>& before,
    const QVector<QPointF>& after)
    : AbstractCommand(parent)
    , m_layer(layer)
    , m_before(before)
    , m_after(after)
{
  setText(QString("Perspective Transform Layer %1").arg(layer->index()));
}

void PerspectiveTransformCommand::undo()
{
    m_layer->perspective().setTargetQuad(m_before);
    m_layer->applyPerspective();
}

void PerspectiveTransformCommand::redo()
{
    m_layer->perspective().setTargetQuad(m_after);
    m_layer->applyPerspective();
}

QJsonObject PerspectiveTransformCommand::toJson() const
{
    QJsonObject obj = AbstractCommand::toJson();
    obj["type"] = "PerspectiveTransform";
    auto write = [](const QVector<QPointF>& pts) {
        QJsonArray arr;
        for (auto& p : pts) {
            QJsonObject j;
            j["x"] = p.x();
            j["y"] = p.y();
            arr.append(j);
        }
        return arr;
    };
    obj["before"] = write(m_before);
    obj["after"]  = write(m_after);
    return obj;
}

PerspectiveTransformCommand* PerspectiveTransformCommand::fromJson( const QJsonObject& obj, const QList<LayerItem*>& layers )
{
  return nullptr;
}