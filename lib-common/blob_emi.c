/**************************************************************************/
/*                                                                        */
/*  Copyright (C) 2004-2006 INTERSEC                                      */
/*                                                                        */
/*  Should you receive a copy of this source code, you must check you     */
/*  have a proper, written authorization of INTERSEC to hold it. If you   */
/*  don't have such an authorization, you must DELETE all source code     */
/*  files in your possession, and inform INTERSEC of the fact you obtain  */
/*  these files. Should you not comply to these terms, you can be         */
/*  prosecuted in the extent permitted by applicable law.                 */
/*                                                                        */
/**************************************************************************/

#include "macros.h"
#include "blob.h"

/*---------------- IRA Conversion ----------------*/

static unsigned short const win1252_to_gsm7[] = {
#define UNK  '.'
#define _

    /* 0x00,   0x01,   0x02,   0x03,   0x04,   0x05,   0x06,   0x07,  */
    /* 0x00,   0x01,   0x02,   0x03,   0x04,   0x05,   0x06,   0x07,  */
    _   UNK,    UNK,    UNK,    UNK,    UNK,    UNK,    UNK,    UNK,   

    /* 0x08,   0x09,   0x0A,   0x0B,   0x0C,   0x0D,   0x0E,   0x0F,  */
    /* '\b',   '\t',   '\n',   0x0B,   '\f',   '\r',   0x0E,   0x0F,  */
    _   UNK,   0x20,   0x0A,    UNK,  0x1B0A,  0x0D,    UNK,    UNK,

    /* 0x10,   0x11,   0x12,   0x13,   0x14,   0x15,   0x16,   0x17,  */
    /* 0x10,   0x11,   0x12,   0x13,   0x14,   0x15,   0x16,   0x17,  */
    _   UNK,    UNK,    UNK,    UNK,    UNK,    UNK,    UNK,    UNK,   

    /* 0x18,   0x19,   0x1A,   0x1B,   0x1C,   0x1D,   0x1E,   0x1F,  */
    /* 0x18,   0x19,   0x1A,   0x1B,   0x1C,   0x1D,   0x1E,   0x1F,  */
    _   UNK,    UNK,    UNK,    UNK,    UNK,    UNK,    UNK,    UNK,

    /* 0x20,   0x21,   0x22,   0x23,   0x24,   0x25,   0x26,   0x27,  */
    /*  ' ',    '!',    '"',    '#',    '$',    '%',    '&',   '\'',  */
    _  0x20,   0x21,   0x22,   0x23,   0x02,   0x25,   0x26,   0x27,   

    /* 0x28,   0x29,   0x2A,   0x2B,   0x2C,   0x2D,   0x2E,   0x2F,  */
    /*  '(',    ')',    '*',    '+',    ',',    '-',    '.',    '/',  */
    _  0x28,   0x29,   0x2A,   0x2B,   0x2C,   0x2D,   0x2E,   0x2F,

    /* 0x30,   0x31,   0x32,   0x33,   0x34,   0x35,   0x36,   0x37,  */
    /*  '0',    '1',    '2',    '3',    '4',    '5',    '6',    '7',  */
    _  0x30,   0x31,   0x32,   0x33,   0x34,   0x35,   0x36,   0x37,   

    /* 0x38,   0x39,   0x3A,   0x3B,   0x3C,   0x3D,   0x3E,   0x3F,  */
    /*  '8',    '9',    ':',    ';',    '<',    '=',    '>',    '?',  */
    _  0x38,   0x39,   0x3A,   0x3B,   0x3C,   0x3D,   0x3E,   0x3F,

    /* 0x40,   0x41,   0x42,   0x43,   0x44,   0x45,   0x46,   0x47,  */
    /*  '@',    'A',    'B',    'C',    'D',    'E',    'F',    'G',  */
    _  0x00,   0x41,   0x42,   0x43,   0x44,   0x45,   0x46,   0x47,   

    /* 0x48,   0x49,   0x4A,   0x4B,   0x4C,   0x4D,   0x4E,   0x4F,  */
    /*  'H',    'I',    'J',    'K',    'L',    'M',    'N',    'O',  */
    _  0x48,   0x49,   0x4A,   0x4B,   0x4C,   0x4D,   0x4E,   0x4F,

    /* 0x50,   0x51,   0x52,   0x53,   0x54,   0x55,   0x56,   0x57,  */
    /*  'P',    'Q',    'R',    'S',    'T',    'U',    'V',    'W',  */
    _  0x50,   0x51,   0x52,   0x53,   0x54,   0x55,   0x56,   0x57,   

    /* 0x58,   0x59,   0x5A,   0x5B,   0x5C,   0x5D,   0x5E,   0x5F,  */
    /*  'X',    'Y',    'Z',    '[',   '\\',    ']',    '^',    '_',  */
    _  0x58,   0x59,   0x5A,  0x1B3C, 0x1B2F, 0x1B3E, 0x1B14,  0x11,

    /* 0x60,   0x61,   0x62,   0x63,   0x64,   0x65,   0x66,   0x67,  */
    /*  '`',    'a',    'b',    'c',    'd',    'e',    'f',    'g',  */
    _  0x27,   0x61,   0x62,   0x63,   0x64,   0x65,   0x66,   0x67,   

    /* 0x68,   0x69,   0x6A,   0x6B,   0x6C,   0x6D,   0x6E,   0x6F,  */
    /*  'h',    'i',    'j',    'k',    'l',    'm',    'n',    'o',  */
    _  0x68,   0x69,   0x6A,   0x6B,   0x6C,   0x6D,   0x6E,   0x6F,

    /* 0x70,   0x71,   0x72,   0x73,   0x74,   0x75,   0x76,   0x77,  */
    /*  'p',    'q',    'r',    's',    't',    'u',    'v',    'w',  */
    _  0x70,   0x71,   0x72,   0x73,   0x74,   0x75,   0x76,   0x77,   

    /* 0x78,   0x79,   0x7A,   0x7B,   0x7C,   0x7D,   0x7E,   0x7F,  */
    /*  'x',    'y',    'z',    '{',    '|',    '}',    '~',   0x7F,  */
    _  0x78,   0x79,   0x7A,  0x1B28, 0x1B40, 0x1B29, 0x1B3D,   UNK,

    /* 0x80,   0x81,   0x82,   0x83,   0x84,   0x85,   0x86,   0x87,  */
    /*  EUR,   0x81,   0x82,   0x83,   0x84,   0x85,   0x86,   0x87,  */
    _ 0x1B65,   UNK,    ',',    'f',    '"',    UNK,    UNK,    UNK, 

    /* 0x88,   0x89,   0x8A,   0x8B,   0x8C,   0x8D,   0x8E,   0x8F,  */
    /* 0x88,   0x89,   0x8A,   0x8B,   0x8C,   0x8D,   0x8E,   0x8F,  */
    _ 0x1B14,   UNK,    'S',    '<',  0x4F45,   UNK,    'Z',    UNK,

    /* 0x90,   0x91,   0x92,   0x93,   0x94,   0x95,   0x96,   0x97,  */
    /* 0x90,   0x91,   0x92,   0x93,   0x94,   0x95,   0x96,   0x97,  */
    _   UNK,   0x27,   0x27,   0x22,    0x22,   UNK,    '-',    '-',  

    /* 0x98,   0x99,   0x9A,   0x9B,   0x9C,   0x9D,   0x9E,   0x9F,  */
    /* 0x98,   0x99,   0x9A,   0x9B,   0x9C,   0x9D,   0x9E,   0x9F,  */
    _ 0x1B3D, 0x746D,   's',    '>',  0x6F65,   UNK,    'z',    'Y',

    /* 0xA0,   0xA1,   0xA2,   0xA3,   0xA4,   0xA5,   0xA6,   0xA7,  */
    /*  ' ',    '¡',    '¢',    '£',    '¤',    '¥',    '¦',    '§',  */
    _  0x20,   0x40,    'c',   0x01,  0x1b65,  0x03,  0x1B40,  0x5F,   

    /* 0xA8,   0xA9,   0xAA,   0xAB,   0xAC,   0xAD,   0xAE,   0xAF,  */
    /*  '¨',    '©',    'ª',    '«',    '¬',    '­',    '®',    '¯',  */
    _  0x22,    'c',    'a',   0x22,    '-',   0x2D,    'r',    '-',

    /* 0xB0,   0xB1,   0xB2,   0xB3,   0xB4,   0xB5,   0xB6,   0xB7,  */
    /*  '°',    '±',    '²',    '³',    '´',    'µ',    '¶',    '·',  */
    _   UNK,    UNK,    '2',    '3',   0x27,    'u',    UNK,    '.',   

    /* 0xB8,   0xB9,   0xBA,   0xBB,   0xBC,   0xBD,   0xBE,   0xBF,  */
    /*  '¸',    '¹',    'º',    '»',    '¼',    '½',    '¾',    '¿',  */
    _   UNK,    '1',    'o',   0x22,    UNK,    UNK,    UNK,   0x60,

    /* 0xC0,   0xC1,   0xC2,   0xC3,   0xC4,   0xC5,   0xC6,   0xC7,  */
    /*  'À',    'Á',    'Â',    'Ã',    'Ä',    'Å',    'Æ',    'Ç',  */
    _  0x41,   0x41,   0x41,   0x41,   0x5B,   0x0E,   0x1C,   0x09,   

    /* 0xC8,   0xC9,   0xCA,   0xCB,   0xCC,   0xCD,   0xCE,   0xCF,  */
    /*  'È',    'É',    'Ê',    'Ë',    'Ì',    'Í',    'Î',    'Ï',  */
    _  0x45,   0x1F,   0x45,   0x45,   0x49,   0x49,   0x49,   0x49,

    /* 0xD0,   0xD1,   0xD2,   0xD3,   0xD4,   0xD5,   0xD6,   0xD7,  */
    /*  'Ð',    'Ñ',    'Ò',    'Ó',    'Ô',    'Õ',    'Ö',    '×',  */
    _  0x44,   0x5D,   0x4F,   0x4F,   0x4F,   0x4F,   0x5C,    'x',   

    /* 0xD8,   0xD9,   0xDA,   0xDB,   0xDC,   0xDD,   0xDE,   0xDF,  */
    /*  'Ø',    'Ù',    'Ú',    'Û',    'Ü',    'Ý',    'Þ',    'ß',  */
    _  0x0B,   0x55,   0x55,   0x55,   0x5E,   0x59,    UNK,   0x1E,

    /* 0xE0,   0xE1,   0xE2,   0xE3,   0xE4,   0xE5,   0xE6,   0xE7,  */
    /*  'à',    'á',    'â',    'ã',    'ä',    'å',    'æ',    'ç',  */
    _  0x7F,   0x61,   0x61,   0x61,   0x7B,   0x0F,   0x1D,   0x63,   

    /* 0xE8,   0xE9,   0xEA,   0xEB,   0xEC,   0xED,   0xEE,   0xEF,  */
    /*  'è',    'é',    'ê',    'ë',    'ì',    'í',    'î',    'ï',  */
    _  0x04,   0x05,   0x65,   0x65,   0x07,   0x69,   0x69,   0x69,

    /* 0xF0,   0xF1,   0xF2,   0xF3,   0xF4,   0xF5,   0xF6,   0xF7,  */
    /*  'ð',    'ñ',    'ò',    'ó',    'ô',    'õ',    'ö',    '÷',  */
    _   'd',   0x7D,   0x08,   0x6F,   0x6F,   0x6F,   0x7C,    '/',   

    /* 0xF8,   0xF9,   0xFA,   0xFB,   0xFC,   0xFD,   0xFE,   0xFF,  */
    /*  'ø',    'ù',    'ú',    'û',    'ü',    'ý',    'þ',    'ÿ',  */
    _  0x0C,   0x06,   0x75,   0x75,   0x7E,   0x79,    UNK,   0x79,

#undef _
#undef UNK
};

static char const __str_xdigits_upper[16] = "0123456789ABCDEF";

int blob_append_ira(blob_t *dst, const byte *src, ssize_t len)
{
    int pos;
    const byte *end;
    byte *data;

    pos = dst->len;
    /* Characters may expand to 2 bytes each before hex conversion */
    blob_extend(dst, len * 4);
    data = dst->data + pos;
    end = src + len;
    while (src < end) {
        int c = *src++;

        if (c & 0x80) {
            /* Check for proper UTF8 encoded code point */
            if (c >= 0xC0) {
                if (c < 0xE0) {
                    if (src < end && (src[0] & 0xC0) == 0x80) {
                        /* 0x0080 .. 0x0FFF */
                        c = ((c & 0x3F) << 6) | (*src & 0x3F);
                        src += 1;
                        goto hasunicode;
                    }
                } else
                if (c < 0xF0) {
                    if (src + 1 < end
                    &&  (src[0] & 0xC0) == 0x80
                    &&  (src[1] & 0xC0) == 0x80) {
                        /* 0x1000 .. 0x1FFFF */
                        c = ((c & 0x1F) << 12) | ((src[0] & 0x3F) << 6) |
                              (src[1] & 0x3F);
                        src += 2;
                    hasunicode:
                        switch (c) {
                        case 0x20AC:    /* EURO */
                            c = 0x1B65;
                            break;
                        case 0x0394:    /* GREEK CAPITAL LETTER DELTA */
                            c = 0x10;
                            break;
                        case 0x03A6:    /* GREEK CAPITAL LETTER PHI */
                            c = 0x12;
                            break;
                        case 0x0393:    /* GREEK CAPITAL LETTER GAMMA */
                            c = 0x13;
                            break;
                        case 0x039B:    /* GREEK CAPITAL LETTER LAMDA */
                            c = 0x14;
                            break;
                        case 0x03A9:    /* GREEK CAPITAL LETTER OMEGA */
                            c = 0x15;
                            break;
                        case 0x03A0:    /* GREEK CAPITAL LETTER PI */
                            c = 0x16;
                            break;
                        case 0x03A8:    /* GREEK CAPITAL LETTER PSI */
                            c = 0x17;
                            break;
                        case 0x03A3:    /* GREEK CAPITAL LETTER SIGMA */
                            c = 0x18;
                            break;
                        case 0x0398:    /* GREEK CAPITAL LETTER THETA */
                            c = 0x19;
                            break;
                        case 0x039E:    /* GREEK CAPITAL LETTER XI */
                            c = 0x1A;
                            break;
                        default:
                            if (c > 0xFF) {
                                c = '?';
                            } else {
                                c = win1252_to_gsm7[c];
                            }
                            break;
                        }
                        goto hasc;
                    }
                }
            }
        }
        c = win1252_to_gsm7[c];

    hasc:
        if (c > 0xFF) {
            data[0] = __str_xdigits_upper[(c >> 12) & 0x0F];
            data[1] = __str_xdigits_upper[(c >> 8) & 0x0F];
            data += 2;
        }
        data[0] = __str_xdigits_upper[(c >> 4) & 0x0F];
        data[1] = __str_xdigits_upper[(c >> 0) & 0x0F];
        data += 2;
    }
    dst->len = data - dst->data;
    return 0;
}

