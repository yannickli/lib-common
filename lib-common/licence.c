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

#include <stdio.h>

#include "macros.h"

#define xstr(x)  #x
#define str(x)   xstr(x)
#define str_INTERSECID  str(INTERSECID)
#define LF  "\n"

int show_licence(const char *arg)
{
    time_t t = EXPIRATION_DATE;

    fprintf(stderr,
            "%s -- INTERSEC Multi Channel Marketing Suite version 2.6"      LF
            "Copyright (C) 2004-2006  INTERSEC SAS -- All Rights Reserved"  LF
            "Licence type: Temporary Test Licence"                          LF
            "    Licencee: OrangeFrance SA"                                 LF
            "  Intersecid: " str(INTERSECID) ""                                      LF
            , arg);
#ifdef EXPIRATION_DATE    
    fprintf(stderr,
            "  Expiration: %s", ctime(&t));
#endif
    fprintf(stderr,
            "     Contact: +33 1 7543 5100 -- www.intersec.eu"              LF
           );
    exit(1);
}

int set_licence(const char *arg, const char *licence_data)
{
    if (licence_data && !strcmp(licence_data, str(INTERSECID))) {
        fprintf(stderr, "%s: Licence installed\n", arg);
    } else {
        fprintf(stderr, "%s: Invalid licence key\n", arg);
    }
    exit(1);
}
