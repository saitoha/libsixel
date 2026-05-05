/* ANSI-C code produced by gperf version 3.0.3 */
/* Command-line: /Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/gperf -C -N sixel_factory_classid_lookup -m5 -t src/classid-factory.gperf  */
/* Computed positions: -k'1,8,11,14,20' */

#if !((' ' == 32) && ('!' == 33) && ('"' == 34) && ('#' == 35) \
      && ('%' == 37) && ('&' == 38) && ('\'' == 39) && ('(' == 40) \
      && (')' == 41) && ('*' == 42) && ('+' == 43) && (',' == 44) \
      && ('-' == 45) && ('.' == 46) && ('/' == 47) && ('0' == 48) \
      && ('1' == 49) && ('2' == 50) && ('3' == 51) && ('4' == 52) \
      && ('5' == 53) && ('6' == 54) && ('7' == 55) && ('8' == 56) \
      && ('9' == 57) && (':' == 58) && (';' == 59) && ('<' == 60) \
      && ('=' == 61) && ('>' == 62) && ('?' == 63) && ('A' == 65) \
      && ('B' == 66) && ('C' == 67) && ('D' == 68) && ('E' == 69) \
      && ('F' == 70) && ('G' == 71) && ('H' == 72) && ('I' == 73) \
      && ('J' == 74) && ('K' == 75) && ('L' == 76) && ('M' == 77) \
      && ('N' == 78) && ('O' == 79) && ('P' == 80) && ('Q' == 81) \
      && ('R' == 82) && ('S' == 83) && ('T' == 84) && ('U' == 85) \
      && ('V' == 86) && ('W' == 87) && ('X' == 88) && ('Y' == 89) \
      && ('Z' == 90) && ('[' == 91) && ('\\' == 92) && (']' == 93) \
      && ('^' == 94) && ('_' == 95) && ('a' == 97) && ('b' == 98) \
      && ('c' == 99) && ('d' == 100) && ('e' == 101) && ('f' == 102) \
      && ('g' == 103) && ('h' == 104) && ('i' == 105) && ('j' == 106) \
      && ('k' == 107) && ('l' == 108) && ('m' == 109) && ('n' == 110) \
      && ('o' == 111) && ('p' == 112) && ('q' == 113) && ('r' == 114) \
      && ('s' == 115) && ('t' == 116) && ('u' == 117) && ('v' == 118) \
      && ('w' == 119) && ('x' == 120) && ('y' == 121) && ('z' == 122) \
      && ('{' == 123) && ('|' == 124) && ('}' == 125) && ('~' == 126))
/* The character set is not based on ISO-646.  */
#error "gperf generated tables don't work with this execution character set. Please report a bug to <bug-gnu-gperf@gnu.org>."
#endif

#line 6 "src/classid-factory.gperf"

#if defined(HAVE_STRING_H) && HAVE_STRING_H
#include <string.h>
#endif
#include "lookup-policy-5bit.h"
#include "lookup-policy-6bit.h"
#include "lookup-policy-certlut.h"
#include "lookup-policy-eytzinger.h"
#include "lookup-policy-fhedt.h"
#include "lookup-policy-mahalanobis.h"
#include "lookup-policy-mono-darkbg.h"
#include "lookup-policy-mono-lightbg.h"
#include "lookup-policy-none.h"
#include "lookup-policy-rbc.h"
#include "lookup-policy-vptree.h"
#include "dither-policy-none.h"
#include "dither-policy-fs.h"
#include "dither-policy-atkinson.h"
#include "dither-policy-jajuni.h"
#include "dither-policy-stucki.h"
#include "dither-policy-burkes.h"
#include "dither-policy-sierra1.h"
#include "dither-policy-sierra2.h"
#include "dither-policy-sierra3.h"
#include "dither-policy-lso2.h"
#include "dither-policy-a-dither.h"
#include "dither-policy-x-dither.h"
#include "dither-policy-bluenoise.h"
#include "dither-policy-interframe.h"
#include "loader-builtin.h"
#include "loader-coregraphics.h"
#include "loader-gd.h"
#include "loader-gdk-pixbuf2.h"
#include "loader-gnome-thumbnailer.h"
#include "loader-libjpeg.h"
#include "loader-libpng.h"
#include "loader-librsvg.h"
#include "loader-libtiff.h"
#include "loader-libwebp.h"
#include "loader-quicklook.h"
#include "loader-wic.h"
#include "sixel-writer.h"
#include "encoder-core.h"
#include "chunk.h"
#include "frame.h"
#include "palette.h"
#include "loader-manager.h"
#include "timeline-logger.h"

typedef SIXELSTATUS (*sixel_factory_class_new_fn)(
    sixel_allocator_t *allocator,
    void **object);

#if 1
# define SIXEL_FACTORY_CLASSID_CREATE_1 sixel_lookup_policy_5bit_8bit_new
#else
# define SIXEL_FACTORY_CLASSID_CREATE_1 0
#endif
#if 1
# define SIXEL_FACTORY_CLASSID_CREATE_2 sixel_lookup_policy_5bit_float32_new
#else
# define SIXEL_FACTORY_CLASSID_CREATE_2 0
#endif
#if 1
# define SIXEL_FACTORY_CLASSID_CREATE_3 sixel_lookup_policy_6bit_8bit_new
#else
# define SIXEL_FACTORY_CLASSID_CREATE_3 0
#endif
#if 1
# define SIXEL_FACTORY_CLASSID_CREATE_4 sixel_lookup_policy_6bit_float32_new
#else
# define SIXEL_FACTORY_CLASSID_CREATE_4 0
#endif
#if 1
# define SIXEL_FACTORY_CLASSID_CREATE_5 sixel_lookup_policy_certlut_8bit_new
#else
# define SIXEL_FACTORY_CLASSID_CREATE_5 0
#endif
#if 1
# define SIXEL_FACTORY_CLASSID_CREATE_6 sixel_lookup_policy_certlut_float32_new
#else
# define SIXEL_FACTORY_CLASSID_CREATE_6 0
#endif
#if 1
# define SIXEL_FACTORY_CLASSID_CREATE_7 sixel_lookup_policy_eytzinger_8bit_new
#else
# define SIXEL_FACTORY_CLASSID_CREATE_7 0
#endif
#if 1
# define SIXEL_FACTORY_CLASSID_CREATE_8 sixel_lookup_policy_eytzinger_float32_new
#else
# define SIXEL_FACTORY_CLASSID_CREATE_8 0
#endif
#if 1
# define SIXEL_FACTORY_CLASSID_CREATE_9 sixel_lookup_policy_fhedt_8bit_new
#else
# define SIXEL_FACTORY_CLASSID_CREATE_9 0
#endif
#if 1
# define SIXEL_FACTORY_CLASSID_CREATE_10 sixel_lookup_policy_fhedt_float32_new
#else
# define SIXEL_FACTORY_CLASSID_CREATE_10 0
#endif
#if 1
# define SIXEL_FACTORY_CLASSID_CREATE_11 sixel_lookup_policy_mahalanobis_8bit_new
#else
# define SIXEL_FACTORY_CLASSID_CREATE_11 0
#endif
#if 1
# define SIXEL_FACTORY_CLASSID_CREATE_12 sixel_lookup_policy_mahalanobis_float32_new
#else
# define SIXEL_FACTORY_CLASSID_CREATE_12 0
#endif
#if 1
# define SIXEL_FACTORY_CLASSID_CREATE_13 sixel_lookup_policy_mono_darkbg_8bit_new
#else
# define SIXEL_FACTORY_CLASSID_CREATE_13 0
#endif
#if 1
# define SIXEL_FACTORY_CLASSID_CREATE_14 sixel_lookup_policy_mono_darkbg_float32_new
#else
# define SIXEL_FACTORY_CLASSID_CREATE_14 0
#endif
#if 1
# define SIXEL_FACTORY_CLASSID_CREATE_15 sixel_lookup_policy_mono_lightbg_8bit_new
#else
# define SIXEL_FACTORY_CLASSID_CREATE_15 0
#endif
#if 1
# define SIXEL_FACTORY_CLASSID_CREATE_16 sixel_lookup_policy_mono_lightbg_float32_new
#else
# define SIXEL_FACTORY_CLASSID_CREATE_16 0
#endif
#if 1
# define SIXEL_FACTORY_CLASSID_CREATE_17 sixel_lookup_policy_none_8bit_new
#else
# define SIXEL_FACTORY_CLASSID_CREATE_17 0
#endif
#if 1
# define SIXEL_FACTORY_CLASSID_CREATE_18 sixel_lookup_policy_none_float32_new
#else
# define SIXEL_FACTORY_CLASSID_CREATE_18 0
#endif
#if 1
# define SIXEL_FACTORY_CLASSID_CREATE_19 sixel_lookup_policy_rbc_8bit_new
#else
# define SIXEL_FACTORY_CLASSID_CREATE_19 0
#endif
#if 1
# define SIXEL_FACTORY_CLASSID_CREATE_20 sixel_lookup_policy_rbc_float32_new
#else
# define SIXEL_FACTORY_CLASSID_CREATE_20 0
#endif
#if 1
# define SIXEL_FACTORY_CLASSID_CREATE_21 sixel_lookup_policy_vptree_8bit_new
#else
# define SIXEL_FACTORY_CLASSID_CREATE_21 0
#endif
#if 1
# define SIXEL_FACTORY_CLASSID_CREATE_22 sixel_lookup_policy_vptree_float32_new
#else
# define SIXEL_FACTORY_CLASSID_CREATE_22 0
#endif
#if 1
# define SIXEL_FACTORY_CLASSID_CREATE_23 sixel_dither_policy_none_8bit_new
#else
# define SIXEL_FACTORY_CLASSID_CREATE_23 0
#endif
#if 1
# define SIXEL_FACTORY_CLASSID_CREATE_24 sixel_dither_policy_none_float32_new
#else
# define SIXEL_FACTORY_CLASSID_CREATE_24 0
#endif
#if 1
# define SIXEL_FACTORY_CLASSID_CREATE_25 sixel_dither_policy_fs_8bit_new
#else
# define SIXEL_FACTORY_CLASSID_CREATE_25 0
#endif
#if 1
# define SIXEL_FACTORY_CLASSID_CREATE_26 sixel_dither_policy_fs_float32_new
#else
# define SIXEL_FACTORY_CLASSID_CREATE_26 0
#endif
#if 1
# define SIXEL_FACTORY_CLASSID_CREATE_27 sixel_dither_policy_atkinson_8bit_new
#else
# define SIXEL_FACTORY_CLASSID_CREATE_27 0
#endif
#if 1
# define SIXEL_FACTORY_CLASSID_CREATE_28 sixel_dither_policy_atkinson_float32_new
#else
# define SIXEL_FACTORY_CLASSID_CREATE_28 0
#endif
#if 1
# define SIXEL_FACTORY_CLASSID_CREATE_29 sixel_dither_policy_jajuni_8bit_new
#else
# define SIXEL_FACTORY_CLASSID_CREATE_29 0
#endif
#if 1
# define SIXEL_FACTORY_CLASSID_CREATE_30 sixel_dither_policy_jajuni_float32_new
#else
# define SIXEL_FACTORY_CLASSID_CREATE_30 0
#endif
#if 1
# define SIXEL_FACTORY_CLASSID_CREATE_31 sixel_dither_policy_stucki_8bit_new
#else
# define SIXEL_FACTORY_CLASSID_CREATE_31 0
#endif
#if 1
# define SIXEL_FACTORY_CLASSID_CREATE_32 sixel_dither_policy_stucki_float32_new
#else
# define SIXEL_FACTORY_CLASSID_CREATE_32 0
#endif
#if 1
# define SIXEL_FACTORY_CLASSID_CREATE_33 sixel_dither_policy_burkes_8bit_new
#else
# define SIXEL_FACTORY_CLASSID_CREATE_33 0
#endif
#if 1
# define SIXEL_FACTORY_CLASSID_CREATE_34 sixel_dither_policy_burkes_float32_new
#else
# define SIXEL_FACTORY_CLASSID_CREATE_34 0
#endif
#if 1
# define SIXEL_FACTORY_CLASSID_CREATE_35 sixel_dither_policy_sierra1_8bit_new
#else
# define SIXEL_FACTORY_CLASSID_CREATE_35 0
#endif
#if 1
# define SIXEL_FACTORY_CLASSID_CREATE_36 sixel_dither_policy_sierra1_float32_new
#else
# define SIXEL_FACTORY_CLASSID_CREATE_36 0
#endif
#if 1
# define SIXEL_FACTORY_CLASSID_CREATE_37 sixel_dither_policy_sierra2_8bit_new
#else
# define SIXEL_FACTORY_CLASSID_CREATE_37 0
#endif
#if 1
# define SIXEL_FACTORY_CLASSID_CREATE_38 sixel_dither_policy_sierra2_float32_new
#else
# define SIXEL_FACTORY_CLASSID_CREATE_38 0
#endif
#if 1
# define SIXEL_FACTORY_CLASSID_CREATE_39 sixel_dither_policy_sierra3_8bit_new
#else
# define SIXEL_FACTORY_CLASSID_CREATE_39 0
#endif
#if 1
# define SIXEL_FACTORY_CLASSID_CREATE_40 sixel_dither_policy_sierra3_float32_new
#else
# define SIXEL_FACTORY_CLASSID_CREATE_40 0
#endif
#if 1
# define SIXEL_FACTORY_CLASSID_CREATE_41 sixel_dither_policy_lso2_8bit_new
#else
# define SIXEL_FACTORY_CLASSID_CREATE_41 0
#endif
#if 1
# define SIXEL_FACTORY_CLASSID_CREATE_42 sixel_dither_policy_lso2_float32_new
#else
# define SIXEL_FACTORY_CLASSID_CREATE_42 0
#endif
#if 1
# define SIXEL_FACTORY_CLASSID_CREATE_43 sixel_dither_policy_a_dither_8bit_new
#else
# define SIXEL_FACTORY_CLASSID_CREATE_43 0
#endif
#if 1
# define SIXEL_FACTORY_CLASSID_CREATE_44 sixel_dither_policy_a_dither_float32_new
#else
# define SIXEL_FACTORY_CLASSID_CREATE_44 0
#endif
#if 1
# define SIXEL_FACTORY_CLASSID_CREATE_45 sixel_dither_policy_x_dither_8bit_new
#else
# define SIXEL_FACTORY_CLASSID_CREATE_45 0
#endif
#if 1
# define SIXEL_FACTORY_CLASSID_CREATE_46 sixel_dither_policy_x_dither_float32_new
#else
# define SIXEL_FACTORY_CLASSID_CREATE_46 0
#endif
#if 1
# define SIXEL_FACTORY_CLASSID_CREATE_47 sixel_dither_policy_bluenoise_8bit_new
#else
# define SIXEL_FACTORY_CLASSID_CREATE_47 0
#endif
#if 1
# define SIXEL_FACTORY_CLASSID_CREATE_48 sixel_dither_policy_bluenoise_float32_new
#else
# define SIXEL_FACTORY_CLASSID_CREATE_48 0
#endif
#if 1
# define SIXEL_FACTORY_CLASSID_CREATE_49 sixel_dither_policy_interframe_8bit_new
#else
# define SIXEL_FACTORY_CLASSID_CREATE_49 0
#endif
#if 1
# define SIXEL_FACTORY_CLASSID_CREATE_50 sixel_dither_policy_interframe_float32_new
#else
# define SIXEL_FACTORY_CLASSID_CREATE_50 0
#endif
#if 1
# define SIXEL_FACTORY_CLASSID_CREATE_51 sixel_loader_builtin_new
#else
# define SIXEL_FACTORY_CLASSID_CREATE_51 0
#endif
#if HAVE_COREGRAPHICS
# define SIXEL_FACTORY_CLASSID_CREATE_52 sixel_loader_coregraphics_new
#else
# define SIXEL_FACTORY_CLASSID_CREATE_52 0
#endif
#if HAVE_GD
# define SIXEL_FACTORY_CLASSID_CREATE_53 sixel_loader_gd_new
#else
# define SIXEL_FACTORY_CLASSID_CREATE_53 0
#endif
#if HAVE_GDK_PIXBUF2
# define SIXEL_FACTORY_CLASSID_CREATE_54 sixel_loader_gdkpixbuf2_new
#else
# define SIXEL_FACTORY_CLASSID_CREATE_54 0
#endif
#if HAVE_FREEDESKTOP_THUMBNAILING
# define SIXEL_FACTORY_CLASSID_CREATE_55 sixel_loader_gnome_thumbnailer_new
#else
# define SIXEL_FACTORY_CLASSID_CREATE_55 0
#endif
#if HAVE_JPEG
# define SIXEL_FACTORY_CLASSID_CREATE_56 sixel_loader_libjpeg_new
#else
# define SIXEL_FACTORY_CLASSID_CREATE_56 0
#endif
#if HAVE_LIBPNG
# define SIXEL_FACTORY_CLASSID_CREATE_57 sixel_loader_libpng_new
#else
# define SIXEL_FACTORY_CLASSID_CREATE_57 0
#endif
#if HAVE_LIBRSVG
# define SIXEL_FACTORY_CLASSID_CREATE_58 sixel_loader_librsvg_new
#else
# define SIXEL_FACTORY_CLASSID_CREATE_58 0
#endif
#if HAVE_LIBTIFF
# define SIXEL_FACTORY_CLASSID_CREATE_59 sixel_loader_libtiff_new
#else
# define SIXEL_FACTORY_CLASSID_CREATE_59 0
#endif
#if HAVE_WEBP
# define SIXEL_FACTORY_CLASSID_CREATE_60 sixel_loader_libwebp_new
#else
# define SIXEL_FACTORY_CLASSID_CREATE_60 0
#endif
#if HAVE_COREGRAPHICS && HAVE_QUICKLOOK
# define SIXEL_FACTORY_CLASSID_CREATE_61 sixel_loader_quicklook_new
#else
# define SIXEL_FACTORY_CLASSID_CREATE_61 0
#endif
#if HAVE_WIC
# define SIXEL_FACTORY_CLASSID_CREATE_62 sixel_loader_wic_new
#else
# define SIXEL_FACTORY_CLASSID_CREATE_62 0
#endif
#if 1
# define SIXEL_FACTORY_CLASSID_CREATE_63 sixel_writer_factory_new
#else
# define SIXEL_FACTORY_CLASSID_CREATE_63 0
#endif
#if 1
# define SIXEL_FACTORY_CLASSID_CREATE_64 sixel_encoder_core_factory_new
#else
# define SIXEL_FACTORY_CLASSID_CREATE_64 0
#endif
#if 1
# define SIXEL_FACTORY_CLASSID_CREATE_65 sixel_encoder_core_normal_factory_new
#else
# define SIXEL_FACTORY_CLASSID_CREATE_65 0
#endif
#if 1
# define SIXEL_FACTORY_CLASSID_CREATE_66 sixel_encoder_core_highcolor_factory_new
#else
# define SIXEL_FACTORY_CLASSID_CREATE_66 0
#endif
#if 1
# define SIXEL_FACTORY_CLASSID_CREATE_67 sixel_encoder_core_ormode_factory_new
#else
# define SIXEL_FACTORY_CLASSID_CREATE_67 0
#endif
#if 1
# define SIXEL_FACTORY_CLASSID_CREATE_68 sixel_chunk_factory_new
#else
# define SIXEL_FACTORY_CLASSID_CREATE_68 0
#endif
#if 1
# define SIXEL_FACTORY_CLASSID_CREATE_69 sixel_frame_factory_new
#else
# define SIXEL_FACTORY_CLASSID_CREATE_69 0
#endif
#if 1
# define SIXEL_FACTORY_CLASSID_CREATE_70 sixel_palette_factory_new
#else
# define SIXEL_FACTORY_CLASSID_CREATE_70 0
#endif
#if 1
# define SIXEL_FACTORY_CLASSID_CREATE_71 sixel_loader_manager_new
#else
# define SIXEL_FACTORY_CLASSID_CREATE_71 0
#endif
#if 1
# define SIXEL_FACTORY_CLASSID_CREATE_72 sixel_timeline_logger_factory_new
#else
# define SIXEL_FACTORY_CLASSID_CREATE_72 0
#endif
#line 420 "src/classid-factory.gperf"
struct sixel_factory_classid_entry {
    char const *name;
    sixel_factory_class_new_fn create;
};

#define TOTAL_KEYWORDS 72
#define MIN_WORD_LENGTH 9
#define MAX_WORD_LENGTH 28
#define MIN_HASH_VALUE 16
#define MAX_HASH_VALUE 101
/* maximum key range = 86, duplicates = 0 */

#ifdef __GNUC__
__inline
#else
#ifdef __cplusplus
inline
#endif
#endif
static unsigned int
hash (register const char *str, register unsigned int len)
{
  static const unsigned char asso_values[] =
    {
      102, 102, 102, 102, 102, 102, 102, 102, 102, 102,
      102, 102, 102, 102, 102, 102, 102, 102, 102, 102,
      102, 102, 102, 102, 102, 102, 102, 102, 102, 102,
      102, 102, 102, 102, 102, 102, 102, 102, 102, 102,
      102, 102, 102, 102, 102,  35,   6, 102, 102,  44,
       29,  20, 102,  59,  58, 102,  27, 102, 102, 102,
      102, 102, 102, 102, 102, 102, 102, 102, 102, 102,
      102, 102, 102, 102, 102, 102, 102, 102, 102, 102,
      102, 102, 102, 102, 102, 102, 102, 102, 102, 102,
      102, 102, 102, 102, 102, 102, 102,   5,   3,   6,
        4,   3,   9,  11,  13,   3,   8,  26,   3,   8,
        3,   5,  58,  58,   3,  13,  11,  31,  33,   3,
       34, 102,  38, 102, 102, 102, 102, 102, 102, 102,
      102, 102, 102, 102, 102, 102, 102, 102, 102, 102,
      102, 102, 102, 102, 102, 102, 102, 102, 102, 102,
      102, 102, 102, 102, 102, 102, 102, 102, 102, 102,
      102, 102, 102, 102, 102, 102, 102, 102, 102, 102,
      102, 102, 102, 102, 102, 102, 102, 102, 102, 102,
      102, 102, 102, 102, 102, 102, 102, 102, 102, 102,
      102, 102, 102, 102, 102, 102, 102, 102, 102, 102,
      102, 102, 102, 102, 102, 102, 102, 102, 102, 102,
      102, 102, 102, 102, 102, 102, 102, 102, 102, 102,
      102, 102, 102, 102, 102, 102, 102, 102, 102, 102,
      102, 102, 102, 102, 102, 102, 102, 102, 102, 102,
      102, 102, 102, 102, 102, 102, 102, 102, 102, 102,
      102, 102, 102, 102, 102, 102
    };
  register unsigned int hval = len;

  switch (hval)
    {
      default:
        hval += asso_values[(unsigned char)str[19]];
      /*FALLTHROUGH*/
      case 19:
      case 18:
      case 17:
      case 16:
      case 15:
      case 14:
        hval += asso_values[(unsigned char)str[13]];
      /*FALLTHROUGH*/
      case 13:
      case 12:
      case 11:
        hval += asso_values[(unsigned char)str[10]];
      /*FALLTHROUGH*/
      case 10:
      case 9:
      case 8:
        hval += asso_values[(unsigned char)str[7]];
      /*FALLTHROUGH*/
      case 7:
      case 6:
      case 5:
      case 4:
      case 3:
      case 2:
      case 1:
        hval += asso_values[(unsigned char)str[0]];
        break;
    }
  return hval;
}

static const struct sixel_factory_classid_entry wordlist[] =
  {
    {"", 0}, {"", 0}, {"", 0}, {"", 0}, {"", 0}, {"", 0}, {"", 0}, {"", 0}, {"", 0},
    {"", 0}, {"", 0}, {"", 0}, {"", 0}, {"", 0}, {"", 0}, {"", 0},
#line 486 "src/classid-factory.gperf"
    {"loader/wic", SIXEL_FACTORY_CLASSID_CREATE_62},
    {"", 0}, {"", 0}, {"", 0},
#line 493 "src/classid-factory.gperf"
    {"image/frame", SIXEL_FACTORY_CLASSID_CREATE_69},
    {"", 0}, {"", 0},
#line 477 "src/classid-factory.gperf"
    {"loader/gd", SIXEL_FACTORY_CLASSID_CREATE_53},
    {"", 0}, {"", 0},
#line 475 "src/classid-factory.gperf"
    {"loader/builtin", SIXEL_FACTORY_CLASSID_CREATE_51},
#line 487 "src/classid-factory.gperf"
    {"io/sixel-writer", SIXEL_FACTORY_CLASSID_CREATE_63},
#line 441 "src/classid-factory.gperf"
    {"lookup/none.8bit", SIXEL_FACTORY_CLASSID_CREATE_17},
#line 447 "src/classid-factory.gperf"
    {"dither/none.8bit", SIXEL_FACTORY_CLASSID_CREATE_23},
#line 443 "src/classid-factory.gperf"
    {"lookup/rbc.8bit", SIXEL_FACTORY_CLASSID_CREATE_19},
#line 442 "src/classid-factory.gperf"
    {"lookup/none.float32", SIXEL_FACTORY_CLASSID_CREATE_18},
#line 448 "src/classid-factory.gperf"
    {"dither/none.float32", SIXEL_FACTORY_CLASSID_CREATE_24},
#line 495 "src/classid-factory.gperf"
    {"loader/manager", SIXEL_FACTORY_CLASSID_CREATE_71},
#line 482 "src/classid-factory.gperf"
    {"loader/librsvg", SIXEL_FACTORY_CLASSID_CREATE_58},
#line 444 "src/classid-factory.gperf"
    {"lookup/rbc.float32", SIXEL_FACTORY_CLASSID_CREATE_20},
#line 476 "src/classid-factory.gperf"
    {"loader/coregraphics", SIXEL_FACTORY_CLASSID_CREATE_52},
#line 471 "src/classid-factory.gperf"
    {"dither/bluenoise.8bit", SIXEL_FACTORY_CLASSID_CREATE_47},
#line 473 "src/classid-factory.gperf"
    {"dither/interframe.8bit", SIXEL_FACTORY_CLASSID_CREATE_49},
#line 480 "src/classid-factory.gperf"
    {"loader/libjpeg", SIXEL_FACTORY_CLASSID_CREATE_56},
#line 483 "src/classid-factory.gperf"
    {"loader/libtiff", SIXEL_FACTORY_CLASSID_CREATE_59},
#line 474 "src/classid-factory.gperf"
    {"dither/interframe.float32", SIXEL_FACTORY_CLASSID_CREATE_50},
#line 472 "src/classid-factory.gperf"
    {"dither/bluenoise.float32", SIXEL_FACTORY_CLASSID_CREATE_48},
#line 468 "src/classid-factory.gperf"
    {"dither/a_dither.float32", SIXEL_FACTORY_CLASSID_CREATE_44},
#line 450 "src/classid-factory.gperf"
    {"dither/fs.float32", SIXEL_FACTORY_CLASSID_CREATE_26},
#line 452 "src/classid-factory.gperf"
    {"dither/atkinson.float32", SIXEL_FACTORY_CLASSID_CREATE_28},
#line 467 "src/classid-factory.gperf"
    {"dither/a_dither.8bit", SIXEL_FACTORY_CLASSID_CREATE_43},
#line 455 "src/classid-factory.gperf"
    {"dither/stucki.8bit", SIXEL_FACTORY_CLASSID_CREATE_31},
#line 451 "src/classid-factory.gperf"
    {"dither/atkinson.8bit", SIXEL_FACTORY_CLASSID_CREATE_27},
#line 439 "src/classid-factory.gperf"
    {"lookup/mono-lightbg.8bit", SIXEL_FACTORY_CLASSID_CREATE_15},
#line 429 "src/classid-factory.gperf"
    {"lookup/certlut.8bit", SIXEL_FACTORY_CLASSID_CREATE_5},
    {"", 0},
#line 440 "src/classid-factory.gperf"
    {"lookup/mono-lightbg.float32", SIXEL_FACTORY_CLASSID_CREATE_16},
#line 492 "src/classid-factory.gperf"
    {"image/chunk", SIXEL_FACTORY_CLASSID_CREATE_68},
#line 436 "src/classid-factory.gperf"
    {"lookup/mahalanobis.float32", SIXEL_FACTORY_CLASSID_CREATE_12},
#line 465 "src/classid-factory.gperf"
    {"dither/lso2.8bit", SIXEL_FACTORY_CLASSID_CREATE_41},
#line 438 "src/classid-factory.gperf"
    {"lookup/mono-darkbg.float32", SIXEL_FACTORY_CLASSID_CREATE_14},
#line 457 "src/classid-factory.gperf"
    {"dither/burkes.8bit", SIXEL_FACTORY_CLASSID_CREATE_33},
#line 466 "src/classid-factory.gperf"
    {"dither/lso2.float32", SIXEL_FACTORY_CLASSID_CREATE_42},
#line 463 "src/classid-factory.gperf"
    {"dither/sierra3.8bit", SIXEL_FACTORY_CLASSID_CREATE_39},
#line 433 "src/classid-factory.gperf"
    {"lookup/fhedt.8bit", SIXEL_FACTORY_CLASSID_CREATE_9},
#line 496 "src/classid-factory.gperf"
    {"diagnostics/timeline-logger", SIXEL_FACTORY_CLASSID_CREATE_72},
#line 479 "src/classid-factory.gperf"
    {"loader/gnome-thumbnailer", SIXEL_FACTORY_CLASSID_CREATE_55},
#line 445 "src/classid-factory.gperf"
    {"lookup/vptree.8bit", SIXEL_FACTORY_CLASSID_CREATE_21},
#line 430 "src/classid-factory.gperf"
    {"lookup/certlut.float32", SIXEL_FACTORY_CLASSID_CREATE_6},
#line 449 "src/classid-factory.gperf"
    {"dither/fs.8bit", SIXEL_FACTORY_CLASSID_CREATE_25},
#line 488 "src/classid-factory.gperf"
    {"codec/encoder-core", SIXEL_FACTORY_CLASSID_CREATE_64},
#line 453 "src/classid-factory.gperf"
    {"dither/jajuni.8bit", SIXEL_FACTORY_CLASSID_CREATE_29},
#line 461 "src/classid-factory.gperf"
    {"dither/sierra2.8bit", SIXEL_FACTORY_CLASSID_CREATE_37},
#line 435 "src/classid-factory.gperf"
    {"lookup/mahalanobis.8bit", SIXEL_FACTORY_CLASSID_CREATE_11},
#line 456 "src/classid-factory.gperf"
    {"dither/stucki.float32", SIXEL_FACTORY_CLASSID_CREATE_32},
#line 437 "src/classid-factory.gperf"
    {"lookup/mono-darkbg.8bit", SIXEL_FACTORY_CLASSID_CREATE_13},
#line 470 "src/classid-factory.gperf"
    {"dither/x_dither.float32", SIXEL_FACTORY_CLASSID_CREATE_46},
#line 464 "src/classid-factory.gperf"
    {"dither/sierra3.float32", SIXEL_FACTORY_CLASSID_CREATE_40},
#line 434 "src/classid-factory.gperf"
    {"lookup/fhedt.float32", SIXEL_FACTORY_CLASSID_CREATE_10},
#line 469 "src/classid-factory.gperf"
    {"dither/x_dither.8bit", SIXEL_FACTORY_CLASSID_CREATE_45},
#line 489 "src/classid-factory.gperf"
    {"codec/encoder-core.normal", SIXEL_FACTORY_CLASSID_CREATE_65},
#line 481 "src/classid-factory.gperf"
    {"loader/libpng", SIXEL_FACTORY_CLASSID_CREATE_57},
#line 491 "src/classid-factory.gperf"
    {"codec/encoder-core.ormode", SIXEL_FACTORY_CLASSID_CREATE_67},
#line 431 "src/classid-factory.gperf"
    {"lookup/eytzinger.8bit", SIXEL_FACTORY_CLASSID_CREATE_7},
#line 458 "src/classid-factory.gperf"
    {"dither/burkes.float32", SIXEL_FACTORY_CLASSID_CREATE_34},
#line 484 "src/classid-factory.gperf"
    {"loader/libwebp", SIXEL_FACTORY_CLASSID_CREATE_60},
#line 462 "src/classid-factory.gperf"
    {"dither/sierra2.float32", SIXEL_FACTORY_CLASSID_CREATE_38},
#line 459 "src/classid-factory.gperf"
    {"dither/sierra1.8bit", SIXEL_FACTORY_CLASSID_CREATE_35},
#line 432 "src/classid-factory.gperf"
    {"lookup/eytzinger.float32", SIXEL_FACTORY_CLASSID_CREATE_8},
    {"", 0},
#line 446 "src/classid-factory.gperf"
    {"lookup/vptree.float32", SIXEL_FACTORY_CLASSID_CREATE_22},
#line 494 "src/classid-factory.gperf"
    {"quant/palette", SIXEL_FACTORY_CLASSID_CREATE_70},
#line 485 "src/classid-factory.gperf"
    {"loader/quicklook", SIXEL_FACTORY_CLASSID_CREATE_61},
#line 490 "src/classid-factory.gperf"
    {"codec/encoder-core.highcolor", SIXEL_FACTORY_CLASSID_CREATE_66},
#line 454 "src/classid-factory.gperf"
    {"dither/jajuni.float32", SIXEL_FACTORY_CLASSID_CREATE_30},
#line 427 "src/classid-factory.gperf"
    {"lookup/6bit.8bit", SIXEL_FACTORY_CLASSID_CREATE_3},
#line 425 "src/classid-factory.gperf"
    {"lookup/5bit.8bit", SIXEL_FACTORY_CLASSID_CREATE_1},
    {"", 0},
#line 428 "src/classid-factory.gperf"
    {"lookup/6bit.float32", SIXEL_FACTORY_CLASSID_CREATE_4},
#line 426 "src/classid-factory.gperf"
    {"lookup/5bit.float32", SIXEL_FACTORY_CLASSID_CREATE_2},
    {"", 0},
#line 460 "src/classid-factory.gperf"
    {"dither/sierra1.float32", SIXEL_FACTORY_CLASSID_CREATE_36},
    {"", 0}, {"", 0}, {"", 0},
#line 478 "src/classid-factory.gperf"
    {"loader/gdk-pixbuf2", SIXEL_FACTORY_CLASSID_CREATE_54}
  };

const struct sixel_factory_classid_entry *
sixel_factory_classid_lookup (register const char *str, register unsigned int len)
{
  if (len <= MAX_WORD_LENGTH && len >= MIN_WORD_LENGTH)
    {
      unsigned int key = hash (str, len);

      if (key <= MAX_HASH_VALUE)
        {
          register const char *s = wordlist[key].name;

          if (*str == *s && !strcmp (str + 1, s + 1))
            return &wordlist[key];
        }
    }
  return 0;
}
#line 497 "src/classid-factory.gperf"

#undef TOTAL_KEYWORDS
#undef MIN_WORD_LENGTH
#undef MAX_WORD_LENGTH
#undef MIN_HASH_VALUE
#undef MAX_HASH_VALUE
