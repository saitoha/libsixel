/* ANSI-C code produced by gperf version 3.0.3 */
/* Command-line: /Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/gperf -C -N sixel_dither_policy_classid_lookup --language=ANSI-C -H sixel_dither_policy_classid_hash -W sixel_dither_policy_classid_wordlist ../../src/classid-dither.gperf  */
/* Computed positions: -k'8,14' */

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

#line 6 "../../src/classid-dither.gperf"

#if defined(HAVE_STRING_H) && HAVE_STRING_H
#include <string.h>
#endif
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

typedef SIXELSTATUS (*sixel_dither_policy_create_fn)(
    sixel_allocator_t *allocator,
    void **policy);
#line 29 "../../src/classid-dither.gperf"
struct sixel_dither_policy_classid_entry {
    char const *name;
    sixel_dither_policy_create_fn create;
};

#define TOTAL_KEYWORDS 28
#define MIN_WORD_LENGTH 14
#define MAX_WORD_LENGTH 25
#define MIN_HASH_VALUE 19
#define MAX_HASH_VALUE 52
/* maximum key range = 34, duplicates = 0 */

#ifdef __GNUC__
__inline
#else
#ifdef __cplusplus
inline
#endif
#endif
static unsigned int
sixel_dither_policy_classid_hash (register const char *str, register unsigned int len)
{
  static const unsigned char asso_values[] =
    {
      53, 53, 53, 53, 53, 53, 53, 53, 53, 53,
      53, 53, 53, 53, 53, 53, 53, 53, 53, 53,
      53, 53, 53, 53, 53, 53, 53, 53, 53, 53,
      53, 53, 53, 53, 53, 53, 53, 53, 53, 53,
      53, 53, 53, 53, 53, 53,  5, 53, 53, 30,
      15,  5, 53, 53, 53, 53, 53, 53, 53, 53,
      53, 53, 53, 53, 53, 53, 53, 53, 53, 53,
      53, 53, 53, 53, 53, 53, 53, 53, 53, 53,
      53, 53, 53, 53, 53, 53, 53, 53, 53, 53,
      53, 53, 53, 53, 53, 53, 53, 15,  5, 53,
      53,  5, 10, 53, 53, 10, 15, 53,  0, 53,
      25, 10, 53, 53,  0,  0,  5, 53, 53, 53,
       5, 53, 53, 53, 53, 53, 53, 53, 53, 53,
      53, 53, 53, 53, 53, 53, 53, 53, 53, 53,
      53, 53, 53, 53, 53, 53, 53, 53, 53, 53,
      53, 53, 53, 53, 53, 53, 53, 53, 53, 53,
      53, 53, 53, 53, 53, 53, 53, 53, 53, 53,
      53, 53, 53, 53, 53, 53, 53, 53, 53, 53,
      53, 53, 53, 53, 53, 53, 53, 53, 53, 53,
      53, 53, 53, 53, 53, 53, 53, 53, 53, 53,
      53, 53, 53, 53, 53, 53, 53, 53, 53, 53,
      53, 53, 53, 53, 53, 53, 53, 53, 53, 53,
      53, 53, 53, 53, 53, 53, 53, 53, 53, 53,
      53, 53, 53, 53, 53, 53, 53, 53, 53, 53,
      53, 53, 53, 53, 53, 53, 53, 53, 53, 53,
      53, 53, 53, 53, 53, 53
    };
  return len + asso_values[(unsigned char)str[13]] + asso_values[(unsigned char)str[7]];
}

static const struct sixel_dither_policy_classid_entry sixel_dither_policy_classid_wordlist[] =
  {
    {"", 0}, {"", 0}, {"", 0}, {"", 0}, {"", 0}, {"", 0}, {"", 0}, {"", 0}, {"", 0},
    {"", 0}, {"", 0}, {"", 0}, {"", 0}, {"", 0}, {"", 0}, {"", 0}, {"", 0}, {"", 0},
    {"", 0},
#line 53 "../../src/classid-dither.gperf"
    {"dither/lso2.float32", sixel_dither_policy_lso2_float32_new},
    {"", 0},
#line 52 "../../src/classid-dither.gperf"
    {"dither/lso2.8bit", sixel_dither_policy_lso2_8bit_new},
    {"", 0},
#line 42 "../../src/classid-dither.gperf"
    {"dither/stucki.8bit", sixel_dither_policy_stucki_8bit_new},
#line 50 "../../src/classid-dither.gperf"
    {"dither/sierra3.8bit", sixel_dither_policy_sierra3_8bit_new},
    {"", 0},
#line 43 "../../src/classid-dither.gperf"
    {"dither/stucki.float32", sixel_dither_policy_stucki_float32_new},
#line 51 "../../src/classid-dither.gperf"
    {"dither/sierra3.float32", sixel_dither_policy_sierra3_float32_new},
#line 44 "../../src/classid-dither.gperf"
    {"dither/burkes.8bit", sixel_dither_policy_burkes_8bit_new},
#line 36 "../../src/classid-dither.gperf"
    {"dither/fs.8bit", sixel_dither_policy_fs_8bit_new},
#line 56 "../../src/classid-dither.gperf"
    {"dither/x_dither.8bit", sixel_dither_policy_x_dither_8bit_new},
#line 45 "../../src/classid-dither.gperf"
    {"dither/burkes.float32", sixel_dither_policy_burkes_float32_new},
#line 60 "../../src/classid-dither.gperf"
    {"dither/interframe.8bit", sixel_dither_policy_interframe_8bit_new},
#line 57 "../../src/classid-dither.gperf"
    {"dither/x_dither.float32", sixel_dither_policy_x_dither_float32_new},
#line 48 "../../src/classid-dither.gperf"
    {"dither/sierra2.8bit", sixel_dither_policy_sierra2_8bit_new},
#line 61 "../../src/classid-dither.gperf"
    {"dither/interframe.float32", sixel_dither_policy_interframe_float32_new},
#line 58 "../../src/classid-dither.gperf"
    {"dither/bluenoise.8bit", sixel_dither_policy_bluenoise_8bit_new},
#line 49 "../../src/classid-dither.gperf"
    {"dither/sierra2.float32", sixel_dither_policy_sierra2_float32_new},
#line 40 "../../src/classid-dither.gperf"
    {"dither/jajuni.8bit", sixel_dither_policy_jajuni_8bit_new},
#line 59 "../../src/classid-dither.gperf"
    {"dither/bluenoise.float32", sixel_dither_policy_bluenoise_float32_new},
#line 54 "../../src/classid-dither.gperf"
    {"dither/a_dither.8bit", sixel_dither_policy_a_dither_8bit_new},
#line 41 "../../src/classid-dither.gperf"
    {"dither/jajuni.float32", sixel_dither_policy_jajuni_float32_new},
#line 37 "../../src/classid-dither.gperf"
    {"dither/fs.float32", sixel_dither_policy_fs_float32_new},
#line 55 "../../src/classid-dither.gperf"
    {"dither/a_dither.float32", sixel_dither_policy_a_dither_float32_new},
#line 35 "../../src/classid-dither.gperf"
    {"dither/none.float32", sixel_dither_policy_none_float32_new},
#line 38 "../../src/classid-dither.gperf"
    {"dither/atkinson.8bit", sixel_dither_policy_atkinson_8bit_new},
#line 34 "../../src/classid-dither.gperf"
    {"dither/none.8bit", sixel_dither_policy_none_8bit_new},
    {"", 0},
#line 39 "../../src/classid-dither.gperf"
    {"dither/atkinson.float32", sixel_dither_policy_atkinson_float32_new},
#line 46 "../../src/classid-dither.gperf"
    {"dither/sierra1.8bit", sixel_dither_policy_sierra1_8bit_new},
    {"", 0}, {"", 0},
#line 47 "../../src/classid-dither.gperf"
    {"dither/sierra1.float32", sixel_dither_policy_sierra1_float32_new}
  };

const struct sixel_dither_policy_classid_entry *
sixel_dither_policy_classid_lookup (register const char *str, register unsigned int len)
{
  if (len <= MAX_WORD_LENGTH && len >= MIN_WORD_LENGTH)
    {
      unsigned int key = sixel_dither_policy_classid_hash (str, len);

      if (key <= MAX_HASH_VALUE)
        {
          register const char *s = sixel_dither_policy_classid_wordlist[key].name;

          if (*str == *s && !strcmp (str + 1, s + 1))
            return &sixel_dither_policy_classid_wordlist[key];
        }
    }
  return 0;
}
#line 62 "../../src/classid-dither.gperf"

#undef TOTAL_KEYWORDS
#undef MIN_WORD_LENGTH
#undef MAX_WORD_LENGTH
#undef MIN_HASH_VALUE
#undef MAX_HASH_VALUE
