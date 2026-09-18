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

#include "UpdatePolygonLayerCommand.h"
#include "../gui/MainWindow.h"

UpdatePolygonLayerCommand::UpdatePolygonLayerCommand(
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
    QUndoCommand*    parent)
    : AbstractCommand(parent),
      m_srcLayer(srcLayer),
      m_cutLayer(cutLayer),
      m_lassoCmd(lassoCmd),
      m_oldSrcImage(oldSrcImage),   m_newSrcImage(newSrcImage),
      m_oldCutImage(oldCutImage),   m_newCutImage(newCutImage),
      m_oldCutPos(oldCutPos),       m_newCutPos(newCutPos),
      m_oldLassoBackup(oldLassoBackup), m_newLassoBackup(newLassoBackup),
      m_oldLassoBounds(oldLassoBounds), m_newLassoBounds(newLassoBounds)
{
    m_srcLayerId = srcLayer ? srcLayer->id() : -1;
    m_cutLayerId = cutLayer ? cutLayer->id() : -1;
    setText(name);

    QByteArray svgData =
        "<svg viewBox='0 0 64 64'>"
        "<rect x='8' y='12' width='48' height='40' rx='4' "
        "fill='none' stroke='white' stroke-width='3' stroke-dasharray='6,3'/>"
        "<path d='M24 32 L40 32 M32 24 L32 40' "
        "fill='none' stroke='white' stroke-width='3' stroke-linecap='round'/>"
        "</svg>";
    setIcon(AbstractCommand::getIconFromSvg(svgData));
}

// ── helpers ───────────────────────────────────────────────────────────────────

void UpdatePolygonLayerCommand::applyState(
    const QImage& srcImg, const QImage& cutImg, const QPointF& cutPos,
    const QImage& lassoBackup, const QRect& lassoBounds)
{
    if (m_srcLayer) {
        m_srcLayer->setImage(srcImg);
        m_srcLayer->updatePixmap();
        m_srcLayer->update();
    }
    if (m_cutLayer) {
        m_cutLayer->setImage(cutImg);
        m_cutLayer->setOriginalImage(cutImg);
        m_cutLayer->setPos(cutPos);
        m_cutLayer->updatePixmap();
        m_cutLayer->update();
    }
    if (m_lassoCmd)
        m_lassoCmd->updateData(lassoBackup, lassoBounds);

    MainWindow* window = m_srcLayer
        ? dynamic_cast<MainWindow*>(m_srcLayer->parent())
        : nullptr;
    if (window) window->updateLayerList();
}

// ── undo / redo ───────────────────────────────────────────────────────────────

void UpdatePolygonLayerCommand::redo()
{
    applyState(m_newSrcImage, m_newCutImage, m_newCutPos,
               m_newLassoBackup, m_newLassoBounds);
}

void UpdatePolygonLayerCommand::undo()
{
    applyState(m_oldSrcImage, m_oldCutImage, m_oldCutPos,
               m_oldLassoBackup, m_oldLassoBounds);
}

// ── JSON (runtime command – no full deserialisation needed) ───────────────────

QJsonObject UpdatePolygonLayerCommand::toJson() const
{
    QJsonObject obj = AbstractCommand::toJson();
    obj["type"]        = "UpdatePolygonLayer";
    obj["srcLayerId"]  = m_srcLayerId;
    obj["cutLayerId"]  = m_cutLayerId;
    return obj;
}
