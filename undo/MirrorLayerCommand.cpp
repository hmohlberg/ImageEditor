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

#include "MirrorLayerCommand.h"

#include <QPainter>
#include <QtMath>

// -------------- Constructor --------------
MirrorLayerCommand::MirrorLayerCommand( LayerItem* layer, const int idx, int mirrorPlane, QUndoCommand* parent )
        : AbstractCommand(parent)
        , m_layer(layer)
        , m_mirrorPlane(mirrorPlane)
        , m_layerId(idx)
{
    QString planeName = mirrorPlane == 1 ? "Vertical" : "Horizontal";
    setText(QString("Mirror %1 Layer %2").arg(planeName).arg(idx));
    if ( planeName == "Vertical" ) {
     QByteArray verticalFlipSvg = 
      "<svg width='24' height='24' viewBox='0 0 24 24' fill='none' xmlns='http://www.w3.org/2000/svg'> "
       "<line x1='2' y1='12' x2='22' y2='12' stroke='white' stroke-width='2' stroke-linecap='round'/> "
       "<path d='M6 9L12 3L18 9H6Z' fill='white'/> "
       "<path d='M6 15L12 21L18 15H6Z' fill='white'/>"
       "</svg>";
     setIcon(AbstractCommand::getIconFromSvg(verticalFlipSvg));
    } else {
     QByteArray horizontalFlipSvg = 
       "<svg width='24' height='24' viewBox='0 0 24 24' fill='none' xmlns='http://www.w3.org/2000/svg'> "
       "<line x1='12' y1='2' x2='12' y2='22' stroke='white' stroke-width='2' stroke-linecap='round'/> "
       "<path d='M9 18L3 12L9 6V18Z' fill='white'/> "
       "<path d='M15 18L21 12L15 6V18Z' fill='white'/> "
       "</svg>";
     setIcon(AbstractCommand::getIconFromSvg(horizontalFlipSvg));
    }
}
    

// -------------- history related methods  -------------- 
QJsonObject MirrorLayerCommand::toJson() const
{
   QJsonObject obj = AbstractCommand::toJson();
   obj["layerId"] = m_layerId;
   obj["mirrorPlane"] = m_mirrorPlane;
   obj["type"] = type();
   return obj;
}

MirrorLayerCommand* MirrorLayerCommand::fromJson( const QJsonObject& obj, const QList<LayerItem*>& layers )
{
    const int layerId = obj["layerId"].toInt(-1);
    LayerItem* layer = nullptr;
    for ( LayerItem* l : layers ) {
        if ( l->id() == layerId ) {
            layer = l;
            break;
        }
    }
    if ( !layer ) {
      qWarning() << "MirrorLayerCommand::fromJson(): Layer " << layerId << " not found.";
      return nullptr;
    }
    return new MirrorLayerCommand(
        layer,
        obj["layerId"].toInt(),
        obj["mirrorPlane"].toInt()
    );
}

