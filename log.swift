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

import Glibc

extension Logger : Wipeable {
    /// Build a new static logger.
    ///
    /// Build a new logger whose lifetime is the one of the program.
    ///
    /// - Parameter name: the name of the logger. That name will be concatenated
    ///  with `parent`'s name to build the fullname of the logger.
    /// - Parameter parent: the parent of the logger.
    /// - Parameter level: default logging level of the logger.
    public init(name: StaticString,
                parent: UnsafeMutablePointer<Logger>? = nil,
                level: core.LogLevel = LOG_LEVEL_INHERITS)
    {
        self.init()
        assert (name.hasPointerRepresentation)

        self.is_static = true
        self.parent = parent
        self.name = name.utf8Start.withMemoryRebound(to: Int8.self, capacity: name.utf8CodeUnitCount) {
            (ptr: UnsafePointer<Int8>) -> LString in

            let len = strlen(ptr)
            return LString(ptr, count: Int32(len), flags: 0)
        }
        self.level = LOG_UNDEFINED
        self.defined_level = LOG_UNDEFINED
        self.default_level = Int32(level.rawValue)
    }

    mutating func format<C: Collection>(items: C, separator: String) -> String {
        var out = ""

        for item in items {
            if !out.isEmpty && !separator.isEmpty {
                out.write(separator)
            }
            print(item, terminator: "", to: &out)
        }
        return out
    }

    mutating func log<C: Collection>(level: Int32, items: C, separator: String, file: String, function: String, line: Int) {
        _ = self.format(items: items, separator: separator).withLString {
            withVaList([$0.len, $0.data!]) {
                logger_vlog(&self, level, nil, -1, file, function, Int32(line), "%*pM", $0)
            }
        }
    }

    mutating func log<C: Collection>(level: core.LogLevel, items: C, separator: String, file: String, function: String, line: Int) {
        let level = Int32(level.rawValue)

        if !logger_has_level(&self, level) {
            return
        }

        self.log(level: level, items: items, separator: separator, file: file, function: function, line: line)
    }

    /// Emit a log line with the specified criticity `level`
    public mutating func log(level: core.LogLevel, _ items: Any..., separator: String = " ", file: String = #file, function: String = #function, line: Int = #line) {
        self.log(level: level, items: items, separator: separator, file: file, function: function, line: line)
    }

    /// Emit an error log.
    public mutating func error(_ items: Any..., separator: String = " ", file: String = #file, function: String = #function, line: Int = #line) {
        self.log(level: LOG_LEVEL_ERR, items: items, separator: separator, file: file, function: function, line: line)
    }

    /// Emit a warning log.
    public mutating func warning(_ items: Any..., separator: String = " ", file: String = #file, function: String = #function, line: Int = #line) {
        self.log(level: LOG_LEVEL_WARNING, items: items, separator: separator, file: file, function: function, line: line)
    }

    /// Emit a notice log.
    public mutating func notice(_ items: Any..., separator: String = " ", file: String = #file, function: String = #function, line: Int = #line) {
        self.log(level: LOG_LEVEL_NOTICE, items: items, separator: separator, file: file, function: function, line: line)
    }

    /// Emit an informational log.
    public mutating func info(_ items: Any..., separator: String = " ", file: String = #file, function: String = #function, line: Int = #line) {
        self.log(level: LOG_LEVEL_INFO, items: items, separator: separator, file: file, function: function, line: line)
    }

    /// Emit a debug log.
    public mutating func debug(_ items: Any..., separator: String = " ", file: String = #file, function: String = #function, line: Int = #line) {
        self.log(level: LOG_LEVEL_DEBUG, items: items, separator: separator, file: file, function: function, line: line)
    }

    /// Emit a trace line.
    ///
    /// Traces are defined by increasing `level`: the greater the level
    /// is, the more specific the trace is.
    public mutating func trace(level: Int32, _ items: Any..., separator: String = " ", file: String = #file, function: String = #function, line: Int = #line) {
        let logLevel = level + Int32(LOG_LEVEL_TRACE.rawValue)

        if !logger_has_level(&self, logLevel) && __logger_is_traced(&self, level, file, function, self.full_name.data) <= 0 {
            return
        }

        self.log(level: logLevel, items: items, separator: separator, file: file, function: function, line: line)
    }

    /// Emit a log and cleaning exit the program.
    public mutating func exit(_ items: Any..., separator: String = " ", file: String = #file, function: String = #function, line: Int = #line) -> Never {
        return self.format(items: items, separator: separator).withCString {
            return withVaList([$0]) {
                __logger_vexit(&self, file, function, Int32(line), "%s", $0)
            }
        }
    }

    /// Emit a log and terminate the execution of the program.
    public mutating func fatal(_ items: Any..., separator: String = " ", file: String = #file, function: String = #function, line: Int = #line) -> Never {
        return self.format(items: items, separator: separator).withCString {
            return withVaList([$0]) {
                __logger_vfatal(&self, file, function, Int32(line), "%s", $0)
            }
        }
    }

    /// Emit a log a abort the execution of the program.
    public mutating func panic(_ items: Any..., separator: String = " ", file: String = #file, function: String = #function, line: Int = #line) -> Never {
        return self.format(items: items, separator: separator).withCString {
            return withVaList([$0]) {
                __logger_vpanic(&self, file, function, Int32(line), "%s", $0)
            }
        }
    }
}
