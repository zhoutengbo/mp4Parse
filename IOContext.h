#ifndef __IOCONTEXT_H__
#define __IOCONTEXT_H__

#include "common.h"

class CIOContext
{
public:
	CIOContext();
	~CIOContext();

	/**
	 * Allocate and initialize an CIOContext for buffered I/O. It must be later
	 * freed with Release().
	 *
	 * @param buffer Memory block for input/output operations via AVIOContext.
	 *        The buffer must be allocated with malloc() and friends.
	 *        It may be freed and replaced with a new buffer by libavformat.
	 *        AVIOContext.buffer holds the buffer currently in use,
	 *        which must be later freed with free().
	 * @param buffer_size The buffer size is very important for performance.
	 *        For protocols with fixed blocksize it should be set to this blocksize.
	 *        For others a typical size is a cache page, e.g. 4kb.
	 * @param write_flag Set to 1 if the buffer should be writable, 0 otherwise.
	 * @param opaque An opaque pointer to user-specific data.
	 * @param read_packet  A function for refilling the buffer, may be NULL.
	 *                     For stream protocols, must never return 0 but rather
	 *                     a proper AVERROR code.
	 * @param write_packet A function for writing the buffer contents, may be NULL.
	 *        The function may not change the input buffers content.
	 * @param seek A function for seeking to specified byte position, may be NULL.
	 *
	 * @return 0 or NOT zero on failure.
	 */
	int Init(unsigned char *buffer,
	      int buffer_size,
	      int write_flag,
	      void *opaque,
	      int (*read_packet)(void *opaque, uint8_t *buf, int buf_size),
	      int (*write_packet)(void *opaque, uint8_t *buf, int buf_size),
	      int64_t (*seek)(void *opaque, int64_t offset, int whence));

	/**
	 * Free the supplied IO context and everything associated with it.
	 *
	 * @param s Double pointer to the IO context. This function will write NULL
	 * into s.
	 */
	void Relase();

    

    /**
     * Similar to feof() but also returns nonzero on read errors.
     * @return non zero if and only if at end of file or a read error happened when reading.
     */
    int avio_feof();

    int64_t avio_size();

    void avio_w8( int b);


    int avio_r8();

    unsigned int avio_rb16();

    unsigned int avio_rb32();

    unsigned int avio_rl16();

    unsigned int avio_rb24();

    unsigned int avio_rl32();

    uint64_t     avio_rb64();

    /**
     * Read size bytes from AVIOContext into buf.
     * @return number of bytes read or AVERROR
     */
    int avio_read(unsigned char *buf, int size);

    int ffio_read_size(unsigned char *buf, int size);

    /**
     * fseek() equivalent for AVIOContext.
     * @return new position or AVERROR.
     */
    int64_t avio_seek(int64_t offset, int whence);

    /**
     * Skip given number of bytes forward
     * @return new position or AVERROR.
     */
    int64_t avio_skip( int64_t offset);


    int64_t avio_tell();

    int av_get_packet(AVPacket *pkt, int size);

    /* Read the data in sane-sized chunks and append to pkt.
 * Return the number of bytes read or an error. */
    int append_packet_chunked(AVPacket *pkt, int size);

    int read_packet_wrapper(uint8_t *buf, int size);

    

    /*
     * The following shows the relationship between buffer, buf_ptr,
     * buf_ptr_max, buf_end, buf_size, and pos, when reading and when writing
     * (since AVIOContext is used for both):
     *
     **********************************************************************************
     *                                   READING
     **********************************************************************************
     *
     *                            |              buffer_size              |
     *                            |---------------------------------------|
     *                            |                                       |
     *
     *                         buffer          buf_ptr       buf_end
     *                            +---------------+-----------------------+
     *                            |/ / / / / / / /|/ / / / / / /|         |
     *  read buffer:              |/ / consumed / | to be read /|         |
     *                            |/ / / / / / / /|/ / / / / / /|         |
     *                            +---------------+-----------------------+
     *
     *                                                         pos
     *              +-------------------------------------------+-----------------+
     *  input file: |                                           |                 |
     *              +-------------------------------------------+-----------------+
     *
     *
     **********************************************************************************
     *                                   WRITING
     **********************************************************************************
     *
     *                             |          buffer_size                 |
     *                             |--------------------------------------|
     *                             |                                      |
     *
     *                                                buf_ptr_max
     *                          buffer                 (buf_ptr)       buf_end
     *                             +-----------------------+--------------+
     *                             |/ / / / / / / / / / / /|              |
     *  write buffer:              | / / to be flushed / / |              |
     *                             |/ / / / / / / / / / / /|              |
     *                             +-----------------------+--------------+
     *                               buf_ptr can be in this
     *                               due to a backward seek
     *
     *                            pos
     *               +-------------+----------------------------------------------+
     *  output file: |             |                                              |
     *               +-------------+----------------------------------------------+
     *
     */
    unsigned char *m_buffer;  /**< Start of the buffer. */
    int m_buffer_size;        /**< Maximum buffer size */
    unsigned char *m_buf_ptr; /**< Current position in the buffer */
    unsigned char *m_buf_end; /**< End of the data, may be less than
                                 buffer+buffer_size if the read function returned
                                 less data than requested, e.g. for streams where
                                 no more data has been received yet. */
	void *m_opaque;           /**< A private pointer, passed to the read/write/seek/...
                                 functions. */

    /**
     * Threshold to favor readahead over seek.
     * This is current internal only, do not use from outside.
     */
    int m_short_seek_threshold;
   

	int (*m_read_packet)(void *opaque, uint8_t *buf, int buf_size);
    int (*m_write_packet)(void *opaque, uint8_t *buf, int buf_size);
    int64_t (*m_seek)(void *opaque, int64_t offset, int whence);

    /**
     * A callback that is used instead of write_packet.
     */
    int (*m_write_data_type)(void *opaque, uint8_t *buf, int buf_size,
                           enum AVIODataMarkerType type, int64_t time);

    unsigned long (*m_update_checksum)(unsigned long checksum, const uint8_t *buf, unsigned int size);

     /**
     * A callback that is used instead of short_seek_threshold.
     * This is current internal only, do not use from outside.
     */
    int (*m_short_seek_get)(void *opaque);

    int64_t m_pos;            /**< position in the file of the current buffer */
    int m_eof_reached;        /**< true if was unable to read due to error or eof */
    int m_max_packet_size;
    int m_max_buffer_size;
    int m_orig_buffer_size;
    int m_write_flag;         /**< true if open for writing */
    int m_error;              /**< contains the error code or 0 if no error happened */

    /**
     * Bytes read statistic
     * This field is internal to libavformat and access from outside is not allowed.
     */
    int64_t m_bytes_read;
    
    unsigned long m_checksum;
    unsigned char *m_checksum_ptr;

    /**
     * writeout statistic
     * This field is internal to libavformat and access from outside is not allowed.
     */
    int m_writeout_count;

    /**
     * Maximum reached position before a backward seek in the write buffer,
     * used keeping track of already written data for a later flush.
     */
    unsigned char *m_buf_ptr_max;

    /**
     * Try to buffer at least this amount of data before flushing it
     */
    int m_min_packet_size;

    int64_t m_written;

    int64_t m_last_time;

    /**
     * avio_read and avio_write should if possible be satisfied directly
     * instead of going through a buffer, and avio_seek will always
     * call the underlying seek function directly.
     */
    int m_direct;

    /**
     * A combination of AVIO_SEEKABLE_ flags or 0 when the stream is not seekable.
     */
    int m_seekable;

     /**
     * seek statistic
     * This field is internal to libavformat and access from outside is not allowed.
     */
    int m_seek_count;


     /**
     * Internal, not meant to be used from outside of AVIOContext.
     */
    enum AVIODataMarkerType m_current_type;
};

#endif //__IOCONTEXT_H__