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

/* Modules {{{ */

/// A loadable code module.
///
/// A module represent a section of code that can be dynamic loaded with
/// tracking of dependences. The Swift modules are provided as a way to
/// load swift code in a C program and can be required by C modules.
///
/// Swift modules must be defined in both C and Swift. The C part contains
/// a standard declaration:
///
///     MODULE_DECLARE(my_swift_module)
///
/// As well as a specific definition that indicates the module is actually
/// implemented in Swift.
///
///     MODULE_SWIFT(swift_lib, 9, my_swift_module, 15)
///
/// Then, in swift, you must instantiate in `swift_lib` a variable of type
/// `Module` with the same name as your module:
///
///     private func initialize(_ ptr: UnsafeMutableRawPointer?) -> Int32 {
///         return 0
///     }
///
///     private func shutdown() -> Int32 {
///         return 0
///     }
///
///     let my_swift_module = Module(name: "my_swift_module",
///                                module: my_swift_module_module_ptr,
///                            initialize: initialize,
///                              shutdown: shutdown)
///
/// In case you want to add some dependencies or module method implementation,
/// you should use a closure to initialize your variable:
///
///     let my_swift_module = ({ () in
///         let module = Module(name: "my_swift_module",
///                           module: my_swift_module_module_ptr,
///                       initialize: initialize,
///                         shutdown: shutdown)
///         module.depends(on: log_moduleptr, named: "log")
///         return module
///     })()
public class Module {
    /// Type of callback called when loading a module
    public typealias Initializer = @convention(c) (_ arg: UnsafeMutableRawPointer?) -> Int32

    /// Type of callback called when shutting down a module
    public typealias Shutdown = @convention(c) () -> Int32

    /// C-module descriptor
    public typealias CModule = UnsafeMutablePointer<module_ptr_t?>

    /// C-method descriptor
    public typealias CMethod = UnsafePointer<module_method_t>

    /// Reference to the corresponding C module.
    private let module : UnsafeMutablePointer<module_ptr_t?>

    /// Name of the module.
    private let name : String

    /// Creates a module Swift descriptor.
    ///
    /// - Parameter name: the name of the module. The name must be the same
    ///  as the one given to the C object. So if the declaration is done
    ///  with `MODULE_DECLARE(my_module)`, name must be `"my_module"`.
    ///
    /// - Parameter module: the C-Module descriptor. For a module declared
    ///  with `MODULE_DECLARE(my_module)`, the descriptor is `my_module_module_ptr`.
    ///
    /// - Parameter initialize: the function called when the module is loaded.
    ///
    /// - Parameter shutdown: the function called when the module is unloaded.
    public init(name: String, module: CModule, initialize: @escaping Initializer, shutdown: @escaping Shutdown) {
        name.withLString { cName in 
            module[0] = module_register(cName, module, initialize, shutdown, nil, 0)
        }
        self.name = name
        self.module = module
    }

    /// Add a dependency from the current module on `module`.
    ///
    /// This will ensure that `module` will be loaded before the current module
    /// is loaded itself.
    ///
    ///  - Parameter module: the descriptor of the other module.
    ///
    ///  - Parameter name: the name of the other module.
    public func depends(on module: CModule, named name: String) {
        self.name.withLString { modName in
            name.withLString { depName in
                module_add_dep(self.module[0]!, modName, depName, module)
            }
        }
    }

    /// Add a dependency from `module` on the current module
    ///
    /// This will ensure that the current module get loaded before `module`
    /// is loaded.
    ///
    ///  - Parameter module: the descriptor of the other module.
    ///
    ///  - Parameter name: the name of the other module.
    public func needed(by module: CModule, named name: String) {
        self.name.withLString { modName in name.withLString { depName in
                module_add_dep(module[0]!, depName, modName, self.module)
            }
        }
    }

    /// Specifies that the current module implements the method `method`.
    ///
    /// When the module is loaded and the module method `method` is called, the
    /// provided function `body` will be called. The execution order of the various
    /// implementations of the same method depends on the dependencies between
    /// the modules that provide the implementations.
    ///
    ///  - Parameter method: the descriptor of the implemented method.
    ///
    ///  - Parameter body: the function called when the method is invoked.
    public func implements(method: CMethod, with body: @escaping @convention(c) (Int32) -> ()) {
        module_implement_method_int(self.module[0]!, method, body)
    }

    /// Specifies that the current module implements the method `method`.
    ///
    /// When the module is loaded and the module method `method` is called, the
    /// provided function `body` will be called. The execution order of the various
    /// implementations of the same method depends on the dependencies between
    /// the modules that provide the implementations.
    ///
    ///  - Parameter method: the descriptor of the implemented method.
    ///
    ///  - Parameter body: the function called when the method is invoked.
    public func implements(method: CMethod, with body: @escaping @convention(c) () -> ()) {
        module_implement_method_void(self.module[0]!, method, body)
    }
}

/* }}} */
/* Allocator {{{ */

/// Type of an object that contains the adequate context to perform an allocation.
///
/// An allocator is a wrapper around a `MemoryPool` that provides guarantees
/// that the pool is usable before using it.
public protocol Allocator {
    var pool : UnsafeMutablePointer<MemoryPool> { get }
}

public enum StandardAllocator : Allocator {
    case malloc
    case alignedMalloc
    case global

    public var pool : UnsafeMutablePointer<MemoryPool> {
        switch self {
          case .malloc:
            return UnsafeMutablePointer(&MemoryPool.libC)

          case .alignedMalloc:
            return UnsafeMutablePointer(&MemoryPool.libCAligned)

          case .global:
            return UnsafeMutablePointer(&MemoryPool.static)
        }
    }
}

public protocol FrameBasedAllocator : Allocator {
}

/* }}} */
/* tScope {{{ */


public extension UnsafeMutableRawPointer {
    /// Allocates on `allocator` and points at uninitialized memory for
    /// size `bytes` with `alignedTo` alignment.
    ///
    /// The lifetime of the allocated memory depends on `allocator` used.
    public static func allocate(bytes: Int, alignedTo: Int, on allocator: Allocator) -> UnsafeMutableRawPointer {
        let pool = allocator.pool

        return pool.pointee.malloc(pool, bytes, alignedTo, 1 << 8)
    }
}

public extension UnsafeMutableRawBufferPointer {
    /// Allocate memory for size `bytes` with word alignment on `allocator`.
    ///
    /// The lifetime of the allocated memory depends on `allocator` used.
    public static func allocate(count: Int, on allocator: Allocator) -> UnsafeMutableRawBufferPointer {
        let mem = UnsafeMutableRawPointer.allocate(bytes: count, alignedTo: 8, on: allocator)
        return UnsafeMutableRawBufferPointer(start: mem, count: count)
    }
}

public extension UnsafeMutablePointer {
    /// Allocates on `allocator` and points at uninitialized aligned memory
    /// for `count` instances of `Pointee`.
    ///
    /// The lifetime of the allocated memory depends on `allocator` used.
    public static func allocate(capacity: Int, on allocator: Allocator) -> UnsafeMutablePointer<Pointee> {
        let bytes = capacity * MemoryLayout<Pointee>.stride
        let alignment = MemoryLayout<Pointee>.alignment
        let raw = UnsafeMutableRawPointer.allocate(bytes: bytes, alignedTo: alignment, on: allocator)

        return raw.bindMemory(to: Pointee.self, capacity: capacity)
    }
}

public extension Array {
    /// Duplicates the content of the arrays on `allocator`.
    ///
    /// The lifetime of the allocated memory depends on `allocator` used.
    public func duplicated(on allocator: Allocator) -> UnsafeMutablePointer<Element> {
        let mem = UnsafeMutablePointer<Element>.allocate(capacity: self.count, on: allocator)

        self.withUnsafeBufferPointer {
            mem.assign(from: $0.baseAddress!, count: self.count)
        }
        return mem
    }
}

private struct TStackFrame : FrameBasedAllocator {
    let frame : UnsafePointer<mem_stack_frame_t>

    public var pool : UnsafeMutablePointer<MemoryPool> {
        precondition(frame == UnsafePointer(MemoryPool.tPool.pointee.stack),
                     "trying to allocate in the wrong stack frame")
        return MemoryPool.tStack
    }
}

/// Create an allocation scope on the t_stack.
///
/// The t_stack is a special frame-based stacked allocator. When a new frame is
/// pushed on the allocator, all new allocations occurs in that frame. All the
/// memory allocated on the t_stack is automatically freed when popping the frame.
///
/// A `tScope` wraps the creation and pushing of a new frame when entering
/// the scope, the execution of the provided `body` closure and popping of the
/// frame when exiting the scope. As a consequence, `body` can allocate memory
/// on the t_stack that will be freed when it exits. `body` receives an allocator
/// that it can use inside that `tScope`.
///
/// - Parameter body: the closure executed within the t_stack frame defined by
///  the `tScope`
///
/// - Returns: the return value of `body`, if any.
///
/// - warning: `body` must not return t_stack allocated memory, since that memory
///  will be freed at its exit.
public func tScope<Result>(_ body: (FrameBasedAllocator) throws -> Result) rethrows -> Result {
    let pool = MemoryPool.tPool
    let pushToken = mem_stack_push(pool)
    defer {
        let popToken = mem_stack_pop(pool)
        assert (popToken == pushToken, "unbalanced t_stack")
    }
    return try body(TStackFrame(frame: pushToken.bindMemory(to: mem_stack_frame_t.self, capacity: 1)))
}

/* }}} */
/* {{{ Memory management */

/// A type that has a `wipe()` method for deinitialization.
public protocol Wipeable {
    /// Deinit the object by wiping out all the associated ressources.
    mutating func wipe()
}

/// Wrapper class around a `Wipeable` object.
///
/// That class wraps a wipeable object, ensuring that object will be
/// be wiped when the class is deinitialized.
public class AutoWipe<Wrapped: Wipeable> {
    public var wrapped: Wrapped

    public init(_ obj: Wrapped) {
        self.wrapped = obj
    }

    deinit {
        self.wrapped.wipe()
    }
}

extension AutoWipe : CustomStringConvertible {
    public var description : String {
        var res = ""

        print(self.wrapped, terminator: "", to: &res)
        return res
    }
}

extension AutoWipe : CustomDebugStringConvertible {
    public var debugDescription : String {
        var res = ""

        debugPrint(self.wrapped, terminator: "", to: &res)
        return res
    }
}

/* }}} */
