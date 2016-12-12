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

var cron: el_t? = nil

func cronCb(el: el_t, _: data_t) {
    print(#function)
}

func swiftFromCInitialize(ptr: UnsafeMutableRawPointer?) -> Int32 {
    cron = el_timer_register(1000, 1000, 0, cronCb, nil)
    print(#function)
    return 0
}

func swiftFromCOnTerm(signo: Int32) {
    print("\(#function) with signo \(signo)")
    el_unregister(&cron)
}

func swiftFromCShutdown() -> Int32 {
    print(#function)
    return 0
}

let swift_from_c = ({ () -> Module in
    let mod = Module(name: "swift_from_c", module: swift_from_c_module_ptr,
                     initialize: swiftFromCInitialize,
                     shutdown: swiftFromCShutdown)
    mod.depends(on: c_from_swift_module_ptr, named: "c_from_swift")
    mod.implements(method: on_term_method_ptr, with: swiftFromCOnTerm)
    return mod
})()
