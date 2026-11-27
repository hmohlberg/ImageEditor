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

#include <QHash>
#include <QImage>
#include <QJsonArray>
#include <QJsonObject>
#include <QPolygonF>

namespace PolygonLayerData {

struct Cut {
    QRect bounds;
    QImage image;
};

struct Recipe {
    QRect bounds;
    qsizetype polygonIndex;
};

QHash<int, Recipe> recipes(const QJsonArray& history, const QSize& sourceSize);
QPolygonF cutPoints(const QJsonObject& polygonCommand);
bool isCompact(const QJsonObject& layer);
QJsonArray pointsToJson(const QPolygonF& polygon);
QPolygonF pointsFromJson(const QJsonArray& points);
QImage extract(const QImage& source, const QPolygonF& polygon, const QRect& bounds);
QImage binaryMask(const QImage& cut);
bool samePixels(const QImage& first, const QImage& second);
bool rebuild(const QJsonObject& project, const QImage& source, QHash<int, Cut>& cuts, int& invalidId);

}
