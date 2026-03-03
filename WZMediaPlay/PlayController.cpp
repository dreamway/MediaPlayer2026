#include "PlayController.h"
#include "GlobalDef.h"
#include "PlaybackStateMachine.h"
#include "SystemMonitor.h"
#include "videoDecoder/AudioThread.h"
#include "videoDecoder/Decoder.h"
#include "videoDecoder/DemuxerThread.h"
#include "videoDecoder/FFDecHW.h"
#include "videoDecoder/FFDecSW.h"
#include "videoDecoder/OpenALAudio.h"
#include "videoDecoder/Statistics.h"
#include "videoDecoder/VideoRenderer.h"
#include "videoDecoder/VideoThread.h"
#include "videoDecoder/VideoWidgetBase.h"
#include "videoDecoder/chronons.h"
#include "videoDecoder/ffmpeg.h"
#include "videoDecoder/opengl/OpenGLRenderer.h"
#include "videoDecoder/opengl/StereoOpenGLRenderer.h"
#include <atomic>
#include <chrono>
#include <cstring>
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <thread>

// 定义AV_ERROR_MAX_STRING_SIZE（如果未定义）
#ifndef AV_ERROR_MAX_STRING_SIZE
#define AV_ERROR_MAX_STRING_SIZE 64
#endif

extern spdlog::logger *logger;

static double fraction_3Double(AVRational r)
{
    return r.den == 0 ? 0 : (double) r.num / r.den;
}

PlayController::PlayController(QObject *parent)
    : QObject(parent)
    , audio_(nullptr)
    , demuxer_(nullptr)
    , enableHardwareDecoding_(false) // [临时禁用硬件解码] TODO：修复硬件解码问题后启用（参考 docs/TODO_CURRENT.md）
    , videoDecoder_(nullptr)
    , videoStreamIndex_(-1)
    , audioStreamIndex_(-1)
    , subtitleStreamIndex_(-1)
    , masterClock_(std::make_unique<AVClock>(AVClock::AudioClock, this)) // 创建主时钟，默认使用音频时钟
{
    // 初始化状态机为Idle状态
    stateMachine_.transitionTo(PlaybackState::Idle, "PlayController constructed");

    // 初始化线程健康检查时间点
    auto now = std::chrono::steady_clock::now();
    lastVideoFrameTime_ = now;
    lastAudioFrameTime_ = now;
    lastDemuxTime_ = now;

    // 读取配置决定是否启用硬件解码
    QString cfgPath = QCoreApplication::applicationDirPath() + "/config/SystemConfig.ini";
    QSettings setting(cfgPath, QSettings::IniFormat);
    QVariant variant = setting.value("System/EnableHardwareDecoding"); // 修复：去掉前导斜杠

    // 添加详细的配置读取日志
    SPDLOG_LOGGER_INFO(logger, "PlayController: Reading config from: {}", cfgPath.toStdString());

    // 处理配置值：如果配置不存在或值为 "false"/false，则禁用硬件解码
    if (variant.isNull()) {
        enableHardwareDecoding_ = true; // 默认启用
        SPDLOG_LOGGER_INFO(logger, "PlayController: Config key not found, using default: true");
    } else {
        // 尝试多种方式读取布尔值
        QString strValue = variant.toString().toLower().trimmed();
        if (strValue == "false" || strValue == "0" || strValue.isEmpty()) {
            enableHardwareDecoding_ = false;
            SPDLOG_LOGGER_INFO(logger, "PlayController: Config value '{}' interpreted as false", strValue.toStdString());
        } else {
            enableHardwareDecoding_ = variant.toBool();
            SPDLOG_LOGGER_INFO(logger, "PlayController: Config value converted to bool: {}", enableHardwareDecoding_);
        }
    }

    SPDLOG_LOGGER_INFO(logger, "Hardware decoding enabled: {} (final value)", enableHardwareDecoding_);

    // 初始化 Audio 指针（Video 类已删除）
    audio_ = nullptr;

    SPDLOG_LOGGER_INFO(logger, "PlayController created");
}

PlayController::~PlayController()
{
    // 停止所有线程
    stop();

    // 清理 Decoder 和 Renderer
    videoDecoder_.reset();
    videoRenderer_.reset();

    // 清理 Audio（Video 类已删除，功能已迁移到 PlayController）
    if (audio_) {
        delete audio_;
        audio_ = nullptr;
    }

    // 清理视频流信息
    videoStream_ = nullptr;
    videoCodecCtx_.reset();

    SPDLOG_LOGGER_INFO(logger, "PlayController destroyed");
}

bool PlayController::open(const QString &filename)
{
    SPDLOG_LOGGER_INFO(logger, "PlayController::open: {}", filename.toStdString());

    // 如果已经打开，先停止
    if (isOpened()) {
        stop();
    }

    // 清除 seeking 状态（确保新打开文件时状态正确）
    // 统一使用状态机管理状态
    if (stateMachine_.isSeeking()) {
        stateMachine_.transitionTo(PlaybackState::Idle, "Opening new file, clearing seeking state");
    }

    // 创建或重置 Demuxer
    if (!demuxer_) {
        demuxer_ = std::make_unique<Demuxer>();
    } else {
        demuxer_->close();
    }

    // 转换到Opening状态
    stateMachine_.transitionTo(PlaybackState::Opening, "Opening file: " + filename.toStdString());

    // 使用 Demuxer 打开文件
    if (!demuxer_->open(filename)) {
        SPDLOG_LOGGER_ERROR(logger, "PlayController::open: Demuxer::open failed");
        stateMachine_.transitionTo(PlaybackState::Error, "Failed to open file");
        return false;
    }

    // 创建 Audio（Video 类已删除，功能已迁移到 PlayController）
    if (!audio_) {
        audio_ = new Audio(*this);
        SPDLOG_LOGGER_INFO(logger, "PlayController::open: Audio created");
    }

    // 重置视频流信息
    videoStream_ = nullptr;
    videoCodecCtx_.reset();

    // 清理解码器状态（在初始化解码器之前，确保旧解码器状态被清理）
    if (videoDecoder_) {
        videoDecoder_->clearFrames();
        SPDLOG_LOGGER_DEBUG(logger, "PlayController::open: Video decoder frames cleared before reinitialization");
    }

    // 获取流索引（在创建线程和初始化解码器之前）
    int videoStreamIndex = demuxer_->getVideoStreamIndex();
    int audioStreamIndex = demuxer_->getAudioStreamIndex();

    // 创建 AudioThread（在 initializeCodecs 之前，因为 streamComponentOpen 需要 audioThread_）
    if (audioStreamIndex >= 0) {
        // 先清理旧的 AudioThread（如果存在）
        if (audioThread_) {
            SPDLOG_LOGGER_INFO(logger, "PlayController::open: Stopping existing AudioThread (before initializeCodecs)");
            disconnectThreadSignals(audioThread_.get());
            stopThread(audioThread_.get(), "AudioThread", 5000);
            audioThread_.reset();
            SPDLOG_LOGGER_DEBUG(logger, "PlayController::open: AudioThread cleaned up successfully");
        }

        audioThread_ = std::make_shared<AudioThread>(this, audio_, this);

        // 断开finished()信号连接，手动管理线程生命周期
        // 虽然新线程刚创建，但为了安全，还是断开连接
        disconnectThreadSignals(audioThread_.get());

        // AudioThread不使用Decoder类，直接使用AVCodecContext进行解码
        // 明确设置为nullptr，避免调试代码误报
        audioThread_->setDec(nullptr);

        // 设置 Statistics 到 AudioThread
        if (statistics_) {
            audioThread_->setStatistics(statistics_.get());
            SPDLOG_LOGGER_INFO(logger, "PlayController::open: Statistics set to AudioThread");
        }

        SPDLOG_LOGGER_INFO(logger, "PlayController::open: AudioThread created (before initializeCodecs)");
    }

    // 注意：VideoThread 的创建移到 initializeCodecs() 之后，因为 videoDecoder_ 是在 initializeCodecs() 中创建的
    // 但 AudioThread 可以在 initializeCodecs() 之前创建，因为 AudioThread 不依赖 videoDecoder_

    // 注意：此时 audioThread_ 已经创建，streamComponentOpen 可以正确设置音频 codec context
    if (!initializeCodecs()) {
        SPDLOG_LOGGER_ERROR(logger, "PlayController::open: initializeCodecs failed");
        stateMachine_.transitionTo(PlaybackState::Error, "Failed to initialize codecs");
        if (audioThread_) {
            disconnectThreadSignals(audioThread_.get());
            stopThread(audioThread_.get(), "AudioThread", 5000);
            audioThread_.reset();
            SPDLOG_LOGGER_DEBUG(logger, "PlayController::open: AudioThread cleaned up after initializeCodecs failure");
        }
        return false;
    }

    // 创建 VideoThread（在 initializeCodecs 之后，因为 videoDecoder_ 是在 initializeCodecs() 中创建的）
    if (videoStreamIndex >= 0) {
        // 先清理旧的 VideoThread（如果存在）
        // 使用统一的停止顺序，避免死锁
        if (videoThread_) {
            SPDLOG_LOGGER_INFO(logger, "PlayController::open: Stopping existing VideoThread (after initializeCodecs)");
            disconnectThreadSignals(videoThread_.get());
            stopThread(videoThread_.get(), "VideoThread", 5000);
            videoThread_.reset();
            SPDLOG_LOGGER_DEBUG(logger, "PlayController::open: VideoThread cleaned up successfully");
        }

        // 检查解码器和写入器是否已创建（此时应该已经创建了）
        if (!videoDecoder_) {
            SPDLOG_LOGGER_ERROR(logger, "PlayController::open: videoDecoder_ is null after initializeCodecs, cannot create VideoThread");
            stateMachine_.transitionTo(PlaybackState::Error, "videoDecoder_ is null");
            if (audioThread_) {
                disconnectThreadSignals(audioThread_.get());
                stopThread(audioThread_.get(), "AudioThread", 5000);
                audioThread_.reset();
                SPDLOG_LOGGER_DEBUG(logger, "PlayController::open: AudioThread cleaned up after videoDecoder_ null");
            }
            return false;
        }
        if (!videoRenderer_) {
            SPDLOG_LOGGER_ERROR(logger, "PlayController::open: videoRenderer_ is null, cannot create VideoThread");
            stateMachine_.transitionTo(PlaybackState::Error, "videoRenderer_ is null");
            if (audioThread_) {
                disconnectThreadSignals(audioThread_.get());
                stopThread(audioThread_.get(), "AudioThread", 5000);
                audioThread_.reset();
                SPDLOG_LOGGER_DEBUG(logger, "PlayController::open: AudioThread cleaned up after videoRenderer_ null");
            }
            return false;
        }

        videoThread_ = std::make_shared<VideoThread>(this, &vPackets_, this);

        // 断开finished()信号连接，手动管理线程生命周期
        // 虽然新线程刚创建，但为了安全，还是断开连接
        disconnectThreadSignals(videoThread_.get());

        // 设置 Decoder 和 VideoRenderer 到 VideoThread
        videoThread_->setDec(videoDecoder_.get());
        SPDLOG_LOGGER_INFO(
            logger, "PlayController::open: Decoder set to VideoThread, decoder type: {}, decoder ptr: {}", videoDecoder_->name(), (void *) videoDecoder_.get());

        videoThread_->setVideoRenderer(videoRenderer_.get());
        SPDLOG_LOGGER_INFO(logger, "PlayController::open: VideoRenderer set to VideoThread");

        // 设置 Statistics 到 VideoThread
        if (statistics_) {
            videoThread_->setStatistics(statistics_.get());
            SPDLOG_LOGGER_INFO(logger, "PlayController::open: Statistics set to VideoThread");
        }

        // 设置视频帧率（用于动态调整渲染间隔）
        if (demuxer_) {
            double videoFps = demuxer_->getVideoFrameRate();
            if (videoFps > 0) {
                videoThread_->setVideoFrameRate(videoFps);
                SPDLOG_LOGGER_INFO(logger, "PlayController::open: Set video frame rate to VideoThread: {:.1f} FPS", videoFps);
            } else {
                SPDLOG_LOGGER_WARN(logger, "PlayController::open: Invalid video frame rate from demuxer, using default");
            }
        }

        // 连接 VideoThread 的信号
        connect(videoThread_.get(), &VideoThread::playbackCompleted, this, &PlayController::onPlaybackCompleted);

        SPDLOG_LOGGER_INFO(logger, "PlayController::open: VideoThread created (after initializeCodecs)");
    }

    // 注意：硬件解码器现在由 FFDecHW 管理，不再需要传递给 Video
    // Video 不再需要直接访问硬件解码器，解码由 VideoThread 通过 PlayController 的 videoDecoder_ 完成

    // 创建 DemuxerThread（先停止并清理旧的线程）
    if (demuxThread_) {
        SPDLOG_LOGGER_INFO(logger, "PlayController::open: Stopping existing DemuxerThread");
        stopThread(demuxThread_.get(), "DemuxerThread", 5000);
        demuxThread_.reset();
    }

    demuxThread_ = std::make_shared<DemuxerThread>(this, this);

    // 断开finished()信号连接，手动管理线程生命周期
    disconnectThreadSignals(demuxThread_.get());

    // 设置 Demuxer
    demuxThread_->setDemuxer(demuxer_.get());

    // 设置流索引
    demuxThread_->setStreamIndices(demuxer_->getVideoStreamIndex(), demuxer_->getAudioStreamIndex(), demuxer_->getSubtitleStreamIndex());

    // 设置 PacketQueue（使用 PlayController 直接管理的 PacketQueue）
    demuxThread_->setVideoPacketQueue(&vPackets_);
    demuxThread_->setAudioPacketQueue(&aPackets_);

    // 注意：Audio 类不再需要 PacketQueue，由 AudioThread 直接管理

    // 连接 DemuxerThread 的信号
    connect(demuxThread_.get(), &DemuxerThread::seekFinished, this, &PlayController::onDemuxerThreadSeekFinished, Qt::UniqueConnection);
    connect(demuxThread_.get(), &DemuxerThread::eofReached, this, &PlayController::onDemuxerThreadEofReached, Qt::UniqueConnection);
    connect(demuxThread_.get(), &DemuxerThread::errorOccurred, this, &PlayController::onDemuxerThreadErrorOccurred, Qt::UniqueConnection);

    // 注意：AudioThread 在 initializeCodecs 之前创建（因为 streamComponentOpen 需要 audioThread_）
    // VideoThread 在 initializeCodecs 之后创建（因为 videoDecoder_ 是在 initializeCodecs 中创建的）
    // codecctx_ 是在 initializeCodecs 中设置的，所以线程启动应该在 initializeCodecs 之后

    // 启动所有线程（在 initializeCodecs 之后，确保 codecctx_ 已经设置）
    demuxThread_->start();
    if (videoThread_) {
        videoThread_->start();
        SPDLOG_LOGGER_INFO(logger, "PlayController::open: VideoThread started");
    }
    if (audioThread_) {
        // 检查线程是否已经在运行
        if (audioThread_->isRunning()) {
            SPDLOG_LOGGER_WARN(logger, "PlayController::open: AudioThread is already running, skipping start");
        } else {
            try {
                audioThread_->start();
                SPDLOG_LOGGER_INFO(logger, "PlayController::open: AudioThread started");
            } catch (const std::system_error &e) {
                SPDLOG_LOGGER_ERROR(logger, "PlayController::open: Failed to start AudioThread (system_error): {} (code: {})", e.what(), e.code().value());
                stateMachine_.transitionTo(PlaybackState::Error, "Failed to start AudioThread: " + std::string(e.what()));
                return false;
            } catch (const std::exception &e) {
                SPDLOG_LOGGER_ERROR(logger, "PlayController::open: Failed to start AudioThread (exception): {}", e.what());
                stateMachine_.transitionTo(PlaybackState::Error, "Failed to start AudioThread: " + std::string(e.what()));
                return false;
            } catch (...) {
                SPDLOG_LOGGER_ERROR(logger, "PlayController::open: Failed to start AudioThread: Unknown exception");
                stateMachine_.transitionTo(PlaybackState::Error, "Failed to start AudioThread: Unknown exception");
                return false;
            }
        }
    }

    // 初始化线程健康检查时间戳（启动线程后）
    auto now = std::chrono::steady_clock::now();
    lastVideoFrameTime_ = now;
    lastAudioFrameTime_ = now;
    lastDemuxTime_ = now;

    // 转换到Ready状态（文件已打开，准备播放）
    stateMachine_.transitionTo(PlaybackState::Ready, "File opened successfully");

    // 转换到Playing状态
    stateMachine_.transitionTo(PlaybackState::Playing, "Starting playback");

    // 同步时钟基准：确保音频和视频时钟从同一时间开始
    // 这有助于减少播放开始时的同步误差
    lastVideoFrameTime_ = std::chrono::steady_clock::now();
    lastAudioFrameTime_ = std::chrono::steady_clock::now();
    lastDemuxTime_ = std::chrono::steady_clock::now();

    // 启动系统监控（仅在 LOG_LEVEL <= DEBUG 时启用，避免性能影响）
    if (GlobalDef::getInstance()->LOG_LEVEL <= 1) {
        if (!systemMonitor_) {
            systemMonitor_ = std::make_unique<SystemMonitor>();
        }
        systemMonitor_->start(this);
    }

    // 注意：音频播放将在 AudioThread::initializeDecoder() 中启动
    // 因为 audio_->open() 需要在 AudioThread 中调用，此时 source_ 才会被创建

    SPDLOG_LOGGER_INFO(logger, "PlayController::open succeeded");
    return true;
}

bool PlayController::initializeCodecs()
{
    SPDLOG_LOGGER_INFO(logger, "PlayController::initializeCodecs");

    if (!demuxer_ || !demuxer_->isOpened()) {
        SPDLOG_LOGGER_ERROR(logger, "PlayController::initializeCodecs: Demuxer not opened");
        return false;
    }

    AVFormatContext *fmtctx = demuxer_->getFormatContext();
    if (!fmtctx) {
        SPDLOG_LOGGER_ERROR(logger, "PlayController::initializeCodecs: Failed to get AVFormatContext");
        return false;
    }

    // 获取流索引
    videoStreamIndex_ = demuxer_->getVideoStreamIndex();
    audioStreamIndex_ = demuxer_->getAudioStreamIndex();
    subtitleStreamIndex_ = demuxer_->getSubtitleStreamIndex();

    // 打开解码器
    for (unsigned int i = 0; i < fmtctx->nb_streams; i++) {
        auto *codecpar = fmtctx->streams[i]->codecpar;
        switch (codecpar->codec_type) {
        case AVMEDIA_TYPE_VIDEO:
            if (videoStreamIndex_ >= 0) {
                videoStreamIndex_ = streamComponentOpen(i);
            }
            break;
        case AVMEDIA_TYPE_AUDIO:
            if (audioStreamIndex_ >= 0) {
                audioStreamIndex_ = streamComponentOpen(i);
            }
            break;
        case AVMEDIA_TYPE_SUBTITLE:
            if (subtitleStreamIndex_ >= 0) {
                subtitleStreamIndex_ = streamComponentOpen(i);
            }
            break;
        }
    }

    if (videoStreamIndex_ < 0 && audioStreamIndex_ < 0) {
        SPDLOG_LOGGER_ERROR(logger, "PlayController::initializeCodecs: Fail to open codecs");
        return false;
    }

    // 创建视频解码器（FFDecSW 或 FFDecHW）
    if (videoStreamIndex_ >= 0) {
        if (!videoCodecCtx_) {
            SPDLOG_LOGGER_ERROR(logger, "PlayController::initializeCodecs: videoCodecCtx_ is null (streamComponentOpen may have failed)");
            return false;
        }

        SPDLOG_LOGGER_INFO(logger, "PlayController::initializeCodecs: videoCodecCtx_ is valid, codec_id: {}", int(videoCodecCtx_->codec_id));

        // 根据 enableHardwareDecoding_ 标志决定创建硬件解码器还是软件解码器
        // FFDecHW::init() 内部会尝试硬件解码，失败则自动回退到软件解码
        if (enableHardwareDecoding_) {
            // 创建硬件解码器（内部会尝试硬件解码，失败则回退到软件解码）
            videoDecoder_ = std::make_unique<FFDecHW>();
            SPDLOG_LOGGER_INFO(logger, "Created FFDecHW decoder (will try hardware decoding)");
        } else {
            // 创建软件解码器
            videoDecoder_ = std::make_unique<FFDecSW>();
            SPDLOG_LOGGER_INFO(logger, "Created FFDecSW decoder");
        }

        // 初始化解码器
        AVRational stream_time_base = videoStream_ ? videoStream_->time_base : AVRational{1, 1000000};
        SPDLOG_LOGGER_INFO(
            logger, "PlayController::initializeCodecs: Initializing decoder with stream_time_base: {}/{}", stream_time_base.num, stream_time_base.den);
        SPDLOG_LOGGER_INFO(logger, "PlayController::initializeCodecs: videoCodecCtx_ ptr: {}", (void *) videoCodecCtx_.get());

        if (!videoDecoder_->init(videoCodecCtx_.get(), stream_time_base)) {
            SPDLOG_LOGGER_ERROR(logger, "PlayController::initializeCodecs: Failed to initialize video decoder");
            videoDecoder_.reset();
            return false;
        }
        SPDLOG_LOGGER_INFO(logger, "PlayController::initializeCodecs: Video decoder initialized successfully, decoder ptr: {}", (void *) videoDecoder_.get());
    } else {
        SPDLOG_LOGGER_WARN(logger, "PlayController::initializeCodecs: videoStreamIndex_ < 0, skipping video decoder initialization");
    }

    // 创建 VideoRenderer（OpenGLRenderer），如果外部没有设置的话
    if (videoStreamIndex_ >= 0 && !videoRenderer_) {
        videoRenderer_ = std::make_shared<OpenGLRenderer>();
        if (!videoRenderer_->open()) {
            SPDLOG_LOGGER_ERROR(logger, "Failed to open OpenGLRenderer");
            videoRenderer_.reset();
            return false;
        }
        SPDLOG_LOGGER_INFO(logger, "OpenGLRenderer opened successfully");
    } else if (videoStreamIndex_ >= 0 && videoRenderer_) {
        SPDLOG_LOGGER_INFO(logger, "PlayController::initializeCodecs: Using external VideoRenderer");
    }

    // 初始化时钟同步（使用 AVClock）
    // 根据是否有音频流选择时钟类型
    if (audioStreamIndex_ >= 0) {
        masterClock_->setClockType(AVClock::AudioClock);
        sync_ = SyncMaster::Audio;
    } else {
        masterClock_->setClockType(AVClock::VideoClock);
        sync_ = SyncMaster::Video;
    }

    // 注意：clockbase_ 已不再使用，保留变量定义用于兼容
    // clockbase_ 的功能已由 AVClock 替代

    SPDLOG_LOGGER_INFO(
        logger, "PlayController::initializeCodecs: AVClock initialized, type: {}", (masterClock_->clockType() == AVClock::AudioClock) ? "Audio" : "Video");

    SPDLOG_LOGGER_INFO(logger, "PlayController::initializeCodecs succeeded");
    return true;
}

QWidget *PlayController::getVideoWidget() const
{
    // 使用 VideoWidgetBase
    if (videoWidgetBase_) {
        return videoWidgetBase_;
    }

    // 从 VideoRenderer 获取 widget
    if (videoRenderer_) {
        return videoRenderer_->target();
    }

    return nullptr;
}

void PlayController::setVideoRenderer(std::shared_ptr<VideoRenderer> renderer)
{
    if (renderer) {
        videoRenderer_ = renderer;
        SPDLOG_LOGGER_INFO(logger, "PlayController::setVideoRenderer: VideoRenderer set to {}", renderer->name().toStdString());

        // 如果 VideoThread 存在，更新其渲染器
        if (videoThread_) {
            videoThread_->setVideoRenderer(videoRenderer_.get());
            SPDLOG_LOGGER_INFO(logger, "PlayController::setVideoRenderer: VideoThread renderer updated");
        }
    } else {
        videoRenderer_.reset();
        if (videoThread_) {
            videoThread_->setVideoRenderer(nullptr);
        }
        SPDLOG_LOGGER_INFO(logger, "PlayController::setVideoRenderer: VideoRenderer cleared");
    }
}

void PlayController::setStatistics(std::shared_ptr<videoDecoder::Statistics> stats)
{
    statistics_ = stats;
    SPDLOG_LOGGER_INFO(logger, "PlayController::setStatistics: Statistics object set");

    // 如果 VideoThread 存在，更新其统计信息对象
    if (videoThread_) {
        videoThread_->setStatistics(statistics_.get());
        SPDLOG_LOGGER_INFO(logger, "PlayController::setStatistics: VideoThread statistics updated");
    }

    // 如果 AudioThread 存在，更新其统计信息对象
    if (audioThread_) {
        audioThread_->setStatistics(statistics_.get());
        SPDLOG_LOGGER_INFO(logger, "PlayController::setStatistics: AudioThread statistics updated");
    }
}

void PlayController::setVideoWidgetBase(VideoWidgetBase *widget)
{
    videoWidgetBase_ = widget;

    if (widget) {
        SPDLOG_LOGGER_INFO(logger, "PlayController::setVideoWidgetBase: VideoWidgetBase set");

        // 如果 VideoWidgetBase 有渲染器，使用它
        if (widget->renderer()) {
            videoRenderer_ = widget->renderer();
            SPDLOG_LOGGER_INFO(logger, "PlayController::setVideoWidgetBase: Using renderer from VideoWidgetBase");
        }
    } else {
        SPDLOG_LOGGER_INFO(logger, "PlayController::setVideoWidgetBase: VideoWidgetBase cleared");
    }
}

int PlayController::streamComponentOpen(unsigned int stream_index)
{
    AVFormatContext *fmtctx = demuxer_->getFormatContext();
    if (!fmtctx) {
        SPDLOG_LOGGER_ERROR(logger, "PlayController::streamComponentOpen: Failed to get AVFormatContext");
        return -1;
    }

    AVCodecContext *avCodecContext = avcodec_alloc_context3(nullptr);
    if (!avCodecContext) {
        SPDLOG_LOGGER_ERROR(logger, "PlayController::streamComponentOpen: Fail to avcodec_alloc_context3");
        return -1;
    }

    int ret = avcodec_parameters_to_context(avCodecContext, fmtctx->streams[stream_index]->codecpar);
    if (ret < 0) {
        SPDLOG_LOGGER_ERROR(logger, "PlayController::streamComponentOpen: Fail to avcodec_parameters_to_context");
        avcodec_free_context(&avCodecContext);
        return -1;
    }

    avCodecContext->flags2 |= AV_CODEC_FLAG2_FAST; // 允许不符合规范的加速技巧。
    // 注意：硬件解码器可能不支持多线程，在使用硬件解码时会禁用
    avCodecContext->thread_count = 8; // 使用8线程解码（软件解码时使用）
    avCodecContext->pkt_timebase = fmtctx->streams[stream_index]->time_base;
    SPDLOG_LOGGER_INFO(logger, "PlayController::streamComponentOpen: avCodecContext->pkt_timebase:{}", fraction_3Double(avCodecContext->pkt_timebase));
    SPDLOG_LOGGER_INFO(logger, "PlayController::streamComponentOpen: codec_id:{}", int(avCodecContext->codec_id));

    const AVCodec *codec = nullptr;

    // 注意：硬件解码器的初始化现在由 FFDecHW::init() 负责
    // 这里只负责打开软件解码器，硬件解码的尝试会在 FFDecHW::init() 中进行
    // 如果硬件解码失败或未启用，使用软件解码
    if (!codec) {
        codec = avcodec_find_decoder(avCodecContext->codec_id);
        if (!codec) {
            SPDLOG_LOGGER_ERROR(logger, "PlayController::streamComponentOpen: Unsupported codec: {}", avcodec_get_name(avCodecContext->codec_id));
            avcodec_free_context(&avCodecContext);
            return -1;
        }
        SPDLOG_LOGGER_INFO(logger, "PlayController::streamComponentOpen: Using software decoder: {}", codec->name);
    }

    int open_ret = avcodec_open2(avCodecContext, codec, nullptr);
    if (open_ret < 0) {
        char errbuf[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(open_ret, errbuf, AV_ERROR_MAX_STRING_SIZE);
        SPDLOG_LOGGER_ERROR(logger, "PlayController::streamComponentOpen: Failed to open codec: {} (error: {})", codec->name, errbuf);
        avcodec_free_context(&avCodecContext);
        return -1;
    }

    // 将解码器分配给 Video 或 Audio（直接使用 PlayController 的 Video 和 Audio）
    switch (avCodecContext->codec_type) {
    case AVMEDIA_TYPE_AUDIO:
        audioStream_ = fmtctx->streams[stream_index];
        audioCodecCtx_ = std::move(AVCodecContextPtr(avCodecContext));
        // 将 codecctx_ 的所有权转移给 AudioThread
        if (audioThread_) {
            AVCodecContextPtr codecctx = std::move(audioCodecCtx_);
            int codec_id = codecctx ? codecctx->codec_id : AV_CODEC_ID_NONE;
            audioThread_->setCodecContext(std::move(codecctx), audioStream_);
            SPDLOG_LOGGER_INFO(logger, "PlayController::streamComponentOpen: Audio codec context assigned to AudioThread, codec_id: {}", int(codec_id));
        } else {
            SPDLOG_LOGGER_WARN(logger, "PlayController::streamComponentOpen: audioThread_ is null, cannot assign codec context");
        }
        break;
    case AVMEDIA_TYPE_VIDEO:
        videoStream_ = fmtctx->streams[stream_index];
        videoCodecCtx_ = std::move(AVCodecContextPtr(avCodecContext));
        SPDLOG_LOGGER_INFO(logger, "PlayController::streamComponentOpen: Video codec context assigned, codec_id: {}", int(avCodecContext->codec_id));
        break;
    default:
        SPDLOG_LOGGER_WARN(logger, "PlayController::streamComponentOpen: Unsupported codec type: {}", int(avCodecContext->codec_type));
        avcodec_free_context(&avCodecContext);
        return -1;
    }

    return static_cast<int>(stream_index);
}

void PlayController::play()
{
    SPDLOG_LOGGER_INFO(logger, "PlayController::play");

    if (!isOpened()) {
        SPDLOG_LOGGER_WARN(logger, "PlayController::play: not opened");
        return;
    }

    // 如果 DemuxerThread 没有运行，启动它
    if (demuxThread_ && !demuxThread_->isRunning()) {
        demuxThread_->start();
    }

    // 注意：音频播放会在 AudioThread::writeAudio() 中首次写入数据后自动启动
    // OpenAL 要求队列中至少有一个缓冲区才能调用 alSourcePlay()

    // 注意：暂停状态已统一到状态机管理
    // AudioThread 现在通过状态机检查暂停状态，无需单独调用 setPaused

    // 启动 AVClock
    if (masterClock_) {
        masterClock_->start();
    }

    // 转换到 Playing 状态（统一使用状态机）
    stateMachine_.transitionTo(PlaybackState::Playing, "User started playback");
}

void PlayController::pause()
{
    SPDLOG_LOGGER_INFO(logger, "PlayController::pause");

    if (!isOpened()) {
        SPDLOG_LOGGER_WARN(logger, "PlayController::pause: not opened");
        return;
    }

    // 注意：暂停状态已统一到状态机管理
    // AudioThread 现在通过状态机检查暂停状态，无需单独调用 setPaused

    // 暂停 AVClock
    if (masterClock_) {
        masterClock_->pause(true);
    }

    // 转换到Paused状态（统一使用状态机）
    stateMachine_.transitionTo(PlaybackState::Paused, "User paused playback");
}

void PlayController::stop()
{
    SPDLOG_LOGGER_INFO(logger, "PlayController::stop");

    // 防止重复调用
    if (stateMachine_.isStopping() || stateMachine_.isStopped()) {
        SPDLOG_LOGGER_DEBUG(logger, "PlayController::stop: Already stopping or stopped");
        return;
    }

    // 停止 AVClock
    if (masterClock_) {
        masterClock_->reset();
    }

    // 0. 停止系统监控
    if (systemMonitor_) {
        systemMonitor_->stop();
    }

    // 1. 转换到Stopping状态
    stateMachine_.transitionTo(PlaybackState::Stopping, "Stop requested");

    // 1.5 立即唤醒所有在队列上等待的线程（Seek 后切换视频崩溃修复）
    // 在 stopThread 之前调用，确保 VideoThread/AudioThread/DemuxerThread 能立即退出等待
    vPackets_.setFinished();
    aPackets_.setFinished();
    SPDLOG_LOGGER_DEBUG(logger, "PlayController::stop: setFinished on queues to wake waiting threads");

    // 2. 先转换所有线程到停止状态（不立即删除）
    // 使用智能指针管理线程生命周期
    std::shared_ptr<DemuxerThread> demuxThreadSafe;
    std::shared_ptr<VideoThread> videoThreadSafe;
    std::shared_ptr<AudioThread> audioThreadSafe;

    // 临时接管线程指针所有权（使用move确保所有权转移）
    demuxThreadSafe = std::move(demuxThread_);
    videoThreadSafe = std::move(videoThread_);
    audioThreadSafe = std::move(audioThread_);

    // 3. 停止所有线程（使用统一停止顺序）
    // 停止DemuxerThread
    if (demuxThreadSafe) {
        stopThread(demuxThreadSafe.get(), "DemuxerThread", 5000);
    }

    // 停止VideoThread
    if (videoThreadSafe) {
        stopThread(videoThreadSafe.get(), "VideoThread", 5000);
    }

    // 停止AudioThread
    if (audioThreadSafe) {
        stopThread(audioThreadSafe.get(), "AudioThread", 5000);
    }

    // 5.5 确保所有线程已经完全停止（增强版本）
    // 使用多次检查，确保线程真正退出，避免在 Reset 队列时还有线程在访问

    // 第一次等待：100ms，让线程有机会退出
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // 检查线程是否还在运行，如果还在运行，继续等待
    int checkCount = 0;
    const int MAX_CHECKS = 30;        // 最多检查30次（1.5秒）
    const int CHECK_INTERVAL_MS = 50; // 每次检查间隔50ms

    bool videoRunning = false;
    bool audioRunning = false;
    bool demuxRunning = false;

    while (checkCount < MAX_CHECKS) {
        videoRunning = videoThreadSafe && videoThreadSafe->isRunning();
        audioRunning = audioThreadSafe && audioThreadSafe->isRunning();
        demuxRunning = demuxThreadSafe && demuxThreadSafe->isRunning();

        if (!videoRunning && !audioRunning && !demuxRunning) {
            // 所有线程都已停止
            SPDLOG_LOGGER_INFO(logger, "PlayController::stop: All threads stopped after {} checks", checkCount + 1);
            break;
        }

        checkCount++;
        if (checkCount % 5 == 0) {
            SPDLOG_LOGGER_DEBUG(
                logger,
                "PlayController::stop: Waiting for threads to stop, check {}/{} (video={}, audio={}, demux={})",
                checkCount + 1,
                MAX_CHECKS,
                videoRunning,
                audioRunning,
                demuxRunning);
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(CHECK_INTERVAL_MS));
    }

    if (checkCount >= MAX_CHECKS) {
        SPDLOG_LOGGER_WARN(
            logger,
            "PlayController::stop: Threads still running after {} checks (video={}, audio={}, demux={}), forcing continue",
            MAX_CHECKS,
            videoRunning,
            audioRunning,
            demuxRunning);
    }

    // 6. 清空队列（在线程完全停止后）
    try {
        SPDLOG_LOGGER_INFO(logger, "PlayController::stop: Resetting packet queues");
        vPackets_.Reset("VideoQueue");
        aPackets_.Reset("AudioQueue");
        SPDLOG_LOGGER_INFO(logger, "PlayController::stop: Packet queues reset complete");
    } catch (const std::exception &e) {
        SPDLOG_LOGGER_ERROR(logger, "PlayController::stop: Exception while resetting queues: {}", e.what());
    }

    // 7. 重置状态标志
    flushVideo_ = false;
    flushAudio_ = false;
    videoSeekPos_ = -1.0;
    audioSeekPos_ = -1.0;

    // 8. 转换到Stopped状态
    stateMachine_.transitionTo(PlaybackState::Stopped, "Playback stopped");

    // 9. 智能指针自动释放线程对象
    // demuxThreadSafe, videoThreadSafe, audioThreadSafe 在作用域结束时自动释放

    SPDLOG_LOGGER_INFO(logger, "PlayController::stop completed");
}

void PlayController::stopThread(QThread *thread, const char *threadName, int timeoutMs)
{
    if (!thread || !thread->isRunning()) {
        return;
    }

    SPDLOG_LOGGER_DEBUG(logger, "PlayController::stopThread: Stopping {}", threadName);

    // 根据线程类型调用相应的停止方法
    if (strcmp(threadName, "AudioThread") == 0) {
        AudioThread *audioThread = dynamic_cast<AudioThread *>(thread);
        if (audioThread) {
            audioThread->requestStop();
            // 通知音频队列线程可以退出等待
            aPackets_.setFinished();
        } else {
            SPDLOG_LOGGER_ERROR(logger, "PlayController::stopThread: Failed to cast to AudioThread");
            thread->terminate();
        }
    } else if (strcmp(threadName, "VideoThread") == 0) {
        VideoThread *videoThread = dynamic_cast<VideoThread *>(thread);
        if (videoThread) {
            videoThread->requestStop();
            // 通知视频队列线程可以退出等待
            vPackets_.setFinished();
        } else {
            SPDLOG_LOGGER_ERROR(logger, "PlayController::stopThread: Failed to cast to VideoThread");
            thread->terminate();
        }
    } else if (strcmp(threadName, "DemuxerThread") == 0) {
        // DemuxerThread 没有 requestStop 方法，直接终止
        thread->terminate();
        // 通知所有队列线程可以退出等待
        vPackets_.setFinished();
        aPackets_.setFinished();
    } else {
        SPDLOG_LOGGER_WARN(logger, "PlayController::stopThread: Unknown thread type {}, terminating", threadName);
        thread->terminate();
    }

    emptyBufferCond_.wakeAll();

    auto startWait = std::chrono::steady_clock::now();
    while (thread->isRunning()) {
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - startWait).count();

        if (elapsed > timeoutMs) {
            SPDLOG_LOGGER_WARN(logger, "PlayController::stopThread: {} timeout after {} ms, terminating", threadName, elapsed);
            thread->terminate();
            thread->wait(1000);
            break;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    SPDLOG_LOGGER_DEBUG(logger, "PlayController::stopThread: {} stopped", threadName);
}

bool PlayController::seek(int64_t positionMs)
{
    SPDLOG_LOGGER_INFO(logger, "PlayController::seek: {} ms", positionMs);

    if (!isOpened()) {
        SPDLOG_LOGGER_WARN(logger, "PlayController::seek: not opened");
        return false;
    }

    if (!demuxThread_) {
        SPDLOG_LOGGER_WARN(logger, "PlayController::seek: demuxThread_ is null (possibly playback stopped), cannot seek");
        return false;
    }

    // 检查 DemuxerThread 是否还在运行，如果已经退出，需要重新启动
    if (!demuxThread_->isRunning()) {
        SPDLOG_LOGGER_WARN(logger, "PlayController::seek: DemuxerThread is not running (likely reached EOF), restarting it");

        // 等待线程完全退出（如果还在退出过程中）
        // 使用超时机制避免无限等待
        auto startWait = std::chrono::steady_clock::now();
        while (demuxThread_->isRunning()) {
            auto elapsed = std::chrono::steady_clock::now() - startWait;
            if (elapsed > std::chrono::milliseconds(1000)) {
                SPDLOG_LOGGER_WARN(logger, "PlayController::seek: DemuxerThread wait timeout, forcing cleanup");
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        // 安全地删除旧的DemuxerThread
        if (demuxThread_) {
            // 确保线程已完全停止
            if (demuxThread_->isRunning()) {
                SPDLOG_LOGGER_ERROR(logger, "PlayController::seek: DemuxerThread is still running, cannot delete safely");
                stateMachine_.transitionTo(PlaybackState::Error, "DemuxerThread still running during seek restart");
                return false;
            }
            // 智能指针会自动管理生命周期
            demuxThread_.reset();
        }

        // 重新创建并启动 DemuxerThread
        try {
            demuxThread_ = std::make_shared<DemuxerThread>(this, this);

            // 断开finished()信号连接，手动管理线程生命周期
            disconnectThreadSignals(demuxThread_.get());

            demuxThread_->setDemuxer(demuxer_.get());
            demuxThread_->setStreamIndices(demuxer_->getVideoStreamIndex(), demuxer_->getAudioStreamIndex(), demuxer_->getSubtitleStreamIndex());
            demuxThread_->setVideoPacketQueue(&vPackets_);
            demuxThread_->setAudioPacketQueue(&aPackets_);

            // 重新连接DemuxerThread的信号（确保只有一次连接）
            connect(demuxThread_.get(), &DemuxerThread::seekFinished, this, &PlayController::onDemuxerThreadSeekFinished, Qt::UniqueConnection);
            connect(demuxThread_.get(), &DemuxerThread::eofReached, this, &PlayController::onDemuxerThreadEofReached, Qt::UniqueConnection);
            connect(demuxThread_.get(), &DemuxerThread::errorOccurred, this, &PlayController::onDemuxerThreadErrorOccurred, Qt::UniqueConnection);

            demuxThread_->start();
            SPDLOG_LOGGER_INFO(logger, "PlayController::seek: DemuxerThread restarted, will process seek request");

            // 等待一小段时间确保线程已经启动
            std::this_thread::sleep_for(std::chrono::milliseconds(50));

            // 验证线程已启动
            if (!demuxThread_->isRunning()) {
                SPDLOG_LOGGER_ERROR(logger, "PlayController::seek: DemuxerThread failed to start");
                stateMachine_.transitionTo(PlaybackState::Error, "DemuxerThread failed to start");
                return false;
            }
        } catch (const std::exception &e) {
            SPDLOG_LOGGER_ERROR(logger, "PlayController::seek: Exception while restarting DemuxerThread: {}", e.what());
            stateMachine_.transitionTo(PlaybackState::Error, "Exception restarting DemuxerThread: " + std::string(e.what()));
            return false;
        } catch (...) {
            SPDLOG_LOGGER_ERROR(logger, "PlayController::seek: Unknown exception while restarting DemuxerThread");
            stateMachine_.transitionTo(PlaybackState::Error, "Unknown exception restarting DemuxerThread");
            return false;
        }
    }

    // 检查是否已经在 seeking（避免重复 seek）
    // 统一使用状态机检查状态
    if (stateMachine_.isSeeking()) {
        SPDLOG_LOGGER_WARN(logger, "PlayController::seek: already seeking, ignoring new seek request");
        return false;
    }

    // 检查是否已经播放完成（EOF 状态）
    // 如果视频已经播放完毕，需要重新初始化播放器
    if (stateMachine_.isStopped() || (vPackets_.IsFinished() && aPackets_.IsFinished())) {
        SPDLOG_LOGGER_INFO(logger, "PlayController::seek: Playback finished (EOF state), cannot seek. Consider restarting playback.");
        return false;
    }

    // 计算 seek 位置（秒）
    double seekPosSec = static_cast<double>(positionMs) / 1000.0;
    int64_t positionUs = positionMs * 1000;

    // 自适应Seeking策略
    bool backward = false;

    // 检查当前播放位置
    int64_t currentPosMs = getCurrentPositionMs();

    // 如果seek距离很近（< 2秒），使用快速seek（不回退）
    if (std::abs(positionMs - currentPosMs) < 2000) {
        backward = false; // 直接跳转
    }
    // 如果seek距离很远（> 10秒），向后seek以确保有足够的关键帧
    else if (std::abs(positionMs - currentPosMs) > 10000) {
        backward = true; // 向后seek
    }
    // 中等距离，根据当前状态决定
    else {
        backward = (positionMs < currentPosMs); // 向前seek
    }

    SPDLOG_LOGGER_INFO(
        logger, "PlayController::seek: {} ms (current: {} ms, diff: {} ms), backward: {}", positionMs, currentPosMs, positionMs - currentPosMs, backward);

    // 验证 positionUs 的合理性
    if (positionUs < 0) {
        SPDLOG_LOGGER_ERROR(logger, "PlayController::seek: Invalid positionUs: {} (positionMs: {})", positionUs, positionMs);
        return false;
    }

    // 1. 先调用requestSeek()设置seekPositionUs_，避免竞态条件
    // 这样可以避免DemuxerThread在run()循环中读取到旧的seekPositionUs_（0）
    try {
        demuxThread_->requestSeek(positionUs, false); // 先设置seek参数，不执行seek
        SPDLOG_LOGGER_DEBUG(logger, "PlayController::seek: requestSeek called with positionUs={}", positionUs);
    } catch (const std::exception &e) {
        SPDLOG_LOGGER_ERROR(logger, "PlayController::seek: Exception while requesting seek: {}", e.what());
        return false;
    } catch (...) {
        SPDLOG_LOGGER_ERROR(logger, "PlayController::seek: Unknown exception while requesting seek");
        return false;
    }

    // 2. 使用统一的 enterSeeking()（保存 Seek 前状态，便于恢复到 Playing 或 Paused）
    if (!stateMachine_.enterSeeking("Seeking to " + std::to_string(positionMs) + " ms")) {
        SPDLOG_LOGGER_ERROR(logger, "PlayController::seek: Failed to enter Seeking state");
        return false;
    }

    SPDLOG_LOGGER_INFO(logger, "PlayController::seek: seeking flag set to true, preparing seek to {} ms", positionMs);

    // 2. 锁定 VideoThread 和 AudioThread（参考 QMPlayer2 的 DemuxerThr::seek()）
    // 使用统一的锁定顺序避免死锁：先VideoThread，后AudioThread
    // 注意：锁定失败时不应该继续执行，避免状态不一致
    bool vLocked = false, aLocked = false;

    // 先锁定VideoThread（统一顺序避免死锁）
    if (videoThread_) {
        vLocked = videoThread_->lock();
        if (!vLocked) {
            SPDLOG_LOGGER_ERROR(logger, "PlayController::seek: failed to lock VideoThread (timeout), aborting seek");
            // 回退状态：如果之前已经转换到 Seeking，需要回退
            if (stateMachine_.isSeeking()) {
                stateMachine_.exitSeeking("Seek aborted: failed to lock VideoThread");
            }
            return false;
        }
        SPDLOG_LOGGER_DEBUG(logger, "PlayController::seek: VideoThread locked successfully");
    }

    // 再锁定AudioThread（统一顺序避免死锁）
    if (audioThread_) {
        aLocked = audioThread_->lock();
        if (!aLocked) {
            SPDLOG_LOGGER_ERROR(logger, "PlayController::seek: failed to lock AudioThread (timeout), aborting seek");
            // 如果已经锁定了VideoThread，需要解锁
            if (vLocked && videoThread_) {
                videoThread_->unlock();
                SPDLOG_LOGGER_DEBUG(logger, "PlayController::seek: VideoThread unlocked after AudioThread lock failure");
            }
            // 回退状态：如果之前已经转换到 Seeking，需要回退
            if (stateMachine_.isSeeking()) {
                stateMachine_.exitSeeking("Seek aborted: failed to lock AudioThread");
            }
            return false;
        }
        SPDLOG_LOGGER_DEBUG(logger, "PlayController::seek: AudioThread locked successfully");
    }

    // 检查死锁（使用ThreadSyncManager）
    if (threadSyncManager_.detectDeadlock()) {
        SPDLOG_LOGGER_ERROR(logger, "PlayController::seek: Possible deadlock detected, aborting seek");
        if (vLocked && videoThread_) {
            videoThread_->unlock();
        }
        if (aLocked && audioThread_) {
            audioThread_->unlock();
        }
        // 回退状态：如果之前已经转换到 Seeking，需要回退
        if (stateMachine_.isSeeking()) {
            stateMachine_.exitSeeking("Seek aborted: deadlock detected");
        }
        return false;
    }

    // 3. 设置 flush 标志和 seek 位置（供 VideoThread 和 AudioThread 使用）
    flushVideo_ = true;
    flushAudio_ = true;
    videoSeekPos_ = seekPosSec;
    audioSeekPos_ = seekPosSec;

    // 4. 清空缓存（Video 和 Audio 的 PacketQueue）
    // 注意：在锁定线程后清空，确保线程安全
    try {
        vPackets_.Reset("VideoQueue");
        aPackets_.Reset("AudioQueue");
        SPDLOG_LOGGER_DEBUG(logger, "PlayController::seek: Queues reset");
    } catch (const std::exception &e) {
        SPDLOG_LOGGER_ERROR(logger, "PlayController::seek: Exception while resetting queues: {}", e.what());
    } catch (...) {
        SPDLOG_LOGGER_ERROR(logger, "PlayController::seek: Unknown exception while resetting queues");
    }

    // 5. 重置解码器缓冲区（确保 Seeking 后从新位置开始解码）
    // 注意：视频解码器的刷新由 VideoThread 在检查 flushVideo 时处理
    // 音频解码器的刷新由 AudioThread 在检查 flushAudio 时处理
    // 这里重置所有时钟基准，设置为seek目标位置，确保时钟计算正确

    // 使用 AVClock 更新时钟值
    if (masterClock_) {
        masterClock_->reset();                     // 先重置时钟
        masterClock_->setInitialValue(seekPosSec); // 设置初始值为seek位置
        masterClock_->start();                     // 重新启动时钟
    }

    // 设置全局基准 PTS（供 VideoThread 和 AudioThread 使用）
    nanoseconds seekBasePts = nanoseconds{static_cast<long long>(seekPosSec * 1000000000.0)};
    setBasePts(seekBasePts);      // 设置全局基准 PTS 为seek位置
    setVideoBasePts(seekBasePts); // 设置视频基准 PTS 为seek位置

    // 更新旧的视频时钟（用于 getVideoClock() fallback，AVClock完全集成后可删除）
    {
        if (!threadSyncManager_.tryLock(videoClockMutex_, std::chrono::milliseconds(100))) {
            SPDLOG_LOGGER_WARN(logger, "PlayController::seek: Failed to lock videoClockMutex_ (timeout)");
        } else {
            videoClock_ = seekBasePts;
            videoClockTime_ = {}; // 重置时间点，标记为"未更新"
            threadSyncManager_.unlock(videoClockMutex_);
        }
    }

    // 注意：clockbase_ 已不再使用，保留变量定义用于兼容
    SPDLOG_LOGGER_DEBUG(
        logger,
        "PlayController::seek: videoClock set to seekBasePts ({}ms), basePts set ({}ms), videoBasePts set ({}ms), videoClockTime reset",
        std::chrono::duration_cast<milliseconds>(seekBasePts).count(),
        std::chrono::duration_cast<milliseconds>(seekBasePts).count(),
        std::chrono::duration_cast<milliseconds>(seekBasePts).count());

    // 6. 清空 VideoRenderer 中的图像（在锁定状态下，避免并发渲染）
    if (videoRenderer_ && vLocked) {
        videoRenderer_->clear();
        SPDLOG_LOGGER_DEBUG(logger, "PlayController::seek: VideoRenderer image cleared (under lock)");
    }

    // 7. 解锁线程（在 seek 请求前解锁，让线程可以继续运行并检查 flush 标志）
    // 解锁顺序与锁定顺序相反：先AudioThread，后VideoThread
    if (aLocked && audioThread_) {
        audioThread_->unlock();
        SPDLOG_LOGGER_DEBUG(logger, "PlayController::seek: AudioThread unlocked");
    }
    if (vLocked && videoThread_) {
        videoThread_->unlock();
        SPDLOG_LOGGER_DEBUG(logger, "PlayController::seek: VideoThread unlocked");
    }

    // 8. 唤醒线程（让线程检查 flush 标志）
    emptyBufferCond_.wakeAll();

    // 9. 注意：requestSeek()已在前面调用，这里不需要再次调用
    // DemuxerThread的run()循环会检测到seeking状态并调用handleSeek()
    // 这样可以避免竞态条件，确保seekPositionUs_已正确设置
    SPDLOG_LOGGER_INFO(logger, "PlayController::seek: DemuxerThread will handle seek request (seekPositionUs_ already set)");

    // 注意：Seeking 状态由状态机管理，会在 onDemuxerThreadSeekFinished() 或 onDemuxerThreadErrorOccurred() 中转换
    // flushVideo_ 和 flushAudio_ 会在 VideoThread 和 AudioThread 中清除
    // videoSeekPos_ 和 audioSeekPos_ 会在 VideoThread 和 AudioThread 中清除

    return true;
}

bool PlayController::canSeek() const
{
    // 检查是否可以进行 seek 操作
    if (!isOpened()) {
        return false; // 文件未打开
    }

    if (stateMachine_.isStopped() || stateMachine_.isStopping()) {
        return false; // 播放已停止
    }

    if (!demuxThread_ || !demuxThread_->isRunning()) {
        return false; // DemuxerThread 不存在或未运行
    }

    // 检查是否已经播放完成（EOF 状态）
    if (vPackets_.IsFinished() && aPackets_.IsFinished()) {
        return false; // 播放已完成
    }

    return true;
}

int64_t PlayController::getDurationMs() const
{
    // 直接从 Demuxer 获取时长，不再通过 Movie
    return demuxer_ ? demuxer_->getDurationMs() : 0;
}

int64_t PlayController::getCurrentPositionMs() const
{
    // 通过主时钟获取当前播放位置
    // 注意：在新架构中，使用 getMasterClock() 获取主时钟（音频或视频）
    nanoseconds masterClock = getMasterClock();
    if (isValidTimestamp(masterClock)) {
        int64_t positionMs = std::chrono::duration_cast<milliseconds>(masterClock).count();
        // 验证返回值是否有效（避免负数或过大值导致UI错误）
        if (positionMs < 0) {
            SPDLOG_LOGGER_WARN(logger, "PlayController::getCurrentPositionMs: Invalid position ({} ms), returning 0", positionMs);
            return 0;
        }
        // 验证返回值是否超过视频时长（避免UI显示错误）
        int64_t durationMs = getDurationMs();
        if (durationMs > 0 && positionMs > durationMs) {
            SPDLOG_LOGGER_WARN(
                logger, "PlayController::getCurrentPositionMs: Position ({} ms) exceeds duration ({} ms), clamping to duration", positionMs, durationMs);
            return durationMs;
        }
        return positionMs;
    }
    return 0;
}

void PlayController::setVolume(float volume)
{
    // 直接通过 Audio 设置音量
    if (audio_) {
        audio_->setVolume(volume);
    }
}

float PlayController::getVolume() const
{
    // 直接通过 Audio 获取音量
    return audio_ ? audio_->getVolume() : 0.0f;
}

void PlayController::toggleMute()
{
    // 直接通过 Audio 切换静音
    if (audio_) {
        audio_->ToggleMute();
    }
}

bool PlayController::setPlaybackRate(double rate)
{
    // TODO: 实现播放速度控制功能
    // Movie 中的实现返回 false，表示未实现
    // 暂时返回 false，后续可以实现
    SPDLOG_LOGGER_WARN(logger, "PlayController::setPlaybackRate: Not implemented yet");
    return false;
}

double PlayController::getPlaybackRate() const
{
    // TODO: 实现播放速度控制功能
    // Movie 中的实现返回 1.0，表示正常速度
    // 暂时返回 1.0，后续可以实现
    return 1.0;
}

float PlayController::getVideoFrameRate() const
{
    // 直接从 Demuxer 获取视频帧率，不再通过 Movie
    return demuxer_ ? static_cast<float>(demuxer_->getVideoFrameRate()) : 0.0f;
}

bool PlayController::isThreadHealthy() const
{
    // 如果正在 Seeking，暂时放宽健康检查，避免误报
    if (stateMachine_.isSeeking()) {
        return true; // Seeking 期间认为线程健康
    }

    auto now = std::chrono::steady_clock::now();
    auto videoTimeout = now - lastVideoFrameTime_;
    auto audioTimeout = now - lastAudioFrameTime_;
    auto demuxTimeout = now - lastDemuxTime_;

    // 如果超过5秒没有新帧，认为线程可能有问题
    bool videoHealthy = videoTimeout < std::chrono::seconds(5);
    bool audioHealthy = audioTimeout < std::chrono::seconds(5);
    bool demuxHealthy = demuxTimeout < std::chrono::seconds(5);

    if (!videoHealthy || !audioHealthy || !demuxHealthy) {
        SPDLOG_LOGGER_WARN(
            logger,
            "Thread health check failed - Video: {}ms, Audio: {}ms, Demux: {}ms",
            std::chrono::duration_cast<std::chrono::milliseconds>(videoTimeout).count(),
            std::chrono::duration_cast<std::chrono::milliseconds>(audioTimeout).count(),
            std::chrono::duration_cast<std::chrono::milliseconds>(demuxTimeout).count());
    }

    return videoHealthy && audioHealthy && demuxHealthy;
}

bool PlayController::isPlaybackFinished(int64_t currentElapsedSeconds) const
{
    // 如果已经是 STOP 状态，不需要检查
    if (stateMachine_.isStopped() || stateMachine_.isStopping()) {
        return false;
    }

    // 检查1：队列是否都为空（或基本为空）
    bool videoQueueEmpty = vPackets_.IsEmpty();
    bool audioQueueEmpty = aPackets_.IsEmpty();

    // 检查2：线程健康状态
    auto now = std::chrono::steady_clock::now();
    auto videoTimeout = now - lastVideoFrameTime_;
    auto audioTimeout = now - lastAudioFrameTime_;
    auto demuxTimeout = now - lastDemuxTime_;

    // Video和Audio超过3秒没有新帧（可能是播放完成）
    bool videoFinished = videoTimeout >= std::chrono::seconds(3);
    bool audioFinished = audioTimeout >= std::chrono::seconds(3);

    // 检查3：播放时间是否接近总时长（如果总时长有效）
    bool timeNearEnd = false;
    int64_t totalMs = getDurationMs();
    int64_t totalSeconds = totalMs / 1000;

    // 如果总时长无效（<=0），不能判断为播放完成
    if (totalMs <= 0) {
        SPDLOG_LOGGER_DEBUG(logger, "isPlaybackFinished: totalMilliseconds is invalid ({}), cannot determine if finished", totalMs);
        return false;
    }

    if (totalSeconds > 3) {
        int64_t timeDiff = totalSeconds - currentElapsedSeconds;
        timeNearEnd = (timeDiff >= -2 && timeDiff <= 10);
        if (timeDiff < -2) {
            timeNearEnd = true;
        }
    }

    // 检查4：如果队列为空且线程已停止，即使时间未到，也可能是播放完成
    bool earlyFinish = false;

    if (videoQueueEmpty && audioQueueEmpty) {
        if (videoFinished && audioFinished) {
            int64_t videoTimeoutSeconds = std::chrono::duration_cast<std::chrono::seconds>(videoTimeout).count();
            int64_t audioTimeoutSeconds = std::chrono::duration_cast<std::chrono::seconds>(audioTimeout).count();
            if (videoTimeoutSeconds >= 1 && audioTimeoutSeconds >= 1) {
                if (totalSeconds > 3 && currentElapsedSeconds >= totalSeconds * 0.7) {
                    earlyFinish = true;
                } else if (totalSeconds <= 3 && currentElapsedSeconds >= 1) {
                    earlyFinish = true;
                }
            }
        }
    }

    // 检查超时是否超过总时长
    bool timeoutExceeded = false;
    if (totalSeconds > 3) {
        int64_t videoTimeoutSeconds = std::chrono::duration_cast<std::chrono::seconds>(videoTimeout).count();
        int64_t audioTimeoutSeconds = std::chrono::duration_cast<std::chrono::seconds>(audioTimeout).count();
        timeoutExceeded = (videoTimeoutSeconds > totalSeconds + 5) && (audioTimeoutSeconds > totalSeconds + 5);
    }

    // 综合判断
    // 注意：audioQueueEmpty 应该作为必要条件，因为音频队列为空可能表示播放完成
    // 但需要结合其他条件，避免误判（比如刚打开文件时队列也是空的）
    // 统一使用状态机检查停止状态
    bool isFinished = videoQueueEmpty && audioQueueEmpty && videoFinished && audioFinished
                      && (stateMachine_.isStopping() || stateMachine_.isStopped() || timeNearEnd || (totalSeconds <= 3 && totalSeconds > 0) || timeoutExceeded
                          || earlyFinish);

    if (isFinished) {
        SPDLOG_LOGGER_INFO(
            logger,
            "Playback finished detected - VideoQueue:{}, AudioQueue:{}, "
            "VideoTimeout:{}ms, AudioTimeout:{}ms, CurrentTime:{}s, TotalTime:{}s",
            videoQueueEmpty,
            audioQueueEmpty,
            std::chrono::duration_cast<std::chrono::milliseconds>(videoTimeout).count(),
            std::chrono::duration_cast<std::chrono::milliseconds>(audioTimeout).count(),
            currentElapsedSeconds,
            totalSeconds);
    }

    return isFinished;
}

bool PlayController::checkAndStopIfFinished(int64_t currentElapsedSeconds)
{
    // 如果已经是 STOP 状态或正在停止，不需要检查
    if (stateMachine_.isStopped() || stateMachine_.isStopping()) {
        return false;
    }

    // 检查是否播放完成
    if (isPlaybackFinished(currentElapsedSeconds)) {
        SPDLOG_LOGGER_INFO(logger, "checkAndStopIfFinished: Detected playback finished, triggering stop. currentElapsed:{}s", currentElapsedSeconds);

        // 主动触发停止（调用 stop() 方法完成完整的停止流程）
        stop();

        SPDLOG_LOGGER_INFO(logger, "checkAndStopIfFinished: Stop() called, playback will stop and trigger state change");

        return true;
    }

    return false;
}

void PlayController::onDemuxerThreadSeekFinished(int64_t positionUs)
{
    SPDLOG_LOGGER_INFO(logger, "PlayController::onDemuxerThreadSeekFinished: {} us", positionUs);

    // 检查 seek 是否失败（positionUs < 0 表示失败）
    if (positionUs < 0) {
        SPDLOG_LOGGER_ERROR(logger, "PlayController::onDemuxerThreadSeekFinished: Seek failed (positionUs: {})", positionUs);

        // Seek 失败，恢复到 Seek 前的状态
        if (stateMachine_.isSeeking()) {
            stateMachine_.exitSeeking("Seek failed: positionUs < 0");
        }

        // 发送失败信号（使用 -1 表示失败）
        emit seekingFinished(-1);
        return;
    }

    // 记录当前 seeking 状态（在清除之前）
    bool wasSeeking = stateMachine_.isSeeking();
    SPDLOG_LOGGER_INFO(logger, "PlayController::onDemuxerThreadSeekFinished: Current seeking state before clear: {}", wasSeeking);

    // 防止重复处理：如果状态已经不是 Seeking，说明已经被处理过了，直接返回
    PlaybackState currentState = stateMachine_.getState();
    if (currentState != PlaybackState::Seeking) {
        SPDLOG_LOGGER_DEBUG(
            logger,
            "PlayController::onDemuxerThreadSeekFinished: State is already {} (not Seeking), ignoring duplicate seekFinished signal",
            static_cast<int>(currentState));
        return; // 已经处理过，避免重复处理
    }

    // 验证seeking位置是否有效（避免发送错误的UI更新）
    // 如果positionUs为0，可能是中间seeking操作，不应该更新UI
    if (positionUs == 0 && wasSeeking) {
        SPDLOG_LOGGER_WARN(
            logger, "PlayController::onDemuxerThreadSeekFinished: Ignoring seekFinished signal with positionUs=0 (likely intermediate seek operation)");
        // 不转换状态，等待最终的seeking完成
        return;
    }

    // Seek 完成，恢复到 Seek 前的状态（Playing 或 Paused）
    stateMachine_.exitSeeking("Seek completed successfully");

    // 验证状态确实已转换
    bool isSeekingAfter = stateMachine_.isSeeking();
    if (isSeekingAfter) {
        SPDLOG_LOGGER_ERROR(logger, "PlayController::onDemuxerThreadSeekFinished: CRITICAL - state machine is still in Seeking state!");
        // 强制转换到Error状态
        stateMachine_.transitionTo(PlaybackState::Error, "State machine stuck in Seeking state");
    }

    SPDLOG_LOGGER_INFO(
        logger,
        "PlayController::onDemuxerThreadSeekFinished: State transition completed (was Seeking: {}, now Seeking: {}), threads can resume",
        wasSeeking,
        isSeekingAfter);

    // 注意：videoClock_已在seek()方法中设置为seekBasePts，此处不需要再次设置
    // 如果再次设置，会导致时钟基准不一致（VideoThread使用相对PTS，但videoClock使用绝对时间）
    SPDLOG_LOGGER_DEBUG(logger, "PlayController::onDemuxerThreadSeekFinished: videoClock already set in seek() method, skipping reset to maintain consistency");

    // 转换为毫秒并发送信号（通知 UI 层 seeking 完成）
    int64_t positionMs = positionUs / 1000;
    SPDLOG_LOGGER_INFO(
        logger, "PlayController::onDemuxerThreadSeekFinished: Sending seekingFinished signal with positionMs={} (from positionUs={})", positionMs, positionUs);
    emit seekingFinished(positionMs);

    // 再次验证状态机状态（确保信号发送后状态正确）
    bool finalCheck = stateMachine_.isSeeking();
    if (finalCheck) {
        SPDLOG_LOGGER_ERROR(logger, "PlayController::onDemuxerThreadSeekFinished: CRITICAL - state machine is still in Seeking state after signal emit!");
    } else {
        SPDLOG_LOGGER_DEBUG(logger, "PlayController::onDemuxerThreadSeekFinished: Final verification - state machine is correctly not Seeking");
    }
}

void PlayController::onPlaybackCompleted()
{
    SPDLOG_LOGGER_INFO(logger, "PlayController::onPlaybackCompleted: VideoThread reported playback completion");

    // 检查音频是否也完成了
    if (isAudioFinished()) {
        SPDLOG_LOGGER_INFO(logger, "PlayController::onPlaybackCompleted: Both video and audio finished, stopping playback");

        // 停止播放（但不立即转换到下一个视频，保持原有行为）
        // 注意：播放列表自动切换功能由MainWindow处理，这里只负责停止当前播放
        if (!stateMachine_.isStopping() && !stateMachine_.isStopped()) {
            stop();
        }

        // 发送播放完成信号，供MainWindow处理播放列表自动切换
        emit playbackCompleted();
    } else {
        SPDLOG_LOGGER_DEBUG(logger, "PlayController::onPlaybackCompleted: Video finished but audio still playing");
    }
}

void PlayController::onDemuxerThreadEofReached()
{
    SPDLOG_LOGGER_INFO(logger, "PlayController::onDemuxerThreadEofReached");

    // 检查是否有 pending 的 seek 请求（在 EOF 后用户可能想要 seek）
    // 如果有 seek 请求，需要重新启动 DemuxerThread 来处理
    if (stateMachine_.isSeeking()) {
        SPDLOG_LOGGER_INFO(logger, "PlayController::onDemuxerThreadEofReached: Seeking is pending, will restart DemuxerThread");
        // 不在这里重启，让 seek() 方法处理重启逻辑
        // 但如果线程已经退出，seek() 方法会重新启动它
        return;
    }

    // EOF 处理：当解复用线程到达文件末尾时，等待所有帧播放完成
    // 播放完成检测由 isPlaybackFinished() 和 checkAndStopIfFinished() 处理
    // 这里只记录日志，不主动停止播放（让自然播放完成）
}

void PlayController::wakeAllThreads()
{
    emptyBufferCond_.wakeAll();
}

void PlayController::disconnectThreadSignals(QThread *thread)
{
    if (thread) {
        // 断开finished()信号连接，避免Qt自动删除对象
        // 我们需要手动管理线程的生命周期
        disconnect(thread, &QThread::finished, thread, &QObject::deleteLater);

        // 断开DemuxerThread的信号连接
        if (DemuxerThread *demuxThread = dynamic_cast<DemuxerThread *>(thread)) {
            disconnect(demuxThread, &DemuxerThread::seekFinished, this, &PlayController::onDemuxerThreadSeekFinished);
            disconnect(demuxThread, &DemuxerThread::eofReached, this, &PlayController::onDemuxerThreadEofReached);
            disconnect(demuxThread, &DemuxerThread::errorOccurred, this, &PlayController::onDemuxerThreadErrorOccurred);
        }
    }
}

void PlayController::onDemuxerThreadErrorOccurred(int errorCode)
{
    SPDLOG_LOGGER_ERROR(logger, "PlayController::onDemuxerThreadErrorOccurred: {}", errorCode);

    // 如果正在 seeking，转换状态，避免卡死
    if (stateMachine_.isSeeking()) {
        SPDLOG_LOGGER_WARN(logger, "PlayController::onDemuxerThreadErrorOccurred: Seeking in progress, transitioning to Error state to avoid deadlock");
        stateMachine_.transitionTo(PlaybackState::Error, "DemuxerThread error occurred during seeking");
        // 发送 seekFinished 信号，但位置为 -1 表示失败
        emit seekingFinished(-1);
    }

    // 错误处理：解复用线程发生错误时，停止播放
    // 这通常表示文件读取失败或格式不支持
    SPDLOG_LOGGER_WARN(logger, "DemuxerThread error occurred, stopping playback");
    stop();
}

nanoseconds PlayController::getMasterClock() const
{
    // 音频为主时钟时：优先使用 Audio 的实际播放位置（AL_SAMPLE_OFFSET），而非“最后写入 PTS”
    // 修复：音视频同步、进度条与实际播放不同步（日志分析 BUG 1、3）
    if (sync_ == SyncMaster::Audio && audioThread_) {
        nanoseconds audioClock = audioThread_->getClock();
        if (isValidTimestamp(audioClock)) {
            nanoseconds basePts = getBasePts();
            if (isValidTimestamp(basePts)) {
                return basePts + audioClock;
            }
            // basePts 未设置时（如 seek 后尚未开始），若 AVClock 有 initialValue 则使用
            if (masterClock_ && masterClock_->initialValue() > 0.0) {
                return nanoseconds{static_cast<long long>(masterClock_->initialValue() * 1000000000.0)};
            }
            return audioClock;
        }
    }

    // 使用 AVClock 获取主时钟（视频为主或 fallback）
    if (masterClock_ && masterClock_->isActive()) {
        double clockValue = masterClock_->value();
        nanoseconds clockNs = nanoseconds{static_cast<long long>(clockValue * 1000000000.0)};
        return clockNs;
    }

    // Fallback 到视频时钟
    nanoseconds videoClock = getVideoClock();
    return isValidTimestamp(videoClock) ? videoClock : nanoseconds::zero();
}

void PlayController::updateVideoClock(nanoseconds pts)
{
    // 防止无效时间戳导致后续算术溢出
    if (!isValidTimestamp(pts)) {
        pts = nanoseconds(0);
    }

    // 更新视频时钟（由 VideoThread 在渲染帧时调用）
    // 同时更新 AVClock 和旧的视频时钟（用于 fallback）

    // 更新 AVClock（如果使用视频时钟）
    if (masterClock_ && masterClock_->clockType() == AVClock::VideoClock) {
        double ptsSeconds = static_cast<double>(pts.count()) / 1000000000.0;
        masterClock_->updateVideoTime(ptsSeconds);
    }

    // 更新旧的视频时钟（用于 getVideoClock() fallback）
    if (!threadSyncManager_.tryLock(videoClockMutex_, std::chrono::milliseconds(100))) {
        SPDLOG_LOGGER_WARN(logger, "PlayController::updateVideoClock: Failed to lock videoClockMutex_ (timeout)");
    } else {
        videoClock_ = pts;
        videoClockTime_ = std::chrono::steady_clock::now(); // 记录更新时间
        threadSyncManager_.unlock(videoClockMutex_);
    }
}

nanoseconds PlayController::getVideoClock() const
{
    // 参考旧版本的实现：displayPts_ + delta
    // 视频时钟 = 最后渲染帧的PTS + 从渲染时间到现在的经过时间
    if (!const_cast<ThreadSyncManager &>(threadSyncManager_).tryLock(const_cast<std::mutex &>(videoClockMutex_), std::chrono::milliseconds(100))) {
        SPDLOG_LOGGER_WARN(logger, "PlayController::getVideoClock: Failed to lock videoClockMutex_ (timeout)");
        return videoClock_; // 返回当前值，即使未获取锁
    } else {
        nanoseconds result = videoClock_;

        // 防止对无效时间戳做算术运算（溢出风险）
        if (!isValidTimestamp(result)) {
            const_cast<ThreadSyncManager &>(threadSyncManager_).unlock(const_cast<std::mutex &>(videoClockMutex_));
            return kInvalidTimestamp;
        }

        // 如果 videoClockTime_ 已设置，计算实时时钟
        if (videoClockTime_ != std::chrono::steady_clock::time_point{}) {
            auto now = std::chrono::steady_clock::now();
            auto delta = now - videoClockTime_;
            result += std::chrono::duration_cast<nanoseconds>(delta);
        }

        const_cast<ThreadSyncManager &>(threadSyncManager_).unlock(const_cast<std::mutex &>(videoClockMutex_));
        return result;
    }
}

// 全局基准 PTS 管理方法
void PlayController::setBasePts(nanoseconds pts)
{
    std::lock_guard<std::mutex> lock(basePtsMutex_);
    basePts_ = pts;
    SPDLOG_LOGGER_INFO(logger, "PlayController::setBasePts: basePts_ set to {}ms", std::chrono::duration_cast<milliseconds>(pts).count());
}

nanoseconds PlayController::getBasePts() const
{
    std::lock_guard<std::mutex> lock(basePtsMutex_);
    return basePts_;
}

void PlayController::setVideoBasePts(nanoseconds pts)
{
    std::lock_guard<std::mutex> lock(basePtsMutex_);
    videoBasePts_ = pts;
    SPDLOG_LOGGER_INFO(logger, "PlayController::setVideoBasePts: videoBasePts_ set to {}ms", std::chrono::duration_cast<milliseconds>(pts).count());
}

nanoseconds PlayController::getVideoBasePts() const
{
    std::lock_guard<std::mutex> lock(basePtsMutex_);
    return videoBasePts_;
}

void PlayController::updateSyncThreshold(nanoseconds currentError)
{
    // 计算移动平均误差
    const int alpha = 10; // 平滑因子
    avgSyncError_ = (avgSyncError_ * (alpha - 1) + currentError) / alpha;
    syncErrorCount_++;

    // 根据误差动态调整阈值（针对新的宽松阈值）
    if (std::abs(avgSyncError_.count()) > 100) { // 100ms以上误差
        // 误差较大，增大阈值
        syncThreshold_ = std::min(syncThreshold_ + milliseconds(10), maxSyncThreshold_);
    } else if (std::abs(avgSyncError_.count()) < 20) { // 20ms以内误差
        // 误差较小，可以稍微收紧阈值
        syncThreshold_ = std::max(syncThreshold_ - milliseconds(5), nanoseconds{milliseconds(20)});
    }
}

bool PlayController::isAudioFinished() const
{
    bool finished = audioThread_ ? audioThread_->audioFinished_.load(std::memory_order_acquire) : false;
    if (logger) {
        SPDLOG_LOGGER_DEBUG(logger, "PlayController::isAudioFinished: audioThread_={}, audioFinished_={}", (void *) audioThread_.get(), finished);
    }
    return finished;
}
