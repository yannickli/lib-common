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

/* Utilities {{{ */

/// Errors emitted during the conversion from C object to Swift object.
public enum IopImportError : Error {
    /// Import failed because the class has not been registered.
    case unregisteredClass(UnsafePointer<iop_struct_t>)

    /// Import failed because classes are unrelated.
    ///
    /// This error is emitted when trying to build a Swift class from a
    /// C object that does not represent an object of a child class
    /// of that Swift class.
    case notAChild(UnsafePointer<iop_struct_t>, IopClass.Type)

    /// Import failed because of an invalid union tag.
    case invalidUnionTag(UnsafePointer<iop_struct_t>, UInt16)
}

/* }}} */
/* Simple optionals {{{ */

/// Protocol describing C simple types optionals.
public protocol IopOptional {
    associatedtype Element

    var v: Element { get set }
    var has_field : Bool { get set }
    init()
}

public extension IopOptional {
    public var value: Element? {
        if has_field {
            return v
        }
        return nil
    }

    public init(_ value: Element?) {
        self.init()
        if let value = value {
            has_field = true
            v = value
        }
    }

    public static func from(_ value: Element?) -> Self {
        return Self(value)
    }
}

extension opt_i8_t : IopOptional { }
extension opt_u8_t : IopOptional { }
extension opt_i16_t : IopOptional { }
extension opt_u16_t : IopOptional { }
extension opt_i32_t : IopOptional { }
extension opt_u32_t : IopOptional { }
extension opt_i64_t : IopOptional { }
extension opt_u64_t : IopOptional { }
extension opt_enum_t : IopOptional { }
extension opt_double_t : IopOptional { }
extension opt_bool_t : IopOptional { }

/* }}} */
/* Arrays {{{ */

/// Protocol describing C IOP arrays.
///
/// This protocol allows mapping the C arrays to collections
public protocol IopArray : Collection {
    associatedtype Element

    var tab: UnsafeMutablePointer<Element>? { get set }
    var len: Int32 { get set }

    init()
}

/* implements Collection protocol */
public extension IopArray {
    public var startIndex: Int {
        return 0
    }

    public var endIndex: Int {
        return Int(len)
    }

    public subscript(pos: Int) -> Element {
        let len = Int(self.len)
        let tab = self.tab!

        precondition(pos < len, "cannot fetch element \(pos) of vector of len \(len)")
        return tab[pos]
    }

    public func index(after pos: Int) -> Int {
        return pos + 1
    }

    public func index(before pos: Int) -> Int {
        return pos - 1
    }
}

/// C array of simple types.
public protocol IopSimpleArray : IopArray {
}

public extension IopSimpleArray {
    public init(_ array: [Element], on allocator: Allocator) {
        self.init()
        self.tab = array.duplicated(on: allocator)
        self.len = Int32(array.count)
    }
}

extension iop_array_i8_t : IopSimpleArray { }
extension iop_array_u8_t : IopSimpleArray { }
extension iop_array_i16_t : IopSimpleArray { }
extension iop_array_u16_t : IopSimpleArray { }
extension iop_array_i32_t : IopSimpleArray { }
extension iop_array_u32_t : IopSimpleArray { }
extension iop_array_i64_t : IopSimpleArray { }
extension iop_array_u64_t : IopSimpleArray { }
extension iop_array_bool_t : IopSimpleArray { }
extension iop_array_double_t : IopSimpleArray { }

extension iop_array_lstr_t : IopArray {
    public init(_ array: [String], on allocator: Allocator) {
        let vec = array.map { $0.duplicated(on: allocator) }

        self.init()
        self.tab = vec.duplicated(on: allocator)
        self.len = Int32(vec.count)
    }

    public init(_ array: [[Int8]], on allocator: Allocator) {
        let vec = array.map { LString($0.duplicated(on: allocator), count: Int32($0.count), flags: 0) }

        self.init()
        self.tab = vec.duplicated(on: allocator)
        self.len = Int32(vec.count)
    }
}

/* }}} */
/* Enum {{{ */

/// Type for all IOP enum.
///
/// IOP enums are directly imported from C, we only add the
/// necessary attributes to conform to this protocol, which
/// allows conversion to string as well as access to extra
/// metadata such as the number of entries, the minimum value
/// and the maximum value.
///
/// Depending on whether the enum is tagged @strict or not, it
/// will however be imported either as a Swift enum or a simple
/// RawRepresentable object without particular constraints.
public protocol IopEnum : RawRepresentable,
                          CustomStringConvertible
{
    typealias RawValue = Int32

    static var descriptor: UnsafePointer<iop_enum_t> { get }
    static var min : Int32 { get }
    static var max : Int32 { get }
    static var count: Int32 { get }
}

extension IopEnum {
    public var description: String {
        guard let data = iop_enum_to_str_desc(Self.descriptor, self.rawValue).data else {
            return String(self.rawValue)
        }

        return String(cString: data)
    }
}

/* }}} */
/* Struct {{{ */

/// Type for all IOP complex objects.
///
/// This protocol defines the interface used to import object from C as
/// well as create a C object from Swift.
public protocol IopComplexType: CustomStringConvertible,
                                CustomDebugStringConvertible
{
    /// Create a new object from the content of a C instance.
    ///
    /// This API is unsafe since it requires `c` to have the very right C type.
    init(_ c: UnsafeRawPointer) throws

    /// Create a new object from the conten tof a C instance.
    ///
    /// This API handle the case where we receive NULL pointers from C, which
    /// can occur either because the object is optional and not set, or because
    /// the object is of the Void type and thus has not associated data.
    init?(_ c: UnsafeRawPointer?) throws

    /// Fill a preallocated buffer representing a C instance for the same IOP
    /// type as `Self` with the content of the fields.
    func fill(_: UnsafeMutableRawPointer, on allocator: FrameBasedAllocator)

    /// Call `body` with a pointer to C instance representing `Self`.
    ///
    /// This function allocates and fill a structure representing the right C
    /// type for the current object and then call the `body` closre. The
    /// provided object must not be returned enterely or partially by `body`.
    ///
    /// - Returns: the value returned by `body`, if any.
    func withC<Result>(_ body: (UnsafeRawPointer) throws -> Result) rethrows -> Result

    /// C Type descriptor for `Self`.
    static var descriptor: UnsafePointer<iop_struct_t> { get }
}

public extension IopComplexType {
    public func duplicated(on allocator: FrameBasedAllocator) -> UnsafeMutableRawPointer {
        let desc = type(of: self).descriptor
        let buf = t_iop_new_desc(desc)

        self.fill(buf, on: allocator)
        return buf
    }

    public init?(_ c: UnsafeRawPointer?) throws {
        guard let c = c else {
            return nil
        }
        try self.init(c)
    }

    public func withC<Result>(_ body: (UnsafeRawPointer) throws -> Result) rethrows -> Result {
        return try tScope {
            return try body(self.duplicated(on: $0))
        }
    }

    public func JSON(_ flags: iop_jpack_flags = []) -> String {
        return withC {
            let desc = type(of: self).descriptor
            let buf = AutoWipe(StringBuffer())

            iop_sb_jpack(&buf.wrapped, desc, $0, flags.rawValue)
            buf.wrapped.shrink(1)
            return buf.description
        }
    }

    public var description: String {
        return JSON(.minimal)
    }

    public var debugDescription: String {
        return JSON()
    }

    public static func make(_ c: UnsafeRawPointer) throws -> Self {
        return try Self(c)
    }

    public static func make(optional c: UnsafeRawPointer?) throws -> Self? {
        return try Self(c)
    }
}

public protocol IopComplexTypeArray : IopArray {
}

public extension IopComplexTypeArray {
    public init<T: IopComplexType>(_ array: [T], on allocator: FrameBasedAllocator) {
        let vec = array.map {
            $0.duplicated(on: allocator).bindMemory(to: Self.Element.self, capacity: 1).pointee
        }

        self.init()
        self.tab = vec.duplicated(on: allocator)
        self.len = Int32(vec.count)
    }
}

/// Type of an IOP struct.
///
/// IOP struct are imported in Swift as either struct or classes depending on whether
/// the type is recursive or not. In both case the type fully implements the IopStruct
/// protocol ensuring we can interract with C code.
///
/// Fields from a structure are imported as-is, after mapping their type to the
/// corresponding Swift type.
public typealias IopStruct = IopComplexType

/// Representation of the IOP Void type.
public struct IopVoid : IopComplexType {
    public static let descriptor = iop__void__sp

    public init() {
    }

    public init(_ c: UnsafeRawPointer) {
    }

    public init(_ c: UnsafeRawPointer?) {
    }

    public func fill(_ c: UnsafeMutableRawPointer, on allocator: FrameBasedAllocator) {
    }
}

/* }}} */
/* Union {{{ */

/// Type of an IOP union.
///
/// IOP unions are represented in Swift as union. Each field of the union is a case
/// of the enum whose associated value is the content of the field.
///
/// For example:
///
///     union MyUnion {
///         MyStruct a;
///         int b;
///     }
///
/// Will be imported in Swift as
///
///     enum MyUnion {
///         case a(MyStruct)
///         case b(Int32)
///     }
///
/// In case the union might include itself, it automatically becomes an indirect enum.
public protocol IopUnion : IopComplexType {
}

public extension IopUnion {
    public static func _explode(_ data: UnsafeRawPointer) -> (UInt16, UnsafeRawPointer) {
        let tag = data.bindMemory(to: UInt16.self, capacity: 1).pointee
        let desc = Self.descriptor

        let off = desc.pointee.fields[0].data_offs
        return (tag, data.advanced(by: Int(off)))
    }

    public static func _explode(_ data: UnsafeMutableRawPointer) -> (UnsafeMutablePointer<UInt16>, UnsafeMutableRawPointer) {
        let tag = data.bindMemory(to: UInt16.self, capacity: 1)
        let desc = Self.descriptor

        let off = desc.pointee.fields[0].data_offs
        return (tag, data.advanced(by: Int(off)))
    }
}

/* }}} */
/* Class {{{ */

/// Base class for all IOP classes
///
/// IOP classes are imported as Swift classes which mirror the inheritance line. All
/// IOP base classes inherit from `IopClass` and must provide their own impoter/exporter.
///
/// Classes that are declared `local` in IOP are automatically marked as `final` in Swift
/// if they have no child in their package.
open class IopClass : IopComplexType {
    public static func descriptor(of buffer: UnsafeRawPointer) -> UnsafePointer<iop_struct_t> {
        return buffer.bindMemory(to: UnsafePointer<iop_struct_t>.self, capacity: 1)[0]
    }

    open class var descriptor: UnsafePointer<iop_struct_t> {
        fatalError("This property must be provided")
    }

    open class var isAbstract: Bool {
        fatalError("This property must be provided")
    }

    public init() {
        precondition(!type(of: self).isAbstract, "cannot instantiate abstract classes")
    }

    public required init(_ c: UnsafeRawPointer) throws {
        precondition(!type(of: self).isAbstract, "cannot instantiate abstract classes")
        precondition(IopClass.descriptor(of: c) == type(of: self).descriptor,
                     "type mismatch for object: use IopClass.build(fromC:)")
    }

    open func fill(_ c: UnsafeMutableRawPointer, on allocator: FrameBasedAllocator) {
        precondition(false, "Unimplemented")
    }

    public class func make(_ buffer: UnsafeRawPointer, using env: IopEnv = .defaultEnv) throws -> Self {
        return try env.makeObject(buffer)
    }

    public class func make(optional buffer: UnsafeRawPointer?, using env: IopEnv = .defaultEnv) throws -> Self? {
        guard let buffer = buffer else {
            return nil
        }
        return try .make(buffer, using: env)
    }
}

public protocol IopClassArray : IopArray {
}

public extension IopClassArray {
    public init<T: IopClass>(_ array: [T], on allocator: FrameBasedAllocator) {
        let vec = array.map {
            $0.duplicated(on: allocator)
        }

        self.init()
        self.tab = UnsafeMutableRawPointer(vec.duplicated(on: allocator)).bindMemory(to: Self.Element.self, capacity: vec.count)
        self.len = Int32(vec.count)
    }
}

/* }}} */
/* {{{ Environment */

/// Environment in which IOP packages are registered.
///
/// An environment is an object against which packages are registered
/// and from which reflection queries are performed. In particular,
/// an environment maps the class descriptors to their types in order to
/// allow their instantiation.
public class IopEnv {
    static let defaultEnv = IopEnv()

    var classRegistry : [UnsafePointer<iop_struct_t>:IopClass.Type] = [:]

    func makeObject<T: IopClass>(_ buffer: UnsafeRawPointer) throws -> T {
        let desc = IopClass.descriptor(of: buffer)
        guard let ctor = classRegistry[IopClass.descriptor(of: buffer)] else {
            throw IopImportError.unregisteredClass(desc)
        }

        guard let typedCtor = ctor as? T.Type else {
            throw IopImportError.notAChild(desc, T.self)
        }

        return try typedCtor.init(buffer)
    }

    /// Register `package` against the environment.
    public func register<T: IopPackage>(package: T.Type) {
        for cls in T.classes {
            classRegistry[cls.descriptor] = cls
        }
    }

    /// Register `package` against the default environment.
    public static func register<T: IopPackage>(package: T.Type) {
        IopEnv.defaultEnv.register(package: T.self)
    }
}

/* }}} */

/// Type for IOP package.
///
/// An IOP package is a namespace that contains a set of types, interfaces and modules.
/// A package must be registered before its classes can be instantiated from C objects.
public protocol IopPackage {
    static var classes: [IopClass.Type] { get }
}
