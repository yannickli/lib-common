/**************************************************************************/
/*                                                                        */
/*  Copyright (C) 2004-2008 INTERSEC SAS                                  */
/*                                                                        */
/*  Should you receive a copy of this source code, you must check you     */
/*  have a proper, written authorization of INTERSEC to hold it. If you   */
/*  don't have such an authorization, you must DELETE all source code     */
/*  files in your possession, and inform INTERSEC of the fact you obtain  */
/*  these files. Should you not comply to these terms, you can be         */
/*  prosecuted in the extent permitted by applicable law.                 */
/*                                                                        */
/**************************************************************************/

#include "str-conv.h"
#include "blob.h"
#include "str.h"

/*---------------- IRA Conversion ----------------*/

/* Convert to subset of ISO-8859-15: the only difference with
 * ISO-8859-1 (and thus from unicode) should be the EURO symbol located
 * at 0xA4 instead of U+20AC
 */
static int const gsm7_to_iso8859_15[] = {

#define X(x)     (x)
#define UNK      '.'
    /* 0x00,   0x01,   0x02,   0x03,   0x04,   0x05,   0x06,   0x07,    */
       X(0x40),X(0xA3),X(0x24),X(0xA5),X(0xE8),X(0xE9),X(0xFA),X(0xEC),
    /* 0x08,   0x09,   0x0A,   0x0B,   0x0C,   0x0D,   0x0E,   0x0F,    */
       X(0xF2),X(0xC7),X(0x0A),X(0xD8),X(0xF8),X(0x0D),X(0xC5),X(0xE5),
    /* 0x10,   0x11,   0x12,   0x13,   0x14,   0x15,   0x16,   0x17,    */
       UNK,    X(0x5F),UNK,    UNK,    UNK,    UNK,    UNK,    UNK,
    /* 0x18,   0x19,   0x1A,   0x1B,   0x1C,   0x1D,   0x1E,   0x1F,    */
       UNK,    UNK,    UNK,    UNK,    X(0xC6),X(0xE6),X(0xDF),X(0xCA),
    /* 0x20,   0x21,   0x22,   0x23,   0x24,   0x25,   0x26,   0x27,    */
       X(0x20),X(0x21),X(0x22),X(0x23),UNK,    X(0x25),X(0x26),X(0x27),
    /* 0x28,   0x29,   0x2A,   0x2B,   0x2C,   0x2D,   0x2E,   0x2F,    */
       X(0x28),X(0x29),X(0x2A),X(0x2B),X(0x2C),X(0x2D),X(0x2E),X(0x2F),
    /* 0x30,   0x31,   0x32,   0x33,   0x34,   0x35,   0x36,   0x37,    */
       X(0x30),X(0x31),X(0x32),X(0x33),X(0x34),X(0x35),X(0x36),X(0x37),
    /* 0x38,   0x39,   0x3A,   0x3B,   0x3C,   0x3D,   0x3E,   0x3F,    */
       X(0x38),X(0x39),X(0x3A),X(0x3B),X(0x3C),X(0x3D),X(0x3E),X(0x3F),
    /* 0x40,   0x41,   0x42,   0x43,   0x44,   0x45,   0x46,   0x47,    */
       X(0xA1),X(0x41),X(0x42),X(0x43),X(0x44),X(0x45),X(0x46),X(0x47),
    /* 0x48,   0x49,   0x4A,   0x4B,   0x4C,   0x4D,   0x4E,   0x4F,    */
       X(0x48),X(0x49),X(0x4A),X(0x4B),X(0x4C),X(0x4D),X(0x4E),X(0x4F),
    /* 0x50,   0x51,   0x52,   0x53,   0x54,   0x55,   0x56,   0x57,    */
       X(0x50),X(0x51),X(0x52),X(0x53),X(0x54),X(0x55),X(0x56),X(0x57),
    /* 0x58,   0x59,   0x5A,   0x5B,   0x5C,   0x5D,   0x5E,   0x5F,    */
       X(0x58),X(0x59),X(0x5A),X(0xC4),X(0xD6),X(0xD1),X(0xDC),X(0xA7),
    /* 0x60,   0x61,   0x62,   0x63,   0x64,   0x65,   0x66,   0x67,    */
       X(0xBF),X(0x61),X(0x62),X(0x63),X(0x64),X(0x65),X(0x66),X(0x67),
    /* 0x68,   0x69,   0x6A,   0x6B,   0x6C,   0x6D,   0x6E,   0x6F,    */
       X(0x68),X(0x69),X(0x6A),X(0x6B),X(0x6C),X(0x6D),X(0x6E),X(0x6F),
    /* 0x70,   0x71,   0x72,   0x73,   0x74,   0x75,   0x76,   0x77,    */
       X(0x70),X(0x71),X(0x72),X(0x73),X(0x74),X(0x75),X(0x76),X(0x77),
    /* 0x78,   0x79,   0x7A,   0x7B,   0x7C,   0x7D,   0x7E,   0x7F,    */
       X(0x78),X(0x79),X(0x7A),X(0xE4),X(0xF6),X(0xF1),X(0xFC),X(0xE0),
    /* 0x80,   0x81,   0x82,   0x83,   0x84,   0x85,   0x86,   0x87,    */
       UNK,    UNK,    UNK,    UNK,    UNK,    UNK,    UNK,    UNK,
    /* 0x88,   0x89,   0x8A,   0x8B,   0x8C,   0x8D,   0x8E,   0x8F,    */
       UNK,    UNK,    UNK,    UNK,    UNK,    UNK,    UNK,    UNK,
    /* 0x90,   0x91,   0x92,   0x93,   0x94,   0x95,   0x96,   0x97,    */
       UNK,    UNK,    UNK,    UNK,    UNK,    UNK,    UNK,    UNK,
    /* 0x98,   0x99,   0x9A,   0x9B,   0x9C,   0x9D,   0x9E,   0x9F,    */
       UNK,    UNK,    UNK,    UNK,    UNK,    UNK,    UNK,    UNK,
    /* 0xA0,   0xA1,   0xA2,   0xA3,   0xA4,   0xA5,   0xA6,   0xA7,    */
       UNK,    UNK,    UNK,    UNK,    UNK,    UNK,    UNK,    UNK,
    /* 0xA8,   0xA9,   0xAA,   0xAB,   0xAC,   0xAD,   0xAE,   0xAF,    */
       UNK,    UNK,    UNK,    UNK,    UNK,    UNK,    UNK,    UNK,
    /* 0xB0,   0xB1,   0xB2,   0xB3,   0xB4,   0xB5,   0xB6,   0xB7,    */
       UNK,    UNK,    UNK,    UNK,    UNK,    UNK,    UNK,    UNK,
    /* 0xB8,   0xB9,   0xBA,   0xBB,   0xBC,   0xBD,   0xBE,   0xBF,    */
       UNK,    UNK,    UNK,    UNK,    UNK,    UNK,    UNK,    UNK,
    /* 0xC0,   0xC1,   0xC2,   0xC3,   0xC4,   0xC5,   0xC6,   0xC7,    */
       UNK,    UNK,    UNK,    UNK,    UNK,    UNK,    UNK,    UNK,
    /* 0xC8,   0xC9,   0xCA,   0xCB,   0xCC,   0xCD,   0xCE,   0xCF,    */
       UNK,    UNK,    UNK,    UNK,    UNK,    UNK,    UNK,    UNK,
    /* 0xD0,   0xD1,   0xD2,   0xD3,   0xD4,   0xD5,   0xD6,   0xD7,    */
       UNK,    UNK,    UNK,    UNK,    UNK,    UNK,    UNK,    UNK,
    /* 0xD8,   0xD9,   0xDA,   0xDB,   0xDC,   0xDD,   0xDE,   0xDF,    */
       UNK,    UNK,    UNK,    UNK,    UNK,    UNK,    UNK,    UNK,
    /* 0xE0,   0xE1,   0xE2,   0xE3,   0xE4,   0xE5,   0xE6,   0xE7,    */
       UNK,    UNK,    UNK,    UNK,    UNK,    UNK,    UNK,    UNK,
    /* 0xE8,   0xE9,   0xEA,   0xEB,   0xEC,   0xED,   0xEE,   0xEF,    */
       UNK,    UNK,    UNK,    UNK,    UNK,    UNK,    UNK,    UNK,
    /* 0xF0,   0xF1,   0xF2,   0xF3,   0xF4,   0xF5,   0xF6,   0xF7,    */
       UNK,    UNK,    UNK,    UNK,    UNK,    UNK,    UNK,    UNK,
    /* 0xF8,   0xF9,   0xFA,   0xFB,   0xFC,   0xFD,   0xFE,   0xFF,    */
       UNK,    UNK,    UNK,    UNK,    UNK,    UNK,    UNK,    UNK,

    /* 0x1B00, 0x1B01, 0x1B02, 0x1B03, 0x1B04, 0x1B05, 0x1B06, 0x1B07,  */
       UNK,    UNK,    UNK,    UNK,    UNK,    UNK,    UNK,    UNK,
    /* 0x1B08, 0x1B09, 0x1B0A, 0x1B0B, 0x1B0C, 0x1B0D, 0x1B0E, 0x1B0F,  */
       UNK,    UNK,    X(0x0C),UNK,    UNK,    UNK,    UNK,    UNK,
    /* 0x1B10, 0x1B11, 0x1B12, 0x1B13, 0x1B14, 0x1B15, 0x1B16, 0x1B17,  */
       UNK,    UNK,    UNK,    UNK,    X(0x5E),UNK,    UNK,    UNK,
    /* 0x1B18, 0x1B19, 0x1B1A, 0x1B1B, 0x1B1C, 0x1B1D, 0x1B1E, 0x1B1F,  */
       UNK,    UNK,    UNK,    UNK,    UNK,    UNK,    UNK,    UNK,
    /* 0x1B20, 0x1B21, 0x1B22, 0x1B23, 0x1B24, 0x1B25, 0x1B26, 0x1B27,  */
       UNK,    UNK,    UNK,    UNK,    UNK,    UNK,    UNK,    UNK,
    /* 0x1B28, 0x1B29, 0x1B2A, 0x1B2B, 0x1B2C, 0x1B2D, 0x1B2E, 0x1B2F,  */
       X(0x7B),X(0x7D),UNK,    UNK,    UNK,    UNK,    UNK,    X(0x5C),
    /* 0x1B30, 0x1B31, 0x1B32, 0x1B33, 0x1B34, 0x1B35, 0x1B36, 0x1B37,  */
       UNK,    UNK,    UNK,    UNK,    UNK,    UNK,    UNK,    UNK,
    /* 0x1B38, 0x1B39, 0x1B3A, 0x1B3B, 0x1B3C, 0x1B3D, 0x1B3E, 0x1B3F,  */
       UNK,    UNK,    UNK,    UNK,    X(0x5B),X(0x7E),X(0x5D),UNK,
    /* 0x1B40, 0x1B41, 0x1B42, 0x1B43, 0x1B44, 0x1B45, 0x1B46, 0x1B47,  */
       X(0x7C),UNK,    UNK,    UNK,    UNK,    UNK,    UNK,    UNK,
    /* 0x1B48, 0x1B49, 0x1B4A, 0x1B4B, 0x1B4C, 0x1B4D, 0x1B4E, 0x1B4F,  */
       UNK,    UNK,    UNK,    UNK,    UNK,    UNK,    UNK,    UNK,
    /* 0x1B50, 0x1B51, 0x1B52, 0x1B53, 0x1B54, 0x1B55, 0x1B56, 0x1B57,  */
       UNK,    UNK,    UNK,    UNK,    UNK,    UNK,    UNK,    UNK,
    /* 0x1B58, 0x1B59, 0x1B5A, 0x1B5B, 0x1B5C, 0x1B5D, 0x1B5E, 0x1B5F,  */
       UNK,    UNK,    UNK,    UNK,    UNK,    UNK,    UNK,    UNK,
    /* 0x1B60, 0x1B61, 0x1B62, 0x1B63, 0x1B64, 0x1B65, 0x1B66, 0x1B67,  */
       UNK,    UNK,    UNK,    UNK,    UNK,    X(0xA4),UNK,    UNK,
    /* 0x1B68, 0x1B69, 0x1B6A, 0x1B6B, 0x1B6C, 0x1B6D, 0x1B6E, 0x1B6F,  */
       UNK,    UNK,    UNK,    UNK,    UNK,    UNK,    UNK,    UNK,
    /* 0x1B70, 0x1B71, 0x1B72, 0x1B73, 0x1B74, 0x1B75, 0x1B76, 0x1B77,  */
       UNK,    UNK,    UNK,    UNK,    UNK,    UNK,    UNK,    UNK,
    /* 0x1B78, 0x1B79, 0x1B7A, 0x1B7B, 0x1B7C, 0x1B7D, 0x1B7E, 0x1B7F,  */
       UNK,    UNK,    UNK,    UNK,    UNK,    UNK,    UNK,    UNK,
    /* 0x1B80, 0x1B81, 0x1B82, 0x1B83, 0x1B84, 0x1B85, 0x1B86, 0x1B87,  */
       UNK,    UNK,    UNK,    UNK,    UNK,    UNK,    UNK,    UNK,
    /* 0x1B88, 0x1B89, 0x1B8A, 0x1B8B, 0x1B8C, 0x1B8D, 0x1B8E, 0x1B8F,  */
       UNK,    UNK,    UNK,    UNK,    UNK,    UNK,    UNK,    UNK,
    /* 0x1B90, 0x1B91, 0x1B92, 0x1B93, 0x1B94, 0x1B95, 0x1B96, 0x1B97,  */
       UNK,    UNK,    UNK,    UNK,    UNK,    UNK,    UNK,    UNK,
    /* 0x1B98, 0x1B99, 0x1B9A, 0x1B9B, 0x1B9C, 0x1B9D, 0x1B9E, 0x1B9F,  */
       UNK,    UNK,    UNK,    UNK,    UNK,    UNK,    UNK,    UNK,
    /* 0x1BA0, 0x1BA1, 0x1BA2, 0x1BA3, 0x1BA4, 0x1BA5, 0x1BA6, 0x1BA7,  */
       UNK,    UNK,    UNK,    UNK,    UNK,    UNK,    UNK,    UNK,
    /* 0x1BA8, 0x1BA9, 0x1BAA, 0x1BAB, 0x1BAC, 0x1BAD, 0x1BAE, 0x1BAF,  */
       UNK,    UNK,    UNK,    UNK,    UNK,    UNK,    UNK,    UNK,
    /* 0x1BB0, 0x1BB1, 0x1BB2, 0x1BB3, 0x1BB4, 0x1BB5, 0x1BB6, 0x1BB7,  */
       UNK,    UNK,    UNK,    UNK,    UNK,    UNK,    UNK,    UNK,
    /* 0x1BB8, 0x1BB9, 0x1BBA, 0x1BBB, 0x1BBC, 0x1BBD, 0x1BBE, 0x1BBF,  */
       UNK,    UNK,    UNK,    UNK,    UNK,    UNK,    UNK,    UNK,
    /* 0x1BC0, 0x1BC1, 0x1BC2, 0x1BC3, 0x1BC4, 0x1BC5, 0x1BC6, 0x1BC7,  */
       UNK,    UNK,    UNK,    UNK,    UNK,    UNK,    UNK,    UNK,
    /* 0x1BC8, 0x1BC9, 0x1BCA, 0x1BCB, 0x1BCC, 0x1BCD, 0x1BCE, 0x1BCF,  */
       UNK,    UNK,    UNK,    UNK,    UNK,    UNK,    UNK,    UNK,
    /* 0x1BD0, 0x1BD1, 0x1BD2, 0x1BD3, 0x1BD4, 0x1BD5, 0x1BD6, 0x1BD7,  */
       UNK,    UNK,    UNK,    UNK,    UNK,    UNK,    UNK,    UNK,
    /* 0x1BD8, 0x1BD9, 0x1BDA, 0x1BDB, 0x1BDC, 0x1BDD, 0x1BDE, 0x1BDF,  */
       UNK,    UNK,    UNK,    UNK,    UNK,    UNK,    UNK,    UNK,
    /* 0x1BE0, 0x1BE1, 0x1BE2, 0x1BE3, 0x1BE4, 0x1BE5, 0x1BE6, 0x1BE7,  */
       UNK,    UNK,    UNK,    UNK,    UNK,    UNK,    UNK,    UNK,
    /* 0x1BE8, 0x1BE9, 0x1BEA, 0x1BEB, 0x1BEC, 0x1BED, 0x1BEE, 0x1BEF,  */
       UNK,    UNK,    UNK,    UNK,    UNK,    UNK,    UNK,    UNK,
    /* 0x1BF0, 0x1BF1, 0x1BF2, 0x1BF3, 0x1BF4, 0x1BF5, 0x1BF6, 0x1BF7,  */
       UNK,    UNK,    UNK,    UNK,    UNK,    UNK,    UNK,    UNK,
    /* 0x1BF8, 0x1BF9, 0x1BFA, 0x1BFB, 0x1BFC, 0x1BFD, 0x1BFE, 0x1BFF   */
       UNK,    UNK,    UNK,    UNK,    UNK,    UNK,    UNK,    UNK,
#undef UNK
#undef X
#undef HEX
};

/* Convert GSM charset to Unicode */
static int const gsm7_to_unicode[] = {

#define X(x)     (0x##x)
#define UNK      '.'
    /* 0x00,   0x01,   0x02,   0x03,   0x04,   0x05,   0x06,   0x07,    */
       X(40),  X(A3),  X(24),  X(A5),  X(E8),  X(E9),  X(FA),  X(EC),
    /* 0x08,   0x09,   0x0A,   0x0B,   0x0C,   0x0D,   0x0E,   0x0F,    */
       X(F2),  X(C7),  X(0A),  X(D8),  X(F8),  X(0D),  X(C5),  X(E5),
    /* 0x10,   0x11,   0x12,   0x13,   0x14,   0x15,   0x16,   0x17,    */
       X(0394),X(5F),  X(03A6),X(0393),X(039B),X(03A9),X(03A0),X(03A8),
    /* 0x18,   0x19,   0x1A,   0x1B,   0x1C,   0x1D,   0x1E,   0x1F,    */
       X(03A3),X(0398),X(039E),UNK,    X(C6),  X(E6),  X(DF),  X(CA),
    /* 0x20,   0x21,   0x22,   0x23,   0x24,   0x25,   0x26,   0x27,    */
       X(20),  X(21),  X(22),  X(23),  UNK,    X(25),  X(26),  X(27),
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
    /* 0x80,   0x81,   0x82,   0x83,   0x84,   0x85,   0x86,   0x87,    */
       UNK,    UNK,    UNK,    UNK,    UNK,    UNK,    UNK,    UNK,
    /* 0x88,   0x89,   0x8A,   0x8B,   0x8C,   0x8D,   0x8E,   0x8F,    */
       UNK,    UNK,    UNK,    UNK,    UNK,    UNK,    UNK,    UNK,
    /* 0x90,   0x91,   0x92,   0x93,   0x94,   0x95,   0x96,   0x97,    */
       UNK,    UNK,    UNK,    UNK,    UNK,    UNK,    UNK,    UNK,
    /* 0x98,   0x99,   0x9A,   0x9B,   0x9C,   0x9D,   0x9E,   0x9F,    */
       UNK,    UNK,    UNK,    UNK,    UNK,    UNK,    UNK,    UNK,
    /* 0xA0,   0xA1,   0xA2,   0xA3,   0xA4,   0xA5,   0xA6,   0xA7,    */
       UNK,    UNK,    UNK,    UNK,    UNK,    UNK,    UNK,    UNK,
    /* 0xA8,   0xA9,   0xAA,   0xAB,   0xAC,   0xAD,   0xAE,   0xAF,    */
       UNK,    UNK,    UNK,    UNK,    UNK,    UNK,    UNK,    UNK,
    /* 0xB0,   0xB1,   0xB2,   0xB3,   0xB4,   0xB5,   0xB6,   0xB7,    */
       UNK,    UNK,    UNK,    UNK,    UNK,    UNK,    UNK,    UNK,
    /* 0xB8,   0xB9,   0xBA,   0xBB,   0xBC,   0xBD,   0xBE,   0xBF,    */
       UNK,    UNK,    UNK,    UNK,    UNK,    UNK,    UNK,    UNK,
    /* 0xC0,   0xC1,   0xC2,   0xC3,   0xC4,   0xC5,   0xC6,   0xC7,    */
       UNK,    UNK,    UNK,    UNK,    UNK,    UNK,    UNK,    UNK,
    /* 0xC8,   0xC9,   0xCA,   0xCB,   0xCC,   0xCD,   0xCE,   0xCF,    */
       UNK,    UNK,    UNK,    UNK,    UNK,    UNK,    UNK,    UNK,
    /* 0xD0,   0xD1,   0xD2,   0xD3,   0xD4,   0xD5,   0xD6,   0xD7,    */
       UNK,    UNK,    UNK,    UNK,    UNK,    UNK,    UNK,    UNK,
    /* 0xD8,   0xD9,   0xDA,   0xDB,   0xDC,   0xDD,   0xDE,   0xDF,    */
       UNK,    UNK,    UNK,    UNK,    UNK,    UNK,    UNK,    UNK,
    /* 0xE0,   0xE1,   0xE2,   0xE3,   0xE4,   0xE5,   0xE6,   0xE7,    */
       UNK,    UNK,    UNK,    UNK,    UNK,    UNK,    UNK,    UNK,
    /* 0xE8,   0xE9,   0xEA,   0xEB,   0xEC,   0xED,   0xEE,   0xEF,    */
       UNK,    UNK,    UNK,    UNK,    UNK,    UNK,    UNK,    UNK,
    /* 0xF0,   0xF1,   0xF2,   0xF3,   0xF4,   0xF5,   0xF6,   0xF7,    */
       UNK,    UNK,    UNK,    UNK,    UNK,    UNK,    UNK,    UNK,
    /* 0xF8,   0xF9,   0xFA,   0xFB,   0xFC,   0xFD,   0xFE,   0xFF,    */
       UNK,    UNK,    UNK,    UNK,    UNK,    UNK,    UNK,    UNK,

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
    /* 0x1B80, 0x1B81, 0x1B82, 0x1B83, 0x1B84, 0x1B85, 0x1B86, 0x1B87,  */
       UNK,    UNK,    UNK,    UNK,    UNK,    UNK,    UNK,    UNK,
    /* 0x1B88, 0x1B89, 0x1B8A, 0x1B8B, 0x1B8C, 0x1B8D, 0x1B8E, 0x1B8F,  */
       UNK,    UNK,    UNK,    UNK,    UNK,    UNK,    UNK,    UNK,
    /* 0x1B90, 0x1B91, 0x1B92, 0x1B93, 0x1B94, 0x1B95, 0x1B96, 0x1B97,  */
       UNK,    UNK,    UNK,    UNK,    UNK,    UNK,    UNK,    UNK,
    /* 0x1B98, 0x1B99, 0x1B9A, 0x1B9B, 0x1B9C, 0x1B9D, 0x1B9E, 0x1B9F,  */
       UNK,    UNK,    UNK,    UNK,    UNK,    UNK,    UNK,    UNK,
    /* 0x1BA0, 0x1BA1, 0x1BA2, 0x1BA3, 0x1BA4, 0x1BA5, 0x1BA6, 0x1BA7,  */
       UNK,    UNK,    UNK,    UNK,    UNK,    UNK,    UNK,    UNK,
    /* 0x1BA8, 0x1BA9, 0x1BAA, 0x1BAB, 0x1BAC, 0x1BAD, 0x1BAE, 0x1BAF,  */
       UNK,    UNK,    UNK,    UNK,    UNK,    UNK,    UNK,    UNK,
    /* 0x1BB0, 0x1BB1, 0x1BB2, 0x1BB3, 0x1BB4, 0x1BB5, 0x1BB6, 0x1BB7,  */
       UNK,    UNK,    UNK,    UNK,    UNK,    UNK,    UNK,    UNK,
    /* 0x1BB8, 0x1BB9, 0x1BBA, 0x1BBB, 0x1BBC, 0x1BBD, 0x1BBE, 0x1BBF,  */
       UNK,    UNK,    UNK,    UNK,    UNK,    UNK,    UNK,    UNK,
    /* 0x1BC0, 0x1BC1, 0x1BC2, 0x1BC3, 0x1BC4, 0x1BC5, 0x1BC6, 0x1BC7,  */
       UNK,    UNK,    UNK,    UNK,    UNK,    UNK,    UNK,    UNK,
    /* 0x1BC8, 0x1BC9, 0x1BCA, 0x1BCB, 0x1BCC, 0x1BCD, 0x1BCE, 0x1BCF,  */
       UNK,    UNK,    UNK,    UNK,    UNK,    UNK,    UNK,    UNK,
    /* 0x1BD0, 0x1BD1, 0x1BD2, 0x1BD3, 0x1BD4, 0x1BD5, 0x1BD6, 0x1BD7,  */
       UNK,    UNK,    UNK,    UNK,    UNK,    UNK,    UNK,    UNK,
    /* 0x1BD8, 0x1BD9, 0x1BDA, 0x1BDB, 0x1BDC, 0x1BDD, 0x1BDE, 0x1BDF,  */
       UNK,    UNK,    UNK,    UNK,    UNK,    UNK,    UNK,    UNK,
    /* 0x1BE0, 0x1BE1, 0x1BE2, 0x1BE3, 0x1BE4, 0x1BE5, 0x1BE6, 0x1BE7,  */
       UNK,    UNK,    UNK,    UNK,    UNK,    UNK,    UNK,    UNK,
    /* 0x1BE8, 0x1BE9, 0x1BEA, 0x1BEB, 0x1BEC, 0x1BED, 0x1BEE, 0x1BEF,  */
       UNK,    UNK,    UNK,    UNK,    UNK,    UNK,    UNK,    UNK,
    /* 0x1BF0, 0x1BF1, 0x1BF2, 0x1BF3, 0x1BF4, 0x1BF5, 0x1BF6, 0x1BF7,  */
       UNK,    UNK,    UNK,    UNK,    UNK,    UNK,    UNK,    UNK,
    /* 0x1BF8, 0x1BF9, 0x1BFA, 0x1BFB, 0x1BFC, 0x1BFD, 0x1BFE, 0x1BFF   */
       UNK,    UNK,    UNK,    UNK,    UNK,    UNK,    UNK,    UNK,
#undef UNK
#undef X
#undef HEX
};

/* Achtung: Decode a hex encoded (IRA) byte array into ISO-8859-15
 * subset, not UTF-8
 */
int blob_decode_ira_hex_as_latin15(blob_t *dst, const char *src, int len)
{
    int pos;
    const char *end;
    byte *data;

    pos = dst->len;
    blob_grow(dst, len / 2 + 1);
    data = dst->data + pos;
    /* trim last char if len is odd */
    end = src + (len & ~1);
    while (src < end) {
        int ind = hexdecode(src);

        src += 2;
        if (src < end && ind == 0x1B) {
            ind = 256 + hexdecode(src);
            src += 2;
        }
        *data++ = gsm7_to_iso8859_15[ind];
    }
    data[0] = '\0';
    dst->len = data - dst->data;
    return 0;
}

int blob_decode_ira_bin_as_latin15(blob_t *dst, const char *src, int len)
{
    int pos;
    const char *end;
    byte *data;

    pos = dst->len;
    blob_grow(dst, len);
    data = dst->data + pos;
    end = src + len;
    while (src < end) {
        int ind = (byte)*src++;

        if (ind == 0x1B && src < end) {
            ind = 256 + (byte)*src++;
        }
        *data++ = gsm7_to_iso8859_15[ind];
    }
    *data = '\0';
    dst->len = data - dst->data;
    return 0;
}

/* Decode a hex encoded (IRA) char array into UTF-8 at end of blob */
int blob_decode_ira_hex_as_utf8(blob_t *dst, const char *src, int slen)
{
    int pos, len;

    if (slen & 1)
        return -1;

    pos = dst->len;
    len = slen / 2;
    for (;;) {
        blob_grow(dst, len);
        len = string_decode_ira_hex_as_utf8((char *)dst->data + pos,
                                            dst->size - pos,
                                            (const char*)src, slen);
        if (pos + len < dst->size)
            break;
    }
    dst->len += len;
    return 0;
}

int blob_decode_ira_bin_as_utf8(blob_t *dst, const char *src, int slen)
{
    int pos, len;

    pos = dst->len;
    len = slen;
    for (;;) {
        blob_grow(dst, len);
        len = string_decode_ira_bin_as_utf8((char *)dst->data + pos,
                                            dst->size - pos,
                                            (const char*)src, slen);
        if (pos + len < dst->size)
            break;
    }
    dst->len += len;
    return 0;
}

/* Achtung: Decode a hex encoded (IRA) char array into ISO-8859-15
 * subset, not UTF-8
 */
int string_decode_ira_hex_as_latin15(char *dst, int size,
                                     const char *src, int len)
{
    int pos = 0;

    if (len & 1)
        return -1;

    while (len >= 2) {
        int ind = hexdecode(src);
        if (ind < 0)
            break;

        src += 2;
        len -= 2;

        if (ind == 0x1B && len >= 2) {
            ind = hexdecode(src);
            if (ind < 0)
                break;
            ind += 256;
            src += 2;
            len -= 2;
        }
        if (++pos < size)
            *dst++ = gsm7_to_iso8859_15[ind];
    }
    if (size > 0) {
        *dst = '\0';
    }
    return pos;
}

int string_decode_ira_bin_as_latin15(char *dst, int size,
                                     const char *src, int len)
{
    const char *end = src + len;
    int pos = 0;

    while (src < end) {
        int ind = (byte)*src++;

        if (ind == 0x1B && src < end) {
            ind = 256 + (byte)*src++;
        }
        ++pos;
        if (pos < size) {
            *dst++ = gsm7_to_iso8859_15[ind];
        }
    }
    if (size > 0) {
        *dst = '\0';
    }
    return pos;
}

/* Parse IRA (hex encoded) string into UTF-8 char array */
int string_decode_ira_hex_as_utf8(char *dst, int size,
                                  const char *src, int len)
{
    int pos = 0;

    while (len >= 2) {
        int c, ind;

        ind = hexdecode(src);
        if (ind < 0)
            break;

        src += 2;
        len -= 2;

        if (ind == 0x1B && len >= 2) {
            ind = hexdecode(src);
            if (ind < 0)
                break;
            ind += 256;
            src += 2;
            len -= 2;
        }
        c = gsm7_to_unicode[ind];
        /* Encode c as UTF-8 */
        if (c < 0x80) {
            pos += 1;
            if (pos < size) {
                *dst++ = c;
            }
        } else
        if (c < 0x1000) {
            pos += 2;
            if (pos < size) {
                dst[0] = 0xC0 | (((c) >>  6));
                dst[1] = 0x80 | (((c) >>  0) & 0x3F);
                dst += 2;
            }
        } else {
            pos += 3;
            if (pos < size) {
                dst[0] = 0xE0 | (((c) >> 12));
                dst[1] = 0x80 | (((c) >>  6) & 0x3F);
                dst[2] = 0x80 | (((c) >>  0) & 0x3F);
                dst += 3;
            }
        }
    }
    if (size > 0) {
        *dst = '\0';
    }
    return pos;
}

int string_decode_ira_bin_as_utf8(char *dst, int size,
                                  const char *src, int len)
{
    const char *end = src + len;
    int pos = 0;

    while (src < end) {
        int c, ind = (byte)*src++;

        if (ind == 0x1B && src < end) {
            ind = 256 + (byte)*src++;
        }
        c = gsm7_to_unicode[ind];
        /* Encode c as UTF-8 */
        if (c < 0x80) {
            pos += 1;
            if (pos < size) {
                *dst++ = c;
            }
        } else
        if (c < 0x1000) {
            pos += 2;
            if (pos < size) {
                dst[0] = 0xC0 | (((c) >>  6));
                dst[1] = 0x80 | (((c) >>  0) & 0x3F);
                dst += 2;
            }
        } else {
            pos += 3;
            if (pos < size) {
                dst[0] = 0xE0 | (((c) >> 12));
                dst[1] = 0x80 | (((c) >>  6) & 0x3F);
                dst[2] = 0x80 | (((c) >>  0) & 0x3F);
                dst += 3;
            }
        }
    }
    if (size > 0) {
        *dst = '\0';
    }
    return pos;
}

static unsigned short const win1252_to_gsm7[] = {
#define X(x)  (x)
#define UNK  '.'
#define _

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
    X(0x20), X(0x40),  X('c'), X(0x01),X(0x1b65),X(0x03),X(0x1B40),X(0x5F),

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

#undef _
#undef UNK
};

static int unicode_to_gsm7(int c, int unknown)
{
    if ((unsigned)c <= 0xff)
        return win1252_to_gsm7[c];

    switch (c) {
      case 0x20AC:    /* EURO */
        return X(0x1B65);
      case 0x0394:    /* GREEK CAPITAL LETTER DELTA */
        return X(0x10);
      case 0x03A6:    /* GREEK CAPITAL LETTER PHI */
        return X(0x12);
      case 0x0393:    /* GREEK CAPITAL LETTER GAMMA */
        return X(0x13);
      case 0x039B:    /* GREEK CAPITAL LETTER LAMDA */
        return X(0x14);
      case 0x03A9:    /* GREEK CAPITAL LETTER OMEGA */
        return X(0x15);
      case 0x03A0:    /* GREEK CAPITAL LETTER PI */
        return X(0x16);
      case 0x03A8:    /* GREEK CAPITAL LETTER PSI */
        return X(0x17);
      case 0x03A3:    /* GREEK CAPITAL LETTER SIGMA */
        return X(0x18);
      case 0x0398:    /* GREEK CAPITAL LETTER THETA */
        return X(0x19);
      case 0x039E:    /* GREEK CAPITAL LETTER XI */
        return X(0x1A);
      default:
        return unknown;
    }
}

int gsm7_charlen(int c)
{
    c = unicode_to_gsm7(c, -1);
    if (c < 0)
        return -1;
    return 1 + (c > 0xff);
}

int blob_append_ira_bin(blob_t *dst, const byte *src, int len)
{
    int c, pos;
    const byte *end;
    byte *data;

    pos = dst->len;
    /* Characters may expand to 2 bytes each */
    blob_extend(dst, len * 2);
    data = dst->data + pos;
    end = src + len;
    while (src < end) {
        c = *src++;
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
                        c = unicode_to_gsm7(c, '?');
                        goto hasc;
                    }
                }
            }
        }
        c = win1252_to_gsm7[c];

    hasc:
        if (c > 0xFF) {
            *data++ = (c >> 8);
        }
        *data++ = c;
    }
    *data = '\0';

    dst->len = data - dst->data;
    return 0;
}


#if __BYTE_ORDER != __LITTLE_ENDIAN // Little endian, misaligned access OK

static char const __str_xdigits_upper[16] = "0123456789ABCDEF";

/* More portable, but slower version */

int blob_append_ira_hex(blob_t *dst, const byte *src, int len)
{
    int c, pos;
    const byte *end;
    byte *data;

    pos = dst->len;
    /* Characters may expand to 2 bytes each before hex conversion */
    blob_extend(dst, len * 4);
    data = dst->data + pos;
    end = src + len;
    while (src < end) {
        c = *src++;
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
                        c = unicode_to_gsm7(c, '?');
                        goto hasc;
                    }
                }
            }
        }
        c = win1252_to_gsm7[c];

    hasc:
        if (c > 0xFF) {
            *data++ = __str_xdigits_upper[(c >> 12) & 0x0F];
            *data++ = __str_xdigits_upper[(c >> 8) & 0x0F];
        }
        *data++ = __str_xdigits_upper[(c >> 4) & 0x0F];
        *data++ = __str_xdigits_upper[(c >> 0) & 0x0F];
    }
    *data = '\0';

    dst->len = data - dst->data;
    return 0;
}

#undef X

#else

#undef X

static int const win1252_to_gsm7_hex[] = {

#define HEX(x)   ((x) < 10 ? '0' + (x) : 'A' + (x) - 10)
#define X2(x)    ((HEX(((x) >> 4) & 15))  + (HEX(((x) >> 0) & 15) << 8))
#define X4(x)    ((HEX(((x) >> 12) & 15)) + (HEX(((x) >> 8) & 15) << 8) + \
                  (HEX(((x) >> 4) & 15) << 16) + (HEX(((x) >> 0) & 15) << 24))
#define X(x)     (((x) > 0xFF) ? X4(x) : X2(x))
#define UNK      '.'
#define ESC      0x10000
#define _

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
    X(0x20), X(0x40),  X('c'), X(0x01),X(0x1b65),X(0x03),X(0x1B40),X(0x5F),

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

    ESC,     ESC,     ESC,     ESC,     ESC,     ESC,     ESC,     ESC,
    ESC,     ESC,     ESC,     ESC,     ESC,     ESC,     ESC,     ESC,
    ESC,     ESC,     ESC,     ESC,     ESC,     ESC,     ESC,     ESC,
    ESC,     ESC,     ESC,     ESC,     ESC,     ESC,     ESC,     ESC,
    ESC,     ESC,     ESC,     ESC,     ESC,     ESC,     ESC,     ESC,
    ESC,     ESC,     ESC,     ESC,     ESC,     ESC,     ESC,     ESC,
    ESC,     ESC,     ESC,     ESC,     ESC,     ESC,     ESC,     ESC,
    ESC,     ESC,     ESC,     ESC,     ESC,     ESC,     ESC,     ESC,
    ESC,     ESC,     ESC,     ESC,     ESC,     ESC,     ESC,     ESC,
    ESC,     ESC,     ESC,     ESC,     ESC,     ESC,     ESC,     ESC,
    ESC,     ESC,     ESC,     ESC,     ESC,     ESC,     ESC,     ESC,
    ESC,     ESC,     ESC,     ESC,     ESC,     ESC,     ESC,     ESC,
    ESC,     ESC,     ESC,     ESC,     ESC,     ESC,     ESC,     ESC,
    ESC,     ESC,     ESC,     ESC,     ESC,     ESC,     ESC,     ESC,
    ESC,     ESC,     ESC,     ESC,     ESC,     ESC,     ESC,     ESC,
    ESC,     ESC,     ESC,     ESC,     ESC,     ESC,     ESC,     ESC,

#undef ESC
#undef _
};

int blob_append_ira_hex(blob_t *dst, const byte *src, int len)
{
    int c, pos;
    const byte *end;
    const byte *end_4;
    byte *data;

    pos = dst->len;
    /* Characters may expand to 2 bytes each before hex conversion */
    blob_extend(dst, len * 4);
    data = dst->data + pos;
    end = src + len;
    end_4 = src + len - 4;

    while (src < end) {
#if 1   // Unroll plain character case 4 times */
        while (src < end_4) {
            c = win1252_to_gsm7_hex[128 + src[0]];
            if (c > 0xFFFF)
                break;
            *(int16_t *)(data + 0) = c;   /* Endian dependent */
            c = win1252_to_gsm7_hex[128 + src[1]];
            if (c > 0xFFFF)
                goto generic1;
            *(int16_t *)(data + 2) = c;   /* Endian dependent */
            c = win1252_to_gsm7_hex[128 + src[2]];
            if (c > 0xFFFF)
                goto generic2;
            *(int16_t *)(data + 4) = c;   /* Endian dependent */
            c = win1252_to_gsm7_hex[128 + src[3]];
            if (c > 0xFFFF)
                goto generic3;
            *(int16_t *)(data + 6) = c;   /* Endian dependent */
            src  += 4;
            data += 8;
            continue;

        generic3:
            src  += 1;
            data += 2;
        generic2:
            src  += 1;
            data += 2;
        generic1:
            src  += 1;
            data += 2;
            break;
        }
#endif
        c = win1252_to_gsm7_hex[128 + *src];

        if (c < 0x10000) {
            *(int16_t *)data = c;   /* Endian dependent */
            src  += 1;
            data += 2;
            continue;
        }

        c = *src++;
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
                            c = X(0x1B65);
                            break;
                        case 0x0394:    /* GREEK CAPITAL LETTER DELTA */
                            c = X(0x10);
                            break;
                        case 0x03A6:    /* GREEK CAPITAL LETTER PHI */
                            c = X(0x12);
                            break;
                        case 0x0393:    /* GREEK CAPITAL LETTER GAMMA */
                            c = X(0x13);
                            break;
                        case 0x039B:    /* GREEK CAPITAL LETTER LAMDA */
                            c = X(0x14);
                            break;
                        case 0x03A9:    /* GREEK CAPITAL LETTER OMEGA */
                            c = X(0x15);
                            break;
                        case 0x03A0:    /* GREEK CAPITAL LETTER PI */
                            c = X(0x16);
                            break;
                        case 0x03A8:    /* GREEK CAPITAL LETTER PSI */
                            c = X(0x17);
                            break;
                        case 0x03A3:    /* GREEK CAPITAL LETTER SIGMA */
                            c = X(0x18);
                            break;
                        case 0x0398:    /* GREEK CAPITAL LETTER THETA */
                            c = X(0x19);
                            break;
                        case 0x039E:    /* GREEK CAPITAL LETTER XI */
                            c = X(0x1A);
                            break;
                        default:
                            if (c > 0xFF) {
                                c = X('?');
                            } else {
                                c = win1252_to_gsm7_hex[c ^ 0x80];
                            }
                            break;
                        }
                        goto hasc;
                    }
                }
            }
        }
        c = win1252_to_gsm7_hex[c ^ 0x80];

    hasc:
        if (c < 0x10000) {
            *(int16_t *)data = c;   /* Endian dependent */
            data += 2;
        } else {
            *(int32_t *)data = c;   /* Endian dependent */
            data += 4;
        }
    }
    *data = '\0';

    dst->len = data - dst->data;
    return 0;
}

/* write up to 8 septets in the 56 most significant bits of "pack" */
static void blob_append_gsm_aligned_pack(blob_t *out, uint64_t pack, int len)
{
#if __BYTE_ORDER == __BIG_ENDIAN
    pack = bswap64(pack);
#endif
    blob_append_data(out, &pack, len);
}

int blob_append_gsm7_packed(blob_t *out, const char *utf8, int unknown)
{
    uint64_t pack = 0;
    int septet = 0;

    for (;;) {
        int c = utf8_getc(utf8, &utf8);

        if (c <= 0) {
            blob_append_gsm_aligned_pack(out, pack, septet);
            return c;
        }

        c = unicode_to_gsm7(c, unknown);
        if (c < 0)
            return -1;

        if (c > 0xff) {
            pack |= ((c >> 8) << (7 * septet));
            if (++septet == 8) {
                blob_append_gsm_aligned_pack(out, pack, 7);
                septet = 0;
                pack = 0;
            }
        }
        pack |= ((c & 0x7f) << (7 * septet));
        if (++septet == 8) {
            blob_append_gsm_aligned_pack(out, pack, 7);
            septet = 0;
            pack = 0;
        }
    }
}

#undef HEX
#undef X2
#undef X4
#undef X

#endif
