#ifndef __CODEC_H__
#define __CODEC_H__

#include "common.h"

class Codec
{
public:
	Codec();
	~Codec();
	
	int m_width;
	int m_height;
	uint8_t *m_extradata;
    /**
     * Size of the extradata content in bytes.
     */
    int      m_extradata_size;
    int m_bits_per_coded_sample;
};

#endif //__CODEC_H__