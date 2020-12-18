#include "Stream.h"
#include <stdio.h>
#include <stdlib.h> 

CStream::CStream()
{
	m_index = -1;
	m_id = 0;
	m_info = NULL;
	m_priv_data = NULL;
	m_index_entries = NULL;
	m_nb_index_entries = 0;
	m_index_entries_allocated_size = 0;

    m_codec = NULL;
}
CStream::~CStream()
{

}


CStream *CStream::avformat_new_stream(Cmp4Parse *s, const Codec *c)
{
	CStream *st;
    int i;
    CStream **streams;

    if (s->m_nb_streams >= FFMIN(s->m_max_streams, INT_MAX/sizeof(*streams))) {
    	if (s->m_max_streams < INT_MAX/sizeof(*streams))
        	printf("Number of streams exceeds max_streams parameter (%d), see the documentation if you wish to increase it\n", s->m_max_streams);
    	return NULL;
	}

	streams = (CStream **)realloc((void*)s->m_streams, (s->m_nb_streams + 1) * sizeof(*streams));
	if (!streams)
        return NULL;
    s->m_streams = streams;

    st = new CStream;
    if (!st)
        return NULL;
    if (!(st->m_info = (info*)malloc(sizeof(*st->m_info)))) {
        delete st;
        return NULL;
    }

    st->m_info->last_dts = AV_NOPTS_VALUE;

#if FF_API_LAVF_AVCTX
FF_DISABLE_DEPRECATION_WARNINGS
    st->codec = avcodec_alloc_context3(c);
    if (!st->codec) {
        av_free(st->info);
        av_free(st);
        return NULL;
    }
FF_ENABLE_DEPRECATION_WARNINGS
#endif
	
    st->m_codec = new Codec;
	//st->internal = av_mallocz(sizeof(*st->internal));
    //if (!st->internal)
    //	goto fail;

	//st->codecpar = avcodec_parameters_alloc();
    //if (!st->codecpar)
    //    goto fail;

	//st->internal->avctx = avcodec_alloc_context3(NULL);
    //if (!st->internal->avctx)
    //    goto fail;
    
    st->m_index = s->m_nb_streams;
	s->m_streams[s->m_nb_streams++] = st;

    return st;

	
fail:
    free_stream(&st);
    return NULL;
}

void CStream::free_stream(CStream **pst)
{
    CStream *st = *pst;
    int i;

    if (!st)
        return;

    if (st->m_info)
    	free(st->m_info);

    delete st;
}

