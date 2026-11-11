#import <Cocoa/Cocoa.h>
#include "ui/window.h"

/* Drag region in content-view logical coordinates (top-left origin), updated every frame. */
static NSRect bm_drag_rect;
static __weak NSWindow *bm_drag_window;
static id bm_drag_monitor;

void bm_window_setup(void *native_window) {
    if (!native_window) return;
    @autoreleasepool {
        NSWindow *w = (__bridge NSWindow *)native_window;
        w.titleVisibility = NSWindowTitleHidden;
        w.titlebarAppearsTransparent = YES;
        w.styleMask |= NSWindowStyleMaskFullSizeContentView;
        w.movableByWindowBackground = NO;
        bm_drag_window = w;

        if (!bm_drag_monitor) {
            /* Handle the drag from the real mouse-down event: performWindowDragWithEvent:
             * needs it, and [NSApp currentEvent] during a frame may be any event type. */
            bm_drag_monitor = [NSEvent addLocalMonitorForEventsMatchingMask:NSEventMaskLeftMouseDown
                                                                    handler:^NSEvent *(NSEvent *e) {
                NSWindow *win = bm_drag_window;
                if (!win || e.window != win || NSIsEmptyRect(bm_drag_rect)) return e;
                NSView *cv = win.contentView;
                NSPoint p = [cv convertPoint:e.locationInWindow fromView:nil];
                if (!cv.isFlipped) p.y = cv.bounds.size.height - p.y;
                if (!NSPointInRect(p, bm_drag_rect)) return e;
                if (e.clickCount == 2) {
                    NSString *action = [[NSUserDefaults standardUserDefaults] stringForKey:@"AppleActionOnDoubleClick"];
                    if ([action isEqualToString:@"Minimize"]) [win performMiniaturize:nil];
                    else if (![action isEqualToString:@"None"]) [win performZoom:nil];
                } else {
                    [win performWindowDragWithEvent:e];
                }
                return nil; /* consumed: the UI never sees a press it would not get a release for */
            }];
        }
    }
}

void bm_window_set_drag_rect(void *native_window, float x, float y, float w, float h) {
    (void)native_window;
    bm_drag_rect = (w > 0 && h > 0) ? NSMakeRect(x, y, w, h) : NSZeroRect;
}

void bm_window_drag(void *native_window) {
    (void)native_window; /* handled by the event monitor on macOS */
}

void bm_window_minimize(void *native_window) {
    if (!native_window) return;
    [(__bridge NSWindow *)native_window performMiniaturize:nil];
}

void bm_window_maximize_toggle(void *native_window) {
    if (!native_window) return;
    [(__bridge NSWindow *)native_window performZoom:nil];
}

void bm_window_close(void *native_window) {
    if (!native_window) return;
    [(__bridge NSWindow *)native_window performClose:nil];
}

bool bm_window_is_maximized(void *native_window) {
    if (!native_window) return false;
    return [(__bridge NSWindow *)native_window isZoomed];
}

bool bm_window_has_native_traffic_lights(void) {
    return true;
}
