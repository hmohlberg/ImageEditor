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
#include <QGraphicsRectItem>
#include <QPointer>
#include <QUndoStack>
#include <QMap>

class LayerItem;
class TransformLayerCommand;
class TransformHandleItem;
class CenterHandleItem;

enum class HandleType {
    Corner_TL,
    Corner_TR,
    Corner_BR,
    Corner_BL,
    Side_Left,
    Side_Right,
    Side_Top,
    Side_Bottom
};

class TransformOverlay : public QObject, public QGraphicsItem
{
    Q_OBJECT

 public:
 
    TransformOverlay( LayerItem* layer, QUndoStack* undoStack );

    QRectF boundingRect() const override;
    void paint( QPainter*, const QStyleOptionGraphicsItem*, QWidget* ) override;

    void updateOverlay();
    void beginTransform();
    void endTransform();
    void reset();

    void applyHandleDrag( HandleType type, const QPointF& delta );
    void translateLayer( const QPointF& delta );
    
    
 protected:
 
    QVariant itemChange( GraphicsItemChange change, const QVariant &value ) override;

 private:
 
    void createHandles();
    void updateHandlePositions();

 private:
 
    LayerItem* m_layer = nullptr;
    QUndoStack* m_undoStack = nullptr;
    
    TransformLayerCommand* m_transformCommand = nullptr;

    QRectF m_rect;
    QMap<HandleType, TransformHandleItem*> m_handles;
    
    CenterHandleItem* m_centerHandle = nullptr;

    QTransform m_startTransform;
    QTransform m_initialTransform;
    
};
