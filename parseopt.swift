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

/// Base class for command line switches.
///
/// Parameters provides ability to declare a new command line switch in the
/// form of a fullname, an optional short swift and a description. Once the
/// command line has been parse, the parsed value for the parameter is
/// accessible trough the `value` propertie.
///
/// Don't use this class directly, you should instead use the various
/// specialized types available in in `CommandLineOption`
public class Parameter<Type, StorageType> : CommandLineOption {
    let name: Swift.String
    let short: UnicodeScalar?
    let help: Swift.String
    var valueStorage : StorageType

    /// Parsed value of the parameter.
    ///
    /// If the switch was provided on the command line, the provided value
    /// is returned, otherwise, the default value.
    public var value : Type {
        fatalError("unimplemented")
    }

    init(name: Swift.String, short: UnicodeScalar?, help: Swift.String, defaultValue: StorageType) {
        self.name = name
        self.short = short
        self.help = help
        self.valueStorage = defaultValue
        super.init()
    }

    var kind : popt_kind {
        fatalError("unimplemented")
    }

    override func convertedToC(on allocator: Allocator) -> popt_t {
        return popt_t(kind: self.kind,
                      shrt: Int32(self.short?.value ?? 0),
                       lng: self.name.duplicated(on: allocator).data,
                     value: &self.valueStorage,
                      init: 0,
                      help: self.help.duplicated(on: allocator).data,
                 int_vsize: MemoryLayout<StorageType>.size)
    }
}

/// Base class for command line options.
///
/// Command line options are split in two kinds:
/// - groups that allows grouping several switches
/// - individual switches that specify commande line parameters.
public class CommandLineOption {
    func convertedToC(on allocator: Allocator) -> popt_t {
        fatalError("unimplemented")
    }

    /// Defines a integer switch.
    public final class Integer : Parameter<Int, Int> {
        public override init(name: Swift.String, short: UnicodeScalar?, help: Swift.String, defaultValue: Int = 0) {
            super.init(name: name, short: short, help: help, defaultValue: defaultValue)
        }

        override var kind: popt_kind { return .int }
        public override var value : Int { return valueStorage }
    }

    /// Defines an unsigned integer switch.
    public final class UnsignedInteger : Parameter<UInt, UInt> {
        public override init(name: Swift.String, short: UnicodeScalar?, help: Swift.String, defaultValue: UInt = 0) {
            super.init(name: name, short: short, help: help, defaultValue: defaultValue)
        }

        override var kind: popt_kind { return .uint }
        public override var value : UInt { return valueStorage }
    }

    /// Defines a boolean switch.
    public final class Flag : Parameter<Bool, Bool> {
        public override init(name: Swift.String, short: UnicodeScalar?, help: Swift.String, defaultValue: Bool = false) {
            super.init(name: name, short: short, help: help, defaultValue: defaultValue)
        }

        override var kind : popt_kind { return .flag }
        public override var value : Bool { return valueStorage }
    }

    /// Defines a string parameter.
    public final class String : Parameter<Swift.String?, UnsafeMutablePointer<CChar>?> {
        var defaultValue : Swift.String?

        public init(name: Swift.String, short: UnicodeScalar?, help: Swift.String, defaultValue: Swift.String? = nil) {
            self.defaultValue = defaultValue
            super.init(name: name, short: short, help: help, defaultValue: nil)
        }

        override var kind : popt_kind { return .str }
        public override var value : Swift.String? {
            if let value = self.valueStorage {
                return Swift.String(cString: value)
            } else {
                return self.defaultValue
            }
        }
    }

    /// Defines the beginning of a switch groups.
    public final class Group : CommandLineOption {
        var name: Swift.String

        public init(name: Swift.String) {
            self.name = name
        }

        override func convertedToC(on allocator: Allocator) -> popt_t {
            return popt_t(kind: .group,
                          shrt: 0,
                           lng: nil,
                         value: nil,
                          init: 0,
                          help: self.name.duplicated(on: allocator).data,
                     int_vsize: 0)
        }
    }

    static func withPopt<Result>(options: [CommandLineOption], _ body: (UnsafeMutablePointer<popt_t>) throws -> Result) rethrows -> Result {
        return try tScope {
            (frame) in

            var array : [popt_t] = options.map { $0.convertedToC(on: frame) }

            array.append(popt_t(kind: .end,
                                shrt: 0,
                                 lng: nil,
                               value: nil,
                                init: 0,
                                help: nil,
                           int_vsize: 0))

            return try array.withUnsafeMutableBufferPointer {
                return try body($0.baseAddress!)
            }
        }
    }
}

public extension CommandLine {
    static var consumed : Int32 = 1

    /// Number of unconsummed arguments
    public static var remain : Int32 {
        get {
            return CommandLine.argc - CommandLine.consumed
        }

        set {
            CommandLine.consumed = CommandLine.argc - newValue
        }
    }

    /// Consume and returns the first unconsummed argument.
    public static func nextArg() -> String? {
        if CommandLine.consumed >= CommandLine.argc {
            return nil
        }
        let val = CommandLine.arguments[Int(CommandLine.consumed)]
        CommandLine.consumed += 1
        return val
    }

    /// Parse the command line according to a set of options.
    ///
    /// This function fills the content of the options according to the
    /// arguments received on the command line and consumes the corresponding
    /// arguments. Once executed, it updates the number of remaining argument
    /// so that only parameters that are not part of the switches remain.
    ///
    /// - Parameter options: ordered list of options.
    /// - Parameter flags: extra flags to apply.
    public static func parse(options: [CommandLineOption], flags: popt_options = []) {
        CommandLineOption.withPopt(options: options) {
            CommandLine.remain = parseopt(CommandLine.argc - CommandLine.consumed,
                                  CommandLine.unsafeArgv.advanced(by: Int(CommandLine.consumed)),
                                  $0, Int32(flags.rawValue))
        }
    }

    /// Build the usage message and exit the programs.
    ///
    /// - Parameter params: string describing the command line options not
    ///  already taken into account in the `options` switches.
    /// - Parameter options: list of the command line switches.
    public static func makeUsage(_ params: String, options: [CommandLineOption]) -> Never {
        return CommandLineOption.withPopt(options: options) {
            makeusage(EXIT_FAILURE, CommandLine.unsafeArgv[0]!, params, nil, $0)
        }
    }
}
