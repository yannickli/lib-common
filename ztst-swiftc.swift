/**************************************************************************/
/*                                                                        */
/*  Copyright (C) 2004-2018 INTERSEC SA                                   */
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

class Toto {
    var i = 0

    deinit {
        print("wiping toto")
    }
}

var cron: El? = nil

func swiftFromCInitialize(ptr: UnsafeMutableRawPointer?) -> Int32 {
    let toto = Toto()

    cron = El.schedule(in: 1000, repeatEvery: 1000) { _ in
        print("\(#function): \(toto.i)")
        toto.i += 1
    }
    print(#function)
    return 0
}

func swiftFromCOnTerm(signo: Int32) {
    print("\(#function) with signo \(signo)")
    cron = nil
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

func useVector(qv: UnsafeMutablePointer<qv_u64_t>) {
    for val in qv.pointee {
        print(val)
    }
}
