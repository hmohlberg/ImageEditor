#include "Inpainting.h"

#include <algorithm>
#include <cmath>
#include <queue>
#include <random>
#include <vector>

// ─────────────────────────────────────────────────────────────────────────────
// Internal types
// ─────────────────────────────────────────────────────────────────────────────

struct Match { int dx, dy; float dist; };

// ─────────────────────────────────────────────────────────────────────────────
// Patch distance: SSD over pixels where the *source* patch is known (not hole).
// earlyOut: abandon and return current dist when it already exceeds the threshold.
// ─────────────────────────────────────────────────────────────────────────────
static float patchSSD( const uint32_t* pix, const uint8_t* hole,
                        int W, int H,
                        int ax, int ay,   // source patch centre (hole side)
                        int bx, int by,   // candidate patch centre (known side)
                        int R, float earlyOut )
{
    float dist = 0.f;
    int   cnt  = 0;
    for ( int ry = -R; ry <= R; ++ry ) {
        int say = ay + ry, sby = by + ry;
        if ( say < 0 || say >= H || sby < 0 || sby >= H ) continue;
        const uint32_t* rowA = pix + say * W;
        const uint32_t* rowB = pix + sby * W;
        const uint8_t*  hA   = hole + say * W;
        for ( int rx = -R; rx <= R; ++rx ) {
            int sax = ax + rx, sbx = bx + rx;
            if ( sax < 0 || sax >= W || sbx < 0 || sbx >= W ) continue;
            if ( hA[sax] ) continue;              // skip unknown source pixels
            uint32_t ca = rowA[sax], cb = rowB[sbx];
            int r = int((ca>>16)&0xFF) - int((cb>>16)&0xFF);
            int g = int((ca>> 8)&0xFF) - int((cb>> 8)&0xFF);
            int b = int( ca     &0xFF) - int( cb     &0xFF);
            dist += float(r*r + g*g + b*b);
            ++cnt;
            if ( dist > earlyOut ) return dist;
        }
    }
    return cnt > 0 ? dist / float(cnt) : 1e30f;
}

// Try offset (dx,dy) for hole pixel (px,py); update best if it improves.
static bool tryOffset( Match& best,
                        const uint32_t* pix, const uint8_t* hole,
                        const uint8_t* allowed,
                        int W, int H,
                        int px, int py, int dx, int dy, int R )
{
    int qx = px + dx, qy = py + dy;
    if ( qx < 0 || qx >= W || qy < 0 || qy >= H ) return false;
    if ( hole[qy * W + qx] ) return false;       // candidate must be known
    if ( !allowed[qy * W + qx] ) return false;   // candidate must be in source region
    float d = patchSSD( pix, hole, W, H, px, py, qx, qy, R, best.dist );
    if ( d < best.dist ) { best = { dx, dy, d }; return true; }
    return false;
}

// ─────────────────────────────────────────────────────────────────────────────

QImage Inpainting::run( const QImage& src, const QImage& holeMask,
                        const QImage& sourceMask,
                        int patchRadius, int iterations )
{
    if ( src.isNull() || holeMask.isNull() ) return src;

    const int srcW = src.width(), srcH = src.height();

    // ── 1. Build full-image hole mask ────────────────────────────────────────
    QImage gm = holeMask.convertToFormat( QImage::Format_Grayscale8 );
    std::vector<uint8_t> holeAll( srcW * srcH, 0 );
    for ( int y = 0; y < srcH; ++y ) {
        const uint8_t* row = gm.constScanLine(y);
        for ( int x = 0; x < srcW; ++x )
            if ( row[x] ) holeAll[y * srcW + x] = 1;
    }

    // ── 1b. Build source-region mask (optional) ──────────────────────────────
    // When provided, only source-mask pixels are valid NNF targets.
    std::vector<uint8_t> srcAllowed( srcW * srcH, 1 ); // 1 = allowed by default
    if ( !sourceMask.isNull() ) {
        std::fill( srcAllowed.begin(), srcAllowed.end(), 0 );
        QImage sm = sourceMask.convertToFormat( QImage::Format_Grayscale8 );
        for ( int y = 0; y < srcH; ++y ) {
            const uint8_t* row = sm.constScanLine(y);
            for ( int x = 0; x < srcW; ++x )
                if ( row[x] ) srcAllowed[y * srcW + x] = 1;
        }
    }

    // ── 2. Find bounding box of hole + context margin ────────────────────────
    int hx0 = srcW, hy0 = srcH, hx1 = 0, hy1 = 0;
    for ( int y = 0; y < srcH; ++y )
        for ( int x = 0; x < srcW; ++x )
            if ( holeAll[y * srcW + x] ) {
                hx0 = std::min(hx0, x); hy0 = std::min(hy0, y);
                hx1 = std::max(hx1, x); hy1 = std::max(hy1, y);
            }

    if ( hx0 > hx1 ) return src; // no hole

    int holeW  = hx1 - hx0 + 1, holeH = hy1 - hy0 + 1;
    int margin = std::max( 3 * std::max(holeW, holeH), 4 * patchRadius + 32 );
    int cx0 = std::max( 0, hx0 - margin );
    int cy0 = std::max( 0, hy0 - margin );
    int cx1 = std::min( srcW - 1, hx1 + margin );
    int cy1 = std::min( srcH - 1, hy1 + margin );
    int W   = cx1 - cx0 + 1;
    int H   = cy1 - cy0 + 1;

    // ── 3. Build cropped pixel buffer, hole mask and source-allowed mask ─────
    QImage workSrc = src.convertToFormat( QImage::Format_ARGB32 );

    std::vector<uint32_t> pixels ( W * H );
    std::vector<uint8_t>  hole   ( W * H, 0 );
    std::vector<uint8_t>  allowed( W * H, 0 );

    for ( int y = 0; y < H; ++y ) {
        const uint32_t* srcRow = reinterpret_cast<const uint32_t*>(
                                     workSrc.constScanLine( cy0 + y )) + cx0;
        for ( int x = 0; x < W; ++x ) {
            pixels [y * W + x] = srcRow[x];
            hole   [y * W + x] = holeAll  [(cy0+y)*srcW+(cx0+x)];
            allowed[y * W + x] = srcAllowed[(cy0+y)*srcW+(cx0+x)];
        }
    }

    // ── 4. Collect valid source indices for random NNF init ──────────────────
    // A pixel is a valid source if it is not a hole AND it is in the allowed set.
    std::vector<int> knownIdx;
    knownIdx.reserve( W * H );
    for ( int i = 0; i < W * H; ++i )
        if ( !hole[i] && allowed[i] ) knownIdx.push_back(i);

    if ( knownIdx.empty() ) return src;

    // ── 5. Initialise NNF ────────────────────────────────────────────────────
    std::mt19937 rng( 42 );
    std::vector<Match> nnf( W * H, {0, 0, 0.f} );

    for ( int y = 0; y < H; ++y ) {
        for ( int x = 0; x < W; ++x ) {
            if ( !hole[y * W + x] ) continue;
            int ki = knownIdx[ rng() % knownIdx.size() ];
            int kx = ki % W, ky = ki / W;
            float d = patchSSD( pixels.data(), hole.data(), W, H,
                                x, y, kx, ky, patchRadius, 1e30f );
            nnf[y * W + x] = { kx - x, ky - y, d };
        }
    }

    // ── 6. PatchMatch iterations ─────────────────────────────────────────────
    const int maxR = std::max(W, H);
    std::uniform_real_distribution<float> uni(-1.f, 1.f);

    for ( int iter = 0; iter < iterations; ++iter ) {
        const bool rev   = iter & 1;
        const int  yS    = rev ? H-1 : 0,   yE = rev ? -1 : H,  yD = rev ? -1 : 1;
        const int  xS    = rev ? W-1 : 0,   xE = rev ? -1 : W,  xD = rev ? -1 : 1;

        for ( int y = yS; y != yE; y += yD ) {
            for ( int x = xS; x != xE; x += xD ) {
                if ( !hole[y * W + x] ) continue;
                Match& best = nnf[y * W + x];

                // Propagation from spatial neighbours
                int nx = x - xD;
                if ( nx >= 0 && nx < W ) {
                    const Match& nm = nnf[y * W + nx];
                    tryOffset( best, pixels.data(), hole.data(), allowed.data(), W, H,
                                x, y, nm.dx, nm.dy, patchRadius );
                }
                int ny = y - yD;
                if ( ny >= 0 && ny < H ) {
                    const Match& nm = nnf[ny * W + x];
                    tryOffset( best, pixels.data(), hole.data(), allowed.data(), W, H,
                                x, y, nm.dx, nm.dy, patchRadius );
                }

                // Random search at halving radii
                float radius = float(maxR);
                while ( radius >= 1.f ) {
                    int rdx = best.dx + int( uni(rng) * radius );
                    int rdy = best.dy + int( uni(rng) * radius );
                    tryOffset( best, pixels.data(), hole.data(), allowed.data(), W, H,
                                x, y, rdx, rdy, patchRadius );
                    radius *= 0.5f;
                }
            }
        }
    }

    // ── 7. Onion-peel reconstruction ─────────────────────────────────────────
    // Fill hole pixels from the boundary inward (BFS order).  Each pixel is
    // assigned directly from its best-match NNF target — no averaging — so the
    // original texture variability is fully preserved.  Already-filled pixels
    // are immediately available as sources for deeper pixels.
    std::vector<uint32_t> result( pixels );
    std::vector<uint8_t>  fillHole( hole );   // working copy; 0 = known/filled

    // BFS: collect fill order by distance from the known region
    std::vector<int> fillOrder;
    fillOrder.reserve( knownIdx.size() );     // hole can't be larger than known
    std::vector<int> dist( W * H, -1 );
    std::queue<int>  bfsQ;

    for ( int y = 0; y < H; ++y ) {
        for ( int x = 0; x < W; ++x ) {
            if ( !fillHole[y * W + x] ) continue;
            bool adj = ( x > 0   && !fillHole[y*W+x-1] )
                    || ( x < W-1 && !fillHole[y*W+x+1] )
                    || ( y > 0   && !fillHole[(y-1)*W+x] )
                    || ( y < H-1 && !fillHole[(y+1)*W+x] );
            if ( adj ) { dist[y*W+x] = 0; bfsQ.push(y*W+x); }
        }
    }
    while ( !bfsQ.empty() ) {
        int i = bfsQ.front(); bfsQ.pop();
        fillOrder.push_back(i);
        int ix = i % W, iy = i / W, d = dist[i];
        int nb[4]  = { i-1, i+1, i-W, i+W };
        bool ok[4] = { ix>0, ix<W-1, iy>0, iy<H-1 };
        for ( int k = 0; k < 4; ++k ) {
            if ( ok[k] && fillHole[nb[k]] && dist[nb[k]] < 0 ) {
                dist[nb[k]] = d + 1;
                bfsQ.push(nb[k]);
            }
        }
    }

    // Assign each hole pixel using a small 3×3 vote window (NNF of pixel and
    // direct neighbours), averaging only already-filled-or-known sources.
    // This suppresses salt-and-pepper while keeping texture variability.
    for ( int idx : fillOrder ) {
        int x = idx % W, y = idx / W;
        float sumW = 0.f, sumR = 0.f, sumG = 0.f, sumB = 0.f;
        for ( int ry = -1; ry <= 1; ++ry ) {
            int qy = y + ry;
            if ( qy < 0 || qy >= H ) continue;
            for ( int rx = -1; rx <= 1; ++rx ) {
                int qx = x + rx;
                if ( qx < 0 || qx >= W ) continue;
                const Match& m = nnf[qy * W + qx];
                int sx = x + m.dx, sy = y + m.dy;
                if ( sx < 0 || sx >= W || sy < 0 || sy >= H ) continue;
                if ( fillHole[sy * W + sx] ) continue;  // not yet filled
                float w    = 1.f / (1.f + m.dist);
                uint32_t c = result[sy * W + sx];
                sumR += w * float((c >> 16) & 0xFF);
                sumG += w * float((c >>  8) & 0xFF);
                sumB += w * float( c        & 0xFF);
                sumW += w;
            }
        }
        if ( sumW > 0.f ) {
            result[idx] = 0xFF000000u
                | ( uint32_t(std::clamp(int(sumR/sumW+.5f),0,255)) << 16 )
                | ( uint32_t(std::clamp(int(sumG/sumW+.5f),0,255)) <<  8 )
                |   uint32_t(std::clamp(int(sumB/sumW+.5f),0,255));
        }
        fillHole[idx] = 0;   // now available as source for deeper pixels
    }

    // ── 8. Paste inpainted crop back into full-size result ───────────────────
    QImage output = workSrc.copy();
    for ( int y = 0; y < H; ++y ) {
        uint32_t* dstRow = reinterpret_cast<uint32_t*>(output.scanLine( cy0 + y )) + cx0;
        for ( int x = 0; x < W; ++x ) {
            if ( hole[y * W + x] )
                dstRow[x] = result[y * W + x];
        }
    }
    return output;
}
