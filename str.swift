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

extension StringBuffer : CustomStringConvertible {
    public var description: String {
        return String(cString: self.data)
    }

    /// Append the UTF8 representation of `string` to the content of the buffer.
    mutating public func addString(_ string: String) {
        string.utf8CString.withUnsafeBufferPointer { ptr in
            self.addBuffer(ptr.baseAddress!, count: Int32(ptr.count - 1))
        }
    }
}

extension StringBuffer : Wipeable { }

extension LString : Wipeable { }
extension LString : Collection {
    public typealias Element = Int8
    public typealias Index = Int

    public var startIndex : Int {
        return 0
    }

    public var endIndex : Int {
        return Int(self.len)
    }

    public subscript(pos: Int) -> Int8 {
        return self.s![pos]
    }

    public func index(after pos: Int) -> Int {
        return pos + 1
    }
}

public extension String {
    /// Calls a closure with a view of the string UTF8 representation as a `LString`.
    ///
    /// - Parameter body: A closure with a LString parameter that points to the
    ///  UTF8 representation of the string. If `body` has a return value, it is
    ///  used as the return value of `withLString(_:)` method. The argument is
    ///  valid only for the duration of the closure's execution.
    ///
    /// - Returns: The return value of the `body` closure parameter
    public func withLString<Result>(_ body: (LString) throws -> Result) rethrows -> Result {
        return try self.utf8CString.withUnsafeBufferPointer { ptr in
            return try body(LString(ptr.baseAddress, count: Int32(ptr.count - 1), flags: 0))
        }
    }

    /// Create a new optional string from the content of a `LString`.
    ///
    /// The content of the LString is imported as a C string. The constructor returns
    /// `nil` if the received string is a `LSTR_NULL`.
    public init?(_ str: LString) {
        if str.data == nil {
            return nil
        }

        if str.len == 0 {
            self.init()
        } else {
            var arr = Array(str)
            arr.append(0)
            self.init(cString: arr)
        }
    }

    /// Duplicates the UTF8 representation of the string on the t_stack as a `LString`.
    ///
    /// The requested memory is automatically freed when exited the nearest `t_scope`.
    ///
    /// - warning: This must be called within an active `t_scope`
    public func duplicated(on allocator: Allocator) -> LString {
        return withLString {
            return mp_lstr_dup(allocator.pool, $0)
        }
    }
}
