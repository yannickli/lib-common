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

/*
 *
 * \file       crc_macros.h
 * \brief      CRC32 calculation
 *
 * Calculate the CRC32 using the slice-by-eight algorithm.
 * It is explained in this document:
 * http://www.intel.com/technology/comms/perfnet/download/CRC_generators.pdf
 * The code in this file is not the same as in Intel's paper, but
 * the basic principle is identical.
 *
 *  Author:     Lasse Collin
 *
 *  This file has been put into the public domain.
 *  You can do whatever you want with this file.
 *
 */

#if __BYTE_ORDER == __BIG_ENDIAN
#   define A(x)   ((x) >> 24)
#   define A1(x)  ((x) >> 56)
#   define B(x)   (((x) >> 16) & 0xFF)
#   define C(x)   (((x) >> 8) & 0xFF)
#   define D(x)   ((x) & 0xFF)

#   define S8(x)  ((x) << 8)
#   define S32(x) ((x) << 32)
#else
#   define A(x)   ((x) & 0xFF)
#   define A1(x)  A(x)
#   define B(x)   (((x) >> 8) & 0xFF)
#   define C(x)   (((x) >> 16) & 0xFF)
#   define D(x)   ((x) >> 24)

#   define S8(x)  ((x) >> 8)
#   define S32(x) ((x) >> 32)
#endif
