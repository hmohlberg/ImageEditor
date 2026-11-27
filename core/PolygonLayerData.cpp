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

#include "PolygonLayerData.h"

#include <QPainter>
#include <QSet>
#include <cmath>

namespace PolygonLayerData {

bool isCompact(const QJsonObject& layer)
{
    return layer.value("rebuildFromPolygon").toBool() && !layer.contains("data");
}

QJsonArray pointsToJson(const QPolygonF& polygon)
{
    QJsonArray result;
    for (const QPointF& point : polygon)
        result.append(QJsonObject{{"x", point.x()}, {"y", point.y()}});
    return result;
}

QPolygonF pointsFromJson(const QJsonArray& points)
{
    QPolygonF polygon;
    for (const QJsonValue& value : points) {
        const QJsonObject point = value.toObject();
        const double x = point["x"].toDouble(qQNaN());
        const double y = point["y"].toDouble(qQNaN());
        if (!std::isfinite(x) || !std::isfinite(y))
            return {};
        polygon.append(QPointF(x, y));
    }
    return polygon;
}

QImage extract(const QImage& source, const QPolygonF& polygon, const QRect& bounds)
{
    if (source.format() != QImage::Format_Grayscale8 || polygon.size() < 3 ||
        bounds.isEmpty() || !source.rect().contains(bounds) ||
        !QRectF(bounds).contains(polygon.boundingRect()) ||
        polygon.boundingRect().toAlignedRect() != bounds)
        return {};

    const int right = source.width() - 1;
    const int bottom = source.height() - 1;
    const int background = (source.constScanLine(0)[0] + source.constScanLine(0)[right] +
                            source.constScanLine(bottom)[0] + source.constScanLine(bottom)[right]) / 4;

    QImage coverage(bounds.size(), QImage::Format_Alpha8);
    QImage cut(bounds.size(), QImage::Format_ARGB32_Premultiplied);
    if (coverage.isNull() || cut.isNull())
        return {};
    coverage.fill(0);
    QPainter painter(&coverage);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::NoPen);
    painter.setBrush(Qt::white);
    painter.drawPolygon(polygon.translated(-bounds.topLeft()));
    painter.end();

    for (int y = 0; y < bounds.height(); ++y) {
        const uchar* sourceRow = source.constScanLine(bounds.y() + y) + bounds.x();
        const uchar* coverageRow = coverage.constScanLine(y);
        auto* cutRow = reinterpret_cast<QRgb*>(cut.scanLine(y));
        for (int x = 0; x < bounds.width(); ++x) {
            const int gray = sourceRow[x];
            cutRow[x] = coverageRow[x] > 0 && gray != background ? qRgb(gray, gray, gray) : 0;
        }
    }
    return cut;
}

QImage binaryMask(const QImage& cut)
{
    QImage mask(cut.size(), QImage::Format_Grayscale8);
    if (mask.isNull())
        return {};
    for (int y = 0; y < cut.height(); ++y) {
        const auto* src = reinterpret_cast<const QRgb*>(cut.constScanLine(y));
        uchar* dst = mask.scanLine(y);
        for (int x = 0; x < cut.width(); ++x)
            dst[x] = qAlpha(src[x]) > 0 ? 255 : 0;
    }
    return mask;
}

bool samePixels(const QImage& first, const QImage& second)
{
    if (first.isNull() || first.size() != second.size() || first.format() != second.format())
        return false;
    const qsizetype rowBytes = (qsizetype(first.width()) * first.depth() + 7) / 8;
    for (int y = 0; y < first.height(); ++y) {
        if (QByteArrayView(first.constScanLine(y), rowBytes) !=
            QByteArrayView(second.constScanLine(y), rowBytes))
            return false;
    }
    return true;
}

QHash<int, Recipe> recipes(const QJsonArray& history, const QSize& sourceSize)
{
    QHash<int, qsizetype> polygonIndices;
    QSet<int> seenCuts;
    QHash<int, Recipe> result;
    for (qsizetype index = 0; index < history.size(); ++index) {
        const QJsonObject command = history[index].toObject();
        const QString type = command["type"].toString();
        if (type == "EditablePolygonCommand" || type == "EditablePolygon") {
            const int id = command["childLayerId"].toInt(-1);
            if (id > 0 && command["layerId"].toInt(-1) == 0)
                polygonIndices.insert(id, index);
        } else if (type == "LassoCutCommand" || type == "LassoCut") {
            const int id = command["newLayerId"].toInt(-1);
            if (seenCuts.contains(id)) {
                result.remove(id);
                continue;
            }
            seenCuts.insert(id);
            if (!polygonIndices.contains(id) || command["originalLayerId"].toInt(-1) != 0)
                continue;
            const QJsonObject rect = command["rect"].toObject();
            const int x = rect["x"].toInt(-1);
            const int y = rect["y"].toInt(-1);
            const int width = rect["width"].toInt();
            const int height = rect["height"].toInt();
            if (x < 0 || y < 0 || width <= 0 || height <= 0 ||
                width > sourceSize.width() - x || height > sourceSize.height() - y)
                continue;
            result.insert(id, {QRect(x, y, width, height), polygonIndices.value(id)});
        }
    }
    return result;
}

QPolygonF cutPoints(const QJsonObject& polygonCommand)
{
    const auto points = polygonCommand.contains("layerPoints")
        ? polygonCommand["layerPoints"] : polygonCommand["points"];
    return pointsFromJson(points.toArray());
}

bool rebuild(const QJsonObject& project, const QImage& source, QHash<int, Cut>& cuts, int& invalidId)
{
    QSet<int> compactIds;
    for (const QJsonValue& value : project["layers"].toArray()) {
        const QJsonObject layer = value.toObject();
        if (isCompact(layer))
            compactIds.insert(layer["id"].toInt(-1));
    }
    if (compactIds.isEmpty())
        return true;

    const QImage graySource = source.convertToFormat(QImage::Format_Grayscale8);
    const QJsonArray history = project["undoStack"].toArray();
    const auto cutRecipes = recipes(history, graySource.size());
    for (int id : compactIds) {
        const auto recipe = cutRecipes.constFind(id);
        if (recipe != cutRecipes.cend()) {
            const auto polygon = cutPoints(history[recipe->polygonIndex].toObject());
            const auto image = extract(graySource, polygon, recipe->bounds);
            if (!image.isNull()) {
                cuts.insert(id, {recipe->bounds, image});
                continue;
            }
        }
        invalidId = id;
        return false;
    }
    return true;
}

}
