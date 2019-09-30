#include <iostream>
#include <vector>
#include <algorithm>
using namespace std;


extern "C" {
#include "libavcodec/avcodec.h"
#include "libavutil/mem.h"    
#include "libavutil/imgutils.h"
#include "libavformat/avformat.h"
}

int err = -1;
char buf[1024] = { 0 };
#define IN_VEDIO_FILE  "cuc_ieschool.h264"
#define IN_AUDIO_FILE  "cuc_ieschool.mp3"
#define OUT_FILE "cuc_ieschool.mp4"

#define  ptr_check(x) \
            do {\
                if (!x){\
                    printf("operator fail"); \
                    return -1; \
                }\
            }while(0) 

#define void_handle(x) \
            if ((err = (x)) < 0) {\
                memset(buf, 0, 1024); \
                av_strerror(err, buf, 1024); \
                printf("err msg = %s", buf); \
                return -1; \
            }

#define ret_handle(x, r) \
            if ((r = (x)) < 0) {\
                memset(buf, 0, 1024); \
                av_strerror(r, buf, 1024); \
                printf("err msg = %s", buf); \
                return -1; \
            }   

int main()
{
    AVFormatContext *in_vedio_ctx = NULL, *in_audio_ctx = NULL, *out_ctx = NULL;
    vector<int> stream_indexs;
    bool isVedio = true;

    //h264 info
    void_handle(avformat_open_input(&in_vedio_ctx, IN_VEDIO_FILE, NULL, NULL));
    void_handle(avformat_find_stream_info(in_vedio_ctx, NULL));

    //mp3 info
    void_handle(avformat_open_input(&in_audio_ctx, IN_AUDIO_FILE, NULL, NULL));
    void_handle(avformat_find_stream_info(in_audio_ctx, NULL));

    //mp4 init
    void_handle(avformat_alloc_output_context2(&out_ctx, NULL, NULL, OUT_FILE));

    //get stream
    int vedio_stream_index = -1, audio_stream_index = -1;
    ret_handle(av_find_best_stream(in_vedio_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0), vedio_stream_index);
    ret_handle(av_find_best_stream(in_audio_ctx, AVMEDIA_TYPE_AUDIO, -1, -1, NULL, 0), audio_stream_index);

    stream_indexs.push_back(vedio_stream_index);
    stream_indexs.push_back(audio_stream_index);

    //Ϊ��������Ĵ�����
    for_each(stream_indexs.begin(), stream_indexs.end(), [&](int index) {
        AVStream *out_stream = avformat_new_stream(out_ctx, NULL);
        ptr_check(out_stream);
        void_handle(avcodec_parameters_from_context(out_stream->codecpar,
            isVedio ? in_vedio_ctx->streams[index]->codec : in_audio_ctx->streams[index]->codec));

        isVedio = false;
        if (out_ctx->oformat->flags & AVFMT_GLOBALHEADER) {
            out_stream->codec->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
        }

    });

    //������ļ�
    if (!(out_ctx->oformat->flags & AVFMT_NOFILE)) {
        void_handle(avio_open(&out_ctx->pb, OUT_FILE, AVIO_FLAG_READ_WRITE));
    }

    //д���ļ�ͷ
    avformat_write_header(out_ctx, NULL);

    //��ʼд�ļ�
    AVPacket *packet = av_packet_alloc();
    int64_t ts_a = 0, ts_b = 0;
    int64_t *ts_p = NULL;
    int out_stream_index = -1;
    AVFormatContext *cur_ctx = NULL;
    AVStream *cur_stream = NULL;
    int frame = 0;

    while (1) {

        //ָ����ǰ��ȡ��Ƶ��������
        if (av_compare_ts(ts_a, in_vedio_ctx->streams[vedio_stream_index]->time_base, ts_b, in_audio_ctx->streams[audio_stream_index]->time_base) <= 0) {
            cur_ctx = in_vedio_ctx;
            ts_p = &ts_a;
            cur_stream = in_vedio_ctx->streams[vedio_stream_index];
            out_stream_index = 0;
        } else {
            cur_ctx = in_audio_ctx;
            ts_p = &ts_b;
            cur_stream = in_audio_ctx->streams[audio_stream_index];
            out_stream_index = 1;
        }

        if (av_read_frame(cur_ctx, packet) < 0) {
            memset(buf, 0, 1024);
            av_strerror(err, buf, 1024);
            printf(buf);
            break;
        }

        //����pts dts, ����ֻ�Ǽ������ǰ�Ŀ̶�,������Ҫ�ټ���ɾ����ʱ��
        if (packet->pts == AV_NOPTS_VALUE) {

            //���������(ԭʼ)��Ƶһ֡�೤ʱ��,�����λΪ΢��
            int64_t each_frame_time = (double)AV_TIME_BASE / av_q2d(cur_stream->r_frame_rate);   

            //��ԭʼһ֡�ĳ���ʱ�����ʱ���,��ʱ��̶Ⱦ�����,����ʱ����ĵ�λΪ��.������ʱ��(each_frame_time)Ϊ΢��,�ʻ���Ҫ����AV_TIME_BASE
            packet->pts = (double)(frame++ * each_frame_time) / (double)(av_q2d(cur_stream->time_base) * AV_TIME_BASE);  
            packet->dts = packet->pts;

            //һ֡��ʱ��Ϊeach_frame_time΢��,����AV_TIME_BASE������,�ٳ���ʱ���,��ʱ��̶Ⱦͳ�����.
            packet->duration = (double)each_frame_time / (double)(av_q2d(cur_stream->time_base) * AV_TIME_BASE);
        }

        *ts_p = packet->pts;

        //����pts��Ӧ�ľ����ʱ��
        av_packet_rescale_ts(packet, cur_stream->time_base, out_ctx->streams[out_stream_index]->time_base);
        packet->stream_index = out_stream_index;
        printf("write file pts = %lld, index = %d\n", packet->pts, packet->stream_index);

        //д���ļ�
        void_handle(av_interleaved_write_frame(out_ctx, packet));

        av_packet_unref(packet);
    }


    //д�ļ�β
    void_handle(av_write_trailer(out_ctx));

    if (!(out_ctx->oformat->flags & AVFMT_NOFILE)) {
        void_handle(avio_close(out_ctx->pb));
    }
    avformat_close_input(&in_audio_ctx);
    avformat_close_input(&in_vedio_ctx);
    avformat_free_context(out_ctx);
    av_packet_free(&packet);
    return 0;
}