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

#include <QUndoCommand>
#include <QVector>
#include <QPoint>
#include "../layer/MaskLayer.h"

class MaskPaintCommand : public QUndoCommand
{
public:
    struct PixelChange
    {
        int x;
        int y;
        uchar before;
        uchar after;
    };

    MaskPaintCommand(
        MaskLayer* layer,
        QVector<PixelChange>&& changes,
        QUndoCommand* parent = nullptr
    );

    void undo() override;
    void redo() override;

private:
    MaskLayer* m_layer;
    QVector<PixelChange> m_changes;
};
