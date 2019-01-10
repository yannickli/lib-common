/**************************************************************************/
/*                                                                        */
/*  Copyright (C) INTERSEC SA                                             */
/*                                                                        */
/*  Should you receive a copy of this source code, you must check you     */
/*  have a proper, written authorization of INTERSEC to hold it. If you   */
/*  don't have such an authorization, you must DELETE all source code     */
/*  files in your possession, and inform INTERSEC of the fact you obtain  */
/*  these files. Should you not comply to these terms, you can be         */
/*  prosecuted in the extent permitted by applicable law.                 */
/*                                                                        */
/**************************************************************************/

#include <CoreServices/CoreServices.h>

static struct {
    el_t cron;
    int  watchers;
} fsevent_g;

static void el_fs_watch_shutdown(void)
{
}

static void el_fs_watch_cron(el_t el, data_t data)
{
    switch (CFRunLoopRunInMode(CFSTR("el"), 0.1, false)) {
      case kCFRunLoopRunFinished:
        break;

      case kCFRunLoopRunStopped:
        break;

      case kCFRunLoopRunTimedOut:
        break;

      case kCFRunLoopRunHandledSource:
        break;
    }
}

static void el_fs_events_cb(ConstFSEventStreamRef streamRef,
                            void *clientCallBackInfo, size_t numEvents,
                            void *eventPaths,
                            const FSEventStreamEventFlags eventFlags[],
                            const FSEventStreamEventId eventIds[])
{
    const char **paths = eventPaths;
    el_t el = clientCallBackInfo;

    for (size_t i = 0; i < numEvents; i++) {
        FSEventStreamEventFlags flags = eventFlags[i];

#if 0
        if (flags & kFSEventStreamEventFlagMustScanSubDirs) {
            e_trace(0, "MustScanSubDirs");
        }
        if (flags & kFSEventStreamEventFlagUserDropped) {
            e_trace(0, "UserDropped");
        }
        if (flags & kFSEventStreamEventFlagKernelDropped) {
            e_trace(0, "KernelDropped");
        }
        if (flags & kFSEventStreamEventFlagEventIdsWrapped) {
            e_trace(0, "EventIdsWrapped");
        }
        if (flags & kFSEventStreamEventFlagHistoryDone) {
            e_trace(0, "HistoryDone");
        }
        if (flags & kFSEventStreamEventFlagRootChanged) {
            e_trace(0, "RootChanged");
        }
        if (flags & kFSEventStreamEventFlagMount) {
            e_trace(0, "Mount");
        }
        if (flags & kFSEventStreamEventFlagUnmount) {
            e_trace(0, "Unmount");
        }
        if (flags & kFSEventStreamEventFlagItemCreated) {
            e_trace(0, "Created");
        }
        if (flags & kFSEventStreamEventFlagItemRemoved) {
            e_trace(0, "Removed");
        }
        if (flags & kFSEventStreamEventFlagItemInodeMetaMod) {
            e_trace(0, "ItemInodeMetaMod");
        }
        if (flags & kFSEventStreamEventFlagItemRenamed) {
            e_trace(0, "ItemInodeRenamed");
        }
        if (flags & kFSEventStreamEventFlagItemModified) {
            e_trace(0, "ItemModified");
        }
        if (flags & kFSEventStreamEventFlagItemFinderInfoMod) {
            e_trace(0, "ItemFinderInfoMod");
        }
        if (flags & kFSEventStreamEventFlagItemChangeOwner) {
            e_trace(0, "ItemChangeOwner");
        }
        if (flags & kFSEventStreamEventFlagItemXattrMod) {
            e_trace(0, "ItemXattrMod");
        }
        if (flags & kFSEventStreamEventFlagItemIsFile) {
            e_trace(0, "ItemIsFile");

            el_fs_watch_fire(el, 0, 0, LSTR(paths[i]));
        }
        if (flags & kFSEventStreamEventFlagItemIsDir) {
            e_trace(0, "ItemIsDir");
        }
        if (flags & kFSEventStreamEventFlagItemIsSymlink) {
            e_trace(0, "ItemIsSymlink");
        }
#endif
        if (flags & kFSEventStreamEventFlagItemIsFile) {
            lstr_t path = LSTR(paths[i] + strlen(el->fs_watch.path) + 1);
            struct stat st;
            bool path_exists = stat(paths[i], &st) >= 0;
            uint32_t notifs = 0;

            if (flags & kFSEventStreamEventFlagItemRenamed) {
                notifs |= path_exists ? IN_MOVED_TO : IN_MOVED_FROM;
            } else
            if (flags & kFSEventStreamEventFlagItemCreated) {
                notifs |= IN_CREATE;
            } else
            if (flags & kFSEventStreamEventFlagItemRemoved) {
                notifs |= IN_DELETE;
            }
            el_fs_watch_fire(el, notifs, 0, path);
        }
    }
}

el_t el_fs_watch_register_d(const char *path, uint32_t flags,
                            el_fs_watch_f *cb, data_t priv)
{
    FSEventStreamRef stream;
    CFArrayRef array;
    CFStringRef str;
    ev_t *ev = el_create(EV_FS_WATCH, cb, priv, true);

    str = CFStringCreateWithCString(NULL, path, kCFStringEncodingUTF8);
    array = CFArrayCreate(NULL, (const void **)&str, 1, NULL);

    stream = FSEventStreamCreate(NULL, &el_fs_events_cb,
                                 &(FSEventStreamContext){ .info = ev },
                                 array, kFSEventStreamEventIdSinceNow, 0.5,
                                 kFSEventStreamCreateFlagFileEvents
                                 | kFSEventStreamCreateFlagWatchRoot);

    CFRelease(array);
    CFRelease(str);

    array = FSEventStreamCopyPathsBeingWatched(stream);
    str = CFArrayGetValueAtIndex(array, 0);
    ev->fs_watch.path = p_strdup(CFStringGetCStringPtr(str,
                                                       kCFStringEncodingUTF8));
    CFRelease(array);

    FSEventStreamScheduleWithRunLoop(stream, CFRunLoopGetCurrent(),
                                     CFSTR("el"));
    if (!FSEventStreamStart(stream)) {
        logger_fatal(&_G.logger, "cannot setup event stream");
    }

    ev->fs_watch.ctx.ptr = stream;

    fsevent_g.watchers++;
    if (!fsevent_g.cron) {
        fsevent_g.cron = el_timer_register(0, 1000, EL_TIMER_LOWRES,
                                           &el_fs_watch_cron, NULL);
        el_unref(fsevent_g.cron);
    }

    return ev;
}

int el_fs_watch_change(el_t el, uint32_t flags)
{
    return 0;
}

data_t el_fs_watch_unregister(el_t *elp)
{
    if (*elp) {
        FSEventStreamRef stream = (*elp)->fs_watch.ctx.ptr;

        FSEventStreamStop(stream);
        FSEventStreamUnscheduleFromRunLoop(stream, CFRunLoopGetCurrent(),
                                           CFSTR("el"));
        FSEventStreamRelease(stream);

        if (--fsevent_g.watchers == 0) {
            el_timer_unregister(&fsevent_g.cron);
        }

        p_delete(&(*elp)->fs_watch.path);
        return el_destroy(elp);
    }
    return (data_t)NULL;
}
