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

#include <QDebug>
#include <QImage>
#include <QOffscreenSurface>
#include <QOpenGLBuffer>
#include <QOpenGLContext>
#include <QOpenGLFramebufferObject>
#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>
#include <QOpenGLTexture>
#include <QOpenGLVertexArrayObject>
#include <QPointF>
#include <QRectF>
#include <QSize>
#include <QSurface>
#include <QSurfaceFormat>
#include <QVector>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <memory>
#include <vector>

// ------------------------- --- -------------------------

class CageWarpRenderer
{

  public:

    enum class CageInterpolation {
        Bilinear = 0,
        CatmullRom = 1
    };

    enum class InverseMapping {
        FixedPoint = 0,
        Newton = 1
    };

    enum class WarpBackend {
        InverseField = 0,
        RasterizedMesh = 1
    };

    struct WarpOptions
    {
        CageInterpolation cageInterpolation;
        InverseMapping inverseMapping;
        WarpBackend backend;
        int inverseIterations;
        int meshSubdivisions;

        WarpOptions(
            CageInterpolation cageInterpolation_ = CageInterpolation::Bilinear,
            InverseMapping inverseMapping_ = InverseMapping::FixedPoint,
            WarpBackend backend_ = WarpBackend::RasterizedMesh,
            int inverseIterations_ = 12,
            int meshSubdivisions_ = 4
        )
            : cageInterpolation(cageInterpolation_),
              inverseMapping(inverseMapping_),
              backend(backend_),
              inverseIterations(inverseIterations_),
              meshSubdivisions(meshSubdivisions_)
        {
        }
    };

    CageWarpRenderer() = default;

    ~CageWarpRenderer() {
        destroy();
    }

    bool initialize() {
        if (m_initialized)
            return true;

        QSurfaceFormat format;
        format.setRenderableType(QSurfaceFormat::OpenGL);
        format.setVersion(3, 3);
        format.setProfile(QSurfaceFormat::CoreProfile);
        format.setDepthBufferSize(0);
        format.setStencilBufferSize(0);

        m_context = std::make_unique<QOpenGLContext>();
        m_context->setFormat(format);

        if (!m_context->create()) {
            qWarning() << "CageWarpRenderer: could not create OpenGL context";
            m_context.reset();
            return false;
        }

        m_surface = std::make_unique<QOffscreenSurface>();
        m_surface->setFormat(m_context->format());
        m_surface->create();

        if (!m_surface->isValid()) {
            qWarning() << "CageWarpRenderer: invalid offscreen surface";
            m_surface.reset();
            m_context.reset();
            return false;
        }

        if (!m_context->makeCurrent(m_surface.get())) {
            qWarning() << "CageWarpRenderer: could not make context current";
            m_surface.reset();
            m_context.reset();
            return false;
        }

        const bool ok = initializeGlObjects();
        m_context->doneCurrent();
        if (!ok) {
            destroy();
            return false;
        }
        m_initialized = true;
        return true;
    }

    bool setSourceImage( const QImage& image ) {
        if (image.isNull()) {
            qWarning() << "CageWarpRenderer::setSourceImage: image is null";
            return false;
        }

        if (!initialize())
            return false;

        ScopedCurrent current(*this);
        if (!current)
            return false;

        m_sourceSize = image.size();

        m_imageTexture.reset();
        m_imageTexture = std::make_unique<QOpenGLTexture>(QOpenGLTexture::Target2D);
        #if QT_VERSION >= QT_VERSION_CHECK(6, 9, 0)
           m_imageTexture->setData(image.convertToFormat(QImage::Format_RGBA8888).flipped(Qt::Vertical));
        #else
           m_imageTexture->setData(image.convertToFormat(QImage::Format_RGBA8888).mirrored(false, true));
        #endif
        m_imageTexture->setMinMagFilters(QOpenGLTexture::Linear, QOpenGLTexture::Linear);
        m_imageTexture->setWrapMode(QOpenGLTexture::ClampToEdge);

        return true;
    }

    bool setGridSize( int columns, int rows ) {
        if (columns < 2 || rows < 2) {
            qWarning() << "CageWarpRenderer::setGridSize: invalid grid" << columns << rows;
            return false;
        }

        if (!initialize())
            return false;

        const bool sameSize =
            m_cageTexture &&
            m_gridColumns == columns &&
            m_gridRows == rows;

        m_gridColumns = columns;
        m_gridRows = rows;
        m_cageData.assign(columns * rows * 4, 0.0f);

        if (sameSize)
            return true;

        ScopedCurrent current(*this);
        if (!current)
            return false;

        createCageTexture();
        return true;
    }

    void setFixedOutputRect( const QRectF& rect ) {
        m_fixedOutputRect = rect;
        m_hasFixedOutputRect = true;
    }

    void clearFixedOutputRect() {
        m_hasFixedOutputRect = false;
    }

    QImage warp( const QVector<QPointF>& warpedGridPoints ) {
        return warp(warpedGridPoints, nullptr, WarpOptions());
    }

    QImage warp( const QVector<QPointF>& warpedGridPoints, const WarpOptions& options ) {
        return warp(warpedGridPoints, nullptr, options);
    }

    QImage warp(
        const QVector<QPointF>& warpedGridPoints,
        QPointF* outputOrigin,
        const WarpOptions& options = WarpOptions()
    )
    {
        if (!initialize() || !m_imageTexture) {
            qWarning() << "CageWarpRenderer::warp: renderer is not ready";
            return QImage();
        }

        if (m_gridColumns < 2 || m_gridRows < 2 || !m_cageTexture) {
            qWarning() << "CageWarpRenderer::warp: grid is not ready";
            return QImage();
        }

        if (warpedGridPoints.size() != m_gridColumns * m_gridRows) {
            qWarning() << "CageWarpRenderer::warp: warpedGridPoints has wrong size"
                       << warpedGridPoints.size()
                       << "expected"
                       << m_gridColumns * m_gridRows;
            return QImage();
        }

        ScopedCurrent current(*this);
        if (!current)
            return QImage();

        if (options.backend == WarpBackend::RasterizedMesh)
            return warpRasterizedMeshCurrent(warpedGridPoints, outputOrigin, options);

        return warpInverseFieldCurrent(warpedGridPoints, outputOrigin, options);
    }

  private:
  
    class ScopedCurrent {
      
      public:
      
        explicit ScopedCurrent(CageWarpRenderer& renderer)
            : m_renderer(renderer),
              m_previousContext(QOpenGLContext::currentContext()),
              m_previousSurface(m_previousContext ? m_previousContext->surface() : nullptr)
        {
            m_ok = m_renderer.m_context &&
                   m_renderer.m_surface &&
                   m_renderer.m_context->makeCurrent(m_renderer.m_surface.get());

            if (!m_ok)
                qWarning() << "CageWarpRenderer: could not make context current";
        }

        ~ScopedCurrent() {
            if ( !m_ok )
                return;
            if (m_previousContext && m_previousSurface && m_previousContext != m_renderer.m_context.get())
                m_previousContext->makeCurrent(m_previousSurface);
            else
                m_renderer.m_context->doneCurrent();
        }

        explicit operator bool() const {
            return m_ok;
        }

      private:
        CageWarpRenderer& m_renderer;
        QOpenGLContext* m_previousContext = nullptr;
        QSurface* m_previousSurface = nullptr;
        bool m_ok = false;
    };

    struct MeshVertex
    {
        float targetX;
        float targetY;
        float sourceU;
        float sourceV;
    };

    struct MeshBuild
    {
        std::vector<MeshVertex> vertices;
        std::vector<GLuint> indices;
        QRectF bounds;
    };

    static int clampInt(int value, int lo, int hi)
    {
        return std::max(lo, std::min(value, hi));
    }

    static double clampDouble(double value, double lo, double hi)
    {
        return std::max(lo, std::min(value, hi));
    }

    static double mixDouble(double a, double b, double t)
    {
        return a + (b - a) * t;
    }

    static QPointF mixPoint(const QPointF& a, const QPointF& b, double t)
    {
        return QPointF(
            mixDouble(a.x(), b.x(), t),
            mixDouble(a.y(), b.y(), t)
        );
    }

    static double catmullRomScalar(double p0, double p1, double p2, double p3, double t)
    {
        const double t2 = t * t;
        const double t3 = t2 * t;

        return 0.5 * (
            2.0 * p1 +
            (-p0 + p2) * t +
            (2.0 * p0 - 5.0 * p1 + 4.0 * p2 - p3) * t2 +
            (-p0 + 3.0 * p1 - 3.0 * p2 + p3) * t3
        );
    }

    static QPointF catmullRomPoint(
        const QPointF& p0,
        const QPointF& p1,
        const QPointF& p2,
        const QPointF& p3,
        double t
    )
    {
        return QPointF(
            catmullRomScalar(p0.x(), p1.x(), p2.x(), p3.x(), t),
            catmullRomScalar(p0.y(), p1.y(), p2.y(), p3.y(), t)
        );
    }

    QPointF regularGridPoint( double gridX, double gridY ) const
    {
        return QPointF(
            double(m_sourceSize.width()) * gridX / double(m_gridColumns - 1),
            double(m_sourceSize.height()) * gridY / double(m_gridRows - 1)
        );
    }

    QPointF sampleGridDisplacement(
        const QVector<QPointF>& warpedGridPoints,
        int x,
        int y
    ) const
    {
        x = clampInt(x, 0, m_gridColumns - 1);
        y = clampInt(y, 0, m_gridRows - 1);

        const QPointF source = regularGridPoint(double(x), double(y));
        const QPointF target = warpedGridPoints[y * m_gridColumns + x];

        return target - source;
    }

    QPointF interpolatedDisplacement(
        const QVector<QPointF>& warpedGridPoints,
        double gridX,
        double gridY,
        CageInterpolation interpolation
    ) const
    {
        gridX = clampDouble(gridX, 0.0, double(m_gridColumns - 1));
        gridY = clampDouble(gridY, 0.0, double(m_gridRows - 1));

        if (interpolation == CageInterpolation::CatmullRom) {
            const int baseX = int(std::floor(gridX));
            const int baseY = int(std::floor(gridY));
            const double fx = gridX - double(baseX);
            const double fy = gridY - double(baseY);

            const QPointF row0 = catmullRomPoint(
                sampleGridDisplacement(warpedGridPoints, baseX - 1, baseY - 1),
                sampleGridDisplacement(warpedGridPoints, baseX,     baseY - 1),
                sampleGridDisplacement(warpedGridPoints, baseX + 1, baseY - 1),
                sampleGridDisplacement(warpedGridPoints, baseX + 2, baseY - 1),
                fx
            );

            const QPointF row1 = catmullRomPoint(
                sampleGridDisplacement(warpedGridPoints, baseX - 1, baseY),
                sampleGridDisplacement(warpedGridPoints, baseX,     baseY),
                sampleGridDisplacement(warpedGridPoints, baseX + 1, baseY),
                sampleGridDisplacement(warpedGridPoints, baseX + 2, baseY),
                fx
            );

            const QPointF row2 = catmullRomPoint(
                sampleGridDisplacement(warpedGridPoints, baseX - 1, baseY + 1),
                sampleGridDisplacement(warpedGridPoints, baseX,     baseY + 1),
                sampleGridDisplacement(warpedGridPoints, baseX + 1, baseY + 1),
                sampleGridDisplacement(warpedGridPoints, baseX + 2, baseY + 1),
                fx
            );

            const QPointF row3 = catmullRomPoint(
                sampleGridDisplacement(warpedGridPoints, baseX - 1, baseY + 2),
                sampleGridDisplacement(warpedGridPoints, baseX,     baseY + 2),
                sampleGridDisplacement(warpedGridPoints, baseX + 1, baseY + 2),
                sampleGridDisplacement(warpedGridPoints, baseX + 2, baseY + 2),
                fx
            );

            return catmullRomPoint(row0, row1, row2, row3, fy);
        }

        const int x0 = std::min(int(std::floor(gridX)), m_gridColumns - 2);
        const int y0 = std::min(int(std::floor(gridY)), m_gridRows - 2);
        const int x1 = x0 + 1;
        const int y1 = y0 + 1;

        const double fx = gridX - double(x0);
        const double fy = gridY - double(y0);

        const QPointF d00 = sampleGridDisplacement(warpedGridPoints, x0, y0);
        const QPointF d10 = sampleGridDisplacement(warpedGridPoints, x1, y0);
        const QPointF d01 = sampleGridDisplacement(warpedGridPoints, x0, y1);
        const QPointF d11 = sampleGridDisplacement(warpedGridPoints, x1, y1);

        return mixPoint(
            mixPoint(d00, d10, fx),
            mixPoint(d01, d11, fx),
            fy
        );
    }

    MeshBuild buildRasterizedMesh(
        const QVector<QPointF>& warpedGridPoints,
        const WarpOptions& options
    ) const
    {
        MeshBuild mesh;

        const int subdivisions = std::max(1, std::min(options.meshSubdivisions, 64));
        const int meshColumns = (m_gridColumns - 1) * subdivisions + 1;
        const int meshRows = (m_gridRows - 1) * subdivisions + 1;

        mesh.vertices.reserve(size_t(meshColumns * meshRows));
        mesh.indices.reserve(size_t((meshColumns - 1) * (meshRows - 1) * 6));

        double minX = std::numeric_limits<double>::max();
        double minY = std::numeric_limits<double>::max();
        double maxX = -std::numeric_limits<double>::max();
        double maxY = -std::numeric_limits<double>::max();

        for (int y = 0; y < meshRows; ++y) {
            for (int x = 0; x < meshColumns; ++x) {
                const double gridX = double(x) / double(subdivisions);
                const double gridY = double(y) / double(subdivisions);

                const QPointF source = regularGridPoint(gridX, gridY);
                const QPointF displacement = interpolatedDisplacement(
                    warpedGridPoints,
                    gridX,
                    gridY,
                    options.cageInterpolation
                );
                const QPointF target = source + displacement;

                minX = std::min(minX, target.x());
                minY = std::min(minY, target.y());
                maxX = std::max(maxX, target.x());
                maxY = std::max(maxY, target.y());

                mesh.vertices.push_back({
                    float(target.x()),
                    float(target.y()),
                    float(source.x() / double(m_sourceSize.width())),
                    float(source.y() / double(m_sourceSize.height()))
                });
            }
        }

        for (int y = 0; y < meshRows - 1; ++y) {
            for (int x = 0; x < meshColumns - 1; ++x) {
                const GLuint i00 = GLuint(y * meshColumns + x);
                const GLuint i10 = GLuint(y * meshColumns + x + 1);
                const GLuint i01 = GLuint((y + 1) * meshColumns + x);
                const GLuint i11 = GLuint((y + 1) * meshColumns + x + 1);

                mesh.indices.push_back(i00);
                mesh.indices.push_back(i10);
                mesh.indices.push_back(i11);

                mesh.indices.push_back(i00);
                mesh.indices.push_back(i11);
                mesh.indices.push_back(i01);
            }
        }

        mesh.bounds = QRectF(QPointF(minX, minY), QPointF(maxX, maxY)).normalized();
        return mesh;
    }

    QRectF controlPointBoundingRect( const QVector<QPointF>& points ) const
    {
        if (points.isEmpty())
            return QRectF();

        double minX = points[0].x();
        double minY = points[0].y();
        double maxX = points[0].x();
        double maxY = points[0].y();

        for (const QPointF& p : points) {
            minX = std::min(minX, p.x());
            minY = std::min(minY, p.y());
            maxX = std::max(maxX, p.x());
            maxY = std::max(maxY, p.y());
        }

        return QRectF(QPointF(minX, minY), QPointF(maxX, maxY)).normalized();
    }

    void outputRectToInts(
        const QRectF& rect,
        int& outX,
        int& outY,
        int& outW,
        int& outH
    ) const
    {
        const QRectF r = rect.normalized();

        const int left = int(std::floor(r.left()));
        const int top = int(std::floor(r.top()));
        const int right = int(std::ceil(r.right()));
        const int bottom = int(std::ceil(r.bottom()));

        outX = left;
        outY = top;
        outW = std::max(1, right - left);
        outH = std::max(1, bottom - top);
    }

    bool initializeGlObjects()
    {
        QOpenGLContext* context = QOpenGLContext::currentContext();
        if (!context) {
            qWarning() << "CageWarpRenderer: no current OpenGL context";
            return false;
        }

        m_gl = context->functions();
        if (!m_gl) {
            qWarning() << "CageWarpRenderer: OpenGL functions unavailable";
            return false;
        }

        m_gl->initializeOpenGLFunctions();

        if (!createInverseProgram())
            return false;

        if (!createMeshProgram())
            return false;

        m_fullscreenVao.create();
        m_meshVao.create();
        m_meshVbo.create();
        m_meshIbo.create();

        return true;
    }

    bool createInverseProgram()
    {
        static const char* vertexShaderSource = R"(
            #version 330 core

            void main()
            {
                vec2 p[3] = vec2[](
                    vec2(-1.0, -1.0),
                    vec2( 3.0, -1.0),
                    vec2(-1.0,  3.0)
                );

                gl_Position = vec4(p[gl_VertexID], 0.0, 1.0);
            }
        )";

        static const char* fragmentShaderSource = R"(
            #version 330 core

            uniform sampler2D uImage;
            uniform sampler2D uCage;

            uniform vec2 uImageSize;
            uniform vec2 uOutputSize;
            uniform vec2 uOutputOrigin;
            uniform vec2 uGridSize;

            uniform int uCageInterpolation;
            uniform int uInverseMapping;
            uniform int uInverseIterations;

            out vec4 fragColor;

            vec2 cageFetch(ivec2 p)
            {
                ivec2 maxP = ivec2(uGridSize) - ivec2(1);
                p = clamp(p, ivec2(0), maxP);
                return texelFetch(uCage, p, 0).rg;
            }

            vec2 catmullRom(vec2 p0, vec2 p1, vec2 p2, vec2 p3, float t)
            {
                float t2 = t * t;
                float t3 = t2 * t;

                return 0.5 * (
                    2.0 * p1 +
                    (-p0 + p2) * t +
                    (2.0 * p0 - 5.0 * p1 + 4.0 * p2 - p3) * t2 +
                    (-p0 + 3.0 * p1 - 3.0 * p2 + p3) * t3
                );
            }

            vec2 cageDisplacementBilinear(vec2 sourceUv)
            {
                vec2 g = clamp(sourceUv, 0.0, 1.0) * (uGridSize - vec2(1.0));

                ivec2 p0 = ivec2(floor(g));
                ivec2 p1 = min(p0 + ivec2(1), ivec2(uGridSize) - ivec2(1));

                vec2 f = fract(g);

                vec2 d00 = cageFetch(p0);
                vec2 d10 = cageFetch(ivec2(p1.x, p0.y));
                vec2 d01 = cageFetch(ivec2(p0.x, p1.y));
                vec2 d11 = cageFetch(p1);

                return mix(mix(d00, d10, f.x), mix(d01, d11, f.x), f.y);
            }

            vec2 cageDisplacementCatmullRom(vec2 sourceUv)
            {
                vec2 g = clamp(sourceUv, 0.0, 1.0) * (uGridSize - vec2(1.0));

                ivec2 base = ivec2(floor(g));
                vec2 f = fract(g);

                vec2 row0 = catmullRom(
                    cageFetch(base + ivec2(-1, -1)),
                    cageFetch(base + ivec2( 0, -1)),
                    cageFetch(base + ivec2( 1, -1)),
                    cageFetch(base + ivec2( 2, -1)),
                    f.x
                );

                vec2 row1 = catmullRom(
                    cageFetch(base + ivec2(-1, 0)),
                    cageFetch(base + ivec2( 0, 0)),
                    cageFetch(base + ivec2( 1, 0)),
                    cageFetch(base + ivec2( 2, 0)),
                    f.x
                );

                vec2 row2 = catmullRom(
                    cageFetch(base + ivec2(-1, 1)),
                    cageFetch(base + ivec2( 0, 1)),
                    cageFetch(base + ivec2( 1, 1)),
                    cageFetch(base + ivec2( 2, 1)),
                    f.x
                );

                vec2 row3 = catmullRom(
                    cageFetch(base + ivec2(-1, 2)),
                    cageFetch(base + ivec2( 0, 2)),
                    cageFetch(base + ivec2( 1, 2)),
                    cageFetch(base + ivec2( 2, 2)),
                    f.x
                );

                return catmullRom(row0, row1, row2, row3, f.y);
            }

            vec2 cageDisplacement(vec2 sourceUv)
            {
                if (uCageInterpolation == 1)
                    return cageDisplacementCatmullRom(sourceUv);

                return cageDisplacementBilinear(sourceUv);
            }

            vec2 forwardError(vec2 sourceUv, vec2 targetPixel)
            {
                return sourceUv * uImageSize + cageDisplacement(sourceUv) - targetPixel;
            }

            vec2 solveFixedPoint(vec2 targetPixel)
            {
                vec2 sourceUv = targetPixel / uImageSize;

                for (int i = 0; i < uInverseIterations; ++i) {
                    vec2 d = cageDisplacement(sourceUv);
                    sourceUv = (targetPixel - d) / uImageSize;
                }

                return sourceUv;
            }

            vec2 solveNewton(vec2 targetPixel)
            {
                vec2 sourceUv = targetPixel / uImageSize;

                for (int i = 0; i < 3; ++i) {
                    vec2 d = cageDisplacement(sourceUv);
                    sourceUv = (targetPixel - d) / uImageSize;
                }

                vec2 eps = max(vec2(1.0) / uImageSize, vec2(0.0001));

                for (int i = 0; i < uInverseIterations; ++i) {
                    vec2 f = forwardError(sourceUv, targetPixel);

                    if (dot(f, f) < 0.0001)
                        break;

                    vec2 fx1 = forwardError(sourceUv + vec2(eps.x, 0.0), targetPixel);
                    vec2 fx0 = forwardError(sourceUv - vec2(eps.x, 0.0), targetPixel);
                    vec2 fy1 = forwardError(sourceUv + vec2(0.0, eps.y), targetPixel);
                    vec2 fy0 = forwardError(sourceUv - vec2(0.0, eps.y), targetPixel);

                    vec2 dFdx = (fx1 - fx0) / (2.0 * eps.x);
                    vec2 dFdy = (fy1 - fy0) / (2.0 * eps.y);

                    float det = dFdx.x * dFdy.y - dFdy.x * dFdx.y;

                    if (abs(det) < 1e-6)
                        break;

                    vec2 delta = vec2(
                        ( dFdy.y * f.x - dFdy.x * f.y) / det,
                        (-dFdx.y * f.x + dFdx.x * f.y) / det
                    );

                    delta = clamp(delta, vec2(-0.25), vec2(0.25));
                    sourceUv -= delta;
                    sourceUv = clamp(sourceUv, vec2(-0.25), vec2(1.25));
                }

                return sourceUv;
            }

            void main()
            {
                vec2 outputPixel = vec2(gl_FragCoord.x, uOutputSize.y - gl_FragCoord.y);
                vec2 targetPixel = uOutputOrigin + outputPixel;

                vec2 sourceUv;

                if (uInverseMapping == 1)
                    sourceUv = solveNewton(targetPixel);
                else
                    sourceUv = solveFixedPoint(targetPixel);

                if (sourceUv.x < 0.0 || sourceUv.y < 0.0 ||
                    sourceUv.x > 1.0 || sourceUv.y > 1.0) {
                    fragColor = vec4(0.0);
                    return;
                }

                fragColor = texture(uImage, vec2(sourceUv.x, 1.0 - sourceUv.y));
            }
        )";

        m_inverseProgram = std::make_unique<QOpenGLShaderProgram>();

        if (!m_inverseProgram->addShaderFromSourceCode(QOpenGLShader::Vertex, vertexShaderSource)) {
            qWarning() << "CageWarpRenderer inverse vertex shader:" << m_inverseProgram->log();
            return false;
        }

        if (!m_inverseProgram->addShaderFromSourceCode(QOpenGLShader::Fragment, fragmentShaderSource)) {
            qWarning() << "CageWarpRenderer inverse fragment shader:" << m_inverseProgram->log();
            return false;
        }

        if (!m_inverseProgram->link()) {
            qWarning() << "CageWarpRenderer inverse shader link:" << m_inverseProgram->log();
            return false;
        }

        return true;
    }

    bool createMeshProgram()
    {
        static const char* vertexShaderSource = R"(
            #version 330 core

            layout(location = 0) in vec2 aTargetPixel;
            layout(location = 1) in vec2 aSourceUv;

            uniform vec2 uOutputSize;
            uniform vec2 uOutputOrigin;

            out vec2 vSourceUv;

            void main()
            {
                vec2 p = aTargetPixel - uOutputOrigin;

                vec2 ndc = vec2(
                    p.x / uOutputSize.x * 2.0 - 1.0,
                    1.0 - p.y / uOutputSize.y * 2.0
                );

                gl_Position = vec4(ndc, 0.0, 1.0);
                vSourceUv = aSourceUv;
            }
        )";

        static const char* fragmentShaderSource = R"(
            #version 330 core

            uniform sampler2D uImage;

            in vec2 vSourceUv;
            out vec4 fragColor;

            void main()
            {
                fragColor = texture(uImage, vec2(vSourceUv.x, 1.0 - vSourceUv.y));
            }
        )";

        m_meshProgram = std::make_unique<QOpenGLShaderProgram>();

        if (!m_meshProgram->addShaderFromSourceCode(QOpenGLShader::Vertex, vertexShaderSource)) {
            qWarning() << "CageWarpRenderer mesh vertex shader:" << m_meshProgram->log();
            return false;
        }

        if (!m_meshProgram->addShaderFromSourceCode(QOpenGLShader::Fragment, fragmentShaderSource)) {
            qWarning() << "CageWarpRenderer mesh fragment shader:" << m_meshProgram->log();
            return false;
        }

        if (!m_meshProgram->link()) {
            qWarning() << "CageWarpRenderer mesh shader link:" << m_meshProgram->log();
            return false;
        }

        return true;
    }

    void createCageTexture()
    {
        m_cageTexture.reset();
        m_cageTexture = std::make_unique<QOpenGLTexture>(QOpenGLTexture::Target2D);
        m_cageTexture->setSize(m_gridColumns, m_gridRows);
        m_cageTexture->setFormat(QOpenGLTexture::RGBA32F);
        m_cageTexture->allocateStorage(QOpenGLTexture::RGBA, QOpenGLTexture::Float32);
        m_cageTexture->setMinMagFilters(QOpenGLTexture::Nearest, QOpenGLTexture::Nearest);
        m_cageTexture->setWrapMode(QOpenGLTexture::ClampToEdge);
    }

    void updateCageTexture( const QVector<QPointF>& warpedGridPoints )
    {
        for (int y = 0; y < m_gridRows; ++y) {
            for (int x = 0; x < m_gridColumns; ++x) {
                const QPointF source = regularGridPoint(double(x), double(y));
                const QPointF target = warpedGridPoints[y * m_gridColumns + x];
                const QPointF displacement = target - source;

                const int k = 4 * (y * m_gridColumns + x);
                m_cageData[k + 0] = float(displacement.x());
                m_cageData[k + 1] = float(displacement.y());
                m_cageData[k + 2] = 0.0f;
                m_cageData[k + 3] = 0.0f;
            }
        }

        m_cageTexture->setData(QOpenGLTexture::RGBA, QOpenGLTexture::Float32, m_cageData.data());
    }

    void ensureFbo( int width, int height ) {
        if ( m_fbo && m_fbo->width() == width && m_fbo->height() == height )
            return;
        QOpenGLFramebufferObjectFormat format;
        format.setInternalTextureFormat(GL_RGBA8);
        m_fbo = std::make_unique<QOpenGLFramebufferObject>(width, height, format);
    }

    QImage warpInverseFieldCurrent( const QVector<QPointF>& warpedGridPoints, QPointF* outputOrigin, const WarpOptions& options ) {
        updateCageTexture(warpedGridPoints);

        const QRectF outputRect = m_hasFixedOutputRect
            ? m_fixedOutputRect
            : controlPointBoundingRect(warpedGridPoints);

        int outX = 0;
        int outY = 0;
        int outW = 1;
        int outH = 1;
        outputRectToInts(outputRect, outX, outY, outW, outH);

        if (outputOrigin)
            *outputOrigin = QPointF(outX, outY);

        ensureFbo(outW, outH);

        if (!m_fbo || !m_fbo->isValid()) {
            qWarning() << "CageWarpRenderer::warpInverseFieldCurrent: invalid framebuffer";
            return QImage();
        }

        GLint oldFramebuffer = 0;
        GLint oldViewport[4] = {};
        const GLboolean depthWasEnabled = m_gl->glIsEnabled(GL_DEPTH_TEST);
        const GLboolean cullWasEnabled = m_gl->glIsEnabled(GL_CULL_FACE);
        const GLboolean blendWasEnabled = m_gl->glIsEnabled(GL_BLEND);

        m_gl->glGetIntegerv(GL_FRAMEBUFFER_BINDING, &oldFramebuffer);
        m_gl->glGetIntegerv(GL_VIEWPORT, oldViewport);

        m_fbo->bind();

        m_gl->glViewport(0, 0, outW, outH);
        m_gl->glDisable(GL_DEPTH_TEST);
        m_gl->glDisable(GL_CULL_FACE);
        m_gl->glDisable(GL_BLEND);
        m_gl->glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
        m_gl->glClear(GL_COLOR_BUFFER_BIT);

        m_fullscreenVao.bind();
        m_inverseProgram->bind();

        m_inverseProgram->setUniformValue("uImage", 0);
        m_inverseProgram->setUniformValue("uCage", 1);
        m_inverseProgram->setUniformValue("uImageSize", float(m_sourceSize.width()), float(m_sourceSize.height()));
        m_inverseProgram->setUniformValue("uOutputSize", float(outW), float(outH));
        m_inverseProgram->setUniformValue("uOutputOrigin", float(outX), float(outY));
        m_inverseProgram->setUniformValue("uGridSize", float(m_gridColumns), float(m_gridRows));
        m_inverseProgram->setUniformValue("uCageInterpolation", int(options.cageInterpolation));
        m_inverseProgram->setUniformValue("uInverseMapping", int(options.inverseMapping));
        m_inverseProgram->setUniformValue("uInverseIterations", std::max(1, std::min(options.inverseIterations, 32)));

        m_imageTexture->bind(0);
        m_cageTexture->bind(1);

        m_gl->glDrawArrays(GL_TRIANGLES, 0, 3);

        m_cageTexture->release();
        m_imageTexture->release();
        m_inverseProgram->release();
        m_fullscreenVao.release();

        QImage result = m_fbo->toImage();

        m_gl->glBindFramebuffer(GL_FRAMEBUFFER, GLuint(oldFramebuffer));
        m_gl->glViewport(oldViewport[0], oldViewport[1], oldViewport[2], oldViewport[3]);

        if (depthWasEnabled) m_gl->glEnable(GL_DEPTH_TEST); else m_gl->glDisable(GL_DEPTH_TEST);
        if (cullWasEnabled) m_gl->glEnable(GL_CULL_FACE); else m_gl->glDisable(GL_CULL_FACE);
        if (blendWasEnabled) m_gl->glEnable(GL_BLEND); else m_gl->glDisable(GL_BLEND);

        return result;
    }

    QImage warpRasterizedMeshCurrent( const QVector<QPointF>& warpedGridPoints, QPointF* outputOrigin, const WarpOptions& options ) {
        const MeshBuild mesh = buildRasterizedMesh(warpedGridPoints, options);

        const QRectF outputRect = m_hasFixedOutputRect
            ? m_fixedOutputRect
            : mesh.bounds;

        int outX = 0;
        int outY = 0;
        int outW = 1;
        int outH = 1;
        outputRectToInts(outputRect, outX, outY, outW, outH);

        if (outputOrigin)
            *outputOrigin = QPointF(outX, outY);

        ensureFbo(outW, outH);

        if (!m_fbo || !m_fbo->isValid()) {
            qWarning() << "CageWarpRenderer::warpRasterizedMeshCurrent: invalid framebuffer";
            return QImage();
        }

        GLint oldFramebuffer = 0;
        GLint oldViewport[4] = {};
        const GLboolean depthWasEnabled = m_gl->glIsEnabled(GL_DEPTH_TEST);
        const GLboolean cullWasEnabled = m_gl->glIsEnabled(GL_CULL_FACE);
        const GLboolean blendWasEnabled = m_gl->glIsEnabled(GL_BLEND);

        m_gl->glGetIntegerv(GL_FRAMEBUFFER_BINDING, &oldFramebuffer);
        m_gl->glGetIntegerv(GL_VIEWPORT, oldViewport);

        m_fbo->bind();

        m_gl->glViewport(0, 0, outW, outH);
        m_gl->glDisable(GL_DEPTH_TEST);
        m_gl->glDisable(GL_CULL_FACE);
        m_gl->glDisable(GL_BLEND);
        m_gl->glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
        m_gl->glClear(GL_COLOR_BUFFER_BIT);

        m_meshProgram->bind();
        m_meshProgram->setUniformValue("uImage", 0);
        m_meshProgram->setUniformValue("uOutputSize", float(outW), float(outH));
        m_meshProgram->setUniformValue("uOutputOrigin", float(outX), float(outY));

        m_imageTexture->bind(0);

        m_meshVao.bind();

        m_meshVbo.bind();
        m_meshVbo.allocate(
            mesh.vertices.data(),
            int(mesh.vertices.size() * sizeof(MeshVertex))
        );

        m_meshIbo.bind();
        m_meshIbo.allocate(
            mesh.indices.data(),
            int(mesh.indices.size() * sizeof(GLuint))
        );

        m_meshProgram->enableAttributeArray(0);
        m_meshProgram->enableAttributeArray(1);

        m_meshProgram->setAttributeBuffer(
            0,
            GL_FLOAT,
            int(offsetof(MeshVertex, targetX)),
            2,
            sizeof(MeshVertex)
        );

        m_meshProgram->setAttributeBuffer(
            1,
            GL_FLOAT,
            int(offsetof(MeshVertex, sourceU)),
            2,
            sizeof(MeshVertex)
        );
        m_gl->glDrawElements(
            GL_TRIANGLES,
            GLsizei(mesh.indices.size()),
            GL_UNSIGNED_INT,
            nullptr
        );
        m_meshProgram->disableAttributeArray(0);
        m_meshProgram->disableAttributeArray(1);
        m_meshIbo.release();
        m_meshVbo.release();
        m_meshVao.release();
        m_imageTexture->release();
        m_meshProgram->release();
        QImage result = m_fbo->toImage();
        m_gl->glBindFramebuffer(GL_FRAMEBUFFER, GLuint(oldFramebuffer));
        m_gl->glViewport(oldViewport[0], oldViewport[1], oldViewport[2], oldViewport[3]);
        if (depthWasEnabled) m_gl->glEnable(GL_DEPTH_TEST); else m_gl->glDisable(GL_DEPTH_TEST);
        if (cullWasEnabled) m_gl->glEnable(GL_CULL_FACE); else m_gl->glDisable(GL_CULL_FACE);
        if (blendWasEnabled) m_gl->glEnable(GL_BLEND); else m_gl->glDisable(GL_BLEND);
        return result;
    }

    void destroy() {
        if (!m_context)
            return;

        QOpenGLContext* previousContext = QOpenGLContext::currentContext();
        QSurface* previousSurface = previousContext ? previousContext->surface() : nullptr;

        if ( m_surface && m_context->makeCurrent(m_surface.get()) ) {
            m_fbo.reset();
            m_cageTexture.reset();
            m_imageTexture.reset();
            m_inverseProgram.reset();
            m_meshProgram.reset();
            if (m_meshIbo.isCreated())
                m_meshIbo.destroy();
            if (m_meshVbo.isCreated())
                m_meshVbo.destroy();
            m_meshVao.destroy();
            m_fullscreenVao.destroy();
            m_context->doneCurrent();
        }
        if ( previousContext && previousContext != m_context.get() && previousSurface )
            previousContext->makeCurrent(previousSurface);
        m_gl = nullptr;
        m_initialized = false;
        m_surface.reset();
        m_context.reset();
    }

  private:
  
    std::unique_ptr<QOpenGLContext> m_context;
    std::unique_ptr<QOffscreenSurface> m_surface;
    QOpenGLFunctions* m_gl = nullptr;

    bool m_initialized = false;

    std::unique_ptr<QOpenGLShaderProgram> m_inverseProgram;
    std::unique_ptr<QOpenGLShaderProgram> m_meshProgram;

    QOpenGLVertexArrayObject m_fullscreenVao;
    QOpenGLVertexArrayObject m_meshVao;
    QOpenGLBuffer m_meshVbo = QOpenGLBuffer(QOpenGLBuffer::VertexBuffer);
    QOpenGLBuffer m_meshIbo = QOpenGLBuffer(QOpenGLBuffer::IndexBuffer);

    QSize m_sourceSize;
    int m_gridColumns = 0;
    int m_gridRows = 0;

    std::unique_ptr<QOpenGLTexture> m_imageTexture;
    std::unique_ptr<QOpenGLTexture> m_cageTexture;
    std::unique_ptr<QOpenGLFramebufferObject> m_fbo;
    std::vector<float> m_cageData;

    bool m_hasFixedOutputRect = false;
    QRectF m_fixedOutputRect;
    
};
