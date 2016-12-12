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
}
