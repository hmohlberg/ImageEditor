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

#include "CageEditCommand.h"
#include "../layer/LayerItem.h"

CageEditCommand::CageEditCommand( LayerItem*              layer,
                                  const QVector<QPointF>& ptsBefore,
                                  const QVector<QPointF>& origBefore,
                                  int                     colsBefore,
                                  int                     rowsBefore,
                                  const QPointF&          posBefore,
                                  const QVector<QPointF>& ptsAfter,
                                  const QVector<QPointF>& origAfter,
                                  int                     colsAfter,
                                  int                     rowsAfter,
                                  const QPointF&          posAfter,
                                  const QString&          text,
                                  QUndoCommand*           parent )
    : QUndoCommand(text, parent)
    , m_layer(layer)
    , m_ptsBefore(ptsBefore)
    , m_origBefore(origBefore)
    , m_colsBefore(colsBefore)
    , m_rowsBefore(rowsBefore)
    , m_posBefore(posBefore)
    , m_ptsAfter(ptsAfter)
    , m_origAfter(origAfter)
    , m_colsAfter(colsAfter)
    , m_rowsAfter(rowsAfter)
    , m_posAfter(posAfter)
{}

void CageEditCommand::undo()
{
    if ( m_layer )
        m_layer->restoreCageState(m_ptsBefore, m_origBefore, m_colsBefore, m_rowsBefore, m_posBefore);
}

void CageEditCommand::redo()
{
    if ( m_layer )
        m_layer->restoreCageState(m_ptsAfter, m_origAfter, m_colsAfter, m_rowsAfter, m_posAfter);
}
