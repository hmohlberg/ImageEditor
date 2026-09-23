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

#include "GpuInfo.h"

#include <QOffscreenSurface>
#include <QOpenGLContext>
#include <QOpenGLFunctions>
#include <QSurfaceFormat>

static QString classifyType( const QString& vendor, const QString& renderer )
{
    const QString r = renderer.toLower();
    const QString v = vendor.toLower();
    if ( r.contains("llvmpipe") || r.contains("softpipe") || r.contains("software") )
        return "software renderer";
    if ( v.contains("apple") || r.contains("apple") )
        return "integrated (Apple GPU)";
    if ( v.contains("intel") || r.contains("intel") )
        return "integrated (Intel)";
    if ( v.contains("nvidia") || r.contains("nvidia") )
        return "dedicated (NVIDIA)";
    if ( v.contains("amd") || v.contains("ati") || r.contains("radeon") || r.contains("amd") )
        return "dedicated (AMD)";
    return "unknown";
}

GpuInfo GpuInfo::query()
{
    GpuInfo info;

    QOffscreenSurface surface;
    QSurfaceFormat fmt;
    fmt.setRenderableType(QSurfaceFormat::OpenGL);
    surface.setFormat(fmt);
    surface.create();
    if ( !surface.isValid() )
        return info;

    QOpenGLContext ctx;
    ctx.setFormat(fmt);
    if ( !ctx.create() || !ctx.makeCurrent(&surface) )
        return info;

    auto* f       = ctx.functions();
    info.vendor   = reinterpret_cast<const char*>(f->glGetString(GL_VENDOR));
    info.renderer = reinterpret_cast<const char*>(f->glGetString(GL_RENDERER));
    info.version  = reinterpret_cast<const char*>(f->glGetString(GL_VERSION));
    info.type     = classifyType(info.vendor, info.renderer);
    info.available = true;
    ctx.doneCurrent();
    return info;
}

QString GpuInfo::terminalSummary() const
{
    if ( !available )
        return "not available";
    return QString("%1 [%2]").arg(renderer, type);
}

QString GpuInfo::htmlSummary() const
{
    if ( !available )
        return "not available";
    return QString("%1&nbsp;&middot;&nbsp;%2<br><small>%3</small>")
        .arg(renderer.toHtmlEscaped(), type, version.toHtmlEscaped());
}
