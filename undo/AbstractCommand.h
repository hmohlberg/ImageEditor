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

#include <QDateTime>
#include <QSvgRenderer>
#include <QUndoCommand>
#include <QJsonObject>
#include <QString>
#include <QIcon>

class LayerItem;
class ImageView;

/**
 * @brief Basisklasse für alle serialisierbaren Undo/Redo Commands
 *
 * - Erweitert QUndoCommand
 * - Erzwingt JSON-Serialisierung
 * - Ermöglicht Rekonstruktion nach Neustart
 */
class AbstractCommand : public QUndoCommand
{

public:

    explicit AbstractCommand( QUndoCommand* parent = nullptr );
    virtual ~AbstractCommand() override = default;

    // ---- Serialisierung ----
    virtual int id() const override = 0;
    virtual LayerItem* layer() const = 0;
    virtual QString type() const = 0;
    virtual QJsonObject toJson() const {
        QJsonObject obj;
        obj["text"] = text();
        return obj;
    };

    // ---- Factory ----
    static AbstractCommand* fromJson( const QJsonObject& obj, const QList<LayerItem*>& layers );
    static AbstractCommand* fromJson( const QJsonObject& obj, ImageView* view );
    
    // --- ---
    QString timeString() const { return m_timestamp.toString("HH:mm"); }
    void setIcon( const QIcon &icon ) { m_icon = icon; }
    QIcon icon() const { return m_icon; }
      
    // --- Static Helper ---
    static LayerItem* getLayerItem( const QList<LayerItem*>& layers, int layerId = 0 );
    static QIcon getIconFromSvg( const QByteArray &svgData );
    
private:

    QIcon m_icon;
    QDateTime m_timestamp;
    
};
