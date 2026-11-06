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

#ifdef __APPLE__
#include <QImage>
#include <QTransform>

namespace MetalTransform {

// GPU-accelerated affine scale transform via Metal Performance Shaders (Lanczos quality).
// Only handles pure positive scale transforms (no rotation, no shear, no flip).
// Flips are handled by QImage::flipped() in LayerItem; rotation falls back to QPainter.
// Returns a null QImage when the GPU path is unavailable or the transform is unsupported.
QImage transformWithMPS(const QImage& src, const QTransform& t);

// SIMD-accelerated (vImage / Accelerate framework) affine warp with Lanczos-5 quality.
// Handles arbitrary affine transforms, including scale, rotation and shear.
// Returns a null QImage on any error (caller should use the bicubic CPU fallback).
QImage transformWithVImage(const QImage& src, const QTransform& t);

} // namespace MetalTransform
#endif // __APPLE__
