#include "mp4Parse.h"
#include "IOContext.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const char h264_nalu_start_code[4] = {0x00, 0x00, 0x00, 0x01};

int main()
{
    int sps_and_pps_length=0;
    unsigned char *sps_and_pps=NULL;
	Cmp4Parse mp4Parse;
    FILE* fp = fopen("test.h264","wb");
	CIOContext ioContext;
    AVPacket pkt;
	int ret = 0;
	int avio_ctx_buffer_size = 4096;
	uint8_t *buffer = NULL, *avio_ctx_buffer = NULL;

	avio_ctx_buffer = (uint8_t *)malloc(avio_ctx_buffer_size);
    if (!avio_ctx_buffer) {
       return 0;
    }
    
    ret = mp4Parse.init_input(0,"./output.mp4",NULL);
    if (ret == -1)
    {
    	printf("init_input error,ret=%d\n",ret );
    	if (avio_ctx_buffer) {
    		free(avio_ctx_buffer);
    		avio_ctx_buffer = NULL;
    	}
    	return 0;
    }

    ioContext.Init(avio_ctx_buffer,avio_ctx_buffer_size,0,&mp4Parse,Cmp4Parse::io_read_packet,NULL,Cmp4Parse::io_seek);
	mp4Parse.setIOContext(&ioContext);
	mp4Parse.mov_read_header();

    mp4Parse.get_sps_and_pps_from_mp4_avcc_extradata(&mp4Parse,0,sps_and_pps,sps_and_pps_length);
    if (sps_and_pps_length > 0)
        fwrite(sps_and_pps,sps_and_pps_length,1,fp);

    do {
            ret = mp4Parse.mov_read_packet(&mp4Parse,&pkt);
            if (ret == 0) {


                if(sps_and_pps_length > 0) //如果是avc1格式，m_pkt.data前面的4个字节表示数据的大小，需要将其替换成h264_nalu_start_code，GPU才能解码
                {
                    uint8_t * p = pkt.data;
                    int size = 0;


                    while(1) //av_read_frame的接口描述中虽然有：For video, the packet contains exactly one frame.但是有的视频一个packet可能包含两帧数据
                    {
                        size = (p[0] << 24) | (p[1] << 16) | (p[2] << 8) | (p[3]);
                        if(size < 0) {
                            printf("%s error data [size=%d]\n", __FUNCTION__, size);
                            return 0;
                        }
                        memcpy(p, h264_nalu_start_code, 4);

                        p += 4 + size;
                        if(p >= pkt.data + pkt.size){break;}
                    }
                }


                fwrite(pkt.data,pkt.size,1,fp);
            }
          //  printf("%d\n",pkt.pos );
    }while(ret == 0);
    

	if (avio_ctx_buffer) {
		free(avio_ctx_buffer);
		avio_ctx_buffer = NULL;
	}
	return 0;
}
