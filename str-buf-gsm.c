/**************************************************************************/
/*                                                                        */
/*  Copyright (C) INTERSEC SA                                             */
/*                                                                        */
/*  Should you receive a copy of this source code, you must check you     */
/*  have a proper, written authorization of INTERSEC to hold it. If you   */
/*  don't have such an authorization, you must DELETE all source code     */
/*  files in your possession, and inform INTERSEC of the fact you obtain  */
/*  these files. Should you not comply to these terms, you can be         */
/*  prosecuted in the extent permitted by applicable law.                 */
/*                                                                        */
/**************************************************************************/

#include "core.h"
#include "z.h"


/* Convert GSM charset to Unicode.
 * This supports gsm7 default alphabet + default alphabet extension table,
 * using the 0x1B escape. c.f. 3GPP TS 23.038
 *
 * TODO: support other single shift and locking shift tables
 * (lib-inet/gsm-encoding.c supports turkish, spanish and portuguese) !
 */
static int16_t const __gsm7_to_unicode[] = {
#define X(x)     (0x##x)
#define UNK      (-1)
    /* 0x00,   0x01,   0x02,   0x03,   0x04,   0x05,   0x06,   0x07,    */
       X(40),  X(A3),  X(24),  X(A5),  X(E8),  X(E9),  X(F9),  X(EC),
    /* 0x08,   0x09,   0x0A,   0x0B,   0x0C,   0x0D,   0x0E,   0x0F,    */
       X(F2),  X(C7),  X(0A),  X(D8),  X(F8),  X(0D),  X(C5),  X(E5),
    /* 0x10,   0x11,   0x12,   0x13,   0x14,   0x15,   0x16,   0x17,    */
       X(0394),X(5F),  X(03A6),X(0393),X(039B),X(03A9),X(03A0),X(03A8),
    /* 0x18,   0x19,   0x1A,   0x1B,   0x1C,   0x1D,   0x1E,   0x1F,    */
       X(03A3),X(0398),X(039E),UNK,    X(C6),  X(E6),  X(DF),  X(C9),
    /* 0x20,   0x21,   0x22,   0x23,   0x24,   0x25,   0x26,   0x27,    */
       X(20),  X(21),  X(22),  X(23),  X(A4),  X(25),  X(26),  X(27),
    /* 0x28,   0x29,   0x2A,   0x2B,   0x2C,   0x2D,   0x2E,   0x2F,    */
       X(28),  X(29),  X(2A),  X(2B),  X(2C),  X(2D),  X(2E),  X(2F),
    /* 0x30,   0x31,   0x32,   0x33,   0x34,   0x35,   0x36,   0x37,    */
       X(30),  X(31),  X(32),  X(33),  X(34),  X(35),  X(36),  X(37),
    /* 0x38,   0x39,   0x3A,   0x3B,   0x3C,   0x3D,   0x3E,   0x3F,    */
       X(38),  X(39),  X(3A),  X(3B),  X(3C),  X(3D),  X(3E),  X(3F),
    /* 0x40,   0x41,   0x42,   0x43,   0x44,   0x45,   0x46,   0x47,    */
       X(A1),  X(41),  X(42),  X(43),  X(44),  X(45),  X(46),  X(47),
    /* 0x48,   0x49,   0x4A,   0x4B,   0x4C,   0x4D,   0x4E,   0x4F,    */
       X(48),  X(49),  X(4A),  X(4B),  X(4C),  X(4D),  X(4E),  X(4F),
    /* 0x50,   0x51,   0x52,   0x53,   0x54,   0x55,   0x56,   0x57,    */
       X(50),  X(51),  X(52),  X(53),  X(54),  X(55),  X(56),  X(57),
    /* 0x58,   0x59,   0x5A,   0x5B,   0x5C,   0x5D,   0x5E,   0x5F,    */
       X(58),  X(59),  X(5A),  X(C4),  X(D6),  X(D1),  X(DC),  X(A7),
    /* 0x60,   0x61,   0x62,   0x63,   0x64,   0x65,   0x66,   0x67,    */
       X(BF),  X(61),  X(62),  X(63),  X(64),  X(65),  X(66),  X(67),
    /* 0x68,   0x69,   0x6A,   0x6B,   0x6C,   0x6D,   0x6E,   0x6F,    */
       X(68),  X(69),  X(6A),  X(6B),  X(6C),  X(6D),  X(6E),  X(6F),
    /* 0x70,   0x71,   0x72,   0x73,   0x74,   0x75,   0x76,   0x77,    */
       X(70),  X(71),  X(72),  X(73),  X(74),  X(75),  X(76),  X(77),
    /* 0x78,   0x79,   0x7A,   0x7B,   0x7C,   0x7D,   0x7E,   0x7F,    */
       X(78),  X(79),  X(7A),  X(E4),  X(F6),  X(F1),  X(FC),  X(E0),

    /* 0x1B00, 0x1B01, 0x1B02, 0x1B03, 0x1B04, 0x1B05, 0x1B06, 0x1B07,  */
       UNK,    UNK,    UNK,    UNK,    UNK,    UNK,    UNK,    UNK,
    /* 0x1B08, 0x1B09, 0x1B0A, 0x1B0B, 0x1B0C, 0x1B0D, 0x1B0E, 0x1B0F,  */
       UNK,    UNK,    X(0C),  UNK,    UNK,    UNK,    UNK,    UNK,
    /* 0x1B10, 0x1B11, 0x1B12, 0x1B13, 0x1B14, 0x1B15, 0x1B16, 0x1B17,  */
       UNK,    UNK,    UNK,    UNK,    X(5E),  UNK,    UNK,    UNK,
    /* 0x1B18, 0x1B19, 0x1B1A, 0x1B1B, 0x1B1C, 0x1B1D, 0x1B1E, 0x1B1F,  */
       UNK,    UNK,    UNK,    UNK,    UNK,    UNK,    UNK,    UNK,
    /* 0x1B20, 0x1B21, 0x1B22, 0x1B23, 0x1B24, 0x1B25, 0x1B26, 0x1B27,  */
       UNK,    UNK,    UNK,    UNK,    UNK,    UNK,    UNK,    UNK,
    /* 0x1B28, 0x1B29, 0x1B2A, 0x1B2B, 0x1B2C, 0x1B2D, 0x1B2E, 0x1B2F,  */
       X(7B),  X(7D),  UNK,    UNK,    UNK,    UNK,    UNK,    X(5C),
    /* 0x1B30, 0x1B31, 0x1B32, 0x1B33, 0x1B34, 0x1B35, 0x1B36, 0x1B37,  */
       UNK,    UNK,    UNK,    UNK,    UNK,    UNK,    UNK,    UNK,
    /* 0x1B38, 0x1B39, 0x1B3A, 0x1B3B, 0x1B3C, 0x1B3D, 0x1B3E, 0x1B3F,  */
       UNK,    UNK,    UNK,    UNK,    X(5B),  X(7E),  X(5D),  UNK,
    /* 0x1B40, 0x1B41, 0x1B42, 0x1B43, 0x1B44, 0x1B45, 0x1B46, 0x1B47,  */
       X(7C),  UNK,    UNK,    UNK,    UNK,    UNK,    UNK,    UNK,
    /* 0x1B48, 0x1B49, 0x1B4A, 0x1B4B, 0x1B4C, 0x1B4D, 0x1B4E, 0x1B4F,  */
       UNK,    UNK,    UNK,    UNK,    UNK,    UNK,    UNK,    UNK,
    /* 0x1B50, 0x1B51, 0x1B52, 0x1B53, 0x1B54, 0x1B55, 0x1B56, 0x1B57,  */
       UNK,    UNK,    UNK,    UNK,    UNK,    UNK,    UNK,    UNK,
    /* 0x1B58, 0x1B59, 0x1B5A, 0x1B5B, 0x1B5C, 0x1B5D, 0x1B5E, 0x1B5F,  */
       UNK,    UNK,    UNK,    UNK,    UNK,    UNK,    UNK,    UNK,
    /* 0x1B60, 0x1B61, 0x1B62, 0x1B63, 0x1B64, 0x1B65, 0x1B66, 0x1B67,  */
       UNK,    UNK,    UNK,    UNK,    UNK,    X(20AC),UNK,    UNK,
    /* 0x1B68, 0x1B69, 0x1B6A, 0x1B6B, 0x1B6C, 0x1B6D, 0x1B6E, 0x1B6F,  */
       UNK,    UNK,    UNK,    UNK,    UNK,    UNK,    UNK,    UNK,
    /* 0x1B70, 0x1B71, 0x1B72, 0x1B73, 0x1B74, 0x1B75, 0x1B76, 0x1B77,  */
       UNK,    UNK,    UNK,    UNK,    UNK,    UNK,    UNK,    UNK,
    /* 0x1B78, 0x1B79, 0x1B7A, 0x1B7B, 0x1B7C, 0x1B7D, 0x1B7E, 0x1B7F,  */
       UNK,    UNK,    UNK,    UNK,    UNK,    UNK,    UNK,    UNK,

#undef UNK
#undef X
};

/* We apparently use the Windows-1252 table to describe the range 00-FF of
 * Unicode characters. In practice it matches quite well, except for 0x8x and
 * 0x9x which are control characters in unicode anyway */
static int16_t const __win1252_to_gsm7[] = {
#define X(x)  (x)
#define UNK   (-1)

    /*0x00,   0x01,    0x02,    0x03,    0x04,    0x05,    0x06,    0x07 */
    /*0x00,   0x01,    0x02,    0x03,    0x04,    0x05,    0x06,    0x07 */
    X(UNK),  X(UNK),  X(UNK),  X(UNK),  X(UNK),  X(UNK),  X(UNK),  X(UNK),

    /*0x08,   0x09,    0x0A,    0x0B,    0x0C,    0x0D,    0x0E,    0x0F */
    /*'\b',   '\t',    '\n',    0x0B,    '\f',    '\r',    0x0E,    0x0F */
    X(UNK), X(0x20), X(0x0A),  X(UNK),X(0x1B0A),X(0x0D),  X(UNK),  X(UNK),

    /*0x10,   0x11,    0x12,    0x13,    0x14,    0x15,    0x16,    0x17 */
    /*0x10,   0x11,    0x12,    0x13,    0x14,    0x15,    0x16,    0x17 */
    X(UNK),  X(UNK),  X(UNK),  X(UNK),  X(UNK),  X(UNK),  X(UNK),  X(UNK),

    /*0x18,   0x19,    0x1A,    0x1B,    0x1C,    0x1D,    0x1E,    0x1F */
    /*0x18,   0x19,    0x1A,    0x1B,    0x1C,    0x1D,    0x1E,    0x1F */
    X(UNK),  X(UNK),  X(UNK),  X(UNK),  X(UNK),  X(UNK),  X(UNK),  X(UNK),

    /*0x20,   0x21,    0x22,    0x23,    0x24,    0x25,    0x26,    0x27 */
    /* ' ',    '!',     '"',     '#',     '$',     '%',     '&',    '\'' */
    X(0x20), X(0x21), X(0x22), X(0x23), X(0x02), X(0x25), X(0x26), X(0x27),

    /*0x28,   0x29,    0x2A,    0x2B,    0x2C,    0x2D,    0x2E,    0x2F */
    /* '(',    ')',     '*',     '+',     ',',     '-',     '.',     '/' */
    X(0x28), X(0x29), X(0x2A), X(0x2B), X(0x2C), X(0x2D), X(0x2E), X(0x2F),

    /*0x30,   0x31,    0x32,    0x33,    0x34,    0x35,    0x36,    0x37 */
    /* '0',    '1',     '2',     '3',     '4',     '5',     '6',     '7' */
    X(0x30), X(0x31), X(0x32), X(0x33), X(0x34), X(0x35), X(0x36), X(0x37),

    /*0x38,   0x39,    0x3A,    0x3B,    0x3C,    0x3D,    0x3E,    0x3F */
    /* '8',    '9',     ':',     ';',     '<',     '=',     '>',     '?' */
    X(0x38), X(0x39), X(0x3A), X(0x3B), X(0x3C), X(0x3D), X(0x3E), X(0x3F),

    /*0x40,   0x41,    0x42,    0x43,    0x44,    0x45,    0x46,    0x47 */
    /* '@',    'A',     'B',     'C',     'D',     'E',     'F',     'G' */
    X(0x00), X(0x41), X(0x42), X(0x43), X(0x44), X(0x45), X(0x46), X(0x47),

    /*0x48,   0x49,    0x4A,    0x4B,    0x4C,    0x4D,    0x4E,    0x4F */
    /* 'H',    'I',     'J',     'K',     'L',     'M',     'N',     'O' */
    X(0x48), X(0x49), X(0x4A), X(0x4B), X(0x4C), X(0x4D), X(0x4E), X(0x4F),

    /*0x50,   0x51,    0x52,    0x53,    0x54,    0x55,    0x56,    0x57 */
    /* 'P',    'Q',     'R',     'S',     'T',     'U',     'V',     'W' */
    X(0x50), X(0x51), X(0x52), X(0x53), X(0x54), X(0x55), X(0x56), X(0x57),

    /*0x58,   0x59,    0x5A,    0x5B,    0x5C,    0x5D,    0x5E,    0x5F */
    /* 'X',    'Y',     'Z',     '[',    '\\',     ']',     '^',     '_' */
    X(0x58), X(0x59), X(0x5A),X(0x1B3C),X(0x1B2F),X(0x1B3E),X(0x1B14),X(0x11),

    /*0x60,   0x61,    0x62,    0x63,    0x64,    0x65,    0x66,    0x67 */
    /* '`',    'a',     'b',     'c',     'd',     'e',     'f',     'g' */
    X(0x27), X(0x61), X(0x62), X(0x63), X(0x64), X(0x65), X(0x66), X(0x67),

    /*0x68,   0x69,    0x6A,    0x6B,    0x6C,    0x6D,    0x6E,    0x6F */
    /* 'h',    'i',     'j',     'k',     'l',     'm',     'n',     'o' */
    X(0x68), X(0x69), X(0x6A), X(0x6B), X(0x6C), X(0x6D), X(0x6E), X(0x6F),

    /*0x70,   0x71,    0x72,    0x73,    0x74,    0x75,    0x76,    0x77 */
    /* 'p',    'q',     'r',     's',     't',     'u',     'v',     'w' */
    X(0x70), X(0x71), X(0x72), X(0x73), X(0x74), X(0x75), X(0x76), X(0x77),

    /*0x78,   0x79,    0x7A,    0x7B,    0x7C,    0x7D,    0x7E,    0x7F */
    /* 'x',    'y',     'z',     '{',     '|',     '}',     '~',    0x7F */
    X(0x78), X(0x79), X(0x7A),X(0x1B28),X(0x1B40),X(0x1B29),X(0x1B3D),X(UNK),

    /*0x80,   0x81,    0x82,    0x83,    0x84,    0x85,    0x86,    0x87 */
    /* EUR,   0x81,    0x82,    0x83,    0x84,    0x85,    0x86,    0x87 */
    X(0x1B65),X(UNK),  X(','),  X('f'),  X('"'),  X(UNK),  X(UNK),  X(UNK),

    /*0x88,   0x89,    0x8A,    0x8B,    0x8C,    0x8D,    0x8E,    0x8F */
    /*0x88,   0x89,    0x8A,    0x8B,    0x8C,    0x8D,    0x8E,    0x8F */
    X(0x1B14),X(UNK),  X('S'),  X('<'),X(0x4F45), X(UNK),  X('Z'),  X(UNK),

    /*0x90,   0x91,    0x92,    0x93,    0x94,    0x95,    0x96,    0x97 */
    /*0x90,   0x91,    0x92,    0x93,    0x94,    0x95,    0x96,    0x97 */
    X(UNK), X(0x27), X(0x27), X(0x22),  X(0x22), X(UNK),  X('-'),  X('-'),

    /*0x98,   0x99,    0x9A,    0x9B,    0x9C,    0x9D,    0x9E,    0x9F */
    /*0x98,   0x99,    0x9A,    0x9B,    0x9C,    0x9D,    0x9E,    0x9F */
    X(0x1B3D),X(0x746D),X('s'), X('>'),X(0x6F65), X(UNK),  X('z'),  X('Y'),

    /*0xA0,   0xA1,    0xA2,    0xA3,    0xA4,    0xA5,    0xA6,    0xA7 */
    /* ' ',    '¡',     '¢',     '£',     '¤',     '¥',     '¦',     '§' */
    X(0x20), X(0x40),  X('c'), X(0x01), X(0x24), X(0x03), X(0x1B40),X(0x5F),

    /*0xA8,   0xA9,    0xAA,    0xAB,    0xAC,    0xAD,    0xAE,    0xAF */
    /* '¨',    '©',     'ª',     '«',     '¬',     '­',     '®',     '¯' */
    X(0x22),  X('c'),  X('a'), X(0x22),  X('-'), X(0x2D),  X('r'),  X('-'),

    /*0xB0,   0xB1,    0xB2,    0xB3,    0xB4,    0xB5,    0xB6,    0xB7 */
    /* '°',    '±',     '²',     '³',     '´',     'µ',     '¶',     '·' */
    X(UNK),  X(UNK),  X('2'),  X('3'), X(0x27),  X('u'),  X(UNK),  X('.'),

    /*0xB8,   0xB9,    0xBA,    0xBB,    0xBC,    0xBD,    0xBE,    0xBF */
    /* '¸',    '¹',     'º',     '»',     '¼',     '½',     '¾',     '¿' */
    X(UNK),  X('1'),  X('o'), X(0x22),  X(UNK),  X(UNK),  X(UNK), X(0x60),

    /*0xC0,   0xC1,    0xC2,    0xC3,    0xC4,    0xC5,    0xC6,    0xC7 */
    /* 'À',    'Á',     'Â',     'Ã',     'Ä',     'Å',     'Æ',     'Ç' */
    X(0x41), X(0x41), X(0x41), X(0x41), X(0x5B), X(0x0E), X(0x1C), X(0x09),

    /*0xC8,   0xC9,    0xCA,    0xCB,    0xCC,    0xCD,    0xCE,    0xCF */
    /* 'È',    'É',     'Ê',     'Ë',     'Ì',     'Í',     'Î',     'Ï' */
    X(0x45), X(0x1F), X(0x45), X(0x45), X(0x49), X(0x49), X(0x49), X(0x49),

    /*0xD0,   0xD1,    0xD2,    0xD3,    0xD4,    0xD5,    0xD6,    0xD7 */
    /* 'Ð',    'Ñ',     'Ò',     'Ó',     'Ô',     'Õ',     'Ö',     '×' */
    X(0x44), X(0x5D), X(0x4F), X(0x4F), X(0x4F), X(0x4F), X(0x5C),  X('x'),

    /*0xD8,   0xD9,    0xDA,    0xDB,    0xDC,    0xDD,    0xDE,    0xDF */
    /* 'Ø',    'Ù',     'Ú',     'Û',     'Ü',     'Ý',     'Þ',     'ß' */
    X(0x0B), X(0x55), X(0x55), X(0x55), X(0x5E), X(0x59),  X(UNK), X(0x1E),

    /*0xE0,   0xE1,    0xE2,    0xE3,    0xE4,    0xE5,    0xE6,    0xE7 */
    /* 'à',    'á',     'â',     'ã',     'ä',     'å',     'æ',     'ç' */
    X(0x7F), X(0x61), X(0x61), X(0x61), X(0x7B), X(0x0F), X(0x1D), X(0x63),

    /*0xE8,   0xE9,    0xEA,    0xEB,    0xEC,    0xED,    0xEE,    0xEF */
    /* 'è',    'é',     'ê',     'ë',     'ì',     'í',     'î',     'ï' */
    X(0x04), X(0x05), X(0x65), X(0x65), X(0x07), X(0x69), X(0x69), X(0x69),

    /*0xF0,   0xF1,    0xF2,    0xF3,    0xF4,    0xF5,    0xF6,    0xF7 */
    /* 'ð',    'ñ',     'ò',     'ó',     'ô',     'õ',     'ö',     '÷' */
    X('d'),  X(0x7D), X(0x08), X(0x6F), X(0x6F), X(0x6F), X(0x7C),  X('/'),

    /*0xF8,   0xF9,    0xFA,    0xFB,    0xFC,    0xFD,    0xFE,    0xFF */
    /* 'ø',    'ù',     'ú',     'û',     'ü',     'ý',     'þ',     'ÿ' */
    X(0x0C), X(0x06), X(0x75), X(0x75), X(0x7E), X(0x79),  X(UNK), X(0x79),

#undef UNK
#undef X
};

struct cimd_esc_table {
    uint8_t  c1, c2;
    uint16_t unicode;
};

#define CIMD_HASH(c1, c2) \
    ((uint8_t)(((c1) ^ 0x33) + ((((uint8_t)(c2)) << 4) | (((uint8_t)(c2)) >> 4))))

struct cimd_esc_table const cimd_esc_map[256] = {
#define E(C1, C2, codepoint) \
    [CIMD_HASH(C1, C2)] = { .c1 = C1, .c2 = C2, .unicode = codepoint, }

    E('O', 'a',  '@'),    E('L', '-',  0x00A3), E('Y', '-', 0x00A5 ),
    E('e', '`',  0x00E8), E('e', '\'', 0x00E9), E('u', '`', 0x00F9 ),
    E('i', '`',  0x00EC), E('o', '`',  0x00F2), E('C', ',', 0x00C7 ),
    E('O', '/',  0x00D8), E('o', '/',  0x00F8), E('A', '*', 0x00C5 ),
    E('a', '*',  0x00E5), E('g', 'd',  0x0394), E('-', '-', '_'),
    E('g', 'f',  0x03A6), E('g', 'g',  0x0393), E('g', 'l', 0x039B),
    E('g', 'o',  0x03A9), E('g', 'p',  0x03A0), E('g', 'i', 0x03A8),
    E('g', 's',  0x03A3), E('g', 't',  0x0398), E('g', 'x', 0x039E),
    E('A', 'E',  0x00C6), E('a', 'e',  0x00E6), E('s', 's', 0x00DF),
    E('E', '\'', 0x00C9), E('q', 'q',  '"'),    E('o', 'x', 0x00A4),
    E('!', '!',  0x00A1), E('A', '"',  0x00C4), E('O', '"', 0x00D6),
    E('N', '~',  0x00D1), E('U', '"',  0x00DC), E('s', 'o', 0x00A7),
    E('?', '?',  0x00BF), E('a', '"',  0x00E4), E('o', '"', 0x00F6),
    E('n', '~',  0x00F1), E('u', '"',  0x00FC), E('a', '`', 0x00E0),

#undef E
};

static inline int win1252_to_gsm7(uint8_t u8, int unknown, gsm_conv_plan_t plan)
{
    int c = __win1252_to_gsm7[u8];

    if (unlikely(c < 0))
        return unknown;
    if (plan == GSM_DEFAULT_PLAN && c > 0xff)
        return unknown;
    return c;
}

int unicode_to_gsm7(int c, int unknown, gsm_conv_plan_t plan)
{
    assert (plan != GSM_CIMD_PLAN);

    if ((unsigned)c <= 0xff) {
        return win1252_to_gsm7(c, unknown, plan);
    }

    if (plan == GSM_EXTENSION_PLAN) {
        switch (c) {
          case 0x20AC:  return 0x1B65;  /* EURO */
          default:      break;
        }
    }

    switch (c) {
      case 0x0394:  return 0x10;    /* GREEK CAPITAL LETTER DELTA */
      case 0x03A6:  return 0x12;    /* GREEK CAPITAL LETTER PHI */
      case 0x0393:  return 0x13;    /* GREEK CAPITAL LETTER GAMMA */
      case 0x039B:  return 0x14;    /* GREEK CAPITAL LETTER LAMDA */
      case 0x03A9:  return 0x15;    /* GREEK CAPITAL LETTER OMEGA */
      case 0x03A0:  return 0x16;    /* GREEK CAPITAL LETTER PI */
      case 0x03A8:  return 0x17;    /* GREEK CAPITAL LETTER PSI */
      case 0x03A3:  return 0x18;    /* GREEK CAPITAL LETTER SIGMA */
      case 0x0398:  return 0x19;    /* GREEK CAPITAL LETTER THETA */
      case 0x039E:  return 0x1A;    /* GREEK CAPITAL LETTER XI */
      default:      return unknown;
    }
}

int gsm7_to_unicode(uint8_t u8, int unknown)
{
    int c = __gsm7_to_unicode[u8];
    return unlikely(c < 0) ? unknown : c;
}

static int
cimd_special_to_unicode(const byte *p, const byte *end, const byte **out)
{
    uint8_t hash;
    struct cimd_esc_table const *esc;

    if (p + 2 > end)
        return -1;

    if (p[0] == 'X') {
        int c;

        if (p[1] != 'X')
            return -1;
        p += 2;
        if (p + 1 > end)
            return -1;

        switch (*p++) {
          case 'e':  c = 0x20AC; break;   /* EURO symbol */
          case '(':  c = '{';    break;
          case ')':  c = '}';    break;
          case '\n': c = '\n';   break;   /* should be '\f' ? */
          case '<':  c = '[';    break;
          case '>':  c = ']';    break;
          case '=':  c = '~';    break;
          case '/':  c = '\\';   break;
          case '_':
            if (p + 2 > end)
                return -1;
            if (p[0] == '!' && p[1] == '!') {
                c  = '|';
                p += 2;
                break;
            }
            if (p[0] == 'g' && p[1] == 'l') {
                c  = '^';
                p += 2;
                break;
            }
          default:
            return -1;
        }
        *out = p;
        return c;
    }

    hash = CIMD_HASH(p[0], p[1]);
    esc  = &cimd_esc_map[hash];
    if (esc->c1 == p[0] && esc->c2 == p[1] && esc->unicode) {
        *out = p + 2;
        return esc->unicode;
    }
    return -1;
}

static int cimd_to_unicode(uint8_t u8, int unknown)
{
    switch (u8) {
      case '@':
      case '$':
      case 10:
      case 13:
      case 32 ... 35:
      case 37 ... 63:
      case 65 ... 90:
      case 97 ... 122:
        return u8;
      case ']':
        return 0x00C5;
      case '}':
        return 0x00E5;
      case '[':
        return 0x00C4;
      case '\\':
        return 0x00D6;
      case '^':
        return 0x00DC;
      case '{':
        return 0x00E4;
      case '|':
        return 0x00F6;
      case '~':
        return 0x00FC;

      default:
        return unknown;
    }
}

/* Decode a hex encoded (IRA) char array into UTF-8 at end of sb */
int sb_conv_from_gsm_hex(sb_t *sb, const void *data, int slen)
{
    const char *p = data, *end = p + slen;
    sb_t orig = *sb;

    if (slen & 1)
        return -1;

    sb_grow(sb, slen / 2 + 4);

    while (p < end) {
        int c = hexdecode(p);

        p += 2;
        if (c < 0 || c > 0x7f)
            goto error;

        if (c == 0x1b) {
            if (p == end)
                goto error;
            c  = hexdecode(p);
            p += 2;
            if (c < 0 || c > 0x7f)
                goto error;
            c |= 0x80;
        }
        c = gsm7_to_unicode(c, '.');
        sb_adduc(sb, c);
    }
    return 0;

  error:
    return __sb_rewind_adds(sb, &orig);
}

int sb_conv_from_gsm_plan(sb_t *sb, const void *data, int slen, int plan)
{
    const byte *p = data, *end = p + slen;
    int nb_invalid = 0;

    sb_grow(sb, slen + 4);

    while (p < end) {
        int c = *p++;

        if (plan == GSM_CIMD_PLAN) {
            if (c == '_') {
                c = cimd_special_to_unicode(p, end, &p);
                if (unlikely(c < 0))
                    goto invalid;
            } else {
                c = cimd_to_unicode(c, '.');
            }
            goto next;
        }

        if (c & 0x80)
            goto invalid;

        if (c == 0x1b) {
            if (unlikely(plan != GSM_EXTENSION_PLAN))
                goto invalid;
            if (unlikely(p == end))
                goto invalid;
            c = *p++;
            if (c & 0x80)
                goto invalid;
            c |= 0x80;
        }
        c = gsm7_to_unicode(c, '.');
        goto next;

      invalid:
        nb_invalid++;
        c = '.';

      next:
        sb_adduc(sb, c);
    }
    return nb_invalid;
}

int gsm7_charlen(int c)
{
    c = RETHROW(unicode_to_gsm7(c, -1, GSM_EXTENSION_PLAN));
    return 1 + (c > 0xff);
}

bool sb_conv_to_gsm_isok(const void *data, int len, gsm_conv_plan_t plan)
{
    const char *p = data, *end = p + len;

    assert (plan != GSM_CIMD_PLAN);

    while (p < end) {
        int c = (unsigned char)*p++;

        if (c & 0x80) {
            int u = utf8_ngetc(p - 1, end - p + 1, &p);
            if (u >= 0)
                c = u;
        }
        if ((unsigned)c <= 0xff) {
            /* inlined path for common cases */
            if (win1252_to_gsm7(c, -1, plan) < 0)
                return false;
        } else {
            if (unicode_to_gsm7(c, -1, plan) < 0)
                return false;
        }
    }
    return true;
}

void sb_conv_to_gsm(sb_t *sb, const void *data, int len)
{
    const char *p = data, *end = p + len;

    sb_grow(sb, 2 * len + 2);
    while (p < end) {
        int c = (unsigned char)*p++;

        if (c & 0x80) {
            int u = utf8_ngetc(p - 1, end - p + 1, &p);
            if (u >= 0)
                c = u;
        }
        c = unicode_to_gsm7(c, '.', GSM_EXTENSION_PLAN);

        if (c > 0xff) {
            sb_addc(sb, c >> 8);
        }
        sb_addc(sb, c);
    }
}

void sb_conv_to_gsm_hex(sb_t *sb, const void *data, int len)
{
    const char *p = data, *end = p + len;

    sb_grow(sb, 4 * len + 4);
    while (p < end) {
        int c = (unsigned char)*p++;

        if (c & 0x80) {
            int u = utf8_ngetc(p - 1, end - p + 1, &p);
            if (u >= 0)
                c = u;
        }
        c = unicode_to_gsm7(c, '.', GSM_EXTENSION_PLAN);

        if (c > 0xff) {
            sb_addc(sb, __str_digits_upper[(c >> 12) & 0xf]);
            sb_addc(sb, __str_digits_upper[(c >>  8) & 0xf]);
        }
        sb_addc(sb, __str_digits_upper[(c >> 4) & 0xf]);
        sb_addc(sb, __str_digits_upper[(c >> 0) & 0xf]);
    }
}

/*
 * write up to 8 septets in the 56 least significant bits of "pack"
 * XXX: the tragic truth about septets is that writing 8 septets means we pass
 *      `7` as %len value, the same value we use for 7 septets, except that
 *      for the latter case, it writes 7 bits of padding.
 */
static void put_gsm_pack(sb_t *out, uint64_t pack, int len)
{
#if __BYTE_ORDER == __BIG_ENDIAN
    pack = bswap64(pack);
#endif
    sb_add(out, &pack, len);
}

static uint64_t get_gsm7_pack(const void *src, int len)
{
    uint64_t pack = 0;

    memcpy(&pack, src, len);
#if __BYTE_ORDER == __BIG_ENDIAN
    pack = bswap64(pack);
#endif
    return pack;
}

/*
 * gsm_start points to the first octet of out->data holding 7-bits packed GSM
 * data. sb_conv_to_gsm7 is not able to be restartable, it assumes the
 * octets between position %gsm_start and sb->len are 8bit aligned (UDH) and
 * pads with 0 bits up to the next septet boundary.
 *
 * If we mean to have a restartable API, we need to know the current amount of
 * septets written since %gsm_start since we have no way to know if when we
 * have for example gsm->len - gsm_start == 7 if 7 or 8 septets are written.
 *
 * For us here, we know that if gsm->len - gsm_start is 7, 7 _octets_ of UDH
 * are written and no padding is needed to be aligned on the next septet
 * boundary.
 */
int sb_conv_to_gsm7(sb_t *out, int gsm_start, const char *utf8, int unknown,
                    gsm_conv_plan_t plan)
{
    uint64_t pack = 0;
    int septet = (out->len - gsm_start) % 7;

    if (septet) {
        pack = get_gsm7_pack(out->data + out->len - septet, septet);
        sb_shrink(out, septet);
        septet++;
    }

    for (;;) {
        int c = utf8_getc(utf8, &utf8);

        if (c <= 0) {
            int len = 8 * (out->len - gsm_start) / 7;
            put_gsm_pack(out, pack, septet);
            return c < 0 ? c : len + septet;
        }

        c = RETHROW(unicode_to_gsm7(c, unknown, plan));
        if (c > 0xff) {
            pack |= ((uint64_t)(c >> 8) << (7 * septet));
            if (++septet == 8) {
                put_gsm_pack(out, pack, 7);
                septet = 0;
                pack = 0;
            }
        }
        pack |= ((uint64_t)(c & 0x7f) << (7 * septet));
        if (++septet == 8) {
            put_gsm_pack(out, pack, 7);
            septet = 0;
            pack = 0;
        }
    }
}

static int decode_gsm7_pack(sb_t *out, uint64_t pack, int nbchars, int c)
{
    for (int i = 0; i < nbchars; i++) {
        c |= pack & 0x7f;
        pack >>= 7;
        if (c == 0x1b) {
            c = 0x80;
        } else {
            c  = gsm7_to_unicode(c, '.');
            sb_adduc(out, c);
            c  = 0;
        }
    }
    return c;
}

/*
 * %gsmlen is the number of septets in %_src.
 *
 * The caller has to ensure %udhlen (in octets) and %gsmlen and the actual
 * size of the %_src buffer make sense:
 *   IOW that %udhlen is smaller or equal to the size of %_src,
 *   which in turn must be equal to (7 * %gsmlen + 7) / 8.
 */
int sb_conv_from_gsm7(sb_t *out, const void *_src, int gsmlen, int udhlen)
{
    const char *src = _src;
    int c = 0;

    sb_grow(out, (gsmlen - udhlen) * 2);

    src    += udhlen - (udhlen % 7);
    gsmlen -= 8 * (udhlen / 7);
    if (udhlen % 7) {
        /* udh overlaps up to the next (hence +1) septet boundary included */
        int overlap = (udhlen % 7) + 1;
        uint64_t pack;

        if (gsmlen >= 8) {
            pack = get_gsm7_pack(src, 7) >> (7 * overlap);
            c    = decode_gsm7_pack(out, pack, 8 - overlap, c);
        } else {
            pack = get_gsm7_pack(src, gsmlen) >> (7 * overlap);
            c    = decode_gsm7_pack(out, pack, gsmlen - overlap, c);
            goto done;
        }
        src    += 7;
        gsmlen -= 8;
    }

    for (; gsmlen >= 8; src += 7, gsmlen -= 8) {
        c = decode_gsm7_pack(out, get_gsm7_pack(src, 7), 8, c);
    }
    if (gsmlen) {
        c = decode_gsm7_pack(out, get_gsm7_pack(src, gsmlen), gsmlen, c);
    }
  done:
    return c ? -1 : 0;
}

/* {{{ Convertion checks */

Z_GROUP_EXPORT(conv)
{
    sb_t tmp, out;
    lstr_t default_tab = LSTR_IMMED(
        "\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0a\x0b\x0c\x0d\x0e\x0f"
        "\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19\x1a\x1c\x1d\x1e\x1f"
        "\x20\x21\x22\x23\x24\x25\x26\x27\x28\x29\x2a\x2b\x2c\x2d\x2e\x2f"
        "\x30\x31\x32\x33\x34\x35\x36\x37\x38\x39\x3a\x3b\x3c\x3d\x3e\x3f"
        "\x40\x41\x42\x43\x44\x45\x46\x47\x48\x49\x4a\x4b\x4c\x4d\x4e\x4f"
        "\x50\x51\x52\x53\x54\x55\x56\x57\x58\x59\x5a\x5b\x5c\x5d\x5e\x5f"
        "\x60\x61\x62\x63\x64\x65\x66\x67\x68\x69\x6a\x6b\x6c\x6d\x6e\x6f"
        "\x70\x71\x72\x73\x74\x75\x76\x77\x78\x79\x7a\x7b\x7c\x7d\x7e\x7f");
    lstr_t extended_tab = LSTR_IMMED(
        "\x1b\x14\x1b\x28\x1b\x29\x1b\x2f\x1b\x3c\x1b\x3d\x1b\x3e\x1b\x40\x1b\x65");

    Z_TEST(sb_conv_gsm, "sb conv from/to gsm") {
#define TL(input, expected, desc)       \
        ({  lstr_t in   = input;                                             \
            lstr_t exp  = expected;                                          \
            sb_reset(&tmp);                                                  \
            sb_reset(&out);                                                  \
            sb_conv_from_gsm(&tmp, in.s, in.len);                            \
            sb_conv_to_gsm(&out, tmp.data, tmp.len);                         \
            Z_ASSERT_LSTREQUAL(exp, LSTR_SB_V(&out), desc);                  \
        })
#define TLHEX(input, expected, desc)    \
        ({  lstr_t in   = input;                                             \
            lstr_t exp  = expected;                                          \
            SB_1k(in_hex); SB_1k(exp_hex);                                   \
            sb_add_hex(&in_hex, in.s, in.len);                               \
            sb_add_hex(&exp_hex, exp.s, exp.len);                            \
            sb_reset(&tmp);                                                  \
            sb_reset(&out);                                                  \
            sb_conv_from_gsm_hex(&tmp, in_hex.data, in_hex.len);             \
            sb_conv_to_gsm_hex(&out, tmp.data, tmp.len);                     \
            Z_ASSERT_LSTREQUAL(LSTR_SB_V(&exp_hex), LSTR_SB_V(&out), desc);  \
        })

        sb_init(&tmp);
        sb_init(&out);

        /* behavior in 2012.4 */
        TL(LSTR_IMMED("\x80\x20\x81\x20\x82"),
           LSTR_IMMED("\x2e\x20\x2e\x20\x2e"),
           "conversion with invalid characters");

        for (int i = 0; i < default_tab.len; i++) {
            TL(LSTR_INIT_V(default_tab.s, i),
               LSTR_INIT_V(default_tab.s, i),
               "test default table with various lengths");
            TLHEX(LSTR_INIT_V(default_tab.s, i),
                  LSTR_INIT_V(default_tab.s, i),
                  "test default table with various lengths (hex)");
        }
        for (int i = 0; i < extended_tab.len; i += 2) {
            TL(LSTR_INIT_V(extended_tab.s, i),
               LSTR_INIT_V(extended_tab.s, i),
               "test extension table with various lengths");
            TLHEX(LSTR_INIT_V(extended_tab.s, i),
                  LSTR_INIT_V(extended_tab.s, i),
                  "test extension table with various lengths (hex)");
        }

        {
            lstr_t str = LSTR_IMMED(
                "coucou random:\"jk6q?#hU*1/m.VVteU[i4S|\\\"@>'wrTFuV[Csrvi<^|%/1>|"
                "'9kpfG76aY5)gWN!+1D8aj-j|)'3'\"ZO:F#XL7n2=DpIEtU5%H8UICK.F\"&2HBOi6ZLZ[|ptN-z");
            sb_wipe(&tmp);
            sb_reset(&out);
            sb_conv_to_gsm_hex(&tmp, str.s, str.len);
            sb_conv_from_gsm_hex(&out, tmp.data, tmp.len);
            Z_ASSERT_LSTREQUAL(str, LSTR_SB_V(&out), "emi teaser crash");
        }

        sb_wipe(&tmp);
        sb_wipe(&out);

#undef TL
#undef TLHEX
    } Z_TEST_END

    Z_TEST(sb_conv_cimd, "sb conv from cimd") {
        SB_1k(sb);

#define T(input, expected, description)      \
        ({  lstr_t exp = LSTR_IMMED(expected);                               \
            lstr_t in  = LSTR_IMMED(input);                                  \
                                                                             \
            sb_reset(&out);                                                  \
            sb_conv_from_gsm_plan(&out, in.s, in.len, GSM_CIMD_PLAN);        \
            Z_ASSERT_LSTREQUAL(exp, LSTR_SB_V(&out), description);           \
        })

        sb_init(&out);

        /* Example 22 from CIMD spec 8.0 (@£$¥èéùìòç) */
        T("_Oa_L-$_Y-_e`_e'_u`_i`_o`_C,",
          "\x40\xc2\xa3\x24\xc2\xa5\xc3\xa8\xc3\xa9\xc3\xb9\xc3\xac\xc3\xb2"
          "\xc3\x87",
          "Default character conversion over 7-bit link");

        /* A few characters can be encoded either using one latin1 char or a
         * special combination of ascii char */
        T("_Oa_A*_a*_qq_A\"_O\"_U\"_a\"_o\"_u\"",
          "\x40\xC3\x85\xC3\xA5\x22\xC3\x84\xC3\x96\xC3\x9C\xC3\xA4\xC3"
          "\xB6\xC3\xBC",
          "special combination");
        T("@]}\"[\\^{|~",
          "\x40\xC3\x85\xC3\xA5\x22\xC3\x84\xC3\x96\xC3\x9C\xC3\xA4\xC3"
          "\xB6\xC3\xBC",
          "iso-latin");

        for (int c = 1; c < 256; c++) {
            sb_addc(&sb, c);
        }
        sb_reset(&out);
        Z_ASSERT_N(sb_conv_from_gsm_plan(&out, sb.data, sb.len,
                                         GSM_DEFAULT_PLAN));
        sb_reset(&out);
        Z_ASSERT_N(sb_conv_from_gsm_plan(&out, sb.data, sb.len,
                                         GSM_CIMD_PLAN));

        sb_wipe(&out);

#undef T
    } Z_TEST_END

    Z_TEST(sb_conv_to_gsm_isok, "sb_conv_to_gsm_isok") {
#define T(input, res, plan, description)      \
        ({  lstr_t in  = LSTR_IMMED(input);                                 \
            Z_ASSERT(res == sb_conv_to_gsm_isok(in.s, in.len, plan),        \
                     description);                                          \
        })

        T("\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0a\x0b\x0c\x0d\x0e\x0f",
          false, GSM_DEFAULT_PLAN,
          "utf8 which cannot be mapped to gsm7");

        T("\x40\x41\x42\x43\x44\x45\x46\x47\x48\x49\x4a\x4b\x4c\x4d\x4e\x4f"
          /* éèêàâç */
          "\xc3\xa9\xc3\xa8\xc3\xaa\xc3\xa0\xc3\xa2\xc3\xa7",
          true, GSM_DEFAULT_PLAN,
          "utf8 which can be mapped to gsm7");

        T("\xe2\x82\xac", false, GSM_DEFAULT_PLAN,
          "euro cannot be mapped with default table");
        T("\xe2\x82\xac", true, GSM_EXTENSION_PLAN,
          "euro can be mapped with extension table");

#undef T
    } Z_TEST_END
} Z_GROUP_END

/* }}} */
