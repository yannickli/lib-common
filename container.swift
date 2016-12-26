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

/* {{{ QVector */

/// Protocol allowing using C's `qv_t()` types as Swift collections.
public protocol QVector : RandomAccessCollection,
                          RangeReplaceableCollection,
                          MutableCollection,
                          ExpressibleByArrayLiteral,
                          CustomStringConvertible,
                          CustomDebugStringConvertible
{
    associatedtype Element
    typealias Index = Int32
    typealias Comparator = (UnsafePointer<Element>, UnsafePointer<Element>) -> Int32

    var qv : qvector_t { get set }
    init(qv: qvector_t)

    /* Exposed C APIs */
    mutating func _sorted(cmp: @escaping Comparator)
}

/* Implements base accessors */
extension QVector {
    public init(on allocator: Allocator, withCapacity size: Int32) {
        var qv = qvector_t()
        let mem = UnsafeMutablePointer<Element>.allocate(capacity: Int(size), on: allocator)

        __qvector_init(&qv, mem, 0, size, allocator.pool)
        self.init(qv: qv)
    }

    public init(on allocator: Allocator, withElements newElements: Element...) {
        self.init(on: allocator, withCapacity: Int32(newElements.count))
        self.append(contentOf: newElements)
    }

    public mutating func wipe() {
        qvector_wipe(&self.qv, MemoryLayout<Element>.stride)
    }
}

/* Implements RandomAccessCollection protocol */
extension QVector {
    public var startIndex : Int32 {
        return 0
    }

    public var endIndex : Int32 {
        return qv.len
    }

    public subscript(pos: Int32) -> Element {
        get {
            let len = Int(qv.len)
            let pos = Int(pos)
            let tab = qv.tab!

            precondition(pos < len, "cannot fetch element \(pos) of vector of len \(len)")
            return tab.withMemoryRebound(to: Element.self, capacity: len) { $0[pos] }
        }

        set(newValue) {
            let len = Int(qv.len)
            let pos = Int(pos)
            let tab = qv.tab!

            precondition(pos < len, "cannot fetch element \(pos) of vector of len \(len)")
            tab.withMemoryRebound(to: Element.self, capacity: len) {
                $0[pos] = newValue
            }
        }
    }

    public func index(after pos: Int32) -> Int32 {
        return pos + 1
    }

    public func index(before pos: Int32) -> Int32 {
        return pos - 1
    }
}

/* Implements RangeReplaceableCollection protocol */
extension QVector {
    public mutating func replaceSubrange<C : Collection>(_ range: Range<Int32>, with
   newElements: C) where C.Iterator.Element == Element {
        let removeCount = range.count
        let insertCount = Int32(newElements.count.toIntMax())
        let mem = __qvector_splice(&self.qv, Int(MemoryLayout<Element>.stride),
                                   Int(MemoryLayout<Element>.alignment),
                                   range.lowerBound, Int32(removeCount),
                                   insertCount)

        var buf = mem.bindMemory(to: Element.self, capacity: Int(insertCount))
        for elt in newElements {
            buf[0] = elt
            buf += 1
        }
    }

    mutating func growlen<Result>(by extra: Int32,
                                  cb: (UnsafeMutablePointer<Element>) -> Result) -> Result {
        let mem = qvector_growlen(&self.qv, MemoryLayout<Element>.stride,
                                  MemoryLayout<Element>.alignment, extra)

        return cb(mem.bindMemory(to: Element.self, capacity: Int(extra)))
    }

    public mutating func append(_ elm: Element) {
        self.growlen(by: 1) { $0[0] = elm }
    }

    public mutating func append<C : Collection>(contentOf newElements: C) where C.Iterator.Element == Element {
        self.growlen(by: Int32(newElements.count.toIntMax())) {
            var buf = $0

            for elt in newElements {
                buf[0] = elt
                buf += 1
            }
        }
    }

    public mutating func append<S : Sequence>(contentOf newElements: S) where S.Iterator.Element == Element {
        for lit in newElements {
            self.append(lit)
        }
    }

    public mutating func removeAll(keepingCapacity: Bool = true) {
        qvector_reset(&self.qv, MemoryLayout<Element>.stride)
        if !keepingCapacity {
            qvector_optimize(&self.qv, MemoryLayout<Element>.stride,
                             MemoryLayout<Element>.alignment, 0, 0)
        }
    }

    public mutating func reserveCapacity(_ extra: Int32) {
        let _ = qvector_grow(&qv, MemoryLayout<Element>.stride,
                             MemoryLayout<Element>.alignment, extra)
    }
}

/* Implements MutableCollection protocol */
extension QVector {
    public subscript(bounds: Range<Int32>) -> MutableRangeReplaceableRandomAccessSlice<Self> {
        get {
            return .init(base: self, bounds: bounds)
        }

        set {
            self.replaceSubrange(bounds, with: newValue)
        }
    }
}

/* Implements ExpressibleByArrayLiteral protocol */
extension QVector {
    public init(arrayLiteral newElements: Element...) {
        self.init(qv: qvector_t())
        self.append(contentOf: newElements)
    }
}

/* Implements Custom*StringConvertible */
extension QVector {
    internal func _makeDescription(isDebug: Bool) -> String {
        var result = isDebug ? "QVector([" : "["
        var first = true
        for item in self {
            if first {
                first = false
            } else {
                result += ", "
            }
            debugPrint(item, terminator: "", to: &result)
        }
        result += isDebug ? "])" : "]"
        return result
    }

    /// A textual representation of the array and its elements.
    public var description: String {
        return _makeDescription(isDebug: false)
    }

    /// A textual representation of the array and its elements, suitable for
    /// debugging.
    public var debugDescription: String {
        return _makeDescription(isDebug: true)
    }
}

extension qv_i8_t : QVector { }
extension qv_u8_t : QVector { }
extension qv_i16_t : QVector { }
extension qv_u16_t : QVector { }
extension qv_i32_t : QVector { }
extension qv_u32_t : QVector { }
extension qv_i64_t : QVector { }
extension qv_u64_t : QVector { }
extension qv_void_t : QVector { }
extension qv_double_t : QVector { }
extension qv_str_t : QVector { }
extension qv_lstr_t : QVector { }
extension qv_pstream_t : QVector { }
extension qv_cvoid_t : QVector { }
extension qv_cstr_t : QVector { }
extension qv_sbp_t : QVector { }

/* }}} */
