#ifndef _FFM_VDEC_H_
#define _FFM_VDEC_H_
#include <iostream>
#include <thread>
extern "C"{
   #include "libavformat/avformat.h"
   #include "libavcodec/avcodec.h"
   #include "libswscale/swscale.h"
}

class FFMVdec
{
public:
    FFMVdec();
    ~FFMVdec();
    int open(const char* video_path);
    int close();
    void setConfig(int w,int h,int fmt);
    bool getOpenStatus();

private:
    void send_pkt_thread();


    std::thread* m_send_pkt_thr;


    AVFormatContext *m_fm_context;
    AVPacket* m_pkt;
    AVFrame* m_frame;
    SwsContext* m_vctx;
    AVCodecContext* m_vc;
    int m_videostream;
    int m_w;
    int m_h;
    int m_fmt;
    bool m_opened;
};
#endif
