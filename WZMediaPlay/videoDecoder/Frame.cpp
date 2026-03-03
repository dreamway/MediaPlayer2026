#include "Frame.h"
#include "spdlog/spdlog.h"

extern spdlog::logger *logger;

#include <libavutil/hwcontext.h>
#include <libavutil/imgutils.h>

// 辅助函数：检查是否是硬件格式
static bool isHardwareFormat(AVPixelFormat fmt)
{
    return fmt == AV_PIX_FMT_D3D11 ||
           fmt == AV_PIX_FMT_DXVA2_VLD ||
           fmt == AV_PIX_FMT_VAAPI ||
           fmt == AV_PIX_FMT_CUDA ||
           fmt == AV_PIX_FMT_VIDEOTOOLBOX ||
           fmt == AV_PIX_FMT_QSV ||
           fmt == AV_PIX_FMT_OPENCL ||
           fmt == AV_PIX_FMT_VULKAN;
}

// Frame 实现

Frame::Frame()
    : m_frame(nullptr)
    , m_timeBase({1, 1000000})  // 默认时间基准：微秒
    , m_pixelFormat(AV_PIX_FMT_NONE)
    , m_pixelFmtDescriptor(nullptr)
    , m_isSecondField(false)
    , m_hasBorders(false)
{
}

Frame::Frame(AVFrame *avFrame, AVPixelFormat newPixelFormat)
    : m_frame(nullptr)
    , m_timeBase({1, 1000000})
    , m_pixelFormat(AV_PIX_FMT_NONE)
    , m_pixelFmtDescriptor(nullptr)
    , m_isSecondField(false)
    , m_hasBorders(false)
{
    if (avFrame) {
        m_frame = AVFramePtr(av_frame_clone(avFrame));
        if (m_frame) {
            copyAVFrameInfo(m_frame.get());
            if (newPixelFormat != AV_PIX_FMT_NONE) {
                // 如果需要转换格式，这里可以添加转换逻辑
                // 目前先保持原格式
            }
            obtainPixelFormat();
        }
    }
}

Frame::Frame(const Frame &other)
    : m_frame(nullptr)
    , m_timeBase(other.m_timeBase)
    , m_onDestroyFn(other.m_onDestroyFn)
    , m_pixelFormat(other.m_pixelFormat)
    , m_pixelFmtDescriptor(other.m_pixelFmtDescriptor)
    , m_isSecondField(other.m_isSecondField)
    , m_hasBorders(other.m_hasBorders)
{
    if (other.m_frame) {
        m_frame = AVFramePtr(av_frame_clone(other.m_frame.get()));
        if (m_frame) {
            copyAVFrameInfo(m_frame.get());
        }
    }
}

Frame::Frame(Frame &&other) noexcept
    : m_frame(std::move(other.m_frame))
    , m_timeBase(other.m_timeBase)
    , m_onDestroyFn(std::move(other.m_onDestroyFn))
    , m_pixelFormat(other.m_pixelFormat)
    , m_pixelFmtDescriptor(other.m_pixelFmtDescriptor)
    , m_isSecondField(other.m_isSecondField)
    , m_hasBorders(other.m_hasBorders)
{
    other.m_frame = nullptr;
    other.m_pixelFormat = AV_PIX_FMT_NONE;
    other.m_pixelFmtDescriptor = nullptr;
}

Frame::~Frame()
{
    clear();
}

Frame &Frame::operator=(const Frame &other)
{
    if (this != &other) {
        clear();
        m_timeBase = other.m_timeBase;
        m_onDestroyFn = other.m_onDestroyFn;
        m_pixelFormat = other.m_pixelFormat;
        m_pixelFmtDescriptor = other.m_pixelFmtDescriptor;
        m_isSecondField = other.m_isSecondField;
        m_hasBorders = other.m_hasBorders;
        
        if (other.m_frame) {
            m_frame = AVFramePtr(av_frame_clone(other.m_frame.get()));
            if (m_frame) {
                copyAVFrameInfo(m_frame.get());
            }
        }
    }
    return *this;
}

Frame &Frame::operator=(Frame &&other) noexcept
{
    if (this != &other) {
        clear();
        m_frame = std::move(other.m_frame);
        m_timeBase = other.m_timeBase;
        m_onDestroyFn = std::move(other.m_onDestroyFn);
        m_pixelFormat = other.m_pixelFormat;
        m_pixelFmtDescriptor = other.m_pixelFmtDescriptor;
        m_isSecondField = other.m_isSecondField;
        m_hasBorders = other.m_hasBorders;
        
        other.m_frame = nullptr;
        other.m_pixelFormat = AV_PIX_FMT_NONE;
        other.m_pixelFmtDescriptor = nullptr;
    }
    return *this;
}

bool Frame::isEmpty() const
{
    if (!m_frame) {
        return true;
    }
    
    // 检查格式是否有效
    if (m_frame->format == AV_PIX_FMT_NONE || m_frame->format == -1) {
        return true;
    }
    
    // 检查尺寸是否有效
    if (m_frame->width <= 0 || m_frame->height <= 0) {
        return true;
    }
    
    // 检查数据指针是否有效
    if (!m_frame->data[0]) {
        return true;
    }
    
    return false;
}

void Frame::clear()
{
    if (m_onDestroyFn && *m_onDestroyFn) {
        (*m_onDestroyFn)();
    }
    
    m_frame = nullptr;
    m_pixelFormat = AV_PIX_FMT_NONE;
    m_pixelFmtDescriptor = nullptr;
    m_isSecondField = false;
    m_hasBorders = false;
    m_onDestroyFn = nullptr;
}

void Frame::setTimeBase(const AVRational &timeBase)
{
    m_timeBase = timeBase;
}

AVRational Frame::timeBase() const
{
    return m_timeBase;
}

bool Frame::isTsValid() const
{
    if (!m_frame) {
        return false;
    }
    return m_frame->pts != AV_NOPTS_VALUE && m_frame->best_effort_timestamp != AV_NOPTS_VALUE;
}

nanoseconds Frame::ts() const
{
    if (!m_frame || !isTsValid()) {
        return kInvalidTimestamp;
    }
    
    // 使用 best_effort_timestamp 或 pts
    int64_t tsValue = m_frame->best_effort_timestamp != AV_NOPTS_VALUE 
        ? m_frame->best_effort_timestamp 
        : m_frame->pts;
    
    if (tsValue == AV_NOPTS_VALUE) {
        return kInvalidTimestamp;
    }
    
    // 转换为 nanoseconds
    double tsSeconds = av_q2d(m_timeBase) * tsValue;
    return nanoseconds(static_cast<int64_t>(tsSeconds * 1e9));
}

int64_t Frame::tsInt() const
{
    if (!m_frame || !isTsValid()) {
        return AV_NOPTS_VALUE;
    }
    return m_frame->best_effort_timestamp != AV_NOPTS_VALUE 
        ? m_frame->best_effort_timestamp 
        : m_frame->pts;
}

void Frame::setTS(nanoseconds ts)
{
    if (!m_frame) {
        return;
    }
    
    if (ts == kInvalidTimestamp) {
        m_frame->pts = AV_NOPTS_VALUE;
        m_frame->best_effort_timestamp = AV_NOPTS_VALUE;
        return;
    }
    
    // 转换为时间基准单位
    double tsSeconds = ts.count() / 1e9;
    int64_t tsValue = static_cast<int64_t>(tsSeconds / av_q2d(m_timeBase));
    
    m_frame->pts = tsValue;
    m_frame->best_effort_timestamp = tsValue;
}

void Frame::setTSInt(int64_t ts)
{
    if (!m_frame) {
        return;
    }
    m_frame->pts = ts;
    m_frame->best_effort_timestamp = ts;
}

bool Frame::isInterlaced() const
{
    if (!m_frame) {
        return false;
    }
    // 使用新的 flags API 替代废弃的 interlaced_frame
    return (m_frame->flags & AV_FRAME_FLAG_INTERLACED) != 0;
}

bool Frame::isTopFieldFirst() const
{
    if (!m_frame) {
        return false;
    }
    // 使用新的 flags API 替代废弃的 top_field_first
    return (m_frame->flags & AV_FRAME_FLAG_TOP_FIELD_FIRST) != 0;
}

bool Frame::isSecondField() const
{
    return m_isSecondField;
}

void Frame::setInterlaced(bool topFieldFirst)
{
    if (m_frame) {
        // 使用新的 flags API 替代废弃的字段
        m_frame->flags |= AV_FRAME_FLAG_INTERLACED;
        if (topFieldFirst) {
            m_frame->flags |= AV_FRAME_FLAG_TOP_FIELD_FIRST;
        } else {
            m_frame->flags &= ~AV_FRAME_FLAG_TOP_FIELD_FIRST;
        }
    }
}

void Frame::setNoInterlaced()
{
    if (m_frame) {
        // 清除隔行扫描标志
        m_frame->flags &= ~AV_FRAME_FLAG_INTERLACED;
        m_frame->flags &= ~AV_FRAME_FLAG_TOP_FIELD_FIRST;
    }
}

void Frame::setIsSecondField(bool secondField)
{
    m_isSecondField = secondField;
}

bool Frame::hasCPUAccess() const
{
    if (!m_frame) {
        return false;
    }
    // 软件帧有CPU访问权限
    return !isHardwareFormat(static_cast<AVPixelFormat>(m_frame->format));
}

bool Frame::isHW() const
{
    if (!m_frame) {
        return false;
    }
    return isHardwareFormat(static_cast<AVPixelFormat>(m_frame->format));
}

AVBufferRef* Frame::hwFrameBufferRef() const
{
    if (!m_frame || !isHW()) {
        return nullptr;
    }
    return m_frame->hw_frames_ctx;
}

AVPixelFormat Frame::pixelFormat() const
{
    if (m_pixelFormat != AV_PIX_FMT_NONE) {
        return m_pixelFormat;
    }
    if (m_frame) {
        return static_cast<AVPixelFormat>(m_frame->format);
    }
    return AV_PIX_FMT_NONE;
}

AVColorPrimaries Frame::colorPrimaries() const
{
    if (!m_frame) {
        return AVCOL_PRI_UNSPECIFIED;
    }
    return m_frame->color_primaries;
}

AVColorTransferCharacteristic Frame::colorTrc() const
{
    if (!m_frame) {
        return AVCOL_TRC_UNSPECIFIED;
    }
    return m_frame->color_trc;
}

AVColorSpace Frame::colorSpace() const
{
    if (!m_frame) {
        return AVCOL_SPC_UNSPECIFIED;
    }
    return m_frame->colorspace;
}

bool Frame::isLimited() const
{
    if (!m_frame) {
        return true;
    }
    // YUVJ格式是全范围，其他格式默认是有限范围
    return m_frame->color_range != AVCOL_RANGE_JPEG;
}

const AVMasteringDisplayMetadata *Frame::masteringDisplayMetadata() const
{
    if (!m_frame) {
        return nullptr;
    }
    AVFrameSideData *sideData = av_frame_get_side_data(m_frame.get(), AV_FRAME_DATA_MASTERING_DISPLAY_METADATA);
    if (sideData) {
        return reinterpret_cast<const AVMasteringDisplayMetadata *>(sideData->data);
    }
    return nullptr;
}

bool Frame::isGray() const
{
    if (!m_pixelFmtDescriptor) {
        return false;
    }
    return !(m_pixelFmtDescriptor->flags & AV_PIX_FMT_FLAG_RGB) &&
           !(m_pixelFmtDescriptor->nb_components > 1);
}

bool Frame::isPlanar() const
{
    if (!m_pixelFmtDescriptor) {
        return false;
    }
    return m_pixelFmtDescriptor->flags & AV_PIX_FMT_FLAG_PLANAR;
}

bool Frame::isRGB() const
{
    if (!m_pixelFmtDescriptor) {
        return false;
    }
    return m_pixelFmtDescriptor->flags & AV_PIX_FMT_FLAG_RGB;
}

int Frame::chromaShiftW() const
{
    if (!m_pixelFmtDescriptor) {
        return 0;
    }
    return m_pixelFmtDescriptor->log2_chroma_w;
}

int Frame::chromaShiftH() const
{
    if (!m_pixelFmtDescriptor) {
        return 0;
    }
    return m_pixelFmtDescriptor->log2_chroma_h;
}

int Frame::numPlanes() const
{
    if (!m_pixelFmtDescriptor) {
        return 0;
    }
    return m_pixelFmtDescriptor->nb_components;
}

int Frame::depth() const
{
    if (!m_pixelFmtDescriptor) {
        return 8;
    }
    return m_pixelFmtDescriptor->comp[0].depth;
}

int Frame::paddingBits() const
{
    if (!m_pixelFmtDescriptor) {
        return 0;
    }
    return m_pixelFmtDescriptor->comp[0].shift;
}

int *Frame::linesize() const
{
    if (!m_frame) {
        return nullptr;
    }
    return m_frame->linesize;
}

int Frame::linesize(int plane) const
{
    if (!m_frame || plane < 0 || plane >= AV_NUM_DATA_POINTERS) {
        return 0;
    }
    return m_frame->linesize[plane];
}

int Frame::width(int plane) const
{
    if (!m_frame) {
        return 0;
    }
    if (plane == 0) {
        return m_frame->width;
    }
    // 对于其他平面，根据chroma shift计算
    int shiftW = chromaShiftW();
    return (m_frame->width + (1 << shiftW) - 1) >> shiftW;
}

int Frame::height(int plane) const
{
    if (!m_frame) {
        return 0;
    }
    if (plane == 0) {
        return m_frame->height;
    }
    // 对于其他平面，根据chroma shift计算
    int shiftH = chromaShiftH();
    return (m_frame->height + (1 << shiftH) - 1) >> shiftH;
}

AVRational Frame::sampleAspectRatio() const
{
    if (!m_frame) {
        return {1, 1};
    }
    return m_frame->sample_aspect_ratio;
}

const uint8_t *Frame::constData(int plane) const
{
    if (!m_frame || plane < 0 || plane >= AV_NUM_DATA_POINTERS) {
        return nullptr;
    }
    return m_frame->data[plane];
}

uint8_t *Frame::data(int plane)
{
    if (!m_frame || plane < 0 || plane >= AV_NUM_DATA_POINTERS) {
        return nullptr;
    }
    return m_frame->data[plane];
}

uint8_t **Frame::dataArr()
{
    if (!m_frame) {
        return nullptr;
    }
    return m_frame->data;
}

bool Frame::setVideoData(AVBufferRef *buffer[], const int *linesize, uint8_t *data[], bool ref)
{
    if (!m_frame) {
        return false;
    }
    
    if (ref && buffer) {
        // 引用模式：使用提供的缓冲区
        for (int i = 0; i < AV_NUM_DATA_POINTERS; i++) {
            if (buffer[i]) {
                m_frame->buf[i] = av_buffer_ref(buffer[i]);
            }
            if (linesize) {
                m_frame->linesize[i] = linesize[i];
            }
            if (data) {
                m_frame->data[i] = data[i];
            }
        }
    } else {
        // 复制模式：复制数据
        // 这里需要分配缓冲区并复制数据
        // 简化实现：暂时不支持
        return false;
    }
    
    return true;
}

void Frame::setOnDestroyFn(const OnDestroyFn &onDestroyFn)
{
    m_onDestroyFn = std::make_shared<OnDestroyFn>(onDestroyFn);
}

bool Frame::copyDataInternal(void *dest[4], int linesize[4]) const
{
    if (isEmpty()) {
        return false;
    }
    
    int numPlanes = this->numPlanes();
    for (int i = 0; i < numPlanes && i < 4; i++) {
        int planeHeight = height(i);
        int planeWidth = width(i);
        int srcLinesize = this->linesize(i);
        int dstLinesize = linesize[i];
        
        const uint8_t *src = constData(i);
        uint8_t *dst = static_cast<uint8_t *>(dest[i]);
        
        if (!src || !dst) {
            continue;
        }
        
        int copyLinesize = std::min(srcLinesize, dstLinesize);
        for (int y = 0; y < planeHeight; y++) {
            memcpy(dst + y * dstLinesize, src + y * srcLinesize, copyLinesize);
        }
    }
    
    return true;
}

void Frame::copyAVFrameInfo(const AVFrame *other)
{
    if (!other || !m_frame) {
        return;
    }
    
    // 复制基本属性
    m_frame->width = other->width;
    m_frame->height = other->height;
    m_frame->format = other->format;
    m_frame->pts = other->pts;
    m_frame->best_effort_timestamp = other->best_effort_timestamp;
    m_frame->pkt_dts = other->pkt_dts;
    // pkt_duration 已废弃，使用 duration 字段
    m_frame->duration = other->duration;
    m_frame->repeat_pict = other->repeat_pict;
    // 使用新的 flags API 替代废弃的字段
    if (other->flags & AV_FRAME_FLAG_INTERLACED) {
        m_frame->flags |= AV_FRAME_FLAG_INTERLACED;
    } else {
        m_frame->flags &= ~AV_FRAME_FLAG_INTERLACED;
    }
    if (other->flags & AV_FRAME_FLAG_TOP_FIELD_FIRST) {
        m_frame->flags |= AV_FRAME_FLAG_TOP_FIELD_FIRST;
    } else {
        m_frame->flags &= ~AV_FRAME_FLAG_TOP_FIELD_FIRST;
    }
    m_frame->sample_aspect_ratio = other->sample_aspect_ratio;
    m_frame->color_primaries = other->color_primaries;
    m_frame->color_trc = other->color_trc;
    m_frame->colorspace = other->colorspace;
    m_frame->color_range = other->color_range;
}

void Frame::obtainPixelFormat(bool checkForYUVJ)
{
    if (!m_frame) {
        m_pixelFormat = AV_PIX_FMT_NONE;
        m_pixelFmtDescriptor = nullptr;
        return;
    }
    
    m_pixelFormat = static_cast<AVPixelFormat>(m_frame->format);
    
    // 检查是否是YUVJ格式（全范围）
    if (checkForYUVJ && m_pixelFormat != AV_PIX_FMT_NONE) {
        const AVPixFmtDescriptor *desc = av_pix_fmt_desc_get(m_pixelFormat);
        if (desc && desc->name) {
            bool isYUV = (desc->name[0] == 'y' || desc->name[0] == 'Y') &&
                         (desc->name[1] == 'u' || desc->name[1] == 'U') &&
                         (desc->name[2] == 'v' || desc->name[2] == 'V');
            
            if (isYUV && m_frame->color_range == AVCOL_RANGE_JPEG) {
                // 转换为YUVJ格式
                if (m_pixelFormat == AV_PIX_FMT_YUV420P) {
                    m_pixelFormat = AV_PIX_FMT_YUVJ420P;
                } else if (m_pixelFormat == AV_PIX_FMT_YUV422P) {
                    m_pixelFormat = AV_PIX_FMT_YUVJ422P;
                } else if (m_pixelFormat == AV_PIX_FMT_YUV444P) {
                    m_pixelFormat = AV_PIX_FMT_YUVJ444P;
                }
            }
        }
    }
    
    m_pixelFmtDescriptor = av_pix_fmt_desc_get(m_pixelFormat);
}

Frame Frame::downloadHwData(SwsContext **swsCtx, const std::vector<AVPixelFormat> &supportedPixelFormats) const
{
    // 如果不是硬件帧，直接返回自身
    if (!isHW() || isEmpty()) {
        return *this;
    }
    
    if (!m_frame || !m_frame->hw_frames_ctx) {
        logger->warn("Frame::downloadHwData() called but hw_frames_ctx is null");
        return Frame();
    }
    
    // 创建软件帧
    Frame swFrame;
    swFrame.m_frame = AVFramePtr(av_frame_alloc());
    if (!swFrame.m_frame) {
        logger->error("Frame::downloadHwData() failed to allocate software frame");
        return Frame();
    }
    
    // 使用 av_hwframe_transfer_data 下载硬件帧
    int ret = av_hwframe_transfer_data(swFrame.m_frame.get(), m_frame.get(), 0);
    if (ret < 0) {
        char errbuf[256] = {0};
        av_strerror(ret, errbuf, sizeof(errbuf));
        logger->error("Frame::downloadHwData() av_hwframe_transfer_data failed: {} ({})", errbuf, ret);
        return Frame();
    }
    
    // 复制帧信息
    swFrame.copyAVFrameInfo(m_frame.get());
    swFrame.obtainPixelFormat();
    
    // 如果下载后的格式不在支持的格式列表中，需要转换
    AVPixelFormat downloadedFormat = swFrame.pixelFormat();
    AVPixelFormat targetFormat = AV_PIX_FMT_NONE;
    
    if (!supportedPixelFormats.empty()) {
        // 查找第一个支持的格式
        for (AVPixelFormat fmt : supportedPixelFormats) {
            if (fmt == downloadedFormat) {
                // 已经是支持的格式，直接返回
                return swFrame;
            }
            if (targetFormat == AV_PIX_FMT_NONE) {
                targetFormat = fmt;
            }
        }
        
        // 需要转换格式
        if (targetFormat != AV_PIX_FMT_NONE && targetFormat != downloadedFormat) {
            logger->debug("Frame::downloadHwData() converting from {} to {}", 
                         av_get_pix_fmt_name(downloadedFormat),
                         av_get_pix_fmt_name(targetFormat));
            
            // 创建转换后的帧
            Frame convertedFrame = createEmpty(swFrame.m_frame.get(), true, targetFormat);
            if (convertedFrame.isEmpty()) {
                logger->error("Frame::downloadHwData() failed to create converted frame");
                return swFrame;  // 返回未转换的帧
            }
            
            // 使用 SwsContext 进行格式转换
            SwsContext *ctx = *swsCtx;
            if (!ctx || 
                swFrame.width() != convertedFrame.width() ||
                swFrame.height() != convertedFrame.height() ||
                downloadedFormat != convertedFrame.pixelFormat()) {
                if (ctx) {
                    sws_freeContext(ctx);
                }
                ctx = sws_getContext(
                    swFrame.width(), swFrame.height(), downloadedFormat,
                    convertedFrame.width(), convertedFrame.height(), targetFormat,
                    SWS_BILINEAR, nullptr, nullptr, nullptr);
                if (!ctx) {
                    logger->error("Frame::downloadHwData() failed to create SwsContext");
                    return swFrame;
                }
                *swsCtx = ctx;
            }
            
            // 执行转换
            int swsRet = sws_scale(ctx,
                                   swFrame.dataArr(), swFrame.linesize(), 0, swFrame.height(),
                                   convertedFrame.dataArr(), convertedFrame.linesize());
            
            if (swsRet != swFrame.height()) {
                logger->error("Frame::downloadHwData() sws_scale failed: {} (expected {})", 
                             swsRet, swFrame.height());
                return swFrame;
            }
            
            return convertedFrame;
        }
    }
    
    return swFrame;
}

AVFramePtr Frame::toAVFramePtr() const
{
    if (!m_frame) {
        return nullptr;
    }
    // 克隆帧，返回新的 AVFramePtr
    AVFrame *cloned = av_frame_clone(m_frame.get());
    return AVFramePtr(cloned);
}

// 静态工厂方法

Frame Frame::createEmpty(const Frame &other, bool allocBuffers, AVPixelFormat newPixelFormat)
{
    Frame frame;
    if (other.m_frame) {
        frame.m_frame = AVFramePtr(av_frame_alloc());
        if (frame.m_frame) {
            frame.copyAVFrameInfo(other.m_frame.get());
            if (newPixelFormat != AV_PIX_FMT_NONE) {
                frame.m_frame->format = newPixelFormat;
            }
            if (allocBuffers) {
                av_frame_get_buffer(frame.m_frame.get(), 0);
            }
            frame.obtainPixelFormat();
        }
    }
    return frame;
}

Frame Frame::createEmpty(const AVFrame *other, bool allocBuffers, AVPixelFormat newPixelFormat)
{
    Frame frame;
    if (other) {
        frame.m_frame = AVFramePtr(av_frame_alloc());
        if (frame.m_frame) {
            frame.copyAVFrameInfo(other);
            if (newPixelFormat != AV_PIX_FMT_NONE) {
                frame.m_frame->format = newPixelFormat;
            }
            if (allocBuffers) {
                av_frame_get_buffer(frame.m_frame.get(), 0);
            }
            frame.obtainPixelFormat();
        }
    }
    return frame;
}

Frame Frame::createEmpty(int width, int height, AVPixelFormat pixelFormat, bool interlaced, bool topFieldFirst, AVColorSpace colorSpace, bool isLimited, AVColorPrimaries colorPrimaries, AVColorTransferCharacteristic colorTrc)
{
    Frame frame;
    frame.m_frame = AVFramePtr(av_frame_alloc());
    if (frame.m_frame) {
        frame.m_frame->width = width;
        frame.m_frame->height = height;
        frame.m_frame->format = pixelFormat;
        // 使用新的 flags API 替代废弃的字段
        if (interlaced) {
            frame.m_frame->flags |= AV_FRAME_FLAG_INTERLACED;
            if (topFieldFirst) {
                frame.m_frame->flags |= AV_FRAME_FLAG_TOP_FIELD_FIRST;
            } else {
                frame.m_frame->flags &= ~AV_FRAME_FLAG_TOP_FIELD_FIRST;
            }
        } else {
            frame.m_frame->flags &= ~AV_FRAME_FLAG_INTERLACED;
            frame.m_frame->flags &= ~AV_FRAME_FLAG_TOP_FIELD_FIRST;
        }
        frame.m_frame->colorspace = colorSpace;
        frame.m_frame->color_range = isLimited ? AVCOL_RANGE_MPEG : AVCOL_RANGE_JPEG;
        frame.m_frame->color_primaries = colorPrimaries;
        frame.m_frame->color_trc = colorTrc;
        av_frame_get_buffer(frame.m_frame.get(), 0);
        frame.obtainPixelFormat();
    }
    return frame;
}
