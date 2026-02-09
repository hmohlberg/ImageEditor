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

#include "AbstractCommand.h"

// >>>
#include <QApplication>
#include <QCoreApplication>

// Include of all relevant commands
#include "CageWarpCommand.h"
#include "MoveLayerCommand.h"
#include "PaintStrokeCommand.h"
#include "InvertLayerCommand.h"
#include "LassoCutCommand.h"
#include "TransformLayerCommand.h"
#include "EditablePolygonCommand.h"

// >>>
#include <QDebug>

// -------------------- Constructor --------------------
AbstractCommand::AbstractCommand( QUndoCommand* parent )
    : QUndoCommand(parent)
{
  m_timestamp = QDateTime::currentDateTime();
}

/**
 * @brief Factory zur Rekonstruktion eines Commands aus JSON
 *
 * Wichtig:
 * - Reihenfolge im JSON = Ausführungsreihenfolge
 * - redo() wird beim push() automatisch ausgeführt
 */
AbstractCommand* AbstractCommand::fromJson( const QJsonObject& obj, ImageView* view )
{
    if ( !obj.contains("type") ) {
        qWarning() << "AbstractCommand::fromJson(): Missing type.";
        return nullptr;
    }
    const QString type = obj["type"].toString();
    // ---- Dispatch nach Command-Typ ----
    // ---- ------------------------- ----
    qWarning() << "AbstractCommand::fromJson(): Unprocessed command type: " << type;
    return nullptr;
}

AbstractCommand* AbstractCommand::fromJson( const QJsonObject& obj, const QList<LayerItem*>& layers )
{
    if ( !obj.contains("type") ) {
      qWarning() << "AbstractCommand::fromJson(): Missing type.";
      return nullptr;
    }
    const QString type = obj["type"].toString();
    // ---- Dispatch nach Command-Typ ----
    if ( type == "MoveLayer" )
        return MoveLayerCommand::fromJson(obj,layers);
    if ( type == "Paint" )
        return nullptr;
    if ( type == "LassoCut" )
        return nullptr;
    if ( type == "PaintStroke" )
        return PaintStrokeCommand::fromJson(obj,layers);
    if ( type == "InvertLayer" )
        return InvertLayerCommand::fromJson(obj,layers);
    if ( type == "CageWarp" )
        return CageWarpCommand::fromJson(obj,layers);
    if ( type == "LassoCut" )
        return LassoCutCommand::fromJson(obj,layers);
    if ( type == "TransformLayer" )
    	return TransformLayerCommand::fromJson(obj,layers);
    if ( type == "EditablePolygonCommand" )
        return EditablePolygonCommand::fromJson(obj,layers);
    qWarning() << "AbstractCommand::fromJson(): Unprocessed command type: " << type;
    return nullptr;
}

/**
 * static members
 */
 
LayerItem* AbstractCommand::getLayerItem( const QList<LayerItem*>& layers, int layerId )
{
    LayerItem* layer = nullptr;
    for ( LayerItem* l : layers ) {
        if ( l->id() == layerId ) {
            layer = l;
            break;
        }
    }
    return layer;
}

QIcon AbstractCommand::getIconFromSvg( const QByteArray &svgData ) 
{
  if ( qobject_cast<QApplication*>(qApp) ) {
    QSvgRenderer renderer(svgData);
    QPixmap pixmap(64, 64);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    renderer.render(&painter);
    return QIcon(pixmap);
  }
  return QIcon();
}
