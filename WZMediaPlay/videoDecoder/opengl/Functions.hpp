/*
    简化的 Functions 工具类，只包含核心渲染所需的函数
*/

#pragma once

#include <QMatrix3x3>
#include <libavutil/pixfmt.h>

namespace Functions {
    // YUV 到 RGB 颜色空间转换矩阵
    QMatrix3x3 getYUVtoRGBmatrix(AVColorSpace colorSpace);
    
    // 颜色原色到 709 的转换矩阵
    QMatrix3x3 getColorPrimariesTo709Matrix(AVColorPrimaries colorPrimaries);
}
