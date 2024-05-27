extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
}

int main() {
    // 初始化 FFmpeg
    av_register_all();

    // 打开视频文件
    AVFormatContext* formatContext = avformat_alloc_context();
    if (avformat_open_input(&formatContext, "input.mp4", NULL, NULL) != 0) {
        // 错误处理
        return -1;
    }

    // 获取流信息
    if (avformat_find_stream_info(formatContext, NULL) < 0) {
        // 错误处理
        return -1;
    }

    // 查找视频流
    int videoStreamIndex = -1;
    for (int i = 0; i < formatContext->nb_streams; i++) {
        if (formatContext->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            videoStreamIndex = i;
            break;
        }
    }
    if (videoStreamIndex == -1) {
        // 找不到视频流
        return -1;
    }

    // 获取解码器
    AVCodecParameters* codecParameters = formatContext->streams[videoStreamIndex]->codecpar;
    AVCodec* codec = avcodec_find_decoder(codecParameters->codec_id);
    if (codec == NULL) {
        // 找不到解码器
        return -1;
    }

    // 打开解码器
    AVCodecContext* codecContext = avcodec_alloc_context3(codec);
    if (avcodec_parameters_to_context(codecContext, codecParameters) < 0) {
        // 错误处理
        return -1;
    }
    if (avcodec_open2(codecContext, codec, NULL) < 0) {
        // 错误处理
        return -1;
    }

    // 分配内存用于解码后的帧
    AVFrame* frame = av_frame_alloc();

    // 创建转换上下文
    struct SwsContext* swsContext = sws_getContext(codecContext->width, codecContext->height, codecContext->pix_fmt,
                                                  codecContext->width, codecContext->height, AV_PIX_FMT_BGR24,
                                                  SWS_BILINEAR, NULL, NULL, NULL);

    // 读取视频帧
    AVPacket packet;
    while (av_read_frame(formatContext, &packet) >= 0) {
        if (packet.stream_index == videoStreamIndex) {
            // 解码帧
            int response = avcodec_send_packet(codecContext, &packet);
            if (response < 0 || response == AVERROR(EAGAIN) || response == AVERROR_EOF) {
                // 错误处理
                break;
            }
            response = avcodec_receive_frame(codecContext, frame);
            if (response < 0 || response == AVERROR(EAGAIN) || response == AVERROR_EOF) {
                // 错误处理
                break;
            }

            // 将解码后的帧转换为 BGR 格式
            AVFrame* frameRGB = av_frame_alloc();
            int numBytes = av_image_get_buffer_size(AV_PIX_FMT_BGR24, codecContext->width, codecContext->height, 1);
            uint8_t* buffer = (uint8_t*)av_malloc(numBytes * sizeof(uint8_t));
            av_image_fill_arrays(frameRGB->data, frameRGB->linesize, buffer, AV_PIX_FMT_BGR24, codecContext->width, codecContext->height, 1);
            sws_scale(swsContext, (uint8_t const* const*)frame->data, frame->linesize, 0, codecContext->height,
                      frameRGB->data, frameRGB->linesize);

            // 在这里可以对 frameRGB->data[0] 进行任何 OpenCV 的图像处理操作
            // 例如，你可以使用 cv::Mat 构造函数来创建一个 OpenCV Mat 对象
            // cv::Mat cvFrame(codecContext->height, codecContext->width, CV_8UC3, frameRGB->data[0]);

            // 释放临时帧资源
            av_freep(&frameRGB->data[0]);
            av_frame_free(&frameRGB);
        }
        av_packet_unref(&packet);
    }

    // 释放资源
    avformat_close_input(&formatContext);
    avcodec_free_context(&codecContext);
    av_frame_free(&frame);
    sws_freeContext(swsContext);

    return 0;
}
