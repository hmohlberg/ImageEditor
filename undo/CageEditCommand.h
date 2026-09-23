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
#include <QPointF>

class LayerItem;

// Lightweight cage-editing undo/redo entry: captures the complete cage mesh
// state (current points, original points, cols, rows) before and after one
// editing operation (control-point drag or grid resize).  Lives on a
// per-layer cage edit stack, separate from the main QUndoStack so that
// undoing here does not close the editing session.
class CageEditCommand : public QUndoCommand
{
public:
    CageEditCommand( LayerItem*                  layer,
                     const QVector<QPointF>&     ptsBefore,
                     const QVector<QPointF>&     origBefore,
                     int                         colsBefore,
                     int                         rowsBefore,
                     const QPointF&              posBefore,
                     const QVector<QPointF>&     ptsAfter,
                     const QVector<QPointF>&     origAfter,
                     int                         colsAfter,
                     int                         rowsAfter,
                     const QPointF&              posAfter,
                     const QString&              text,
                     QUndoCommand*               parent = nullptr );

    void undo() override;
    void redo() override;

private:
    LayerItem*        m_layer;
    QVector<QPointF>  m_ptsBefore;
    QVector<QPointF>  m_origBefore;
    int               m_colsBefore;
    int               m_rowsBefore;
    QPointF           m_posBefore;
    QVector<QPointF>  m_ptsAfter;
    QVector<QPointF>  m_origAfter;
    int               m_colsAfter;
    int               m_rowsAfter;
    QPointF           m_posAfter;
};
