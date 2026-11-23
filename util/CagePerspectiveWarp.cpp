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

#include "CagePerspectiveWarp.h"

#include "../layer/CageMesh.h"

#include <QtConcurrentMap>
#include <QPolygonF>
#include <array>
#include <cmath>
#include <limits>
#include <numeric>
#include <vector>

namespace CagePerspectiveWarp {
namespace {

qreal cross(const QPointF& a, const QPointF& b)
{
    return a.x() * b.y() - a.y() * b.x();
}

struct Patch {
    std::array<QPointF, 4> corners;
    int count;
    QPointF sourceOrigin, sourceU, sourceV;
    QPointF axisU, axisV, twist;
    qreal area, quadratic;
};

Patch makePatch(const std::array<QPointF, 4>& dst, int count,
                const std::array<QPointF, 4>& src)
{
    const QPointF u = dst[1] - dst[0];
    const QPointF v = dst[count - 1] - dst[0];
    const QPointF twist = count == 4 ? dst[2] - dst[0] - u - v : QPointF();
    return {dst, count, src[0], src[1] - src[0], src[count - 1] - src[0],
            u, v, twist, cross(u, v), cross(twist, v)};
}

void addCell(const std::array<QPointF, 4>& dst, const std::array<QPointF, 4>& src,
             std::vector<Patch>& patches)
{
    const auto area = [&](int a, int b, int c) {
        return cross(dst[b] - dst[a], dst[c] - dst[a]);
    };
    const qreal a = area(0, 1, 2);
    const qreal b = area(0, 2, 3);
    const qreal c = area(0, 1, 3);
    const qreal d = area(1, 2, 3);
    const auto sameWinding = [](qreal first, qreal second) {
        return (first > 0 && second > 0) || (first < 0 && second < 0);
    };
    const bool diagonal02 = sameWinding(a, b);
    const bool diagonal13 = sameWinding(c, d);

    if (diagonal02 && diagonal13 && sameWinding(a, c)) {
        patches.push_back(makePatch(dst, 4, src));
        return;
    }

    // A concave cell has no one-to-one bilinear inverse. Split it along the
    // interior diagonal. The two affine triangles still agree on every shared edge.
    const auto triangle = [&](int i, int j, int k) {
        if (area(i, j, k) != 0)
            patches.push_back(makePatch({dst[i], dst[j], dst[k], {}}, 3,
                                        {src[i], src[j], src[k], {}}));
    };
    if (diagonal13) {
        triangle(0, 1, 3);
        triangle(1, 2, 3);
    } else {
        triangle(0, 1, 2);
        triangle(0, 2, 3);
    }
}

int pixelBoundary(qreal edge, int limit)
{
    return int(std::ceil(qBound(qreal(0), edge - 0.5, qreal(limit))));
}

struct Source {
    const uchar* bits;
    qsizetype stride;
    int width, height;
};

QRgb sample(const Source& src, const QPointF& position)
{
    const qreal x = qBound(qreal(0), position.x() - 0.5, qreal(src.width - 1));
    const qreal y = qBound(qreal(0), position.y() - 0.5, qreal(src.height - 1));
    const int x0 = int(x), y0 = int(y);
    const int x1 = qMin(x0 + 1, src.width - 1);
    const int y1 = qMin(y0 + 1, src.height - 1);
    const int dx = qRound((x - x0) * 256), dy = qRound((y - y0) * 256);
    const auto* row0 = reinterpret_cast<const QRgb*>(src.bits + y0 * src.stride);
    const auto* row1 = reinterpret_cast<const QRgb*>(src.bits + y1 * src.stride);
    const QRgb a = row0[x0], b = row0[x1], c = row1[x0], d = row1[x1];
    const auto channel = [&](int shift) {
        const int top = (256 - dx) * ((a >> shift) & 255) + dx * ((b >> shift) & 255);
        const int bottom = (256 - dx) * ((c >> shift) & 255) + dx * ((d >> shift) & 255);
        return ((256 - dy) * top + dy * bottom + 32768) >> 16;
    };
    return qRgba(channel(16), channel(8), channel(0), channel(24));
}

void paintRow(const Patch& patch, const Source& src, QRgb* row, int y, int outputWidth)
{
    const qreal scanY = y + 0.5;
    std::array<qreal, 2> crossings{};
    int count = 0;
    for (int i = 0; i < patch.count; ++i) {
        const QPointF& a = patch.corners[i];
        const QPointF& b = patch.corners[(i + 1) % patch.count];
        if ((a.y() <= scanY && scanY < b.y()) || (b.y() <= scanY && scanY < a.y())) {
            if (count == 2)
                return;
            // Use the same directed edge in adjacent cells to round crossings identically.
            const bool ascending = a.y() < b.y();
            const QPointF& low = ascending ? a : b;
            const QPointF& high = ascending ? b : a;
            crossings[count++] = low.x() + (scanY - low.y()) * (high.x() - low.x()) / (high.y() - low.y());
        }
    }
    if (count != 2)
        return;

    const int left = pixelBoundary(qMin(crossings[0], crossings[1]), outputWidth);
    const int right = pixelBoundary(qMax(crossings[0], crossings[1]), outputWidth);
    QPointF p(left + 0.5 - patch.corners[0].x(), scanY - patch.corners[0].y());
    if (patch.twist.isNull()) {
        QPointF point = patch.sourceOrigin + cross(p, patch.axisV) / patch.area * patch.sourceU
                       + cross(patch.axisU, p) / patch.area * patch.sourceV;
        const QPointF step = (patch.axisV.y() * patch.sourceU - patch.axisU.y() * patch.sourceV) / patch.area;
        for (int x = left; x < right; ++x, point += step)
            row[x] = sample(src, point);
        return;
    }

    // Invert P = origin + u*U + v*V + u*v*twist. On a convex cell the
    // Jacobian keeps the sign of area, which selects the stable quadratic root.
    for (int x = left; x < right; ++x, p.rx() += 1) {
        const qreal b = patch.area + cross(p, patch.twist);
        const qreal c = cross(p, patch.axisU);
        const qreal det = qMax(qreal(0), b * b - 4 * patch.quadratic * c);
        const qreal v = -2 * c / (b + std::copysign(std::sqrt(det), patch.area));
        const QPointF uAxis = patch.axisU + v * patch.twist;
        const QPointF q = p - v * patch.axisV;
        const qreal u = QPointF::dotProduct(q, uAxis) / QPointF::dotProduct(uAxis, uAxis);
        row[x] = sample(src, patch.sourceOrigin + u * patch.sourceU + v * patch.sourceV);
    }
}

} // namespace

QSize outputSize(const QRectF& cageBounds)
{
    const qreal width = std::ceil(cageBounds.width());
    const qreal height = std::ceil(cageBounds.height());
    constexpr int limit = std::numeric_limits<int>::max();
    if (!(width > 0 && height > 0 && width < limit && height < limit))
        return {};
    return QSize(int(width), int(height));
}

QImage warpByCage(const QImage& input, const CageMesh& mesh)
{
    const int cols = mesh.cols(), rows = mesh.rows();
    const auto& points = mesh.points();
    if (cols < 2 || rows < 2 || points.size() != qsizetype(cols) * rows || input.isNull())
        return {};

    const QRectF bounds = QPolygonF(points).boundingRect();
    const QSize size = outputSize(bounds);
    if (size.isEmpty())
        return {};

    const QImage source = input.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    QImage result(size, QImage::Format_ARGB32_Premultiplied);
    if (source.isNull() || result.isNull())
        return {};

    const QPointF origin = bounds.topLeft();
    const qreal width = source.width() / qreal(cols - 1);
    const qreal height = source.height() / qreal(rows - 1);
    std::vector<Patch> patches;
    for (int r = 0; r < rows - 1; ++r) {
        for (int c = 0; c < cols - 1; ++c) {
            const int i = r * cols + c;
            addCell({points[i] - origin, points[i + 1] - origin,
                     points[i + cols + 1] - origin, points[i + cols] - origin},
                    {QPointF(c * width, r * height), QPointF((c + 1) * width, r * height),
                     QPointF((c + 1) * width, (r + 1) * height), QPointF(c * width, (r + 1) * height)},
                    patches);
        }
    }
    if (patches.empty())
        return {};

    std::vector<std::vector<const Patch*>> scanlines(size.height());
    for (const Patch& patch : patches) {
        qreal top = patch.corners[0].y();
        qreal bottom = top;
        for (int i = 1; i < patch.count; ++i) {
            top = qMin(top, patch.corners[i].y());
            bottom = qMax(bottom, patch.corners[i].y());
        }
        const int end = pixelBoundary(bottom, size.height());
        for (int y = pixelBoundary(top, size.height()); y < end; ++y)
            scanlines[y].push_back(&patch);
    }

    result.fill(Qt::transparent);
    const Source src{source.constBits(), source.bytesPerLine(), source.width(), source.height()};
    uchar* const pixels = result.bits();
    const qsizetype stride = result.bytesPerLine();
    constexpr int blockHeight = 16;
    std::vector<int> blocks((size.height() + blockHeight - 1) / blockHeight);
    std::iota(blocks.begin(), blocks.end(), 0);
    const auto paintBlock = [&](int block) {
        const int first = block * blockHeight;
        const int end = qMin(first + blockHeight, size.height());
        for (int y = first; y < end; ++y) {
            auto* row = reinterpret_cast<QRgb*>(pixels + y * stride);
            for (const Patch* patch : scanlines[y])
                paintRow(*patch, src, row, y, size.width());
        }
    };
    if (qint64(size.width()) * size.height() >= 256 * 1024)
        QtConcurrent::blockingMap(blocks, paintBlock);
    else
        for (int block : blocks)
            paintBlock(block);
    return result;
}

} // namespace CagePerspectiveWarp
