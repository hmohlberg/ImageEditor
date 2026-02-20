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
    LayerItem* layer, const QVector<QPointF>& before, const QVector<QPointF>& after, QUndoCommand* parent )
    : AbstractCommand(parent)
    , m_layer(layer)
    , m_before(before)
    , m_after(after)
{
  m_layerId = layer->id();
  setText(QString("Perspective Transform Layer %1").arg(m_layerId));
  QByteArray perspectiveLayerSvg = "<svg viewBox='0 0 64 64' xmlns='http://www.w3.org/2000/svg'>"
      "<path d='M12 12h40v40H12z' fill='none' stroke='#ccc' stroke-dasharray='2,2' stroke-width='1'/>"
      "<path d='M10 20 L54 10 L48 54 L16 44 Z' fill='rgba(0, 122, 255, 0.2)' stroke='#007aff' stroke-width='3' stroke-linejoin='round'/>"
      "<circle cx='10' cy='20' r='4' fill='white' stroke='#007aff' stroke-width='2'/>"
      "<circle cx='54' cy='10' r='4' fill='white' stroke='#007aff' stroke-width='2'/>"
      "<circle cx='48' cy='54' r='4' fill='white' stroke='#007aff' stroke-width='2'/>"
      "<circle cx='16' cy='44' r='4' fill='white' stroke='#007aff' stroke-width='2'/>"
      "<path d='M6 16 l4 4 m0-4 l-4 4' stroke='#007aff' stroke-width='1.5' stroke-linecap='round'/>"
      "</svg>";
  setIcon(AbstractCommand::getIconFromSvg(perspectiveLayerSvg));
}

void PerspectiveTransformCommand::undo()
{
  qDebug() << "PerspectiveTransformCommand::undo(): Processing..."; 
  {
    m_layer->perspective().setTargetQuad(m_before);
    m_layer->applyPerspective();
  }
}

void PerspectiveTransformCommand::redo()
{
  qDebug() << "PerspectiveTransformCommand::redo(): Processing..."; 
  {
    m_layer->perspective().setTargetQuad(m_after);
    m_layer->applyPerspective();
  }
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