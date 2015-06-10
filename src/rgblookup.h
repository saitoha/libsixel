/* C code produced by gperf version 3.0.3 */
/* Command-line: /Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/gperf -C -N=lookup_rgb --ignore-case -P rgblookup.gperf  */
/* Computed positions: -k'1,3,5-9,12-15,$' */

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
error "gperf generated tables don't work with this execution character set. Please report a bug to <bug-gnu-gperf@gnu.org>."
#endif

#line 8 "rgblookup.gperf"

#include <stddef.h>
#line 2 "rgblookup.gperf"
struct color {
    char *name;
    unsigned char r;
    unsigned char g;
    unsigned char b;
};

#define TOTAL_KEYWORDS 752
#define MIN_WORD_LENGTH 3
#define MAX_WORD_LENGTH 22
#define MIN_HASH_VALUE 3
#define MAX_HASH_VALUE 5574
/* maximum key range = 5572, duplicates = 0 */

#ifndef GPERF_DOWNCASE
#define GPERF_DOWNCASE 1
static unsigned char gperf_downcase[256] =
{
    0,   1,   2,   3,   4,   5,   6,   7,   8,   9,  10,  11,  12,  13,  14,
    15,  16,  17,  18,  19,  20,  21,  22,  23,  24,  25,  26,  27,  28,  29,
    30,  31,  32,  33,  34,  35,  36,  37,  38,  39,  40,  41,  42,  43,  44,
    45,  46,  47,  48,  49,  50,  51,  52,  53,  54,  55,  56,  57,  58,  59,
    60,  61,  62,  63,  64,  97,  98,  99, 100, 101, 102, 103, 104, 105, 106,
    107, 108, 109, 110, 111, 112, 113, 114, 115, 116, 117, 118, 119, 120, 121,
    122,  91,  92,  93,  94,  95,  96,  97,  98,  99, 100, 101, 102, 103, 104,
    105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 116, 117, 118, 119,
    120, 121, 122, 123, 124, 125, 126, 127, 128, 129, 130, 131, 132, 133, 134,
    135, 136, 137, 138, 139, 140, 141, 142, 143, 144, 145, 146, 147, 148, 149,
    150, 151, 152, 153, 154, 155, 156, 157, 158, 159, 160, 161, 162, 163, 164,
    165, 166, 167, 168, 169, 170, 171, 172, 173, 174, 175, 176, 177, 178, 179,
    180, 181, 182, 183, 184, 185, 186, 187, 188, 189, 190, 191, 192, 193, 194,
    195, 196, 197, 198, 199, 200, 201, 202, 203, 204, 205, 206, 207, 208, 209,
    210, 211, 212, 213, 214, 215, 216, 217, 218, 219, 220, 221, 222, 223, 224,
    225, 226, 227, 228, 229, 230, 231, 232, 233, 234, 235, 236, 237, 238, 239,
    240, 241, 242, 243, 244, 245, 246, 247, 248, 249, 250, 251, 252, 253, 254,
    255
};
#endif

#ifndef GPERF_CASE_STRCMP
#define GPERF_CASE_STRCMP 1
static int
gperf_case_strcmp (s1, s2)
register const char *s1;
register const char *s2;
{
    for (;;)
    {
        unsigned char c1 = gperf_downcase[(unsigned char)*s1++];
        unsigned char c2 = gperf_downcase[(unsigned char)*s2++];
        if (c1 != 0 && c1 == c2)
            continue;
        return (int)c1 - (int)c2;
    }
}
#endif

#ifdef __GNUC__
__inline
#else
#ifdef __cplusplus
inline
#endif
#endif
static unsigned int
hash (str, len)
register const char *str;
register unsigned int len;
{
    static const unsigned short asso_values[] =
    {
        5575, 5575, 5575, 5575, 5575, 5575, 5575, 5575, 5575, 5575,
        5575, 5575, 5575, 5575, 5575, 5575, 5575, 5575, 5575, 5575,
        5575, 5575, 5575, 5575, 5575, 5575, 5575, 5575, 5575, 5575,
        5575, 5575,  520, 5575, 5575, 5575, 5575, 5575, 5575, 5575,
        5575, 5575, 5575, 5575, 5575, 5575, 5575, 5575,  920,   25,
        20,    5,    0, 1007,  841,   16,  915,  840, 5575, 5575,
        5575, 5575, 5575, 5575, 5575,   80,    5,  980,    0,    0,
        55,    0,  670,  673,    0,  395,  215,  190,  160,  100,
        1015,  145,    0,    0,  155,  325,  740,  831, 5575,  265,
        5575, 5575, 5575, 5575, 5575, 5575, 5575,   80,    5,  980,
        0,    0,   55,    0,  670,  673,    0,  395,  215,  190,
        160,  100, 1015,  145,    0,    0,  155,  325,  740,  831,
        5575,  265, 5575, 5575, 5575, 5575, 5575, 5575, 5575, 5575,
        5575, 5575, 5575, 5575, 5575, 5575, 5575, 5575, 5575, 5575,
        5575, 5575, 5575, 5575, 5575, 5575, 5575, 5575, 5575, 5575,
        5575, 5575, 5575, 5575, 5575, 5575, 5575, 5575, 5575, 5575,
        5575, 5575, 5575, 5575, 5575, 5575, 5575, 5575, 5575, 5575,
        5575, 5575, 5575, 5575, 5575, 5575, 5575, 5575, 5575, 5575,
        5575, 5575, 5575, 5575, 5575, 5575, 5575, 5575, 5575, 5575,
        5575, 5575, 5575, 5575, 5575, 5575, 5575, 5575, 5575, 5575,
        5575, 5575, 5575, 5575, 5575, 5575, 5575, 5575, 5575, 5575,
        5575, 5575, 5575, 5575, 5575, 5575, 5575, 5575, 5575, 5575,
        5575, 5575, 5575, 5575, 5575, 5575, 5575, 5575, 5575, 5575,
        5575, 5575, 5575, 5575, 5575, 5575, 5575, 5575, 5575, 5575,
        5575, 5575, 5575, 5575, 5575, 5575, 5575, 5575, 5575, 5575,
        5575, 5575, 5575, 5575, 5575, 5575
    };
    register unsigned int hval = len;

    switch (hval)
    {
    default:
        hval += asso_values[(unsigned char)str[14]];
    /*FALLTHROUGH*/
    case 14:
        hval += asso_values[(unsigned char)str[13]];
    /*FALLTHROUGH*/
    case 13:
        hval += asso_values[(unsigned char)str[12]];
    /*FALLTHROUGH*/
    case 12:
        hval += asso_values[(unsigned char)str[11]];
    /*FALLTHROUGH*/
    case 11:
    case 10:
    case 9:
        hval += asso_values[(unsigned char)str[8]];
    /*FALLTHROUGH*/
    case 8:
        hval += asso_values[(unsigned char)str[7]];
    /*FALLTHROUGH*/
    case 7:
        hval += asso_values[(unsigned char)str[6]];
    /*FALLTHROUGH*/
    case 6:
        hval += asso_values[(unsigned char)str[5]];
    /*FALLTHROUGH*/
    case 5:
        hval += asso_values[(unsigned char)str[4]];
    /*FALLTHROUGH*/
    case 4:
    case 3:
        hval += asso_values[(unsigned char)str[2]];
    /*FALLTHROUGH*/
    case 2:
    case 1:
        hval += asso_values[(unsigned char)str[0]];
        break;
    }
    return hval + asso_values[(unsigned char)str[len - 1]];
}

struct stringpool_t
{
    char stringpool_str3[sizeof("red")];
    char stringpool_str4[sizeof("red4")];
    char stringpool_str5[sizeof("grey4")];
    char stringpool_str6[sizeof("grey44")];
    char stringpool_str7[sizeof("darkred")];
    char stringpool_str9[sizeof("red3")];
    char stringpool_str11[sizeof("grey34")];
    char stringpool_str15[sizeof("grey3")];
    char stringpool_str16[sizeof("grey43")];
    char stringpool_str21[sizeof("grey33")];
    char stringpool_str22[sizeof("grey74")];
    char stringpool_str24[sizeof("red2")];
    char stringpool_str26[sizeof("grey24")];
    char stringpool_str29[sizeof("red1")];
    char stringpool_str31[sizeof("grey14")];
    char stringpool_str32[sizeof("grey73")];
    char stringpool_str36[sizeof("grey23")];
    char stringpool_str37[sizeof("grey7")];
    char stringpool_str38[sizeof("grey47")];
    char stringpool_str41[sizeof("grey13")];
    char stringpool_str43[sizeof("grey37")];
    char stringpool_str45[sizeof("grey2")];
    char stringpool_str46[sizeof("grey42")];
    char stringpool_str51[sizeof("grey32")];
    char stringpool_str54[sizeof("grey77")];
    char stringpool_str55[sizeof("grey1")];
    char stringpool_str56[sizeof("grey41")];
    char stringpool_str58[sizeof("grey27")];
    char stringpool_str61[sizeof("grey31")];
    char stringpool_str62[sizeof("grey72")];
    char stringpool_str63[sizeof("grey17")];
    char stringpool_str66[sizeof("grey22")];
    char stringpool_str71[sizeof("grey12")];
    char stringpool_str72[sizeof("grey71")];
    char stringpool_str76[sizeof("grey21")];
    char stringpool_str81[sizeof("grey11")];
    char stringpool_str85[sizeof("gray4")];
    char stringpool_str86[sizeof("gray44")];
    char stringpool_str91[sizeof("gray34")];
    char stringpool_str95[sizeof("gray3")];
    char stringpool_str96[sizeof("gray43")];
    char stringpool_str101[sizeof("gray33")];
    char stringpool_str102[sizeof("gray74")];
    char stringpool_str105[sizeof("snow4")];
    char stringpool_str106[sizeof("gray24")];
    char stringpool_str111[sizeof("gray14")];
    char stringpool_str112[sizeof("gray73")];
    char stringpool_str115[sizeof("snow3")];
    char stringpool_str116[sizeof("gray23")];
    char stringpool_str117[sizeof("gray7")];
    char stringpool_str118[sizeof("gray47")];
    char stringpool_str121[sizeof("gray13")];
    char stringpool_str123[sizeof("gray37")];
    char stringpool_str125[sizeof("gray2")];
    char stringpool_str126[sizeof("gray42")];
    char stringpool_str131[sizeof("gray32")];
    char stringpool_str134[sizeof("gray77")];
    char stringpool_str135[sizeof("gray1")];
    char stringpool_str136[sizeof("gray41")];
    char stringpool_str138[sizeof("gray27")];
    char stringpool_str141[sizeof("gray31")];
    char stringpool_str142[sizeof("gray72")];
    char stringpool_str143[sizeof("gray17")];
    char stringpool_str145[sizeof("snow2")];
    char stringpool_str146[sizeof("gray22")];
    char stringpool_str151[sizeof("gray12")];
    char stringpool_str152[sizeof("gray71")];
    char stringpool_str155[sizeof("snow1")];
    char stringpool_str156[sizeof("gray21")];
    char stringpool_str161[sizeof("gray11")];
    char stringpool_str166[sizeof("green4")];
    char stringpool_str172[sizeof("springgreen4")];
    char stringpool_str176[sizeof("green3")];
    char stringpool_str182[sizeof("springgreen3")];
    char stringpool_str186[sizeof("orange")];
    char stringpool_str187[sizeof("orange4")];
    char stringpool_str189[sizeof("orangered")];
    char stringpool_str190[sizeof("orangered4")];
    char stringpool_str195[sizeof("orangered3")];
    char stringpool_str197[sizeof("orange3")];
    char stringpool_str206[sizeof("green2")];
    char stringpool_str210[sizeof("orangered2")];
    char stringpool_str212[sizeof("springgreen2")];
    char stringpool_str215[sizeof("orangered1")];
    char stringpool_str216[sizeof("green1")];
    char stringpool_str219[sizeof("gold")];
    char stringpool_str220[sizeof("gold4")];
    char stringpool_str222[sizeof("springgreen1")];
    char stringpool_str227[sizeof("orange2")];
    char stringpool_str230[sizeof("gold3")];
    char stringpool_str237[sizeof("orange1")];
    char stringpool_str247[sizeof("sienna4")];
    char stringpool_str249[sizeof("seagreen4")];
    char stringpool_str253[sizeof("darkseagreen4")];
    char stringpool_str257[sizeof("sienna3")];
    char stringpool_str259[sizeof("seagreen3")];
    char stringpool_str260[sizeof("gold2")];
    char stringpool_str263[sizeof("darkseagreen3")];
    char stringpool_str269[sizeof("grey")];
    char stringpool_str270[sizeof("gold1")];
    char stringpool_str271[sizeof("brown4")];
    char stringpool_str281[sizeof("brown3")];
    char stringpool_str287[sizeof("sienna2")];
    char stringpool_str289[sizeof("seagreen2")];
    char stringpool_str293[sizeof("darkseagreen2")];
    char stringpool_str297[sizeof("sienna1")];
    char stringpool_str299[sizeof("seagreen1")];
    char stringpool_str303[sizeof("darkseagreen1")];
    char stringpool_str311[sizeof("brown2")];
    char stringpool_str319[sizeof("tan4")];
    char stringpool_str321[sizeof("brown1")];
    char stringpool_str324[sizeof("tan3")];
    char stringpool_str325[sizeof("green")];
    char stringpool_str326[sizeof("sienna")];
    char stringpool_str329[sizeof("darkgreen")];
    char stringpool_str331[sizeof("springgreen")];
    char stringpool_str334[sizeof("blue")];
    char stringpool_str335[sizeof("blue4")];
    char stringpool_str336[sizeof("bisque")];
    char stringpool_str337[sizeof("bisque4")];
    char stringpool_str339[sizeof("tan2")];
    char stringpool_str344[sizeof("tan1")];
    char stringpool_str345[sizeof("blue3")];
    char stringpool_str347[sizeof("bisque3")];
    char stringpool_str349[sizeof("gray")];
    char stringpool_str350[sizeof("darkorange")];
    char stringpool_str351[sizeof("darkorange4")];
    char stringpool_str356[sizeof("darkorange3")];
    char stringpool_str371[sizeof("darkorange2")];
    char stringpool_str375[sizeof("blue2")];
    char stringpool_str376[sizeof("darkorange1")];
    char stringpool_str377[sizeof("bisque2")];
    char stringpool_str381[sizeof("forestgreen")];
    char stringpool_str385[sizeof("blue1")];
    char stringpool_str387[sizeof("bisque1")];
    char stringpool_str408[sizeof("seagreen")];
    char stringpool_str409[sizeof("sea green")];
    char stringpool_str410[sizeof("azure")];
    char stringpool_str411[sizeof("azure4")];
    char stringpool_str412[sizeof("darkseagreen")];
    char stringpool_str421[sizeof("azure3")];
    char stringpool_str428[sizeof("darkgoldenrod")];
    char stringpool_str429[sizeof("darkgoldenrod4")];
    char stringpool_str430[sizeof("brown")];
    char stringpool_str435[sizeof("slategray4")];
    char stringpool_str439[sizeof("darkgoldenrod3")];
    char stringpool_str440[sizeof("slategray3")];
    char stringpool_str451[sizeof("azure2")];
    char stringpool_str455[sizeof("slategray2")];
    char stringpool_str457[sizeof("maroon4")];
    char stringpool_str460[sizeof("slategray1")];
    char stringpool_str461[sizeof("azure1")];
    char stringpool_str467[sizeof("maroon3")];
    char stringpool_str469[sizeof("darkgoldenrod2")];
    char stringpool_str478[sizeof("tan")];
    char stringpool_str479[sizeof("darkgoldenrod1")];
    char stringpool_str482[sizeof("salmon4")];
    char stringpool_str484[sizeof("goldenrod")];
    char stringpool_str485[sizeof("goldenrod4")];
    char stringpool_str490[sizeof("goldenrod3")];
    char stringpool_str491[sizeof("saddlebrown")];
    char stringpool_str492[sizeof("salmon3")];
    char stringpool_str497[sizeof("maroon2")];
    char stringpool_str505[sizeof("goldenrod2")];
    char stringpool_str507[sizeof("maroon1")];
    char stringpool_str510[sizeof("goldenrod1")];
    char stringpool_str521[sizeof("darkmagenta")];
    char stringpool_str522[sizeof("salmon2")];
    char stringpool_str528[sizeof("dark red")];
    char stringpool_str532[sizeof("salmon1")];
    char stringpool_str538[sizeof("darkgrey")];
    char stringpool_str540[sizeof("lightgreen")];
    char stringpool_str553[sizeof("darkblue")];
    char stringpool_str555[sizeof("dodgerblue")];
    char stringpool_str556[sizeof("dodgerblue4")];
    char stringpool_str561[sizeof("dodgerblue3")];
    char stringpool_str564[sizeof("mistyrose")];
    char stringpool_str565[sizeof("mistyrose4")];
    char stringpool_str570[sizeof("mistyrose3")];
    char stringpool_str576[sizeof("dodgerblue2")];
    char stringpool_str581[sizeof("dodgerblue1")];
    char stringpool_str585[sizeof("mistyrose2")];
    char stringpool_str590[sizeof("mistyrose1")];
    char stringpool_str593[sizeof("magenta4")];
    char stringpool_str603[sizeof("magenta3")];
    char stringpool_str607[sizeof("tomato4")];
    char stringpool_str616[sizeof("maroon")];
    char stringpool_str617[sizeof("tomato3")];
    char stringpool_str618[sizeof("darkgray")];
    char stringpool_str619[sizeof("slategrey")];
    char stringpool_str633[sizeof("magenta2")];
    char stringpool_str634[sizeof("slateblue")];
    char stringpool_str635[sizeof("slateblue4")];
    char stringpool_str640[sizeof("slateblue3")];
    char stringpool_str641[sizeof("salmon")];
    char stringpool_str643[sizeof("magenta1")];
    char stringpool_str647[sizeof("tomato2")];
    char stringpool_str655[sizeof("slateblue2")];
    char stringpool_str657[sizeof("tomato1")];
    char stringpool_str660[sizeof("slateblue1")];
    char stringpool_str672[sizeof("magenta")];
    char stringpool_str683[sizeof("beige")];
    char stringpool_str690[sizeof("dark green")];
    char stringpool_str699[sizeof("slategray")];
    char stringpool_str700[sizeof("linen")];
    char stringpool_str706[sizeof("tomato")];
    char stringpool_str710[sizeof("orange red")];
    char stringpool_str727[sizeof("dimgrey")];
    char stringpool_str728[sizeof("dim grey")];
    char stringpool_str734[sizeof("limegreen")];
    char stringpool_str751[sizeof("dodger blue")];
    char stringpool_str755[sizeof("darksalmon")];
    char stringpool_str769[sizeof("steelblue")];
    char stringpool_str770[sizeof("steelblue4")];
    char stringpool_str775[sizeof("steelblue3")];
    char stringpool_str783[sizeof("lightseagreen")];
    char stringpool_str788[sizeof("darkslateblue")];
    char stringpool_str790[sizeof("steelblue2")];
    char stringpool_str795[sizeof("steelblue1")];
    char stringpool_str799[sizeof("lightgoldenrod")];
    char stringpool_str800[sizeof("lightgoldenrod4")];
    char stringpool_str807[sizeof("dimgray")];
    char stringpool_str808[sizeof("dim gray")];
    char stringpool_str809[sizeof("darkslategray4")];
    char stringpool_str810[sizeof("lightgoldenrod3")];
    char stringpool_str812[sizeof("skyblue")];
    char stringpool_str813[sizeof("skyblue4")];
    char stringpool_str818[sizeof("sky blue")];
    char stringpool_str819[sizeof("darkslategray3")];
    char stringpool_str823[sizeof("skyblue3")];
    char stringpool_str840[sizeof("lightgoldenrod2")];
    char stringpool_str846[sizeof("grey94")];
    char stringpool_str847[sizeof("grey64")];
    char stringpool_str849[sizeof("darkslategray2")];
    char stringpool_str850[sizeof("lightgoldenrod1")];
    char stringpool_str853[sizeof("skyblue2")];
    char stringpool_str856[sizeof("grey93")];
    char stringpool_str857[sizeof("grey63")];
    char stringpool_str859[sizeof("darkslategray1")];
    char stringpool_str863[sizeof("skyblue1")];
    char stringpool_str867[sizeof("lightsalmon4")];
    char stringpool_str868[sizeof("olivedrab4")];
    char stringpool_str871[sizeof("dark orange")];
    char stringpool_str872[sizeof("olivedrab")];
    char stringpool_str873[sizeof("olivedrab3")];
    char stringpool_str875[sizeof("slate grey")];
    char stringpool_str877[sizeof("lightsalmon3")];
    char stringpool_str878[sizeof("grey97")];
    char stringpool_str879[sizeof("grey67")];
    char stringpool_str880[sizeof("black")];
    char stringpool_str886[sizeof("grey92")];
    char stringpool_str887[sizeof("grey62")];
    char stringpool_str888[sizeof("olivedrab2")];
    char stringpool_str891[sizeof("deepskyblue")];
    char stringpool_str892[sizeof("deepskyblue4")];
    char stringpool_str893[sizeof("olivedrab1")];
    char stringpool_str896[sizeof("grey91")];
    char stringpool_str897[sizeof("grey61")];
    char stringpool_str902[sizeof("deepskyblue3")];
    char stringpool_str907[sizeof("lightsalmon2")];
    char stringpool_str909[sizeof("lightgrey")];
    char stringpool_str917[sizeof("lightsalmon1")];
    char stringpool_str921[sizeof("grey84")];
    char stringpool_str922[sizeof("indianred")];
    char stringpool_str923[sizeof("indianred4")];
    char stringpool_str924[sizeof("lightblue")];
    char stringpool_str925[sizeof("lightblue4")];
    char stringpool_str926[sizeof("gray94")];
    char stringpool_str927[sizeof("gray64")];
    char stringpool_str928[sizeof("indianred3")];
    char stringpool_str930[sizeof("lightblue3")];
    char stringpool_str931[sizeof("grey83")];
    char stringpool_str932[sizeof("deepskyblue2")];
    char stringpool_str935[sizeof("snow")];
    char stringpool_str936[sizeof("gray93")];
    char stringpool_str937[sizeof("gray63")];
    char stringpool_str942[sizeof("deepskyblue1")];
    char stringpool_str943[sizeof("indianred2")];
    char stringpool_str945[sizeof("lightblue2")];
    char stringpool_str948[sizeof("indianred1")];
    char stringpool_str949[sizeof("dark goldenrod")];
    char stringpool_str950[sizeof("lightblue1")];
    char stringpool_str953[sizeof("grey87")];
    char stringpool_str955[sizeof("slate gray")];
    char stringpool_str958[sizeof("gray97")];
    char stringpool_str959[sizeof("gray67")];
    char stringpool_str961[sizeof("grey82")];
    char stringpool_str962[sizeof("dark magenta")];
    char stringpool_str963[sizeof("darkturquoise")];
    char stringpool_str966[sizeof("gray92")];
    char stringpool_str967[sizeof("gray62")];
    char stringpool_str971[sizeof("grey81")];
    char stringpool_str976[sizeof("gray91")];
    char stringpool_str977[sizeof("gray61")];
    char stringpool_str987[sizeof("gainsboro")];
    char stringpool_str989[sizeof("lightgray")];
    char stringpool_str992[sizeof("wheat4")];
    char stringpool_str993[sizeof("darkslategrey")];
    char stringpool_str1001[sizeof("gray84")];
    char stringpool_str1002[sizeof("wheat3")];
    char stringpool_str1004[sizeof("violetred")];
    char stringpool_str1005[sizeof("violetred4")];
    char stringpool_str1010[sizeof("violetred3")];
    char stringpool_str1011[sizeof("gray83")];
    char stringpool_str1012[sizeof("spring green")];
    char stringpool_str1013[sizeof("grey54")];
    char stringpool_str1023[sizeof("grey53")];
    char stringpool_str1025[sizeof("violetred2")];
    char stringpool_str1026[sizeof("lightsalmon")];
    char stringpool_str1030[sizeof("violetred1")];
    char stringpool_str1032[sizeof("wheat2")];
    char stringpool_str1033[sizeof("gray87")];
    char stringpool_str1034[sizeof("royalblue")];
    char stringpool_str1035[sizeof("royalblue4")];
    char stringpool_str1040[sizeof("royalblue3")];
    char stringpool_str1041[sizeof("gray82")];
    char stringpool_str1042[sizeof("wheat1")];
    char stringpool_str1044[sizeof("ivory4")];
    char stringpool_str1045[sizeof("grey57")];
    char stringpool_str1047[sizeof("lightskyblue")];
    char stringpool_str1048[sizeof("lightskyblue4")];
    char stringpool_str1051[sizeof("gray81")];
    char stringpool_str1053[sizeof("grey52")];
    char stringpool_str1054[sizeof("ivory3")];
    char stringpool_str1055[sizeof("royalblue2")];
    char stringpool_str1058[sizeof("lightskyblue3")];
    char stringpool_str1059[sizeof("dark grey")];
    char stringpool_str1060[sizeof("royalblue1")];
    char stringpool_str1061[sizeof("light green")];
    char stringpool_str1062[sizeof("forest green")];
    char stringpool_str1063[sizeof("grey51")];
    char stringpool_str1065[sizeof("cyan4")];
    char stringpool_str1072[sizeof("saddle brown")];
    char stringpool_str1073[sizeof("darkslategray")];
    char stringpool_str1074[sizeof("dark blue")];
    char stringpool_str1075[sizeof("cyan3")];
    char stringpool_str1077[sizeof("lightyellow4")];
    char stringpool_str1079[sizeof("lightsteelblue")];
    char stringpool_str1080[sizeof("lightsteelblue4")];
    char stringpool_str1084[sizeof("ivory2")];
    char stringpool_str1085[sizeof("misty rose")];
    char stringpool_str1087[sizeof("lightyellow3")];
    char stringpool_str1088[sizeof("lightskyblue2")];
    char stringpool_str1090[sizeof("lightsteelblue3")];
    char stringpool_str1093[sizeof("gray54")];
    char stringpool_str1094[sizeof("ivory1")];
    char stringpool_str1095[sizeof("lime green")];
    char stringpool_str1098[sizeof("lightskyblue1")];
    char stringpool_str1103[sizeof("gray53")];
    char stringpool_str1105[sizeof("cyan2")];
    char stringpool_str1106[sizeof("rosybrown4")];
    char stringpool_str1111[sizeof("rosybrown3")];
    char stringpool_str1115[sizeof("cyan1")];
    char stringpool_str1117[sizeof("lightyellow2")];
    char stringpool_str1119[sizeof("mediumseagreen")];
    char stringpool_str1120[sizeof("lightsteelblue2")];
    char stringpool_str1123[sizeof("lavender")];
    char stringpool_str1125[sizeof("gray57")];
    char stringpool_str1126[sizeof("rosybrown2")];
    char stringpool_str1127[sizeof("lightyellow1")];
    char stringpool_str1130[sizeof("lightsteelblue1")];
    char stringpool_str1131[sizeof("rosybrown1")];
    char stringpool_str1133[sizeof("gray52")];
    char stringpool_str1139[sizeof("dark gray")];
    char stringpool_str1143[sizeof("gray51")];
    char stringpool_str1146[sizeof("wheat")];
    char stringpool_str1154[sizeof("khaki4")];
    char stringpool_str1155[sizeof("slate blue")];
    char stringpool_str1156[sizeof("violet")];
    char stringpool_str1164[sizeof("khaki3")];
    char stringpool_str1165[sizeof("light grey")];
    char stringpool_str1167[sizeof("oldlace")];
    char stringpool_str1169[sizeof("navy")];
    char stringpool_str1176[sizeof("dark salmon")];
    char stringpool_str1180[sizeof("pink4")];
    char stringpool_str1189[sizeof("seashell4")];
    char stringpool_str1190[sizeof("pink3")];
    char stringpool_str1194[sizeof("khaki2")];
    char stringpool_str1199[sizeof("seashell3")];
    char stringpool_str1201[sizeof("coral4")];
    char stringpool_str1204[sizeof("khaki1")];
    char stringpool_str1205[sizeof("thistle")];
    char stringpool_str1206[sizeof("thistle4")];
    char stringpool_str1211[sizeof("coral3")];
    char stringpool_str1216[sizeof("thistle3")];
    char stringpool_str1220[sizeof("pink2")];
    char stringpool_str1221[sizeof("sandy brown")];
    char stringpool_str1224[sizeof("cyan")];
    char stringpool_str1229[sizeof("seashell2")];
    char stringpool_str1230[sizeof("pink1")];
    char stringpool_str1236[sizeof("purple")];
    char stringpool_str1237[sizeof("purple4")];
    char stringpool_str1239[sizeof("seashell1")];
    char stringpool_str1241[sizeof("coral2")];
    char stringpool_str1245[sizeof("light gray")];
    char stringpool_str1246[sizeof("thistle2")];
    char stringpool_str1247[sizeof("purple3")];
    char stringpool_str1251[sizeof("coral1")];
    char stringpool_str1256[sizeof("thistle1")];
    char stringpool_str1260[sizeof("mediumblue")];
    char stringpool_str1262[sizeof("turquoise")];
    char stringpool_str1263[sizeof("turquoise4")];
    char stringpool_str1265[sizeof("rosybrown")];
    char stringpool_str1268[sizeof("turquoise3")];
    char stringpool_str1277[sizeof("purple2")];
    char stringpool_str1283[sizeof("turquoise2")];
    char stringpool_str1287[sizeof("purple1")];
    char stringpool_str1288[sizeof("turquoise1")];
    char stringpool_str1290[sizeof("steel blue")];
    char stringpool_str1305[sizeof("light sea green")];
    char stringpool_str1307[sizeof("aliceblue")];
    char stringpool_str1308[sizeof("ivory")];
    char stringpool_str1310[sizeof("burlywood")];
    char stringpool_str1311[sizeof("burlywood4")];
    char stringpool_str1316[sizeof("burlywood3")];
    char stringpool_str1331[sizeof("burlywood2")];
    char stringpool_str1336[sizeof("burlywood1")];
    char stringpool_str1344[sizeof("peru")];
    char stringpool_str1345[sizeof("plum4")];
    char stringpool_str1355[sizeof("plum3")];
    char stringpool_str1364[sizeof("lightslategrey")];
    char stringpool_str1374[sizeof("lightslateblue")];
    char stringpool_str1375[sizeof("lawngreen")];
    char stringpool_str1383[sizeof("old lace")];
    char stringpool_str1385[sizeof("plum2")];
    char stringpool_str1388[sizeof("olive drab")];
    char stringpool_str1395[sizeof("plum1")];
    char stringpool_str1400[sizeof("palegreen4")];
    char stringpool_str1401[sizeof("medium sea green")];
    char stringpool_str1403[sizeof("seashell")];
    char stringpool_str1405[sizeof("palegreen3")];
    char stringpool_str1415[sizeof("coral")];
    char stringpool_str1418[sizeof("yellow4")];
    char stringpool_str1420[sizeof("palegreen2")];
    char stringpool_str1425[sizeof("palegreen1")];
    char stringpool_str1428[sizeof("yellow3")];
    char stringpool_str1443[sizeof("indian red")];
    char stringpool_str1444[sizeof("lightslategray")];
    char stringpool_str1445[sizeof("light blue")];
    char stringpool_str1453[sizeof("navyblue")];
    char stringpool_str1454[sizeof("dark sea green")];
    char stringpool_str1456[sizeof("medium blue")];
    char stringpool_str1458[sizeof("yellow2")];
    char stringpool_str1468[sizeof("yellow1")];
    char stringpool_str1480[sizeof("light goldenrod")];
    char stringpool_str1509[sizeof("white")];
    char stringpool_str1515[sizeof("dark slate grey")];
    char stringpool_str1517[sizeof("light salmon")];
    char stringpool_str1518[sizeof("aquamarine")];
    char stringpool_str1519[sizeof("aquamarine4")];
    char stringpool_str1524[sizeof("aquamarine3")];
    char stringpool_str1525[sizeof("violet red")];
    char stringpool_str1530[sizeof("dark slate blue")];
    char stringpool_str1531[sizeof("sandybrown")];
    char stringpool_str1534[sizeof("plum")];
    char stringpool_str1539[sizeof("aquamarine2")];
    char stringpool_str1544[sizeof("aquamarine1")];
    char stringpool_str1550[sizeof("chartreuse")];
    char stringpool_str1551[sizeof("chartreuse4")];
    char stringpool_str1555[sizeof("royal blue")];
    char stringpool_str1556[sizeof("chartreuse3")];
    char stringpool_str1559[sizeof("palegreen")];
    char stringpool_str1560[sizeof("mediumslateblue")];
    char stringpool_str1571[sizeof("chartreuse2")];
    char stringpool_str1574[sizeof("pink")];
    char stringpool_str1576[sizeof("chartreuse1")];
    char stringpool_str1582[sizeof("yellowgreen")];
    char stringpool_str1595[sizeof("dark slate gray")];
    char stringpool_str1626[sizeof("rosy brown")];
    char stringpool_str1639[sizeof("chocolate")];
    char stringpool_str1640[sizeof("chocolate4")];
    char stringpool_str1645[sizeof("chocolate3")];
    char stringpool_str1653[sizeof("darkcyan")];
    char stringpool_str1658[sizeof("palegoldenrod")];
    char stringpool_str1660[sizeof("chocolate2")];
    char stringpool_str1665[sizeof("chocolate1")];
    char stringpool_str1685[sizeof("grey9")];
    char stringpool_str1686[sizeof("grey49")];
    char stringpool_str1687[sizeof("grey6")];
    char stringpool_str1688[sizeof("grey46")];
    char stringpool_str1689[sizeof("cadetblue")];
    char stringpool_str1690[sizeof("cadetblue4")];
    char stringpool_str1691[sizeof("grey39")];
    char stringpool_str1693[sizeof("grey36")];
    char stringpool_str1695[sizeof("cadetblue3")];
    char stringpool_str1697[sizeof("greenyellow")];
    char stringpool_str1702[sizeof("grey79")];
    char stringpool_str1704[sizeof("grey76")];
    char stringpool_str1705[sizeof("midnightblue")];
    char stringpool_str1706[sizeof("grey29")];
    char stringpool_str1708[sizeof("grey26")];
    char stringpool_str1710[sizeof("cadetblue2")];
    char stringpool_str1711[sizeof("grey19")];
    char stringpool_str1713[sizeof("grey16")];
    char stringpool_str1715[sizeof("cadetblue1")];
    char stringpool_str1736[sizeof("lawn green")];
    char stringpool_str1755[sizeof("lightcoral")];
    char stringpool_str1759[sizeof("orchid")];
    char stringpool_str1760[sizeof("orchid4")];
    char stringpool_str1765[sizeof("gray9")];
    char stringpool_str1766[sizeof("gray49")];
    char stringpool_str1767[sizeof("gray6")];
    char stringpool_str1768[sizeof("gray46")];
    char stringpool_str1770[sizeof("orchid3")];
    char stringpool_str1771[sizeof("gray39")];
    char stringpool_str1773[sizeof("gray36")];
    char stringpool_str1782[sizeof("gray79")];
    char stringpool_str1784[sizeof("gray76")];
    char stringpool_str1786[sizeof("gray29")];
    char stringpool_str1788[sizeof("gray26")];
    char stringpool_str1791[sizeof("gray19")];
    char stringpool_str1793[sizeof("gray16")];
    char stringpool_str1797[sizeof("mediumorchid")];
    char stringpool_str1798[sizeof("mediumorchid4")];
    char stringpool_str1799[sizeof("mintcream")];
    char stringpool_str1800[sizeof("orchid2")];
    char stringpool_str1804[sizeof("lavenderblush4")];
    char stringpool_str1808[sizeof("mediumorchid3")];
    char stringpool_str1810[sizeof("orchid1")];
    char stringpool_str1814[sizeof("lavenderblush3")];
    char stringpool_str1826[sizeof("khaki")];
    char stringpool_str1828[sizeof("alice blue")];
    char stringpool_str1832[sizeof("dark turquoise")];
    char stringpool_str1835[sizeof("grey8")];
    char stringpool_str1836[sizeof("grey48")];
    char stringpool_str1838[sizeof("mediumorchid2")];
    char stringpool_str1841[sizeof("grey38")];
    char stringpool_str1844[sizeof("lavenderblush2")];
    char stringpool_str1845[sizeof("grey0")];
    char stringpool_str1846[sizeof("grey40")];
    char stringpool_str1848[sizeof("mediumorchid1")];
    char stringpool_str1851[sizeof("grey30")];
    char stringpool_str1852[sizeof("grey78")];
    char stringpool_str1854[sizeof("lavenderblush1")];
    char stringpool_str1856[sizeof("grey28")];
    char stringpool_str1861[sizeof("grey18")];
    char stringpool_str1862[sizeof("grey70")];
    char stringpool_str1865[sizeof("lightcyan4")];
    char stringpool_str1866[sizeof("grey20")];
    char stringpool_str1870[sizeof("lightcyan3")];
    char stringpool_str1871[sizeof("grey10")];
    char stringpool_str1885[sizeof("lightcyan2")];
    char stringpool_str1890[sizeof("lightcyan1")];
    char stringpool_str1893[sizeof("darkviolet")];
    char stringpool_str1897[sizeof("mediumspringgreen")];
    char stringpool_str1901[sizeof("lightgoldenrodyellow")];
    char stringpool_str1903[sizeof("darkolivegreen4")];
    char stringpool_str1907[sizeof("lightyellow")];
    char stringpool_str1913[sizeof("darkolivegreen3")];
    char stringpool_str1915[sizeof("gray8")];
    char stringpool_str1916[sizeof("gray48")];
    char stringpool_str1920[sizeof("pale green")];
    char stringpool_str1921[sizeof("gray38")];
    char stringpool_str1925[sizeof("gray0")];
    char stringpool_str1926[sizeof("gray40")];
    char stringpool_str1931[sizeof("gray30")];
    char stringpool_str1932[sizeof("gray78")];
    char stringpool_str1935[sizeof("honeydew4")];
    char stringpool_str1936[sizeof("gray28")];
    char stringpool_str1941[sizeof("gray18")];
    char stringpool_str1942[sizeof("gray70")];
    char stringpool_str1943[sizeof("darkolivegreen2")];
    char stringpool_str1945[sizeof("honeydew3")];
    char stringpool_str1946[sizeof("gray20")];
    char stringpool_str1951[sizeof("gray10")];
    char stringpool_str1953[sizeof("darkolivegreen1")];
    char stringpool_str1973[sizeof("mediumturquoise")];
    char stringpool_str1974[sizeof("navy blue")];
    char stringpool_str1975[sizeof("honeydew2")];
    char stringpool_str1985[sizeof("honeydew1")];
    char stringpool_str1986[sizeof("light slate grey")];
    char stringpool_str2011[sizeof("medium orchid")];
    char stringpool_str2019[sizeof("grey5")];
    char stringpool_str2020[sizeof("grey45")];
    char stringpool_str2024[sizeof("lightcyan")];
    char stringpool_str2025[sizeof("grey35")];
    char stringpool_str2036[sizeof("grey75")];
    char stringpool_str2038[sizeof("deep sky blue")];
    char stringpool_str2040[sizeof("grey25")];
    char stringpool_str2045[sizeof("grey15")];
    char stringpool_str2057[sizeof("mediumpurple")];
    char stringpool_str2058[sizeof("mediumpurple4")];
    char stringpool_str2061[sizeof("hotpink4")];
    char stringpool_str2062[sizeof("darkolivegreen")];
    char stringpool_str2066[sizeof("light slate gray")];
    char stringpool_str2068[sizeof("mediumpurple3")];
    char stringpool_str2071[sizeof("hotpink3")];
    char stringpool_str2089[sizeof("blanchedalmond")];
    char stringpool_str2098[sizeof("mediumpurple2")];
    char stringpool_str2099[sizeof("gray5")];
    char stringpool_str2100[sizeof("gray45")];
    char stringpool_str2101[sizeof("hotpink2")];
    char stringpool_str2104[sizeof("light sky blue")];
    char stringpool_str2105[sizeof("gray35")];
    char stringpool_str2108[sizeof("mediumpurple1")];
    char stringpool_str2111[sizeof("hotpink1")];
    char stringpool_str2116[sizeof("gray75")];
    char stringpool_str2118[sizeof("firebrick4")];
    char stringpool_str2120[sizeof("gray25")];
    char stringpool_str2123[sizeof("firebrick3")];
    char stringpool_str2125[sizeof("gray15")];
    char stringpool_str2126[sizeof("light steel blue")];
    char stringpool_str2130[sizeof("mint cream")];
    char stringpool_str2138[sizeof("firebrick2")];
    char stringpool_str2143[sizeof("firebrick1")];
    char stringpool_str2174[sizeof("dark cyan")];
    char stringpool_str2179[sizeof("pale goldenrod")];
    char stringpool_str2184[sizeof("mediumaquamarine")];
    char stringpool_str2193[sizeof("paleturquoise")];
    char stringpool_str2194[sizeof("paleturquoise4")];
    char stringpool_str2196[sizeof("light coral")];
    char stringpool_str2197[sizeof("medium slate blue")];
    char stringpool_str2199[sizeof("whitesmoke")];
    char stringpool_str2204[sizeof("paleturquoise3")];
    char stringpool_str2210[sizeof("cadet blue")];
    char stringpool_str2218[sizeof("antiquewhite")];
    char stringpool_str2219[sizeof("antiquewhite4")];
    char stringpool_str2223[sizeof("blueviolet")];
    char stringpool_str2224[sizeof("antique white")];
    char stringpool_str2229[sizeof("antiquewhite3")];
    char stringpool_str2234[sizeof("paleturquoise2")];
    char stringpool_str2244[sizeof("paleturquoise1")];
    char stringpool_str2248[sizeof("yellow")];
    char stringpool_str2251[sizeof("moccasin")];
    char stringpool_str2252[sizeof("deeppink4")];
    char stringpool_str2259[sizeof("antiquewhite2")];
    char stringpool_str2262[sizeof("deeppink3")];
    char stringpool_str2263[sizeof("yellow green")];
    char stringpool_str2266[sizeof("light slate blue")];
    char stringpool_str2269[sizeof("antiquewhite1")];
    char stringpool_str2272[sizeof("cornsilk4")];
    char stringpool_str2281[sizeof("dark orchid")];
    char stringpool_str2282[sizeof("cornsilk3")];
    char stringpool_str2292[sizeof("deeppink2")];
    char stringpool_str2302[sizeof("deeppink1")];
    char stringpool_str2312[sizeof("cornsilk2")];
    char stringpool_str2318[sizeof("light goldenrod yellow")];
    char stringpool_str2322[sizeof("cornsilk1")];
    char stringpool_str2325[sizeof("white smoke")];
    char stringpool_str2385[sizeof("light cyan")];
    char stringpool_str2388[sizeof("mediumvioletred")];
    char stringpool_str2401[sizeof("powderblue")];
    char stringpool_str2410[sizeof("medium aquamarine")];
    char stringpool_str2414[sizeof("dark violet")];
    char stringpool_str2424[sizeof("dark olive green")];
    char stringpool_str2433[sizeof("darkorchid")];
    char stringpool_str2434[sizeof("darkorchid4")];
    char stringpool_str2439[sizeof("darkorchid3")];
    char stringpool_str2454[sizeof("darkorchid2")];
    char stringpool_str2455[sizeof("hotpink")];
    char stringpool_str2459[sizeof("darkorchid1")];
    char stringpool_str2473[sizeof("lavenderblush")];
    char stringpool_str2483[sizeof("floral white")];
    char stringpool_str2512[sizeof("firebrick")];
    char stringpool_str2526[sizeof("grey99")];
    char stringpool_str2527[sizeof("grey69")];
    char stringpool_str2528[sizeof("grey96")];
    char stringpool_str2529[sizeof("grey66")];
    char stringpool_str2546[sizeof("midnight blue")];
    char stringpool_str2594[sizeof("ghostwhite")];
    char stringpool_str2597[sizeof("powder blue")];
    char stringpool_str2601[sizeof("grey89")];
    char stringpool_str2603[sizeof("grey86")];
    char stringpool_str2606[sizeof("gray99")];
    char stringpool_str2607[sizeof("gray69")];
    char stringpool_str2608[sizeof("gray96")];
    char stringpool_str2609[sizeof("gray66")];
    char stringpool_str2623[sizeof("lightpink4")];
    char stringpool_str2628[sizeof("lightpink3")];
    char stringpool_str2635[sizeof("floralwhite")];
    char stringpool_str2643[sizeof("lightpink2")];
    char stringpool_str2646[sizeof("deeppink")];
    char stringpool_str2648[sizeof("lightpink1")];
    char stringpool_str2666[sizeof("cornsilk")];
    char stringpool_str2676[sizeof("grey98")];
    char stringpool_str2677[sizeof("grey68")];
    char stringpool_str2681[sizeof("gray89")];
    char stringpool_str2683[sizeof("gray86")];
    char stringpool_str2686[sizeof("grey90")];
    char stringpool_str2687[sizeof("grey60")];
    char stringpool_str2693[sizeof("grey59")];
    char stringpool_str2695[sizeof("grey56")];
    char stringpool_str2720[sizeof("blanched almond")];
    char stringpool_str2735[sizeof("cornflowerblue")];
    char stringpool_str2741[sizeof("cornflower blue")];
    char stringpool_str2743[sizeof("dark khaki")];
    char stringpool_str2744[sizeof("blue violet")];
    char stringpool_str2751[sizeof("grey88")];
    char stringpool_str2756[sizeof("gray98")];
    char stringpool_str2757[sizeof("gray68")];
    char stringpool_str2761[sizeof("grey80")];
    char stringpool_str2765[sizeof("honeydew")];
    char stringpool_str2766[sizeof("gray90")];
    char stringpool_str2767[sizeof("gray60")];
    char stringpool_str2773[sizeof("gray59")];
    char stringpool_str2775[sizeof("gray56")];
    char stringpool_str2792[sizeof("grey100")];
    char stringpool_str2793[sizeof("medium purple")];
    char stringpool_str2819[sizeof("medium turquoise")];
    char stringpool_str2831[sizeof("gray88")];
    char stringpool_str2834[sizeof("green yellow")];
    char stringpool_str2841[sizeof("gray80")];
    char stringpool_str2843[sizeof("grey58")];
    char stringpool_str2853[sizeof("grey50")];
    char stringpool_str2860[sizeof("grey95")];
    char stringpool_str2861[sizeof("grey65")];
    char stringpool_str2872[sizeof("gray100")];
    char stringpool_str2895[sizeof("darkkhaki")];
    char stringpool_str2923[sizeof("gray58")];
    char stringpool_str2933[sizeof("gray50")];
    char stringpool_str2935[sizeof("grey85")];
    char stringpool_str2940[sizeof("gray95")];
    char stringpool_str2941[sizeof("gray65")];
    char stringpool_str2960[sizeof("ghost white")];
    char stringpool_str2971[sizeof("palevioletred")];
    char stringpool_str2972[sizeof("palevioletred4")];
    char stringpool_str2982[sizeof("palevioletred3")];
    char stringpool_str3012[sizeof("palevioletred2")];
    char stringpool_str3015[sizeof("gray85")];
    char stringpool_str3017[sizeof("lightpink")];
    char stringpool_str3022[sizeof("palevioletred1")];
    char stringpool_str3027[sizeof("grey55")];
    char stringpool_str3033[sizeof("navajo white")];
    char stringpool_str3044[sizeof("light yellow")];
    char stringpool_str3062[sizeof("pale turquoise")];
    char stringpool_str3099[sizeof("medium spring green")];
    char stringpool_str3107[sizeof("gray55")];
    char stringpool_str3116[sizeof("lemonchiffon4")];
    char stringpool_str3126[sizeof("lemonchiffon3")];
    char stringpool_str3143[sizeof("light pink")];
    char stringpool_str3156[sizeof("lemonchiffon2")];
    char stringpool_str3166[sizeof("lemonchiffon1")];
    char stringpool_str3167[sizeof("deep pink")];
    char stringpool_str3185[sizeof("navajowhite")];
    char stringpool_str3186[sizeof("navajowhite4")];
    char stringpool_str3196[sizeof("navajowhite3")];
    char stringpool_str3225[sizeof("peachpuff4")];
    char stringpool_str3226[sizeof("navajowhite2")];
    char stringpool_str3230[sizeof("peachpuff3")];
    char stringpool_str3236[sizeof("navajowhite1")];
    char stringpool_str3245[sizeof("peachpuff2")];
    char stringpool_str3250[sizeof("peachpuff1")];
    char stringpool_str3275[sizeof("lemonchiffon")];
    char stringpool_str3279[sizeof("peachpuff")];
    char stringpool_str3314[sizeof("lavender blush")];
    char stringpool_str3330[sizeof("medium violet red")];
    char stringpool_str3471[sizeof("hot pink")];
    char stringpool_str3745[sizeof("peach puff")];
    char stringpool_str3841[sizeof("lemon chiffon")];
    char stringpool_str4013[sizeof("pale violet red")];
    char stringpool_str5422[sizeof("papaya whip")];
    char stringpool_str5574[sizeof("papayawhip")];
};
static const struct stringpool_t stringpool_contents =
{
    "red",
    "red4",
    "grey4",
    "grey44",
    "darkred",
    "red3",
    "grey34",
    "grey3",
    "grey43",
    "grey33",
    "grey74",
    "red2",
    "grey24",
    "red1",
    "grey14",
    "grey73",
    "grey23",
    "grey7",
    "grey47",
    "grey13",
    "grey37",
    "grey2",
    "grey42",
    "grey32",
    "grey77",
    "grey1",
    "grey41",
    "grey27",
    "grey31",
    "grey72",
    "grey17",
    "grey22",
    "grey12",
    "grey71",
    "grey21",
    "grey11",
    "gray4",
    "gray44",
    "gray34",
    "gray3",
    "gray43",
    "gray33",
    "gray74",
    "snow4",
    "gray24",
    "gray14",
    "gray73",
    "snow3",
    "gray23",
    "gray7",
    "gray47",
    "gray13",
    "gray37",
    "gray2",
    "gray42",
    "gray32",
    "gray77",
    "gray1",
    "gray41",
    "gray27",
    "gray31",
    "gray72",
    "gray17",
    "snow2",
    "gray22",
    "gray12",
    "gray71",
    "snow1",
    "gray21",
    "gray11",
    "green4",
    "springgreen4",
    "green3",
    "springgreen3",
    "orange",
    "orange4",
    "orangered",
    "orangered4",
    "orangered3",
    "orange3",
    "green2",
    "orangered2",
    "springgreen2",
    "orangered1",
    "green1",
    "gold",
    "gold4",
    "springgreen1",
    "orange2",
    "gold3",
    "orange1",
    "sienna4",
    "seagreen4",
    "darkseagreen4",
    "sienna3",
    "seagreen3",
    "gold2",
    "darkseagreen3",
    "grey",
    "gold1",
    "brown4",
    "brown3",
    "sienna2",
    "seagreen2",
    "darkseagreen2",
    "sienna1",
    "seagreen1",
    "darkseagreen1",
    "brown2",
    "tan4",
    "brown1",
    "tan3",
    "green",
    "sienna",
    "darkgreen",
    "springgreen",
    "blue",
    "blue4",
    "bisque",
    "bisque4",
    "tan2",
    "tan1",
    "blue3",
    "bisque3",
    "gray",
    "darkorange",
    "darkorange4",
    "darkorange3",
    "darkorange2",
    "blue2",
    "darkorange1",
    "bisque2",
    "forestgreen",
    "blue1",
    "bisque1",
    "seagreen",
    "sea green",
    "azure",
    "azure4",
    "darkseagreen",
    "azure3",
    "darkgoldenrod",
    "darkgoldenrod4",
    "brown",
    "slategray4",
    "darkgoldenrod3",
    "slategray3",
    "azure2",
    "slategray2",
    "maroon4",
    "slategray1",
    "azure1",
    "maroon3",
    "darkgoldenrod2",
    "tan",
    "darkgoldenrod1",
    "salmon4",
    "goldenrod",
    "goldenrod4",
    "goldenrod3",
    "saddlebrown",
    "salmon3",
    "maroon2",
    "goldenrod2",
    "maroon1",
    "goldenrod1",
    "darkmagenta",
    "salmon2",
    "dark red",
    "salmon1",
    "darkgrey",
    "lightgreen",
    "darkblue",
    "dodgerblue",
    "dodgerblue4",
    "dodgerblue3",
    "mistyrose",
    "mistyrose4",
    "mistyrose3",
    "dodgerblue2",
    "dodgerblue1",
    "mistyrose2",
    "mistyrose1",
    "magenta4",
    "magenta3",
    "tomato4",
    "maroon",
    "tomato3",
    "darkgray",
    "slategrey",
    "magenta2",
    "slateblue",
    "slateblue4",
    "slateblue3",
    "salmon",
    "magenta1",
    "tomato2",
    "slateblue2",
    "tomato1",
    "slateblue1",
    "magenta",
    "beige",
    "dark green",
    "slategray",
    "linen",
    "tomato",
    "orange red",
    "dimgrey",
    "dim grey",
    "limegreen",
    "dodger blue",
    "darksalmon",
    "steelblue",
    "steelblue4",
    "steelblue3",
    "lightseagreen",
    "darkslateblue",
    "steelblue2",
    "steelblue1",
    "lightgoldenrod",
    "lightgoldenrod4",
    "dimgray",
    "dim gray",
    "darkslategray4",
    "lightgoldenrod3",
    "skyblue",
    "skyblue4",
    "sky blue",
    "darkslategray3",
    "skyblue3",
    "lightgoldenrod2",
    "grey94",
    "grey64",
    "darkslategray2",
    "lightgoldenrod1",
    "skyblue2",
    "grey93",
    "grey63",
    "darkslategray1",
    "skyblue1",
    "lightsalmon4",
    "olivedrab4",
    "dark orange",
    "olivedrab",
    "olivedrab3",
    "slate grey",
    "lightsalmon3",
    "grey97",
    "grey67",
    "black",
    "grey92",
    "grey62",
    "olivedrab2",
    "deepskyblue",
    "deepskyblue4",
    "olivedrab1",
    "grey91",
    "grey61",
    "deepskyblue3",
    "lightsalmon2",
    "lightgrey",
    "lightsalmon1",
    "grey84",
    "indianred",
    "indianred4",
    "lightblue",
    "lightblue4",
    "gray94",
    "gray64",
    "indianred3",
    "lightblue3",
    "grey83",
    "deepskyblue2",
    "snow",
    "gray93",
    "gray63",
    "deepskyblue1",
    "indianred2",
    "lightblue2",
    "indianred1",
    "dark goldenrod",
    "lightblue1",
    "grey87",
    "slate gray",
    "gray97",
    "gray67",
    "grey82",
    "dark magenta",
    "darkturquoise",
    "gray92",
    "gray62",
    "grey81",
    "gray91",
    "gray61",
    "gainsboro",
    "lightgray",
    "wheat4",
    "darkslategrey",
    "gray84",
    "wheat3",
    "violetred",
    "violetred4",
    "violetred3",
    "gray83",
    "spring green",
    "grey54",
    "grey53",
    "violetred2",
    "lightsalmon",
    "violetred1",
    "wheat2",
    "gray87",
    "royalblue",
    "royalblue4",
    "royalblue3",
    "gray82",
    "wheat1",
    "ivory4",
    "grey57",
    "lightskyblue",
    "lightskyblue4",
    "gray81",
    "grey52",
    "ivory3",
    "royalblue2",
    "lightskyblue3",
    "dark grey",
    "royalblue1",
    "light green",
    "forest green",
    "grey51",
    "cyan4",
    "saddle brown",
    "darkslategray",
    "dark blue",
    "cyan3",
    "lightyellow4",
    "lightsteelblue",
    "lightsteelblue4",
    "ivory2",
    "misty rose",
    "lightyellow3",
    "lightskyblue2",
    "lightsteelblue3",
    "gray54",
    "ivory1",
    "lime green",
    "lightskyblue1",
    "gray53",
    "cyan2",
    "rosybrown4",
    "rosybrown3",
    "cyan1",
    "lightyellow2",
    "mediumseagreen",
    "lightsteelblue2",
    "lavender",
    "gray57",
    "rosybrown2",
    "lightyellow1",
    "lightsteelblue1",
    "rosybrown1",
    "gray52",
    "dark gray",
    "gray51",
    "wheat",
    "khaki4",
    "slate blue",
    "violet",
    "khaki3",
    "light grey",
    "oldlace",
    "navy",
    "dark salmon",
    "pink4",
    "seashell4",
    "pink3",
    "khaki2",
    "seashell3",
    "coral4",
    "khaki1",
    "thistle",
    "thistle4",
    "coral3",
    "thistle3",
    "pink2",
    "sandy brown",
    "cyan",
    "seashell2",
    "pink1",
    "purple",
    "purple4",
    "seashell1",
    "coral2",
    "light gray",
    "thistle2",
    "purple3",
    "coral1",
    "thistle1",
    "mediumblue",
    "turquoise",
    "turquoise4",
    "rosybrown",
    "turquoise3",
    "purple2",
    "turquoise2",
    "purple1",
    "turquoise1",
    "steel blue",
    "light sea green",
    "aliceblue",
    "ivory",
    "burlywood",
    "burlywood4",
    "burlywood3",
    "burlywood2",
    "burlywood1",
    "peru",
    "plum4",
    "plum3",
    "lightslategrey",
    "lightslateblue",
    "lawngreen",
    "old lace",
    "plum2",
    "olive drab",
    "plum1",
    "palegreen4",
    "medium sea green",
    "seashell",
    "palegreen3",
    "coral",
    "yellow4",
    "palegreen2",
    "palegreen1",
    "yellow3",
    "indian red",
    "lightslategray",
    "light blue",
    "navyblue",
    "dark sea green",
    "medium blue",
    "yellow2",
    "yellow1",
    "light goldenrod",
    "white",
    "dark slate grey",
    "light salmon",
    "aquamarine",
    "aquamarine4",
    "aquamarine3",
    "violet red",
    "dark slate blue",
    "sandybrown",
    "plum",
    "aquamarine2",
    "aquamarine1",
    "chartreuse",
    "chartreuse4",
    "royal blue",
    "chartreuse3",
    "palegreen",
    "mediumslateblue",
    "chartreuse2",
    "pink",
    "chartreuse1",
    "yellowgreen",
    "dark slate gray",
    "rosy brown",
    "chocolate",
    "chocolate4",
    "chocolate3",
    "darkcyan",
    "palegoldenrod",
    "chocolate2",
    "chocolate1",
    "grey9",
    "grey49",
    "grey6",
    "grey46",
    "cadetblue",
    "cadetblue4",
    "grey39",
    "grey36",
    "cadetblue3",
    "greenyellow",
    "grey79",
    "grey76",
    "midnightblue",
    "grey29",
    "grey26",
    "cadetblue2",
    "grey19",
    "grey16",
    "cadetblue1",
    "lawn green",
    "lightcoral",
    "orchid",
    "orchid4",
    "gray9",
    "gray49",
    "gray6",
    "gray46",
    "orchid3",
    "gray39",
    "gray36",
    "gray79",
    "gray76",
    "gray29",
    "gray26",
    "gray19",
    "gray16",
    "mediumorchid",
    "mediumorchid4",
    "mintcream",
    "orchid2",
    "lavenderblush4",
    "mediumorchid3",
    "orchid1",
    "lavenderblush3",
    "khaki",
    "alice blue",
    "dark turquoise",
    "grey8",
    "grey48",
    "mediumorchid2",
    "grey38",
    "lavenderblush2",
    "grey0",
    "grey40",
    "mediumorchid1",
    "grey30",
    "grey78",
    "lavenderblush1",
    "grey28",
    "grey18",
    "grey70",
    "lightcyan4",
    "grey20",
    "lightcyan3",
    "grey10",
    "lightcyan2",
    "lightcyan1",
    "darkviolet",
    "mediumspringgreen",
    "lightgoldenrodyellow",
    "darkolivegreen4",
    "lightyellow",
    "darkolivegreen3",
    "gray8",
    "gray48",
    "pale green",
    "gray38",
    "gray0",
    "gray40",
    "gray30",
    "gray78",
    "honeydew4",
    "gray28",
    "gray18",
    "gray70",
    "darkolivegreen2",
    "honeydew3",
    "gray20",
    "gray10",
    "darkolivegreen1",
    "mediumturquoise",
    "navy blue",
    "honeydew2",
    "honeydew1",
    "light slate grey",
    "medium orchid",
    "grey5",
    "grey45",
    "lightcyan",
    "grey35",
    "grey75",
    "deep sky blue",
    "grey25",
    "grey15",
    "mediumpurple",
    "mediumpurple4",
    "hotpink4",
    "darkolivegreen",
    "light slate gray",
    "mediumpurple3",
    "hotpink3",
    "blanchedalmond",
    "mediumpurple2",
    "gray5",
    "gray45",
    "hotpink2",
    "light sky blue",
    "gray35",
    "mediumpurple1",
    "hotpink1",
    "gray75",
    "firebrick4",
    "gray25",
    "firebrick3",
    "gray15",
    "light steel blue",
    "mint cream",
    "firebrick2",
    "firebrick1",
    "dark cyan",
    "pale goldenrod",
    "mediumaquamarine",
    "paleturquoise",
    "paleturquoise4",
    "light coral",
    "medium slate blue",
    "whitesmoke",
    "paleturquoise3",
    "cadet blue",
    "antiquewhite",
    "antiquewhite4",
    "blueviolet",
    "antique white",
    "antiquewhite3",
    "paleturquoise2",
    "paleturquoise1",
    "yellow",
    "moccasin",
    "deeppink4",
    "antiquewhite2",
    "deeppink3",
    "yellow green",
    "light slate blue",
    "antiquewhite1",
    "cornsilk4",
    "dark orchid",
    "cornsilk3",
    "deeppink2",
    "deeppink1",
    "cornsilk2",
    "light goldenrod yellow",
    "cornsilk1",
    "white smoke",
    "light cyan",
    "mediumvioletred",
    "powderblue",
    "medium aquamarine",
    "dark violet",
    "dark olive green",
    "darkorchid",
    "darkorchid4",
    "darkorchid3",
    "darkorchid2",
    "hotpink",
    "darkorchid1",
    "lavenderblush",
    "floral white",
    "firebrick",
    "grey99",
    "grey69",
    "grey96",
    "grey66",
    "midnight blue",
    "ghostwhite",
    "powder blue",
    "grey89",
    "grey86",
    "gray99",
    "gray69",
    "gray96",
    "gray66",
    "lightpink4",
    "lightpink3",
    "floralwhite",
    "lightpink2",
    "deeppink",
    "lightpink1",
    "cornsilk",
    "grey98",
    "grey68",
    "gray89",
    "gray86",
    "grey90",
    "grey60",
    "grey59",
    "grey56",
    "blanched almond",
    "cornflowerblue",
    "cornflower blue",
    "dark khaki",
    "blue violet",
    "grey88",
    "gray98",
    "gray68",
    "grey80",
    "honeydew",
    "gray90",
    "gray60",
    "gray59",
    "gray56",
    "grey100",
    "medium purple",
    "medium turquoise",
    "gray88",
    "green yellow",
    "gray80",
    "grey58",
    "grey50",
    "grey95",
    "grey65",
    "gray100",
    "darkkhaki",
    "gray58",
    "gray50",
    "grey85",
    "gray95",
    "gray65",
    "ghost white",
    "palevioletred",
    "palevioletred4",
    "palevioletred3",
    "palevioletred2",
    "gray85",
    "lightpink",
    "palevioletred1",
    "grey55",
    "navajo white",
    "light yellow",
    "pale turquoise",
    "medium spring green",
    "gray55",
    "lemonchiffon4",
    "lemonchiffon3",
    "light pink",
    "lemonchiffon2",
    "lemonchiffon1",
    "deep pink",
    "navajowhite",
    "navajowhite4",
    "navajowhite3",
    "peachpuff4",
    "navajowhite2",
    "peachpuff3",
    "navajowhite1",
    "peachpuff2",
    "peachpuff1",
    "lemonchiffon",
    "peachpuff",
    "lavender blush",
    "medium violet red",
    "hot pink",
    "peach puff",
    "lemon chiffon",
    "pale violet red",
    "papaya whip",
    "papayawhip"
};
#define stringpool ((const char *) &stringpool_contents)
const struct color *
    =lookup_rgb (str, len)
     register const char *str;
register unsigned int len;
{
    static const struct color wordlist[] =
    {
        {-1}, {-1}, {-1},
#line 205 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str3), 0xff, 0x00, 0x00},
#line 487 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str4), 0x8b, 0x00, 0x00},
#line 557 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str5), 0x0a, 0x0a, 0x0a},
#line 637 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str6), 0x70, 0x70, 0x70},
#line 761 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str7), 0x8b, 0x00, 0x00},
        {-1},
#line 486 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str9), 0xcd, 0x00, 0x00},
        {-1},
#line 617 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str11), 0x57, 0x57, 0x57},
        {-1}, {-1}, {-1},
#line 555 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str15), 0x08, 0x08, 0x08},
#line 635 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str16), 0x6e, 0x6e, 0x6e},
        {-1}, {-1}, {-1}, {-1},
#line 615 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str21), 0x54, 0x54, 0x54},
#line 697 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str22), 0xbd, 0xbd, 0xbd},
        {-1},
#line 485 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str24), 0xee, 0x00, 0x00},
        {-1},
#line 597 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str26), 0x3d, 0x3d, 0x3d},
        {-1}, {-1},
#line 484 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str29), 0xff, 0x00, 0x00},
        {-1},
#line 577 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str31), 0x24, 0x24, 0x24},
#line 695 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str32), 0xba, 0xba, 0xba},
        {-1}, {-1}, {-1},
#line 595 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str36), 0x3b, 0x3b, 0x3b},
#line 563 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str37), 0x12, 0x12, 0x12},
#line 643 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str38), 0x78, 0x78, 0x78},
        {-1}, {-1},
#line 575 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str41), 0x21, 0x21, 0x21},
        {-1},
#line 623 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str43), 0x5e, 0x5e, 0x5e},
        {-1},
#line 553 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str45), 0x05, 0x05, 0x05},
#line 633 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str46), 0x6b, 0x6b, 0x6b},
        {-1}, {-1}, {-1}, {-1},
#line 613 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str51), 0x52, 0x52, 0x52},
        {-1}, {-1},
#line 703 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str54), 0xc4, 0xc4, 0xc4},
#line 551 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str55), 0x03, 0x03, 0x03},
#line 631 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str56), 0x69, 0x69, 0x69},
        {-1},
#line 603 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str58), 0x45, 0x45, 0x45},
        {-1}, {-1},
#line 611 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str61), 0x4f, 0x4f, 0x4f},
#line 693 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str62), 0xb8, 0xb8, 0xb8},
#line 583 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str63), 0x2b, 0x2b, 0x2b},
        {-1}, {-1},
#line 593 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str66), 0x38, 0x38, 0x38},
        {-1}, {-1}, {-1}, {-1},
#line 573 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str71), 0x1f, 0x1f, 0x1f},
#line 691 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str72), 0xb5, 0xb5, 0xb5},
        {-1}, {-1}, {-1},
#line 591 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str76), 0x36, 0x36, 0x36},
        {-1}, {-1}, {-1}, {-1},
#line 571 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str81), 0x1c, 0x1c, 0x1c},
        {-1}, {-1}, {-1},
#line 556 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str85), 0x0a, 0x0a, 0x0a},
#line 636 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str86), 0x70, 0x70, 0x70},
        {-1}, {-1}, {-1}, {-1},
#line 616 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str91), 0x57, 0x57, 0x57},
        {-1}, {-1}, {-1},
#line 554 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str95), 0x08, 0x08, 0x08},
#line 634 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str96), 0x6e, 0x6e, 0x6e},
        {-1}, {-1}, {-1}, {-1},
#line 614 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str101), 0x54, 0x54, 0x54},
#line 696 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str102), 0xbd, 0xbd, 0xbd},
        {-1}, {-1},
#line 239 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str105), 0x8b, 0x89, 0x89},
#line 596 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str106), 0x3d, 0x3d, 0x3d},
        {-1}, {-1}, {-1}, {-1},
#line 576 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str111), 0x24, 0x24, 0x24},
#line 694 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str112), 0xba, 0xba, 0xba},
        {-1}, {-1},
#line 238 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str115), 0xcd, 0xc9, 0xc9},
#line 594 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str116), 0x3b, 0x3b, 0x3b},
#line 562 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str117), 0x12, 0x12, 0x12},
#line 642 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str118), 0x78, 0x78, 0x78},
        {-1}, {-1},
#line 574 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str121), 0x21, 0x21, 0x21},
        {-1},
#line 622 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str123), 0x5e, 0x5e, 0x5e},
        {-1},
#line 552 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str125), 0x05, 0x05, 0x05},
#line 632 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str126), 0x6b, 0x6b, 0x6b},
        {-1}, {-1}, {-1}, {-1},
#line 612 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str131), 0x52, 0x52, 0x52},
        {-1}, {-1},
#line 702 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str134), 0xc4, 0xc4, 0xc4},
#line 550 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str135), 0x03, 0x03, 0x03},
#line 630 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str136), 0x69, 0x69, 0x69},
        {-1},
#line 602 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str138), 0x45, 0x45, 0x45},
        {-1}, {-1},
#line 610 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str141), 0x4f, 0x4f, 0x4f},
#line 692 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str142), 0xb8, 0xb8, 0xb8},
#line 582 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str143), 0x2b, 0x2b, 0x2b},
        {-1},
#line 237 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str145), 0xee, 0xe9, 0xe9},
#line 592 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str146), 0x38, 0x38, 0x38},
        {-1}, {-1}, {-1}, {-1},
#line 572 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str151), 0x1f, 0x1f, 0x1f},
#line 690 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str152), 0xb5, 0xb5, 0xb5},
        {-1}, {-1},
#line 236 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str155), 0xff, 0xfa, 0xfa},
#line 590 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str156), 0x36, 0x36, 0x36},
        {-1}, {-1}, {-1}, {-1},
#line 570 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str161), 0x1c, 0x1c, 0x1c},
        {-1}, {-1}, {-1}, {-1},
#line 379 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str166), 0x00, 0x8b, 0x00},
        {-1}, {-1}, {-1}, {-1}, {-1},
#line 375 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str172), 0x00, 0x8b, 0x45},
        {-1}, {-1}, {-1},
#line 378 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str176), 0x00, 0xcd, 0x00},
        {-1}, {-1}, {-1}, {-1}, {-1},
#line 374 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str182), 0x00, 0xcd, 0x66},
        {-1}, {-1}, {-1},
#line 196 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str186), 0xff, 0xa5, 0x00},
#line 467 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str187), 0x8b, 0x5a, 0x00},
        {-1},
#line 204 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str189), 0xff, 0x45, 0x00},
#line 483 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str190), 0x8b, 0x25, 0x00},
        {-1}, {-1}, {-1}, {-1},
#line 482 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str195), 0xcd, 0x37, 0x00},
        {-1},
#line 466 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str197), 0xcd, 0x85, 0x00},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 377 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str206), 0x00, 0xee, 0x00},
        {-1}, {-1}, {-1},
#line 481 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str210), 0xee, 0x40, 0x00},
        {-1},
#line 373 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str212), 0x00, 0xee, 0x76},
        {-1}, {-1},
#line 480 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str215), 0xff, 0x45, 0x00},
#line 376 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str216), 0x00, 0xff, 0x00},
        {-1}, {-1},
#line 168 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str219), 0xff, 0xd7, 0x00},
#line 411 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str220), 0x8b, 0x75, 0x00},
        {-1},
#line 372 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str222), 0x00, 0xff, 0x7f},
        {-1}, {-1}, {-1}, {-1},
#line 465 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str227), 0xee, 0x9a, 0x00},
        {-1}, {-1},
#line 410 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str230), 0xcd, 0xad, 0x00},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 464 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str237), 0xff, 0xa5, 0x00},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 431 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str247), 0x8b, 0x47, 0x26},
        {-1},
#line 367 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str249), 0x2e, 0x8b, 0x57},
        {-1}, {-1}, {-1},
#line 363 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str253), 0x69, 0x8b, 0x69},
        {-1}, {-1}, {-1},
#line 430 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str257), 0xcd, 0x68, 0x39},
        {-1},
#line 366 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str259), 0x43, 0xcd, 0x80},
#line 409 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str260), 0xee, 0xc9, 0x00},
        {-1}, {-1},
#line 362 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str263), 0x9b, 0xcd, 0x9b},
        {-1}, {-1}, {-1}, {-1}, {-1},
#line 70 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str269), 0xbe, 0xbe, 0xbe},
#line 408 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str270), 0xff, 0xd7, 0x00},
#line 455 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str271), 0x8b, 0x23, 0x23},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 454 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str281), 0xcd, 0x33, 0x33},
        {-1}, {-1}, {-1}, {-1}, {-1},
#line 429 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str287), 0xee, 0x79, 0x42},
        {-1},
#line 365 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str289), 0x4e, 0xee, 0x94},
        {-1}, {-1}, {-1},
#line 361 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str293), 0xb4, 0xee, 0xb4},
        {-1}, {-1}, {-1},
#line 428 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str297), 0xff, 0x82, 0x47},
        {-1},
#line 364 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str299), 0x54, 0xff, 0x9f},
        {-1}, {-1}, {-1},
#line 360 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str303), 0xc1, 0xff, 0xc1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 453 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str311), 0xee, 0x3b, 0x3b},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 443 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str319), 0x8b, 0x5a, 0x2b},
        {-1},
#line 452 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str321), 0xff, 0x40, 0x40},
        {-1}, {-1},
#line 442 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str324), 0xcd, 0x85, 0x3f},
#line 144 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str325), 0x00, 0xff, 0x00},
#line 180 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str326), 0xa0, 0x52, 0x2d},
        {-1}, {-1},
#line 127 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str329), 0x00, 0x64, 0x00},
        {-1},
#line 141 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str331), 0x00, 0xff, 0x7f},
        {-1}, {-1},
#line 94 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str334), 0x00, 0x00, 0xff},
#line 299 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str335), 0x00, 0x00, 0x8b},
#line 29 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str336), 0xff, 0xe4, 0xc4},
#line 251 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str337), 0x8b, 0x7d, 0x6b},
        {-1},
#line 441 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str339), 0xee, 0x9a, 0x49},
        {-1}, {-1}, {-1}, {-1},
#line 440 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str344), 0xff, 0xa5, 0x4f},
#line 298 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str345), 0x00, 0x00, 0xcd},
        {-1},
#line 250 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str347), 0xcd, 0xb7, 0x9e},
        {-1},
#line 69 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str349), 0xbe, 0xbe, 0xbe},
#line 198 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str350), 0xff, 0x8c, 0x00},
#line 471 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str351), 0x8b, 0x45, 0x00},
        {-1}, {-1}, {-1}, {-1},
#line 470 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str356), 0xcd, 0x66, 0x00},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1},
#line 469 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str371), 0xee, 0x76, 0x00},
        {-1}, {-1}, {-1},
#line 297 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str375), 0x00, 0x00, 0xee},
#line 468 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str376), 0xff, 0x7f, 0x00},
#line 249 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str377), 0xee, 0xd5, 0xb7},
        {-1}, {-1}, {-1},
#line 155 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str381), 0x22, 0x8b, 0x22},
        {-1}, {-1}, {-1},
#line 296 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str385), 0x00, 0x00, 0xff},
        {-1},
#line 248 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str387), 0xff, 0xe4, 0xc4},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1},
#line 133 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str408), 0x2e, 0x8b, 0x57},
#line 132 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str409), 0x2e, 0x8b, 0x57},
#line 43 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str410), 0xf0, 0xff, 0xff},
#line 287 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str411), 0x83, 0x8b, 0x8b},
#line 131 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str412), 0x8f, 0xbc, 0x8f},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 286 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str421), 0xc1, 0xcd, 0xcd},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 173 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str428), 0xb8, 0x86, 0x0b},
#line 419 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str429), 0x8b, 0x65, 0x08},
#line 190 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str430), 0xa5, 0x2a, 0x2a},
        {-1}, {-1}, {-1}, {-1},
#line 323 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str435), 0x6c, 0x7b, 0x8b},
        {-1}, {-1}, {-1},
#line 418 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str439), 0xcd, 0x95, 0x0c},
#line 322 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str440), 0x9f, 0xb6, 0xcd},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1},
#line 285 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str451), 0xe0, 0xee, 0xee},
        {-1}, {-1}, {-1},
#line 321 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str455), 0xb9, 0xd3, 0xee},
        {-1},
#line 511 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str457), 0x8b, 0x1c, 0x62},
        {-1}, {-1},
#line 320 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str460), 0xc6, 0xe2, 0xff},
#line 284 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str461), 0xf0, 0xff, 0xff},
        {-1}, {-1}, {-1}, {-1}, {-1},
#line 510 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str467), 0xcd, 0x29, 0x90},
        {-1},
#line 417 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str469), 0xee, 0xad, 0x0e},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 187 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str478), 0xd2, 0xb4, 0x8c},
#line 416 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str479), 0xff, 0xb9, 0x0f},
        {-1}, {-1},
#line 459 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str482), 0x8b, 0x4c, 0x39},
        {-1},
#line 171 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str484), 0xda, 0xa5, 0x20},
#line 415 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str485), 0x8b, 0x69, 0x14},
        {-1}, {-1}, {-1}, {-1},
#line 414 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str490), 0xcd, 0x9b, 0x1d},
#line 179 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str491), 0x8b, 0x45, 0x13},
#line 458 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str492), 0xcd, 0x70, 0x54},
        {-1}, {-1}, {-1}, {-1},
#line 509 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str497), 0xee, 0x30, 0xa7},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 413 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str505), 0xee, 0xb4, 0x22},
        {-1},
#line 508 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str507), 0xff, 0x34, 0xb3},
        {-1}, {-1},
#line 412 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str510), 0xff, 0xc1, 0x25},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1},
#line 759 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str521), 0x8b, 0x00, 0x8b},
#line 457 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str522), 0xee, 0x82, 0x62},
        {-1}, {-1}, {-1}, {-1}, {-1},
#line 760 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str528), 0x8b, 0x00, 0x00},
        {-1}, {-1}, {-1},
#line 456 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str532), 0xff, 0x8c, 0x69},
        {-1}, {-1}, {-1}, {-1}, {-1},
#line 751 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str538), 0xa9, 0xa9, 0xa9},
        {-1},
#line 763 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str540), 0x90, 0xee, 0x90},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1},
#line 755 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str553), 0x00, 0x00, 0x8b},
        {-1},
#line 96 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str555), 0x1e, 0x90, 0xff},
#line 303 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str556), 0x10, 0x4e, 0x8b},
        {-1}, {-1}, {-1}, {-1},
#line 302 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str561), 0x18, 0x74, 0xcd},
        {-1}, {-1},
#line 50 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str564), 0xff, 0xe4, 0xe1},
#line 283 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str565), 0x8b, 0x7d, 0x7b},
        {-1}, {-1}, {-1}, {-1},
#line 282 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str570), 0xcd, 0xb7, 0xb5},
        {-1}, {-1}, {-1}, {-1}, {-1},
#line 301 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str576), 0x1c, 0x86, 0xee},
        {-1}, {-1}, {-1}, {-1},
#line 300 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str581), 0x1e, 0x90, 0xff},
        {-1}, {-1}, {-1},
#line 281 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str585), 0xee, 0xd5, 0xd2},
        {-1}, {-1}, {-1}, {-1},
#line 280 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str590), 0xff, 0xe4, 0xe1},
        {-1}, {-1},
#line 519 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str593), 0x8b, 0x00, 0x8b},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 518 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str603), 0xcd, 0x00, 0xcd},
        {-1}, {-1}, {-1},
#line 479 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str607), 0x8b, 0x36, 0x26},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 215 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str616), 0xb0, 0x30, 0x60},
#line 478 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str617), 0xcd, 0x4f, 0x39},
#line 753 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str618), 0xa9, 0xa9, 0xa9},
#line 64 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str619), 0x70, 0x80, 0x90},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1},
#line 517 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str633), 0xee, 0x00, 0xee},
#line 85 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str634), 0x6a, 0x5a, 0xcd},
#line 291 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str635), 0x47, 0x3c, 0x8b},
        {-1}, {-1}, {-1}, {-1},
#line 290 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str640), 0x69, 0x59, 0xcd},
#line 193 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str641), 0xfa, 0x80, 0x72},
        {-1},
#line 516 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str643), 0xff, 0x00, 0xff},
        {-1}, {-1}, {-1},
#line 477 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str647), 0xee, 0x5c, 0x42},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 289 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str655), 0x7a, 0x67, 0xee},
        {-1},
#line 476 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str657), 0xff, 0x63, 0x47},
        {-1}, {-1},
#line 288 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str660), 0x83, 0x6f, 0xff},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1},
#line 220 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str672), 0xff, 0x00, 0xff},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1},
#line 183 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str683), 0xf5, 0xf5, 0xdc},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 126 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str690), 0x00, 0x64, 0x00},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 62 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str699), 0x70, 0x80, 0x90},
#line 22 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str700), 0xfa, 0xf0, 0xe6},
        {-1}, {-1}, {-1}, {-1}, {-1},
#line 202 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str706), 0xff, 0x63, 0x47},
        {-1}, {-1}, {-1},
#line 203 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str710), 0xff, 0x45, 0x00},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 60 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str727), 0x69, 0x69, 0x69},
#line 59 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str728), 0x69, 0x69, 0x69},
        {-1}, {-1}, {-1}, {-1}, {-1},
#line 151 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str734), 0x32, 0xcd, 0x32},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 95 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str751), 0x1e, 0x90, 0xff},
        {-1}, {-1}, {-1},
#line 192 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str755), 0xe9, 0x96, 0x7a},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1},
#line 104 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str769), 0x46, 0x82, 0xb4},
#line 307 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str770), 0x36, 0x64, 0x8b},
        {-1}, {-1}, {-1}, {-1},
#line 306 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str775), 0x4f, 0x94, 0xcd},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 137 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str783), 0x20, 0xb2, 0xaa},
        {-1}, {-1}, {-1}, {-1},
#line 83 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str788), 0x48, 0x3d, 0x8b},
        {-1},
#line 305 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str790), 0x5c, 0xac, 0xee},
        {-1}, {-1}, {-1}, {-1},
#line 304 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str795), 0x63, 0xb8, 0xff},
        {-1}, {-1}, {-1},
#line 170 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str799), 0xee, 0xdd, 0x82},
#line 399 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str800), 0x8b, 0x81, 0x4c},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 58 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str807), 0x69, 0x69, 0x69},
#line 57 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str808), 0x69, 0x69, 0x69},
#line 355 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str809), 0x52, 0x8b, 0x8b},
#line 398 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str810), 0xcd, 0xbe, 0x70},
        {-1},
#line 100 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str812), 0x87, 0xce, 0xeb},
#line 315 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str813), 0x4a, 0x70, 0x8b},
        {-1}, {-1}, {-1}, {-1},
#line 99 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str818), 0x87, 0xce, 0xeb},
#line 354 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str819), 0x79, 0xcd, 0xcd},
        {-1}, {-1}, {-1},
#line 314 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str823), 0x6c, 0xa6, 0xcd},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 397 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str840), 0xee, 0xdc, 0x82},
        {-1}, {-1}, {-1}, {-1}, {-1},
#line 737 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str846), 0xf0, 0xf0, 0xf0},
#line 677 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str847), 0xa3, 0xa3, 0xa3},
        {-1},
#line 353 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str849), 0x8d, 0xee, 0xee},
#line 396 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str850), 0xff, 0xec, 0x8b},
        {-1}, {-1},
#line 313 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str853), 0x7e, 0xc0, 0xee},
        {-1}, {-1},
#line 735 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str856), 0xed, 0xed, 0xed},
#line 675 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str857), 0xa1, 0xa1, 0xa1},
        {-1},
#line 352 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str859), 0x97, 0xff, 0xff},
        {-1}, {-1}, {-1},
#line 312 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str863), 0x87, 0xce, 0xff},
        {-1}, {-1}, {-1},
#line 463 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str867), 0x8b, 0x57, 0x42},
#line 387 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str868), 0x69, 0x8b, 0x22},
        {-1}, {-1},
#line 197 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str871), 0xff, 0x8c, 0x00},
#line 157 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str872), 0x6b, 0x8e, 0x23},
#line 386 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str873), 0x9a, 0xcd, 0x32},
        {-1},
#line 63 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str875), 0x70, 0x80, 0x90},
        {-1},
#line 462 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str877), 0xcd, 0x81, 0x62},
#line 743 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str878), 0xf7, 0xf7, 0xf7},
#line 683 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str879), 0xab, 0xab, 0xab},
#line 52 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str880), 0x00, 0x00, 0x00},
        {-1}, {-1}, {-1}, {-1}, {-1},
#line 733 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str886), 0xeb, 0xeb, 0xeb},
#line 673 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str887), 0x9e, 0x9e, 0x9e},
#line 385 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str888), 0xb3, 0xee, 0x3a},
        {-1}, {-1},
#line 98 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str891), 0x00, 0xbf, 0xff},
#line 311 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str892), 0x00, 0x68, 0x8b},
#line 384 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str893), 0xc0, 0xff, 0x3e},
        {-1}, {-1},
#line 731 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str896), 0xe8, 0xe8, 0xe8},
#line 671 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str897), 0x9c, 0x9c, 0x9c},
        {-1}, {-1}, {-1}, {-1},
#line 310 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str902), 0x00, 0x9a, 0xcd},
        {-1}, {-1}, {-1}, {-1},
#line 461 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str907), 0xee, 0x95, 0x72},
        {-1},
#line 72 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str909), 0xd3, 0xd3, 0xd3},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 460 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str917), 0xff, 0xa0, 0x7a},
        {-1}, {-1}, {-1},
#line 717 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str921), 0xd6, 0xd6, 0xd6},
#line 177 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str922), 0xcd, 0x5c, 0x5c},
#line 427 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str923), 0x8b, 0x3a, 0x3a},
#line 108 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str924), 0xad, 0xd8, 0xe6},
#line 331 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str925), 0x68, 0x83, 0x8b},
#line 736 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str926), 0xf0, 0xf0, 0xf0},
#line 676 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str927), 0xa3, 0xa3, 0xa3},
#line 426 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str928), 0xcd, 0x55, 0x55},
        {-1},
#line 330 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str930), 0x9a, 0xc0, 0xcd},
#line 715 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str931), 0xd4, 0xd4, 0xd4},
#line 309 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str932), 0x00, 0xb2, 0xee},
        {-1}, {-1},
#line 12 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str935), 0xff, 0xfa, 0xfa},
#line 734 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str936), 0xed, 0xed, 0xed},
#line 674 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str937), 0xa1, 0xa1, 0xa1},
        {-1}, {-1}, {-1}, {-1},
#line 308 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str942), 0x00, 0xbf, 0xff},
#line 425 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str943), 0xee, 0x63, 0x63},
        {-1},
#line 329 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str945), 0xb2, 0xdf, 0xee},
        {-1}, {-1},
#line 424 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str948), 0xff, 0x6a, 0x6a},
#line 172 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str949), 0xb8, 0x86, 0x0b},
#line 328 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str950), 0xbf, 0xef, 0xff},
        {-1}, {-1},
#line 723 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str953), 0xde, 0xde, 0xde},
        {-1},
#line 61 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str955), 0x70, 0x80, 0x90},
        {-1}, {-1},
#line 742 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str958), 0xf7, 0xf7, 0xf7},
#line 682 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str959), 0xab, 0xab, 0xab},
        {-1},
#line 713 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str961), 0xd1, 0xd1, 0xd1},
#line 758 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str962), 0x8b, 0x00, 0x8b},
#line 114 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str963), 0x00, 0xce, 0xd1},
        {-1}, {-1},
#line 732 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str966), 0xeb, 0xeb, 0xeb},
#line 672 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str967), 0x9e, 0x9e, 0x9e},
        {-1}, {-1}, {-1},
#line 711 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str971), 0xcf, 0xcf, 0xcf},
        {-1}, {-1}, {-1}, {-1},
#line 730 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str976), 0xe8, 0xe8, 0xe8},
#line 670 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str977), 0x9c, 0x9c, 0x9c},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 17 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str987), 0xdc, 0xdc, 0xdc},
        {-1},
#line 74 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str989), 0xd3, 0xd3, 0xd3},
        {-1}, {-1},
#line 439 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str992), 0x8b, 0x7e, 0x66},
#line 56 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str993), 0x2f, 0x4f, 0x4f},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 716 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1001), 0xd6, 0xd6, 0xd6},
#line 438 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1002), 0xcd, 0xba, 0x96},
        {-1},
#line 219 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1004), 0xd0, 0x20, 0x90},
#line 515 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1005), 0x8b, 0x22, 0x52},
        {-1}, {-1}, {-1}, {-1},
#line 514 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1010), 0xcd, 0x32, 0x78},
#line 714 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1011), 0xd4, 0xd4, 0xd4},
#line 140 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1012), 0x00, 0xff, 0x7f},
#line 657 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1013), 0x8a, 0x8a, 0x8a},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 655 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1023), 0x87, 0x87, 0x87},
        {-1},
#line 513 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1025), 0xee, 0x3a, 0x8c},
#line 195 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1026), 0xff, 0xa0, 0x7a},
        {-1}, {-1}, {-1},
#line 512 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1030), 0xff, 0x3e, 0x96},
        {-1},
#line 437 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1032), 0xee, 0xd8, 0xae},
#line 722 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1033), 0xde, 0xde, 0xde},
#line 93 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1034), 0x41, 0x69, 0xe1},
#line 295 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1035), 0x27, 0x40, 0x8b},
        {-1}, {-1}, {-1}, {-1},
#line 294 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1040), 0x3a, 0x5f, 0xcd},
#line 712 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1041), 0xd1, 0xd1, 0xd1},
#line 436 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1042), 0xff, 0xe7, 0xba},
        {-1},
#line 271 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1044), 0x8b, 0x8b, 0x83},
#line 663 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1045), 0x91, 0x91, 0x91},
        {-1},
#line 102 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1047), 0x87, 0xce, 0xfa},
#line 319 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1048), 0x60, 0x7b, 0x8b},
        {-1}, {-1},
#line 710 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1051), 0xcf, 0xcf, 0xcf},
        {-1},
#line 653 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1053), 0x85, 0x85, 0x85},
#line 270 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1054), 0xcd, 0xcd, 0xc1},
#line 293 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1055), 0x43, 0x6e, 0xee},
        {-1}, {-1},
#line 318 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1058), 0x8d, 0xb6, 0xcd},
#line 750 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1059), 0xa9, 0xa9, 0xa9},
#line 292 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1060), 0x48, 0x76, 0xff},
#line 762 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1061), 0x90, 0xee, 0x90},
#line 154 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1062), 0x22, 0x8b, 0x22},
#line 651 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1063), 0x82, 0x82, 0x82},
        {-1},
#line 351 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1065), 0x00, 0x8b, 0x8b},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 178 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1072), 0x8b, 0x45, 0x13},
#line 54 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1073), 0x2f, 0x4f, 0x4f},
#line 754 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1074), 0x00, 0x00, 0x8b},
#line 350 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1075), 0x00, 0xcd, 0xcd},
        {-1},
#line 403 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1077), 0x8b, 0x8b, 0x7a},
        {-1},
#line 106 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1079), 0xb0, 0xc4, 0xde},
#line 327 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1080), 0x6e, 0x7b, 0x8b},
        {-1}, {-1}, {-1},
#line 269 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1084), 0xee, 0xee, 0xe0},
#line 49 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1085), 0xff, 0xe4, 0xe1},
        {-1},
#line 402 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1087), 0xcd, 0xcd, 0xb4},
#line 317 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1088), 0xa4, 0xd3, 0xee},
        {-1},
#line 326 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1090), 0xa2, 0xb5, 0xcd},
        {-1}, {-1},
#line 656 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1093), 0x8a, 0x8a, 0x8a},
#line 268 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1094), 0xff, 0xff, 0xf0},
#line 150 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1095), 0x32, 0xcd, 0x32},
        {-1}, {-1},
#line 316 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1098), 0xb0, 0xe2, 0xff},
        {-1}, {-1}, {-1}, {-1},
#line 654 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1103), 0x87, 0x87, 0x87},
        {-1},
#line 349 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1105), 0x00, 0xee, 0xee},
#line 423 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1106), 0x8b, 0x69, 0x69},
        {-1}, {-1}, {-1}, {-1},
#line 422 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1111), 0xcd, 0x9b, 0x9b},
        {-1}, {-1}, {-1},
#line 348 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1115), 0x00, 0xff, 0xff},
        {-1},
#line 401 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1117), 0xee, 0xee, 0xd1},
        {-1},
#line 135 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1119), 0x3c, 0xb3, 0x71},
#line 325 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1120), 0xbc, 0xd2, 0xee},
        {-1}, {-1},
#line 46 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1123), 0xe6, 0xe6, 0xfa},
        {-1},
#line 662 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1125), 0x91, 0x91, 0x91},
#line 421 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1126), 0xee, 0xb4, 0xb4},
#line 400 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1127), 0xff, 0xff, 0xe0},
        {-1}, {-1},
#line 324 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1130), 0xca, 0xe1, 0xff},
#line 420 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1131), 0xff, 0xc1, 0xc1},
        {-1},
#line 652 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1133), 0x85, 0x85, 0x85},
        {-1}, {-1}, {-1}, {-1}, {-1},
#line 752 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1139), 0xa9, 0xa9, 0xa9},
        {-1}, {-1}, {-1},
#line 650 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1143), 0x82, 0x82, 0x82},
        {-1}, {-1},
#line 184 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1146), 0xf5, 0xde, 0xb3},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 395 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1154), 0x8b, 0x86, 0x4e},
#line 84 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1155), 0x6a, 0x5a, 0xcd},
#line 221 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1156), 0xee, 0x82, 0xee},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 394 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1164), 0xcd, 0xc6, 0x73},
#line 71 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1165), 0xd3, 0xd3, 0xd3},
        {-1},
#line 21 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1167), 0xfd, 0xf5, 0xe6},
        {-1},
#line 77 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1169), 0x00, 0x00, 0x80},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 191 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1176), 0xe9, 0x96, 0x7a},
        {-1}, {-1}, {-1},
#line 499 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1180), 0x8b, 0x63, 0x6c},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 243 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1189), 0x8b, 0x86, 0x82},
#line 498 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1190), 0xcd, 0x91, 0x9e},
        {-1}, {-1}, {-1},
#line 393 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1194), 0xee, 0xe6, 0x85},
        {-1}, {-1}, {-1}, {-1},
#line 242 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1199), 0xcd, 0xc5, 0xbf},
        {-1},
#line 475 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1201), 0x8b, 0x3e, 0x2f},
        {-1}, {-1},
#line 392 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1204), 0xff, 0xf6, 0x8f},
#line 235 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1205), 0xd8, 0xbf, 0xd8},
#line 547 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1206), 0x8b, 0x7b, 0x8b},
        {-1}, {-1}, {-1}, {-1},
#line 474 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1211), 0xcd, 0x5b, 0x45},
        {-1}, {-1}, {-1}, {-1},
#line 546 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1216), 0xcd, 0xb5, 0xcd},
        {-1}, {-1}, {-1},
#line 497 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1220), 0xee, 0xa9, 0xb8},
#line 185 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1221), 0xf4, 0xa4, 0x60},
        {-1}, {-1},
#line 118 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1224), 0x00, 0xff, 0xff},
        {-1}, {-1}, {-1}, {-1},
#line 241 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1229), 0xee, 0xe5, 0xde},
#line 496 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1230), 0xff, 0xb5, 0xc5},
        {-1}, {-1}, {-1}, {-1}, {-1},
#line 232 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1236), 0xa0, 0x20, 0xf0},
#line 539 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1237), 0x55, 0x1a, 0x8b},
        {-1},
#line 240 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1239), 0xff, 0xf5, 0xee},
        {-1},
#line 473 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1241), 0xee, 0x6a, 0x50},
        {-1}, {-1}, {-1},
#line 73 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1245), 0xd3, 0xd3, 0xd3},
#line 545 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1246), 0xee, 0xd2, 0xee},
#line 538 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1247), 0x7d, 0x26, 0xcd},
        {-1}, {-1}, {-1},
#line 472 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1251), 0xff, 0x72, 0x56},
        {-1}, {-1}, {-1}, {-1},
#line 544 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1256), 0xff, 0xe1, 0xff},
        {-1}, {-1}, {-1},
#line 91 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1260), 0x00, 0x00, 0xcd},
        {-1},
#line 117 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1262), 0x40, 0xe0, 0xd0},
#line 347 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1263), 0x00, 0x86, 0x8b},
        {-1},
#line 175 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1265), 0xbc, 0x8f, 0x8f},
        {-1}, {-1},
#line 346 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1268), 0x00, 0xc5, 0xcd},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 537 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1277), 0x91, 0x2c, 0xee},
        {-1}, {-1}, {-1}, {-1}, {-1},
#line 345 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1283), 0x00, 0xe5, 0xee},
        {-1}, {-1}, {-1},
#line 536 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1287), 0x9b, 0x30, 0xff},
#line 344 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1288), 0x00, 0xf5, 0xff},
        {-1},
#line 103 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1290), 0x46, 0x82, 0xb4},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1},
#line 136 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1305), 0x20, 0xb2, 0xaa},
        {-1},
#line 45 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1307), 0xf0, 0xf8, 0xff},
#line 36 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1308), 0xff, 0xff, 0xf0},
        {-1},
#line 182 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1310), 0xde, 0xb8, 0x87},
#line 435 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1311), 0x8b, 0x73, 0x55},
        {-1}, {-1}, {-1}, {-1},
#line 434 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1316), 0xcd, 0xaa, 0x7d},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1},
#line 433 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1331), 0xee, 0xc5, 0x91},
        {-1}, {-1}, {-1}, {-1},
#line 432 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1336), 0xff, 0xd3, 0x9b},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 181 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1344), 0xcd, 0x85, 0x3f},
#line 527 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1345), 0x8b, 0x66, 0x8b},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 526 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1355), 0xcd, 0x96, 0xcd},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 68 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1364), 0x77, 0x88, 0x99},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 89 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1374), 0x84, 0x70, 0xff},
#line 143 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1375), 0x7c, 0xfc, 0x00},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 20 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1383), 0xfd, 0xf5, 0xe6},
        {-1},
#line 525 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1385), 0xee, 0xae, 0xee},
        {-1}, {-1},
#line 156 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1388), 0x6b, 0x8e, 0x23},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 524 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1395), 0xff, 0xbb, 0xff},
        {-1}, {-1}, {-1}, {-1},
#line 371 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1400), 0x54, 0x8b, 0x54},
#line 134 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1401), 0x3c, 0xb3, 0x71},
        {-1},
#line 39 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1403), 0xff, 0xf5, 0xee},
        {-1},
#line 370 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1405), 0x7c, 0xcd, 0x7c},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 199 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1415), 0xff, 0x7f, 0x50},
        {-1}, {-1},
#line 407 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1418), 0x8b, 0x8b, 0x00},
        {-1},
#line 369 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1420), 0x90, 0xee, 0x90},
        {-1}, {-1}, {-1}, {-1},
#line 368 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1425), 0x9a, 0xff, 0x9a},
        {-1}, {-1},
#line 406 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1428), 0xcd, 0xcd, 0x00},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1},
#line 176 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1443), 0xcd, 0x5c, 0x5c},
#line 66 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1444), 0x77, 0x88, 0x99},
#line 107 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1445), 0xad, 0xd8, 0xe6},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 79 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1453), 0x00, 0x00, 0x80},
#line 130 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1454), 0x8f, 0xbc, 0x8f},
        {-1},
#line 90 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1456), 0x00, 0x00, 0xcd},
        {-1},
#line 405 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1458), 0xee, 0xee, 0x00},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 404 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1468), 0xff, 0xff, 0x00},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1},
#line 169 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1480), 0xee, 0xdd, 0x82},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1},
#line 51 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1509), 0xff, 0xff, 0xff},
        {-1}, {-1}, {-1}, {-1}, {-1},
#line 55 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1515), 0x2f, 0x4f, 0x4f},
        {-1},
#line 194 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1517), 0xff, 0xa0, 0x7a},
#line 125 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1518), 0x7f, 0xff, 0xd4},
#line 359 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1519), 0x45, 0x8b, 0x74},
        {-1}, {-1}, {-1}, {-1},
#line 358 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1524), 0x66, 0xcd, 0xaa},
#line 218 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1525), 0xd0, 0x20, 0x90},
        {-1}, {-1}, {-1}, {-1},
#line 82 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1530), 0x48, 0x3d, 0x8b},
#line 186 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1531), 0xf4, 0xa4, 0x60},
        {-1}, {-1},
#line 222 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1534), 0xdd, 0xa0, 0xdd},
        {-1}, {-1}, {-1}, {-1},
#line 357 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1539), 0x76, 0xee, 0xc6},
        {-1}, {-1}, {-1}, {-1},
#line 356 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1544), 0x7f, 0xff, 0xd4},
        {-1}, {-1}, {-1}, {-1}, {-1},
#line 145 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1550), 0x7f, 0xff, 0x00},
#line 383 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1551), 0x45, 0x8b, 0x00},
        {-1}, {-1}, {-1},
#line 92 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1555), 0x41, 0x69, 0xe1},
#line 382 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1556), 0x66, 0xcd, 0x00},
        {-1}, {-1},
#line 139 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1559), 0x98, 0xfb, 0x98},
#line 87 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1560), 0x7b, 0x68, 0xee},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1},
#line 381 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1571), 0x76, 0xee, 0x00},
        {-1}, {-1},
#line 210 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1574), 0xff, 0xc0, 0xcb},
        {-1},
#line 380 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1576), 0x7f, 0xff, 0x00},
        {-1}, {-1}, {-1}, {-1}, {-1},
#line 153 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1582), 0x9a, 0xcd, 0x32},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1},
#line 53 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1595), 0x2f, 0x4f, 0x4f},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1},
#line 174 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1626), 0xbc, 0x8f, 0x8f},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1},
#line 188 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1639), 0xd2, 0x69, 0x1e},
#line 447 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1640), 0x8b, 0x45, 0x13},
        {-1}, {-1}, {-1}, {-1},
#line 446 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1645), 0xcd, 0x66, 0x1d},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 757 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1653), 0x00, 0x8b, 0x8b},
        {-1}, {-1}, {-1}, {-1},
#line 162 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1658), 0xee, 0xe8, 0xaa},
        {-1},
#line 445 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1660), 0xee, 0x76, 0x21},
        {-1}, {-1}, {-1}, {-1},
#line 444 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1665), 0xff, 0x7f, 0x24},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1},
#line 567 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1685), 0x17, 0x17, 0x17},
#line 647 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1686), 0x7d, 0x7d, 0x7d},
#line 561 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1687), 0x0f, 0x0f, 0x0f},
#line 641 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1688), 0x75, 0x75, 0x75},
#line 122 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1689), 0x5f, 0x9e, 0xa0},
#line 343 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1690), 0x53, 0x86, 0x8b},
#line 627 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1691), 0x63, 0x63, 0x63},
        {-1},
#line 621 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1693), 0x5c, 0x5c, 0x5c},
        {-1},
#line 342 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1695), 0x7a, 0xc5, 0xcd},
        {-1},
#line 149 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1697), 0xad, 0xff, 0x2f},
        {-1}, {-1}, {-1}, {-1},
#line 707 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1702), 0xc9, 0xc9, 0xc9},
        {-1},
#line 701 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1704), 0xc2, 0xc2, 0xc2},
#line 76 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1705), 0x19, 0x19, 0x70},
#line 607 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1706), 0x4a, 0x4a, 0x4a},
        {-1},
#line 601 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1708), 0x42, 0x42, 0x42},
        {-1},
#line 341 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1710), 0x8e, 0xe5, 0xee},
#line 587 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1711), 0x30, 0x30, 0x30},
        {-1},
#line 581 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1713), 0x29, 0x29, 0x29},
        {-1},
#line 340 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1715), 0x98, 0xf5, 0xff},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1},
#line 142 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1736), 0x7c, 0xfc, 0x00},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 201 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1755), 0xf0, 0x80, 0x80},
        {-1}, {-1}, {-1},
#line 223 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1759), 0xda, 0x70, 0xd6},
#line 523 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1760), 0x8b, 0x47, 0x89},
        {-1}, {-1}, {-1}, {-1},
#line 566 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1765), 0x17, 0x17, 0x17},
#line 646 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1766), 0x7d, 0x7d, 0x7d},
#line 560 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1767), 0x0f, 0x0f, 0x0f},
#line 640 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1768), 0x75, 0x75, 0x75},
        {-1},
#line 522 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1770), 0xcd, 0x69, 0xc9},
#line 626 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1771), 0x63, 0x63, 0x63},
        {-1},
#line 620 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1773), 0x5c, 0x5c, 0x5c},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 706 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1782), 0xc9, 0xc9, 0xc9},
        {-1},
#line 700 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1784), 0xc2, 0xc2, 0xc2},
        {-1},
#line 606 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1786), 0x4a, 0x4a, 0x4a},
        {-1},
#line 600 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1788), 0x42, 0x42, 0x42},
        {-1}, {-1},
#line 586 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1791), 0x30, 0x30, 0x30},
        {-1},
#line 580 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1793), 0x29, 0x29, 0x29},
        {-1}, {-1}, {-1},
#line 225 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1797), 0xba, 0x55, 0xd3},
#line 531 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1798), 0x7a, 0x37, 0x8b},
#line 42 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1799), 0xf5, 0xff, 0xfa},
#line 521 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1800), 0xee, 0x7a, 0xe9},
        {-1}, {-1}, {-1},
#line 279 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1804), 0x8b, 0x83, 0x86},
        {-1}, {-1}, {-1},
#line 530 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1808), 0xb4, 0x52, 0xcd},
        {-1},
#line 520 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1810), 0xff, 0x83, 0xfa},
        {-1}, {-1}, {-1},
#line 278 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1814), 0xcd, 0xc1, 0xc5},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1},
#line 160 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1826), 0xf0, 0xe6, 0x8c},
        {-1},
#line 44 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1828), 0xf0, 0xf8, 0xff},
        {-1}, {-1}, {-1},
#line 113 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1832), 0x00, 0xce, 0xd1},
        {-1}, {-1},
#line 565 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1835), 0x14, 0x14, 0x14},
#line 645 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1836), 0x7a, 0x7a, 0x7a},
        {-1},
#line 529 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1838), 0xd1, 0x5f, 0xee},
        {-1}, {-1},
#line 625 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1841), 0x61, 0x61, 0x61},
        {-1}, {-1},
#line 277 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1844), 0xee, 0xe0, 0xe5},
#line 549 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1845), 0x00, 0x00, 0x00},
#line 629 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1846), 0x66, 0x66, 0x66},
        {-1},
#line 528 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1848), 0xe0, 0x66, 0xff},
        {-1}, {-1},
#line 609 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1851), 0x4d, 0x4d, 0x4d},
#line 705 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1852), 0xc7, 0xc7, 0xc7},
        {-1},
#line 276 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1854), 0xff, 0xf0, 0xf5},
        {-1},
#line 605 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1856), 0x47, 0x47, 0x47},
        {-1}, {-1}, {-1}, {-1},
#line 585 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1861), 0x2e, 0x2e, 0x2e},
#line 689 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1862), 0xb3, 0xb3, 0xb3},
        {-1}, {-1},
#line 335 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1865), 0x7a, 0x8b, 0x8b},
#line 589 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1866), 0x33, 0x33, 0x33},
        {-1}, {-1}, {-1},
#line 334 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1870), 0xb4, 0xcd, 0xcd},
#line 569 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1871), 0x1a, 0x1a, 0x1a},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1},
#line 333 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1885), 0xd1, 0xee, 0xee},
        {-1}, {-1}, {-1}, {-1},
#line 332 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1890), 0xe0, 0xff, 0xff},
        {-1}, {-1},
#line 229 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1893), 0x94, 0x00, 0xd3},
        {-1}, {-1}, {-1},
#line 147 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1897), 0x00, 0xfa, 0x9a},
        {-1}, {-1}, {-1},
#line 164 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1901), 0xfa, 0xfa, 0xd2},
        {-1},
#line 391 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1903), 0x6e, 0x8b, 0x3d},
        {-1}, {-1}, {-1},
#line 166 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1907), 0xff, 0xff, 0xe0},
        {-1}, {-1}, {-1}, {-1}, {-1},
#line 390 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1913), 0xa2, 0xcd, 0x5a},
        {-1},
#line 564 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1915), 0x14, 0x14, 0x14},
#line 644 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1916), 0x7a, 0x7a, 0x7a},
        {-1}, {-1}, {-1},
#line 138 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1920), 0x98, 0xfb, 0x98},
#line 624 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1921), 0x61, 0x61, 0x61},
        {-1}, {-1}, {-1},
#line 548 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1925), 0x00, 0x00, 0x00},
#line 628 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1926), 0x66, 0x66, 0x66},
        {-1}, {-1}, {-1}, {-1},
#line 608 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1931), 0x4d, 0x4d, 0x4d},
#line 704 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1932), 0xc7, 0xc7, 0xc7},
        {-1}, {-1},
#line 275 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1935), 0x83, 0x8b, 0x83},
#line 604 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1936), 0x47, 0x47, 0x47},
        {-1}, {-1}, {-1}, {-1},
#line 584 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1941), 0x2e, 0x2e, 0x2e},
#line 688 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1942), 0xb3, 0xb3, 0xb3},
#line 389 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1943), 0xbc, 0xee, 0x68},
        {-1},
#line 274 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1945), 0xc1, 0xcd, 0xc1},
#line 588 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1946), 0x33, 0x33, 0x33},
        {-1}, {-1}, {-1}, {-1},
#line 568 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1951), 0x1a, 0x1a, 0x1a},
        {-1},
#line 388 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1953), 0xca, 0xff, 0x70},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1},
#line 116 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1973), 0x48, 0xd1, 0xcc},
#line 78 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1974), 0x00, 0x00, 0x80},
#line 273 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1975), 0xe0, 0xee, 0xe0},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 272 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1985), 0xf0, 0xff, 0xf0},
#line 67 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str1986), 0x77, 0x88, 0x99},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 224 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str2011), 0xba, 0x55, 0xd3},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 559 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str2019), 0x0d, 0x0d, 0x0d},
#line 639 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str2020), 0x73, 0x73, 0x73},
        {-1}, {-1}, {-1},
#line 120 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str2024), 0xe0, 0xff, 0xff},
#line 619 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str2025), 0x59, 0x59, 0x59},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1},
#line 699 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str2036), 0xbf, 0xbf, 0xbf},
        {-1},
#line 97 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str2038), 0x00, 0xbf, 0xff},
        {-1},
#line 599 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str2040), 0x40, 0x40, 0x40},
        {-1}, {-1}, {-1}, {-1},
#line 579 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str2045), 0x26, 0x26, 0x26},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1},
#line 234 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str2057), 0x93, 0x70, 0xdb},
#line 543 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str2058), 0x5d, 0x47, 0x8b},
        {-1}, {-1},
#line 495 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str2061), 0x8b, 0x3a, 0x62},
#line 129 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str2062), 0x55, 0x6b, 0x2f},
        {-1}, {-1}, {-1},
#line 65 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str2066), 0x77, 0x88, 0x99},
        {-1},
#line 542 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str2068), 0x89, 0x68, 0xcd},
        {-1}, {-1},
#line 494 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str2071), 0xcd, 0x60, 0x90},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 28 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str2089), 0xff, 0xeb, 0xcd},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 541 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str2098), 0x9f, 0x79, 0xee},
#line 558 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str2099), 0x0d, 0x0d, 0x0d},
#line 638 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str2100), 0x73, 0x73, 0x73},
#line 493 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str2101), 0xee, 0x6a, 0xa7},
        {-1}, {-1},
#line 101 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str2104), 0x87, 0xce, 0xfa},
#line 618 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str2105), 0x59, 0x59, 0x59},
        {-1}, {-1},
#line 540 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str2108), 0xab, 0x82, 0xff},
        {-1}, {-1},
#line 492 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str2111), 0xff, 0x6e, 0xb4},
        {-1}, {-1}, {-1}, {-1},
#line 698 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str2116), 0xbf, 0xbf, 0xbf},
        {-1},
#line 451 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str2118), 0x8b, 0x1a, 0x1a},
        {-1},
#line 598 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str2120), 0x40, 0x40, 0x40},
        {-1}, {-1},
#line 450 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str2123), 0xcd, 0x26, 0x26},
        {-1},
#line 578 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str2125), 0x26, 0x26, 0x26},
#line 105 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str2126), 0xb0, 0xc4, 0xde},
        {-1}, {-1}, {-1},
#line 41 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str2130), 0xf5, 0xff, 0xfa},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 449 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str2138), 0xee, 0x2c, 0x2c},
        {-1}, {-1}, {-1}, {-1},
#line 448 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str2143), 0xff, 0x30, 0x30},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1},
#line 756 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str2174), 0x00, 0x8b, 0x8b},
        {-1}, {-1}, {-1}, {-1},
#line 161 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str2179), 0xee, 0xe8, 0xaa},
        {-1}, {-1}, {-1}, {-1},
#line 124 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str2184), 0x66, 0xcd, 0xaa},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 112 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str2193), 0xaf, 0xee, 0xee},
#line 339 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str2194), 0x66, 0x8b, 0x8b},
        {-1},
#line 200 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str2196), 0xf0, 0x80, 0x80},
#line 86 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str2197), 0x7b, 0x68, 0xee},
        {-1},
#line 16 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str2199), 0xf5, 0xf5, 0xf5},
        {-1}, {-1}, {-1}, {-1},
#line 338 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str2204), 0x96, 0xcd, 0xcd},
        {-1}, {-1}, {-1}, {-1}, {-1},
#line 121 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str2210), 0x5f, 0x9e, 0xa0},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 24 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str2218), 0xfa, 0xeb, 0xd7},
#line 247 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str2219), 0x8b, 0x83, 0x78},
        {-1}, {-1}, {-1},
#line 231 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str2223), 0x8a, 0x2b, 0xe2},
#line 23 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str2224), 0xfa, 0xeb, 0xd7},
        {-1}, {-1}, {-1}, {-1},
#line 246 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str2229), 0xcd, 0xc0, 0xb0},
        {-1}, {-1}, {-1}, {-1},
#line 337 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str2234), 0xae, 0xee, 0xee},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 336 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str2244), 0xbb, 0xff, 0xff},
        {-1}, {-1}, {-1},
#line 167 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str2248), 0xff, 0xff, 0x00},
        {-1}, {-1},
#line 34 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str2251), 0xff, 0xe4, 0xb5},
#line 491 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str2252), 0x8b, 0x0a, 0x50},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 245 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str2259), 0xee, 0xdf, 0xcc},
        {-1}, {-1},
#line 490 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str2262), 0xcd, 0x10, 0x76},
#line 152 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str2263), 0x9a, 0xcd, 0x32},
        {-1}, {-1},
#line 88 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str2266), 0x84, 0x70, 0xff},
        {-1}, {-1},
#line 244 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str2269), 0xff, 0xef, 0xdb},
        {-1}, {-1},
#line 267 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str2272), 0x8b, 0x88, 0x78},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 226 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str2281), 0x99, 0x32, 0xcc},
#line 266 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str2282), 0xcd, 0xc8, 0xb1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 489 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str2292), 0xee, 0x12, 0x89},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 488 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str2302), 0xff, 0x14, 0x93},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 265 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str2312), 0xee, 0xe8, 0xcd},
        {-1}, {-1}, {-1}, {-1}, {-1},
#line 163 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str2318), 0xfa, 0xfa, 0xd2},
        {-1}, {-1}, {-1},
#line 264 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str2322), 0xff, 0xf8, 0xdc},
        {-1}, {-1},
#line 15 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str2325), 0xf5, 0xf5, 0xf5},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1},
#line 119 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str2385), 0xe0, 0xff, 0xff},
        {-1}, {-1},
#line 217 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str2388), 0xc7, 0x15, 0x85},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1},
#line 110 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str2401), 0xb0, 0xe0, 0xe6},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 123 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str2410), 0x66, 0xcd, 0xaa},
        {-1}, {-1}, {-1},
#line 228 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str2414), 0x94, 0x00, 0xd3},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 128 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str2424), 0x55, 0x6b, 0x2f},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 227 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str2433), 0x99, 0x32, 0xcc},
#line 535 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str2434), 0x68, 0x22, 0x8b},
        {-1}, {-1}, {-1}, {-1},
#line 534 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str2439), 0x9a, 0x32, 0xcd},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1},
#line 533 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str2454), 0xb2, 0x3a, 0xee},
#line 207 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str2455), 0xff, 0x69, 0xb4},
        {-1}, {-1}, {-1},
#line 532 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str2459), 0xbf, 0x3e, 0xff},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1},
#line 48 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str2473), 0xff, 0xf0, 0xf5},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 18 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str2483), 0xff, 0xfa, 0xf0},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1},
#line 189 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str2512), 0xb2, 0x22, 0x22},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1},
#line 747 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str2526), 0xfc, 0xfc, 0xfc},
#line 687 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str2527), 0xb0, 0xb0, 0xb0},
#line 741 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str2528), 0xf5, 0xf5, 0xf5},
#line 681 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str2529), 0xa8, 0xa8, 0xa8},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 75 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str2546), 0x19, 0x19, 0x70},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1},
#line 14 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str2594), 0xf8, 0xf8, 0xff},
        {-1}, {-1},
#line 109 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str2597), 0xb0, 0xe0, 0xe6},
        {-1}, {-1}, {-1},
#line 727 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str2601), 0xe3, 0xe3, 0xe3},
        {-1},
#line 721 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str2603), 0xdb, 0xdb, 0xdb},
        {-1}, {-1},
#line 746 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str2606), 0xfc, 0xfc, 0xfc},
#line 686 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str2607), 0xb0, 0xb0, 0xb0},
#line 740 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str2608), 0xf5, 0xf5, 0xf5},
#line 680 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str2609), 0xa8, 0xa8, 0xa8},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1},
#line 503 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str2623), 0x8b, 0x5f, 0x65},
        {-1}, {-1}, {-1}, {-1},
#line 502 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str2628), 0xcd, 0x8c, 0x95},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 19 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str2635), 0xff, 0xfa, 0xf0},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 501 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str2643), 0xee, 0xa2, 0xad},
        {-1}, {-1},
#line 209 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str2646), 0xff, 0x14, 0x93},
        {-1},
#line 500 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str2648), 0xff, 0xae, 0xb9},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 35 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str2666), 0xff, 0xf8, 0xdc},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 745 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str2676), 0xfa, 0xfa, 0xfa},
#line 685 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str2677), 0xad, 0xad, 0xad},
        {-1}, {-1}, {-1},
#line 726 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str2681), 0xe3, 0xe3, 0xe3},
        {-1},
#line 720 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str2683), 0xdb, 0xdb, 0xdb},
        {-1}, {-1},
#line 729 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str2686), 0xe5, 0xe5, 0xe5},
#line 669 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str2687), 0x99, 0x99, 0x99},
        {-1}, {-1}, {-1}, {-1}, {-1},
#line 667 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str2693), 0x96, 0x96, 0x96},
        {-1},
#line 661 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str2695), 0x8f, 0x8f, 0x8f},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 27 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str2720), 0xff, 0xeb, 0xcd},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1},
#line 81 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str2735), 0x64, 0x95, 0xed},
        {-1}, {-1}, {-1}, {-1}, {-1},
#line 80 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str2741), 0x64, 0x95, 0xed},
        {-1},
#line 158 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str2743), 0xbd, 0xb7, 0x6b},
#line 230 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str2744), 0x8a, 0x2b, 0xe2},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 725 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str2751), 0xe0, 0xe0, 0xe0},
        {-1}, {-1}, {-1}, {-1},
#line 744 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str2756), 0xfa, 0xfa, 0xfa},
#line 684 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str2757), 0xad, 0xad, 0xad},
        {-1}, {-1}, {-1},
#line 709 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str2761), 0xcc, 0xcc, 0xcc},
        {-1}, {-1}, {-1},
#line 40 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str2765), 0xf0, 0xff, 0xf0},
#line 728 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str2766), 0xe5, 0xe5, 0xe5},
#line 668 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str2767), 0x99, 0x99, 0x99},
        {-1}, {-1}, {-1}, {-1}, {-1},
#line 666 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str2773), 0x96, 0x96, 0x96},
        {-1},
#line 660 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str2775), 0x8f, 0x8f, 0x8f},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 749 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str2792), 0xff, 0xff, 0xff},
#line 233 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str2793), 0x93, 0x70, 0xdb},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 115 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str2819), 0x48, 0xd1, 0xcc},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1},
#line 724 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str2831), 0xe0, 0xe0, 0xe0},
        {-1}, {-1},
#line 148 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str2834), 0xad, 0xff, 0x2f},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 708 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str2841), 0xcc, 0xcc, 0xcc},
        {-1},
#line 665 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str2843), 0x94, 0x94, 0x94},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 649 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str2853), 0x7f, 0x7f, 0x7f},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 739 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str2860), 0xf2, 0xf2, 0xf2},
#line 679 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str2861), 0xa6, 0xa6, 0xa6},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1},
#line 748 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str2872), 0xff, 0xff, 0xff},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1},
#line 159 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str2895), 0xbd, 0xb7, 0x6b},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 664 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str2923), 0x94, 0x94, 0x94},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 648 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str2933), 0x7f, 0x7f, 0x7f},
        {-1},
#line 719 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str2935), 0xd9, 0xd9, 0xd9},
        {-1}, {-1}, {-1}, {-1},
#line 738 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str2940), 0xf2, 0xf2, 0xf2},
#line 678 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str2941), 0xa6, 0xa6, 0xa6},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 13 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str2960), 0xf8, 0xf8, 0xff},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1},
#line 214 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str2971), 0xdb, 0x70, 0x93},
#line 507 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str2972), 0x8b, 0x47, 0x5d},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 506 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str2982), 0xcd, 0x68, 0x89},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1},
#line 505 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str3012), 0xee, 0x79, 0x9f},
        {-1}, {-1},
#line 718 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str3015), 0xd9, 0xd9, 0xd9},
        {-1},
#line 212 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str3017), 0xff, 0xb6, 0xc1},
        {-1}, {-1}, {-1}, {-1},
#line 504 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str3022), 0xff, 0x82, 0xab},
        {-1}, {-1}, {-1}, {-1},
#line 659 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str3027), 0x8c, 0x8c, 0x8c},
        {-1}, {-1}, {-1}, {-1}, {-1},
#line 32 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str3033), 0xff, 0xde, 0xad},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1},
#line 165 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str3044), 0xff, 0xff, 0xe0},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 111 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str3062), 0xaf, 0xee, 0xee},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 146 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str3099), 0x00, 0xfa, 0x9a},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 658 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str3107), 0x8c, 0x8c, 0x8c},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 263 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str3116), 0x8b, 0x89, 0x70},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 262 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str3126), 0xcd, 0xc9, 0xa5},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 211 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str3143), 0xff, 0xb6, 0xc1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1},
#line 261 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str3156), 0xee, 0xe9, 0xbf},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 260 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str3166), 0xff, 0xfa, 0xcd},
#line 208 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str3167), 0xff, 0x14, 0x93},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 33 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str3185), 0xff, 0xde, 0xad},
#line 259 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str3186), 0x8b, 0x79, 0x5e},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 258 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str3196), 0xcd, 0xb3, 0x8b},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1},
#line 255 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str3225), 0x8b, 0x77, 0x65},
#line 257 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str3226), 0xee, 0xcf, 0xa1},
        {-1}, {-1}, {-1},
#line 254 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str3230), 0xcd, 0xaf, 0x95},
        {-1}, {-1}, {-1}, {-1}, {-1},
#line 256 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str3236), 0xff, 0xde, 0xad},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 253 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str3245), 0xee, 0xcb, 0xad},
        {-1}, {-1}, {-1}, {-1},
#line 252 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str3250), 0xff, 0xda, 0xb9},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 38 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str3275), 0xff, 0xfa, 0xcd},
        {-1}, {-1}, {-1},
#line 31 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str3279), 0xff, 0xda, 0xb9},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 47 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str3314), 0xff, 0xf0, 0xf5},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 216 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str3330), 0xc7, 0x15, 0x85},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1},
#line 206 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str3471), 0xff, 0x69, 0xb4},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1},
#line 30 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str3745), 0xff, 0xda, 0xb9},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1},
#line 37 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str3841), 0xff, 0xfa, 0xcd},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 213 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str4013), 0xdb, 0x70, 0x93},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1},
#line 25 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str5422), 0xff, 0xef, 0xd5},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
        {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 26 "rgblookup.gperf"
        {offsetof(struct stringpool_t, stringpool_str5574), 0xff, 0xef, 0xd5}
    };

    if (len <= MAX_WORD_LENGTH && len >= MIN_WORD_LENGTH)
    {
        unsigned int key = hash (str, len);

        if (key <= MAX_HASH_VALUE)
        {
            register int o = wordlist[key].name;
            if (o >= 0)
            {
                register const char *s = o + stringpool;

                if ((((unsigned char)*str ^ (unsigned char)*s) & ~32) == 0 && !gperf_case_strcmp (str, s))
                    return &wordlist[key];
            }
        }
    }
    return 0;
}
