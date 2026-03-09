#include "video.h"
#include "../movie.h"
#include "libavutil/imgutils.h"
#include <chrono>
#include <iostream>
#include <sstream>

#include "spdlog/spdlog.h"
#include <algorithm>
#include <iostream>
#include <QDebug>
#include <QtLogging>

using std::chrono::duration_cast;

Video::Video(Movie &movie)
    : movie_(movie)
{
    isPause.store(false);
    isQuit.store(false);

    m_error = new char[ERROR_LEN];
}

Video::~Video()
{
    if (m_error) {
        delete m_error;
        m_error = nullptr;
    }
}

void Video::showError(int err)
{
#if PRINT_LOG
    memset(m_error, 0, ERROR_LEN); // 将数组置零
    av_strerror(err, m_error, ERROR_LEN);
    logger->error("DecodeVideo Error：{}", m_error);
#else
    Q_UNUSED(err)
#endif
}


static void pgm_save(AVFrame *frame)
{
    FILE *file;
    int i;
    
    fopen_s(&file, "test.pgm", "wb");
    fprintf(file, "P5\n%d %d\n%d\n", frame->width, frame->height, 255);
    for (i = 0; i < frame->height; i++)
        fwrite(frame->data[0] + i * frame->linesize[0], 1, frame->width, file);

    fclose(file);
}

// 保存帧到指定文件名（用于硬件解码调试）
static void pgm_save_frame(AVFrame *frame, const char *filename)
{
    if (!frame || !filename) return;
    
    FILE *file;
    int i;
    
    fopen_s(&file, filename, "wb");
    if (!file) {
        return;
    }
    
    fprintf(file, "P5\n%d %d\n%d\n", frame->width, frame->height, 255);
    for (i = 0; i < frame->height; i++)
        fwrite(frame->data[0] + i * frame->linesize[0], 1, frame->width, file);

    fclose(file);
}

static void ppm_save(unsigned char *buf, int wrap, int xsize, int ysize, char *filename)
{
    FILE *f;
    int i;

    fopen_s(&f, filename, "wb");
    fprintf(f, "P6\n%d %d\n%d\n", xsize, ysize, 255);

    for (i = 0; i < ysize; i++) {
        fwrite(buf + i * wrap, 1, xsize * 3, f);
    }

    fclose(f);
}

// 测试代码，用于验证解码是否正常
//#define INSPECT_DECODED_FRAME

// 硬件解码帧保存功能（用于调试）
//#define INSPECT_HARDWARE_DECODED_FRAME

int Video::start()
{
    logger->info(" Video.start ");
    pictQRead_.store(0u);
    pictQWrite_.store(1u);
    isQuit.store(false);
    isPause.store(false);
    preloadCompleted_.store(false, std::memory_order_release);  // 重置预加载标志

    // 复用现有的frame，避免重复分配导致内存泄漏
    // RingBuffer的AVFrame应该是一次性分配后持续复用
    std::for_each(pictQ_.begin(), pictQ_.end(), [](Picture &pic) {
        // 如果frame已存在，先unref释放缓冲区，然后复用frame对象
        if (pic.frame) {
            av_frame_unref(pic.frame.get());  // 释放旧的缓冲区，但保留frame对象
        } else {
            // 只有在frame不存在时才分配新的
            AVFrame* frame = av_frame_alloc();
            if (!frame) {
                logger->error("Failed to allocate AVFrame in Video::start()");
                // 如果分配失败，使用nullptr，后续使用时会检查
            }
            pic.frame = AVFramePtr{frame};
        }
        // sw_frame和yuv420p_frame在需要时分配，这里只重置指针（clear()中已释放）
        pic.sw_frame = nullptr;      // 软件帧初始化为空，需要时再分配
        pic.yuv420p_frame = nullptr; // YUV420P 帧初始化为空，需要时再分配
        pic.pts = nanoseconds::min();
    });

    // Prefill the codec buffer.
    do {
        int ret = packets_.sendTo(codecctx_.get());
        if (ret == AVERROR(EAGAIN) || ret == AVErrorEOF) {
            break;
        }
        if (ret != 0) {
            logger->error("Fail to send packet video, ret:{} ", ret);
            showError(ret);
        }
    } while (1);

    static int frameNum = 0;
    static int outputCounter = 0;
    static size_t lastEOFReadIdx = SIZE_MAX;  // 记录EOF等待时的read_idx
    static std::chrono::steady_clock::time_point eofWaitStartTime;  // EOF等待开始时间
    auto current_pts = nanoseconds::zero();
    while (!movie_.quit_.load(std::memory_order_relaxed) && !isQuit.load(std::memory_order_relaxed)) {
        logger->debug("video.mainLoop entered.");
        if (isPause) {
            logger->debug("video.isPause, sleep for 5 ms");
            std::this_thread::sleep_for(milliseconds(5));
            continue;
        }

        size_t write_idx = pictQWrite_.load(std::memory_order_relaxed);
        Picture *vp = &pictQ_[write_idx];

        // Retrieve video frame.
        AVFrame *decoded_frame = vp->frame.get();
        int ret, pret;
        while ((ret = avcodec_receive_frame(codecctx_.get(), decoded_frame)) == AVERROR(EAGAIN) && (pret = packets_.sendTo(codecctx_.get())) != AVErrorEOF) {
        }
        
        // 如果使用硬件解码，可能需要将硬件帧转换为软件帧
        if (ret == 0 && decoded_frame->format != AV_PIX_FMT_NONE) {
            // 检查是否是硬件格式
            bool is_hw_format = (decoded_frame->format == AV_PIX_FMT_D3D11 ||
                                decoded_frame->format == AV_PIX_FMT_CUDA ||
                                decoded_frame->format == AV_PIX_FMT_QSV ||
                                decoded_frame->format == AV_PIX_FMT_DXVA2_VLD);
            
            if (is_hw_format && movie_.hardwareDecoder_ && movie_.hardwareDecoder_->needsTransfer()) {
                // 需要转换硬件帧到软件帧
                if (!vp->sw_frame) {
                    vp->sw_frame = AVFramePtr{av_frame_alloc()};
                    if (!vp->sw_frame) {
                        logger->error("Failed to allocate software frame");
                        continue;
                    }
                }
                
                // 重要：先unref释放旧的缓冲区，避免内存泄漏
                // transferFrame内部可能会分配新缓冲区，先清理旧的
                av_frame_unref(vp->sw_frame.get());
                
                // transferFrame 会自动设置软件帧的格式和属性，不需要手动设置
                ret = movie_.hardwareDecoder_->transferFrame(decoded_frame, vp->sw_frame.get(), codecctx_.get());
                if (ret == 0) {
                    // 转换成功，检查格式是否需要进一步转换
                    AVFrame* sw_frame = vp->sw_frame.get();
                    
                    // 如果转换后的格式是 NV12，需要转换为 YUV420P（渲染器只支持 YUV420P）
                    if (sw_frame->format == AV_PIX_FMT_NV12) {
                        // 创建格式转换上下文（如果还没有）
                        // 使用成员变量而不是静态变量，以便在clear()时释放
                        static SwsContext* nv12_to_yuv420p_ctx = nullptr;
                        static int last_width = 0;
                        static int last_height = 0;
                        static bool ctx_initialized = false;
                        
                        if (!nv12_to_yuv420p_ctx || 
                            last_width != sw_frame->width || 
                            last_height != sw_frame->height) {
                            if (nv12_to_yuv420p_ctx) {
                                sws_freeContext(nv12_to_yuv420p_ctx);
                                nv12_to_yuv420p_ctx = nullptr;
                            }
                            
                            nv12_to_yuv420p_ctx = sws_getContext(
                                sw_frame->width, sw_frame->height, AV_PIX_FMT_NV12,
                                sw_frame->width, sw_frame->height, AV_PIX_FMT_YUV420P,
                                SWS_BILINEAR, nullptr, nullptr, nullptr);
                            
                            if (!nv12_to_yuv420p_ctx) {
                                logger->error("Failed to create SwsContext for NV12 to YUV420P conversion");
                                continue;
                            }
                            
                            last_width = sw_frame->width;
                            last_height = sw_frame->height;
                            ctx_initialized = true;
                            logger->info("Created SwsContext for NV12 to YUV420P conversion: {}x{}", 
                                       sw_frame->width, sw_frame->height);
                        }
                        
                        // 分配 YUV420P 帧（如果还没有）
                        if (!vp->yuv420p_frame) {
                            vp->yuv420p_frame = AVFramePtr{av_frame_alloc()};
                            if (!vp->yuv420p_frame) {
                                logger->error("Failed to allocate YUV420P frame");
                                continue;
                            }
                        }
                        
                        AVFrame* yuv420p_frame = vp->yuv420p_frame.get();
                        
                        // 重要：先unref释放旧的缓冲区，避免内存泄漏
                        // 如果缓冲区已存在，av_frame_get_buffer会失败或导致泄漏
                        av_frame_unref(yuv420p_frame);
                        
                        // 设置 YUV420P 帧的属性
                        yuv420p_frame->format = AV_PIX_FMT_YUV420P;
                        yuv420p_frame->width = sw_frame->width;
                        yuv420p_frame->height = sw_frame->height;
                        yuv420p_frame->pts = sw_frame->pts;
                        yuv420p_frame->best_effort_timestamp = sw_frame->best_effort_timestamp;  // 确保PTS正确传递
                        yuv420p_frame->repeat_pict = sw_frame->repeat_pict;  // 传递repeat_pict
                        
                        // 分配缓冲区（unref后可以安全分配）
                        ret = av_frame_get_buffer(yuv420p_frame, 0);
                        if (ret < 0) {
                            logger->error("Failed to allocate buffer for YUV420P frame: {}", ret);
                            continue;
                        }
                        
                        // 执行格式转换
                        int sws_ret = sws_scale(nv12_to_yuv420p_ctx,
                                                sw_frame->data, sw_frame->linesize, 0, sw_frame->height,
                                                yuv420p_frame->data, yuv420p_frame->linesize);
                        
                        if (sws_ret != sw_frame->height) {
                            logger->error("Failed to convert NV12 to YUV420P: sws_scale returned {} (expected {})", 
                                         sws_ret, sw_frame->height);
                            continue;
                        }
                        
                        // 使用转换后的 YUV420P 帧
                        decoded_frame = yuv420p_frame;
                        // 重要：保持 ret = 0，表示解码成功（sws_scale 返回的是处理的行数，不是错误码）
                        ret = 0;
                        logger->debug("Converted NV12 to YUV420P successfully");
                        
#ifdef INSPECT_HARDWARE_DECODED_FRAME
                        // 保存硬件解码后的帧（用于调试）
                        static int hw_frameNum = 0;
                        hw_frameNum++;
                        if (hw_frameNum <= 10) {
                            // 保存原始硬件帧（NV12格式）
                            char hw_filename[256];
                            snprintf(hw_filename, sizeof(hw_filename), "hw_frame_%03d_nv12.pgm", hw_frameNum);
                            pgm_save_frame(sw_frame, hw_filename);
                            
                            // 保存转换后的YUV420P帧
                            char yuv_filename[256];
                            snprintf(yuv_filename, sizeof(yuv_filename), "hw_frame_%03d_yuv420p.pgm", hw_frameNum);
                            pgm_save_frame(yuv420p_frame, yuv_filename);
                            
                            logger->info("Saved hardware decoded frame {}: NV12 -> YUV420P", hw_frameNum);
                        }
#endif
                    } else {
                        // 格式已经是支持的格式，直接使用
                        decoded_frame = sw_frame;
                        logger->debug("Hardware frame transferred successfully, format: {}, using software frame", 
                                     sw_frame->format);
                        
#ifdef INSPECT_HARDWARE_DECODED_FRAME
                        // 保存硬件解码后的帧（用于调试）
                        static int hw_frameNum = 0;
                        hw_frameNum++;
                        if (hw_frameNum <= 10) {
                            char hw_filename[256];
                            snprintf(hw_filename, sizeof(hw_filename), "hw_frame_%03d_sw.pgm", hw_frameNum);
                            pgm_save_frame(sw_frame, hw_filename);
                            logger->info("Saved hardware decoded frame {}: format {}", hw_frameNum, sw_frame->format);
                        }
#endif
                    }
                } else {
                    logger->error("Failed to transfer hardware frame to software frame: {}", ret);
                    // 转换失败，跳过当前帧，继续处理下一帧
                    continue;
                }
            }
        }

#ifdef INSPECT_DECODED_FRAME
        frameNum += 1;
        if (frameNum == 10) {
            //struct SwsContext *imgCtx = sws_getContext(
            //    codecctx_->width, codecctx_->height, codecctx_->pix_fmt, codecctx_->width, codecctx_->height, AV_PIX_FMT_RGB32, SWS_BICUBIC, NULL, NULL, NULL);
            //int numBytes = av_image_get_buffer_size(AV_PIX_FMT_RGB32, codecctx_->width, codecctx_->height, 1);
            //unsigned char *out_buffer = (unsigned char *) av_malloc(numBytes * sizeof(uchar));
            //saveFrame(decoded_frame, codecctx_->width, codecctx_->height, frameNum);
            Pgm_Save(decoded_frame);

            //Create SWS Context for converting from decode pixel format (like YUV420) to RGB
            ////////////////////////////////////////////////////////////////////////////
            struct SwsContext *sws_ctx = NULL;
            sws_ctx = sws_getContext(
                codecctx_->width, codecctx_->height, codecctx_->pix_fmt, codecctx_->width, codecctx_->height, AV_PIX_FMT_RGB24, SWS_BICUBIC, NULL, NULL, NULL);

            if (sws_ctx == nullptr) {
                logger->error("sws_getContext failed.");
            }
            else {
                //Allocate frame for storing image converted to RGB.
                ////////////////////////////////////////////////////////////////////////////
                AVFrame *pRGBFrame = av_frame_alloc();
                if (!pRGBFrame) {
                    logger->error("av_frame_alloc failed for pRGBFrame");
                    sws_freeContext(sws_ctx);
                } else {
                    pRGBFrame->format = AV_PIX_FMT_RGB24;
                    pRGBFrame->width = codecctx_->width;
                    pRGBFrame->height = codecctx_->height;

                    int sts = av_frame_get_buffer(pRGBFrame, 0);

                    if (sts < 0) {
                        logger->error("av_frame_get_buffer failed.");
                        av_frame_free(&pRGBFrame);
                        sws_freeContext(sws_ctx);
                    } else {
                        //Convert from input format (e.g YUV420) to RGB and save to PPM:
                        ////////////////////////////////////////////////////////////////////////////
                        sts = sws_scale(
                            sws_ctx,              //struct SwsContext* c,
                            decoded_frame->data,  //const uint8_t* const srcSlice[],
                            decoded_frame->linesize, //const int srcStride[],
                            0,                    //int srcSliceY,
                            decoded_frame->height,   //int srcSliceH,
                            pRGBFrame->data,      //uint8_t* const dst[],
                            pRGBFrame->linesize); //const int dstStride[]);

                        if (sts != decoded_frame->height) {
                            logger->error("sws_scale failed");
                        } else {
                            char buf[1024];
                            snprintf(buf, sizeof(buf), "%s_%03d.ppm", "testRGB.ppm", codecctx_->frame_number);
                            ppm_save(pRGBFrame->data[0], pRGBFrame->linesize[0], pRGBFrame->width, pRGBFrame->height, buf);
                        }
                        
                        // 释放分配的资源
                        av_frame_free(&pRGBFrame);
                    }
                    // 释放SwsContext
                    sws_freeContext(sws_ctx);
                }
            }
        }
#endif

        if (ret != 0) {
            if (ret == AVErrorEOF || pret == AVErrorEOF) {
                //qDebug() << "!!! EOF video.mainloop, avcodec_receive_frame, ret:" << ret << "==EOF || pret:" << pret << "==EOF:" << AVErrorEOF << ", check for break";
                size_t read_idx = pictQRead_.load(std::memory_order_relaxed);
                size_t next_idx = (read_idx + 1) % pictQ_.size();
                if (next_idx != pictQWrite_.load(std::memory_order_acquire)) {
                    // 队列还有未消费的帧，等待渲染线程消费
                    // 监控机制：检测队列是否持续未消费
                    auto now = std::chrono::steady_clock::now();
                    
                    // 如果read_idx发生变化，说明渲染线程在消费，重置监控
                    if (lastEOFReadIdx != read_idx || lastEOFReadIdx == SIZE_MAX) {
                        // 首次进入或read_idx变化，初始化/重置监控
                        lastEOFReadIdx = read_idx;
                        eofWaitStartTime = now;
                        if (outputCounter % 100 == 0) {
                            logger->debug(
                                "!! EOF, but packet is not empty, waiting to read all rest frames, next_idx:{}, pictQWrite_:{}", next_idx, pictQWrite_.load());
                        }
                        outputCounter += 1;
                    } else {
                        // read_idx没有变化，检查等待时间
                        auto waitDuration = std::chrono::duration_cast<std::chrono::milliseconds>(now - eofWaitStartTime);
                        const int MAX_EOF_WAIT_MS = 5000;  // 最多等待5秒
                        
                        if (waitDuration.count() > MAX_EOF_WAIT_MS) {
                            // 持续5秒队列没有变化，说明渲染线程可能卡住了
                            logger->error("EOF wait timeout: read_idx has not changed for {}ms, read_idx:{}, write_idx:{}. "
                                        "Rendering thread may be stuck. Exiting decode loop to prevent deadlock.",
                                        waitDuration.count(), read_idx, pictQWrite_.load());
                            // 重置监控变量
                            lastEOFReadIdx = SIZE_MAX;
                            break;  // 退出解码循环
                        } else if (waitDuration.count() > 1000 && outputCounter % 50 == 0) {
                            // 等待超过1秒时，每50次循环打印一次警告
                            logger->warn("EOF wait: read_idx unchanged for {}ms, read_idx:{}, write_idx:{}. "
                                    "Rendering thread may be slow.",
                                    waitDuration.count(), read_idx, pictQWrite_.load());
                        }
                        outputCounter += 1;
                }
                    
                    // 检查是否应该退出
                    if (movie_.quit_.load(std::memory_order_relaxed) || isQuit.load(std::memory_order_relaxed)) {
                        logger->info("EOF wait: quit flag set, exiting decode loop");
                        lastEOFReadIdx = SIZE_MAX;
                        break;
                    }
                    
                    // 短暂休眠，避免CPU占用过高
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                    continue;
                } else {
                    logger->debug("!!EOF. next_idx!=pictQWrite_, check(should equal), next_idx:{} ==  pictQWrite_:{}, break", next_idx, pictQWrite_.load());
                    // 重置监控变量
                    lastEOFReadIdx = SIZE_MAX;
                    break;
                }
            } else {
                logger->warn("Fail to receive frame, ret:{} ", ret);
                showError(ret);
                
                // 改进错误恢复机制
                if (ret == AVERROR(EAGAIN)) {
                    // 暂时没有数据，短暂等待后继续
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                    continue;
                } else if (ret == AVERROR_INVALIDDATA) {
                    // 数据损坏，跳过当前包
                    logger->warn("Invalid data detected, skipping packet");
                    packets_.sendTo(codecctx_.get()); // 跳过当前包
                    continue;
                } else {
                    // 严重错误，可能需要重启解码器
                    logger->error("Serious decode error: {}, attempting recovery", ret);
                    // TODO: 可以在这里添加解码器重启逻辑
                    break;
                }
            }
        } else {
            logger
                ->debug(" video.mainloop, receive video frame ok, pictQRead_:{},pictQWrite_:{}, pts:{} ", pictQRead_.load(), pictQWrite_.load(), vp->pts.count());
            // 更新视频线程健康检查时间戳
            movie_.lastVideoFrameTime_ = std::chrono::steady_clock::now();
        }

        if (ret == AVErrorEOF) {
            logger->info(" ret=AVEOF,  , pictQRead_:{},pictQWrite:{}, pts:{}", pictQRead_.load(), pictQWrite_.load(), vp->pts.count());
        }

        // Update pts - 重要：使用原始硬件帧的PTS（如果decoded_frame指向转换后的帧，需要从原始帧获取PTS）
        // 如果decoded_frame指向yuv420p_frame或sw_frame，它们的PTS应该已经正确设置
        AVFrame* pts_source_frame = decoded_frame;
        // 如果decoded_frame指向转换后的帧，尝试从原始帧获取PTS
        if (decoded_frame == vp->yuv420p_frame.get() || decoded_frame == vp->sw_frame.get()) {
            // 转换后的帧PTS应该已经正确设置，但为了保险，也检查原始帧
            if (vp->frame->best_effort_timestamp != AVNoPtsValue) {
                pts_source_frame = vp->frame.get();
            }
        }
        
        // 尝试多种方式获取PTS
        int64_t pts_value = AVNoPtsValue;
        if (pts_source_frame->best_effort_timestamp != AVNoPtsValue) {
            pts_value = pts_source_frame->best_effort_timestamp;
        } else if (pts_source_frame->pts != AVNoPtsValue) {
            pts_value = pts_source_frame->pts;
        } else if (vp->frame->best_effort_timestamp != AVNoPtsValue) {
            // 如果转换后的帧没有PTS，尝试从原始硬件帧获取
            pts_value = vp->frame->best_effort_timestamp;
        } else if (vp->frame->pts != AVNoPtsValue) {
            pts_value = vp->frame->pts;
        }
        
        if (pts_value != AVNoPtsValue) {
            current_pts = duration_cast<nanoseconds>(seconds_d64{av_q2d(stream_->time_base) * static_cast<double>(pts_value)});
            vp->pts = current_pts;
        } else {
            // 如果仍然没有PTS，使用递增的时间戳（基于帧率）
            // 这对于某些没有PTS的视频（如某些8K视频）很重要
            if (vp->pts == nanoseconds::min()) {
                // 首次没有PTS的帧，从0开始
                current_pts = nanoseconds::zero();
            } else {
                // 后续帧，基于帧率递增
                auto frame_delay = av_q2d(codecctx_->time_base);
                int repeat_pict = decoded_frame->repeat_pict;
                if (decoded_frame == vp->yuv420p_frame.get() || decoded_frame == vp->sw_frame.get()) {
                    if (vp->frame->repeat_pict >= 0) {
                        repeat_pict = vp->frame->repeat_pict;
                    }
                }
                frame_delay += repeat_pict * (frame_delay * 0.5);
                current_pts = vp->pts + duration_cast<nanoseconds>(seconds_d64{frame_delay});
            }
            vp->pts = current_pts;
            logger->debug("Video frame has no PTS, using estimated PTS: {}ns ({}s)", current_pts.count(), current_pts.count() / 1000000000.0);
        }


        // Update the video clock to the next expected pts.
        auto frame_delay = av_q2d(codecctx_->time_base);
        // 使用正确的帧获取repeat_pict
        int repeat_pict = decoded_frame->repeat_pict;
        if (decoded_frame == vp->yuv420p_frame.get() || decoded_frame == vp->sw_frame.get()) {
            // 如果使用转换后的帧，repeat_pict应该已经传递，但为了保险也检查原始帧
            if (vp->frame->repeat_pict >= 0) {
                repeat_pict = vp->frame->repeat_pict;
            }
        }
        frame_delay += repeat_pict * (frame_delay * 0.5);
        current_pts += duration_cast<nanoseconds>(seconds_d64{frame_delay});

        // Put the frame in the queue to be loaded into a texture and displayed
        // by the rendering thread.
        write_idx = (write_idx + 1) % pictQ_.size();
        pictQWrite_.store(write_idx, std::memory_order_release);
        
        // 预加载检查：计算队列中的帧数，达到目标帧数后标记预加载完成
        if (!preloadCompleted_.load(std::memory_order_acquire)) {
            size_t read_idx = pictQRead_.load(std::memory_order_acquire);
            size_t queueSize = pictQ_.size();
            // 计算环形缓冲区中的帧数
            size_t framesInQueue = (write_idx >= read_idx) ? (write_idx - read_idx) : (queueSize - read_idx + write_idx);
            
            if (framesInQueue >= PRELOAD_TARGET_FRAMES) {
                preloadCompleted_.store(true, std::memory_order_release);
                logger->info("Video preload completed: {} frames buffered (target: {})", framesInQueue, PRELOAD_TARGET_FRAMES);
            } else {
                logger->debug("Video preloading: {}/{} frames buffered", framesInQueue, PRELOAD_TARGET_FRAMES);
            }
        }

        // Send a packet for next receive.
        packets_.sendTo(codecctx_.get());

        if (write_idx == pictQRead_.load(std::memory_order_acquire)) {
            logger->debug("wait until have space entered...write_idx: {}", write_idx);
            // Wait until we have space, 若写idx追上picQRead,则说明队列已满，
            std::unique_lock<std::mutex> lck(pictQMutex_);
            
            // 死锁检测和恢复机制
            static int deadlockCounter = 0;
            static int emptyQueueDeadlockCount = 0;  // 连续空队列死锁计数器
            const int maxDeadlockAttempts = 10;  // 最多尝试10次，共1秒
            const int framesToSkipOnDeadlock = 3; // 死锁时跳过的帧数
            
            while (write_idx == pictQRead_.load(std::memory_order_acquire) && !movie_.quit_.load(std::memory_order_relaxed)
                   && !isQuit.load(std::memory_order_relaxed)) {
                logger->debug("write_idx==picQRead.load && movie.isNotQuit, picQRead:{},picQWrite_:{}", pictQRead_.load(), pictQWrite_.load());
                
                // Producer等待，直到consumer线程通知可继续写，再解锁
                // 使用更短的超时时间，以便更快检测死锁
                if (!pictQCond_.wait_for(lck, std::chrono::milliseconds(100), 
                    [this, write_idx]() { 
                        return write_idx != pictQRead_.load(std::memory_order_acquire) || 
                               movie_.quit_.load(std::memory_order_relaxed) || 
                               isQuit.load(std::memory_order_relaxed); 
                    })) {
                    deadlockCounter++;
                    logger->warn("Video decoder timeout waiting for consumer, attempt {}/{}", deadlockCounter, maxDeadlockAttempts);
                    
                    // 超时后检查是否应该退出
                    if (movie_.quit_.load(std::memory_order_relaxed) || isQuit.load(std::memory_order_relaxed)) {
                        break;
                    }
                    
                    // 如果多次超时，说明渲染线程可能已停止消费，主动丢弃一些帧来恢复
                    // 降低阈值，更快响应（从10次降低到5次，即500ms）
                    if (deadlockCounter >= 5) {
                        logger->warn("Deadlock detected! Rendering thread seems stuck (attempt {}/5). Forcing frame skip to recover.", deadlockCounter);
                        size_t currentRead = pictQRead_.load(std::memory_order_acquire);
                        size_t currentWrite = pictQWrite_.load(std::memory_order_acquire);
                        size_t queueSize = pictQ_.size();
                        const int framesToSkipOnDeadlock = 1; // 死锁时每次只跳过1帧，更温和的恢复
                        
                        // 计算环形缓冲区中的有效数据量
                        size_t distance = (currentWrite >= currentRead) ? 
                                         (currentWrite - currentRead) : 
                                         (queueSize - currentRead + currentWrite);
                        
                        if (distance == 0) {
                            // 队列满（write_idx == read_idx），说明渲染线程没有消费帧
                            // 检查是否应该退出
                            if (movie_.quit_.load(std::memory_order_relaxed) || isQuit.load(std::memory_order_relaxed)) {
                                logger->info("Queue is full and quit flag is set, exiting decode loop");
                                break;  // 退出等待循环，最终会退出解码循环
                            }
                            
                            // 检查是否正在Seeking，如果是则跳过死锁恢复，等待Seeking完成
                            // Seeking时队列会被clear()清空，死锁恢复可能导致队列状态不一致
                            if (movie_.IsSeeking()) {
                                logger->debug("Seeking in progress, skipping deadlock recovery to avoid queue state inconsistency");
                                // 重置计数器，避免在Seeking过程中累积
                                emptyQueueDeadlockCount = 0;
                                deadlockCounter = 0;
                                // 等待Seeking完成，使用更短的超时时间
                                pictQCond_.notify_all();
                                std::this_thread::sleep_for(std::chrono::milliseconds(50));
                                continue;  // 继续等待循环
                            }
                            
                            // 队列满时，需要推进read_idx来丢弃旧帧，为新帧腾出空间
                            // 但不要立即推进，先等待几次，给渲染线程机会
                            emptyQueueDeadlockCount++;
                            
                            // 降低等待次数：从20次（2秒）降低到10次（1秒），更快恢复
                            if (emptyQueueDeadlockCount >= 10) {
                                // 连续10次队列满且死锁（1秒），说明渲染线程可能真的卡住了
                                // 此时强制推进read_idx，丢弃旧帧，让解码继续
                                logger->error("Queue full deadlock occurred {} times consecutively ({}ms), forcing read_idx advance to recover", 
                                            emptyQueueDeadlockCount, emptyQueueDeadlockCount * 100);
                                
                                // 强制推进read_idx，丢弃最旧的帧
                                // 推进1个位置，为新帧腾出空间
                                size_t newRead = (currentRead + 1) % queueSize;
                                pictQRead_.store(newRead, std::memory_order_release);
                                logger->warn("Forced advance read_idx: {} -> {} to recover from deadlock (queue full)", 
                                           currentRead, newRead);
                                
                                // 重置计数器，给一次机会
                                emptyQueueDeadlockCount = 0;
                                deadlockCounter = 0;  // 重置死锁计数器
                                
                                // 通知等待的线程
                                pictQCond_.notify_all();
                                
                                // 短暂休眠，避免快速循环
                                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                                
                                // 退出等待循环，继续解码
                                break;
                            } else {
                                // 队列满时，等待更长时间，不要立即推进read_idx
                                // 这可以避免在正常播放时误触发跳帧
                                if (emptyQueueDeadlockCount % 3 == 0) {
                                    logger->debug("Queue full during deadlock, waiting for consumer (attempt {}/10)", 
                                                 emptyQueueDeadlockCount);
                                }
                                
                                // 不推进read_idx，只是等待和通知
                                // 通知等待的线程（虽然可能没有效果，但至少尝试）
                                pictQCond_.notify_all();
                                
                                // 更长的休眠，给渲染线程更多时间
                                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                                
                                // 继续等待循环
                                continue;
                            }
                        } else {
                            // 队列不为空，可以跳帧
                            // 重置连续空队列死锁计数器
                            emptyQueueDeadlockCount = 0;
                            
                            // 逐帧跳，跳过 framesToSkipOnDeadlock 帧
                            size_t newRead = (currentRead + framesToSkipOnDeadlock) % queueSize;
                            
                            // 关键修复：确保在环形缓冲区中，跳帧后 newRead 仍在有效数据范围内
                            // 有效数据范围：如果 currentWrite >= currentRead，在 [currentRead, currentWrite) 之间
                            //              如果 currentWrite < currentRead，在 [currentRead, queueSize) 和 [0, currentWrite) 之间
                            bool isValid = false;
                            if (currentWrite >= currentRead) {
                                // 正常情况：newRead 应该在 [currentRead, currentWrite) 之间
                                isValid = (newRead >= currentRead && newRead < currentWrite);
                            } else {
                                // 环形情况：newRead 应该在 [currentRead, queueSize) 或 [0, currentWrite) 之间
                                isValid = (newRead >= currentRead || newRead < currentWrite);
                            }
                            
                            if (!isValid) {
                                // 如果跳帧后超出了有效范围，限制到 write_idx - 1（最新有效帧）
                                if (currentWrite > 0) {
                                    newRead = currentWrite - 1;
                                } else {
                                    newRead = queueSize - 1;
                                }
                                logger->warn("Skip would exceed valid range, clamping to latest frame: {}", newRead);
                            }
                            
                        pictQRead_.store(newRead, std::memory_order_release);
                            logger->warn("Forced skip frames: {} -> {} (write_idx={}, queue_size={})", 
                                        currentRead, newRead, currentWrite, queueSize);
                        }
                        
                        deadlockCounter = 0;  // 重置计数器
                        emptyQueueDeadlockCount = 0;  // 重置空队列计数器
                        
                        // 短暂休眠，避免快速循环导致CPU占用过高
                        std::this_thread::sleep_for(std::chrono::milliseconds(10));
                        
                        break;  // 退出等待循环，继续解码
                    }
                    // 如果超时但未达到最大尝试次数，继续等待
                    continue;
                } else {
                    // 成功获取空间，重置死锁计数器
                    if (deadlockCounter > 0) {
                        logger->info("Deadlock resolved after {} attempts", deadlockCounter);
                        deadlockCounter = 0;
                    }
                    // 重置连续空队列死锁计数器（成功获取空间说明队列恢复正常）
                    emptyQueueDeadlockCount = 0;
                    break;  // 成功获取空间，退出循环
                }
            }
            logger->debug(" wait until have space leaved.");
        }
        logger->debug("video.mainLoop continue....");
    }
    logger->info("Video.run, leaving...");
}

void Video::SetPause(bool isPause)
{
    pause(isPause);
}

int Video::pause(bool isPause)
{
    this->isPause.store(isPause);
    return 0;
}

bool Video::PictureRingBufferIsEmpty()
{
    size_t read_idx = pictQRead_.load(std::memory_order_relaxed);
    size_t next_idx = (read_idx + 1) % pictQ_.size();
    if (next_idx == pictQWrite_.load(std::memory_order_acquire)) {
        return true;
    }
    return false;
}

void Video::waitForPreload(int timeoutMs)
{
    if (preloadCompleted_.load(std::memory_order_acquire)) {
        return;  // 已经完成预加载
    }
    
    logger->info("Waiting for video preload to complete (timeout: {}ms)...", timeoutMs);
    auto startTime = std::chrono::steady_clock::now();
    
    while (!preloadCompleted_.load(std::memory_order_acquire)) {
        // 检查超时
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - startTime).count();
        
        if (elapsed >= timeoutMs) {
            logger->warn("Video preload timeout after {}ms, proceeding anyway", elapsed);
            // 超时后强制标记为完成，避免无限等待
            preloadCompleted_.store(true, std::memory_order_release);
            break;
        }
        
        // 检查是否应该退出（视频已停止）
        if (movie_.quit_.load(std::memory_order_relaxed) || isQuit.load(std::memory_order_relaxed)) {
            logger->info("Video stopped during preload wait");
            break;
        }
        
        // 短暂休眠，避免CPU占用过高
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - startTime).count();
    logger->info("Video preload wait completed in {}ms", elapsed);
}

int Video::stop()
{
    logger->debug("video stop started.");
    clear();

    isQuit.store(true);
    pictQCond_.notify_one();
    logger->debug("after set isQuit is true");

#if 0
    std::lock_guard<std::mutex> guard(dispPtsMutex_);
    displayPts_ = nanoseconds{ 0 };
    displayPtsTime_ = microseconds::min();

    if (codecctx_) {
        avcodec_close(codecctx_.get());
        codecctx_.release();
    }
#endif

    logger->debug("video.stop ended.");

    return 0;
}

void Video::clear()
{
    pictQMutex_.lock();
    packets_.Reset();
    if (codecctx_) {
        avcodec_flush_buffers(codecctx_.get());
    }
    
    // 重置预加载标志
    preloadCompleted_.store(false, std::memory_order_release);

    // 释放所有Picture中的sw_frame和yuv420p_frame，避免内存泄漏
    // 注意：frame对象本身不释放，只释放缓冲区和辅助帧
    std::for_each(pictQ_.begin(), pictQ_.end(), [](Picture &pic) {
        // 先unref释放缓冲区，然后reset释放对象
        if (pic.sw_frame) {
            av_frame_unref(pic.sw_frame.get());
            pic.sw_frame.reset();      // 释放软件帧对象
        }
        if (pic.yuv420p_frame) {
            av_frame_unref(pic.yuv420p_frame.get());
            pic.yuv420p_frame.reset();  // 释放YUV420P帧对象
        }
        // frame对象保留，只释放其缓冲区（在start()中会unref）
        if (pic.frame) {
            av_frame_unref(pic.frame.get());
        }
        pic.pts = nanoseconds::min();
    });
    
    // 注意：健康检查时间戳的更新应该在Movie::startDemux()的Seeking逻辑中处理
    // 这里不清除健康检查时间戳，因为clear()可能在非Seeking场景下调用
    
    // 注意：静态的SwsContext会在下次start()时重新创建，这里不需要释放
    // 因为它是静态的，会在程序结束时自动释放

    // 重置队列状态，确保新视频开始时队列是干净的
    // 重要：同时重置read_idx和write_idx，确保队列状态一致
    // 初始状态：read_idx=0, write_idx=1（write在read+1的位置，表示队列为空）
    pictQRead_.store(0, std::memory_order_release);
    pictQWrite_.store(1, std::memory_order_release);
    logger->info("Video::clear, picQRead set to 0, pictQWrite set to 1, frames released");
    pictQMutex_.unlock();
    
    // 在锁外通知所有等待的线程，确保它们能立即响应
    // 这对于视频切换时很重要，可以避免新视频开始时仍然受到旧视频死锁状态的影响
    pictQCond_.notify_all();
    logger->info("Video::clear, leaving...");
}

std::pair<AVFrame *, int64_t> Video::currentFrame()
{
    if (movie_.quit_.load(std::memory_order_relaxed)) {
        logger->info("currentFrame, quit is true, return nullptr");
        return {nullptr, 0};
    }
    if (isQuit.load(std::memory_order_relaxed)) {
        logger->info("currentFrame, isQuit, return null");
        return {nullptr, 0};
    }

    size_t read_idx = pictQRead_.load(std::memory_order_relaxed);
    size_t write_idx = pictQWrite_.load(std::memory_order_relaxed);
    logger->debug("read_dix:{}, write_idx:{}", read_idx, write_idx);

    // 注意：在环形缓冲区中，read_idx 和 write_idx 的各种关系都是正常的
    // 只有在死锁检测触发跳帧时，才会进行边界检查
    // 这里不做任何异常检测，避免误判正常的环形缓冲区状态

    Picture *vp = &pictQ_[read_idx];
    
    // 如果使用硬件解码且已转换，优先使用转换后的帧
    // 优先级：yuv420p_frame > sw_frame > frame
    AVFrame *frame_to_use = vp->frame.get();
    if (vp->yuv420p_frame && vp->yuv420p_frame->format != AV_PIX_FMT_NONE) {
        frame_to_use = vp->yuv420p_frame.get();
    } else if (vp->sw_frame && vp->sw_frame->format != AV_PIX_FMT_NONE) {
        frame_to_use = vp->sw_frame.get();
    }

    if (isPause.load(std::memory_order_acquire)) {
        logger->debug("video.currentFrame,isPause=true");
        return {frame_to_use, vp->pts.count()};
    }

    auto clocktime = movie_.getMasterClock();

    // 获取视频时钟作为fallback，用于检测音频时钟是否异常
    auto videoClock = getClock();
    
    logger->debug("curentFrame, get vp:{}, clockTime:{}, videoClock:{}", 
                  vp->pts.count(), clocktime.count(), videoClock.count());
    size_t current_idx = read_idx;
    
    // 注意：移除了read_idx超时强制推进机制
    // 原因：这个机制和死锁恢复机制冲突，可能导致误判和跳帧
    // 死锁恢复机制已经在解码线程中处理，不需要在渲染线程中再次处理
    
    // 检测音频是否已结束（优先检测）
    bool audioFinished = movie_.audio_.audioFinished_.load(std::memory_order_acquire);
    
    // 检测音频时钟是否严重落后（超过3秒）
    // 如果使用音频作为主时钟，且音频时钟严重落后于视频帧PTS，说明音频时钟异常
    static bool audioClockStuck = false;
    static std::chrono::steady_clock::time_point lastClockCheckTime = std::chrono::steady_clock::now();
    static nanoseconds lastMasterClock{0};
    static int clockStuckCounter = 0;
    
    const nanoseconds MAX_CLOCK_LAG = nanoseconds{3LL * 1000000000};  // 3秒
    auto now = std::chrono::steady_clock::now();
    
    // 如果音频已结束，直接使用视频时钟
    if (audioFinished) {
        if (!audioClockStuck) {
            audioClockStuck = true;
            logger->info("Audio finished, switching to video clock for synchronization");
        }
    } else {
        // 检查主时钟是否更新（每100ms检查一次）
        auto timeSinceLastCheck = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastClockCheckTime).count();
        if (timeSinceLastCheck > 100) {
            // 初始化或检查时钟是否卡住
            if (lastMasterClock.count() == 0) {
                // 首次调用，初始化
                lastMasterClock = clocktime;
                lastClockCheckTime = now;
            } else if (lastMasterClock == clocktime && vp->pts != nanoseconds::min() && vp->pts > clocktime) {
                // 主时钟没有更新，且视频帧PTS大于主时钟
                nanoseconds lag = vp->pts - clocktime;
                if (lag > MAX_CLOCK_LAG) {
                    clockStuckCounter++;
                    if (clockStuckCounter >= 3) {
                        // 连续3次检测到时钟卡住，启用fallback
                        audioClockStuck = true;
                        logger->warn("Master clock appears stuck: clock={}ns ({}s), frame_pts={}ns ({}s), lag={}ns ({}s). "
                                   "Using video clock as fallback to prevent deadlock.",
                                   clocktime.count(), clocktime.count() / 1000000000.0,
                                   vp->pts.count(), vp->pts.count() / 1000000000.0,
                                   lag.count(), lag.count() / 1000000000.0);
                    }
                }
            } else {
                // 时钟正常更新，重置计数器
                if (clockStuckCounter > 0) {
                    logger->info("Master clock recovered, resetting stuck counter");
                }
                clockStuckCounter = 0;
                if (audioClockStuck && clocktime > lastMasterClock) {
                    // 时钟恢复，检查是否已经追上
                    if (vp->pts != nanoseconds::min() && clocktime >= vp->pts - MAX_CLOCK_LAG) {
                        audioClockStuck = false;
                        logger->info("Master clock recovered, resuming normal sync");
                    }
                }
            }
            lastMasterClock = clocktime;
            lastClockCheckTime = now;
        }
    }
    
    // 如果音频时钟卡住或音频已结束，使用视频时钟作为fallback
    nanoseconds effectiveClock = audioClockStuck ? videoClock : clocktime;
    
    // 检查当前帧是否过期（在判断帧有效性之前，因为即使帧有效也可能过期）
    nanoseconds currentFrameLag = (vp->pts != nanoseconds::min()) ? (effectiveClock - vp->pts) : nanoseconds{0};
    const nanoseconds MAX_FRAME_LAG = nanoseconds{1LL * 1000000000};  // 1秒
    bool currentFrameExpired = (vp->pts != nanoseconds::min()) && (currentFrameLag > MAX_FRAME_LAG);
    
    // 如果当前帧无效，尝试查找下一个有效帧
    if (vp->pts == nanoseconds::min()) {
        size_t next_idx = (read_idx + 1) % pictQ_.size();
        // 如果队列不为空，尝试前进到下一个有效帧
        if (next_idx != pictQWrite_.load(std::memory_order_acquire)) {
            Picture *nextvp = &pictQ_[next_idx];
            if (nextvp->pts != nanoseconds::min()) {
                current_idx = next_idx;
                vp = nextvp;
                logger->debug("Skip invalid frame at {}, move to next valid frame at {}", read_idx, current_idx);
            }
        }
    } else {
        // 正常情况：根据时钟选择正确的帧
        // currentFrameExpired 和 currentFrameLag 已经在上面定义
        
        if (currentFrameExpired) {
            logger->warn("Current frame expired (lag={}s), forcing advance: current_pts={}ns ({}s), clock={}ns ({}s)",
                        currentFrameLag.count() / 1000000000.0, 
                        vp->pts.count(), vp->pts.count() / 1000000000.0,
                        effectiveClock.count(), effectiveClock.count() / 1000000000.0);
        }
        
        while (1) {
            size_t next_idx = (current_idx + 1) % pictQ_.size();

            if (next_idx == pictQWrite_.load(std::memory_order_acquire)) {
                logger->debug("###### next_idx == picQWrite, break");
                break;
            }
            Picture *nextvp = &pictQ_[next_idx];
            
            // 如果下一帧无效，停止前进
            if (nextvp->pts == nanoseconds::min()) {
                logger->debug("next frame at {} is invalid, stop advancing", next_idx);
                break;
            }

            // 使用有效时钟进行比较
            if (effectiveClock < nextvp->pts) {
                // 如果当前帧已过期，强制前进到下一帧，即使下一帧PTS也小于时钟时间
                // 这样可以避免一直停留在过期帧上，导致画面停顿
                if (currentFrameExpired) {
                    // 检查下一帧是否也比时钟时间小很多，如果是，继续前进
                    nanoseconds nextFrameLag = effectiveClock - nextvp->pts;
                    if (nextFrameLag > MAX_FRAME_LAG) {
                        // 下一帧也过期了，继续前进
                        logger->debug("Next frame also expired (lag={}s), continuing advance: next_pts={}ns",
                                    nextFrameLag.count() / 1000000000.0, nextvp->pts.count());
                        current_idx = next_idx;
                        vp = nextvp;
                        continue;
                    } else {
                        // 下一帧虽然PTS小于时钟时间，但差距不大，使用它
                        logger->debug("Next frame lag acceptable (lag={}s), using it: next_pts={}ns",
                                    nextFrameLag.count() / 1000000000.0, nextvp->pts.count());
                        current_idx = next_idx;
                        break;
                    }
                }
                
                // 如果音频时钟卡住或音频已结束，且视频时钟也落后，允许适当推进（最多推进到视频时钟位置）
                if ((audioClockStuck || audioFinished) && videoClock < nextvp->pts) {
                    // 计算视频时钟与下一帧的差距
                    nanoseconds videoLag = nextvp->pts - videoClock;
                    // 如果差距小于0.5秒，允许推进（逐步追赶，更保守）
                    // 降低阈值，避免跳帧过多导致音视频不同步
                    if (videoLag < nanoseconds{500LL * 1000000}) {
                        logger->debug("Audio clock stuck/finished, allowing frame advance: videoClock={}ns, next_pts={}ns, lag={}ns",
                                    videoClock.count(), nextvp->pts.count(), videoLag.count());
                        current_idx = next_idx;
                        continue;
                    } else {
                        logger->debug("Audio clock stuck/finished, but video lag too large: videoClock={}ns, next_pts={}ns, lag={}ns, break",
                                    videoClock.count(), nextvp->pts.count(), videoLag.count());
                        break;
                    }
                } else {
                    logger->debug("  clocktime:{} <nextvp->pts:{}, break ", effectiveClock.count(), nextvp->pts.count());
                    break;
                }
            }

            current_idx = next_idx;
        }
    }

    if (current_idx != read_idx) {
        vp = &pictQ_[current_idx];
        // 更新 frame_to_use（优先级：yuv420p_frame > sw_frame > frame）
        frame_to_use = vp->frame.get();
        if (vp->yuv420p_frame && vp->yuv420p_frame->format != AV_PIX_FMT_NONE) {
            frame_to_use = vp->yuv420p_frame.get();
        } else if (vp->sw_frame && vp->sw_frame->format != AV_PIX_FMT_NONE) {
            frame_to_use = vp->sw_frame.get();
        }
        pictQRead_.store(current_idx, std::memory_order_release);
        //packet_queue头尾没有追上，即还有空间时，则unlock并通知接收线程继续接收
        std::unique_lock<std::mutex>{pictQMutex_}.unlock();
        pictQCond_.notify_one();
    } else {
        // current_idx == read_idx，没有前进
        // 检查是否需要强制推进read_idx的情况：
        
        // 情况1：当前帧无效，且队列不为空
        if (vp->pts == nanoseconds::min() && read_idx != pictQWrite_.load(std::memory_order_acquire)) {
            size_t next_idx = (read_idx + 1) % pictQ_.size();
            if (next_idx != pictQWrite_.load(std::memory_order_acquire)) {
                logger->debug("Current frame invalid but queue has data, advance read_idx: {} -> {} to prevent deadlock", 
                             read_idx, next_idx);
                pictQRead_.store(next_idx, std::memory_order_release);
                std::unique_lock<std::mutex>{pictQMutex_}.unlock();
                pictQCond_.notify_one();
                // 返回下一帧（可能也无效，但至少推进了队列）
                vp = &pictQ_[next_idx];
                // 更新 frame_to_use（优先级：yuv420p_frame > sw_frame > frame）
                frame_to_use = vp->frame.get();
                if (vp->yuv420p_frame && vp->yuv420p_frame->format != AV_PIX_FMT_NONE) {
                    frame_to_use = vp->yuv420p_frame.get();
                } else if (vp->sw_frame && vp->sw_frame->format != AV_PIX_FMT_NONE) {
                    frame_to_use = vp->sw_frame.get();
                }
            }
        }
        // 情况2：当前帧过期且队列为空（无法前进到下一帧）
        // 这种情况下，需要释放当前帧的位置，让解码线程可以写入新帧
        else if (currentFrameExpired && read_idx == (pictQWrite_.load(std::memory_order_acquire) - 1 + pictQ_.size()) % pictQ_.size()) {
            // 队列为空（read_idx == write_idx - 1），且帧过期
            // 推进read_idx，释放当前帧位置，让解码线程可以写入新帧
            size_t next_idx = (read_idx + 1) % pictQ_.size();
            logger->warn("Frame expired and queue empty, forcing advance read_idx: {} -> {} to unblock decoder. "
                        "Current frame PTS: {}ns ({}s), clock: {}ns ({}s), lag: {}s",
                        read_idx, next_idx,
                        vp->pts.count(), vp->pts.count() / 1000000000.0,
                        effectiveClock.count(), effectiveClock.count() / 1000000000.0,
                        currentFrameLag.count() / 1000000000.0);
            pictQRead_.store(next_idx, std::memory_order_release);
            std::unique_lock<std::mutex>{pictQMutex_}.unlock();
            pictQCond_.notify_one();
            // 更新vp指向新位置（虽然可能还没有数据）
            vp = &pictQ_[next_idx];
            // 更新 frame_to_use
            frame_to_use = vp->frame.get();
            if (vp->yuv420p_frame && vp->yuv420p_frame->format != AV_PIX_FMT_NONE) {
                frame_to_use = vp->yuv420p_frame.get();
            } else if (vp->sw_frame && vp->sw_frame->format != AV_PIX_FMT_NONE) {
                frame_to_use = vp->sw_frame.get();
            }
        }
    }

    if (vp->pts == nanoseconds::min()) {
        logger->debug("  vp-pts == nanoseconds::min, return nullptr");
        return {nullptr, 0};
    }

    if (current_idx != read_idx) {
        std::lock_guard<std::mutex> lck(dispPtsMutex_);
        displayPts_ = vp->pts;
        displayPtsTime_ = get_avtime();
    }

    logger->debug(
        "get vp with current_idx:{}, vp->pts:{},equals (seconds):{},read_idx:{}, write_idx:{}",
        current_idx,
        vp->pts.count(),
        vp->pts.count() / 1000 / 1000 / 1000,
        read_idx,
        write_idx);

#ifdef INSPECT_DECODED_FRAME
    static int frameCounter = 0;
    frameCounter += 1;
    if (frameCounter == 10) {
        //pgm_save(vp->frame.get());
    }
#endif
    
    return {frame_to_use, vp->pts.count()};
}

nanoseconds Video::getClock()
{
    std::lock_guard<std::mutex> _{dispPtsMutex_};
    if (displayPtsTime_ == microseconds::min()) {
        displayPtsTime_ = get_avtime();
        return displayPts_;
    }
    auto delta = get_avtime() - displayPtsTime_;
    return displayPts_ + delta;
}
