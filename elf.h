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

#ifndef IS_LIB_COMMON_ELF_H
#define IS_LIB_COMMON_ELF_H

#include <elf.h>

/* FIXME: would perhaps be better to use linux headers. */
#if defined(__i386__) && !defined(__x86_64__)
#  define       Elf_Ehdr        Elf32_Ehdr
#  define       Elf_Phdr        Elf32_Phdr
#  define       ELF_CLASS       ELFCLASS32
#  define       ELF_DATA        ELFDATA2LSB
#  define       ELF_ARCH        EM_386
#elif defined(__x86_64__)
#  define       Elf_Ehdr        Elf64_Ehdr
#  define       Elf_Phdr        Elf64_Phdr
#  define       ELF_CLASS       ELFCLASS64
#  define       ELF_DATA        ELFDATA2LSB
#  define       ELF_ARCH        EM_X86_64
#else
#  error "architecture not supported (yet)."
#endif

#endif
