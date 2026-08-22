#import <AppKit/AppKit.h>
#import <QuartzCore/QuartzCore.h>
#include "macos_vibrancy.h"

static NSUserInterfaceItemIdentifier const kI2pVibrancyId = @"i2p.vibrancy";
static const CGFloat kI2pWindowRadius = 16.0;

static void i2p_round_view(NSView *view, CGFloat radius, CACornerMask corners) {
    if (view == nil) {
        return;
    }
    view.wantsLayer = YES;
    view.clipsToBounds = YES;
    view.layer.cornerRadius = radius;
    view.layer.masksToBounds = YES;
    if (@available(macOS 10.13, *)) {
        view.layer.maskedCorners = corners;
    }
    if (@available(macOS 10.15, *)) {
        view.layer.cornerCurve = kCACornerCurveContinuous;
    }
}

void i2p_macos_nsview_round_corners(void *nsview, int left_side) {
    (void)left_side;
    if (nsview == nullptr) {
        return;
    }
    NSView *view = (__bridge NSView *)nsview;
    const CACornerMask corners = kCALayerMinXMinYCorner | kCALayerMinXMaxYCorner |
                                 kCALayerMaxXMinYCorner | kCALayerMaxXMaxYCorner;
    i2p_round_view(view, kI2pWindowRadius, corners);
}

void i2p_macos_nsview_apply_vibrancy(void *nsview, int night) {
    if (nsview == nullptr) {
        return;
    }
    NSView *qtView = (__bridge NSView *)nsview;
    NSWindow *window = qtView.window;
    if (window == nil) {
        return;
    }

    NSAppearance *appearance =
        [NSAppearance appearanceNamed:(night ? NSAppearanceNameDarkAqua : NSAppearanceNameAqua)];
    window.appearance = appearance;
    window.opaque = NO;
    window.backgroundColor = NSColor.clearColor;
    window.styleMask |= NSWindowStyleMaskFullSizeContentView;
    window.titlebarAppearsTransparent = YES;
    window.titleVisibility = NSWindowTitleHidden;
    if (@available(macOS 11.0, *)) {
        window.titlebarSeparatorStyle = NSTitlebarSeparatorStyleNone;
    }

    NSView *content = window.contentView ?: qtView;
    NSView *chrome = content.superview;
    if (chrome == nil) {
        return;
    }

    for (NSView *sub in [content.subviews copy]) {
        if ([sub.identifier isEqual:kI2pVibrancyId]) {
            [sub removeFromSuperview];
        }
    }

    NSVisualEffectView *effect = nil;
    for (NSView *sub in chrome.subviews) {
        if ([sub.identifier isEqual:kI2pVibrancyId] && [sub isKindOfClass:[NSVisualEffectView class]]) {
            effect = (NSVisualEffectView *)sub;
            break;
        }
    }
    if (effect == nil) {
        effect = [[NSVisualEffectView alloc] initWithFrame:chrome.bounds];
        effect.identifier = kI2pVibrancyId;
        effect.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
        effect.blendingMode = NSVisualEffectBlendingModeBehindWindow;
        effect.state = NSVisualEffectStateFollowsWindowActiveState;
        [chrome addSubview:effect positioned:NSWindowBelow relativeTo:content];
    }

    effect.appearance = appearance;
    if (@available(macOS 10.14, *)) {
        effect.material = NSVisualEffectMaterialSidebar;
    } else {
        effect.material = NSVisualEffectMaterialAppearanceBased;
    }
    effect.frame = chrome.bounds;
    const CACornerMask window_corners = kCALayerMinXMinYCorner | kCALayerMinXMaxYCorner |
                                        kCALayerMaxXMinYCorner | kCALayerMaxXMaxYCorner;
    i2p_round_view(content, kI2pWindowRadius, window_corners);
    i2p_round_view(effect, kI2pWindowRadius, window_corners);
}
