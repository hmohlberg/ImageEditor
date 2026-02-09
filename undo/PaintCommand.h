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
#include <QImage>
#include <QColor>
#include <QPoint>

class LayerItem; // Vorwärtsdeklaration

class PaintCommand : public QUndoCommand
{
public:
    // Konstruktor: malt auf das gegebene Layer an der Position pos
    PaintCommand(LayerItem* layer,
                 const QPoint& pos,
                 const QColor& color,
                 int radius,
                 qreal hardness = 1.0);

    void undo() override;
    void redo() override;

private:
    LayerItem* m_layer;   // Layer, auf dem gemalt wird
    QImage*    m_image;   // Pointer auf das Bild im Layer
    QPoint     m_pos;     // Position des Pinsels
    QColor     m_color;   // Pinsel-Farbe
    int        m_radius;  // Pinsel-Radius
    qreal      m_hardness;// Pinsel-Härte 0..1

    QImage     m_backup;  // Backup der bearbeiteten Region für Undo
};