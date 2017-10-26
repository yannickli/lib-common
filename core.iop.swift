/***** THIS FILE IS AUTOGENERATED DO NOT MODIFY DIRECTLY ! *****/
extension core__logger_configuration__array_t : libcommon.IopArray { }
extension core__licence_module__array_t : libcommon.IopArray { }
extension core__log_level__t : libcommon.IopEnum {
    public static let descriptor = core__log_level__ep
    public static let min : Swift.Int32 = Swift.Int32(LOG_LEVEL_min)
    public static let max : Swift.Int32 = Swift.Int32(LOG_LEVEL_max)
    public static let count : Swift.Int32 = Swift.Int32(LOG_LEVEL_count)
}
extension core__iop_http_method__t : libcommon.IopEnum {
    public static let descriptor = core__iop_http_method__ep
    public static let min : Swift.Int32 = Swift.Int32(IOP_HTTP_METHOD_min)
    public static let max : Swift.Int32 = Swift.Int32(IOP_HTTP_METHOD_max)
    public static let count : Swift.Int32 = Swift.Int32(IOP_HTTP_METHOD_count)
}
public protocol core__modules__Core : libcommon.IopModule {
    var `log` : core_package.interfaces.Log.Impl { get }
    static var `log` : core_package.interfaces.Log { get }
}
public extension core__modules__Core {
    public var `log` : core_package.interfaces.Log.Impl {
        return core_package.interfaces.Log.Impl(channel: self.channel, tag: 16384)
    }
    public static var `log` : core_package.interfaces.Log {
        return core_package.interfaces.Log(tag: 16384)
    }
}

public enum core : libcommon.IopPackage {
    public typealias LogLevel = core__log_level__t

    public typealias IopHttpMethod = core__iop_http_method__t

    public final class LoggerConfiguration : libcommon.IopStruct {
        open override class var descriptor : Swift.UnsafePointer<iop_struct_t> {
            return core__logger_configuration__sp
        }

        public var `fullName` : Swift.String
        public var `level` : core_package.LogLevel
        public var `forceAll` : Swift.Bool
        public var `isSilent` : Swift.Bool

        public init(`fullName`: Swift.String,
                    `level`: core_package.LogLevel,
                    `forceAll`: Swift.Bool = false,
                    `isSilent`: Swift.Bool = false) {
            self.fullName = `fullName`
            self.level = `level`
            self.forceAll = `forceAll`
            self.isSilent = `isSilent`
            super.init()
        }

        public required init(_ c: Swift.UnsafeRawPointer) throws {
            let data = c.bindMemory(to: core__logger_configuration__t.self, capacity: 1)
            self.fullName = Swift.String(data.pointee.full_name) ?? ""
            self.level = data.pointee.level
            self.forceAll = data.pointee.force_all
            self.isSilent = data.pointee.is_silent
            try super.init(c)
        }

        open override func fill(_ c: Swift.UnsafeMutableRawPointer, on allocator: libcommon.FrameBasedAllocator) {
            let data = c.bindMemory(to: core__logger_configuration__t.self, capacity: 1)
             data.pointee.full_name = self.fullName.duplicated(on: allocator)
            data.pointee.level = self.level
            data.pointee.force_all = self.forceAll
            data.pointee.is_silent = self.isSilent
        }
    }

    public final class LogConfiguration : libcommon.IopStruct {
        open override class var descriptor : Swift.UnsafePointer<iop_struct_t> {
            return core__log_configuration__sp
        }

        public var `rootLevel` : core_package.LogLevel
        public var `forceAll` : Swift.Bool
        public var `isSilent` : Swift.Bool
        public var `specific` : [core_package.LoggerConfiguration]

        public init(`rootLevel`: core_package.LogLevel = core_package.LogLevel(rawValue: -2),
                    `forceAll`: Swift.Bool = false,
                    `isSilent`: Swift.Bool = false,
                    `specific`: [core_package.LoggerConfiguration] = []) {
            self.rootLevel = `rootLevel`
            self.forceAll = `forceAll`
            self.isSilent = `isSilent`
            self.specific = `specific`
            super.init()
        }

        public required init(_ c: Swift.UnsafeRawPointer) throws {
            let data = c.bindMemory(to: core__log_configuration__t.self, capacity: 1)
            self.rootLevel = data.pointee.root_level
            self.forceAll = data.pointee.force_all
            self.isSilent = data.pointee.is_silent
            self.specific = try data.pointee.specific.buffer.map {
                var specific_var = $0
                return try core_package.LoggerConfiguration(&specific_var)
                }
            try super.init(c)
        }

        open override func fill(_ c: Swift.UnsafeMutableRawPointer, on allocator: libcommon.FrameBasedAllocator) {
            let data = c.bindMemory(to: core__log_configuration__t.self, capacity: 1)
            data.pointee.root_level = self.rootLevel
            data.pointee.force_all = self.forceAll
            data.pointee.is_silent = self.isSilent
            libcommon.duplicate(complexTypeArray: self.specific, to: &data.pointee.specific, on: allocator)
        }
    }

    open class LogFileConfiguration : libcommon.IopClass {
        open override class var descriptor : Swift.UnsafePointer<iop_struct_t> {
            return core__log_file_configuration__sp
        }

        open override class var isAbstract : Swift.Bool {
            return false
        }

        public var `maxSize` : Swift.Int32
        public var `maxTime` : Swift.UInt64
        public var `maxFiles` : Swift.Int32
        public var `totalMaxSize` : Swift.Int64
        public var `compress` : Swift.Bool

        public init(`maxSize`: Swift.Int32 = 536870912,
                    `maxTime`: Swift.UInt64 = 86400,
                    `maxFiles`: Swift.Int32 = 0,
                    `totalMaxSize`: Swift.Int64 = 1073741824,
                    `compress`: Swift.Bool = true) {
            self.maxSize = `maxSize`
            self.maxTime = `maxTime`
            self.maxFiles = `maxFiles`
            self.totalMaxSize = `totalMaxSize`
            self.compress = `compress`
            super.init()
        }

        public required init(_ c: Swift.UnsafeRawPointer) throws {
            let data = c.bindMemory(to: core__log_file_configuration__t.self, capacity: 1)
            self.maxSize = data.pointee.max_size
            self.maxTime = data.pointee.max_time
            self.maxFiles = data.pointee.max_files
            self.totalMaxSize = data.pointee.total_max_size
            self.compress = data.pointee.compress
            try super.init(c)
        }

        open override func fill(_ c: Swift.UnsafeMutableRawPointer, on allocator: libcommon.FrameBasedAllocator) {
            let data = c.bindMemory(to: core__log_file_configuration__t.self, capacity: 1)
            data.pointee.max_size = self.maxSize
            data.pointee.max_time = self.maxTime
            data.pointee.max_files = self.maxFiles
            data.pointee.total_max_size = self.totalMaxSize
            data.pointee.compress = self.compress
        }
    }

    open class LicenceModule : libcommon.IopClass {
        open override class var descriptor : Swift.UnsafePointer<iop_struct_t> {
            return core__licence_module__sp
        }

        open override class var isAbstract : Swift.Bool {
            return true
        }

        public var `expirationDate` : Swift.String?
        public var `expirationWarningDelay` : Swift.UInt32

        public init(`expirationDate`: Swift.String? = nil,
                    `expirationWarningDelay`: Swift.UInt32 = 1296000) {
            self.expirationDate = `expirationDate`
            self.expirationWarningDelay = `expirationWarningDelay`
            super.init()
        }

        public required init(_ c: Swift.UnsafeRawPointer) throws {
            let data = c.bindMemory(to: core__licence_module__t.self, capacity: 1)
             self.expirationDate = Swift.String(data.pointee.expiration_date)
            self.expirationWarningDelay = data.pointee.expiration_warning_delay
            try super.init(c)
        }

        open override func fill(_ c: Swift.UnsafeMutableRawPointer, on allocator: libcommon.FrameBasedAllocator) {
            let data = c.bindMemory(to: core__licence_module__t.self, capacity: 1)
            if let expirationDate_val = self.expirationDate {
                data.pointee.expiration_date = expirationDate_val.duplicated(on: allocator)
            }
            data.pointee.expiration_warning_delay = self.expirationWarningDelay
        }
    }

    open class Licence : libcommon.IopClass {
        open override class var descriptor : Swift.UnsafePointer<iop_struct_t> {
            return core__licence__sp
        }

        open override class var isAbstract : Swift.Bool {
            return false
        }

        public var `expirationDate` : Swift.String
        public var `expirationHardDate` : Swift.String?
        public var `expirationWarningDelay` : Swift.UInt32
        public var `registeredTo` : Swift.String
        public var `version` : Swift.String
        public var `productionUse` : Swift.Bool
        public var `cpuSignatures` : [Swift.Int64]
        public var `macAddresses` : [Swift.String]
        public var `modules` : [core_package.LicenceModule]
        public var `signatureTs` : Swift.Int64?

        public init(`expirationDate`: Swift.String = "31-dec-2035",
                    `expirationHardDate`: Swift.String? = nil,
                    `expirationWarningDelay`: Swift.UInt32 = 1296000,
                    `registeredTo`: Swift.String,
                    `version`: Swift.String,
                    `productionUse`: Swift.Bool,
                    `cpuSignatures`: [Swift.Int64] = [],
                    `macAddresses`: [Swift.String] = [],
                    `modules`: [core_package.LicenceModule] = [],
                    `signatureTs`: Swift.Int64? = nil) {
            self.expirationDate = `expirationDate`
            self.expirationHardDate = `expirationHardDate`
            self.expirationWarningDelay = `expirationWarningDelay`
            self.registeredTo = `registeredTo`
            self.version = `version`
            self.productionUse = `productionUse`
            self.cpuSignatures = `cpuSignatures`
            self.macAddresses = `macAddresses`
            self.modules = `modules`
            self.signatureTs = `signatureTs`
            super.init()
        }

        public required init(_ c: Swift.UnsafeRawPointer) throws {
            let data = c.bindMemory(to: core__licence__t.self, capacity: 1)
            self.expirationDate = Swift.String(data.pointee.expiration_date) ?? ""
             self.expirationHardDate = Swift.String(data.pointee.expiration_hard_date)
            self.expirationWarningDelay = data.pointee.expiration_warning_delay
            self.registeredTo = Swift.String(data.pointee.registered_to) ?? ""
            self.version = Swift.String(data.pointee.version) ?? ""
            self.productionUse = data.pointee.production_use
            self.cpuSignatures = Swift.Array(data.pointee.cpu_signatures.buffer)
            self.macAddresses = data.pointee.mac_addresses.buffer.map {                return Swift.String($0) ?? ""}
            self.modules = try data.pointee.modules.buffer.map {                return try core_package.LicenceModule.make(Swift.UnsafeRawPointer($0))}
            if data.pointee.signature_ts.has_field {
                self.signatureTs = data.pointee.signature_ts.v
            }
            try super.init(c)
        }

        open override func fill(_ c: Swift.UnsafeMutableRawPointer, on allocator: libcommon.FrameBasedAllocator) {
            let data = c.bindMemory(to: core__licence__t.self, capacity: 1)
             data.pointee.expiration_date = self.expirationDate.duplicated(on: allocator)
            if let expirationHardDate_val = self.expirationHardDate {
                data.pointee.expiration_hard_date = expirationHardDate_val.duplicated(on: allocator)
            }
            data.pointee.expiration_warning_delay = self.expirationWarningDelay
             data.pointee.registered_to = self.registeredTo.duplicated(on: allocator)
             data.pointee.version = self.version.duplicated(on: allocator)
            data.pointee.production_use = self.productionUse
            data.pointee.cpu_signatures.tab = self.cpuSignatures.duplicated(on: allocator)
            data.pointee.cpu_signatures.len = Swift.Int32(self.cpuSignatures.count)
            data.pointee.mac_addresses = .init(self.macAddresses, on: allocator)
            libcommon.duplicate(classArray: self.modules, to: &data.pointee.modules, on: allocator)
            if let signatureTs_val = self.signatureTs {
                data.pointee.signature_ts.has_field = true
                data.pointee.signature_ts.v = signatureTs_val
            } else {
                data.pointee.signature_ts.has_field = false
            }
        }
    }

    public final class SignedLicence : libcommon.IopStruct {
        open override class var descriptor : Swift.UnsafePointer<iop_struct_t> {
            return core__signed_licence__sp
        }

        public var `licence` : core_package.Licence
        public var `signature` : Swift.String

        public init(`licence`: core_package.Licence,
                    `signature`: Swift.String) {
            self.licence = `licence`
            self.signature = `signature`
            super.init()
        }

        public required init(_ c: Swift.UnsafeRawPointer) throws {
            let data = c.bindMemory(to: core__signed_licence__t.self, capacity: 1)
            self.licence = try core_package.Licence.make(Swift.UnsafeRawPointer(data.pointee.licence))
            self.signature = Swift.String(data.pointee.signature) ?? ""
            try super.init(c)
        }

        open override func fill(_ c: Swift.UnsafeMutableRawPointer, on allocator: libcommon.FrameBasedAllocator) {
            let data = c.bindMemory(to: core__signed_licence__t.self, capacity: 1)
            data.pointee.licence = self.licence.duplicated(on: allocator).bindMemory(to: core__licence__t.self, capacity: 1)
             data.pointee.signature = self.signature.duplicated(on: allocator)
        }
    }

    public final class HttpdCfg : libcommon.IopStruct {
        open override class var descriptor : Swift.UnsafePointer<iop_struct_t> {
            return core__httpd_cfg__sp
        }

        public var `bindAddr` : Swift.String
        public var `outbufMaxSize` : Swift.UInt32
        public var `pipelineDepth` : Swift.UInt16
        public var `noactDelay` : Swift.UInt32
        public var `maxQueries` : Swift.UInt32
        public var `maxConnsIn` : Swift.UInt32
        public var `onDataThreshold` : Swift.UInt32
        public var `headerLineMax` : Swift.UInt32
        public var `headerSizeMax` : Swift.UInt32

        public init(`bindAddr`: Swift.String,
                    `outbufMaxSize`: Swift.UInt32 = 33554432,
                    `pipelineDepth`: Swift.UInt16 = 32,
                    `noactDelay`: Swift.UInt32 = 30000,
                    `maxQueries`: Swift.UInt32 = 1024,
                    `maxConnsIn`: Swift.UInt32 = 1000,
                    `onDataThreshold`: Swift.UInt32 = 16384,
                    `headerLineMax`: Swift.UInt32 = 1024,
                    `headerSizeMax`: Swift.UInt32 = 65536) {
            self.bindAddr = `bindAddr`
            self.outbufMaxSize = `outbufMaxSize`
            self.pipelineDepth = `pipelineDepth`
            self.noactDelay = `noactDelay`
            self.maxQueries = `maxQueries`
            self.maxConnsIn = `maxConnsIn`
            self.onDataThreshold = `onDataThreshold`
            self.headerLineMax = `headerLineMax`
            self.headerSizeMax = `headerSizeMax`
            super.init()
        }

        public required init(_ c: Swift.UnsafeRawPointer) throws {
            let data = c.bindMemory(to: core__httpd_cfg__t.self, capacity: 1)
            self.bindAddr = Swift.String(data.pointee.bind_addr) ?? ""
            self.outbufMaxSize = data.pointee.outbuf_max_size
            self.pipelineDepth = data.pointee.pipeline_depth
            self.noactDelay = data.pointee.noact_delay
            self.maxQueries = data.pointee.max_queries
            self.maxConnsIn = data.pointee.max_conns_in
            self.onDataThreshold = data.pointee.on_data_threshold
            self.headerLineMax = data.pointee.header_line_max
            self.headerSizeMax = data.pointee.header_size_max
            try super.init(c)
        }

        open override func fill(_ c: Swift.UnsafeMutableRawPointer, on allocator: libcommon.FrameBasedAllocator) {
            let data = c.bindMemory(to: core__httpd_cfg__t.self, capacity: 1)
             data.pointee.bind_addr = self.bindAddr.duplicated(on: allocator)
            data.pointee.outbuf_max_size = self.outbufMaxSize
            data.pointee.pipeline_depth = self.pipelineDepth
            data.pointee.noact_delay = self.noactDelay
            data.pointee.max_queries = self.maxQueries
            data.pointee.max_conns_in = self.maxConnsIn
            data.pointee.on_data_threshold = self.onDataThreshold
            data.pointee.header_line_max = self.headerLineMax
            data.pointee.header_size_max = self.headerSizeMax
        }
    }

    public final class HttpcCfg : libcommon.IopStruct {
        open override class var descriptor : Swift.UnsafePointer<iop_struct_t> {
            return core__httpc_cfg__sp
        }

        public var `pipelineDepth` : Swift.UInt16
        public var `noactDelay` : Swift.UInt32
        public var `maxQueries` : Swift.UInt32
        public var `onDataThreshold` : Swift.UInt32
        public var `headerLineMax` : Swift.UInt32
        public var `headerSizeMax` : Swift.UInt32

        public init(`pipelineDepth`: Swift.UInt16 = 32,
                    `noactDelay`: Swift.UInt32 = 30000,
                    `maxQueries`: Swift.UInt32 = 1024,
                    `onDataThreshold`: Swift.UInt32 = 16384,
                    `headerLineMax`: Swift.UInt32 = 1024,
                    `headerSizeMax`: Swift.UInt32 = 65536) {
            self.pipelineDepth = `pipelineDepth`
            self.noactDelay = `noactDelay`
            self.maxQueries = `maxQueries`
            self.onDataThreshold = `onDataThreshold`
            self.headerLineMax = `headerLineMax`
            self.headerSizeMax = `headerSizeMax`
            super.init()
        }

        public required init(_ c: Swift.UnsafeRawPointer) throws {
            let data = c.bindMemory(to: core__httpc_cfg__t.self, capacity: 1)
            self.pipelineDepth = data.pointee.pipeline_depth
            self.noactDelay = data.pointee.noact_delay
            self.maxQueries = data.pointee.max_queries
            self.onDataThreshold = data.pointee.on_data_threshold
            self.headerLineMax = data.pointee.header_line_max
            self.headerSizeMax = data.pointee.header_size_max
            try super.init(c)
        }

        open override func fill(_ c: Swift.UnsafeMutableRawPointer, on allocator: libcommon.FrameBasedAllocator) {
            let data = c.bindMemory(to: core__httpc_cfg__t.self, capacity: 1)
            data.pointee.pipeline_depth = self.pipelineDepth
            data.pointee.noact_delay = self.noactDelay
            data.pointee.max_queries = self.maxQueries
            data.pointee.on_data_threshold = self.onDataThreshold
            data.pointee.header_line_max = self.headerLineMax
            data.pointee.header_size_max = self.headerSizeMax
        }
    }

    public final class IopJsonSubfile : libcommon.IopStruct {
        open override class var descriptor : Swift.UnsafePointer<iop_struct_t> {
            return core__iop_json_subfile__sp
        }

        public var `filePath` : Swift.String
        public var `iopPath` : Swift.String

        public init(`filePath`: Swift.String,
                    `iopPath`: Swift.String) {
            self.filePath = `filePath`
            self.iopPath = `iopPath`
            super.init()
        }

        public required init(_ c: Swift.UnsafeRawPointer) throws {
            let data = c.bindMemory(to: core__iop_json_subfile__t.self, capacity: 1)
            self.filePath = Swift.String(data.pointee.file_path) ?? ""
            self.iopPath = Swift.String(data.pointee.iop_path) ?? ""
            try super.init(c)
        }

        open override func fill(_ c: Swift.UnsafeMutableRawPointer, on allocator: libcommon.FrameBasedAllocator) {
            let data = c.bindMemory(to: core__iop_json_subfile__t.self, capacity: 1)
             data.pointee.file_path = self.filePath.duplicated(on: allocator)
             data.pointee.iop_path = self.iopPath.duplicated(on: allocator)
        }
    }

    public enum interfaces {
        public struct Log : libcommon.IopInterface {
            public enum SetRootLevel : libcommon.IopRPC {
                public final class Argument : libcommon.IopStruct {
                    open override class var descriptor : Swift.UnsafePointer<iop_struct_t> {
                        return core__log__set_root_level_args__sp
                    }

                    public var `level` : core_package.LogLevel
                    public var `forceAll` : Swift.Bool
                    public var `isSilent` : Swift.Bool

                    public init(`level`: core_package.LogLevel,
                                `forceAll`: Swift.Bool = false,
                                `isSilent`: Swift.Bool = false) {
                        self.level = `level`
                        self.forceAll = `forceAll`
                        self.isSilent = `isSilent`
                        super.init()
                    }

                    public required init(_ c: Swift.UnsafeRawPointer) throws {
                        let data = c.bindMemory(to: core__log__set_root_level_args__t.self, capacity: 1)
                        self.level = data.pointee.level
                        self.forceAll = data.pointee.force_all
                        self.isSilent = data.pointee.is_silent
                        try super.init(c)
                    }

                    open override func fill(_ c: Swift.UnsafeMutableRawPointer, on allocator: libcommon.FrameBasedAllocator) {
                        let data = c.bindMemory(to: core__log__set_root_level_args__t.self, capacity: 1)
                        data.pointee.level = self.level
                        data.pointee.force_all = self.forceAll
                        data.pointee.is_silent = self.isSilent
                    }
                }

                public final class Response : libcommon.IopStruct {
                    open override class var descriptor : Swift.UnsafePointer<iop_struct_t> {
                        return core__log__set_root_level_res__sp
                    }

                    public var `level` : core_package.LogLevel

                    public init(`level`: core_package.LogLevel) {
                        self.level = `level`
                        super.init()
                    }

                    public required init(_ c: Swift.UnsafeRawPointer) throws {
                        let data = c.bindMemory(to: core__log__set_root_level_res__t.self, capacity: 1)
                        self.level = data.pointee.level
                        try super.init(c)
                    }

                    open override func fill(_ c: Swift.UnsafeMutableRawPointer, on allocator: libcommon.FrameBasedAllocator) {
                        let data = c.bindMemory(to: core__log__set_root_level_res__t.self, capacity: 1)
                        data.pointee.level = self.level
                    }
                }

                public static let descriptor = core__log__if.funs.advanced(by: 0)
                public static let tag = 1
            }
            public var `setRootLevel` : (Int, SetRootLevel.Type) {
                return ((self._tag << 16) + SetRootLevel.tag, SetRootLevel.self)
            }

            public enum ResetRootLevel : libcommon.IopRPC {
                public final class Response : libcommon.IopStruct {
                    open override class var descriptor : Swift.UnsafePointer<iop_struct_t> {
                        return core__log__reset_root_level_res__sp
                    }

                    public var `level` : core_package.LogLevel

                    public init(`level`: core_package.LogLevel) {
                        self.level = `level`
                        super.init()
                    }

                    public required init(_ c: Swift.UnsafeRawPointer) throws {
                        let data = c.bindMemory(to: core__log__reset_root_level_res__t.self, capacity: 1)
                        self.level = data.pointee.level
                        try super.init(c)
                    }

                    open override func fill(_ c: Swift.UnsafeMutableRawPointer, on allocator: libcommon.FrameBasedAllocator) {
                        let data = c.bindMemory(to: core__log__reset_root_level_res__t.self, capacity: 1)
                        data.pointee.level = self.level
                    }
                }

                public static let descriptor = core__log__if.funs.advanced(by: 1)
                public static let tag = 2
            }
            public var `resetRootLevel` : (Int, ResetRootLevel.Type) {
                return ((self._tag << 16) + ResetRootLevel.tag, ResetRootLevel.self)
            }

            public enum SetLoggerLevel : libcommon.IopRPC {
                public final class Argument : libcommon.IopStruct {
                    open override class var descriptor : Swift.UnsafePointer<iop_struct_t> {
                        return core__log__set_logger_level_args__sp
                    }

                    public var `fullName` : Swift.String
                    public var `level` : core_package.LogLevel
                    public var `forceAll` : Swift.Bool
                    public var `isSilent` : Swift.Bool

                    public init(`fullName`: Swift.String,
                                `level`: core_package.LogLevel,
                                `forceAll`: Swift.Bool = false,
                                `isSilent`: Swift.Bool = false) {
                        self.fullName = `fullName`
                        self.level = `level`
                        self.forceAll = `forceAll`
                        self.isSilent = `isSilent`
                        super.init()
                    }

                    public required init(_ c: Swift.UnsafeRawPointer) throws {
                        let data = c.bindMemory(to: core__log__set_logger_level_args__t.self, capacity: 1)
                        self.fullName = Swift.String(data.pointee.full_name) ?? ""
                        self.level = data.pointee.level
                        self.forceAll = data.pointee.force_all
                        self.isSilent = data.pointee.is_silent
                        try super.init(c)
                    }

                    open override func fill(_ c: Swift.UnsafeMutableRawPointer, on allocator: libcommon.FrameBasedAllocator) {
                        let data = c.bindMemory(to: core__log__set_logger_level_args__t.self, capacity: 1)
                         data.pointee.full_name = self.fullName.duplicated(on: allocator)
                        data.pointee.level = self.level
                        data.pointee.force_all = self.forceAll
                        data.pointee.is_silent = self.isSilent
                    }
                }

                public final class Response : libcommon.IopStruct {
                    open override class var descriptor : Swift.UnsafePointer<iop_struct_t> {
                        return core__log__set_logger_level_res__sp
                    }

                    public var `level` : core_package.LogLevel

                    public init(`level`: core_package.LogLevel) {
                        self.level = `level`
                        super.init()
                    }

                    public required init(_ c: Swift.UnsafeRawPointer) throws {
                        let data = c.bindMemory(to: core__log__set_logger_level_res__t.self, capacity: 1)
                        self.level = data.pointee.level
                        try super.init(c)
                    }

                    open override func fill(_ c: Swift.UnsafeMutableRawPointer, on allocator: libcommon.FrameBasedAllocator) {
                        let data = c.bindMemory(to: core__log__set_logger_level_res__t.self, capacity: 1)
                        data.pointee.level = self.level
                    }
                }

                public static let descriptor = core__log__if.funs.advanced(by: 2)
                public static let tag = 3
            }
            public var `setLoggerLevel` : (Int, SetLoggerLevel.Type) {
                return ((self._tag << 16) + SetLoggerLevel.tag, SetLoggerLevel.self)
            }

            public enum ResetLoggerLevel : libcommon.IopRPC {
                public final class Argument : libcommon.IopStruct {
                    open override class var descriptor : Swift.UnsafePointer<iop_struct_t> {
                        return core__log__reset_logger_level_args__sp
                    }

                    public var `fullName` : Swift.String

                    public init(`fullName`: Swift.String) {
                        self.fullName = `fullName`
                        super.init()
                    }

                    public required init(_ c: Swift.UnsafeRawPointer) throws {
                        let data = c.bindMemory(to: core__log__reset_logger_level_args__t.self, capacity: 1)
                        self.fullName = Swift.String(data.pointee.full_name) ?? ""
                        try super.init(c)
                    }

                    open override func fill(_ c: Swift.UnsafeMutableRawPointer, on allocator: libcommon.FrameBasedAllocator) {
                        let data = c.bindMemory(to: core__log__reset_logger_level_args__t.self, capacity: 1)
                         data.pointee.full_name = self.fullName.duplicated(on: allocator)
                    }
                }

                public final class Response : libcommon.IopStruct {
                    open override class var descriptor : Swift.UnsafePointer<iop_struct_t> {
                        return core__log__reset_logger_level_res__sp
                    }

                    public var `level` : core_package.LogLevel

                    public init(`level`: core_package.LogLevel) {
                        self.level = `level`
                        super.init()
                    }

                    public required init(_ c: Swift.UnsafeRawPointer) throws {
                        let data = c.bindMemory(to: core__log__reset_logger_level_res__t.self, capacity: 1)
                        self.level = data.pointee.level
                        try super.init(c)
                    }

                    open override func fill(_ c: Swift.UnsafeMutableRawPointer, on allocator: libcommon.FrameBasedAllocator) {
                        let data = c.bindMemory(to: core__log__reset_logger_level_res__t.self, capacity: 1)
                        data.pointee.level = self.level
                    }
                }

                public static let descriptor = core__log__if.funs.advanced(by: 3)
                public static let tag = 4
            }
            public var `resetLoggerLevel` : (Int, ResetLoggerLevel.Type) {
                return ((self._tag << 16) + ResetLoggerLevel.tag, ResetLoggerLevel.self)
            }

            public enum ListLoggers : libcommon.IopRPC {
                public final class Argument : libcommon.IopStruct {
                    open override class var descriptor : Swift.UnsafePointer<iop_struct_t> {
                        return core__log__list_loggers_args__sp
                    }

                    public var `prefix` : Swift.String?

                    public init(`prefix`: Swift.String? = nil) {
                        self.prefix = `prefix`
                        super.init()
                    }

                    public required init(_ c: Swift.UnsafeRawPointer) throws {
                        let data = c.bindMemory(to: core__log__list_loggers_args__t.self, capacity: 1)
                         self.prefix = Swift.String(data.pointee.prefix)
                        try super.init(c)
                    }

                    open override func fill(_ c: Swift.UnsafeMutableRawPointer, on allocator: libcommon.FrameBasedAllocator) {
                        let data = c.bindMemory(to: core__log__list_loggers_args__t.self, capacity: 1)
                        if let prefix_val = self.prefix {
                            data.pointee.prefix = prefix_val.duplicated(on: allocator)
                        }
                    }
                }

                public final class Response : libcommon.IopStruct {
                    open override class var descriptor : Swift.UnsafePointer<iop_struct_t> {
                        return core__log__list_loggers_res__sp
                    }

                    public var `loggers` : [core_package.LoggerConfiguration]

                    public init(`loggers`: [core_package.LoggerConfiguration] = []) {
                        self.loggers = `loggers`
                        super.init()
                    }

                    public required init(_ c: Swift.UnsafeRawPointer) throws {
                        let data = c.bindMemory(to: core__log__list_loggers_res__t.self, capacity: 1)
                        self.loggers = try data.pointee.loggers.buffer.map {
                            var loggers_var = $0
                            return try core_package.LoggerConfiguration(&loggers_var)
                            }
                        try super.init(c)
                    }

                    open override func fill(_ c: Swift.UnsafeMutableRawPointer, on allocator: libcommon.FrameBasedAllocator) {
                        let data = c.bindMemory(to: core__log__list_loggers_res__t.self, capacity: 1)
                        libcommon.duplicate(complexTypeArray: self.loggers, to: &data.pointee.loggers, on: allocator)
                    }
                }

                public static let descriptor = core__log__if.funs.advanced(by: 4)
                public static let tag = 5
            }
            public var `listLoggers` : (Int, ListLoggers.Type) {
                return ((self._tag << 16) + ListLoggers.tag, ListLoggers.self)
            }

            public struct Impl : libcommon.IopInterfaceImpl {
                public func setRootLevel(_ args: SetRootLevel.Argument) -> SetRootLevel.ReturnType {
                    return self._query(rpc: SetRootLevel.self, args: args)
                }
                public func `setRootLevel`(`level`: core_package.LogLevel,
                                     `forceAll`: Swift.Bool = false,
                                     `isSilent`: Swift.Bool = false) -> SetRootLevel.ReturnType {
                    return self.setRootLevel(SetRootLevel.Argument(level: `level`,
                                                               forceAll: `forceAll`,
                                                               isSilent: `isSilent`))
                }

                public func `resetRootLevel`() -> ResetRootLevel.ReturnType {
                    return self._query(rpc: ResetRootLevel.self, args: libcommon.IopVoid())
                }

                public func setLoggerLevel(_ args: SetLoggerLevel.Argument) -> SetLoggerLevel.ReturnType {
                    return self._query(rpc: SetLoggerLevel.self, args: args)
                }
                public func `setLoggerLevel`(`fullName`: Swift.String,
                                       `level`: core_package.LogLevel,
                                       `forceAll`: Swift.Bool = false,
                                       `isSilent`: Swift.Bool = false) -> SetLoggerLevel.ReturnType {
                    return self.setLoggerLevel(SetLoggerLevel.Argument(fullName: `fullName`,
                                                                   level: `level`,
                                                                   forceAll: `forceAll`,
                                                                   isSilent: `isSilent`))
                }

                public func resetLoggerLevel(_ args: ResetLoggerLevel.Argument) -> ResetLoggerLevel.ReturnType {
                    return self._query(rpc: ResetLoggerLevel.self, args: args)
                }
                public func `resetLoggerLevel`(`fullName`: Swift.String) -> ResetLoggerLevel.ReturnType {
                    return self.resetLoggerLevel(ResetLoggerLevel.Argument(fullName: `fullName`))
                }

                public func listLoggers(_ args: ListLoggers.Argument) -> ListLoggers.ReturnType {
                    return self._query(rpc: ListLoggers.self, args: args)
                }
                public func `listLoggers`(`prefix`: Swift.String? = nil) -> ListLoggers.ReturnType {
                    return self.listLoggers(ListLoggers.Argument(prefix: `prefix`))
                }


                public let _tag : Swift.Int
                public let _channel : libcommon.IopChannel
                public init(channel: libcommon.IopChannel, tag: Swift.Int) {
                    self._channel = channel
                    self._tag = tag
                }
            }

            public let _tag : Swift.Int
            public init(tag: Swift.Int) {
                self._tag = tag
            }
        }

    }
    public enum modules {
        public struct Core : core__modules__Core {
            public let channel : libcommon.IopChannel
            public init(channel: libcommon.IopChannel) {
                self.channel = channel
            }
        }
    }
    public static let classes : [libcommon.IopClass.Type] = [ LogFileConfiguration.self, LicenceModule.self, Licence.self ]
}

public typealias core_package = core
