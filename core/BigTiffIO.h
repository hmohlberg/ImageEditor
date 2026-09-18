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
#include <functional>

// Returns true when the file at `path` is a BigTIFF (libtiff "w8" type).
// Opens and immediately closes the file; no pixel data is read.
bool bigTiffIsBigTiff(const QString& path);

// Copies a (Big)TIFF pyramid from inputPath to outputPath as a BigTIFF.
// All pyramid levels (IFDs) are copied, decoded with the source codec and
// re-encoded with Deflate + horizontal predictor.
//
// progress(pct 0..100) → return false to cancel.
// On cancel the incomplete output file is removed.
//
// Returns false and stores a human-readable message in *errorOut (when not
// nullptr) on any error.
bool bigTiffCopyPyramid(const QString& inputPath,
                        const QString& outputPath,
                        std::function<bool(int)> progress = {},
                        QString* errorOut = nullptr);
