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
*/

#pragma once

#include <QString>
#include <QJsonObject>
#include <functional>

// Apply a JSON project file to a BigTIFF, scaling all project coordinates
// by scaleFactor (project is at 20-µm, BigTIFF at 1-µm → scaleFactor = 20).
//
// Supported operations (in undoStack):
//   LassoCutCommand + MoveLayer:      cut a binary-masked region, paste at a
//                                     new position (simple translation).
//   LassoCutCommand + TransformLayer: cut a binary-masked region, apply an
//                                     affine transform (rotation/scale/shear)
//                                     and paste the result.
//
// Multiple layers with different operations per layer are supported; each
// operation is associated with its mask via the layerId field.
//
// The output is written as a BigTIFF pyramid (Deflate-compressed).
// Only usable in batch mode (requires no Qt Widgets, only Qt Core + libtiff).
//
// Returns false and populates *errorOut on failure.
bool bigTiffApplyProject(
    const QString& inputPath,
    const QString& outputPath,
    const QJsonObject& project,
    int scaleFactor,
    std::function<bool(int)> progress = {},
    QString* errorOut = nullptr
);
