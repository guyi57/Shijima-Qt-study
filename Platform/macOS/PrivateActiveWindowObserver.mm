// 
// Shijima-Qt - Cross-platform shimeji simulation app for desktop
// Copyright (C) 2025 pixelomer
// 

#import <Foundation/Foundation.h>
#import <AppKit/AppKit.h>
#import <ApplicationServices/ApplicationServices.h>
#include "PrivateActiveWindowObserver.hpp"

// Private API
extern "C" AXError _AXUIElementGetWindow(AXUIElementRef, CGWindowID *out);

static BOOL GetDataFromUIElement(AXUIElementRef element, CGRect *outRect,
    pid_t *outPid, CGWindowID *outWindowID)
{
    AXError err;
    BOOL success;
    if (outPid != NULL) {
        err = AXUIElementGetPid(element, outPid);
        if (err != kAXErrorSuccess) return NO;
    }
    if (outRect != NULL) {
        AXValueRef posVal = NULL;
        err = AXUIElementCopyAttributeValue(element, kAXPositionAttribute,
            (CFTypeRef *)&posVal);
        if (err != kAXErrorSuccess || posVal == NULL) return NO;
        success = AXValueGetValue(posVal, (AXValueType)kAXValueCGPointType,
            &outRect->origin);
        CFRelease(posVal);
        if (!success) return NO;

        AXValueRef sizeVal = NULL;
        err = AXUIElementCopyAttributeValue(element, kAXSizeAttribute,
            (CFTypeRef *)&sizeVal);
        if (err != kAXErrorSuccess || sizeVal == NULL) return NO;
        success = AXValueGetValue(sizeVal, (AXValueType)kAXValueCGSizeType,
            &outRect->size);
        CFRelease(sizeVal);
        if (!success) return NO;
    }
    if (outWindowID != NULL) {
        err = _AXUIElementGetWindow(element, outWindowID);
        if (err != kAXErrorSuccess) return NO;
    }
    return YES;
}

namespace Platform {

PrivateActiveWindowObserver::PrivateActiveWindowObserver() {
    AXIsProcessTrustedWithOptions((__bridge CFDictionaryRef)@{
        (__bridge NSString *)kAXTrustedCheckOptionPrompt: @YES
    });
}

ActiveWindow PrivateActiveWindowObserver::getActiveWindow() {
    @autoreleasepool {
        NSRunningApplication *frontmost = [[NSWorkspace sharedWorkspace] frontmostApplication];
        if (frontmost == nil) {
            return m_activeWindow = {};
        }
        if (frontmost.processIdentifier == getpid()) {
            // Use the application that was previously active
            if (m_activePid == -1) {
                return m_activeWindow = {};
            }
            frontmost = [NSRunningApplication
                runningApplicationWithProcessIdentifier:m_activePid];
            if (frontmost == nil) {
                return m_activeWindow = {};
            }
        }
        AXUIElementRef appRef = AXUIElementCreateApplication(frontmost.processIdentifier);
        if (appRef == NULL) {
            return m_activeWindow = {};
        }
        AXUIElementRef focusedWindowRef = NULL;
        AXError result = AXUIElementCopyAttributeValue(appRef, kAXFocusedWindowAttribute,
            (CFTypeRef*)&focusedWindowRef);
        CFRelease(appRef);

        if (result != kAXErrorSuccess || focusedWindowRef == NULL) {
            return m_activeWindow = {};
        }

        CGRect rect;
        pid_t pid;
        CGWindowID windowID;
        BOOL gotData = GetDataFromUIElement(focusedWindowRef, &rect, &pid, &windowID);
        CFRelease(focusedWindowRef);

        if (!gotData) {
            return m_activeWindow = {};
        }
        m_activePid = pid;
        QString uid = QString::fromStdString(std::to_string(pid) + 
            "-" + std::to_string(windowID));
        return m_activeWindow = { uid, (long)pid, rect.origin.x,
            rect.origin.y, rect.size.width, rect.size.height };
    }
}

}