#include "ffm_vdec.h"
#include "data_center/center_data.h"
using namespace std;
FFMVdec::FFMVdec()
{
    avformat_network_init();
    m_fm_context = NULL;
    m_w = 1280;
    m_h = 720;
    m_fmt = 0;
    m_pkt = av_packet_alloc();
    m_frame = av_frame_alloc();
    m_vctx = NULL;
    m_opened = false;

}
FFMVdec::~FFMVdec()
{
    av_frame_free(&m_frame);
    av_packet_free(&m_pkt);
}
void FFMVdec::setConfig(int w,int h,int fmt)
{
    m_w = w;
    m_h = h;
    m_fmt = fmt;
    return ;
}
int FFMVdec::close()
{
    if(m_fm_context != NULL)
    {
        avformat_close_input(&m_fm_context);
    }
    m_opened = false;
    return 0;
}
bool FFMVdec::getOpenStatus()
{
    return m_opened;
}
int FFMVdec::open(const char* path)
{
    m_opened = true;
    int res = avformat_open_input(&m_fm_context,path,NULL,NULL);
    if(res != 0)
    {
        char buf[1024] = {0};
        av_strerror(res,buf,sizeof (buf));
        cout << buf << endl;
        return -1;
    }
    else
    {
        cout << "open successed!" << endl;
    }



    res = avformat_find_stream_info(m_fm_context,NULL);
    if(res == 0)
    {
        int total_time = m_fm_context->duration / AV_TIME_BASE; // s
        cout << total_time << "s" << endl;
        //av_dump_format(context,0,NULL,0);


        m_videostream = av_find_best_stream(m_fm_context,AVMEDIA_TYPE_VIDEO,-1,-1,NULL,0);
        AVStream* as = m_fm_context->streams[m_videostream];
        if(as->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
        {
            cout << "format     : " << as->codecpar->format << endl;
            cout << "codec id   : " << as->codecpar->codec_id << endl;
            cout << "width      : " << as->codecpar->width << endl;
            cout << "height     : " << as->codecpar->height << endl;
            cout << "fps        : " << as->avg_frame_rate.num/as->avg_frame_rate.den << endl;
        }

        //codec config
        const AVCodec *vcodec = avcodec_find_decoder(m_fm_context->streams[m_videostream]->codecpar->codec_id);
        if(!vcodec)
        {
            cout << "not find vcodec" << endl;
            return -1;
        }
        m_vc = avcodec_alloc_context3(vcodec);
        avcodec_parameters_to_context(m_vc,m_fm_context->streams[m_videostream]->codecpar);
        m_vc->thread_count = 8;
        //open codec
        res = avcodec_open2(m_vc,0,0);
        if(res != 0)
        {
            char buf[1024] = {0};
            av_strerror(res,buf,sizeof (buf));
            cout << buf << endl;
            return -1;
        }
    }
    send_pkt_thread();
    //recv_frame_thread();
    return 0;
}
void FFMVdec::send_pkt_thread()
{
    m_send_pkt_thr = new thread([this]{
        while(1)
        {
            static long long frame_index = 0;
            if(frame_index == 0)
            {
                int s = 150;
                long long pos = (double)s*(double)this->m_fm_context->streams[m_pkt->stream_index]->time_base.num/this->m_fm_context->streams[m_pkt->stream_index]->time_base.den;
                av_seek_frame(this->m_fm_context,m_videostream,pos,AVSEEK_FLAG_BACKWARD | AVSEEK_FLAG_FRAME);
            }
            int res = av_read_frame(this->m_fm_context,m_pkt);
            if(res != 0)
            {
                //int s = 1;
                //long long pos = (double)s*(double)context->streams[pkt->stream_index]->time_base.num/context->streams[pkt->stream_index]->time_base.den;
                //av_seek_frame(context,video_stream,pos,AVSEEK_FLAG_BACKWARD | AVSEEK_FLAG_FRAME);
                break;
            }

            //codec video

            res = avcodec_send_packet(m_vc,m_pkt);
            av_packet_unref(m_pkt);
            if(res != 0)
            {

                continue;
            }
            while(1)
            {
                int res = avcodec_receive_frame(m_vc,m_frame);
                if(res != 0) break;
                //cout <<"recv frame " <<m_frame->format<< " "<<m_frame->linesize[0] << endl;

                AVPixelFormat pixFormat;
                switch (m_frame->format) {
                case AV_PIX_FMT_YUVJ420P :
                    pixFormat = AV_PIX_FMT_YUV420P;
                    break;
                case AV_PIX_FMT_YUVJ422P  :
                    pixFormat = AV_PIX_FMT_YUV422P;
                    break;
                case AV_PIX_FMT_YUVJ444P   :
                    pixFormat = AV_PIX_FMT_YUV444P;
                    break;
                case AV_PIX_FMT_YUVJ440P :
                    pixFormat = AV_PIX_FMT_YUV440P;
                    break;
                default:
                    pixFormat = (AVPixelFormat)m_frame->format;
                    break;
                }
                //to NV12
                m_vctx = sws_getCachedContext(m_vctx,m_frame->width,m_frame->height,pixFormat,\
                                            m_w,m_h,AVPixelFormat::AV_PIX_FMT_NV12,\
                                            SWS_BILINEAR,0,0,0);
                if(!m_vctx)
                {
                    cout << "sws_getCachedContext failed" << endl;
                }
                else
                {
                    unsigned char* buffer = new unsigned char[m_w*m_h*3];
                    uint8_t* data[2] = {0};
                    data[0] = buffer;
                    data[1] = buffer + m_w*m_h;

                    int lines[2] = {0};
                    lines[0] = m_w;
                    lines[1] = m_w;

                    res = sws_scale(m_vctx,m_frame->data,m_frame->linesize,0,m_frame->height,data,lines);


                    frame_index++;
#if 0
                    if(frame_index < 3000)
                    {
                        delete[] buffer;
                        continue;
                    }
#endif
                    if(frame_index % 3 == 0)
                    {
                        if(CenterData::GetSingle()->GetSendFrameStatus() == 0)
                        {
                            break;
                        }
                        NET_INFO* info = new NET_INFO;
                        info->type = 3;

                        info->file_name = "frame_";
                        info->file_name += std::to_string(frame_index);
                        info->buffer = buffer;
                        info->buffer_length = m_w*m_h*3/2;
                        CenterData::GetSingle()->PushSendInfo(info);

                        CenterData::GetSingle()->PushFrameBuffer(buffer,frame_index);
                    }
                    else
                    {
                        delete[] buffer;
                    }


                }

            }
        }
    });
}
