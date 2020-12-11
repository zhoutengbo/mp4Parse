#ifndef __MP4PARSE_H__
#define __MP4PARSE_H__

#include "common.h"
#include "IOContext.h"
class CStream;



class Cmp4Parse
{
public:
	Cmp4Parse();
	~Cmp4Parse();

    int init_input(int flags,const char *filename,
                      AVDictionary **options);

#if 1
    int mov_read_header();

    int setIOContext(CIOContext* pb);

    static int mov_read_packet(Cmp4Parse *s, AVPacket *pkt);

    static AVIndexEntry *mov_find_next_sample(Cmp4Parse *s, CStream **st);
    static void mov_current_sample_inc(MOVStreamContext *sc);
//private:
	static int mov_read_default(Cmp4Parse* cxt,CIOContext* pb,MOVAtom atom);

	static int mov_read_udta_string(Cmp4Parse *cxt, CIOContext *pb, MOVAtom atom);

	static int mov_read_keys(Cmp4Parse *c, CIOContext *pb, MOVAtom atom);
    static int mov_read_moov(Cmp4Parse *c, CIOContext *pb, MOVAtom atom);
    static int mov_read_meta(Cmp4Parse *c, CIOContext *pb, MOVAtom atom);
    static int mov_read_trak(Cmp4Parse *c, CIOContext *pb, MOVAtom atom);
    static int mov_read_stco(Cmp4Parse *c, CIOContext *pb, MOVAtom atom);
    static int mov_read_stsc(Cmp4Parse *c, CIOContext *pb, MOVAtom atom);
    static int mov_read_trun(Cmp4Parse *c, CIOContext *pb, MOVAtom atom);
    static int mov_read_stsz(Cmp4Parse *c, CIOContext *pb, MOVAtom atom);


    static void mov_build_index(Cmp4Parse *mov, CStream *st);

	static int mov_metadata_int8_no_padding(Cmp4Parse *c, CIOContext *pb,
                                        unsigned len, const char *key);

	static int mov_metadata_track_or_disc_number(Cmp4Parse *c, CIOContext *pb,
                                             unsigned len, const char *key);

	static int mov_metadata_gnre(Cmp4Parse *c, CIOContext *pb,
                             unsigned len, const char *key);

	static int mov_metadata_int8_bypass_padding(Cmp4Parse *c, CIOContext *pb,
                             unsigned len, const char *key);

	static int mov_metadata_hmmt(Cmp4Parse *c, CIOContext *pb, unsigned len);

	static int mov_metadata_loci(Cmp4Parse *c, CIOContext *pb, unsigned len);



	static MOVParseTableEntry mov_default_parse_table[];

	AVDictionaryEntry *av_dict_get(const AVDictionary *m, const char *key,
                               const AVDictionaryEntry *prev, int flags);

    static int io_read_packet(void *opaque, uint8_t *buf, int buf_size);
    static int64_t io_seek(void *opaque, int64_t offset, int whence);
#endif
	/**
     * Allow non-standard and experimental extension
     * @see AVCodecContext.strict_std_compliance
     */
    int m_strict_std_compliance;		//


	int m_atom_depth;

	int m_moov_retry;

	int m_found_hdlr_mdta;  ///< 'hdlr' atom with type 'mdta' has been found

	int m_found_moov;       ///< 'moov' atom has been found

	int m_found_mdat;       ///< 'mdat' atom has been found

    int m_trak_index;       ///< Index of the current 'trak'

	 /**
     * can use flags: AVFMT_NOFILE, AVFMT_NEEDNUMBER,
     * AVFMT_GLOBALHEADER, AVFMT_NOTIMESTAMPS, AVFMT_VARIABLE_FPS,
     * AVFMT_NODIMENSIONS, AVFMT_NOSTREAMS, AVFMT_ALLOW_FLUSH,
     * AVFMT_TS_NONSTRICT, AVFMT_TS_NEGATIVE
     */
    int m_flags;

    int64_t m_next_root_atom; ///< offset of the next root atom

    MOVFragmentIndex m_frag_index;

    int m_export_xmp;

    char **m_meta_keys;
    unsigned m_meta_keys_count;

    /**
     * Flags for the user to detect events happening on the stream. Flags must
     * be cleared by the user once the event has been handled.
     * A combination of AVSTREAM_EVENT_FLAG_*.
     */
    int m_event_flags;

    AVDictionary *m_metadata;

    /**
     * Number of elements in AVFormatContext.streams.
     *
     * Set by avformat_new_stream(), must not be modified by any other code.
     */
    unsigned int m_nb_streams;

    /**
     * The maximum number of streams.
     * - encoding: unused
     * - decoding: set by user
     */
    int m_max_streams;


    int m_fb;

    CIOContext* m_pb;

    /**
     * A list of all streams in the file. New streams are created with
     * avformat_new_stream().
     *
     * - demuxing: streams are created by libavformat in avformat_open_input().
     *             If AVFMTCTX_NOHEADER is set in ctx_flags, then new streams may also
     *             appear in av_read_frame().
     * - muxing: streams are created by the user before avformat_write_header().
     *
     * Freed by libavformat in avformat_free_context().
     */
    CStream **m_streams;

};

#endif //__MP4PARSE_H__