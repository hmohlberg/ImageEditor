inline QVector<QColor> defaultMaskColors()
{
    return {
        Qt::transparent,           // 0: Hintergrund
        QColor(255, 0, 0),         // 1: Rot
        QColor(0, 255, 0),         // 2: Grün
        QColor(0, 0, 255),         // 3: Blau
        QColor(255, 215, 0),       // 4: Gold/Gelb (etwas dunkler für Weiß-Kontrast)
        QColor(255, 0, 255),       // 5: Magenta
        QColor(0, 255, 255),       // 6: Cyan
        QColor(255, 128, 0),       // 7: Orange
        QColor(128, 0, 255),       // 8: Violett
        QColor(0, 128, 255),       // 9: Azure-Blau
        QColor(165, 42, 42),       // 10: Braun
        QColor(0, 128, 0),         // 11: Dunkelgrün
        QColor(255, 105, 180),     // 12: Hot Pink
        QColor(0, 128, 128),       // 13: Teal
        QColor(154, 205, 50),      // 14: Yellow Green
        QColor(139, 0, 0),         // 15: Dunkelrot
        QColor(75, 0, 130),        // 16: Indigo
        QColor(255, 160, 122),     // 17: Light Salmon
        QColor(32, 178, 170),      // 18: Light Sea Green
        QColor(218, 112, 214),     // 19: Orchid (Helles Lila)
        QColor(127, 255, 0)        // 20: Chartreuse
    };
}
