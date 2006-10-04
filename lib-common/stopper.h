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

#ifndef _LIBCOMMON_STOPPER_H_
#define _LIBCOMMON_STOPPER_H_

bool is_stopper_waiting(void);
void stopper_initialize(void);
void stopper_shutdown(void);

#endif /* _LIBCOMMON_STOPPER_H_ */
