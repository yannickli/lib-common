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
        self.name.withLString { modName in
            name.withLString { depName in
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
