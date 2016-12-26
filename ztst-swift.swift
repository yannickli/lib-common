/**************************************************************************/
/*                                                                        */
/*  Copyright (C) 2004-2017 INTERSEC SA                                   */
/*                                                                        */
/*  Should you receive a copy of this source code, you must check you     */
/*  have a proper, written authorization of INTERSEC to hold it. If you   */
/*  don't have such an authorization, you must DELETE all source code     */
/*  files in your possession, and inform INTERSEC of the fact you obtain  */
/*  these files. Should you not comply to these terms, you can be         */
/*  prosecuted in the extent permitted by applicable law.                 */
/*                                                                        */
/**************************************************************************/

import libcommon

var superBuffer = StringBuffer()

superBuffer.addString("Hello world!")
print(superBuffer)
superBuffer.wipe()

print("QVector")
var qv : qv_u64_t = [ 0, 1, 2, 3, 4, 5 ]
for v in qv {
    print(v)
}
qv.wipe()

tScope {
    (frame) in

    let qv = qv_u64_t(on: frame, withElements: 0, 1, 2, 3, 4, 5)
    for v in qv {
        print(v)
    }
    print(qv)
    debugPrint(qv)
}
