/*
    简化的 Functions 工具类实现
*/

#include "Functions.hpp"
#include <libavutil/pixfmt.h>
#include <QMatrix4x4>

QMatrix3x3 Functions::getYUVtoRGBmatrix(AVColorSpace colorSpace, int frameHeight)
{
    // 参考QMPlayer2的实现，使用正确的YUV到RGB转换矩阵
    // QMPlayer2使用QMatrix4x4，我们使用QMatrix3x3（只取前3x3部分）

    // 根据颜色空间获取RGB系数
    float cR = 0.299f;  // BT.601默认值
    float cB = 0.114f;  // BT.601默认值

    switch (colorSpace) {
        case AVCOL_SPC_BT709:
            cR = 0.2126f;
            cB = 0.0722f;
            break;
        case AVCOL_SPC_SMPTE170M:
        case AVCOL_SPC_BT470BG:
            cR = 0.299f;
            cB = 0.114f;
            break;
        case AVCOL_SPC_SMPTE240M:
            cR = 0.212f;
            cB = 0.087f;
            break;
        case AVCOL_SPC_BT2020_CL:
        case AVCOL_SPC_BT2020_NCL:
            cR = 0.2627f;
            cB = 0.0593f;
            break;
        case AVCOL_SPC_UNSPECIFIED:
        default:
            // 当颜色空间未指定时，根据视频分辨率猜测：
            // - SD 视频（<= 576p）使用 BT.601
            // - HD 视频（>= 720p）使用 BT.709
            // - UHD 视频（>= 2160p）使用 BT.2020
            if (frameHeight > 0 && frameHeight <= 576) {
                // SD 视频：使用 BT.601
                cR = 0.299f;
                cB = 0.114f;
            } else if (frameHeight >= 2160) {
                // UHD 视频：使用 BT.2020
                cR = 0.2627f;
                cB = 0.0593f;
            } else {
                // HD 视频或未知：使用 BT.709（默认）
                cR = 0.2126f;
                cB = 0.0722f;
            }
            break;
    }
    
    const float cG = 1.0f - cR - cB;
    const float bscale = 0.5f / (cB - 1.0f);
    const float rscale = 0.5f / (cR - 1.0f);
    
    // 构建4x4矩阵（参考QMPlayer2）
    QMatrix4x4 mat4x4(
       cR,          cG,          cB,          0.0f,
       bscale * cR, bscale * cG, 0.5f,        0.0f,
       0.5f,        rscale * cG, rscale * cB, 0.0f,
       0.0f,        0.0f,        0.0f,        1.0f
    );
    
    // 取逆矩阵（QMPlayer2使用inverted()）
    mat4x4 = mat4x4.inverted();
    
    // 转换为3x3矩阵（只取前3x3部分）
    QMatrix3x3 mat;
    mat(0, 0) = mat4x4(0, 0); mat(0, 1) = mat4x4(0, 1); mat(0, 2) = mat4x4(0, 2);
    mat(1, 0) = mat4x4(1, 0); mat(1, 1) = mat4x4(1, 1); mat(1, 2) = mat4x4(1, 2);
    mat(2, 0) = mat4x4(2, 0); mat(2, 1) = mat4x4(2, 1); mat(2, 2) = mat4x4(2, 2);
    
    return mat;
}

QMatrix3x3 Functions::getColorPrimariesTo709Matrix(AVColorPrimaries colorPrimaries)
{
    // 简化的颜色原色转换矩阵
    // 默认返回单位矩阵（不转换）
    QMatrix3x3 mat;
    mat.setToIdentity();
    
    // TODO: 实现完整的颜色原色转换
    // 目前只返回单位矩阵
    
    return mat;
}
