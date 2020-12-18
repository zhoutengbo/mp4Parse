#ifndef __COMMON_my_H__
#define __COMMON_my_H__


#include <inttypes.h>
#include <stddef.h>
//typedef  unsigned char uint8_t;
//typedef  unsigned int uint32_t;
//typedef  long long int64_t;
//typedef unsigned long long uint64_t;

/**
 * @def PUT_UTF8(val, tmp, PUT_BYTE)
 * Convert a 32-bit Unicode character to its UTF-8 encoded form (up to 4 bytes long).
 * @param val is an input-only argument and should be of type uint32_t. It holds
 * a UCS-4 encoded Unicode character that is to be converted to UTF-8. If
 * val is given as a function it is executed only once.
 * @param tmp is a temporary variable and should be of type uint8_t. It
 * represents an intermediate value during conversion that is to be
 * output by PUT_BYTE.
 * @param PUT_BYTE writes the converted UTF-8 bytes to any proper destination.
 * It could be a function or a statement, and uses tmp as the input byte.
 * For example, PUT_BYTE could be "*output++ = tmp;" PUT_BYTE will be
 * executed up to 4 times for values in the valid UTF-8 range and up to
 * 7 times in the general case, depending on the length of the converted
 * Unicode character.
 */
#define PUT_UTF8(val, tmp, PUT_BYTE)\
    {\
        int bytes, shift;\
        uint32_t in = val;\
        if (in < 0x80) {\
            tmp = in;\
            PUT_BYTE\
        } else {\
            bytes = (in + 4) / 5;\
            shift = (bytes - 1) * 6;\
            tmp = (256 - (256 >> bytes)) | (in >> shift);\
            PUT_BYTE\
            while (shift >= 6) {\
                shift -= 6;\
                tmp = 0x80 | ((in >> shift) & 0x3f);\
                PUT_BYTE\
            }\
        }\
    }


#define AV_NOPTS_VALUE          ((int64_t)UINT64_C(0x8000000000000000))


typedef struct MOVStsc {
    int first;
    int count;
    int id;
} MOVStsc;

struct MOVStreamContext
{
    /** extradata array (and size) for multiple stsd */
    uint8_t **extradata;
    int *extradata_size;
    int stsd_count;

    int tmp;
    int current_sample;
    int64_t data_size;
    unsigned int stsc_count;
    MOVStsc *stsc_data;
    unsigned int chunk_count;
    int64_t *chunk_offsets;
    int ffindex;          ///< AVStream index
    unsigned int sample_size; ///< may contain value calculated from stsd or value from stsz atom
    unsigned int stsz_sample_size; ///< always contains sample size from stsz atom
    unsigned int sample_count;
    int *sample_sizes;
};

typedef struct AVPacket {
    /**
     * Presentation timestamp in AVStream->time_base units; the time at which
     * the decompressed packet will be presented to the user.
     * Can be AV_NOPTS_VALUE if it is not stored in the file.
     * pts MUST be larger or equal to dts as presentation cannot happen before
     * decompression, unless one wants to view hex dumps. Some formats misuse
     * the terms dts and pts/cts to mean something different. Such timestamps
     * must be converted to true pts/dts before they are stored in AVPacket.
     */
    int64_t pts;
    /**
     * Decompression timestamp in AVStream->time_base units; the time at which
     * the packet is decompressed.
     * Can be AV_NOPTS_VALUE if it is not stored in the file.
     */
    int64_t dts;
    uint8_t *data;
    int   size;
    int   stream_index;
    /**
     * A combination of AV_PKT_FLAG values
     */
    int   flags;


    /**
     * Duration of this packet in AVStream->time_base units, 0 if unknown.
     * Equals next_pts - this_pts in presentation order.
     */
    int64_t duration;

    int64_t pos;                            ///< byte position in stream, -1 if unknown

} AVPacket;



typedef struct AVIndexEntry {
    int64_t pos;
    int64_t timestamp;        /**<
                               * Timestamp in AVStream.time_base units, preferably the time from which on correctly decoded frames are available
                               * when seeking to this entry. That means preferable PTS on keyframe based formats.
                               * But demuxers can choose to store a different timestamp, if it is more convenient for the implementation or nothing better
                               * is known
                               */
#define AVINDEX_KEYFRAME 0x0001
#define AVINDEX_DISCARD_FRAME  0x0002    /**
                                          * Flag is used to indicate which frame should be discarded after decoding.
                                          */
    int flags:2;
    int size:30; //Yeah, trying to keep the size of this small to reduce memory requirements (it is 24 vs. 32 bytes due to possible 8-byte alignment).
    int min_distance;         /**< Minimum distance between this and the previous keyframe, used to avoid unneeded searching. */
} AVIndexEntry;

typedef struct AVDictionaryEntry {
    char *key;
    char *value;
} AVDictionaryEntry;


struct AVDictionary {
    int count;
    AVDictionaryEntry *elems;
};


typedef struct AVSubsampleEncryptionInfo {
    /** The number of bytes that are clear. */
    unsigned int bytes_of_clear_data;

    /**
     * The number of bytes that are protected.  If using pattern encryption,
     * the pattern applies to only the protected bytes; if not using pattern
     * encryption, all these bytes are encrypted.
     */
    unsigned int bytes_of_protected_data;
} AVSubsampleEncryptionInfo;


/**
 * This describes encryption info for a packet.  This contains frame-specific
 * info for how to decrypt the packet before passing it to the decoder.
 *
 * The size of this struct is not part of the public ABI.
 */
typedef struct AVEncryptionInfo {
    /** The fourcc encryption scheme, in big-endian byte order. */
    uint32_t scheme;

    /**
     * Only used for pattern encryption.  This is the number of 16-byte blocks
     * that are encrypted.
     */
    uint32_t crypt_byte_block;

    /**
     * Only used for pattern encryption.  This is the number of 16-byte blocks
     * that are clear.
     */
    uint32_t skip_byte_block;

    /**
     * The ID of the key used to encrypt the packet.  This should always be
     * 16 bytes long, but may be changed in the future.
     */
    uint8_t *key_id;
    uint32_t key_id_size;

    /**
     * The initialization vector.  This may have been zero-filled to be the
     * correct block size.  This should always be 16 bytes long, but may be
     * changed in the future.
     */
    uint8_t *iv;
    uint32_t iv_size;

    /**
     * An array of subsample encryption info specifying how parts of the sample
     * are encrypted.  If there are no subsamples, then the whole sample is
     * encrypted.
     */
    AVSubsampleEncryptionInfo *subsamples;
    uint32_t subsample_count;
} AVEncryptionInfo;

typedef struct MOVEncryptionIndex {
    // Individual encrypted samples.  If there are no elements, then the default
    // settings will be used.
    unsigned int nb_encrypted_samples;
    AVEncryptionInfo **encrypted_samples;

    uint8_t* auxiliary_info_sizes;
    size_t auxiliary_info_sample_count;
    uint8_t auxiliary_info_default_size;
    uint64_t* auxiliary_offsets;  ///< Absolute seek position
    size_t auxiliary_offsets_count;
} MOVEncryptionIndex;


typedef struct MOVFragmentStreamInfo {
    int id;
    int64_t sidx_pts;
    int64_t first_tfra_pts;
    int64_t tfdt_dts;
    int index_entry;
    MOVEncryptionIndex *encryption_index;
} MOVFragmentStreamInfo;

typedef struct MOVFragmentIndexItem {
    int64_t moof_offset;
    int headers_read;
    int current;
    int nb_stream_info;
    MOVFragmentStreamInfo * stream_info;
} MOVFragmentIndexItem;

typedef struct MOVFragmentIndex {
    int allocated_size;
    int complete;
    int current;
    int nb_items;
    MOVFragmentIndexItem * item;
} MOVFragmentIndex;


typedef struct MOVAtom {
    uint32_t type;
    int64_t size; /* total size (excluding the size and type fields) */
} MOVAtom;


/**
 * Different data types that can be returned via the AVIO
 * write_data_type callback.
 */
enum AVIODataMarkerType {
    /**
     * Header data; this needs to be present for the stream to be decodeable.
     */
    AVIO_DATA_MARKER_HEADER,
    /**
     * A point in the output bytestream where a decoder can start decoding
     * (i.e. a keyframe). A demuxer/decoder given the data flagged with
     * AVIO_DATA_MARKER_HEADER, followed by any AVIO_DATA_MARKER_SYNC_POINT,
     * should give decodeable results.
     */
    AVIO_DATA_MARKER_SYNC_POINT,
    /**
     * A point in the output bytestream where a demuxer can start parsing
     * (for non self synchronizing bytestream formats). That is, any
     * non-keyframe packet start point.
     */
    AVIO_DATA_MARKER_BOUNDARY_POINT,
    /**
     * This is any, unlabelled data. It can either be a muxer not marking
     * any positions at all, it can be an actual boundary/sync point
     * that the muxer chooses not to mark, or a later part of a packet/fragment
     * that is cut into multiple write callbacks due to limited IO buffer size.
     */
    AVIO_DATA_MARKER_UNKNOWN,
    /**
     * Trailer data, which doesn't contain actual content, but only for
     * finalizing the output file.
     */
    AVIO_DATA_MARKER_TRAILER,
    /**
     * A point in the output bytestream where the underlying AVIOContext might
     * flush the buffer depending on latency or buffering requirements. Typically
     * means the end of a packet.
     */
    AVIO_DATA_MARKER_FLUSH_POINT,
};
class Cmp4Parse;
class CIOContext;
/* those functions parse an atom */
/* links atom IDs to parse functions */
typedef struct MOVParseTableEntry {
    uint32_t type;
    int (*parse)(Cmp4Parse *cxt, CIOContext *pb,MOVAtom atom);
} MOVParseTableEntry;


#define MKTAG(a,b,c,d) ((a) | ((b) << 8) | ((c) << 16) | ((unsigned)(d) << 24))
#define MKBETAG(a,b,c,d) ((d) | ((c) << 8) | ((b) << 16) | ((unsigned)(a) << 24))


#define FFERRTAG(a, b, c, d) (-(int)MKTAG(a, b, c, d))

#define AVERROR_BSF_NOT_FOUND      FFERRTAG(0xF8,'B','S','F') ///< Bitstream filter not found
#define AVERROR_BUG                FFERRTAG( 'B','U','G','!') ///< Internal bug, also see AVERROR_BUG2
#define AVERROR_BUFFER_TOO_SMALL   FFERRTAG( 'B','U','F','S') ///< Buffer too small
#define AVERROR_DECODER_NOT_FOUND  FFERRTAG(0xF8,'D','E','C') ///< Decoder not found
#define AVERROR_DEMUXER_NOT_FOUND  FFERRTAG(0xF8,'D','E','M') ///< Demuxer not found
#define AVERROR_ENCODER_NOT_FOUND  FFERRTAG(0xF8,'E','N','C') ///< Encoder not found
#define AVERROR_EOF                FFERRTAG( 'E','O','F',' ') ///< End of file
#define AVERROR_EXIT               FFERRTAG( 'E','X','I','T') ///< Immediate exit was requested; the called function should not be restarted
#define AVERROR_EXTERNAL           FFERRTAG( 'E','X','T',' ') ///< Generic error in an external library
#define AVERROR_FILTER_NOT_FOUND   FFERRTAG(0xF8,'F','I','L') ///< Filter not found
#define AVERROR_INVALIDDATA        FFERRTAG( 'I','N','D','A') ///< Invalid data found when processing input
#define AVERROR_MUXER_NOT_FOUND    FFERRTAG(0xF8,'M','U','X') ///< Muxer not found
#define AVERROR_OPTION_NOT_FOUND   FFERRTAG(0xF8,'O','P','T') ///< Option not found
#define AVERROR_PATCHWELCOME       FFERRTAG( 'P','A','W','E') ///< Not yet implemented in FFmpeg, patches welcome
#define AVERROR_PROTOCOL_NOT_FOUND FFERRTAG(0xF8,'P','R','O') ///< Protocol not found

#define AVERROR_STREAM_NOT_FOUND   FFERRTAG(0xF8,'S','T','R') ///< Stream not found


#define IO_BUFFER_SIZE 32768

#define INT_MAX 21474836472 
#define INT_MIN (-INT_MAX - 1)

/**
 * @name URL open modes
 * The flags argument to avio_open must be one of the following
 * constants, optionally ORed with other flags.
 * @{
 */
#define AVIO_FLAG_READ  1                                      /**< read-only */
#define AVIO_FLAG_WRITE 2                                      /**< write-only */
#define AVIO_FLAG_READ_WRITE (AVIO_FLAG_READ|AVIO_FLAG_WRITE)  /**< read-write pseudo flag */

/**
 * assert() equivalent, that is always enabled.
 */
#define AV_STRINGIFY(s)         AV_TOSTRING(s)
#define AV_TOSTRING(s) #s

#define av_assert0(cond) do {                                           \
    if (!(cond)) {                                                      \
        printf( "Assertion %s failed at %s:%d\n",    \
               AV_STRINGIFY(cond), __FILE__, __LINE__);                 \
        abort();                                                        \
    }                                                                   \
} while (0)


#define AV_INPUT_BUFFER_PADDING_SIZE 64

/**
 * assert() equivalent, that does not lie in speed critical code.
 * These asserts() thus can be enabled without fearing speed loss.
 */
#if defined(ASSERT_LEVEL) && ASSERT_LEVEL > 0
#define av_assert1(cond) av_assert0(cond)
#else
#define av_assert1(cond) ((void)0)
#endif

/**
 * assert() equivalent, that does lie in speed critical code.
 */
#if defined(ASSERT_LEVEL) && ASSERT_LEVEL > 1
#define av_assert2(cond) av_assert0(cond)
#define av_assert2_fpu() av_assert0_fpu()
#else
#define av_assert2(cond) ((void)0)
#define av_assert2_fpu() ((void)0)
#endif

#define FFMAX(a,b) ((a) > (b) ? (a) : (b))
#define FFMAX3(a,b,c) FFMAX(FFMAX(a,b),c)
#define FFMIN(a,b) ((a) > (b) ? (b) : (a))
#define FFMIN3(a,b,c) FFMIN(FFMIN(a,b),c)

#define EPERM            1      /* Operation not permitted */
#define ENOENT           2      /* No such file or directory */
#define ESRCH            3      /* No such process */
#define EINTR            4      /* Interrupted system call */
#define EIO              5      /* I/O error */
#define ENXIO            6      /* No such device or address */
#define E2BIG            7      /* Arg list too long */
#define ENOEXEC          8      /* Exec format error */
#define EBADF            9      /* Bad file number */
#define ECHILD          10      /* No child processes */
#define EAGAIN          11      /* Try again */
#define ENOMEM          12      /* Out of memory */
#define EACCES          13      /* Permission denied */
#define EFAULT          14      /* Bad address */
#define ENOTBLK         15      /* Block device required */
#define EBUSY           16      /* Device or resource busy */
#define EEXIST          17      /* File exists */
#define EXDEV           18      /* Cross-device link */
#define ENODEV          19      /* No such device */
#define ENOTDIR         20      /* Not a directory */
#define EISDIR          21      /* Is a directory */
#define EINVAL          22      /* Invalid argument */
#define ENFILE          23      /* File table overflow */
#define EMFILE          24      /* Too many open files */
#define ENOTTY          25      /* Not a typewriter */
#define ETXTBSY         26      /* Text file busy */
#define EFBIG           27      /* File too large */
#define ENOSPC          28      /* No space left on device */
#define ESPIPE          29      /* Illegal seek */
#define EROFS           30      /* Read-only file system */
#define EMLINK          31      /* Too many links */
#define EPIPE           32      /* Broken pipe */
#define EDOM            33      /* Math argument out of domain of func */
#define ERANGE          34      /* Math result not representable */
#define EDEADLK         35      /* Resource deadlock would occur */
#define ENAMETOOLONG    36      /* File name too long */
#define ENOLCK          37      /* No record locks available */
#define ENOSYS          38      /* Function not implemented */
#define ENOTEMPTY       39      /* Directory not empty */
#define ELOOP           40      /* Too many symbolic links encountered */
#define EWOULDBLOCK     EAGAIN  /* Operation would block */
#define ENOMSG          42      /* No message of desired type */
#define EIDRM           43      /* Identifier removed */
#define ECHRNG          44      /* Channel number out of range */
#define EL2NSYNC        45      /* Level 2 not synchronized */
#define EL3HLT          46      /* Level 3 halted */
#define EL3RST          47      /* Level 3 reset */
#define ELNRNG          48      /* Link number out of range */
#define EUNATCH         49      /* Protocol driver not attached */
#define ENOCSI          50      /* No CSI structure available */
#define EL2HLT          51      /* Level 2 halted */
#define EBADE           52      /* Invalid exchange */
#define EBADR           53      /* Invalid request descriptor */
#define EXFULL          54      /* Exchange full */
#define ENOANO          55      /* No anode */
#define EBADRQC         56      /* Invalid request code */
#define EBADSLT         57      /* Invalid slot */

#define EDEADLOCK       EDEADLK

#define EBFONT          59      /* Bad font file format */
#define ENOSTR          60      /* Device not a stream */
#define ENODATA         61      /* No data available */
#define ETIME           62      /* Timer expired */
#define ENOSR           63      /* Out of streams resources */
#define ENONET          64      /* Machine is not on the network */
#define ENOPKG          65      /* Package not installed */
#define EREMOTE         66      /* Object is remote */
#define ENOLINK         67      /* Link has been severed */
#define EADV            68      /* Advertise error */
#define ESRMNT          69      /* Srmount error */
#define ECOMM           70      /* Communication error on send */
#define EPROTO          71      /* Protocol error */
#define EMULTIHOP       72      /* Multihop attempted */
#define EDOTDOT         73      /* RFS specific error */
#define EBADMSG         74      /* Not a data message */
#define EOVERFLOW       75      /* Value too large for defined data type */
#define ENOTUNIQ        76      /* Name not unique on network */
#define EBADFD          77      /* File descriptor in bad state */
#define EREMCHG         78      /* Remote address changed */
#define ELIBACC         79      /* Can not access a needed shared library */
#define ELIBBAD         80      /* Accessing a corrupted shared library */
#define ELIBSCN         81      /* .lib section in a.out corrupted */
#define ELIBMAX         82      /* Attempting to link in too many shared libraries */
#define ELIBEXEC        83      /* Cannot exec a shared library directly */
#define EILSEQ          84      /* Illegal byte sequence */
#define ERESTART        85      /* Interrupted system call should be restarted */
#define ESTRPIPE        86      /* Streams pipe error */
#define EUSERS          87      /* Too many users */
#define ENOTSOCK        88      /* Socket operation on non-socket */
#define EDESTADDRREQ    89      /* Destination address required */
#define EMSGSIZE        90      /* Message too long */
#define EPROTOTYPE      91      /* Protocol wrong type for socket */
#define ENOPROTOOPT     92      /* Protocol not available */
#define EPROTONOSUPPORT 93      /* Protocol not supported */
#define ESOCKTNOSUPPORT 94      /* Socket type not supported */
#define EOPNOTSUPP      95      /* Operation not supported on transport endpoint */
#define EPFNOSUPPORT    96      /* Protocol family not supported */
#define EAFNOSUPPORT    97      /* Address family not supported by protocol */
#define EADDRINUSE      98      /* Address already in use */
#define EADDRNOTAVAIL   99      /* Cannot assign requested address */
#define ENETDOWN        100     /* Network is down */
#define ENETUNREACH     101     /* Network is unreachable */
#define ENETRESET       102     /* Network dropped connection because of reset */
#define ECONNABORTED    103     /* Software caused connection abort */
#define ECONNRESET      104     /* Connection reset by peer */
#define ENOBUFS         105     /* No buffer space available */
#define EISCONN         106     /* Transport endpoint is already connected */
#define ENOTCONN        107     /* Transport endpoint is not connected */
#define ESHUTDOWN       108     /* Cannot send after transport endpoint shutdown */
#define ETOOMANYREFS    109     /* Too many references: cannot splice */
#define ETIMEDOUT       110     /* Connection timed out */
#define ECONNREFUSED    111     /* Connection refused */
#define EHOSTDOWN       112     /* Host is down */
#define EHOSTUNREACH    113     /* No route to host */
#define EALREADY        114     /* Operation already in progress */
#define EINPROGRESS     115     /* Operation now in progress */
#define ESTALE          116     /* Stale NFS file handle */
#define EUCLEAN         117     /* Structure needs cleaning */
#define ENOTNAM         118     /* Not a XENIX named type file */
#define ENAVAIL         119     /* No XENIX semaphores available */
#define EISNAM          120     /* Is a named type file */
#define EREMOTEIO       121     /* Remote I/O error */
#define EDQUOT          122     /* Quota exceeded */

#define ENOMEDIUM       123     /* No medium found */
#define EMEDIUMTYPE     124     /* Wrong medium type */


 /* error handling */
#if EDOM > 0
#define AVERROR(e) (-(e))   ///< Returns a negative error code from a POSIX error code, to return from library functions.
#define AVUNERROR(e) (-(e)) ///< Returns a POSIX error code from a library function error return value.
#else
/* Some platforms have E* and errno already negated. */
#define AVERROR(e) (e)
#define AVUNERROR(e) (e)
#endif


 /**
 * ORing this as the "whence" parameter to a seek function causes it to
 * return the filesize without seeking anywhere. Supporting this is optional.
 * If it is not supported then the seek function will return <0.
 */
#define AVSEEK_SIZE 0x10000

/**
 * Passing this flag as the "whence" parameter to a seek function causes it to
 * seek by any means (like reopening and linear reading) or other normally unreasonable
 * means that can be extremely slow.
 * This may be ignored by the seek code.
 */
#define AVSEEK_FORCE 0x20000

 /**
 * @brief Undefined timestamp value
 *
 * Usually reported by demuxer that work on containers that do not provide
 * either pts or dts.
 */

//#define INT64_C(c) (c ## LL)
//#define UINT64_C(c) (c ## ULL)

 
#define AV_NOPTS_VALUE          ((int64_t)UINT64_C(0x8000000000000000))


#define FF_COMPLIANCE_VERY_STRICT   2 ///< Strictly conform to an older more strict version of the spec or reference software.
#define FF_COMPLIANCE_STRICT        1 ///< Strictly conform to all the things in the spec no matter what consequences.
#define FF_COMPLIANCE_NORMAL        0
#define FF_COMPLIANCE_UNOFFICIAL   -1 ///< Allow unofficial extensions
#define FF_COMPLIANCE_EXPERIMENTAL -2 ///< Allow nonstandardized experimental things.

/**
 * Seeking works like for a local file.
 */
#define AVIO_SEEKABLE_NORMAL (1 << 0)

/**
 * Seeking by timestamp with avio_seek_time() is possible.
 */
#define AVIO_SEEKABLE_TIME   (1 << 1)

#define AV_FOURCC_MAX_STRING_SIZE 32

/**
 * Fill the provided buffer with a string containing a FourCC (four-character
 * code) representation.
 *
 * @param buf    a buffer with size in bytes of at least AV_FOURCC_MAX_STRING_SIZE
 * @param fourcc the fourcc to represent
 * @return the buffer in input
 */
//char *av_fourcc_make_string(char *buf, uint32_t fourcc);

//#define av_fourcc2str(fourcc) av_fourcc_make_string((char[AV_FOURCC_MAX_STRING_SIZE]){0}, fourcc)



#define AVFMT_FLAG_GENPTS       0x0001 ///< Generate missing pts even if it requires parsing future frames.
#define AVFMT_FLAG_IGNIDX       0x0002 ///< Ignore index.
#define AVFMT_FLAG_NONBLOCK     0x0004 ///< Do not block when reading packets from input.
#define AVFMT_FLAG_IGNDTS       0x0008 ///< Ignore DTS on frames that contain both DTS & PTS
#define AVFMT_FLAG_NOFILLIN     0x0010 ///< Do not infer any values from other values, just return what is stored in the container
#define AVFMT_FLAG_NOPARSE      0x0020 ///< Do not use AVParsers, you also must set AVFMT_FLAG_NOFILLIN as the fillin code works on frames and no parsing -> no frames. Also seeking to frames can not work if parsing to find frame boundaries has been disabled
#define AVFMT_FLAG_NOBUFFER     0x0040 ///< Do not buffer frames when possible
#define AVFMT_FLAG_CUSTOM_IO    0x0080 ///< The caller has supplied a custom AVIOContext, don't avio_close() it.
#define AVFMT_FLAG_DISCARD_CORRUPT  0x0100 ///< Discard frames marked corrupted
#define AVFMT_FLAG_FLUSH_PACKETS    0x0200 ///< Flush the AVIOContext every packet.

#define AVFMT_EVENT_FLAG_METADATA_UPDATED 0x0001 ///< The call resulted in updated metadata.

#define AV_DICT_MATCH_CASE      1   /**< Only get an entry with exact-case key match. Only relevant in av_dict_get(). */
 #define AV_DICT_IGNORE_SUFFIX   2   /**< Return first entry in a dictionary whose first part corresponds to the search key,
                                         ignoring the suffix of the found key string. Only relevant in av_dict_get(). */
#define AV_DICT_DONT_STRDUP_KEY 4   /**< Take ownership of a key that's been
                                         allocated with av_malloc() or another memory allocation function. */
#define AV_DICT_DONT_STRDUP_VAL 8   /**< Take ownership of a value that's been
                                         allocated with av_malloc() or another memory allocation function. */
#define AV_DICT_DONT_OVERWRITE 16   ///< Don't overwrite existing entries.
#define AV_DICT_APPEND         32   /**< If the entry already exists, append to it.  Note that no
                                      delimiter is added, the strings are simply concatenated. */
#define AV_DICT_MULTIKEY       64   /**< Allow to store several equal keys in the dictionary */


#  define stat _stati64

class Common{
public:
static int av_toupper(int c);
static int av_strstart(const char *str, const char *pfx, const char **ptr);
};

#   define AV_RL32(x)                                \
    (((uint32_t)((const uint8_t*)(x))[3] << 24) |    \
               (((const uint8_t*)(x))[2] << 16) |    \
               (((const uint8_t*)(x))[1] <<  8) |    \
                ((const uint8_t*)(x))[0])


#endif