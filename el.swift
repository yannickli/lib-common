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

/// Event loop handler.
///
/// The event loop is the heart of a process. It waits for conditions that
/// trigger actions to be taken. Conditions can be the expiration of a timer,
/// new data to be read on a file-descriptor or a new signal received by
/// the process.
///
/// The event loop can contain both blocking and non-blocking handlers. A
/// blocking handler will prevent the event loop from terminating.
///
/// The list of waited condition is build by registering event loop handlers.
public class El {
    public typealias Priority = ev_priority_t

    let el : el_t

    init(_ el: el_t) {
        self.el = el
    }

    convenience init?(_ el: el_t?) {
        guard let el = el else {
            return nil
        }
        self.init(el)
    }

    /// Make the handler blocking the event loop.
    public func ref() -> Self {
        el_ref(el)
        return self
    }

    /// Make the handler non-blocking for the event loop.
    public func unref() -> Self {
        el_unref(el)
        return self
    }

    deinit {
        var el: el_t? = self.el

        el_unregister(&el)
    }

    /// Handler waiting for the process to be idle.
    ///
    /// An idle handler get triggered when the process is idle.
    /// Idle handler cannot be run more than once every minute,
    /// unless explictly unparked.
    public class Idle: El {
        /// Mark for execution as soon as possible.
        public func unpark() {
            el_idle_unpark(el)
        }
    }

    /// Handler waiting for a child to terminate.
    public class Child : El {
        /// PID of the child.
        public var pid : pid_t {
            return el_child_getpid(el)
        }

        /// Return status of the child.
        ///
        /// In case the child is terminated, contains the value
        /// returned by `wait()`. The return value is undefined in
        /// case the child is still running.
        public var status : Int {
            return Int(el_child_get_status(el))
        }
    }

    /// Handler waiting for event on a file-descriptor.
    public class Fd : El {
        public typealias LoopFlags = ev_fd_loop_flags_t

        /// Wait for activity on a single file-descriptor.
        ///
        /// Wait for activity on a single file-descriptor and exits when either
        /// activity is detected or when one of the following event occurs:
        /// - the specified timeout is reached
        /// - if the flags allow timers, if a timer expires
        /// - if the flags allow signals, if a signal is received.
        ///
        /// - Parameter timeout: the time we wait for an event, in milliseconds.
        ///
        /// - Parameter flags: flags applied such as whether we also wait for
        ///  timers or signals.
        ///
        /// - Returns: true if the handler triggered an event.
        public func loop(timeout: Int32, flags: LoopFlags = []) -> Bool {
            return el_fd_loop(el, timeout, flags) > 0
        }

        /// Wait for activity on a collection of file-descriptor handler.
        ///
        /// Wait for activity on a collection of file-descriptor and exits when
        /// either activity is detected or when one of the following event occurs:
        /// - the specified timeout is reached
        /// - if the flags allow timers, if a timer expires
        /// - if the flags allow signals, if a signal is received.
        ///
        /// - Parameter els: the collection of file-descriptor handler
        ///
        /// - Parameter timeout: the time we wait for an event, in milliseconds.
        ///
        /// - Parameter flags: flags applied such as whether we also wait for
        ///  timers or signals.
        ///
        /// - Returns: true if a handler triggered an event.
        public static func loop<C: Collection>(_ els: C, timeout: Int32, flags: LoopFlags = []) -> Bool
            where C.Iterator.Element: Fd {
            var els: [el_t] = els.map { $0.el }

            return els.withUnsafeMutableBufferPointer {
                return el_fds_loop($0.baseAddress!, Int32(els.count), timeout, flags) > -1
            }
        }

        /// Polling mask applied on the file-descriptor.
        public var mask: Int16 {
            get {
                return el_fd_get_mask(el)
            }

            set {
                el_fd_set_mask(el, newValue)
            }
        }

        /// File-descriptor watched by this handler
        public var fd: Int32 {
            return el_fd_get_fd(el)
        }

        /// Priority of the file-descriptor in the event loop.
        ///
        /// A file-descriptor can have one of the thread priorities: low, normal or high.
        /// in a single loop of the event loop, if several file-descriptor have pending
        /// events, then only the ones with the higher priority will be handled.
        public var priority : Priority {
            get {
                let priority = el_fd_set_priority(el, .normal)

                if priority != .normal {
                    el_fd_set_priority(el, priority)
                }
                return priority
            }

            set {
                el_fd_set_priority(el, newValue)
            }
        }

        /// Enable or disable activity watching on the file desciptor.
        ///
        /// When activity of a file descriptor is watched, the corresponding callback
        /// will be called when the `timeout` milliseconds are spent without any activity
        /// on the file descriptor.
        ///
        /// - Parameter mask: specify the type of watched events.
        ///
        /// - Parameter timeout: number of milliseconds without activity before firing
        ///  an event for the file descriptor.
        ///
        /// - Returns: the previous timeout.
        public func watchActivity(mask: Int16, timeout: Int32) -> Int32 {
            return el_fd_watch_activity(el, mask, timeout)
        }
    }

    /// Handler specifically designed to trigger an event on manual firing.
    public class Wake : El {
        /// Fire an event on that handler.
        public func fire() {
            el_wake_fire(el)
        }
    }

    /// Handler waiting for file-system events.
    public class FsWatch : El {
        /// Path of the file/directory monitored by the handler.
        public var path : String {
            return String(cString: el_fs_watch_get_path(el))
        }
    }

    /// Handler managing a singleshort or repeated timer.
    public class Timer : El {
        public typealias Flags = ev_timer_flags_t

        /// Indicates whether the timer is repeated.
        public var isRepeated : Bool {
            return el_timer_is_repeated(el)
        }

        /// Rearm the timer to fire in `timeout` milliseconds.
        public func restart(next timeout: Int64) {
            el_timer_restart(el, timeout)
        }
    }
}

/* Registration helpers */
extension El {
    /// Register a new handler specifically designed to block the event-loop.
    ///
    /// As long as this object is alive, the event loop cannot exit. A typical
    /// use case is to remove all references to that object when some exit
    /// condition is reached.
    ///
    ///    var blocker: El? = El.registerBlocker()
    ///
    ///    let signal = El.register(signal: SIGINT) { blocker = nil }
    ///    El.loop()
    public static func registerBlocker() -> El {
        return El(el_blocker_register())
    }

    /// Register a handler that is executed before every event loop.
    public static func runBefore(_ cb: @escaping el_cb_b) -> El {
        return El(el_before_register_blk(cb, nil))
    }

    /// Register a handler that is exected when the process is idle.
    public static func runWhenIdle(_ cb: @escaping el_cb_b) -> Idle {
        return Idle(el_idle_register_blk(cb, nil))
    }

    /// Register a handler to call when a signal of `signo` is received by the process.
    public static func register(signal signo: Int32, _ cb: @escaping el_signal_b) -> El {
        return El(el_signal_register_blk(Int32(signo), cb, nil))
    }

    /// Register a handler to call when the child of the given `pid` is terminated.
    public static func register(child pid: pid_t, _ cb: @escaping el_child_b) -> Child {
        return Child(el_child_register_blk(pid, cb, nil))
    }

    /// Register a handler for the specified `fd`
    ///
    /// - Parameter fd: the file descriptor to watch
    ///
    /// - Parameter ownFd: specify whether the handler owns `fd`. If it is the case,
    ///  the file-descriptor is clores when the return handler is unregistered.
    public static func register(fd: Int32, ownFd: Bool = true, events: Int16, _ cb: @escaping el_fd_b) -> Fd {
        return Fd(el_fd_register_blk(fd, ownFd, events, cb, nil))
    }

    /// Register a handler for a wake handler.
    public static func wake(_ cb: @escaping el_cb_b) -> Wake? {
        return Wake(el_wake_register_blk(cb, nil))
    }

    /// Register a file-system watcher.
    ///
    /// - Parameter path: the path to watch
    ///
    /// - Parameter flags: the events to watch.
    public static func watch(path: String, flags: UInt32, _ cb: @escaping el_fs_watch_b) -> FsWatch? {
        return FsWatch(el_fs_watch_register_blk(path, flags, cb, nil))
    }

    /// Schedule a new timer.
    ///
    /// - Parameter next: the number of milliseconds before the first occurrence of the timer.
    ///
    /// - Parameter repeatEvery: the number of milliseconds between occurrences of the timer. 0 means
    ///   the timer is a single shot timer.
    ///
    /// - Paramter flags: flags applied on the timer.
    public static func schedule(in next: Int64, repeatEvery: Int64 = 0, flags: Timer.Flags = [], _ cb: @escaping el_cb_b) -> Timer {
        return Timer(el_timer_register_blk(next, repeatEvery, flags, cb, nil))
    }
}

/* Running the event loop */
extension El {
    /// Run the event loop.
    public static func loop() {
        el_loop()
    }

    /// Force exit of the event loop.
    public static func unloop() {
        el_unloop()
    }

    /// Run a single loop that waits at most `timeout` milliseconds.
    public static func loop(timeout: Int32) {
        el_loop_timeout(timeout)
    }

    /// Specify whether the event loop has exited following the reception
    /// of a termination signal.
    public static var isTerminating : Bool {
        return el_is_terminating()
    }
}
