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

#include <QGraphicsItem>
#include <QPointer>
#include <QMap>
#include <QUndoStack>

class LayerItem;
class TransformHandleItem;
class PerspectiveWarpCommand;

enum class PerspectiveCorner {
    TL, TR, BR, BL
};

class PerspectiveOverlay : public QObject, public QGraphicsItem
{
    Q_OBJECT

 public:
 
    PerspectiveOverlay( LayerItem* layer, QUndoStack* undoStack );

    QRectF boundingRect() const override;
    void paint( QPainter*, const QStyleOptionGraphicsItem*, QWidget* ) override;

    void updateOverlay();

    void beginWarp();
    void endWarp();

    void moveCorner( PerspectiveCorner corner, const QPointF& scenePos );

 private:
 
    void createHandles();
    void updateHandlePositions();

 private:
 
    bool m_dragging = false;
 
    LayerItem* m_layer;
    QUndoStack* m_undoStack = nullptr;
    PerspectiveWarpCommand* m_warpCommand = nullptr;

    QRectF m_rect;

    QMap<PerspectiveCorner, TransformHandleItem*> m_handles;

    QVector<QPointF> m_startQuad;
    QVector<QPointF> m_currentQuad;

    QTransform m_startTransform;
    QTransform m_finalTransform;
    
};
