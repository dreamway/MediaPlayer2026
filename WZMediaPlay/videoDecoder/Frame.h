#pragma once

#include "chronons.h"
#include "ffmpeg.h"

extern "C" {
#include <libavutil/pixfmt.h>
#include <libavutil/rational.h>
#include <libavutil/mastering_display_metadata.h>
#include <libavutil/hwcontext.h>
#include <libavutil/pixdesc.h>  // 包含 AVPixFmtDescriptor 定义
#include <libavutil/frame.h>  // 包含 AVFrameSideData 定义
#include <libswscale/swscale.h>
}

#include <QVector>

#include <memory>
#include <vector>
#include <functional>
#include <cstring>

// 参考 QMPlayer2 的定义
using AVPixelFormats = QVector<AVPixelFormat>;

/**
 * Frame: 统一的视频帧封装类（参考 QMPlayer2 的 Frame 类）
 * 
 * 职责：
 * - 统一封装 AVFrame，隐藏硬件/软件细节
 * - 自动处理硬件帧下载和格式转换
 * - 提供统一的接口访问帧数据
 * - 封装帧的有效性检查
 * 
 * 设计特点：
 * - 单一 AVFrame 指针，不再区分 frame/sw_frame/yuv420p_frame
 * - 硬件帧通过 downloadHwData() 自动下载为软件帧
 * - 格式转换在需要时自动进行
 * - 提供 isEmpty() 方法统一检查有效性
 */
class Frame
{
public:
    using OnDestroyFn = std::function<void()>;
    
    /**
     * 静态工厂方法：创建空帧
     */
    static Frame createEmpty(
        const Frame &other,
        bool allocBuffers,
        AVPixelFormat newPixelFormat = AV_PIX_FMT_NONE
    );
    
    static Frame createEmpty(
        const AVFrame *other,
        bool allocBuffers,
        AVPixelFormat newPixelFormat = AV_PIX_FMT_NONE
    );
    
    static Frame createEmpty(
        int width,
        int height,
        AVPixelFormat pixelFormat,
        bool interlaced = false,
        bool topFieldFirst = false,
        AVColorSpace colorSpace = AVCOL_SPC_UNSPECIFIED,
        bool isLimited = true,
        AVColorPrimaries colorPrimaries = AVCOL_PRI_UNSPECIFIED,
        AVColorTransferCharacteristic colorTrc = AVCOL_TRC_UNSPECIFIED
    );

public:
    /**
     * 构造函数
     */
    Frame();
    explicit Frame(AVFrame *avFrame, AVPixelFormat newPixelFormat = AV_PIX_FMT_NONE);
    Frame(const Frame &other);
    Frame(Frame &&other) noexcept;
    ~Frame();

    /**
     * 赋值操作符
     */
    Frame &operator=(const Frame &other);
    Frame &operator=(Frame &&other) noexcept;

    /**
     * 检查帧是否为空/无效
     */
    bool isEmpty() const;
    
    /**
     * 清空帧数据
     */
    void clear();

    /**
     * 时间戳相关
     */
    void setTimeBase(const AVRational &timeBase);
    AVRational timeBase() const;
    
    bool isTsValid() const;
    
    // 使用 nanoseconds 类型（与现有代码兼容）
    nanoseconds ts() const;
    int64_t tsInt() const;
    
    void setTS(nanoseconds ts);
    void setTSInt(int64_t ts);

public: // Video 相关接口
    
    /**
     * 隔行扫描相关
     */
    bool isInterlaced() const;
    bool isTopFieldFirst() const;
    bool isSecondField() const;
    
    void setInterlaced(bool topFieldFirst);
    void setNoInterlaced();
    void setIsSecondField(bool secondField);

    /**
     * 硬件帧相关
     */
    bool hasCPUAccess() const;  // 是否有CPU访问权限（软件帧）
    bool isHW() const;          // 是否是硬件帧
    struct AVBufferRef* hwFrameBufferRef() const;  // 获取硬件帧缓冲区引用

    /**
     * 像素格式相关
     */
    AVPixelFormat pixelFormat() const;
    
    AVColorPrimaries colorPrimaries() const;
    AVColorTransferCharacteristic colorTrc() const;
    AVColorSpace colorSpace() const;
    bool isLimited() const;
    
    const AVMasteringDisplayMetadata *masteringDisplayMetadata() const;

    /**
     * 帧属性查询
     */
    bool isGray() const;
    bool isPlanar() const;
    bool isRGB() const;
    
    int chromaShiftW() const;
    int chromaShiftH() const;
    int numPlanes() const;
    
    int depth() const;
    int paddingBits() const;

    /**
     * 尺寸和步长
     */
    int *linesize() const;
    int linesize(int plane) const;
    int width(int plane = 0) const;
    int height(int plane = 0) const;
    
    AVRational sampleAspectRatio() const;

    /**
     * 数据访问
     */
    const uint8_t *constData(int plane = 0) const;
    uint8_t *data(int plane = 0);
    uint8_t **dataArr();

    /**
     * 设置视频数据
     */
    bool setVideoData(
        AVBufferRef *buffer[],
        const int *linesize,
        uint8_t *data[] = nullptr,
        bool ref = false
    );

    /**
     * 设置销毁回调
     */
    void setOnDestroyFn(const OnDestroyFn &onDestroyFn);

    /**
     * 复制数据到目标缓冲区
     */
    template<typename D, typename L>
    bool copyData(D *dest[4], L linesize[4]) const;

    /**
     * 下载硬件帧数据（转换为软件帧）
     * 
     * @param swsCtx SwsContext指针的指针（用于缓存转换上下文）
     * @param supportedPixelFormats 支持的像素格式列表（优先使用）
     * @return 下载后的软件帧（如果是硬件帧），否则返回自身
     */
    Frame downloadHwData(
        SwsContext **swsCtx = nullptr,
        const std::vector<AVPixelFormat> &supportedPixelFormats = {}
    ) const;

    /**
     * 获取底层 AVFrame 指针（用于兼容现有代码）
     */
    AVFrame* avFrame() const { return m_frame.get(); }
    
    /**
     * 转换为 AVFramePtr（用于兼容现有代码）
     */
    AVFramePtr toAVFramePtr() const;

private:
    /**
     * 内部数据复制方法
     */
    bool copyDataInternal(void *dest[4], int linesize[4]) const;
    
    /**
     * 从其他 AVFrame 复制信息
     */
    void copyAVFrameInfo(const AVFrame *other);
    
    /**
     * 获取像素格式描述符
     */
    void obtainPixelFormat(bool checkForYUVJ = true);

private:
    AVFramePtr m_frame;              // AVFrame 指针（统一管理）
    AVRational m_timeBase;          // 时间基准
    
    std::shared_ptr<OnDestroyFn> m_onDestroyFn;  // 销毁回调
    
    // Video 相关属性
    AVPixelFormat m_pixelFormat;     // 像素格式（缓存）
    const AVPixFmtDescriptor *m_pixelFmtDescriptor;  // 像素格式描述符
    bool m_isSecondField;            // 是否是第二场
    bool m_hasBorders;               // 是否有边框
};

/* Inline 实现 */

template<typename D, typename L>
bool Frame::copyData(D *dest[4], L linesize[4]) const
{
    static_assert(sizeof(L) == sizeof(int), "Linesize type size mismatch");
    return copyDataInternal(reinterpret_cast<void **>(dest), reinterpret_cast<int *>(linesize));
}
