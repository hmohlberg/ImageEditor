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

#include "MetalTransform.h"

#ifdef __APPLE__

#import <Metal/Metal.h>
#import <MetalPerformanceShaders/MetalPerformanceShaders.h>
#include <Accelerate/Accelerate.h>
#include <cmath>

// ---------------------------------------------------------------------------
// QImage::Format_ARGB32 on little-endian (Apple Silicon):
//   pixel value 0xAARRGGBB stored in memory as bytes [B, G, R, A].
//   That matches MTLPixelFormatBGRA8Unorm — no channel-swap needed.
// ---------------------------------------------------------------------------

namespace MetalTransform {

// ---- GPU device / command queue — initialised once, reused per call --------
static id<MTLDevice>       g_device       = nil;
static id<MTLCommandQueue> g_commandQueue = nil;

static bool ensureGPU()
{
    if (g_device) return true;
    g_device = MTLCreateSystemDefaultDevice();
    if (!g_device) return false;
    g_commandQueue = [g_device newCommandQueue];
    return g_commandQueue != nil;
}

// ============================================================================
// transformWithMPS — GPU Lanczos scale (Metal Performance Shaders)
// ============================================================================
// Restricted to pure positive scale transforms.  Pure flips are handled by
// QImage::flipped() in LayerItem before this is ever called.
// ============================================================================
QImage transformWithMPS(const QImage& src, const QTransform& t)
{
    // Only handle pure positive scale (no shear, no rotation, no flip).
    if (!qFuzzyIsNull(t.m12()) || !qFuzzyIsNull(t.m21())) return QImage();
    if (t.m11() <= 0.0 || t.m22() <= 0.0)                 return QImage();
    if (!ensureGPU())                                       return QImage();

    // Ensure ARGB32: on little-endian ARM its memory layout is [B,G,R,A]
    // which matches MTLPixelFormatBGRA8Unorm — zero-copy upload.
    const QImage srcArgb = (src.format() == QImage::Format_ARGB32)
                           ? src : src.convertToFormat(QImage::Format_ARGB32);
    const int srcW = srcArgb.width(),  srcH = srcArgb.height();

    // Destination dimensions
    const QRectF destRect = t.mapRect(QRectF(srcArgb.rect()));
    const int dw = qMax(1, (int)std::ceil(destRect.width()));
    const int dh = qMax(1, (int)std::ceil(destRect.height()));

    // --- Source texture (shared memory on Apple Silicon — no extra copy) ---
    MTLTextureDescriptor* srcDesc =
        [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm
                                                          width:srcW
                                                         height:srcH
                                                      mipmapped:NO];
    srcDesc.usage       = MTLTextureUsageShaderRead;
    srcDesc.storageMode = MTLStorageModeShared;
    id<MTLTexture> srcTex = [g_device newTextureWithDescriptor:srcDesc];
    if (!srcTex) return QImage();
    [srcTex replaceRegion:MTLRegionMake2D(0, 0, srcW, srcH)
             mipmapLevel:0
               withBytes:srcArgb.constBits()
             bytesPerRow:(NSUInteger)srcArgb.bytesPerLine()];

    // --- Destination texture ---
    MTLTextureDescriptor* dstDesc =
        [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm
                                                          width:dw
                                                         height:dh
                                                      mipmapped:NO];
    dstDesc.usage       = MTLTextureUsageShaderWrite;
    dstDesc.storageMode = MTLStorageModeShared;
    id<MTLTexture> dstTex = [g_device newTextureWithDescriptor:dstDesc];
    if (!dstTex) return QImage();

    // --- MPS scale transform ---
    // Dest pixel (px, py) → source pixel (px / m11,  py / m22).
    // The destRect offset is accounted for by LayerItem placing the output
    // image in scene coordinates, so translateX/Y are always 0 here.
    MPSScaleTransform scaleT;
    scaleT.scaleX     = 1.0 / t.m11();
    scaleT.scaleY     = 1.0 / t.m22();
    scaleT.translateX = 0.0;
    scaleT.translateY = 0.0;

    MPSImageLanczosScale* lanczos = [[MPSImageLanczosScale alloc] initWithDevice:g_device];
    lanczos.scaleTransform = &scaleT;

    id<MTLCommandBuffer> cmdBuf = [g_commandQueue commandBuffer];
    if (!cmdBuf) return QImage();
    [lanczos encodeToCommandBuffer:cmdBuf
                     sourceTexture:srcTex
                destinationTexture:dstTex];
    [cmdBuf commit];
    [cmdBuf waitUntilCompleted];
    if (cmdBuf.status == MTLCommandBufferStatusError) return QImage();

    // Read back (on Apple Silicon this is a shared memory pointer — very cheap)
    QImage result(dw, dh, QImage::Format_ARGB32);
    result.fill(Qt::transparent);
    [dstTex getBytes:result.bits()
         bytesPerRow:(NSUInteger)result.bytesPerLine()
          fromRegion:MTLRegionMake2D(0, 0, dw, dh)
         mipmapLevel:0];
    return result;
}

// ============================================================================
// transformWithVImage — SIMD Lanczos-5 warp (Accelerate / vImage)
// ============================================================================
// Handles arbitrary affine transforms.  On Apple Silicon, vImage uses AMX
// and NEON, so it is significantly faster than a hand-written bicubic loop.
// ============================================================================
QImage transformWithVImage(const QImage& src, const QTransform& t)
{
    bool invertible = false;
    QTransform inv = t.inverted(&invertible);
    if (!invertible) return QImage();

    // Grayscale8 fast path: vImageAffineWarpCG_Planar8 — 4× less bandwidth.
    // Returns Grayscale8 so m_originalImage stays in physical-value space.
    if (src.format() == QImage::Format_Grayscale8) {
        const QRectF destRect = t.mapRect(QRectF(src.rect()));
        const int dw = qMax(1, (int)std::ceil(destRect.width()));
        const int dh = qMax(1, (int)std::ceil(destRect.height()));

        QImage dest(dw, dh, QImage::Format_Grayscale8);
        dest.fill(0);

        vImage_Buffer srcBuf = {
            const_cast<void*>(static_cast<const void*>(src.constBits())),
            static_cast<vImagePixelCount>(src.height()),
            static_cast<vImagePixelCount>(src.width()),
            static_cast<size_t>(src.bytesPerLine())
        };
        vImage_Buffer dstBuf = {
            dest.bits(),
            static_cast<vImagePixelCount>(dh),
            static_cast<vImagePixelCount>(dw),
            static_cast<size_t>(dest.bytesPerLine())
        };

        const double dl = destRect.left(), dt = destRect.top();
        vImage_CGAffineTransform vt;
        vt.a  = inv.m11();  vt.b  = inv.m12();
        vt.c  = inv.m21();  vt.d  = inv.m22();
        vt.tx = inv.m11()*dl + inv.m21()*dt + inv.dx();
        vt.ty = inv.m12()*dl + inv.m22()*dt + inv.dy();

        const Pixel_8 backColor = 0;
        const vImage_Error err = vImageAffineWarpCG_Planar8(
            &srcBuf, &dstBuf,
            /*tempBuffer=*/nullptr,
            &vt,
            backColor,
            kvImageHighQualityResampling | kvImageEdgeExtend);

        return (err == kvImageNoError) ? dest : QImage();
    }

    // Ensure ARGB32
    const QImage srcArgb = (src.format() == QImage::Format_ARGB32)
                           ? src : src.convertToFormat(QImage::Format_ARGB32);

    const QRectF destRect = t.mapRect(QRectF(srcArgb.rect()));
    const int dw = qMax(1, (int)std::ceil(destRect.width()));
    const int dh = qMax(1, (int)std::ceil(destRect.height()));

    QImage dest(dw, dh, QImage::Format_ARGB32);
    dest.fill(Qt::transparent);

    vImage_Buffer srcBuf = {
        const_cast<void*>(static_cast<const void*>(srcArgb.constBits())),
        static_cast<vImagePixelCount>(srcArgb.height()),
        static_cast<vImagePixelCount>(srcArgb.width()),
        static_cast<size_t>(srcArgb.bytesPerLine())
    };
    vImage_Buffer dstBuf = {
        dest.bits(),
        static_cast<vImagePixelCount>(dh),
        static_cast<vImagePixelCount>(dw),
        static_cast<size_t>(dest.bytesPerLine())
    };

    // vImage_CGAffineTransform maps destination pixel → source pixel:
    //   src_x = a * dest_x + c * dest_y + tx
    //   src_y = b * dest_x + d * dest_y + ty
    //
    // Destination pixel (px, py) is at scene position
    //   (px + destRect.left(), py + destRect.top()).
    // That scene point maps to source via the inverse QTransform:
    //   src_x = inv.m11*(px+dl) + inv.m21*(py+dt) + inv.dx
    //         = inv.m11*px + inv.m21*py + (inv.m11*dl + inv.m21*dt + inv.dx)
    const double dl = destRect.left(), dt = destRect.top();
    vImage_CGAffineTransform vt;
    vt.a  = inv.m11();
    vt.b  = inv.m12();
    vt.c  = inv.m21();
    vt.d  = inv.m22();
    vt.tx = inv.m11()*dl + inv.m21()*dt + inv.dx();
    vt.ty = inv.m12()*dl + inv.m22()*dt + inv.dy();

    // Use kvImageBackgroundColorFill (not kvImageEdgeExtend) so that destination
    // pixels outside the source rectangle (rotation corners) become transparent
    // (backColor = {0,0,0,0}) rather than getting the nearest source edge pixel.
    // kvImageEdgeExtend would fill corners with the opaque edge value of the
    // source, producing a black border around rotated layers.
    const Pixel_8888 backColor = { 0, 0, 0, 0 };
    const vImage_Error err = vImageAffineWarpCG_ARGB8888(
        &srcBuf, &dstBuf,
        /*tempBuffer=*/nullptr,
        &vt,
        backColor,
        kvImageHighQualityResampling | kvImageBackgroundColorFill);

    return (err == kvImageNoError) ? dest : QImage();
}

} // namespace MetalTransform
#endif // __APPLE__
