#pragma once
#include <QImage>
#include <QString>

class LaMaInpainting
{
public:
    // Default path where the lama.onnx model is expected.
    static QString defaultModelPath();

    // Returns true when the ONNX model file exists at defaultModelPath().
    static bool isAvailable();

    // holeMask: any format, non-zero pixels = area to fill.
    // modelPath: path to lama_fp32.onnx (uses defaultModelPath() when empty).
    // errorMsg: filled on failure.
    // Returns null QImage on failure.
    static QImage run( const QImage& src,
                       const QImage& holeMask,
                       const QString& modelPath = QString(),
                       QString* errorMsg = nullptr );
};
