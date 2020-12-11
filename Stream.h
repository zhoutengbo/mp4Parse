#ifndef __CStream_H__
#define __CStream_H__
#include "mp4Parse.h"
#include "Codec.h"
#include "common.h"



class CStream
{
public:
	CStream();
	~CStream();
	
	static CStream *avformat_new_stream(Cmp4Parse *s, const Codec *c);
	static void free_stream(CStream **pst);

#define MAX_STD_TIMEBASES (30*12+30+3+6)

	struct info{
        int64_t last_dts;
        int64_t duration_gcd;
        int duration_count;
        int64_t rfps_duration_sum;
        double (*duration_error)[2][MAX_STD_TIMEBASES];
        int64_t codec_info_duration;
        int64_t codec_info_duration_fields;
        int frame_delay_evidence;

        /**
         * 0  -> decoder has not been searched for yet.
         * >0 -> decoder found
         * <0 -> decoder with codec_id == -found_decoder has not been found
         */
        int found_decoder;

        int64_t last_duration;

        /**
         * Those are used for average framerate estimation.
         */
        int64_t fps_first_dts;
        int     fps_first_dts_idx;
        int64_t fps_last_dts;
        int     fps_last_dts_idx;

    } *m_info;

    AVIndexEntry *m_index_entries; /**< Only used if the format does not
                                    support seeking natively. */
    int m_nb_index_entries;
    unsigned int m_index_entries_allocated_size;

    /**
     * Format-specific stream ID.
     * decoding: set by libavformat
     * encoding: set by the user, replaced by libavformat if left unset
     */
    int m_id;

    void *m_priv_data;

    int m_index;    /**< stream index in AVFormatContext */
};

#endif //__FORMATCONTEXT_H__