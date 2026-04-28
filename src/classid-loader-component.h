/* ANSI-C code produced by gperf version 3.0.3 */
/* Command-line: /Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/gperf -C -N sixel_loader_component_classid_lookup --language=ANSI-C -H sixel_loader_component_classid_hash -W sixel_loader_component_classid_wordlist src/classid-loader.gperf  */
/* Computed positions: -k'11' */

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

#line 6 "src/classid-loader.gperf"

#if defined(HAVE_STRING_H) && HAVE_STRING_H
#include <string.h>
#endif
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

typedef SIXELSTATUS (*sixel_loader_component_new_fn)(
    sixel_allocator_t *allocator,
    sixel_loader_component_t **ppcomponent);

#if 1
# define SIXEL_LOADER_COMPONENT_CLASSID_CREATE_1 sixel_loader_builtin_new
#else
# define SIXEL_LOADER_COMPONENT_CLASSID_CREATE_1 0
#endif
#if HAVE_COREGRAPHICS
# define SIXEL_LOADER_COMPONENT_CLASSID_CREATE_2 sixel_loader_coregraphics_new
#else
# define SIXEL_LOADER_COMPONENT_CLASSID_CREATE_2 0
#endif
#if HAVE_GD
# define SIXEL_LOADER_COMPONENT_CLASSID_CREATE_3 sixel_loader_gd_new
#else
# define SIXEL_LOADER_COMPONENT_CLASSID_CREATE_3 0
#endif
#if HAVE_GDK_PIXBUF2
# define SIXEL_LOADER_COMPONENT_CLASSID_CREATE_4 sixel_loader_gdkpixbuf2_new
#else
# define SIXEL_LOADER_COMPONENT_CLASSID_CREATE_4 0
#endif
#if HAVE_FREEDESKTOP_THUMBNAILING
# define SIXEL_LOADER_COMPONENT_CLASSID_CREATE_5 sixel_loader_gnome_thumbnailer_new
#else
# define SIXEL_LOADER_COMPONENT_CLASSID_CREATE_5 0
#endif
#if HAVE_JPEG
# define SIXEL_LOADER_COMPONENT_CLASSID_CREATE_6 sixel_loader_libjpeg_new
#else
# define SIXEL_LOADER_COMPONENT_CLASSID_CREATE_6 0
#endif
#if HAVE_LIBPNG
# define SIXEL_LOADER_COMPONENT_CLASSID_CREATE_7 sixel_loader_libpng_new
#else
# define SIXEL_LOADER_COMPONENT_CLASSID_CREATE_7 0
#endif
#if HAVE_LIBRSVG
# define SIXEL_LOADER_COMPONENT_CLASSID_CREATE_8 sixel_loader_librsvg_new
#else
# define SIXEL_LOADER_COMPONENT_CLASSID_CREATE_8 0
#endif
#if HAVE_LIBTIFF
# define SIXEL_LOADER_COMPONENT_CLASSID_CREATE_9 sixel_loader_libtiff_new
#else
# define SIXEL_LOADER_COMPONENT_CLASSID_CREATE_9 0
#endif
#if HAVE_WEBP
# define SIXEL_LOADER_COMPONENT_CLASSID_CREATE_10 sixel_loader_libwebp_new
#else
# define SIXEL_LOADER_COMPONENT_CLASSID_CREATE_10 0
#endif
#if HAVE_COREGRAPHICS && HAVE_QUICKLOOK
# define SIXEL_LOADER_COMPONENT_CLASSID_CREATE_11 sixel_loader_quicklook_new
#else
# define SIXEL_LOADER_COMPONENT_CLASSID_CREATE_11 0
#endif
#if HAVE_WIC
# define SIXEL_LOADER_COMPONENT_CLASSID_CREATE_12 sixel_loader_wic_new
#else
# define SIXEL_LOADER_COMPONENT_CLASSID_CREATE_12 0
#endif
#line 88 "src/classid-loader.gperf"
struct sixel_loader_component_classid_entry {
    char const *name;
    sixel_loader_component_new_fn create;
};

#define TOTAL_KEYWORDS 12
#define MIN_WORD_LENGTH 9
#define MAX_WORD_LENGTH 24
#define MIN_HASH_VALUE 9
#define MAX_HASH_VALUE 34
/* maximum key range = 26, duplicates = 0 */

#ifdef __GNUC__
__inline
#else
#ifdef __cplusplus
inline
#endif
#endif
static unsigned int
sixel_loader_component_classid_hash (register const char *str, register unsigned int len)
{
  static const unsigned char asso_values[] =
    {
      35, 35, 35, 35, 35, 35, 35, 35, 35, 35,
      35, 35, 35, 35, 35, 35, 35, 35, 35, 35,
      35, 35, 35, 35, 35, 35, 35, 35, 35, 35,
      35, 35, 35, 35, 35, 35, 35, 35, 35, 35,
      35, 35, 35, 35, 35, 10, 35, 35, 35, 35,
      35, 35, 35, 35, 35, 35, 35, 35, 35, 35,
      35, 35, 35, 35, 35, 35, 35, 35, 35, 35,
      35, 35, 35, 35, 35, 35, 35, 35, 35, 35,
      35, 35, 35, 35, 35, 35, 35, 35, 35, 35,
      35, 35, 35, 35, 35, 35, 35, 35, 35,  0,
      35, 15, 35, 35, 35, 35,  9, 35,  4,  5,
      35, 35,  0, 35, 10, 35,  5, 35, 35,  0,
      35, 35, 35, 35, 35, 35, 35, 35, 35, 35,
      35, 35, 35, 35, 35, 35, 35, 35, 35, 35,
      35, 35, 35, 35, 35, 35, 35, 35, 35, 35,
      35, 35, 35, 35, 35, 35, 35, 35, 35, 35,
      35, 35, 35, 35, 35, 35, 35, 35, 35, 35,
      35, 35, 35, 35, 35, 35, 35, 35, 35, 35,
      35, 35, 35, 35, 35, 35, 35, 35, 35, 35,
      35, 35, 35, 35, 35, 35, 35, 35, 35, 35,
      35, 35, 35, 35, 35, 35, 35, 35, 35, 35,
      35, 35, 35, 35, 35, 35, 35, 35, 35, 35,
      35, 35, 35, 35, 35, 35, 35, 35, 35, 35,
      35, 35, 35, 35, 35, 35, 35, 35, 35, 35,
      35, 35, 35, 35, 35, 35, 35, 35, 35, 35,
      35, 35, 35, 35, 35, 35
    };
  register unsigned int hval = len;

  switch (hval)
    {
      default:
        hval += asso_values[(unsigned char)str[10]];
      /*FALLTHROUGH*/
      case 10:
      case 9:
        break;
    }
  return hval;
}

static const struct sixel_loader_component_classid_entry sixel_loader_component_classid_wordlist[] =
  {
    {"", 0}, {"", 0}, {"", 0}, {"", 0}, {"", 0}, {"", 0}, {"", 0}, {"", 0}, {"", 0},
#line 95 "src/classid-loader.gperf"
    {"loader/gd", SIXEL_LOADER_COMPONENT_CLASSID_CREATE_3},
#line 104 "src/classid-loader.gperf"
    {"loader/wic", SIXEL_LOADER_COMPONENT_CLASSID_CREATE_12},
    {"", 0}, {"", 0},
#line 99 "src/classid-loader.gperf"
    {"loader/libpng", SIXEL_LOADER_COMPONENT_CLASSID_CREATE_7},
#line 102 "src/classid-loader.gperf"
    {"loader/libwebp", SIXEL_LOADER_COMPONENT_CLASSID_CREATE_10},
    {"", 0},
#line 103 "src/classid-loader.gperf"
    {"loader/quicklook", SIXEL_LOADER_COMPONENT_CLASSID_CREATE_11},
    {"", 0},
#line 93 "src/classid-loader.gperf"
    {"loader/builtin", SIXEL_LOADER_COMPONENT_CLASSID_CREATE_1},
#line 101 "src/classid-loader.gperf"
    {"loader/libtiff", SIXEL_LOADER_COMPONENT_CLASSID_CREATE_9},
    {"", 0}, {"", 0}, {"", 0},
#line 98 "src/classid-loader.gperf"
    {"loader/libjpeg", SIXEL_LOADER_COMPONENT_CLASSID_CREATE_6},
#line 100 "src/classid-loader.gperf"
    {"loader/librsvg", SIXEL_LOADER_COMPONENT_CLASSID_CREATE_8},
    {"", 0}, {"", 0}, {"", 0},
#line 96 "src/classid-loader.gperf"
    {"loader/gdk-pixbuf2", SIXEL_LOADER_COMPONENT_CLASSID_CREATE_4},
#line 97 "src/classid-loader.gperf"
    {"loader/gnome-thumbnailer", SIXEL_LOADER_COMPONENT_CLASSID_CREATE_5},
    {"", 0}, {"", 0}, {"", 0}, {"", 0},
#line 94 "src/classid-loader.gperf"
    {"loader/coregraphics", SIXEL_LOADER_COMPONENT_CLASSID_CREATE_2}
  };

const struct sixel_loader_component_classid_entry *
sixel_loader_component_classid_lookup (register const char *str, register unsigned int len)
{
  if (len <= MAX_WORD_LENGTH && len >= MIN_WORD_LENGTH)
    {
      unsigned int key = sixel_loader_component_classid_hash (str, len);

      if (key <= MAX_HASH_VALUE)
        {
          register const char *s = sixel_loader_component_classid_wordlist[key].name;

          if (*str == *s && !strcmp (str + 1, s + 1))
            return &sixel_loader_component_classid_wordlist[key];
        }
    }
  return 0;
}
#line 105 "src/classid-loader.gperf"

#undef TOTAL_KEYWORDS
#undef MIN_WORD_LENGTH
#undef MAX_WORD_LENGTH
#undef MIN_HASH_VALUE
#undef MAX_HASH_VALUE
