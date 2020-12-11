#include "IOContext.h"
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>


static int url_resetbuf(CIOContext *s, int flags)
{
    av_assert1(flags == AVIO_FLAG_WRITE || flags == AVIO_FLAG_READ);

    if (flags & AVIO_FLAG_WRITE) {
        s->m_buf_end = s->m_buffer + s->m_buffer_size;
        s->m_write_flag = 1;
    } else {
        s->m_buf_end = s->m_buffer;
        s->m_write_flag = 0;
    }
    return 0;
}


static int ffio_set_buf_size(CIOContext *s, int buf_size)
{
    uint8_t *buffer;
    buffer = (uint8_t *)malloc(buf_size);
    if (!buffer)
        return AVERROR(ENOMEM);

    free(s->m_buffer);
    s->m_buffer = buffer;
    s->m_orig_buffer_size =
    s->m_buffer_size = buf_size;
    s->m_buf_ptr = s->m_buf_ptr_max = buffer;
    url_resetbuf(s, s->m_write_flag ? AVIO_FLAG_WRITE : AVIO_FLAG_READ);
    return 0;
}
static void fill_buffer(CIOContext *s)
{
	int max_buffer_size = s->m_max_packet_size ?
                          s->m_max_packet_size : IO_BUFFER_SIZE;

     uint8_t *dst        = s->m_buf_end - s->m_buffer + s->m_max_buffer_size < s->m_buffer_size ?
                          s->m_buf_end : s->m_buffer;

     int len             = s->m_buffer_size - (dst - s->m_buffer);

     /* can't fill the buffer without read_packet, just set EOF if appropriate */
     if (!s->m_read_packet && s->m_buf_ptr >= s->m_buf_end)
        s->m_eof_reached = 1;

    /* no need to do anything if EOF already reached */
    if (s->m_eof_reached)
        return;

    if (s->m_update_checksum && dst == s->m_buffer) {
        if (s->m_buf_end > s->m_checksum_ptr)
            s->m_checksum = s->m_update_checksum(s->m_checksum, s->m_checksum_ptr,
                                             s->m_buf_end - s->m_checksum_ptr);
        s->m_checksum_ptr = s->m_buffer;
    }


     /* make buffer smaller in case it ended up large after probing */
    if (s->m_read_packet && s->m_orig_buffer_size && s->m_buffer_size > s->m_orig_buffer_size) {
        if (dst == s->m_buffer && s->m_buf_ptr != dst) {
            int ret = ffio_set_buf_size(s, s->m_orig_buffer_size);
            if (ret < 0)
                printf("Failed to decrease buffer size\n");

            s->m_checksum_ptr = dst = s->m_buffer;
        }
        av_assert0(len >= s->m_orig_buffer_size);
        len = s->m_orig_buffer_size;
    }

     len = s->read_packet_wrapper(dst, len);
    if (len == AVERROR_EOF) {
        /* do not modify buffer if EOF reached so that a seek back can
           be done without rereading data */
        s->m_eof_reached = 1;
    } else if (len < 0) {
        s->m_eof_reached = 1;
       // s->error= len;
    } else {
        s->m_pos += len;
        s->m_buf_ptr = dst;
        s->m_buf_end = dst + len;
        s->m_bytes_read += len;
    }
}


static void writeout(CIOContext *s, const uint8_t *data, int len)
{
    if (!s->m_error) {
        int ret = 0;
        if (s->m_write_data_type)
            ret = s->m_write_data_type(s->m_opaque, (uint8_t *)data,
                                     len,
                                     s->m_current_type,
                                     s->m_last_time);
        else if (s->m_write_packet)
            ret = s->m_write_packet(s->m_opaque, (uint8_t *)data, len);
        if (ret < 0) {
            s->m_error = ret;
        } else {
            if (s->m_pos + len > s->m_written)
                s->m_written = s->m_pos + len;
        }
    }
    if (s->m_current_type == AVIO_DATA_MARKER_SYNC_POINT ||
        s->m_current_type == AVIO_DATA_MARKER_BOUNDARY_POINT) {
        s->m_current_type = AVIO_DATA_MARKER_UNKNOWN;
    }
    s->m_last_time = AV_NOPTS_VALUE;
    s->m_writeout_count ++;
    s->m_pos += len;
}


static void flush_buffer(CIOContext *s)
{
    s->m_buf_ptr_max = FFMAX(s->m_buf_ptr, s->m_buf_ptr_max);
    if (s->m_write_flag && s->m_buf_ptr_max > s->m_buffer) {
        writeout(s, s->m_buffer, s->m_buf_ptr_max - s->m_buffer);
        if (s->m_update_checksum) {
            s->m_checksum     = s->m_update_checksum(s->m_checksum, s->m_checksum_ptr,
                                                 s->m_buf_ptr_max - s->m_checksum_ptr);
            s->m_checksum_ptr = s->m_buffer;
        }
    }
    s->m_buf_ptr = s->m_buf_ptr_max = s->m_buffer;
    if (!s->m_write_flag)
        s->m_buf_end = s->m_buffer;
}


CIOContext::CIOContext()
{
	m_buffer 		= NULL;
	m_buffer_size = 0;
	m_buf_ptr     = NULL;
	m_buf_end     = NULL;
	m_opaque      = NULL;

	m_read_packet 	= NULL;
	m_write_packet  = NULL;
	m_seek   		= NULL;
    m_write_data_type = NULL;
    m_update_checksum = NULL;

	m_pos = 0;
	m_eof_reached = 0;
	m_max_packet_size = 0;
	m_write_flag = 0;

	m_error = 0;
    m_checksum = 0;
    m_checksum_ptr = NULL;

	m_min_packet_size = 0;
    m_max_packet_size = 0;
    m_max_buffer_size = 0;

    m_written = 0;

    m_buf_ptr_max = 0;

    m_last_time = 0;

    m_writeout_count = 0;

    m_bytes_read = 0;

    m_short_seek_get  = NULL;

    m_short_seek_threshold = 0;

    m_seekable = 0;

    m_seek_count = 0;
    m_orig_buffer_size = 0;
    //m_current_type = 
}

CIOContext::~CIOContext()
{

}

int CIOContext::Init(unsigned char *buffer,
	      int buffer_size,
	      int write_flag,
	      void *opaque,
	      int (*read_packet)(void *opaque, uint8_t *buf, int buf_size),
	      int (*write_packet)(void *opaque, uint8_t *buf, int buf_size),
	      int64_t (*seek)(void *opaque, int64_t offset, int whence))
{
    m_buf_ptr = 
	m_buffer 		= buffer;
    m_orig_buffer_size = 
	m_buffer_size	= buffer_size;
	m_opaque		= opaque;

	url_resetbuf(this, write_flag ? AVIO_FLAG_WRITE : AVIO_FLAG_READ);

	m_write_packet    = write_packet;
    m_read_packet     = read_packet;
    m_seek            = seek;

    m_pos             = 0;
    m_eof_reached     = 0;
    m_error           = 0;
    m_min_packet_size = 0;
    m_max_packet_size = 0;

    m_direct = 0;

	return 0;
}

void CIOContext::Relase()
{
	
}


int64_t CIOContext::avio_size()
{
    int64_t size;

    if (m_written)
        return m_written;

    if (!m_seek)
        return AVERROR(ENOSYS);
    size = m_seek(m_opaque, 0, AVSEEK_SIZE);
    if (size < 0) {
        if ((size = m_seek(m_opaque, -1, SEEK_END)) < 0)
            return size;
        size++;
        m_seek(m_opaque, m_pos, SEEK_SET);
    }
    return size;
}


void CIOContext::avio_w8( int b)
{
    av_assert2(b>=-128 && b<=255);
    *m_buf_ptr++ = b;
    if (m_buf_ptr >= m_buf_end)
        flush_buffer(this);
}

/**
 * Similar to feof() but also returns nonzero on read errors.
 * @return non zero if and only if at end of file or a read error happened when reading.
 */
int CIOContext::avio_feof()
{
    if(m_eof_reached){
        m_eof_reached=0;
        fill_buffer(this);
    }
    return m_eof_reached;
}

int CIOContext::avio_r8()
{
    if (m_buf_ptr >= m_buf_end)
        fill_buffer(this);
    if (m_buf_ptr < m_buf_end)
        return *m_buf_ptr++;
    return 0;
}


unsigned int CIOContext::avio_rb16()
{
    unsigned int val;
    val = avio_r8() << 8;
    val |= avio_r8();
    return val;
}

unsigned int CIOContext::avio_rb32()
{
    unsigned int val;
    val = avio_rb16() << 16;
    val |= avio_rb16();
    return val;
}

unsigned int CIOContext::avio_rl16()
{
    unsigned int val;
    val = avio_r8();
    val |= avio_r8() << 8;
    return val;
}


unsigned int CIOContext::avio_rb24()
{
    unsigned int val;
    val = avio_rb16() << 8;
    val |= avio_r8();
    return val;
}

unsigned int CIOContext::avio_rl32()
{
    unsigned int val;
    val = avio_rl16();
    val |= avio_rl16() << 16;
    return val;
}

uint64_t CIOContext::avio_rb64()
{
    uint64_t val;
    val = (uint64_t)avio_rb32() << 32;
    val |= (uint64_t)avio_rb32();
    return val;
}
/**
 * Read size bytes from AVIOContext into buf.
 * @return number of bytes read or AVERROR
 */
int CIOContext::avio_read(unsigned char *buf, int size)
{
   int len, size1;

    size1 = size;
    while (size > 0) {
        len = FFMIN(m_buf_end - m_buf_ptr, size);
        if (len == 0 || m_write_flag) {
            if((m_direct || size > m_buffer_size) && !m_update_checksum) {
                // bypass the buffer and read data directly into buf
                len = read_packet_wrapper(buf, size);
                if (len == AVERROR_EOF) {
                    /* do not modify buffer if EOF reached so that a seek back can
                    be done without rereading data */
                    m_eof_reached = 1;
                    break;
                } else if (len < 0) {
                    m_eof_reached = 1;
                    m_error= len;
                    break;
                } else {
                    m_pos += len;
                    m_bytes_read += len;
                    size -= len;
                    buf += len;
                    // reset the buffer
                    m_buf_ptr = m_buffer;
                    m_buf_end = m_buffer/* + len*/;
                }
            } else {
                fill_buffer(this);
                len = m_buf_end - m_buf_ptr;
                if (len == 0)
                    break;
            }
        } else {
            memcpy(buf, m_buf_ptr, len);
            buf += len;
            m_buf_ptr += len;
            size -= len;
        }
    }
    if (size1 == size) {
        if (m_error)      return m_error;
        if (avio_feof())  return AVERROR_EOF;
    }
    return size1 - size;
}


int CIOContext::ffio_read_size(unsigned char *buf, int size)
{
    int ret = avio_read(buf, size);
    if (ret != size)
        return AVERROR_INVALIDDATA;
    return ret;
}


/**
 * fseek() equivalent for AVIOContext.
 * @return new position or AVERROR.
 */
int64_t CIOContext::avio_seek(int64_t offset, int whence)
{
    int64_t offset1;
    int64_t pos;
    int force = whence & AVSEEK_FORCE;
    int buffer_size;
    int short_seek;
    whence &= ~AVSEEK_FORCE;

    buffer_size = m_buf_end - m_buffer;
    // pos is the absolute position that the beginning of s->buffer corresponds to in the file
    pos = m_pos - (m_write_flag ? 0 : buffer_size);

    if (whence != SEEK_CUR && whence != SEEK_SET)
        return AVERROR(EINVAL);

    if (whence == SEEK_CUR) {
        offset1 = pos + (m_buf_ptr - m_buffer);
        if (offset == 0)
            return offset1;
        if (offset > INT_MAX - offset1)
            return AVERROR(EINVAL);
        offset += offset1;
    }
    if (offset < 0)
        return AVERROR(EINVAL);

    if (m_short_seek_get) {
        short_seek = m_short_seek_get(m_opaque);
        /* fallback to default short seek */
        if (short_seek <= 0)
            short_seek = m_short_seek_threshold;
    } else
        short_seek = m_short_seek_threshold;

    offset1 = offset - pos; // "offset1" is the relative offset from the beginning of s->buffer
    m_buf_ptr_max = FFMAX(m_buf_ptr_max, m_buf_ptr);
    if ((!m_direct || !m_seek) &&
        offset1 >= 0 && offset1 <= (m_write_flag ? m_buf_ptr_max - m_buffer : buffer_size)) {
        /* can do the seek inside the buffer */
        m_buf_ptr = m_buffer + offset1;
    } else if ((!(m_seekable & AVIO_SEEKABLE_NORMAL) ||
               offset1 <= buffer_size + short_seek) &&
               !m_write_flag && offset1 >= 0 &&
               (!m_direct || !m_seek) &&
              (whence != SEEK_END || force)) {
        while(m_pos < offset && !m_eof_reached)
            fill_buffer(this);
        if (m_eof_reached)
            return AVERROR_EOF;
        m_buf_ptr = m_buf_end - (m_pos - offset);
    } else if(!m_write_flag && offset1 < 0 && -offset1 < buffer_size>>1 && m_seek && offset > 0) {
        int64_t res;

        pos -= FFMIN(buffer_size>>1, pos);
        if ((res = m_seek(m_opaque, pos, SEEK_SET)) < 0)
            return res;
        m_buf_end =
        m_buf_ptr = m_buffer;
        m_pos = pos;
        m_eof_reached = 0;
        fill_buffer(this);
        return avio_seek(offset, SEEK_SET | force);
    } else {
        int64_t res;
        if (m_write_flag) {
            flush_buffer(this);
        }
        if (!m_seek)
            return AVERROR(EPIPE);
        if ((res = m_seek(m_opaque, offset, SEEK_SET)) < 0)
            return res;
        m_seek_count ++;
        if (!m_write_flag)
            m_buf_end = m_buffer;
        m_buf_ptr = m_buf_ptr_max = m_buffer;
        m_pos = offset;
    }
    m_eof_reached = 0;
    return offset;
}

/**
 * Skip given number of bytes forward
 * @return new position or AVERROR.
 */
int64_t CIOContext::avio_skip( int64_t offset)
{
    return avio_seek(offset, SEEK_CUR);
}

 int64_t CIOContext::avio_tell()
 {
    return avio_seek(0, SEEK_CUR);
 }

 int CIOContext::av_get_packet(AVPacket *pkt, int size)
 {
    pkt->pts                  = AV_NOPTS_VALUE;
    pkt->dts                  = AV_NOPTS_VALUE;
    pkt->pos                  = -1;
    pkt->duration             = 0;

    pkt->flags                = 0;
    pkt->stream_index         = 0;

    pkt->data = NULL;
    pkt->size = 0;
    pkt->pos  = avio_tell();

    return append_packet_chunked( pkt, size);
 }

 int CIOContext::append_packet_chunked(AVPacket *pkt, int size)
 {
    int64_t orig_pos   = pkt->pos; // av_grow_packet might reset pos
    int orig_size      = pkt->size;
    int ret;
    int prev_size = pkt->size;
    int read_size;

    if (size > 0) {
        pkt->data = (uint8_t*)malloc(size);
    }

    do {
        
        /* When the caller requests a lot of data, limit it to the amount
         * left in file or SANE_CHUNK_SIZE when it is not known. */
        read_size = size ;

        ret = avio_read(pkt->data + pkt->size, read_size);
        if (ret > 0) {
            pkt->size += ret;
            size -= ret;
        } else 
            break;
        
    } while (size > 0 );
 //    printf("ret:%d read_size:%d prev_size:%d\n",ret,read_size,prev_size);
  //  if (size > 0)
    //    pkt->flags |= AV_PKT_FLAG_CORRUPT;

    pkt->pos = orig_pos;
//    if (!pkt->size)
  //      av_packet_unref(pkt);
    return pkt->size > orig_size ? pkt->size - orig_size : ret; 
 }

int CIOContext::read_packet_wrapper(uint8_t *buf, int size)
{
    int ret;

    if (!m_read_packet)
        return AVERROR(EINVAL);
    ret = m_read_packet(m_opaque, buf, size);
#if FF_API_OLD_AVIO_EOF_0
    if (!ret && !s->max_packet_size) {
        av_log(NULL, AV_LOG_WARNING, "Invalid return value 0 for stream protocol\n");
        ret = AVERROR_EOF;
    }
#else
    av_assert2(ret ||m_max_packet_size);
#endif
    return ret;
}
