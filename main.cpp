#include "mp4Parse.h"
#include "IOContext.h"
#include <stdio.h>
#include <stdlib.h>



int main()
{
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

    do {
            ret = mp4Parse.mov_read_packet(&mp4Parse,&pkt);
            if (ret == 0)
                    fwrite(pkt.data,pkt.size,1,fp);
            printf("%d\n",pkt.pos );
    }while(ret == 0);
    

	if (avio_ctx_buffer) {
		free(avio_ctx_buffer);
		avio_ctx_buffer = NULL;
	}
	return 0;
}
