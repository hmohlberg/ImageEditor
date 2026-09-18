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
*/

#pragma once

#include "AbstractCommand.h"
#include "LassoCutCommand.h"
#include "../layer/LayerItem.h"

#include <QImage>
#include <QPointF>
#include <QRect>

// Replaces the pixel content of an existing polygon-cut layer with a new cut
// computed from the updated polygon shape.  The original LassoCutCommand's
// backup/bounds are also updated so that undoing past this command still works.
class UpdatePolygonLayerCommand : public AbstractCommand
{
public:
    UpdatePolygonLayerCommand(
        LayerItem*       srcLayer,
        LayerItem*       cutLayer,
        LassoCutCommand* lassoCmd,
        const QImage&    oldSrcImage,
        const QImage&    newSrcImage,
        const QImage&    oldCutImage,
        const QImage&    newCutImage,
        const QPointF&   oldCutPos,
        const QPointF&   newCutPos,
        const QImage&    oldLassoBackup,
        const QImage&    newLassoBackup,
        const QRect&     oldLassoBounds,
        const QRect&     newLassoBounds,
        const QString&   name,
        QUndoCommand*    parent = nullptr
    );

    QString type() const override { return "UpdatePolygonLayer"; }
    AbstractCommand* clone() const override { return nullptr; }
    int id() const override { return 1005; }
    LayerItem* layer() const override { return m_cutLayer; }

    void undo() override;
    void redo() override;

    QJsonObject toJson() const override;

private:
    void applyState(
        const QImage& srcImg, const QImage& cutImg, const QPointF& cutPos,
        const QImage& lassoBackup, const QRect& lassoBounds
    );

    LayerItem*       m_srcLayer  = nullptr;
    LayerItem*       m_cutLayer  = nullptr;
    LassoCutCommand* m_lassoCmd  = nullptr;

    int m_srcLayerId = -1;
    int m_cutLayerId = -1;

    QImage  m_oldSrcImage,    m_newSrcImage;
    QImage  m_oldCutImage,    m_newCutImage;
    QPointF m_oldCutPos,      m_newCutPos;
    QImage  m_oldLassoBackup, m_newLassoBackup;
    QRect   m_oldLassoBounds, m_newLassoBounds;
};
