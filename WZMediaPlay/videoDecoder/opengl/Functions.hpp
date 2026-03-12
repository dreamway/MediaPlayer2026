/*
    简化的 Functions 工具类，只包含核心渲染所需的函数
*/

#pragma once

#include <QMatrix3x3>
#include <libavutil/pixfmt.h>

namespace Functions {
    // YUV 到 RGB 颜色空间转换矩阵
    // colorSpace: 视频的颜色空间
    // frameHeight: 视频帧高度，用于在 colorSpace 为 UNSPECIFIED 时猜测正确的颜色空间
    QMatrix3x3 getYUVtoRGBmatrix(AVColorSpace colorSpace, int frameHeight = 0);

    // 颜色原色到 709 的转换矩阵
    QMatrix3x3 getColorPrimariesTo709Matrix(AVColorPrimaries colorPrimaries);
}
