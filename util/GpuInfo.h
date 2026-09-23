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

#include <QString>

struct GpuInfo {
    bool    available = false;
    QString vendor;
    QString renderer;
    QString version;
    QString type;       // e.g. "integrated (Apple GPU)", "dedicated (NVIDIA)", ...

    // Query via a temporary offscreen OpenGL context.
    // Requires QGuiApplication to exist; returns !available on any failure.
    static GpuInfo query();

    // One-line plain-text summary for terminal output.
    QString terminalSummary() const;

    // HTML fragment for use in Qt label / rich-text table cells.
    QString htmlSummary() const;
};
