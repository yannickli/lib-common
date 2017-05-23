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

#if os(Linux)
import Glibc

private let SOCK_STREAM = Int32(Glibc.SOCK_STREAM.rawValue)
#else
import Darwin

private let SOCK_STREAM = Darwin.SOCK_STREAM
#endif

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
        guard let data = iop_enum_to_str_desc(Self.descriptor, self.rawValue as! Int32).s else {
            return String(describing: self.rawValue)
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
                                CustomDebugStringConvertible,
                                Equatable
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

public func ==<T: IopComplexType>(lhs : T, rhs : T) -> Bool {
    return lhs.withC { lhsC in
        return rhs.withC {
            return iop_equals_desc(type(of: lhs).descriptor, lhsC, $0)
        }
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
                     "type mismatch for object: use IopClass.make(_:)")
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
/* Interface {{{ */

/// Error that can be thrown by RPC.
public enum IopRPCError<RPC: IopRPC> : Error {
    /// Exception thrown.
    ///
    /// This error represent the case an applicative exception has been
    /// thrown by the RPC. Type type of the exception is the type of
    /// `RPC.Exception` destriptor.
    case exception(RPC.Exception)

    /// RPC call could be retried
    case retry

    /// RPC called has been aborted.
    ///
    /// Abort can occur for various reason, such as following a disconnection,
    /// because there is no slot available for a new query, ...
    case abort

    /// RPC request is invalid IOP
    ///
    /// The server hasn't been able to unpack the IOP request. This might
    /// be because the request didn't respect some IOP constraints, or the
    /// consequence of a backward incompatible change.
    ///
    /// - Parameter msg: the message detailling the error
    case invalid(msg: String?)

    /// RPC is not implemented by the server
    case unimplemented

    /// RPC has been rejected by the server for some internal reason
    case serverError

    /// RPC has been aborted by a proxy
    case proxyError

    /// RPC has timed out
    case timedOut

    /// RPC has been canceled
    case canceled

    /// Import an error from the callback of a RPC
    public init?(status: ic_status_t, exn: RPC.Exception?) {
        switch status {
          case .IC_MSG_OK:
            return nil

          case .IC_MSG_EXN:
            guard let exn = exn else {
                return nil
            }
            self = .exception(exn)

          case .IC_MSG_RETRY:
            self = .retry

          case .IC_MSG_ABORT:
            self = .abort

          case .IC_MSG_INVALID:
            let msg = iop_get_err_lstr()
            self = .invalid(msg: String(msg))

          case .IC_MSG_UNIMPLEMENTED:
            self = .unimplemented

          case .IC_MSG_SERVER_ERROR:
            self = .serverError

          case .IC_MSG_PROXY_ERROR:
            self = .proxyError

          case .IC_MSG_TIMEDOUT:
            self = .timedOut

          case .IC_MSG_CANCELED:
            self = .canceled
        }
    }

    /// C status code for the error.
    public var status : ic_status_t {
        switch self {
          case .exception(_):
            return .IC_MSG_EXN

          case .retry:
            return .IC_MSG_RETRY

          case .abort:
            return .IC_MSG_ABORT

          case .invalid(_):
            return .IC_MSG_INVALID

          case .unimplemented:
            return .IC_MSG_UNIMPLEMENTED

          case .serverError:
            return .IC_MSG_SERVER_ERROR

          case .proxyError:
            return .IC_MSG_PROXY_ERROR

          case .timedOut:
            return .IC_MSG_TIMEDOUT

          case .canceled:
            return .IC_MSG_TIMEDOUT
        }
    }
}

extension IopRPCError : CustomStringConvertible,
                        CustomDebugStringConvertible {
    public var description : String {
        var str = String(cString: ic_status_to_string(self.status))

        switch self {
          case .exception(let exn):
            print(": \(exn)", terminator: "", to: &str)

          case .invalid(let msg?):
            print(": \(msg)", terminator: "", to: &str)

          default:
            break
        }
        return str
    }

    public var debugDescription : String {
        var str = String(cString: ic_status_to_string(self.status))

        switch self {
          case .exception(let exn):
            debugPrint(": \(exn)", terminator: "", to: &str)

          case .invalid(let msg?):
            print(": \(msg)", terminator: "", to: &str)

          default:
            break
        }
        return str
    }
}

/// Type of a RPC.
///
/// A RPC is represented by a quadruplet describing the input, output and
/// exception types as defined in the IOP description, but also a `ReturnType`
/// derived from whether the RPC is asynchronous (`out null`) or not.
public protocol IopRPC {
    /// Type of the argument of the RPC.
    associatedtype Argument : IopStruct = IopVoid

    /// Type of the response of the RPC.
    associatedtype Response : IopStruct = IopVoid

    /// Type of the exceptions thrown by the RPC.
    associatedtype Exception : IopStruct = IopVoid

    /// Return type of the RPC.
    ///
    /// Depending on whether the RPC is asynchronous (`out null`) or not, the
    /// return type will be `Swift.Void` or a tuple or `Response` and an `Error`.
    associatedtype ReturnType = Promise<Response>

    /// Descriptor of the RPC C object.
    static var descriptor : UnsafePointer<iop_rpc_t> { get }

    /// Tag of the RPC in its interface.
    static var tag : Int { get }
}

/// Type of an object that can perform IOP queries.
///
/// An IOP query is defined as the emission of a message and some arguments.
public protocol IopChannel {
    /// Send an IOP query.
    ///
    /// The query function receives a pre-built message whose content has
    /// already been filled with the description of the RPC and well as the
    /// potential callback.
    ///
    /// The query function is then responsible of the potential packing of the
    /// arguments of the query, and then making sure the query is transmitted
    /// to its receiver.
    ///
    /// `query(msg:args:)` must ensures the message will eventually get a
    /// reply (which can be either a response or an error), and that it is
    /// replied to only once.
    func query<T: IopStruct>(msg: UnsafeMutablePointer<ic_msg_t>, args: T)
}

/// Type of IOP interface implementation.
///
/// The IOP interface implementation is an object that allow querying RPC
/// through a channel. This object contains the implementation of the RPC
/// that are wrapper calling the `_query(rpc:args:)` method.
public protocol IopInterfaceImpl {
    /// Tag of the interface in the module.
    var _tag : Int { get }

    /// Channel to which queries are forwarded.
    var _channel : IopChannel { get }

    /// Create a new interface instance.
    ///
    /// Create a new interface instance with tagged `tag` that forwards its
    /// queries to `channel`
    init(channel: IopChannel, tag: Int)
}

public extension IopInterfaceImpl {
    /// Implementation of query for asynchronous RPC `rpc`.
    ///
    /// This method builds the message for the asynchronous RPC and
    /// forwards the query to the channel.
    public func _query<RPC: IopRPC>(rpc: RPC.Type, args: RPC.Argument) -> RPC.ReturnType
        where RPC.ReturnType == Void
    {
        let msg = ic_msg_new(0)

        msg.pointee.cb = ic_drop_ans_cb
        msg.pointee.rpc = RPC.descriptor
        msg.pointee.async = true
        msg.pointee.cmd = Int32((self._tag << 16) + RPC.tag)
        msg.pointee.trace = false
        self._channel.query(msg: msg, args: args)
    }

    /// Implementation of query for non-asynchronous RPC `rpc`.
    ///
    /// This method builds the message for the RPC, including the
    /// construction of the callback that will parse the output
    /// and sends that result to the promise.
    ///
    /// - Returns: a promise that receives the result of the RPC.
    public func _query<RPC: IopRPC>(rpc: RPC.Type, args: RPC.Argument) -> RPC.ReturnType
        where RPC.ReturnType == Promise<RPC.Response>
    {
        return Promise<RPC.Response>() {
            (onSuccess, onError) in
            let msg = ic_msg_new_blk {
                (ic, status, res, exn) in

                do {
                    switch status {
                      case .IC_MSG_OK:
                        guard let response = try RPC.Response.make(optional: res) else {
                            throw IopRPCError<RPC>.invalid(msg: "cannot convert response to Swift")
                        }
                        onSuccess(response)

                      default:
                        guard let err = IopRPCError<RPC>(status: status, exn: try RPC.Exception.make(optional: exn)) else {
                            throw IopRPCError<RPC>.invalid(msg: "cannot convert exception to Swift")
                        }
                        onError(err)
                    }
                } catch {
                    onError(error)
                }
            }

            msg.pointee.rpc = RPC.descriptor
            msg.pointee.async = false
            msg.pointee.cmd = Int32((self._tag << 16) + RPC.tag)
            msg.pointee.trace = false
            self._channel.query(msg: msg, args: args)
        }
    }
}

/// Type of an interface.
///
/// An interface is split in two parts:
/// - the registration part, managed by the `IopInterface` type.
/// - the query part, management by the `IopInterface.Impl` type.
///
/// An interface contains the RPC descriptors in the form of subtypes
/// inheriting from `IopRPC` as well as properties that build the
/// registration descriptors of the RPCs.
///
/// For example, the following interface:
///
///     interface Foo {
///         bar
///             in In
///             out Out;
///     }
///
/// Will receives the following Swift interface description:
///
///     struct Foo : IopInterface {
///         // RPC descriptor
///         enum Bar : IopRPC {
///              typealias Argument = In
///              typealias Response = Out
///         }
///
///         // RPC registration member
///         var bar : (Int, Bar.Type)
///
///         // Interface implementation
///         struct Impl : IopInterfaceImpl {
///             func bar(_ args: Bar.Argument) -> Bar.ReturnType
///         }
///     }
public protocol IopInterface {
    /// Type holding the implementation of the RPCs.
    associatedtype Impl : IopInterfaceImpl

    /// Tag of the interface in its module.
    var _tag : Int { get }

    /// Create a new interface.
    init(tag: Int)
}

/// Type of IOP Modules.
///
/// A module is the root type for IOP APIs. It contains a set of
/// instantiated interfaces, themselves containing RPCs.
///
/// The Swift IOP module provides two main features related to the
/// use of IOP RPCs:
/// - the querying part, that requires instantiating a module object.
/// - the registration part, that is performed through static properties
///  in the module type.
///
/// Supposing we do have the following module:
///
///     module Bar {
///         Foo foo
///     }
///
/// The corresponding swift type will look like
///
///     struct Bar : IopModule {
///         // registration descriptor
///         static var foo : Foo { return .init(tag: 1) }
///
///         // query implementation
///         var foo : Foo.Impl { return .init(channel: self.channel, tag: 1) }
///     }
///
/// This allows the use of the IOP-RPC mechanism as follow:
///  - register the RPC in a registry
///
///     registry.implements(rpc: Bar.foo.bar) {
///         (arg, hdr) in
///
///         return Out()
///     }
///
///  - the query part
///
///     let channel = IChannel<Bar>
///     channel.query.foo.bar(In()).then {
///         print("success")
///     }
///
public protocol IopModule {
    /// Channel to which queryies are forwarded
    var channel : IopChannel { get }

    /// Create a new instance of the module to query through `channel`
    init(channel: IopChannel)
}

/// Type of a RPC implementation registry.
///
/// A RPC implementation registry allows mapping a RPC to an implementation
/// closure. In order to conform to that type, you only have to implement
/// `register(cmd:entry:)` method.
///
/// - SeeAlso: IopModule
public protocol IopRpcRegistry {
    /// Register the implementation of RPC with command `cmd`
    func register(cmd: Int, entry: ic_cb_entry_t)
}

public extension IopRpcRegistry {
    public typealias IopRPCImpl = (UnsafeMutablePointer<ichannel_t>, UInt64, UnsafeMutableRawPointer?, UnsafePointer<ic__hdr__t>?) -> ()

    /// Provides a raw implementation to `rpc`.
    ///
    /// Provides a low-level implementation to the register RPC.
    /// This function is a helper that build a `ic_cb_entry_t` from
    /// an implementation callabck.
    func implements<RPC: IopRPC>(rpc: (Int, RPC.Type), _ body: @escaping IopRPCImpl) {
        self.register(cmd: rpc.0,
                    entry: ic_cb_entry_t(cb_type: .IC_CB_NORMAL_BLK,
                                             rpc: RPC.descriptor,
                                        pre_hook: nil,
                                       post_hook: nil,
                                   pre_hook_args: data_t(),
                                  post_hook_args: data_t(),
                                               u: .init(blk: .init(cb: body))))
    }

    /// Register a synchronous implementation for a RPC.
    ///
    /// Register the implementation for a RPC when the implementation
    /// can build a response synchronously. The provided callback
    /// should return the response to the RPC. In case of error, it
    /// should throw an error (a IopRPCError.exception in order to
    /// throw an IOP exception
    public func implements<RPC: IopRPC>(rpc: (Int, RPC.Type), promising: @escaping (RPC.Argument, ic.Hdr?) -> Promise<RPC.Response>)
        where RPC.ReturnType == Promise<RPC.Response> {
        return self.implements(rpc: rpc) {
            (channel: UnsafeMutablePointer<ichannel_t>,
                slot: UInt64,
                args: UnsafeMutableRawPointer?,
                hdr: UnsafePointer<ic__hdr__t>?) in

            do {
                let args = try RPC.Argument(args)!
                let hdr = try ic.Hdr(hdr)

                let res = promising(args, hdr)

                res.then {
                    _ = $0.withC {
                        __ic_reply(nil, slot, Int32(ic_status_t.IC_MSG_OK.rawValue), -1, RPC.Response.descriptor, $0)
                    }
                }
                res.otherwise {
                    switch $0 {
                      case is IopImportError:
                        ic_reply_err(nil, slot, Int32(ic_status_t.IC_MSG_INVALID.rawValue))

                      case IopRPCError<RPC>.exception(let exn):
                        _ = exn.withC {
                            __ic_reply(nil, slot, Int32(ic_status_t.IC_MSG_EXN.rawValue), -1, RPC.Exception.descriptor, $0)
                        }

                      case let error as IopRPCError<RPC>:
                        ic_reply_err(nil, slot, Int32(error.status.rawValue))

                      default:
                        ic_reply_err(nil, slot, Int32(ic_status_t.IC_MSG_SERVER_ERROR.rawValue))
                    }
                }
            } catch {
                ic_reply_err(channel, slot, Int32(ic_status_t.IC_MSG_INVALID.rawValue))
            }
        }
    }


    /// Register a asynchronous implementation for a RPC.
    ///
    /// Register the implementation for a RPC whose response won't be
    /// known in the callback. The implementation must return a promise
    /// whose success case will contain the response to the RPC, while the
    /// error case will contain the exception or any other IopRPCError.
    public func implements<RPC: IopRPC>(rpc: (Int, RPC.Type), cb: @escaping (RPC.Argument, ic.Hdr?) throws -> RPC.Response)
        where RPC.ReturnType == Promise<RPC.Response> {
        return self.implements(rpc: rpc) {
            (args, hdr) in

            return Promise<RPC.Response>() {
                (onSuccess, onError) in

                do {
                    onSuccess(try cb(args, hdr))
                } catch {
                    onError(error)
                }
            }
        }
    }

    /// Register the implementation for an asynchronous RPC.
    ///
    /// Register the implementation for a RPC that does not expect a reply.
    public func implements<RPC: IopRPC>(rpc: (Int, RPC.Type), fn: @escaping (RPC.Argument, ic.Hdr?) -> Void)
        where RPC.ReturnType == Void {
        return self.implements(rpc: rpc) {
            (channel: UnsafeMutablePointer<ichannel_t>,
                slot: UInt64,
                args: UnsafeMutableRawPointer?,
                hdr: UnsafePointer<ic__hdr__t>?) in

            do {
                let args = try RPC.Argument(args)!
                let hdr = try ic.Hdr(hdr)

                fn(args, hdr)
            } catch {
                /* Ignore all exceptions here */
            }
        }
    }
}

/* }}} */
/* Channel {{{ */

/// Errors linked to channel itself.
///
/// This defines transport layer errors that might occur while
/// setting up or using a channel.
public enum ChannelError : Error {
    /// The provided address is invalid.
    case invalid(address: String)

    /// Connection to the server failed.
    case connectionError

    /// Cannot listen on provided address
    case listenError
}

/// Parse `address` into a `sockunion_t`
///
/// This will raise an `ChannelError.invalidAddr` in case of error.
private func resolve(address: String) throws -> sockunion_t {
    return try address.withCString {
        var host = ps_init(nil, 0)
        var port : in_port_t = 0
        var su = sockunion_t()

        if addr_parse(ps_initstr($0), &host, &port, -1) < 0 {
            throw ChannelError.invalid(address: address)
        }
        if addr_info(&su, sa_family_t(AF_UNSPEC), host, port) < 0 {
            throw ChannelError.invalid(address: address)
        }

        return su
    }
}

private func onChannelEvent(ic: UnsafeMutablePointer<ichannel_t>, event: ic_event_t)
{
    guard let data = ic.pointee.priv else {
        return
    }

    let obj: Unmanaged<IChannelBase> = Unmanaged.fromOpaque(data)
    let channel = obj.takeUnretainedValue()

    channel.on(event: event)
}

private func onChannelWipe(ic: UnsafeMutablePointer<ichannel_t>) {
    guard let data = ic.pointee.priv else {
        return
    }

    let obj : Unmanaged<IChannelBase> = Unmanaged.fromOpaque(data)
    ic.pointee.priv = nil
    obj.release()
}

/// Base class for C's `ichannel_t` wrapping.
///
/// This class manages the wrapping of the object in Swift.
public class IChannelBase {
    var ic : UnsafeMutablePointer<ichannel_t>?
    let own : Bool

    init() {
        self.ic = ic_new()
        self.own = true
        self.ic?.pointee.owner = UnsafeMutablePointer(&self.ic)
        self.ic?.pointee.priv = Unmanaged.passUnretained(self).toOpaque()
        self.ic?.pointee.on_event = onChannelEvent
    }

    init(_ ic: UnsafeMutablePointer<ichannel_t>) {
        self.ic = ic
        self.own = false
        self.ic?.pointee.owner = UnsafeMutablePointer(&self.ic)
        self.ic?.pointee.priv = Unmanaged.passRetained(self).toOpaque()
        self.ic?.pointee.on_wipe = onChannelWipe
        self.ic?.pointee.on_event = onChannelEvent
    }

    deinit {
        if self.own {
            ic_delete(&self.ic)
        }
    }

    func on(event: ic_event_t) {
    }

    /// Set the implementation registry.
    ///
    /// The channel will be able to response to any RPC present
    /// in the provided registry. Other RPC will receive an
    /// `IopRPCError.unimplemented` error.
    public func set(impl: IcImplRegistry) {
        self.ic?.pointee.impl = UnsafePointer(impl.impl)
    }

    /// Negotiate termination of the channel.
    ///
    /// Notifies the remote end of the channel that we will not send any more
    /// queries and that it can close the channel.
    public func closeGently() {
        guard let ic = self.ic else {
            return
        }
        ic_bye(ic)
    }

    /// Force the termination of the channel.
    ///
    /// Force the disconnection of the channel.
    public func closeImmediately() {
        guard let ic = self.ic else {
            return
        }
        ic_disconnect(ic)
    }
}

extension IChannelBase : IopChannel {
    public func query<T: IopStruct>(msg: UnsafeMutablePointer<ic_msg_t>, args: T) {
        guard let ic = self.ic else {
            __ic_msg_reply_err(nil, msg, .IC_MSG_ABORT)

            var msg : UnsafeMutablePointer<ic_msg_t>? = msg
            ic_msg_delete(&msg)
            return
        }

        args.withC {
            __ic_msg_build(msg, T.descriptor, $0,
                self.ic?.pointee.is_local == false || msg.pointee.force_pack)
        }
        __ic_query(ic, msg)
    }
}

/// Unicast IOP Channel.
///
/// The IChannel class allows querying a single destination
/// to which the channel is connected. It is a wrapper around
/// the C's `ichannel_t` type.
public final class IChannel<Module: IopModule> : IChannelBase {
    public typealias OnEvent = (IChannel<Module>, ic_event_t) -> Void
    let onEvent : OnEvent

    init(onEvent: @escaping OnEvent) {
        self.onEvent = onEvent
        super.init()
    }

    init(_ ic: UnsafeMutablePointer<ichannel_t>, onEvent: @escaping OnEvent) {
        self.onEvent = onEvent
        super.init(ic)
    }

    override func on(event: ic_event_t) {
        self.onEvent(self, event)
    }

    /// Module allowing queries.
    public var query : Module {
        return Module(channel: self)
    }

    /// Create an IChannel connected to `address`.
    ///
    /// Create a new client IChannel that connects `address`.
    public static func connect(to address: String, onEvent: @escaping OnEvent) throws -> IChannel<Module> {
        let channel = IChannel<Module>(onEvent: onEvent)
        let ic = channel.ic!

        ic.pointee.su = try resolve(address: address)

        if ic_connect(ic) < 0 {
            throw ChannelError.connectionError
        }

        return channel
    }

    /// Create an IChannel server listening on `address`.
    ///
    /// Create a new server that waits for incoming IChannel connections by
    /// listening on `address`. Whenever a new incoming connection is accepted,
    /// the provided `onAccept` cluster is called with the incoming connection
    /// as argument.
    ///
    /// - parameter address: the network address on which the server should listen.
    /// - parameter onAccept: the closure called when having a new incoming
    ///  connection.
    ///
    /// - returns: the event loop handler of the server. The server remains active
    ///  as long as the handler is referenced in your code.
    public static func listen(to address: String, onEvent: @escaping OnEvent) throws -> El.Fd {
        var su = try resolve(address: address)

        let sock = listenx(-1, &su, 1, SOCK_STREAM, Int32(IPPROTO_TCP), O_NONBLOCK)
        if sock < 0 {
            throw ChannelError.listenError
        }

        let el = El.register(fd: sock, events: Int16(POLLIN)) {
            (el, fd, events) in

            while true {
                let sock = acceptx(fd, O_NONBLOCK)

                if sock < 0 {
                    return 0
                }

                let channel = IChannel<Module>(ic_new(), onEvent: onEvent)

                ic_spawn(channel.ic!, sock, nil)
            }
        }
        return el.unref()
    }

    /// Create a local channel
    public static func makeLocal() -> IChannel<Module> {
        let channel = IChannel<Module>() { _, _ in }

        ic_set_local(channel.ic!)
        return channel
    }
}

extension IChannel : Hashable {
    public var hashValue : Int {
        return self.ic?.hashValue ?? 0
    }

    public static func ==(lhs: IChannel, rhs: IChannel) -> Bool {
        return lhs.ic == rhs.ic
    }
}

extension qm_ic_cbs_t : QMapSimple {
    public typealias Value = ic_cb_entry_t
}

/// Implementation registry based on a `qm_t(ic_cbs)`.
///
/// This is the base implementation type that is usable with
/// `IChannel` objects.
public struct IcImplRegistry : IopRpcRegistry {
    public var impl : UnsafeMutablePointer<qm_ic_cbs_t>

    public init(impl: UnsafeMutablePointer<qm_ic_cbs_t>) {
        self.impl = impl
    }

    public func register(cmd: Int, entry: ic_cb_entry_t) {
        self.impl.pointee[UInt32(cmd)] = entry
    }
}

/* }}} */

/// Type for IOP package.
///
/// An IOP package is a namespace that contains a set of types, interfaces and modules.
/// A package must be registered before its classes can be instantiated from C objects.
public protocol IopPackage {
    associatedtype interfaces
    associatedtype modules

    static var classes: [IopClass.Type] { get }
}
