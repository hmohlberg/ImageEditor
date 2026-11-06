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

#ifndef __APPLE__
#include <QImage>
#include <QTransform>

namespace GlslTransform {

// GPU-accelerated affine scale transform via OpenGL (QPainter on FBO).
// Restricted to pure positive scale transforms (no rotation/shear/flip).
// Renders with GL_LINEAR texture filtering — equivalent in quality to
// QPainter::SmoothPixmapTransform, but executed on the GPU.
// Returns a null QImage when the GL path is unavailable or unsupported.
QImage transformWithGL(const QImage& src, const QTransform& t);

} // namespace GlslTransform
#endif // !__APPLE__
