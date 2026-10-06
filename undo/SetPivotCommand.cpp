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

#include "SetPivotCommand.h"

#include <QJsonObject>

SetPivotCommand::SetPivotCommand(LayerItem* layer, const QPointF& oldPivot,
                                 const QPointF& newPivot, bool oldHasPivot,
                                 QUndoCommand* parent)
    : AbstractCommand(parent)
    , m_layer(layer)
    , m_oldPivot(oldPivot)
    , m_newPivot(newPivot)
    , m_oldHasPivot(oldHasPivot)
{
    setText("Set Rotation Pivot");

    // Icon: crosshair pivot at top-left, arrow, crosshair pivot at bottom-right
    static const QByteArray svg =
        "<svg viewBox='0 0 64 64' xmlns='http://www.w3.org/2000/svg'>"
        // old pivot (top-left, faint)
        "<circle cx='18' cy='18' r='7' fill='none' stroke='#888' stroke-width='2'/>"
        "<line x1='11' y1='18' x2='25' y2='18' stroke='#888' stroke-width='2' stroke-linecap='round'/>"
        "<line x1='18' y1='11' x2='18' y2='25' stroke='#888' stroke-width='2' stroke-linecap='round'/>"
        // movement arrow
        "<line x1='28' y1='28' x2='38' y2='38' stroke='white' stroke-width='2.5' stroke-linecap='round'/>"
        "<polygon points='36,42 42,36 44,44' fill='white'/>"
        // new pivot (bottom-right, bright yellow)
        "<circle cx='46' cy='46' r='9' fill='none' stroke='#FFD700' stroke-width='2.5'/>"
        "<line x1='37' y1='46' x2='55' y2='46' stroke='#FFD700' stroke-width='2.5' stroke-linecap='round'/>"
        "<line x1='46' y1='37' x2='46' y2='55' stroke='#FFD700' stroke-width='2.5' stroke-linecap='round'/>"
        "<circle cx='46' cy='46' r='2.5' fill='#FFD700'/>"
        "</svg>";
    setIcon(AbstractCommand::getIconFromSvg(svg));
}

void SetPivotCommand::undo()
{
    if (!m_layer) return;
    if (m_oldHasPivot)
        m_layer->setPivot(m_oldPivot);
    else
        m_layer->resetPivot();
}

void SetPivotCommand::redo()
{
    if (!m_layer) return;
    m_layer->setPivot(m_newPivot);
}

QJsonObject SetPivotCommand::toJson() const
{
    QJsonObject obj = AbstractCommand::toJson();
    obj["type"]    = "SetPivot";
    obj["layerId"] = m_layer ? m_layer->id() : -1;

    QJsonObject oldPivotObj;
    oldPivotObj["x"]      = m_oldPivot.x();
    oldPivotObj["y"]      = m_oldPivot.y();
    oldPivotObj["hasPivot"] = m_oldHasPivot;
    obj["oldPivot"] = oldPivotObj;

    QJsonObject newPivotObj;
    newPivotObj["x"] = m_newPivot.x();
    newPivotObj["y"] = m_newPivot.y();
    obj["newPivot"] = newPivotObj;

    return obj;
}

SetPivotCommand* SetPivotCommand::fromJson(const QJsonObject& obj,
                                            const QList<LayerItem*>& layers,
                                            QUndoCommand* parent)
{
    const int layerId = obj["layerId"].toInt(-1);
    LayerItem* layer = nullptr;
    for (LayerItem* l : layers) {
        if (l->id() == layerId) { layer = l; break; }
    }
    if (!layer) {
        qWarning("SetPivotCommand::fromJson(): Layer %d not found", layerId);
        return nullptr;
    }

    QJsonObject oldObj = obj["oldPivot"].toObject();
    QPointF oldPivot(oldObj["x"].toDouble(), oldObj["y"].toDouble());
    bool oldHasPivot = oldObj["hasPivot"].toBool(false);

    QJsonObject newObj = obj["newPivot"].toObject();
    QPointF newPivot(newObj["x"].toDouble(), newObj["y"].toDouble());

    return new SetPivotCommand(layer, oldPivot, newPivot, oldHasPivot, parent);
}
