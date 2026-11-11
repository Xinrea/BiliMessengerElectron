#import <Cocoa/Cocoa.h>
#include "ui/window.h"

void bm_window_setup(void *native_window) {
    if (!native_window) return;
    @autoreleasepool {
        NSWindow *w = (__bridge NSWindow *)native_window;
        w.titleVisibility = NSWindowTitleHidden;
        w.titlebarAppearsTransparent = YES;
        w.styleMask |= NSWindowStyleMaskFullSizeContentView;
        w.movableByWindowBackground = NO;
    }
}

void bm_window_drag(void *native_window) {
    if (!native_window) return;
    @autoreleasepool {
        NSWindow *w = (__bridge NSWindow *)native_window;
        NSEvent *event = [NSApp currentEvent];
        if (event) {
            if (event.clickCount == 2) {
                [w performZoom:nil];
            } else {
                [w performWindowDragWithEvent:event];
            }
        }
    }
}

void bm_window_minimize(void *native_window) {
    if (!native_window) return;
    @autoreleasepool {
        [(__bridge NSWindow *)native_window performMiniaturize:nil];
    }
}

void bm_window_maximize_toggle(void *native_window) {
    if (!native_window) return;
    @autoreleasepool {
        [(__bridge NSWindow *)native_window performZoom:nil];
    }
}

void bm_window_close(void *native_window) {
    if (!native_window) return;
    @autoreleasepool {
        [(__bridge NSWindow *)native_window performClose:nil];
    }
}

bool bm_window_is_maximized(void *native_window) {
    if (!native_window) return false;
    @autoreleasepool {
        return [(__bridge NSWindow *)native_window isZoomed];
    }
}

bool bm_window_has_native_traffic_lights(void) {
    return true;
}
