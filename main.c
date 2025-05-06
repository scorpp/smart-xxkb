#include <X11/Xlib.h>
#include <X11/XKBlib.h>
#include <X11/Xatom.h>
#include <err.h>
#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include "lru.h"

#define MAXSTR 500
#define PER_WINDOW_CACHE_SIZE 50

typedef struct {
    Window window;
    char *windowTitle;
    LruCache *groupCache;
} ActiveWindow;

Atom NET_ACTIVE_WINDOW;
Atom NET_WM_NAME;
Atom WM_NAME;
Atom MY_KBD_DATA;

Display *display = NULL;
XErrorHandler defaultXErrorHandler = NULL;
ActiveWindow prevActive;
volatile sig_atomic_t acceptEvents = True;


void
terminate_handler(int signum) {
    warnx("Caught SIGTERM|SIGINT, cleaning up and exiting...");
    acceptEvents = False;
}

Display *
xkb_open_display(int *xkbEventType) {
    Display *display;
    int xkbError, xkbReason;
    int mjr = XkbMajorVersion, mnr = XkbMinorVersion;

    display = XkbOpenDisplay(NULL, xkbEventType, &xkbError, &mjr, &mnr, &xkbReason);
    if (NULL == display) {
        warnx("Cannot open X display %s", XDisplayName(NULL));
        switch (xkbReason) {
            case XkbOD_BadLibraryVersion:
            case XkbOD_BadServerVersion:
                warnx("Incompatible versions of client and server xkb libraries");
                break;
            case XkbOD_ConnectionRefused:
                warnx("Connection to X server refused");
                break;
            case XkbOD_NonXkbServer:
                warnx("XKB extension not present");
                break;
            default:
                warnx("Unknown error in XkbOpenDisplay: %d", xkbReason);
        }
        exit(1);
    }

    return display;
}

static void
subscribe_window_events(Display *display, Window window, long eventMask) {
    XSetWindowAttributes windowAttrs;
    windowAttrs.event_mask = eventMask;
    XChangeWindowAttributes(display, window, CWEventMask, &windowAttrs);
}

Window
get_active_window(Display *display) {
    Atom actualType;
    int actualFormat;
    unsigned long nitems, bytes;
    Window *activeWindowId = NULL;
    Window result = 0;
    int status;
    Window root = DefaultRootWindow(display);

    status = XGetWindowProperty(
            display,
            root,
            NET_ACTIVE_WINDOW,
            0,
            MAXSTR,
            False,
            AnyPropertyType,
            &actualType,
            &actualFormat,
            &nitems,
            &bytes,
            (unsigned char **) &activeWindowId
    );
    if (status == Success && activeWindowId) {
        result = *activeWindowId;
        XFree(activeWindowId);
    } else {
        warnx("Cannot get id of current active window");
    }

    return result;
}

int
get_string_property(Display *display, Window window, Atom atom, char **value) {
    Atom actualType;
    int actualFormat;
    unsigned long nitems, bytes;
    int status;

    status = XGetWindowProperty(
            display,
            window,
            atom,
            0,
            MAXSTR,
            False,
            AnyPropertyType,
            &actualType,
            &actualFormat,
            &nitems,
            &bytes,
            (unsigned char **) value
    );
    XSync(display, False);

    if (status != Success) {
        warnx("Cannot get string property %lu of %lu, reason %d", atom, window, status);
    }

    return status;
}

void
set_string_property(Display *display, Window window, Atom property, char *serialized) {
    int status;

    status = XChangeProperty(
            display,
            window,
            property,
            XA_STRING,
            8,
            PropModeReplace,
            (const unsigned char *) serialized,
            (int) strlen(serialized) + 1
    );
    XSync(display, False);

    if (status == BadAlloc
            || status == BadAtom
            || status == BadMatch
            || status == BadValue
            || status == BadWindow) {
        warnx("Cannot set string property %lu of %lu, reason %d", property, window, status);
    }
}

void
persist_old_cache() {
    if (prevActive.groupCache) {
        Window dummy_root, dummy_parent;
        Window *dummy_children = NULL;
        unsigned int dummy_nchildren;
        char *serialized = lru_serialize(prevActive.groupCache);

        XGrabServer(display);  // Lock X server to prevent window destruction
         // Check if window still exists
        if (XQueryTree(display,
                       prevActive.window,
                       &dummy_root,
                       &dummy_parent,
                       &dummy_children,
                       &dummy_nchildren)) {
            if (dummy_children) {
                XFree(dummy_children);
            }
            
            set_string_property(display, prevActive.window, MY_KBD_DATA, serialized);
        }
        XUngrabServer(display);

        lru_free(prevActive.groupCache);
        free(serialized);
        prevActive.groupCache = NULL;
    }
}

void
init_new_cache(Window activeWindow) {
    char *str = NULL;
    LruCache *newCache = NULL;

    if (get_string_property(display, activeWindow, MY_KBD_DATA, &str) == Success) {
        if (str != NULL) {
            newCache = lru_deserialize(str);
            XFree(str);
        }
    }
    
    if (newCache == NULL) {
        newCache = lru_new(PER_WINDOW_CACHE_SIZE);
    }

    prevActive.groupCache = newCache;
}

int
get_window_title(Display *display, Window window, char **title) {
    int status;

    status = get_string_property(display, window, NET_WM_NAME, title);
    if (NULL == *title)
        status = get_string_property(display, window, WM_NAME, title);

    return status;
}

int
handle_change_common(Display *display, Window activeWindow) {
    int changed = False;
    if (0 == activeWindow) return False;

    if (prevActive.window != activeWindow) {
        // don't cancel property change event mask on root window, we still need to sniff active window
        if (prevActive.window != 0 && prevActive.window != DefaultRootWindow(display)) {
            subscribe_window_events(display, activeWindow, NoEventMask);
            persist_old_cache();
        }

        subscribe_window_events(display, activeWindow, PropertyChangeMask);
        prevActive.window = activeWindow;

        init_new_cache(activeWindow);
        changed = True;
    }

    char *activeTitle = NULL;
    int status;

    status = get_window_title(display, activeWindow, &activeTitle);

    if (status == Success && activeTitle != NULL) {
        if (prevActive.window != activeWindow
                || NULL == prevActive.windowTitle
                || (0 != strcmp(activeTitle, prevActive.windowTitle))) {
            strncpy(prevActive.windowTitle, activeTitle, MAXSTR - 1);
            prevActive.windowTitle[MAXSTR - 1] = '\0'; // Ensure null termination
            warnx("Window title of active window %lu to %s", activeWindow, activeTitle);
            changed = True;
        }
    } else {
        // Handle the case where we couldn't get the window title
        if (prevActive.windowTitle != NULL) {
            prevActive.windowTitle[0] = '\0';  // Set to empty string
        }
        warnx("Could not get window title for window %lu", activeWindow);
    }

    if (activeTitle != NULL) {
        XFree(activeTitle);
    }
    return changed;
}

void
handle_change_window(Display *display) {
    lru_value_t group;

    if (0 == lru_get(prevActive.groupCache, prevActive.windowTitle, &group)) {
        warnx("Restoring previous state %d", group);
        XkbLockGroup(display, XkbUseCoreKbd, group);

    } else {
        XkbStateRec xkbState;

        XkbGetState(display, XkbUseCoreKbd, &xkbState);

        lru_set(prevActive.groupCache, prevActive.windowTitle, xkbState.group);
        warnx("Set remembered group to %d", xkbState.group);
    }
}

void
handle_change_title(Display *display) {
    handle_change_window(display);
}

void
handle_change_xkb_group(Display *display, XkbStateNotifyEvent *stateEvent) {
    lru_set(prevActive.groupCache, prevActive.windowTitle, (lru_value_t) stateEvent->group);
    warnx("Set remembered group to %d", stateEvent->group);
}

int
my_x_error_handler(Display *display, XErrorEvent *event) {
    if (BadWindow == event->error_code) {
        // ignore
        return 0;
    } else if (defaultXErrorHandler){
        return defaultXErrorHandler(display, event);
    }
    return 0;
}

int
main() {
    int xkbEventType;

    memset(&prevActive, 0, sizeof(ActiveWindow));
    prevActive.windowTitle = (char *) malloc(MAXSTR);
    if (prevActive.windowTitle == NULL) {
        warnx("Failed to allocate memory for window title");
        exit(1);
    }
    prevActive.windowTitle[0] = '\0';
    prevActive.window = 0;
    prevActive.groupCache = NULL;

    display = xkb_open_display(&xkbEventType);

    fd_set fds;
    struct timeval tv;
    int x11_fd = ConnectionNumber(display);

    signal(SIGTERM, terminate_handler);
    signal(SIGINT, terminate_handler);

    defaultXErrorHandler = XSetErrorHandler(my_x_error_handler);

    XkbSelectEventDetails(display, XkbUseCoreKbd, XkbStateNotify, XkbAllComponentsMask, XkbGroupStateMask);

    /// watch active window

    NET_ACTIVE_WINDOW = XInternAtom(display, "_NET_ACTIVE_WINDOW", True);
    NET_WM_NAME       = XInternAtom(display, "_NET_WM_NAME", True);
    WM_NAME           = XInternAtom(display, "WM_NAME", True);
    MY_KBD_DATA       = XInternAtom(display, "X_TAB_KBD_GRP", False);
    if (NET_ACTIVE_WINDOW == None || NET_WM_NAME == None || WM_NAME == None || MY_KBD_DATA == None) {
        warnx("Failed to initialize required X atoms");
        exit(1);
    }

    Window root = DefaultRootWindow(display);
    warnx("Root window id %lu", root);

    subscribe_window_events(display, root, PropertyChangeMask);

    handle_change_common(display, get_active_window(display));

    while (acceptEvents) {
        // Setup select() to wait for X events        
        FD_ZERO(&fds);
        FD_SET(x11_fd, &fds);
        
        tv.tv_sec = 1;  // 1 second timeout
        tv.tv_usec = 0;
        
        // Wait for X Event or a Timer
        int num_ready_fds = select(x11_fd + 1, &fds, NULL, NULL, &tv);
        if (num_ready_fds < 0) {
            if (errno == EINTR) continue;  // Interrupted by signal
            warnx("Select error: %s", strerror(errno));
            break;
        }
        

        XkbEvent event;

        while (XPending(display)) {
            XNextEvent(display, (XEvent *) &event);
            if (!acceptEvents) break;  // Check again after potentially blocking call
            
            if (event.type == PropertyNotify) {
                XPropertyEvent *xproperty = &event.core.xproperty;
                if (xproperty->atom == NET_ACTIVE_WINDOW) {
                    warnx("Active window changed");
                    if (handle_change_common(display, get_active_window(display)))
                        handle_change_window(display);

                } else if (xproperty->atom == NET_WM_NAME || xproperty->atom == WM_NAME) {
                    warnx("Window title changed");
                    if (handle_change_common(display, xproperty->window))
                        handle_change_title(display);
                }
    //            warnx("Property change event, atom=%lu, serial=%lu, window=%lu",
    //                  xproperty.atom, xproperty.serial, xproperty.window);
            } else if (event.type == xkbEventType) {
                if (event.any.xkb_type == XkbStateNotify) {
                    XkbStateNotifyEvent *state = &event.state;
                    warnx("XkbStateNotify base_group=%d, group=%d, locked_group=%d, latched_group=%d",
                        state->base_group, state->group, state->locked_group, state->latched_group);
                    handle_change_xkb_group(display, state);
                }
            }
        }
    }

    if (display != NULL) {
        XCloseDisplay(display);
        display = NULL;
    }
    if (prevActive.windowTitle != NULL) {
        free(prevActive.windowTitle);
        prevActive.windowTitle = NULL;
    }
    if (prevActive.groupCache != NULL) {
        lru_free(prevActive.groupCache);
        prevActive.groupCache = NULL;
    }

    exit(EXIT_SUCCESS);
}
