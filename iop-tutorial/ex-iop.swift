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
import Glibc
import IopTutorial

private typealias TutoChannel = IChannel<exiop.modules.HelloMod>

private enum Globals {
    static var logger = Logger(name: "ex-iop-swift")
    static var els : (El, El, El, El)?
    static let impl = AutoWipe(qm_ic_cbs_t(on: StandardAllocator.malloc, withCapacity: 0))
    static let registry = IcImplRegistry(impl: &Globals.impl.wrapped)

    /* Client mode */
    static var clientChannel : TutoChannel?

    /* Server mode */
    static var serverEl : El?
    static var clients : Set<TutoChannel> = []
}

/* Client mode {{{ */

private func clientInitialize(address: String) throws {
    Globals.registry.implements(rpc: exiop.modules.HelloMod.helloInterface.sendAsync) {
        (arg, hdr) in

        Globals.logger.trace(level: 0, "received msg:", arg.msg, "from client", arg.seqnum)
    }

    Globals.clientChannel = try TutoChannel.connect(to: address) {
        (channel, event) in

        switch event {
          case .connected:
            Globals.logger.notice("connected to server")

            let res = channel.query.helloInterface.send(seqnum: 1, msg: "From client: Hello (1)")

            res.then {
                Globals.logger.trace(level: 0, "helloworld: res =", $0.res)
            }
            res.otherwise {
                Globals.logger.error("cannot send:", $0)
            }

          case .disconnected:
            Globals.logger.warning("disconnected from server")

          default:
            break
        }
    }
    Globals.clientChannel?.set(impl: Globals.registry)
}

/* }}} */
/* Server mode {{{ */

private func serverInitialize(address: String) throws {
    Globals.registry.implements(rpc: exiop.modules.HelloMod.helloInterface.send, cb: {
        (arg, hdr) in

        Globals.logger.trace(level: 0, "helloworld: msg =", arg.msg, "seqnum =", arg.seqnum)

        for client in Globals.clients {
            client.query.helloInterface.sendAsync(seqnum: 0, msg: arg.msg)
        }
        return .init(res: 1)
    })

    Globals.serverEl = try TutoChannel.listen(to: address) {
        (client, event) in

        switch event {
          case .connected:
            Globals.logger.trace(level: 0, "incoming connection")
            client.set(impl: Globals.registry)
            Globals.clients.insert(client)

          case .disconnected:
            Globals.logger.warning("client disconnected")
            Globals.clients.remove(client)

          default:
            break
        }
    }
}

/* }}} */
/* Command line handling {{{ */

private enum Options {
    static let help = CommandLineOption.Flag(name: "help", short: "h", help: "show this help")
    static let version = CommandLineOption.Flag(name: "version", short: "v", help: "show version")
    static let client = CommandLineOption.Flag(name: "client", short: "C", help: "client mode")
    static let server = CommandLineOption.Flag(name: "server", short: "S", help: "server mode")

    static let options = [ CommandLineOption.Group(name: "Options:"), help, version, client, server ]
}

CommandLine.parse(options: Options.options)

if Options.help.value || CommandLine.remain != 1 {
    CommandLine.makeUsage("<address>", options: Options.options)
}

if Options.version.value {
    Globals.logger.notice("HELLO - Version 1.0")
    exit(EXIT_SUCCESS)
}

/* }}} */

private func onTerm(_: el_t, signum: Int32) {
    Globals.els = nil
    Globals.serverEl = nil
    Globals.clientChannel?.closeGently()
    for client in Globals.clients {
        client.closeGently()
    }
}

do {
    module_require(ic_module!, nil)

    if Options.client.value {
        Globals.logger.notice("launching in client mode...")
        do {
            try clientInitialize(address: CommandLine.nextArg()!)
        } catch {
            Globals.logger.fatal("cannot initialize client:", error)
        }
    } else
    if Options.server.value {
        Globals.logger.notice("launching in server mode...")
        do {
            try serverInitialize(address: CommandLine.nextArg()!)
        } catch {
            Globals.logger.fatal("cannot initialize server:", error)
        }
    }

    Globals.els = (
        El.registerBlocker(),
        El.register(signal: SIGTERM, onTerm),
        El.register(signal: SIGINT, onTerm),
        El.register(signal: SIGQUIT, onTerm)
    )

    El.loop()

    Globals.clientChannel = nil

    module_release(ic_module!)
}
