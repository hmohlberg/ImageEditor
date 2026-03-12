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

#include "DeleteUndoEntryCommand.h"

#include "../gui/MainWindow.h"
#include "../core/Config.h"

#include <QDebug>

DeleteUndoEntryCommand::DeleteUndoEntryCommand( QUndoStack* stack, const QString& name, int layerId, QUndoCommand* parent ) 
        : AbstractCommand(parent), m_stack(stack), m_name(name), m_layerId(layerId)
{
  qCDebug(logEditor) << "DeleteUndoEntryCommand::DeleteUndoEntryCommand(): Processing...";
  {
    setText(QString("Delete command %1").arg(name));
    QByteArray deleteCommandSvg = 
      "<svg viewBox='0 0 64 64'>"
      "<path d='M16 18 H48 M22 18 V12 H42 V18 M20 18 L24 54 H40 L44 18 M28 26 V46 M36 26 V46' "
      "fill='none' stroke='white' stroke-width='3' stroke-linecap='round' stroke-linejoin='round'/>"
      "</svg>";
    setIcon(AbstractCommand::getIconFromSvg(deleteCommandSvg));
    if ( m_stack && m_stack->count() > 0 ) {
        m_target = dynamic_cast<AbstractCommand*>(
            const_cast<QUndoCommand*>(m_stack->command(m_stack->count() - 1))
        );
    }
    printMessage();
  }
}

void DeleteUndoEntryCommand::printMessage( bool isUndo )
{
  if ( auto *ms = IMainSystem::instance() ) {
    if ( isUndo ) {
     IMainSystem::instance()->showMessage(QString("Undo delete command %1").arg(m_name)); 
    } else {
     IMainSystem::instance()->showMessage(QString("Deleted command %1").arg(m_name)); 
    }
  }
}

//-------------- Undo / redo  -------------- 
void DeleteUndoEntryCommand::redo()
{
  qCDebug(logEditor) << "DeleteUndoEntryCommand::redo(): Processing...";
  {
    if ( m_target ) {
        m_target->undo();
        m_target->markDeleted(true);
    }
  }
}

void DeleteUndoEntryCommand::undo()
{
  qCDebug(logEditor) << "DeleteUndoEntryCommand::undo(): Processing...";
  {
    if ( m_target ) {
        m_target->markDeleted(false);
        m_target->redo();
    }
  }
}

// -------------- history related methods  -------------- 
QJsonObject DeleteUndoEntryCommand::toJson() const
{
   QJsonObject obj = AbstractCommand::toJson();
   obj["layerId"] = m_layerId;
   obj["name"] = m_name;
   obj["type"] = type();
   return obj;
}

DeleteUndoEntryCommand* DeleteUndoEntryCommand::fromJson( QUndoStack* stack, const QJsonObject& obj, const QList<LayerItem*>& layers )
{
  qCDebug(logEditor) << "DeleteUndoEntryCommand::fromJson(): Processing...";
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
      qWarning() << "DeleteUndoEntryCommand::fromJson(): Layer " << layerId << " not found.";
      return nullptr;
    }
    QString name = obj.value("name").toString("Unknown");
    return new DeleteUndoEntryCommand(stack,name,layerId);
  }
}
