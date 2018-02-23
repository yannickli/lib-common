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

var superBuffer = StringBuffer()

superBuffer.addString("Hello world!")
print(superBuffer)
superBuffer.wipe()

print("QVector")
do {
    let qv : AutoWipe<qv_u64_t> = AutoWipe([ 0, 1, 2, 3, 4, 5 ])
    for v in qv.wrapped {
        print(v)
    }
    print(qv)
}

tScope {
    (frame) in

    let qv = qv_u64_t(on: frame, withElements: 0, 1, 2, 3, 4, 5)
    for v in qv {
        print(v)
    }
    print(qv)
    debugPrint(qv)
}

print("QSet")
do {
    let qh : AutoWipe<qh_u32_t> = AutoWipe([ 1, 2, 18, 100 ])
    for v in qh.wrapped {
        print(v)
    }
}

tScope {
    (frame) in

    let qh = qh_u32_t(on: frame, withElements: 1, 2, 18, 100)
    for v in qh {
        print(v)
    }
    print(qh)
    debugPrint(qh)
}

print("IOP")
do {
    IopEnv.register(package: core.self)

    let lfc = core.LogFileConfiguration()
    print(lfc)
    lfc.compress = false
    print(lfc)

    lfc.withC {
        let mem = $0.bindMemory(to: core__log_file_configuration__t.self, capacity: 1)
        print(mem.pointee)
    }
}

print("IChannel")
module_require(ic_module!, nil)

do {
    let ic_impl = AutoWipe(qm_ic_cbs_t(on: StandardAllocator.malloc, withCapacity: 0))
    var registry = IcImplRegistry(impl: &ic_impl.wrapped)

    registry.implements(rpc: core.modules.Core.log.setRootLevel, cb: {
        print("\($0) hdr=\(String(describing: $1))")
        return core.interfaces.Log.SetRootLevel.Response(level: LOG_LEVEL_ERR)
    })

    let ic = IChannel<core.modules.Core>.makeLocal()
    ic.set(impl: registry)

    _ = ic.query.log.setRootLevel(level: LOG_LEVEL_WARNING)
}

module_release(ic_module!)
