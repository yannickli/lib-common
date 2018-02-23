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

/* {{{ QVector */

/// Protocol allowing using C's `qv_t()` types as Swift collections.
public protocol QVector : RandomAccessCollection,
                          RangeReplaceableCollection,
                          MutableCollection,
                          ExpressibleByArrayLiteral,
                          CustomStringConvertible,
                          CustomDebugStringConvertible,
                          Wipeable
{
    associatedtype Element
    typealias Index = Int32

    var tab : UnsafeMutablePointer<Element>? { get }
    var len : Int32 { get }
    var qv : qvector_t { get set }
    init(qv: qvector_t)
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
}

/* Implements RandomAccessCollection protocol */
extension QVector {
    public var startIndex : Int32 {
        return 0
    }

    public var endIndex : Int32 {
        return self.len
    }

    public subscript(pos: Int32) -> Element {
        get {
            let len = Int(self.len)
            let pos = Int(pos)

            precondition(pos < len, "cannot fetch element \(pos) of vector of len \(len)")
            return self.tab![pos]
        }

        set(newValue) {
            let len = Int(self.len)
            let pos = Int(pos)

            precondition(pos < len, "cannot fetch element \(pos) of vector of len \(len)")
            self.tab![pos] = newValue
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
extension QVector where MutableRangeReplaceableRandomAccessSlice<Self>.Index == Int32 {
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

/* Implements Custom*StringConvertible protocol */
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

/* Implements Wipeable protocol */
extension QVector {
    public mutating func wipe() {
        qvector_wipe(&self.qv, MemoryLayout<Element>.stride)
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
/* {{{ QHash */

/// Used to access the members in an instance of a `QHash` (`QSet` or `QMap`)
public struct QHashIndex : Comparable {
    let pos : UInt32

    init(pos: UInt32) {
        self.pos = pos
    }

    init?(pos: Int32) {
        if pos < 0 {
            return nil
        }
        self.init(pos: UInt32(pos))
    }

    public static func ==(_ a: QHashIndex, _ b: QHashIndex) -> Bool {
        return a.pos == b.pos
    }

    public static func <(_ a: QHashIndex, _ b: QHashIndex) -> Bool {
        return a.pos < b.pos
    }

    static func from(reservation res: UInt32) -> (pos: QHashIndex, collision: Bool) {
        if res & QHASH_COLLISION != 0 {
            return (pos: QHashIndex(pos: res & ~QHASH_COLLISION), collision: true)
        } else {
            return (pos: QHashIndex(pos: res), collision: false)
        }
    }

    static var endIndex = QHashIndex(pos: UInt32.max)
}

/// Base protocol for types built from C'container-qhash
public protocol QHash : Collection,
                        CustomStringConvertible,
                        CustomDebugStringConvertible,
                        Wipeable
{
    /// Type for keys in the QHash
    associatedtype Key
    typealias Index = QHashIndex

    var qh : qhash_t { get set }
    var hdr : qhash_hdr_t { get set }
    var keys : UnsafeMutablePointer<Key>? { get set }

    init(qh: qhash_t)

    /// Retrieve the index for `key`
    ///
    /// This function allows elements to be moved in order
    /// to optimize further lookups.
    mutating func find(key: Key) -> QHashIndex?

    /// Retrieve the index for `key`
    func findSafe(key: Key) -> QHashIndex?

    /// Allocates a slot in the `QHash` to store the value associated
    /// with `key`
    ///
    /// - Parameter key: the key to insert in the hash table.
    ///
    /// - Parameter overwrite: if true and `key` is already present in the
    ///  hash table, the key is replaced with the new one.
    ///
    /// - Returns: the position where the key will be in the table, as well
    ///  as a flag indicating wether a collision occurred.
    mutating func reserve(key: Key, overwrite: Bool) -> (pos: QHashIndex, collision: Bool)
}

/* Implements Collection */
extension QHash {
    public var startIndex : QHashIndex {
        if self.hdr.len != 0 {
            return self.index(after: QHashIndex(pos: 0))
        } else {
            return .endIndex
        }
    }

    public var endIndex : QHashIndex {
        return .endIndex
    }

    public func index(after pos: QHashIndex) -> QHashIndex {
        var qh = self.qh
        let nextPos = qhash_scan(&qh, pos.pos + 1)

        return QHashIndex(pos: nextPos)
    }
}

/* Implements Custom*StringConvertible protocol */
extension QHash {
    internal func _makeDescription(isDebug: Bool) -> String {
        var result = isDebug ? "QHash([" : "["
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

/* Implements Wipeable protocol */
extension QHash {
    public mutating func wipe() {
        qhash_wipe(&qh)
    }
}

/// Protocol for tables where the key is the value to hash.
///
/// This protocol provides a specialized interface for hash tables whose
/// key type is the direct base for the hash, such as integers or hashed
/// pointers (that is pointers for which the address is the key, not the
/// pointed content).
public protocol QHashSimple : QHash {
    associatedtype Key
    typealias FKey = Key
    typealias CKey = Key

    /* C functions */
    mutating func _find(hash: UnsafePointer<UInt32>?, key: Key) -> Int32
    func _findSafe(hash: UnsafePointer<UInt32>?, key: Key) -> Int32
    mutating func _reserve(hash: UnsafePointer<UInt32>?, key: Key, flags: UInt32) -> UInt32
}

extension QHashSimple {
    public mutating func find(key: Key) -> QHashIndex? {
        return QHashIndex(pos: self._find(hash: nil, key: key))
    }

    public func findSafe(key: Key) -> QHashIndex? {
        return QHashIndex(pos: self._findSafe(hash: nil, key: key))
    }

    public mutating func reserve(key: Key, overwrite: Bool = true) -> (pos: QHashIndex, collision: Bool) {
        return QHashIndex.from(reservation: self._reserve(hash: nil, key: key, flags: overwrite ?  QHASH_OVERWRITE : 0))
    }
}

/// Protocol for tables where keys are complex types stored by value.
public protocol QHashVec : QHash {
    associatedtype Key

    /* C functions */
    mutating func _find(hash: UnsafePointer<UInt32>?, key: UnsafePointer<Key>) -> Int32
    func _findSafe(hash: UnsafePointer<UInt32>?, key: UnsafePointer<Key>) -> Int32
    mutating func _reserve(hash: UnsafePointer<UInt32>?, key: UnsafePointer<Key>, flags: UInt32) -> UInt32
}

extension QHashVec {
    public mutating func find(key: Key) -> QHashIndex? {
        var key = key

        return QHashIndex(pos: self._find(hash: nil, key: &key))
    }

    public func findSafe(key: Key) -> QHashIndex? {
        var key = key

        return QHashIndex(pos: self._findSafe(hash: nil, key: &key))
    }

    public mutating func reserve(key: Key, overwrite: Bool = true) -> (pos: QHashIndex, collision: Bool) {
        var key = key
        let res = self._reserve(hash: nil, key: &key, flags: overwrite ? QHASH_OVERWRITE : 0)

        return QHashIndex.from(reservation: res)
    }
}

/// Protocol for tables where keys are complex types stored by reference.
public protocol QHashPtr : QHash {
    associatedtype Pointee

    mutating func _find(hash: UnsafePointer<UInt32>?, key: UnsafePointer<Pointee>?) -> Int32
    func _findSafe(hash: UnsafePointer<UInt32>?, key: UnsafePointer<Pointee>?) -> Int32
    mutating func _reserve(hash: UnsafePointer<UInt32>?, key: UnsafeMutablePointer<Pointee>?, flags: UInt32) -> UInt32
}

extension QHashPtr {
    public mutating func find(key: UnsafeMutablePointer<Pointee>?) -> QHashIndex? {
        return QHashIndex(pos: self._find(hash: nil, key: key))
    }

    public mutating func find(key: UnsafePointer<Pointee>?) -> QHashIndex? {
        return QHashIndex(pos: self._find(hash: nil, key: key))
    }

    public func findSafe(key: UnsafeMutablePointer<Pointee>?) -> QHashIndex? {
        return QHashIndex(pos: self._findSafe(hash: nil, key: key))
    }

    public func findSafe(key: UnsafePointer<Pointee>?) -> QHashIndex? {
        return QHashIndex(pos: self._findSafe(hash: nil, key: key))
    }

    public mutating func reserve(key: UnsafeMutablePointer<Pointee>?, overwrite: Bool = true) -> (pos: QHashIndex, collision: Bool) {
        let res = self._reserve(hash: nil, key: key, flags: overwrite ?  QHASH_OVERWRITE : 0)

        return QHashIndex.from(reservation: res)
    }
}

/// Protocol for tables where key are void * pointers.
public protocol QHashVoid : QHash {
    mutating func _find(hash: UnsafePointer<UInt32>?, key: UnsafeRawPointer?) -> Int32
    func _findSafe(hash: UnsafePointer<UInt32>?, key: UnsafeRawPointer?) -> Int32
    mutating func _reserve(hash: UnsafePointer<UInt32>?, key: UnsafeMutableRawPointer?, flags: UInt32) -> UInt32
}

extension QHashVoid {
    public mutating func find(key: UnsafeMutableRawPointer?) -> QHashIndex? {
        return QHashIndex(pos: self._find(hash: nil, key: key))
    }

    public mutating func find(key: UnsafeRawPointer?) -> QHashIndex? {
        return QHashIndex(pos: self._find(hash: nil, key: key))
    }

    public func findSafe(key: UnsafeMutableRawPointer?) -> QHashIndex? {
        return QHashIndex(pos: self._findSafe(hash: nil, key: key))
    }

    public func findSafe(key: UnsafeRawPointer?) -> QHashIndex? {
        return QHashIndex(pos: self._findSafe(hash: nil, key: key))
    }

    public mutating func reserve(key: UnsafeMutableRawPointer?, overwrite: Bool = true) -> (pos: QHashIndex, collision: Bool) {
        let res = self._reserve(hash: nil, key: key, flags: overwrite ?  QHASH_OVERWRITE : 0)

        return QHashIndex.from(reservation: res)
    }
}

/* }}} */
/* {{{ QSet */

/// An unordered collection of unique elements mapped on C's `qh_t()` types.
public protocol QSet : QHash, ExpressibleByArrayLiteral {
}

extension QSet {
    public init(on allocator: Allocator, withCapacity size: Int) {
        var qh = qhash_t();

        qhash_init(&qh, UInt16(MemoryLayout<Key>.stride), 0, false, allocator.pool)
        qhash_set_minsize(&qh, UInt32(size))
        self.init(qh: qh)
    }

    public init(on allocator: Allocator, withElements newElements: Key...) {
        self.init(on: allocator, withCapacity: newElements.count)
        for elm in newElements {
            let _ = self.reserve(key: elm, overwrite: true)
        }
    }

    public init(arrayLiteral newElements: Key...) {
        self.init(on: StandardAllocator.malloc, withCapacity: newElements.count)
        for elm in newElements {
            let _ = self.reserve(key: elm, overwrite: true)
        }
    }

    public func index(of key: Key) -> QHashIndex? {
        return self.findSafe(key: key)
    }

    public func contains(_ key: Key) -> Bool {
        return self.index(of: key) != nil
    }

    public func element(at pos: QHashIndex) -> Key {
        return self.keys![Int(pos.pos)]
    }

    public mutating func remove(at pos: QHashIndex) -> Key {
        let elm = self.element(at: pos)

        qhash_del_at(&qh, pos.pos)
        return elm
    }

    public mutating func remove(_ key: Key) -> Key? {
        guard let pos = self.index(of: key) else {
            return nil
        }
        return self.remove(at: pos)
    }

    public subscript(pos: QHashIndex) -> Key {
        get {
            return self.element(at: pos)
        }

        set(newValue) {
            self.keys![Int(pos.pos)] = newValue
        }
    }
}

protocol QSetSimple : QSet, QHashSimple { }
protocol QSetVec : QSet, QHashVec { }
protocol QSetPtr : QSet, QHashPtr { }
protocol QSetVoid : QSet, QHashVoid { }

extension qh_u32_t : QSetSimple { }
extension qh_u64_t : QSetSimple { }
extension qh_lstr_t : QSetVec { }
extension qh_str_t : QSetPtr { }
extension qh_cstr_t : QSetSimple { }
extension qh_ptr_t : QSetVoid { }
extension qh_cptr_t : QSetSimple { }

/* }}} */
/* {{{ QMap */

/// A unordered Key-Value map based on C's `qm_t()` types.
public protocol QMap : QHash, ExpressibleByDictionaryLiteral {
    associatedtype Value
    typealias Element = (key: Key, value: Value)

    var values : UnsafeMutablePointer<Value>? { get set }
}

extension QMap {
    public init(on allocator: Allocator, withCapacity size: Int) {
        var qh = qhash_t();

        qhash_init(&qh, UInt16(MemoryLayout<Key>.stride),
                   UInt16(MemoryLayout<Value>.stride), false, allocator.pool)
        qhash_set_minsize(&qh, UInt32(size))
        self.init(qh: qh)
    }

    public init(on allocator: Allocator, withElements newElements: (Key, Value)...) {
        self.init(on: allocator, withCapacity: newElements.count)
        for elm in newElements {
            self[self.reserve(key: elm.0, overwrite: true).pos] = elm
        }
    }

    public init(dictionaryLiteral newElements: (Key, Value)...) {
        self.init(on: StandardAllocator.malloc, withCapacity: newElements.count)
        for elm in newElements {
            self[self.reserve(key: elm.0, overwrite: true).pos] = elm
        }
    }

    public func index(forKey key: Key) -> QHashIndex? {
        return self.findSafe(key: key)
    }

    public func contains(key: Key) -> Bool {
        return self.index(forKey: key) != nil
    }

    public func element(at pos: QHashIndex) -> (key: Key, value: Value) {
        let key = self.keys![Int(pos.pos)]
        let value = self.values![Int(pos.pos)]

        return (key: key, value: value)
    }

    public mutating func remove(at pos: QHashIndex) -> (key: Key, value: Value) {
        let elm = self.element(at: pos)

        qhash_del_at(&qh, pos.pos)
        return elm
    }

    public mutating func remove(key: Key) -> (key: Key, value: Value)? {
        guard let pos = self.index(forKey: key) else {
            return nil
        }
        return self.remove(at: pos)
    }

    public subscript(pos: QHashIndex) -> (key: Key, value: Value) {
        get {
            return self.element(at: pos)
        }

        set {
            let (key, value) = newValue

            self.keys![Int(pos.pos)] = key
            self.values![Int(pos.pos)] = value
        }
    }

    public subscript(key: Key) -> Value? {
        get {
            guard let pos = self.findSafe(key: key) else {
                return nil
            }

            return self.values![Int(pos.pos)]
        }

        set {
            if let newValue = newValue {
                self[self.reserve(key: key, overwrite: true).pos] = (key: key, value: newValue)
            } else {
                _ = self.remove(key: key)
            }
        }
    }
}

extension QMap where Value : Equatable {
    public func index(of elm: (Key, Value)) -> QHashIndex? {
        guard let pos = self.findSafe(key: elm.0) else {
            return nil
        }
        if self[pos].1 != elm.1 {
            return nil
        }
        return pos
    }

    public func contains(_ elm: (Key, Value)) -> Bool {
        return self.index(of: elm) != nil
    }

    public mutating func remove(_ elm: (Key, Value)) -> (key: Key, value: Value)? {
        guard let pos = self.index(of: elm) else {
            return nil
        }
        return self.remove(at: pos)
    }
}

public protocol QMapSimple : QMap, QHashSimple { }
public protocol QMapVec : QMap, QHashVec { }
public protocol QMapPtr : QMap, QHashPtr { }
public protocol QMapVoid : QMap, QHashVoid { }

/* }}} */
