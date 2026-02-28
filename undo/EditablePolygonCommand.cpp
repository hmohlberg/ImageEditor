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

#include "EditablePolygonCommand.h"

#include "../undo/PolygonMovePointCommand.h"
#include "../core/Config.h"

#include <iostream>

// ------------------------ Constructor ------------------------
EditablePolygonCommand::EditablePolygonCommand( LayerItem *layer, QGraphicsScene* scene,
                                           const QPolygonF& polygon, const QString& name, QUndoCommand* parent )
    : AbstractCommand(parent)
    , m_scene(scene)
    , m_layer(layer)
    , m_polygon(polygon)
    , m_name(name)
{
  qCDebug(logEditor) << "EditablePolygonCommand::EditablePolygonCommand(): name =" << name;
  {
    setText(QString("Editable %1").arg(name));
    m_model = new EditablePolygon("EditablePolygonCommand::EditablePolygonCommand()",m_name);
    m_model->setPolygon(m_polygon);
    m_item = new EditablePolygonItem(m_model,m_layer);
    QByteArray polygonSvg = 
      "<svg viewBox='0 0 64 64'>"
      "<path d='M15 15 L50 20 L45 50 L10 40 Z' "
      "fill='none' stroke='white' stroke-width='3' stroke-dasharray='4,3' stroke-linejoin='round'/>"
      "<circle cx='15' cy='15' r='3' fill='white'/>"
      "<circle cx='50' cy='20' r='3' fill='white'/>"
      "<circle cx='45' cy='50' r='3' fill='white'/>"
      "<circle cx='10' cy='40' r='3' fill='white'/>"
      "</svg>";
    setIcon(AbstractCommand::getIconFromSvg(polygonSvg));
  }
}

// ------------------------ Methods ------------------------
void EditablePolygonCommand::setName( const QString& name )
{
  qCDebug(logEditor) << "EditablePolygonCommand::setName(): name =" << name;
  {
    m_name = name;
    if ( m_model != nullptr ) { m_model->setName(m_name); }
    setText(QString("Editable %1").arg(name));
  }
}

void EditablePolygonCommand::setColor( const QColor& color )
{
    if ( m_item != nullptr ) {
      m_item->setColor(color);
    }
}


bool EditablePolygonCommand::isSelected() const
{
  if ( m_item != nullptr ) {
    return m_item->polygon()->isSelected();
  }
  return false;
}

void EditablePolygonCommand::setSelected( bool isSelected )
{
    if ( m_model != nullptr ) {
      m_model->setSelected(isSelected);
    }
}

void EditablePolygonCommand::setVisible( bool isVisible )
{
  qCDebug(logEditor) << "EditablePolygonCommand::setVisible(): isVisible =" << isVisible;
  {
    if ( m_model != nullptr ) {
      m_model->setVisible(isVisible);
    }
  }
}

void EditablePolygonCommand::redo()
{
   qCDebug(logEditor) << "EditablePolygonCommand::redo(): name =" << m_name;
   {
    if ( m_silent ) return;
    if ( !m_model ) {
      m_model = new EditablePolygon("EditablePolygonCommand::redo()",m_name);
      m_model->setPolygon(m_polygon);
      m_item = new EditablePolygonItem(m_model,m_layer);
    }
    m_model->setVisible(true);
    if ( m_item != nullptr ) { 
      m_item->setName(m_name);
      if ( !m_item->scene() )
        m_scene->addItem(m_item);
    }
   }
}

void EditablePolygonCommand::undo()
{
  qCDebug(logEditor) << "EditablePolygonCommand::undo(): Processing...";
  {
    if ( m_item && m_item->scene() ) {
    //  INTRODUCED TO HANDLE CRASH EVENT: if ( !m_item->parentItem() ) {
        m_scene->removeItem(m_item);
    //  }
    }
  }
}

// ------------------------ Methods ------------------------
QJsonObject EditablePolygonCommand::toJson() const
{
    QJsonObject obj = AbstractCommand::toJson();
    obj["type"] = "EditablePolygonCommand";
    obj["name"] = m_model != nullptr ? m_model->name() : "Unknown";
    obj["layerId"] = m_layer != nullptr ? m_layer->id() : 0;
    obj["childLayerId"] = m_childLayerId;
    QJsonArray pts;
    for ( const QPointF& p : m_polygon ) {
        QJsonObject jp;
        jp["x"] = p.x();
        jp["y"] = p.y();
        pts.append(jp);
    }
    obj["points"] = pts;
    obj["undo"] = m_model->undoStackToJson();
    return obj;
}

EditablePolygonCommand* EditablePolygonCommand::fromJson( const QJsonObject& obj, const QList<LayerItem*>& layers,
                                                     QGraphicsScene* scene )
{
  qCDebug(logEditor) << "EditablePolygonCommand::fromJson(): Processing...";
  {
    QString name = obj.value("name").toString("Unknown");
    const int layerId = obj["layerId"].toInt(-1);
    LayerItem* layer = AbstractCommand::getLayerItem(layers,layerId);
    if ( !layer ) {
      qWarning() << "EditablePolygonCommand::fromJson(): Layer " << layerId << " not found.";
      return nullptr;
    }
    QPolygonF poly;
    QJsonArray pts = obj["points"].toArray();
    for ( const QJsonValue& v : pts ) {
        QJsonObject jp = v.toObject();
        poly << QPointF(jp["x"].toDouble(), jp["y"].toDouble());
    }
    const int childLayerId = obj["childLayerId"].toInt(-1);
    EditablePolygonCommand* editablePolygonCommand = new EditablePolygonCommand( layer, layer->scene(), poly, name );
    editablePolygonCommand->setChildLayerId(childLayerId);
    EditablePolygon *model = editablePolygonCommand->model();
    if ( model != nullptr ) {
      model->undoStackFromJson(obj["undo"].toArray());
    }
    return editablePolygonCommand;
  }
}
