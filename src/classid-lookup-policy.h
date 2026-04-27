/* ANSI-C code produced by gperf version 3.0.3 */
/* Command-line: /Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/gperf -C -N sixel_lookup_policy_classid_lookup --language=ANSI-C -H sixel_lookup_policy_classid_hash -W sixel_lookup_policy_classid_wordlist ../../src/classid.gperf  */
/* Computed positions: -k'8-9' */

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

#line 6 "../../src/classid.gperf"

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

typedef SIXELSTATUS (*sixel_lookup_policy_create_fn)(
    sixel_allocator_t *allocator,
    sixel_lookup_policy_interface_t **policy);
#line 26 "../../src/classid.gperf"
struct sixel_lookup_policy_classid_entry {
    char const *name;
    sixel_lookup_policy_create_fn create;
};

#define TOTAL_KEYWORDS 22
#define MIN_WORD_LENGTH 15
#define MAX_WORD_LENGTH 27
#define MIN_HASH_VALUE 16
#define MAX_HASH_VALUE 49
/* maximum key range = 34, duplicates = 0 */

#ifdef __GNUC__
__inline
#else
#ifdef __cplusplus
inline
#endif
#endif
static unsigned int
sixel_lookup_policy_classid_hash (register const char *str, register unsigned int len)
{
  static const unsigned char asso_values[] =
    {
      50, 50, 50, 50, 50, 50, 50, 50, 50, 50,
      50, 50, 50, 50, 50, 50, 50, 50, 50, 50,
      50, 50, 50, 50, 50, 50, 50, 50, 50, 50,
      50, 50, 50, 50, 50, 50, 50, 50, 50, 50,
      50, 50, 50, 50, 50, 50, 50, 50, 50, 50,
      50, 50, 50, 25, 20, 50, 50, 50, 50, 50,
      50, 50, 50, 50, 50, 50, 50, 50, 50, 50,
      50, 50, 50, 50, 50, 50, 50, 50, 50, 50,
      50, 50, 50, 50, 50, 50, 50, 50, 50, 50,
      50, 50, 50, 50, 50, 50, 50, 10,  5,  5,
      50,  5,  0, 50,  0, 50, 50, 50, 50,  0,
       0,  0,  0, 50,  5, 50, 50, 50,  0, 50,
      50,  5, 50, 50, 50, 50, 50, 50, 50, 50,
      50, 50, 50, 50, 50, 50, 50, 50, 50, 50,
      50, 50, 50, 50, 50, 50, 50, 50, 50, 50,
      50, 50, 50, 50, 50, 50, 50, 50, 50, 50,
      50, 50, 50, 50, 50, 50, 50, 50, 50, 50,
      50, 50, 50, 50, 50, 50, 50, 50, 50, 50,
      50, 50, 50, 50, 50, 50, 50, 50, 50, 50,
      50, 50, 50, 50, 50, 50, 50, 50, 50, 50,
      50, 50, 50, 50, 50, 50, 50, 50, 50, 50,
      50, 50, 50, 50, 50, 50, 50, 50, 50, 50,
      50, 50, 50, 50, 50, 50, 50, 50, 50, 50,
      50, 50, 50, 50, 50, 50, 50, 50, 50, 50,
      50, 50, 50, 50, 50, 50, 50, 50, 50, 50,
      50, 50, 50, 50, 50, 50
    };
  return len + asso_values[(unsigned char)str[8]] + asso_values[(unsigned char)str[7]];
}

static const struct sixel_lookup_policy_classid_entry sixel_lookup_policy_classid_wordlist[] =
  {
    {"", 0}, {"", 0}, {"", 0}, {"", 0}, {"", 0}, {"", 0}, {"", 0}, {"", 0}, {"", 0},
    {"", 0}, {"", 0}, {"", 0}, {"", 0}, {"", 0}, {"", 0}, {"", 0},
#line 47 "../../src/classid.gperf"
    {"lookup/none.8bit", sixel_lookup_policy_none_8bit_new},
#line 39 "../../src/classid.gperf"
    {"lookup/fhedt.8bit", sixel_lookup_policy_fhedt_8bit_new},
#line 51 "../../src/classid.gperf"
    {"lookup/vptree.8bit", sixel_lookup_policy_vptree_8bit_new},
#line 48 "../../src/classid.gperf"
    {"lookup/none.float32", sixel_lookup_policy_none_float32_new},
#line 40 "../../src/classid.gperf"
    {"lookup/fhedt.float32", sixel_lookup_policy_fhedt_float32_new},
#line 52 "../../src/classid.gperf"
    {"lookup/vptree.float32", sixel_lookup_policy_vptree_float32_new},
    {"", 0},
#line 43 "../../src/classid.gperf"
    {"lookup/mono-darkbg.8bit", sixel_lookup_policy_mono_darkbg_8bit_new},
#line 45 "../../src/classid.gperf"
    {"lookup/mono-lightbg.8bit", sixel_lookup_policy_mono_lightbg_8bit_new},
#line 49 "../../src/classid.gperf"
    {"lookup/rbc.8bit", sixel_lookup_policy_rbc_8bit_new},
#line 44 "../../src/classid.gperf"
    {"lookup/mono-darkbg.float32", sixel_lookup_policy_mono_darkbg_float32_new},
#line 46 "../../src/classid.gperf"
    {"lookup/mono-lightbg.float32", sixel_lookup_policy_mono_lightbg_float32_new},
#line 50 "../../src/classid.gperf"
    {"lookup/rbc.float32", sixel_lookup_policy_rbc_float32_new},
#line 35 "../../src/classid.gperf"
    {"lookup/certlut.8bit", sixel_lookup_policy_certlut_8bit_new},
    {"", 0},
#line 37 "../../src/classid.gperf"
    {"lookup/eytzinger.8bit", sixel_lookup_policy_eytzinger_8bit_new},
#line 36 "../../src/classid.gperf"
    {"lookup/certlut.float32", sixel_lookup_policy_certlut_float32_new},
#line 41 "../../src/classid.gperf"
    {"lookup/mahalanobis.8bit", sixel_lookup_policy_mahalanobis_8bit_new},
#line 38 "../../src/classid.gperf"
    {"lookup/eytzinger.float32", sixel_lookup_policy_eytzinger_float32_new},
    {"", 0},
#line 42 "../../src/classid.gperf"
    {"lookup/mahalanobis.float32", sixel_lookup_policy_mahalanobis_float32_new},
    {"", 0}, {"", 0}, {"", 0}, {"", 0},
#line 33 "../../src/classid.gperf"
    {"lookup/6bit.8bit", sixel_lookup_policy_6bit_8bit_new},
    {"", 0}, {"", 0},
#line 34 "../../src/classid.gperf"
    {"lookup/6bit.float32", sixel_lookup_policy_6bit_float32_new},
    {"", 0},
#line 31 "../../src/classid.gperf"
    {"lookup/5bit.8bit", sixel_lookup_policy_5bit_8bit_new},
    {"", 0}, {"", 0},
#line 32 "../../src/classid.gperf"
    {"lookup/5bit.float32", sixel_lookup_policy_5bit_float32_new}
  };

const struct sixel_lookup_policy_classid_entry *
sixel_lookup_policy_classid_lookup (register const char *str, register unsigned int len)
{
  if (len <= MAX_WORD_LENGTH && len >= MIN_WORD_LENGTH)
    {
      unsigned int key = sixel_lookup_policy_classid_hash (str, len);

      if (key <= MAX_HASH_VALUE)
        {
          register const char *s = sixel_lookup_policy_classid_wordlist[key].name;

          if (*str == *s && !strcmp (str + 1, s + 1))
            return &sixel_lookup_policy_classid_wordlist[key];
        }
    }
  return 0;
}
#line 53 "../../src/classid.gperf"

#undef TOTAL_KEYWORDS
#undef MIN_WORD_LENGTH
#undef MAX_WORD_LENGTH
#undef MIN_HASH_VALUE
#undef MAX_HASH_VALUE
