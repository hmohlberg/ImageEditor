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

#ifndef __APPLE__

#include "GlslTransform.h"

#include <QOffscreenSurface>
#include <QOpenGLContext>
#include <QOpenGLFramebufferObject>
#include <QOpenGLFramebufferObjectFormat>
#include <QOpenGLPaintDevice>
#include <QPainter>
#include <QGuiApplication>
#include <QCoreApplication>
#include <cmath>

namespace GlslTransform {

// Lazily-created off-screen GL context.  Shared state across calls;
// only ever touched from the main thread (same as LayerItem operations).
static QOpenGLContext*   g_ctx    = nullptr;
static QOffscreenSurface* g_surf  = nullptr;

static bool ensureGL()
{
    if (g_ctx && g_ctx->isValid()) return true;

    // Requires a QGuiApplication (not available in pure batch / CLI mode)
    if (!qobject_cast<QGuiApplication*>(QCoreApplication::instance())) return false;

    if (!g_surf) {
        g_surf = new QOffscreenSurface();
        // Match the format used by the main window so drivers pick the same driver path
        g_surf->setFormat(QSurfaceFormat::defaultFormat());
        g_surf->create();
        if (!g_surf->isValid()) { delete g_surf; g_surf = nullptr; return false; }
    }

    if (!g_ctx) {
        g_ctx = new QOpenGLContext();
        g_ctx->setFormat(QSurfaceFormat::defaultFormat());
        g_ctx->create();
        if (!g_ctx->isValid()) { delete g_ctx; g_ctx = nullptr; return false; }
    }

    return true;
}

// ============================================================================
// transformWithGL — QPainter on QOpenGLFramebufferObject (GPU bilinear scale)
// ============================================================================
// Qt's OpenGL paint engine (QOpenGL2PaintEngineEx) renders QPainter operations
// via GLSL shaders.  drawImage() with SmoothPixmapTransform hint uses GL_LINEAR
// texture sampling — identical quality to the CPU SmoothPixmapTransform path
// but executed entirely on the GPU, avoiding per-pixel CPU work.
// ============================================================================
QImage transformWithGL(const QImage& src, const QTransform& t)
{
    // Only handle pure positive scale (no rotation/shear/flip).
    // Flips are handled by QImage::flipped() in LayerItem before this point.
    if (!qFuzzyIsNull(t.m12()) || !qFuzzyIsNull(t.m21())) return QImage();
    if (t.m11() <= 0.0 || t.m22() <= 0.0)                 return QImage();
    if (!ensureGL())                                        return QImage();

    // Save any currently active GL context so we don't break the render loop
    QOpenGLContext* prevCtx    = QOpenGLContext::currentContext();
    QSurface*       prevSurf   = prevCtx ? prevCtx->surface() : nullptr;

    if (!g_ctx->makeCurrent(g_surf)) return QImage();

    const QRectF destRect = t.mapRect(QRectF(src.rect()));
    const int dw = qMax(1, (int)std::ceil(destRect.width()));
    const int dh = qMax(1, (int)std::ceil(destRect.height()));

    QOpenGLFramebufferObjectFormat fmt;
    fmt.setSamples(0);                           // no MSAA — we want exact pixels
    fmt.setInternalTextureFormat(GL_RGBA8);
    QOpenGLFramebufferObject fbo(QSize(dw, dh), fmt);
    if (!fbo.isValid()) {
        g_ctx->doneCurrent();
        if (prevCtx && prevSurf) prevCtx->makeCurrent(prevSurf);
        return QImage();
    }

    // QPainter via QOpenGLPaintDevice → QOpenGL2PaintEngineEx (GPU).
    // QOpenGLPaintDevice renders into whichever FBO is currently bound;
    // using it directly is portable across Linux/Windows Qt 6 builds,
    // whereas QPainter(QOpenGLFramebufferObject*) requires the paint-device
    // capability compiled into the OpenGL module (not always present on Linux).
    fbo.bind();
    QOpenGLPaintDevice paintDevice(dw, dh);
    QPainter painter(&paintDevice);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter.translate(-destRect.topLeft());
    painter.setTransform(t, true);
    painter.drawImage(0, 0, src);
    painter.end();
    QOpenGLFramebufferObject::bindDefault();

    // fbo.toImage() reads pixels from GL — Qt marks this Format_ARGB32_Premultiplied,
    // which matches the CPU QPainter path in transformWithHighQuality.
    QImage result = fbo.toImage();

    g_ctx->doneCurrent();
    if (prevCtx && prevSurf) prevCtx->makeCurrent(prevSurf);

    return result;
}

} // namespace GlslTransform
#endif // !__APPLE__
