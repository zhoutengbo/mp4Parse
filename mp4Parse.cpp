#include<sys/types.h>
#include<sys/stat.h>
#include<unistd.h>
#include<fcntl.h>
#include "mp4Parse.h"
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <inttypes.h>
#include <stdlib.h>
#include "IOContext.h"
#include "common.h"
#include "Stream.h"

static const uint32_t mac_to_unicode[128] = {
    0x00C4,0x00C5,0x00C7,0x00C9,0x00D1,0x00D6,0x00DC,0x00E1,
    0x00E0,0x00E2,0x00E4,0x00E3,0x00E5,0x00E7,0x00E9,0x00E8,
    0x00EA,0x00EB,0x00ED,0x00EC,0x00EE,0x00EF,0x00F1,0x00F3,
    0x00F2,0x00F4,0x00F6,0x00F5,0x00FA,0x00F9,0x00FB,0x00FC,
    0x2020,0x00B0,0x00A2,0x00A3,0x00A7,0x2022,0x00B6,0x00DF,
    0x00AE,0x00A9,0x2122,0x00B4,0x00A8,0x2260,0x00C6,0x00D8,
    0x221E,0x00B1,0x2264,0x2265,0x00A5,0x00B5,0x2202,0x2211,
    0x220F,0x03C0,0x222B,0x00AA,0x00BA,0x03A9,0x00E6,0x00F8,
    0x00BF,0x00A1,0x00AC,0x221A,0x0192,0x2248,0x2206,0x00AB,
    0x00BB,0x2026,0x00A0,0x00C0,0x00C3,0x00D5,0x0152,0x0153,
    0x2013,0x2014,0x201C,0x201D,0x2018,0x2019,0x00F7,0x25CA,
    0x00FF,0x0178,0x2044,0x20AC,0x2039,0x203A,0xFB01,0xFB02,
    0x2021,0x00B7,0x201A,0x201E,0x2030,0x00C2,0x00CA,0x00C1,
    0x00CB,0x00C8,0x00CD,0x00CE,0x00CF,0x00CC,0x00D3,0x00D4,
    0xF8FF,0x00D2,0x00DA,0x00DB,0x00D9,0x0131,0x02C6,0x02DC,
    0x00AF,0x02D8,0x02D9,0x02DA,0x00B8,0x02DD,0x02DB,0x02C7,
};

int64_t Cmp4Parse::io_seek(void *opaque, int64_t offset, int whence)
{
    if (opaque == NULL)
        return -1;

     Cmp4Parse *ptr = (Cmp4Parse *)opaque;
   //  printf("offset=%d\n",offset );
     return lseek(ptr->m_fb,offset,whence);
}

int Cmp4Parse::io_read_packet(void *opaque, uint8_t *buf, int buf_size)
{
    int ret = -1;
    if (opaque == NULL||buf == NULL || buf_size < 0)
        return ret;
    Cmp4Parse *ptr = (Cmp4Parse *)opaque;

    ret = read(ptr->m_fb,buf,buf_size);
    if (ret <= 0)
        return AVERROR_EOF;
    //printf("ret=%d\n",ret );
    return ret;
}

MOVParseTableEntry Cmp4Parse::mov_default_parse_table[] = {
    { MKTAG('d','i','n','f'), Cmp4Parse::mov_read_default },
    { MKTAG('e','d','t','s'), Cmp4Parse::mov_read_default },
    { MKTAG('m','d','i','a'), Cmp4Parse::mov_read_default },
    { MKTAG('m','i','n','f'), Cmp4Parse::mov_read_default },
    { MKTAG('m','v','e','x'), Cmp4Parse::mov_read_default },
    { MKTAG('s','t','b','l'), Cmp4Parse::mov_read_default },
    { MKTAG('t','r','a','f'), Cmp4Parse::mov_read_default },
    { MKTAG('t','r','a','k'), Cmp4Parse::mov_read_trak },
    { MKTAG('t','r','u','n'), Cmp4Parse::mov_read_trun },
    { MKTAG('t','r','e','f'), Cmp4Parse::mov_read_default },
    { MKTAG('u','d','t','a'), Cmp4Parse::mov_read_default },
    { MKTAG('s','i','n','f'), Cmp4Parse::mov_read_default },
    { MKTAG('s','c','h','i'), Cmp4Parse::mov_read_default },
    { MKTAG('m','o','o','v'), Cmp4Parse::mov_read_moov },
    { MKTAG('m','e','t','a'), Cmp4Parse::mov_read_meta },
    { MKTAG('s','t','c','o'), Cmp4Parse::mov_read_stco},
    { MKTAG('s','t','s','c'), Cmp4Parse::mov_read_stsc},
    { MKTAG('s','t','s','z'), Cmp4Parse::mov_read_stsz},
    { MKTAG('s','t','s','d'), Cmp4Parse::mov_read_stsd},
    { MKTAG('a','v','c','C'), Cmp4Parse::mov_read_glbl},

   
    {0,0}
};


Cmp4Parse::Cmp4Parse()
{
    m_strict_std_compliance = 0;
	m_atom_depth = 0;

    m_moov_retry = 0;

    m_found_hdlr_mdta = 0;

    m_found_moov = 0;

    m_found_mdat = 0;

    m_flags = 0;

    m_next_root_atom = 0;

    memset(&m_frag_index,0,sizeof(m_frag_index));

    m_export_xmp = 0;

    m_meta_keys = NULL;

    m_meta_keys_count = 0;

    m_event_flags = 0;

    m_metadata = NULL;

    m_trak_index = -1;

    m_nb_streams = 0;

    m_max_streams = 3;

    m_fb = -1;

    m_pb = NULL;

    m_streams = NULL;
}

Cmp4Parse::~Cmp4Parse()
{
    if (m_fb) {
        close(m_fb);
        m_fb = -1;
    }
}


int Cmp4Parse::init_input(int flags,const char *filename,
                      AVDictionary **options)
{
    int access;
    unsigned int mode = 0;
    int fd;

    Common::av_strstart(filename, "file:", &filename);

    if (flags & AVIO_FLAG_WRITE && flags & AVIO_FLAG_READ) {
        access = O_CREAT | O_RDWR;
    } else if (flags & AVIO_FLAG_WRITE) {
        access = O_CREAT | O_WRONLY;
    } else {
        access = O_RDONLY;
    }

    fd = open(filename, flags, mode);

    if (fd > 0) {
        m_fb = fd;
        return 0;
    } else {
        return -1;
    }
}


#if 1



int Cmp4Parse::mov_read_header()
{
    int j, err;
    MOVAtom atom = { AV_RL32("root") };
    int i;

    if (m_pb == NULL)
        return -1;

    /**
    * @原代码中有关于解密的判断，这里先去掉
    */
    m_trak_index = -1;

     /* .mov and .mp4 aren't streamable anyway (only progressive download if moov is before mdat) */
    /**
    * 如果moov在 mdat前，只是进度性下载
    */
    if (m_pb->m_seekable & AVIO_SEEKABLE_NORMAL) {
        atom.size = m_pb->avio_size();
        printf("size:%d\n",(int)atom.size );
    }
    else
        atom.size = INT_MAX;

    /* check MOV header */
    do {
        if (m_moov_retry)
            m_pb->avio_seek(0, SEEK_SET);
     //   printf("------1------\n");
        if ((err = mov_read_default(this, m_pb, atom)) < 0) {
             printf("error reading header\n");
          //  mov_read_close(s);
            return err;
        }
        //printf("------2------%d\n",m_moov_retry);
    } while ((m_pb->m_seekable & AVIO_SEEKABLE_NORMAL) && !m_found_moov && !m_moov_retry++);

    return 0;
}

int Cmp4Parse::setIOContext(CIOContext* pb)
{
    m_pb = pb;
    return 0;
}

int Cmp4Parse::mov_read_packet(Cmp4Parse *s, AVPacket *pkt)
{
    MOVStreamContext *sc;
    AVIndexEntry *sample;
    CStream *st = NULL;
    int64_t current_index;
    int ret;
 retry:
    sample = mov_find_next_sample(s, &st);
    if (!sample) {
        return -1;
    }   
    sc = (MOVStreamContext*)st->m_priv_data;
   // printf("sample.size:%d pos:%lld\n",sample->size,sample->pos );
    
    mov_current_sample_inc(sc);

    int64_t ret64 = s->m_pb->avio_seek( sample->pos, SEEK_SET);

    if (ret64 != sample->pos) {
           printf ("stream %d, offset 0x%"PRIx64": partial file\n",
                   sc->ffindex, sample->pos);
        return AVERROR_INVALIDDATA;
    }
   // current_index = sc->m_current_index;
//    mov_current_sample_inc(sc);
    ret =  s->m_pb->av_get_packet( pkt, sample->size);
    pkt->pos = sample->pos;

    return 0;
}

AVIndexEntry *Cmp4Parse::mov_find_next_sample(Cmp4Parse *s, CStream **st)
{
    AVIndexEntry *sample = NULL;
    int i;
     for (i = 0; i < s->m_nb_streams; i++) {
         CStream *avst = s->m_streams[i];
         MOVStreamContext *msc = (MOVStreamContext *)avst->m_priv_data;
         if (s->m_pb && msc->current_sample < avst->m_nb_index_entries) {
            AVIndexEntry *current_sample = &avst->m_index_entries[ msc->current_sample];
             sample = current_sample;
            *st = avst;
         }

     }

    return sample;
}
void Cmp4Parse::mov_current_sample_inc(MOVStreamContext *sc)
{
    sc->current_sample++;
   // sc->current_index++; 
   /* if (sc->index_ranges &&
        sc->current_index >= sc->current_index_range->end &&
        sc->current_index_range->end) {
        sc->current_index_range++;
        sc->current_index = sc->current_index_range->start;
    }*/
}

int Cmp4Parse::mov_read_default(Cmp4Parse* cxt,CIOContext* pb,MOVAtom atom)
{
	int64_t total_size = 0;
    MOVAtom a;
    int i;

    if (cxt->m_atom_depth > 10) {
       printf("Atoms too deeply nested\n");
        return AVERROR_INVALIDDATA;
    }

    cxt->m_atom_depth ++;

    if (atom.size < 0)
        atom.size = INT_MAX;

    while (total_size <= atom.size - 8 && !pb->avio_feof()) {

       int (*parse)(Cmp4Parse *cxt, CIOContext *pb,MOVAtom atom) = NULL;
        a.size = atom.size;
        a.type=0;
        if (atom.size >= 8) {
            a.size = pb->avio_rb32();
            

            a.type = pb->avio_rl32();
           // printf(" a.size :%d --------0---------:%c %c %c %c\n", a.size ,*((char*)&a.type + 0),*((char*)&a.type + 1),*((char*)&a.type + 2),*((char*)&a.type + 3) );
            if (a.type == MKTAG('f','r','e','e') &&
                a.size >= 8 &&
                cxt->m_strict_std_compliance < FF_COMPLIANCE_STRICT &&
                cxt->m_moov_retry) {
                uint8_t buf[8];
                uint32_t *type = (uint32_t *)buf + 1;
                if (pb->avio_read(buf, 8) != 8)
                    return AVERROR_INVALIDDATA;
                pb->avio_seek(-8, SEEK_CUR);
                if (*type == MKTAG('m','v','h','d') ||
                    *type == MKTAG('c','m','o','v')) {
                    printf("Detected moov in a free atom.\n");
                    a.type = MKTAG('m','o','o','v');
                }
            }
            if (atom.type != MKTAG('r','o','o','t') &&
                atom.type != MKTAG('m','o','o','v'))
            {
                if (a.type == MKTAG('t','r','a','k') || a.type == MKTAG('m','d','a','t'))
                {
                    printf("Broken file, trak/mdat not at top-level\n");
                    pb->avio_skip(-8);
                    cxt->m_atom_depth --;
                    return 0;
                }
            }
            total_size += 8;
            if (a.size == 1 && total_size + 8 <= atom.size) { /* 64 bit extended size */
                a.size = pb->avio_rb64() - 8;
                total_size += 8;
            }
        }

     //   printf( "type:'%s' parent:'%s' sz: %"PRId64" %"PRId64" %"PRId64"\n",
     //         av_fourcc2str(a.type), av_fourcc2str(atom.type), a.size, total_size, atom.size);

        if (a.size == 0) {

            a.size = atom.size - total_size + 8;
         //   printf("--------1----:%d atom.size:%d total_size:%d\n", a.size ,atom.size,total_size );
        }
        a.size -= 8;
        if (a.size < 0)
            break;
        a.size = FFMIN(a.size, atom.size - total_size);

        for (i = 0; mov_default_parse_table[i].type; i++)
            if (mov_default_parse_table[i].type == a.type) {
                parse = mov_default_parse_table[i].parse;
                break;
            }

         // container is user data
        if (!parse && (atom.type == MKTAG('u','d','t','a') ||
                       atom.type == MKTAG('i','l','s','t')))
            parse =  Cmp4Parse::mov_read_udta_string;

        // Supports parsing the QuickTime Metadata Keys.
        // https://developer.apple.com/library/mac/documentation/QuickTime/QTFF/Metadata/Metadata.html
        if (!parse && cxt->m_found_hdlr_mdta &&
            atom.type == MKTAG('m','e','t','a') &&
            a.type == MKTAG('k','e','y','s')) {
            parse = Cmp4Parse::mov_read_keys;
        }

        if (!parse) { /* skip leaf atoms data */
          //  printf("===========> skip - %d (%c %c %c %c)\n",a.size,*((char*)&a.type + 0),*((char*)&a.type + 1),*((char*)&a.type + 2),*((char*)&a.type + 3));
            pb->avio_skip(a.size);
        } else {
            int64_t start_pos = pb->avio_tell();
            int64_t left;
            int err = parse(cxt, pb, a);
            if (err < 0) {
                printf(" errout:%c %c %c %c m_atom_depth:%d ,err:%d\n" ,*((char*)&a.type + 0),*((char*)&a.type + 1),*((char*)&a.type + 2),*((char*)&a.type + 3),cxt->m_atom_depth,err );
                cxt->m_atom_depth --;
                return err;
            }
            if (cxt->m_found_moov && cxt->m_found_mdat &&
                ((!(pb->m_seekable & AVIO_SEEKABLE_NORMAL) || cxt->m_flags & AVFMT_FLAG_IGNIDX || cxt->m_frag_index.complete) ||
                 start_pos + a.size == pb->avio_size())) {
                if (!(pb->m_seekable & AVIO_SEEKABLE_NORMAL) || cxt->m_flags & AVFMT_FLAG_IGNIDX || cxt->m_frag_index.complete)
                    cxt->m_next_root_atom = start_pos + a.size;
                cxt->m_atom_depth --;
                return 0;
            }
            left = a.size - pb->avio_tell() + start_pos;
            if (left > 0) /* skip garbage at atom end */
                pb->avio_skip(left);
            else if (left < 0) {
                printf(
                       "overread end of atom '%.4s' by %"PRId64" bytes\n",
                       (char*)&a.type, -left);
                pb->avio_seek(left, SEEK_CUR);
            }
        }
         
        total_size += a.size;
       //  printf("total_size:%d a.size:%d\n",total_size,a.size );
    }

    if (total_size < atom.size && atom.size < 0x7ffff)
        pb->avio_skip(atom.size - total_size);

    cxt->m_atom_depth --;

    return 0;
}

int Cmp4Parse::mov_read_udta_string(Cmp4Parse *cxt, CIOContext *pb, MOVAtom atom)
{
    char tmp_key[5];
    char key2[32], language[4] = {0};
    char *str = NULL;
    const char *key = NULL;
    uint16_t langcode = 0;
    uint32_t data_type = 0, str_size, str_size_alloc;
    int (*parse)(Cmp4Parse*, CIOContext*, unsigned, const char*) = NULL;
    int raw = 0;
    int num = 0;

    switch (atom.type) {
    case MKTAG( '@','P','R','M'): key = "premiere_version"; raw = 1; break;
    case MKTAG( '@','P','R','Q'): key = "quicktime_version"; raw = 1; break;
    case MKTAG( 'X','M','P','_'):
        if (cxt->m_export_xmp) { key = "xmp"; raw = 1; } break;
    case MKTAG( 'a','A','R','T'): key = "album_artist";    break;
    case MKTAG( 'a','k','I','D'): key = "account_type";
        parse = mov_metadata_int8_no_padding; break;
    case MKTAG( 'a','p','I','D'): key = "account_id"; break;
    case MKTAG( 'c','a','t','g'): key = "category"; break;
    case MKTAG( 'c','p','i','l'): key = "compilation";
        parse = mov_metadata_int8_no_padding; break;
    case MKTAG( 'c','p','r','t'): key = "copyright"; break;
    case MKTAG( 'd','e','s','c'): key = "description"; break;
    case MKTAG( 'd','i','s','k'): key = "disc";
        parse = mov_metadata_track_or_disc_number; break;
    case MKTAG( 'e','g','i','d'): key = "episode_uid";
        parse = mov_metadata_int8_no_padding; break;
    case MKTAG( 'F','I','R','M'): key = "firmware"; raw = 1; break;
    case MKTAG( 'g','n','r','e'): key = "genre";
        parse = mov_metadata_gnre; break;
    case MKTAG( 'h','d','v','d'): key = "hd_video";
        parse = mov_metadata_int8_no_padding; break;
    case MKTAG( 'H','M','M','T'):
        return mov_metadata_hmmt(cxt, pb, atom.size);
    case MKTAG( 'k','e','y','w'): key = "keywords";  break;
    case MKTAG( 'l','d','e','s'): key = "synopsis";  break;
    case MKTAG( 'l','o','c','i'):
        return mov_metadata_loci(cxt, pb, atom.size);
    case MKTAG( 'm','a','n','u'): key = "make"; break;
    case MKTAG( 'm','o','d','l'): key = "model"; break;
    case MKTAG( 'p','c','s','t'): key = "podcast";
        parse = mov_metadata_int8_no_padding; break;
    case MKTAG( 'p','g','a','p'): key = "gapless_playback";
        parse = mov_metadata_int8_no_padding; break;
    case MKTAG( 'p','u','r','d'): key = "purchase_date"; break;
    case MKTAG( 'r','t','n','g'): key = "rating";
        parse = mov_metadata_int8_no_padding; break;
    case MKTAG( 's','o','a','a'): key = "sort_album_artist"; break;
    case MKTAG( 's','o','a','l'): key = "sort_album";   break;
    case MKTAG( 's','o','a','r'): key = "sort_artist";  break;
    case MKTAG( 's','o','c','o'): key = "sort_composer"; break;
    case MKTAG( 's','o','n','m'): key = "sort_name";    break;
    case MKTAG( 's','o','s','n'): key = "sort_show";    break;
    case MKTAG( 's','t','i','k'): key = "media_type";
        parse = mov_metadata_int8_no_padding; break;
    case MKTAG( 't','r','k','n'): key = "track";
        parse = mov_metadata_track_or_disc_number; break;
    case MKTAG( 't','v','e','n'): key = "episode_id"; break;
    case MKTAG( 't','v','e','s'): key = "episode_sort";
        parse = mov_metadata_int8_bypass_padding; break;
    case MKTAG( 't','v','n','n'): key = "network";   break;
    case MKTAG( 't','v','s','h'): key = "show";      break;
    case MKTAG( 't','v','s','n'): key = "season_number";
        parse = mov_metadata_int8_bypass_padding; break;
    case MKTAG(0xa9,'A','R','T'): key = "artist";    break;
    case MKTAG(0xa9,'P','R','D'): key = "producer";  break;
    case MKTAG(0xa9,'a','l','b'): key = "album";     break;
    case MKTAG(0xa9,'a','u','t'): key = "artist";    break;
    case MKTAG(0xa9,'c','h','p'): key = "chapter";   break;
    case MKTAG(0xa9,'c','m','t'): key = "comment";   break;
    case MKTAG(0xa9,'c','o','m'): key = "composer";  break;
    case MKTAG(0xa9,'c','p','y'): key = "copyright"; break;
    case MKTAG(0xa9,'d','a','y'): key = "date";      break;
    case MKTAG(0xa9,'d','i','r'): key = "director";  break;
    case MKTAG(0xa9,'d','i','s'): key = "disclaimer"; break;
    case MKTAG(0xa9,'e','d','1'): key = "edit_date"; break;
    case MKTAG(0xa9,'e','n','c'): key = "encoder";   break;
    case MKTAG(0xa9,'f','m','t'): key = "original_format"; break;
    case MKTAG(0xa9,'g','e','n'): key = "genre";     break;
    case MKTAG(0xa9,'g','r','p'): key = "grouping";  break;
    case MKTAG(0xa9,'h','s','t'): key = "host_computer"; break;
    case MKTAG(0xa9,'i','n','f'): key = "comment";   break;
    case MKTAG(0xa9,'l','y','r'): key = "lyrics";    break;
    case MKTAG(0xa9,'m','a','k'): key = "make";      break;
    case MKTAG(0xa9,'m','o','d'): key = "model";     break;
    case MKTAG(0xa9,'n','a','m'): key = "title";     break;
    case MKTAG(0xa9,'o','p','e'): key = "original_artist"; break;
    case MKTAG(0xa9,'p','r','d'): key = "producer";  break;
    case MKTAG(0xa9,'p','r','f'): key = "performers"; break;
    case MKTAG(0xa9,'r','e','q'): key = "playback_requirements"; break;
    case MKTAG(0xa9,'s','r','c'): key = "original_source"; break;
    case MKTAG(0xa9,'s','t','3'): key = "subtitle";  break;
    case MKTAG(0xa9,'s','w','r'): key = "encoder";   break;
    case MKTAG(0xa9,'t','o','o'): key = "encoder";   break;
    case MKTAG(0xa9,'t','r','k'): key = "track";     break;
    case MKTAG(0xa9,'u','r','l'): key = "URL";       break;
    case MKTAG(0xa9,'w','r','n'): key = "warning";   break;
    case MKTAG(0xa9,'w','r','t'): key = "composer";  break;
    case MKTAG(0xa9,'x','y','z'): key = "location";  break;
    }

    return 0;
}

int Cmp4Parse::mov_read_keys(Cmp4Parse *c, CIOContext *pb, MOVAtom atom)
{
    uint32_t count;
    uint32_t i;

    if (atom.size < 8)
        return 0;

    pb->avio_skip( 4);
    count = pb->avio_rb32();
    if (count > UINT_MAX / sizeof(*c->m_meta_keys) - 1) {
       printf("The 'keys' atom with the invalid key count: %"PRIu32"\n", count);
        return AVERROR_INVALIDDATA;
    }

    c->m_meta_keys_count = count + 1;
    c->m_meta_keys = (char**)malloc(c->m_meta_keys_count * sizeof(*c->m_meta_keys));
    if (!c->m_meta_keys)
        return AVERROR(ENOMEM);

    for (i = 1; i <= count; ++i) {
        uint32_t key_size = pb->avio_rb32();
        uint32_t type = pb->avio_rl32();
        if (key_size < 8) {
            printf(
                   "The key# %"PRIu32" in meta has invalid size:"
                   "%"PRIu32"\n", i, key_size);
            return AVERROR_INVALIDDATA;
        }
        key_size -= 8;
        if (type != MKTAG('m','d','t','a')) {
            pb->avio_skip( key_size);
        }
        c->m_meta_keys[i] = (char*)malloc(key_size + 1);
        if (!c->m_meta_keys[i])
            return AVERROR(ENOMEM);
        pb->avio_read( (unsigned char*)c->m_meta_keys[i], key_size);
    }

    return 0;
}

AVDictionaryEntry* Cmp4Parse::av_dict_get(const AVDictionary *m, const char *key,
                               const AVDictionaryEntry *prev, int flags)
{
    unsigned int i, j;

    if (!m)
        return NULL;

    if (prev)
        i = prev - m->elems + 1;
    else
        i = 0;

    for (; i < m->count; i++) {
        const char *s = m->elems[i].key;
        if (flags & AV_DICT_MATCH_CASE)
            for (j = 0; s[j] == key[j] && key[j]; j++)
                ;
        else
            for (j = 0; Common::av_toupper(s[j]) == Common::av_toupper(key[j]) && key[j]; j++)
                ;
        if (key[j])
            continue;
        if (s[j] && !(flags & AV_DICT_IGNORE_SUFFIX))
            continue;
        return &m->elems[i];
    }
    return NULL;
}


int Cmp4Parse::mov_read_moov(Cmp4Parse *c, CIOContext *pb,MOVAtom atom)
{
    int ret;

    if (c->m_found_moov) {
       printf("Found duplicated MOOV Atom. Skipped it\n");
        pb->avio_skip(atom.size);
        return 0;
    }

    if ((ret = mov_read_default(c, pb, atom)) < 0)
        return ret;
    /* we parsed the 'moov' atom, we can terminate the parsing as soon as we find the 'mdat' */
    /* so we don't parse the whole file if over a network */
    c->m_found_moov=1;
    return 0; /* now go for mdat */
}
int Cmp4Parse::mov_read_meta(Cmp4Parse *c, CIOContext *pb, MOVAtom atom)
{
      printf("in\n");
    while (atom.size > 8) {
        uint32_t tag = pb->avio_rl32();
        atom.size -= 4;
        if (tag == MKTAG('h','d','l','r')) {

            pb->avio_seek( -8, SEEK_CUR);
            atom.size += 8;
            return mov_read_default(c, pb, atom);
        }
    }
    return 0;
}

int Cmp4Parse::mov_read_trak(Cmp4Parse *c, CIOContext *pb, MOVAtom atom)
{
    CStream *st;
    MOVStreamContext *sc;
    int ret;

    st = CStream::avformat_new_stream(c, NULL);
    st->m_id = -1;
    sc = (MOVStreamContext *)malloc(sizeof(MOVStreamContext));
    if (!sc) return AVERROR(ENOMEM);
    memset(sc,0,sizeof(MOVStreamContext));
    st->m_priv_data = sc;
    //st->codecpar->codec_type = AVMEDIA_TYPE_DATA;
    sc->ffindex = st->m_index;
    c->m_trak_index = st->m_index;

    if ((ret = mov_read_default(c, pb, atom)) < 0)
        return ret;


    mov_build_index(c, st);

    //printf("*****************\n" );

    return 0;
}

void Cmp4Parse::mov_build_index(Cmp4Parse *mov, CStream *st)
{
    MOVStreamContext *sc = (MOVStreamContext*)st->m_priv_data;
    int64_t current_offset;
    int64_t last_current_offset = 0;
    int64_t current_dts = 0;
    unsigned int stts_index = 0;
    unsigned int stsc_index = 0;
    unsigned int stss_index = 0;
    unsigned int stps_index = 0;
    unsigned int i, j;
    uint64_t stream_size = 0;

    unsigned chunk_samples, total = 0;
    unsigned current_samples = 0;

    if (!sc->chunk_count)
        return;

// compute total chunk count
    for (i = 0; i < sc->stsc_count; i++) {
        unsigned count, chunk_count;

        chunk_samples = sc->stsc_data[i].count;
       // if (i != sc->stsc_count - 1) {
            //printf( "error unaligned chunk\n");
            //return;
        //}

        //if (sc->samples_per_frame >= 160) { // gsm
//            count = chunk_samples / sc->samples_per_frame;
  //      } else if (sc->samples_per_frame > 1) {
    //        unsigned samples = (1024/sc->samples_per_frame)*sc->samples_per_frame;
      //      count = (chunk_samples+samples-1) / samples;
        //} else {
            count = (chunk_samples+1023) / 1024;
        //}

        if (i < (sc->stsc_count - 1))
            chunk_count = sc->stsc_data[i+1].first - sc->stsc_data[i].first;
        else
            chunk_count = sc->chunk_count - (sc->stsc_data[i].first - 1);
      //  printf("chunk_count - %d count:%d sc->stsc_data[%d].first:%d - sc->stsc_data[%d].first:%d \n",chunk_count,count,i+1,sc->stsc_data[i+1].first,i,sc->stsc_data[i].first);
        total += chunk_count * count;
    }

   
    if (total >= UINT_MAX / sizeof(*st->m_index_entries) - st->m_nb_index_entries)
        return;

    if (st->m_index_entries == NULL) {
        st->m_index_entries = (AVIndexEntry*)malloc((st->m_nb_index_entries + total) *sizeof(*st->m_index_entries));
    }else if (realloc(&st->m_index_entries,
                (st->m_nb_index_entries + total) *sizeof(*st->m_index_entries)) < 0) {
        return;
        st->m_nb_index_entries = 0;
    }

    st->m_index_entries_allocated_size = (st->m_nb_index_entries + total) * sizeof(*st->m_index_entries);

    // populate index
    for (i = 0; i < sc->chunk_count; i++) {
      current_offset = sc->chunk_offsets[i];
      if (stsc_index < (sc->stsc_count -1 ) &&
            i + 1 == sc->stsc_data[stsc_index + 1].first)
            stsc_index++;
        chunk_samples = sc->stsc_data[stsc_index].count;
        while (chunk_samples > 0) {
            AVIndexEntry *e;
            unsigned size = 0, samples;

           // if (sc->samples_per_frame > 1 && !sc->bytes_per_frame) {
            //    printf("Zero bytes per frame, but %d samples per frame",sc->samples_per_frame);
             //   return;
            //}
            //if (sc->samples_per_frame >= 160) { // gsm
            //    samples = sc->samples_per_frame;
            //    size = sc->bytes_per_frame;
            //} else {
                //if (sc->samples_per_frame > 1) {
                //    samples = FFMIN((1024 / sc->samples_per_frame)*
                //                    sc->samples_per_frame, chunk_samples);
                //    size = (samples / sc->samples_per_frame) * sc->bytes_per_frame;
                //} else {
                     samples = FFMIN(1024, chunk_samples);
                    // size = samples * sc->sample_size;
                     for (int k = current_samples;k < current_samples + samples;k++)
                     {
                       size += sc->sample_sizes[k];
                     }
                     current_samples += samples;
                //}
            //}

                if (st->m_nb_index_entries >= total) {
                    printf( "wrong chunk count %u\n", total);
                    return;
                }
                if (size > 0x3FFFFFFF) {
                    printf( "Sample size %u is too large\n", size);
                    return;
                }
                e = &st->m_index_entries[st->m_nb_index_entries++];
                e->pos = current_offset;
                e->timestamp = current_dts;
                e->size = size;
                e->min_distance = 0;
                e->flags = AVINDEX_KEYFRAME;
                

                current_offset += size;
                current_dts += samples;
                chunk_samples -= samples;
                //if (last_current_offset != 0) {
           //         printf( "AVIndex stream %d, chunk %u, offset %"PRIx64", dts %"PRId64", "
             //          "size %u, duration %u  crrent_size %u\n", st->m_index, i, current_offset, current_dts,
               //        current_offset - last_current_offset , samples ,size);
                //}
                last_current_offset = current_offset;
                
        }
    }

    printf("chunk count %u\n", total);
}

int Cmp4Parse::mov_read_trun(Cmp4Parse *c, CIOContext *pb, MOVAtom atom)
{
    return 0;
}

int Cmp4Parse::mov_read_stsz(Cmp4Parse *c, CIOContext *pb, MOVAtom atom)
{
    CStream *st;
    MOVStreamContext *sc;
    unsigned int i, entries, sample_size, field_size, num_bytes;
    unsigned char* buf;
    int ret;

    /**
    * @ 在找到trak时已经建立了CStream
    */
     if (c->m_nb_streams < 1)
        return 0;
    st = c->m_streams[c->m_nb_streams-1];
    sc = (MOVStreamContext*)st->m_priv_data;

    pb->avio_r8(); /* version */
    pb->avio_rb24(); /* flags */


     if (atom.type == MKTAG('s','t','s','z')) {
        sample_size = pb->avio_rb32();
        if (!sc->sample_size) /* do not overwrite value computed in stsd */
            sc->sample_size = sample_size;
        sc->stsz_sample_size = sample_size;
        field_size = 32;
    } else {
        sample_size = 0;
        pb->avio_rb24(); /* reserved */
        field_size = pb->avio_r8();
    }

    entries = pb->avio_rb32();

   // printf("sample_size = %u sample_count = %u field_size=%d\n", sc->sample_size, entries,field_size);

    sc->sample_count = entries;
    if (sample_size)
        return 0;

    if (field_size != 4 && field_size != 8 && field_size != 16 && field_size != 32) {
        printf("Invalid sample field size %u\n", field_size);
        return AVERROR_INVALIDDATA;
    }

    if (!entries)
        return 0;

    if (entries >= (UINT_MAX - 4) / field_size)
        return AVERROR_INVALIDDATA;

    if (sc->sample_sizes)
        printf("Duplicated STSZ atom\n");

    free(sc->sample_sizes);
    sc->sample_count = 0;
    sc->sample_sizes = (int*)malloc(entries *sizeof(*sc->sample_sizes));
    if (!sc->sample_sizes)
        return AVERROR(ENOMEM);

  /*  num_bytes = (entries*field_size+4)>>3;
    printf("num_bytes = %d entries=%d\n",num_bytes,entries );

    buf = (unsigned char*)malloc(num_bytes+AV_INPUT_BUFFER_PADDING_SIZE);
    if (!buf) {
        free(&sc->sample_sizes);
        return AVERROR(ENOMEM);
    }

    ret = pb->ffio_read_size(buf, num_bytes);
    if (ret < 0) {
        free(sc->sample_sizes);
        free(buf);
        printf("STSZ atom truncated\n");
        return 0;
    }*/

    for (i = 0; i < entries && !pb->m_eof_reached; i++) {
        sc->sample_sizes[i] =pb->avio_rb32();
  //      printf("sc->sample_sizes[%d]:%d\n",i,sc->sample_sizes[i] );
        sc->data_size += sc->sample_sizes[i];
    }


    sc->sample_count = i;

    if (pb->m_eof_reached) {
        printf( "reached eof, corrupted STSZ atom\n");
        return AVERROR_EOF;
    }



    return 0;
}

int Cmp4Parse::mov_read_stsd(Cmp4Parse *c, CIOContext *pb, MOVAtom atom)
{
    CStream *st;
    MOVStreamContext *sc;
    int ret,entries;

    /**
    * @ 在找到trak时已经建立了CStream
    */
    if (c->m_nb_streams < 1)
        return 0;
    st = c->m_streams[c->m_nb_streams-1];
    sc = (MOVStreamContext*)st->m_priv_data;

    pb->avio_r8(); /* version */
    pb->avio_rb24(); /* flags */

    entries = pb->avio_rb32();

    /* Each entry contains a size (4 bytes) and format (4 bytes). */
    if (entries <= 0 || entries > atom.size / 8) {
        printf("invalid STSD entries %d\n", entries);
        return AVERROR_INVALIDDATA;
    }

    if (sc->extradata) {
        printf("Duplicate stsd found in this track.\n");
        return AVERROR_INVALIDDATA;
    }

    /* Prepare space for hosting multiple extradata. */
    sc->extradata = (uint8_t**)malloc(entries*sizeof(*sc->extradata));
    if (!sc->extradata)
        return AVERROR(ENOMEM);

    sc->extradata_size = (int*)malloc(entries*sizeof(*sc->extradata_size));
    if (!sc->extradata_size) {
        ret = AVERROR(ENOMEM);
        goto fail;
    }

    /* Restore back the primary extradata. */
    if (st->m_codec->m_extradata) {
        free(st->m_codec->m_extradata);    
        st->m_codec->m_extradata = NULL;
    }

    ret = ff_mov_read_stsd_entries(c, pb, entries);
    if (ret < 0)
        goto fail;
    printf("entries:%d\n",entries );



    //st->m_codec->m_extradata_size = sc->extradata_size[0];
    printf("sc->extradata_size[0]：%d\n",st->m_codec->m_extradata_size);
    if (sc->extradata_size[0]) {

        st->m_codec->m_extradata = (uint8_t*)malloc(sc->extradata_size[0] + AV_INPUT_BUFFER_PADDING_SIZE);
        if (!st->m_codec->m_extradata)
            return AVERROR(ENOMEM);
        memcpy(st->m_codec->m_extradata, sc->extradata[0], sc->extradata_size[0]);
    }
    

    printf("mov_read_stsd\n" );
    return 0;

fail:
    if (sc->extradata) {
        int j;
        for (j = 0; j < sc->stsd_count; j++) {
            free(sc->extradata[j]);
            sc->extradata[j]=NULL;
        }
    }

    free(sc->extradata);
    free(sc->extradata_size);
    sc->extradata=NULL;
    sc->extradata_size=NULL;
    return ret;
}

int Cmp4Parse::ff_mov_read_stsd_entries(Cmp4Parse *c, CIOContext *pb, int entries)
{
    CStream *st;
    MOVStreamContext *sc;

    int pseudo_stream_id;
    av_assert0 (c->m_nb_streams >= 1);
    st = c->m_streams[c->m_nb_streams-1];
    sc = (MOVStreamContext*)st->m_priv_data;

    for (pseudo_stream_id = 0;
         pseudo_stream_id < entries && !pb->m_eof_reached;
         pseudo_stream_id++) {

        //Parsing Sample description table
        //enum AVCodecID id;
        int ret, dref_id = 1;
        MOVAtom a = { AV_RL32("stsd") };
        int64_t start_pos = pb->avio_tell();
        int64_t size    = pb->avio_rb32(); /* size */
        uint32_t format = pb->avio_rl32(); /* data format */

        printf("%c%c%c%c size:%d\n", *(((char*)&format) + 0), *(((char*)&format) + 1), *(((char*)&format) + 2), *(((char*)&format) + 3),size );
         if (size >= 16) {
            pb->avio_rb32(); /* reserved */
            pb->avio_rb16(); /* reserved */
            dref_id = pb->avio_rb16();
        } else if (size <= 7) {
            printf("invalid size %"PRId64" in stsd\n", size);
            return AVERROR_INVALIDDATA;
        }

        /*
        * 跳过烂七八糟的字段 
        */
       // pb->avio_skip(size - (pb->avio_tell() - start_pos));
       // sc->stsd_count++;
        //continue;

        /**
        * @默认均为视频 无音频
        */
        mov_parse_stsd_video(c, pb, st, sc);


        /* this will read extra atoms at the end (wave, alac, damr, avcC, hvcC, SMI ...) */
        a.size = size - (pb->avio_tell() - start_pos);
        if (a.size > 8) {
            if ((ret = mov_read_default(c, pb, a)) < 0)
                return ret;
        } else if (a.size > 0)
            pb->avio_skip( a.size);


    }
    return 0;
}


int Cmp4Parse::ff_get_extradata(Codec *par, CIOContext *pb, int size)
{
    int ret = -1;
    par->m_extradata =(uint8_t*) malloc(size);
    if (par->m_extradata == NULL)
        return ret;
    ret = pb->avio_read(par->m_extradata, size);
    printf("size:%d\n",size );
    if (ret != size) {
        free(par->m_extradata);
        par->m_extradata == NULL;
        par->m_extradata_size = 0;
        printf("Failed to read extradata of size %d\n", size);
        return ret < 0 ? ret : AVERROR_INVALIDDATA;
    } 

    par->m_extradata_size = ret;
    return ret;
}

void Cmp4Parse::mov_parse_stsd_video(Cmp4Parse *c, CIOContext *pb,
                                 CStream *st, MOVStreamContext *sc)
{
    uint8_t codec_name[32] = { 0 };
    int64_t stsd_start;
    unsigned int len;
    int tmp, bit_depth, color_table_id, greyscale, i;

    /* The first 16 bytes of the video sample description are already
     * read in ff_mov_read_stsd_entries() */
    stsd_start = pb->avio_tell() - 16;

    pb->avio_rb16(); /* version */
    pb->avio_rb16(); /* revision level */
    pb->avio_rb32(); /* vendor */
    pb->avio_rb32(); /* temporal quality */
    pb->avio_rb32(); /* spatial quality */

    st->m_codec->m_width  = pb->avio_rb16(); /* width */
    st->m_codec->m_height = pb->avio_rb16(); /* height */

    

    pb->avio_rb32(); /* horiz resolution */
    pb->avio_rb32(); /* vert resolution */
    pb->avio_rb32(); /* data size, always 0 */
    pb->avio_rb16(); /* frames per samples */

    len = pb->avio_r8(); /* codec name, pascal string */
    if (len > 31)
        len = 31;

    mov_read_mac_string(c, pb, len, (char*)codec_name, sizeof(codec_name));

    if (len < 31)
        pb->avio_skip(31 - len);

  //  if (codec_name[0])
   //      av_dict_set(&st->metadata, "encoder", codec_name, 0);

    st->m_codec->m_bits_per_coded_sample = pb->avio_rb16(); /* depth */

    printf("width:%d height:%d m_bits_per_coded_sample:%d\n",st->m_codec->m_width,st->m_codec->m_height ,st->m_codec->m_bits_per_coded_sample);

    pb->avio_seek(stsd_start, SEEK_SET);

     pb->avio_seek( 82, SEEK_CUR);

     /* Get the bit depth and greyscale state */
    tmp = pb->avio_rb16();
    bit_depth = tmp & 0x1F;
    greyscale = tmp & 0x20;

    /* Get the color table ID */
    color_table_id = pb->avio_rb16();

    /* If the depth is 1, 2, 4, or 8 bpp, file is palettized. */
    if ((bit_depth == 1 || bit_depth == 2 || bit_depth == 4 || bit_depth == 8)) {
        printf("not support bit_depth:%d\n",bit_depth );        
        return;
    }

    printf("bit_depth:%d\n",bit_depth );

}

int Cmp4Parse::mov_read_mac_string(Cmp4Parse *c, CIOContext *pb, int len,
                               char *dst, int dstlen)
{
    char *p = dst;
    char *end = dst+dstlen-1;
    int i;

    for (i = 0; i < len; i++) {
        uint8_t t, c = pb->avio_r8();

        if (p >= end)
            continue;

        if (c < 0x80)
            *p++ = c;
        else if (p < end)
            PUT_UTF8(mac_to_unicode[c-0x80], t, if (p < end) *p++ = t;);
    }
    *p = 0;
    return p - dst;
}

int Cmp4Parse::get_sps_and_pps_from_mp4_avcc_extradata(Cmp4Parse * fmt_ctx, int stream_index, unsigned char *& sps_and_pps, int & sps_and_pps_length)
{

    Codec * video_dec_ctx = fmt_ctx->m_streams[stream_index]->m_codec;
    
    if (video_dec_ctx == NULL || video_dec_ctx->m_extradata_size <= 0 || video_dec_ctx->m_extradata == NULL){ return -1; }

    //ISO/IEC 14496-15 5.2.4.1.1 关于avcC的数据的定义
    // mp4 avcC box
    //0x604cd0:       0x01    0x4d    0x00    0x2a    0xff    0xe1    0x00    0x1a
    //0x604cd8:       0x67    0x4d    0x40    0x2a    0x96    0x52    0x01    0x40
    //0x604ce0:       0x7b    0x60    0x29    0x10    0x00    0x00    0x03    0x00
    //0x604ce8:       0x10    0x00    0x00    0x03    0x03    0xc9    0xda    0x16
    //0x604cf0:       0x2d    0x12    0x01    0x00    0x04    0x68    0xeb    0x73
    //0x604cf8:       0x52

    unsigned char * sps = NULL;
    unsigned char * pps = NULL;

    int sps_length = 0;
    int pps_length = 0;

    unsigned char * p = video_dec_ctx->m_extradata;

    unsigned char configurationVersion  = *p; p++;
    if (configurationVersion != 1){ sps_and_pps_length = 0; return -1; }
    
//    m_is_avc1 = true;

    printf("%s: this video file is [H264 - MPEG-4 AVC (part 10) (avc1)]\n", __FUNCTION__);

    unsigned char AVCProfileIndication = *p; p++;
    unsigned char profile_compatibility = *p; p++;
    unsigned char AVCLevelIndication = *p; p++;
    
    unsigned char reserved1 = ((*p) & 0xFC) >> 2; // 0xFC = 0b 1111 1100，保留字节位
    if (reserved1 != 0x3F){ sps_and_pps_length = 0; return -1; }

    unsigned char lengthSizeMinusOne = ((*p) & 0x03) + 1; p++; //NALU长度，一般等于4
    
    unsigned char reserved2 = ((*p) & 0xE0) >> 5; // 0xE0 = 0b 1110 0000，保留字节位
    if (reserved2 != 0x07){ sps_and_pps_length = 0; return -1; }

    unsigned char numOfSequenceParameterSets = (*p) & 0x1F; p++; //sps个数，一般等于1
    printf("%s: num of sps: %d\n", __FUNCTION__, numOfSequenceParameterSets);
    
    if (numOfSequenceParameterSets <= 0){ return -1; }

    for(int i = 0; i < numOfSequenceParameterSets; i++)
    {
        int sequenceParameterSetLength = (*p) << 8 | (*(p+1)); p += 2; //当前sps长度

        if(((*p) & 0x1F) != 0x07) //0x67
        {
            printf("%s: h264 nalu sps header must be 00 00 00 01 67\n", __FUNCTION__);
            continue;
        }

        sps_length = sequenceParameterSetLength;
        sps = new unsigned char[sps_length + 4];
        sps[0] = 0x00;
        sps[1] = 0x00;
        sps[2] = 0x00;
        sps[3] = 0x01;
        memcpy(sps + 4, p, sps_length);
        p += sps_length;

        break; //暂时只读取一个sps
    }

    if (NULL == sps) {
        printf("%s:sps == NULL\n",__FUNCTION__);
        return -1;
    }

    unsigned char numOfPictureParameterSets = *p; p++; //pps个数，一般等于1
    printf("%s: num of pps: %d\n", __FUNCTION__, numOfPictureParameterSets);
    
    if (numOfPictureParameterSets <= 0){ return -1; }

    for(int i = 0; i < numOfPictureParameterSets; i++)
    {
        int pictureParameterSetLength = (*p) << 8 | (*(p+1)); p += 2; //当前pps长度
        
        if(((*p) & 0x1F) != 0x08) //0x68
        {
            printf("%s: h264 nalu pps header must be 00 00 00 01 68\n", __FUNCTION__);
            continue;
        }

        pps_length = pictureParameterSetLength;
        pps = new unsigned char[pps_length + 4];
        pps[0] = 0x00;
        pps[1] = 0x00;
        pps[2] = 0x00;
        pps[3] = 0x01;
        memcpy(pps + 4, p, pps_length);
        p += pps_length;

        break; //暂时只读取一个pps
    }

    if (NULL == pps) {
        printf("%s:pps == NULL\n",__FUNCTION__);
        if(sps != NULL){delete [] sps; sps = NULL;}
        return -1;
    }

    int read_size = p - video_dec_ctx->m_extradata;
    if (read_size > video_dec_ctx->m_extradata_size){ return -1; }

    //---------------------------------------
    if(sps_and_pps != NULL){delete [] sps_and_pps; sps_and_pps = NULL;}
    sps_and_pps_length = sps_length + pps_length + 8;

    sps_and_pps = new unsigned char[sps_and_pps_length];

    memcpy(sps_and_pps, sps, sps_length + 4);
    memcpy(sps_and_pps + sps_length + 4, pps, pps_length + 4);

    if(sps != NULL){delete [] sps; sps = NULL;}
    if(pps != NULL){delete [] pps; pps = NULL;}

    return 0;
}

int Cmp4Parse::mov_read_glbl(Cmp4Parse *c, CIOContext *pb, MOVAtom atom)
{
    CStream *st;
    int ret;

    /**
    * @ 在找到trak时已经建立了CStream
    */
    if (c->m_nb_streams < 1)
        return 0;

    st = c->m_streams[c->m_nb_streams-1];

    if ((uint64_t)atom.size > (1<<30))
        return AVERROR_INVALIDDATA;

    if (atom.size >= 10) {
        // Broken files created by legacy versions of libavformat will
        // wrap a whole fiel atom inside of a glbl atom.
        unsigned size = pb->avio_rb32();
        unsigned type = pb->avio_rl32();
        pb->avio_seek(-8, SEEK_CUR);
        if (type == MKTAG('f','i','e','l') && size == atom.size)
            return mov_read_default(c, pb, atom);
        //*(((char*)&type)+0),*(((char*)&type)+1),*(((char*)&type)+2),*(((char*)&type)+3)
      // printf("  size:%d\n",size );
    } 

    if (st->m_codec->m_extradata_size > 1 && st->m_codec->m_extradata) {
        printf( "ignoring multiple glbl\n");
        return 0;
    }

    if (st->m_codec->m_extradata) {
        free(st->m_codec->m_extradata);    
        st->m_codec->m_extradata = NULL;
    }
    ret = ff_get_extradata(st->m_codec, pb, atom.size);
    if (ret < 0)
        return ret;


    return 0;
}

int Cmp4Parse::mov_read_stco(Cmp4Parse *c, CIOContext *pb, MOVAtom atom)
{
    CStream *st;
    MOVStreamContext *sc;
    unsigned int i, entries;

    /**
    * @ 在找到trak时已经建立了CStream
    */
     if (c->m_nb_streams < 1)
        return 0;
    st = c->m_streams[c->m_nb_streams-1];
    sc = (MOVStreamContext*)st->m_priv_data;

    pb->avio_r8(); /* version */
    pb->avio_rb24(); /* flags */

    entries = pb->avio_rb32();

    if (!entries)
        return 0;
 
    if (sc->chunk_offsets)
        printf("Duplicated STCO atom\n");
    sc->chunk_count = 0;
    sc->chunk_offsets = (int64_t*)malloc(entries * sizeof(*sc->chunk_offsets));
    if (!sc->chunk_offsets)
        return AVERROR(ENOMEM);
    sc->chunk_count = entries;

    if      (atom.type == MKTAG('s','t','c','o'))
        for (i = 0; i < entries && !pb->m_eof_reached; i++)
            sc->chunk_offsets[i] = pb->avio_rb32();
    else if (atom.type == MKTAG('c','o','6','4'))
        for (i = 0; i < entries && !pb->m_eof_reached; i++)
            sc->chunk_offsets[i] =  pb->avio_rb64();
    else
        return AVERROR_INVALIDDATA;

    sc->chunk_count = i;

    if (pb->m_eof_reached) {
        printf("reached eof, corrupted STCO atom\n");
        return AVERROR_EOF;
    }

    //printf("entries:%d\n",i );
    return 0;
}

int Cmp4Parse::mov_read_stsc(Cmp4Parse *c, CIOContext *pb, MOVAtom atom)
{
    CStream *st;
    MOVStreamContext *sc;
    unsigned int i, entries;

    /**
    * @ 在找到trak时已经建立了CStream
    */
     if (c->m_nb_streams < 1)
        return 0;
    st = c->m_streams[c->m_nb_streams-1];
    sc = (MOVStreamContext*)st->m_priv_data;

    pb->avio_r8(); /* version */
    pb->avio_rb24(); /* flags */

    entries = pb->avio_rb32();
   // printf(" entries ================================> %d size:%d\n",entries,atom.size);
    if ((uint64_t)entries * 12 + 4 > atom.size)
        return AVERROR_INVALIDDATA;


   // printf("track[%u].stsc.entries = %u\n", c->m_nb_streams - 1, entries);

    if (!entries)
        return 0;

    if (sc->stsc_data)
        printf("Duplicated STSC atom\n");

    free(sc->stsc_data);
    sc->stsc_count = 0;
    sc->stsc_data = (MOVStsc*)malloc(entries * sizeof(*sc->stsc_data));
    if (!sc->stsc_data)
        return AVERROR(ENOMEM);

    for (i = 0; i < entries && !pb->m_eof_reached; i++) {
        sc->stsc_data[i].first = pb->avio_rb32();
        sc->stsc_data[i].count = pb->avio_rb32();
        sc->stsc_data[i].id = pb->avio_rb32();
    }

    sc->stsc_count = i;
    /**
    * @这里做了一个检测
    */
     for (i = sc->stsc_count - 1; i < UINT_MAX; i--) {
        int64_t first_min = i + 1;
        if ((i+1 < sc->stsc_count && sc->stsc_data[i].first >= sc->stsc_data[i+1].first) ||
            (i > 0 && sc->stsc_data[i].first <= sc->stsc_data[i-1].first) ||
            sc->stsc_data[i].first < first_min ||
            sc->stsc_data[i].count < 1 ||
            sc->stsc_data[i].id < 1) {
                printf("STSC entry %d is invalid (first=%d count=%d id=%d)\n", i, sc->stsc_data[i].first, sc->stsc_data[i].count, sc->stsc_data[i].id);
            
                if (i+1 >= sc->stsc_count) {
                    sc->stsc_data[i].first = FFMAX(sc->stsc_data[i].first, first_min);
                    if (i > 0 && sc->stsc_data[i].first <= sc->stsc_data[i-1].first)
                        sc->stsc_data[i].first = FFMIN(sc->stsc_data[i-1].first + 1LL, INT_MAX);
                    sc->stsc_data[i].count = FFMAX(sc->stsc_data[i].count, 1);
                    sc->stsc_data[i].id    = FFMAX(sc->stsc_data[i].id, 1);
                    continue;
                }
                av_assert0(sc->stsc_data[i+1].first >= 2);
                 // We replace this entry by the next valid
                sc->stsc_data[i].first = sc->stsc_data[i+1].first - 1;
                sc->stsc_data[i].count = sc->stsc_data[i+1].count;
                sc->stsc_data[i].id    = sc->stsc_data[i+1].id;
            }
     }

       if (pb->m_eof_reached) {
        printf("reached eof, corrupted STSC atom\n");
        return AVERROR_EOF;
    }

    return 0;
}


int Cmp4Parse::mov_metadata_int8_no_padding(Cmp4Parse *c, CIOContext *pb,
                                        unsigned len, const char *key)
{
    c->m_event_flags |= AVFMT_EVENT_FLAG_METADATA_UPDATED;
   // av_dict_set_int(&c->m_metadata, key, pb->avio_r8(), 0);
    return 0;   
}

int Cmp4Parse::mov_metadata_track_or_disc_number(Cmp4Parse *c, CIOContext *pb,
                                             unsigned len, const char *key)
{
    printf("mov_metadata_track_or_disc_number\n");
    return -1;
}

int Cmp4Parse::mov_metadata_gnre(Cmp4Parse *c, CIOContext *pb,
                             unsigned len, const char *key)
{
    printf("mov_metadata_gnre\n");
    return -1;
}

int Cmp4Parse::mov_metadata_int8_bypass_padding(Cmp4Parse *c, CIOContext *pb,
                             unsigned len, const char *key)
{
    printf("mov_metadata_int8_bypass_padding\n");
    return -1;
}

int Cmp4Parse::mov_metadata_hmmt(Cmp4Parse *c, CIOContext *pb, unsigned len)
{
    printf("mov_metadata_hmmt\n");
    return -1;
}

int Cmp4Parse::mov_metadata_loci(Cmp4Parse *c, CIOContext *pb, unsigned len)
{
    printf("mov_metadata_loci\n");
    return -1;
}
#endif