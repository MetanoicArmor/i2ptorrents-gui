#import <AppKit/AppKit.h>
#import <QuartzCore/QuartzCore.h>
#include "macos_vibrancy.h"

static NSUserInterfaceItemIdentifier const kI2pVibrancyId = @"i2p.vibrancy";
static NSUserInterfaceItemIdentifier const kI2pMenuVibrancyId = @"i2p.menu.vibrancy";
static const CGFloat kI2pWindowRadius = 16.0;

static NSColor *i2p_srgb(unsigned int rgb, CGFloat alpha) {
    return [NSColor colorWithSRGBRed:((rgb >> 16) & 0xff) / 255.0
                               green:((rgb >> 8) & 0xff) / 255.0
                                blue:(rgb & 0xff) / 255.0
                               alpha:alpha];
}

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

void i2p_macos_nsview_clip_rounded(void *nsview, double radius, unsigned int border_rgb, int which,
                                   unsigned int fill_rgb) {
    if (nsview == nullptr) {
        return;
    }
    NSView *view = (__bridge NSView *)nsview;
    view.wantsLayer = YES;
    view.clipsToBounds = YES;
    view.layer.cornerRadius = radius;
    view.layer.masksToBounds = YES;
    if (fill_rgb != 0) {
        view.layer.opaque = YES;
        view.layer.backgroundColor = [i2p_srgb(fill_rgb, 1.0) CGColor];
    }
    if (border_rgb != 0) {
        view.layer.borderWidth = 1.0;
        view.layer.borderColor = [i2p_srgb(border_rgb, 1.0) CGColor];
    } else {
        view.layer.borderWidth = 0.0;
    }
    if (@available(macOS 10.13, *)) {
        CACornerMask corners = kCALayerMinXMinYCorner | kCALayerMaxXMinYCorner | kCALayerMinXMaxYCorner |
                               kCALayerMaxXMaxYCorner;
        if (which == 1) {
            corners = kCALayerMinXMaxYCorner | kCALayerMaxXMaxYCorner;
        } else if (which == 2) {
            corners = kCALayerMinXMinYCorner | kCALayerMaxXMinYCorner;
        }
        view.layer.maskedCorners = corners;
    }
    if (@available(macOS 10.15, *)) {
        view.layer.cornerCurve = kCACornerCurveContinuous;
    }
}

void i2p_macos_nsview_clip_control(void *nsview, double radius, unsigned int fill_rgb) {
    if (nsview == nullptr) {
        return;
    }
    NSView *view = (__bridge NSView *)nsview;
    view.wantsLayer = YES;
    view.clipsToBounds = YES;
    view.layer.masksToBounds = YES;
    view.layer.cornerRadius = radius;
    view.layer.borderWidth = 0.0;
    if (fill_rgb != 0) {
        view.layer.opaque = YES;
        view.layer.backgroundColor = [i2p_srgb(fill_rgb, 1.0) CGColor];
    }
    if (@available(macOS 10.15, *)) {
        view.layer.cornerCurve = kCACornerCurveContinuous;
    }
}

void i2p_macos_nsview_apply_opaque_dialog(void *nsview, int night) {
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
    window.opaque = YES;
    window.backgroundColor = night ? i2p_srgb(0x1c1c1e, 1.0) : i2p_srgb(0xffffff, 1.0);
    window.styleMask &= ~NSWindowStyleMaskFullSizeContentView;
    window.titlebarAppearsTransparent = YES;
    window.titleVisibility = NSWindowTitleVisible;
    if (@available(macOS 11.0, *)) {
        window.titlebarSeparatorStyle = NSTitlebarSeparatorStyleNone;
    }

    NSView *content = window.contentView ?: qtView;
    NSView *chrome = content.superview;
    if (chrome != nil) {
        for (NSView *sub in [chrome.subviews copy]) {
            if ([sub.identifier isEqual:kI2pVibrancyId]) {
                [sub removeFromSuperview];
            }
        }
    }
    for (NSView *sub in [content.subviews copy]) {
        if ([sub.identifier isEqual:kI2pVibrancyId]) {
            [sub removeFromSuperview];
        }
    }

    const CACornerMask window_corners = kCALayerMinXMinYCorner | kCALayerMinXMaxYCorner |
                                        kCALayerMaxXMinYCorner | kCALayerMaxXMaxYCorner;
    i2p_round_view(content, kI2pWindowRadius, window_corners);
}

void i2p_macos_nsview_apply_vibrancy(void *nsview, int night, int dialog) {
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
    if (dialog) {
        window.styleMask &= ~NSWindowStyleMaskFullSizeContentView;
        window.titlebarAppearsTransparent = YES;
        window.titleVisibility = NSWindowTitleVisible;
    } else {
        window.styleMask |= NSWindowStyleMaskFullSizeContentView;
        window.titlebarAppearsTransparent = YES;
        window.titleVisibility = NSWindowTitleHidden;
    }
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
        effect.material = dialog ? NSVisualEffectMaterialSheet
                                 : NSVisualEffectMaterialSidebar;
    } else {
        effect.material = NSVisualEffectMaterialAppearanceBased;
    }
    effect.frame = chrome.bounds;
    const CACornerMask window_corners = kCALayerMinXMinYCorner | kCALayerMinXMaxYCorner |
                                        kCALayerMaxXMinYCorner | kCALayerMaxXMaxYCorner;
    i2p_round_view(content, kI2pWindowRadius, window_corners);
    i2p_round_view(effect, kI2pWindowRadius, window_corners);
}

static void i2p_apply_rounded_window_mask(NSView *view, CGFloat radius) {
    if (view == nil || NSWidth(view.bounds) < 2.0 || NSHeight(view.bounds) < 2.0) {
        return;
    }
    view.wantsLayer = YES;
    view.clipsToBounds = YES;
    view.layer.opaque = NO;
    view.layer.backgroundColor = [NSColor clearColor].CGColor;
    view.layer.cornerRadius = radius;
    view.layer.masksToBounds = YES;
    view.layer.mask = nil;
    if (@available(macOS 10.15, *)) {
        view.layer.cornerCurve = kCACornerCurveContinuous;
    }
}

void i2p_macos_nsview_apply_menu_vibrancy(void *nsview, int night, double radius) {
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
    window.hasShadow = YES;

    NSView *content = window.contentView ?: qtView;
    NSView *host = content.superview ?: content;
    const CGFloat clip = radius > 0.0 ? (CGFloat)radius : 12.0;

    NSVisualEffectView *effect = nil;
    for (NSView *sub in host.subviews) {
        if ([sub.identifier isEqual:kI2pMenuVibrancyId] && [sub isKindOfClass:[NSVisualEffectView class]]) {
            effect = (NSVisualEffectView *)sub;
            break;
        }
    }
    if (effect == nil) {
        effect = [[NSVisualEffectView alloc] initWithFrame:host.bounds];
        effect.identifier = kI2pMenuVibrancyId;
        effect.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
        effect.blendingMode = NSVisualEffectBlendingModeBehindWindow;
        effect.state = NSVisualEffectStateActive;
        if (host == content) {
            [content addSubview:effect positioned:NSWindowBelow relativeTo:nil];
        } else {
            [host addSubview:effect positioned:NSWindowBelow relativeTo:content];
        }
    }

    effect.appearance = appearance;
    if (@available(macOS 10.14, *)) {
        effect.material = NSVisualEffectMaterialPopover;
    } else {
        effect.material = NSVisualEffectMaterialAppearanceBased;
    }
    effect.alphaValue = 1.0;
    effect.frame = host.bounds;

    i2p_apply_rounded_window_mask(effect, clip);
    for (NSView *view = qtView; view != nil; view = view.superview) {
        i2p_apply_rounded_window_mask(view, clip);
    }
    [window invalidateShadow];
}
