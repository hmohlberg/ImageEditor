#pragma once
#include <QImage>

/**
 * PatchMatch-based inpainting (Simakov et al. / Barnes et al. style).
 *
 * For each hole pixel the algorithm finds the best matching patch in the
 * known region via propagation + random search, then reconstructs the hole
 * by weighted averaging of all patch contributions.
 *
 * patchRadius : half-side of the square comparison patch (default 4 → 9×9)
 * iterations  : PatchMatch sweeps (default 5)
 */
class Inpainting
{
public:
    // holeMask   : Grayscale8, non-zero = pixel to fill
    // sourceMask : Grayscale8, non-zero = pixel usable as source (empty = all non-hole pixels)
    static QImage run( const QImage& src,
                       const QImage& holeMask,
                       const QImage& sourceMask = QImage(),
                       int patchRadius = 4,
                       int iterations  = 8 );
};
