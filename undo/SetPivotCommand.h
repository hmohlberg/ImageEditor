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

#pragma once

#include <QPointF>
#include "AbstractCommand.h"
#include "../layer/LayerItem.h"

class SetPivotCommand : public AbstractCommand
{
public:
    SetPivotCommand(LayerItem* layer, const QPointF& oldPivot, const QPointF& newPivot,
                    bool oldHasPivot, QUndoCommand* parent = nullptr);

    void undo() override;
    void redo() override;

    AbstractCommand* clone() const override {
        return new SetPivotCommand(m_layer, m_oldPivot, m_newPivot, m_oldHasPivot);
    }
    int id() const override { return 1238; }
    LayerItem* layer() const override { return m_layer; }
    QString type() const override { return "SetPivot"; }

    QJsonObject toJson() const override;
    static SetPivotCommand* fromJson(const QJsonObject& obj, const QList<LayerItem*>& layers,
                                     QUndoCommand* parent = nullptr);

private:
    LayerItem* m_layer      = nullptr;
    QPointF    m_oldPivot;
    QPointF    m_newPivot;
    bool       m_oldHasPivot = false;
};
