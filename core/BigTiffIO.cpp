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

#include "BigTiffIO.h"

#include <tiffio.h>

#include <QFile>

#include <vector>
#include <algorithm>
#include <cstring>

// ── helpers ───────────────────────────────────────────────────────────────────

struct TiffLevel {
    uint32_t w = 0, h = 0;
    uint32_t tileW = 0, tileH = 0;
    int      dirIdx = 0;
    bool     tiled  = false;
};

static QVector<TiffLevel> scanLevels(TIFF* tif)
{
    QVector<TiffLevel> raw;
    do {
        TiffLevel lvl;
        lvl.dirIdx = (int)TIFFCurrentDirectory(tif);
        TIFFGetField(tif, TIFFTAG_IMAGEWIDTH,  &lvl.w);
        TIFFGetField(tif, TIFFTAG_IMAGELENGTH, &lvl.h);
        lvl.tiled = TIFFIsTiled(tif) != 0;
        if (lvl.tiled) {
            TIFFGetField(tif, TIFFTAG_TILEWIDTH,  &lvl.tileW);
            TIFFGetField(tif, TIFFTAG_TILELENGTH, &lvl.tileH);
        } else {
            lvl.tileW = lvl.w;
            lvl.tileH = lvl.h;
        }
        if (lvl.w > 0 && lvl.h > 0)
            raw.append(lvl);
    } while (TIFFReadDirectory(tif));

    std::sort(raw.begin(), raw.end(),
              [](const TiffLevel& a, const TiffLevel& b){ return a.w > b.w; });
    return raw;
}

// ── public API ────────────────────────────────────────────────────────────────

bool bigTiffIsBigTiff(const QString& path)
{
    TIFF* t = TIFFOpen(path.toLocal8Bit().constData(), "r");
    if (!t) return false;
    bool big = TIFFIsBigTIFF(t) != 0;
    TIFFClose(t);
    return big;
}

bool bigTiffCopyPyramid(const QString& inputPath,
                        const QString& outputPath,
                        std::function<bool(int)> progress,
                        QString* errorOut)
{
    auto fail = [&](const QString& msg) -> bool {
        if (errorOut) *errorOut = msg;
        return false;
    };

    // Suppress libtiff tag-order warnings
    TIFFSetWarningHandler(nullptr);

    TIFF* in = TIFFOpen(inputPath.toLocal8Bit().constData(), "r");
    if (!in) return fail(QString("Cannot open input file: %1").arg(inputPath));

    QVector<TiffLevel> levels = scanLevels(in);
    if (levels.isEmpty()) {
        TIFFClose(in);
        return fail("No valid IFDs found in input file.");
    }

    TIFF* out = TIFFOpen(outputPath.toLocal8Bit().constData(), "w8");
    if (!out) {
        TIFFClose(in);
        return fail(QString("Cannot create output file: %1").arg(outputPath));
    }

    if (!TIFFIsBigTIFF(out)) {
        TIFFClose(out);
        TIFFClose(in);
        QFile::remove(outputPath);
        return fail(
            "The libtiff library loaded at runtime does not support BigTIFF "
            "write mode (\"w8\"). Ensure libtiff >= 4.0 is on the rpath.");
    }

    // Count total tiles for progress reporting
    int totalTiles = 0;
    for (const auto& lvl : levels) {
        uint32_t tw = lvl.tiled ? lvl.tileW : 256u;
        uint32_t th = lvl.tiled ? lvl.tileH : 256u;
        totalTiles += ((int(lvl.w) + int(tw) - 1) / int(tw))
                    * ((int(lvl.h) + int(th) - 1) / int(th));
    }
    int done = 0;
    bool cancelled = false;

    auto report = [&](int pct) {
        if (progress && !progress(pct)) cancelled = true;
    };

    for (int li = 0; li < levels.size() && !cancelled; ++li) {
        const auto& lvl = levels[li];
        TIFFSetDirectory(in, (uint16_t)lvl.dirIdx);

        // Read source metadata
        uint16_t bps = 8, spp = 1;
        uint16_t photo = PHOTOMETRIC_MINISBLACK;
        TIFFGetField(in, TIFFTAG_BITSPERSAMPLE,   &bps);
        TIFFGetField(in, TIFFTAG_SAMPLESPERPIXEL, &spp);
        TIFFGetField(in, TIFFTAG_PHOTOMETRIC,     &photo);

        uint32_t outTW = lvl.tiled ? lvl.tileW : 256u;
        uint32_t outTH = lvl.tiled ? lvl.tileH : 256u;

        TIFFSetField(out, TIFFTAG_IMAGEWIDTH,      lvl.w);
        TIFFSetField(out, TIFFTAG_IMAGELENGTH,     lvl.h);
        TIFFSetField(out, TIFFTAG_TILEWIDTH,       outTW);
        TIFFSetField(out, TIFFTAG_TILELENGTH,      outTH);
        TIFFSetField(out, TIFFTAG_BITSPERSAMPLE,   bps);
        TIFFSetField(out, TIFFTAG_SAMPLESPERPIXEL, spp);
        TIFFSetField(out, TIFFTAG_PHOTOMETRIC,     photo);
        TIFFSetField(out, TIFFTAG_PLANARCONFIG,    PLANARCONFIG_CONTIG);
        TIFFSetField(out, TIFFTAG_COMPRESSION,     COMPRESSION_DEFLATE);
        TIFFSetField(out, TIFFTAG_PREDICTOR,       PREDICTOR_HORIZONTAL);
        if (li > 0)
            TIFFSetField(out, TIFFTAG_SUBFILETYPE, (uint32_t)FILETYPE_REDUCEDIMAGE);

        int tilesX = (int(lvl.w) + int(outTW) - 1) / int(outTW);
        int tilesY = (int(lvl.h) + int(outTH) - 1) / int(outTH);

        if (lvl.tiled) {
            tsize_t tileBytes = TIFFTileSize(in);
            std::vector<uint8_t> buf((size_t)tileBytes);
            for (int ty = 0; ty < tilesY && !cancelled; ++ty) {
                for (int tx = 0; tx < tilesX && !cancelled; ++tx) {
                    uint32_t x = (uint32_t)tx * outTW;
                    uint32_t y = (uint32_t)ty * outTH;
                    ttile_t idx = TIFFComputeTile(in, x, y, 0, 0);
                    if (TIFFReadEncodedTile(in, idx, buf.data(), tileBytes) > 0)
                        TIFFWriteTile(out, buf.data(), x, y, 0, 0);
                    report(++done * 100 / totalTiles);
                }
            }
        } else {
            // Strip-based level: read as RGBA raster, chop into tiles
            std::vector<uint32_t> raster((size_t)lvl.w * lvl.h);
            TIFFReadRGBAImageOriented(in, lvl.w, lvl.h,
                                      raster.data(), ORIENTATION_TOPLEFT, 0);
            size_t outBufBytes = (size_t)outTW * outTH * spp;
            std::vector<uint8_t> outBuf(outBufBytes, 0);
            for (int ty = 0; ty < tilesY && !cancelled; ++ty) {
                for (int tx = 0; tx < tilesX && !cancelled; ++tx) {
                    uint32_t x0 = (uint32_t)tx * outTW;
                    uint32_t y0 = (uint32_t)ty * outTH;
                    uint32_t tw = std::min(outTW, lvl.w - x0);
                    uint32_t th = std::min(outTH, lvl.h - y0);
                    std::fill(outBuf.begin(), outBuf.end(), 0);
                    for (uint32_t r = 0; r < th; ++r) {
                        for (uint32_t c = 0; c < tw; ++c) {
                            uint32_t abgr = raster[(y0 + r) * lvl.w + (x0 + c)];
                            size_t   di   = ((size_t)r * outTW + c) * spp;
                            if (spp >= 3) {
                                outBuf[di]   = TIFFGetR(abgr);
                                outBuf[di+1] = TIFFGetG(abgr);
                                outBuf[di+2] = TIFFGetB(abgr);
                                if (spp == 4) outBuf[di+3] = TIFFGetA(abgr);
                            } else {
                                outBuf[di] = TIFFGetR(abgr);
                            }
                        }
                    }
                    TIFFWriteTile(out, outBuf.data(), x0, y0, 0, 0);
                    report(++done * 100 / totalTiles);
                }
            }
        }

        TIFFWriteDirectory(out);
    }

    TIFFClose(out);
    TIFFClose(in);

    if (cancelled) {
        QFile::remove(outputPath);
        return fail("Cancelled by user.");
    }
    return true;
}
