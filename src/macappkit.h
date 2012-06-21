/* Definitions and headers for AppKit framework on the Mac OS.
   Copyright (C) 2008-2012  YAMAMOTO Mitsuharu

This file is part of GNU Emacs Mac port.

GNU Emacs Mac port is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

GNU Emacs Mac port is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GNU Emacs Mac port.  If not, see <http://www.gnu.org/licenses/>.  */

#undef Z
#import <Cocoa/Cocoa.h>
#import <AppKit/NSAccessibility.h> /* Mac OS X 10.2 needs this.  */
#ifdef USE_MAC_IMAGE_IO
#import <WebKit/WebKit.h>
#endif
#if MAC_OS_X_VERSION_MAX_ALLOWED >= 1050
#import <QuartzCore/QuartzCore.h>
#endif
#define Z (current_buffer->text->z)

#ifndef NSAppKitVersionNumber10_2
#define NSAppKitVersionNumber10_2 663
#endif
#ifndef NSAppKitVersionNumber10_3
#define NSAppKitVersionNumber10_3 743
#endif
#ifndef NSAppKitVersionNumber10_4
#define NSAppKitVersionNumber10_4 824
#endif
#ifndef NSAppKitVersionNumber10_5
#define NSAppKitVersionNumber10_5 949
#endif
#ifndef NSAppKitVersionNumber10_6
#define NSAppKitVersionNumber10_6 1038
#endif

#ifndef NSINTEGER_DEFINED
typedef int NSInteger;
typedef unsigned int NSUInteger;
#endif

#ifndef __has_feature
#define __has_feature(x) 0
#endif

#ifndef USE_ARC
#if defined (SYNC_INPUT) && defined (__clang__) && __has_feature (objc_arc)
#define USE_ARC 1
#endif
#endif

#if !USE_ARC
#define __unsafe_unretained
#endif

#if MAC_OS_X_VERSION_MAX_ALLOWED < 1060
/* If we add `<NSObject>' here as documented, the 64-bit binary
   compiled on Mac OS X 10.5 fails in startup at -[EmacsController
   methodSignatureForSelector:] when executed on Mac OS X 10.6.  */
@protocol NSApplicationDelegate @end
@protocol NSWindowDelegate @end
@protocol NSToolbarDelegate @end
@protocol NSMenuDelegate @end
@protocol NSUserInterfaceItemSearching @end
#endif

#if MAC_OS_X_VERSION_MAX_ALLOWED < 1050
@protocol NSTextInputClient @end
#endif

@interface NSData (Emacs)
- (Lisp_Object)lispString;
@end

@interface NSString (Emacs)
+ (id)stringWithLispString:(Lisp_Object)lispString;
+ (id)stringWithUTF8LispString:(Lisp_Object)lispString;
+ (id)stringWithUTF8String:(const char *)bytes fallback:(BOOL)flag;
- (Lisp_Object)lispString;
- (Lisp_Object)UTF8LispString;
- (Lisp_Object)UTF16LispString;
- (NSArray *)componentsSeparatedByCamelCasingWithCharactersInSet:(NSCharacterSet *)separator;
@end

@interface NSFont (Emacs)
+ (NSFont *)fontWithFace:(struct face *)face;
@end

@interface NSEvent (Emacs)
- (NSEvent *)mouseEventByChangingType:(NSEventType)type
		          andLocation:(NSPoint)location;
@end

@interface NSAttributedString (Emacs)
- (Lisp_Object)UTF16LispString;
@end

@interface NSImage (Emacs)
+ (id)imageWithCGImage:(CGImageRef)cgImage;
@end

@interface NSApplication (Emacs)
- (void)postDummyEvent;
#if MAC_OS_X_VERSION_MIN_REQUIRED >= 1060
- (void)runTemporarilyWithBlock:(void (^)(void))block;
#else
- (void)runTemporarilyWithInvocation:(NSInvocation *)invocation;
#endif
@end

@interface NSScreen (Emacs)
+ (NSScreen *)screenContainingPoint:(NSPoint)aPoint;
+ (NSScreen *)closestScreenForRect:(NSRect)aRect;
- (BOOL)containsDock;
@end

@interface EmacsApplication : NSApplication
@end

@interface EmacsPosingWindow : NSWindow
+ (void)setup;
@end

/* Class for delegate for NSApplication.  It also becomes the target
   of several actions such as those from EmacsView, menus, dialogs,
   and actions/services bound in the mac-apple-event keymap.  */

@interface EmacsController : NSObject <NSApplicationDelegate>
{
  /* Points to HOLD_QUIT arg passed to read_socket_hook.  */
  struct input_event *hold_quit;

  /* Number of events stored during a
     handleQueuedEventsWithHoldingQuitIn: call.  */
  int count;

  /* Whether to generate a HELP_EVENT at the end of handleOneNSEvent:
     call.  */
  int do_help;

  /* Non-zero means that a HELP_EVENT has been generated since Emacs
   start.  */
  int any_help_event_p;

  /* The frame on which a HELP_EVENT occurs.  */
  struct frame *emacsHelpFrame;

  /* The item selected in the popup menu.  */
  int menuItemSelection;

  /* Non-nil means left mouse tracking has been suspended by this
     object.  */
  id trackingObject;

  /* Selector used for resuming suspended left mouse tracking.  */
  SEL trackingResumeSelector;

  /* Whether a service provider for Emacs is registered as of
     applicationWillFinishLaunching: or not.  */
  BOOL serviceProviderRegistered;

  /* Whether the application should update its presentation options
     when it becomes active next time.  */
  BOOL needsUpdatePresentationOptionsOnBecomingActive;

  /* Whether conflicting Cocoa's text system key bindings (e.g., C-q)
     are disabled or not.  */
  BOOL conflictingKeyBindingsDisabled;

  /* Saved key bindings with or without conflicts (currently, those
     for writing direction commands on Mac OS X 10.6).  */
  NSDictionary *keyBindingsWithConflicts, *keyBindingsWithoutConflicts;

  /* Help topic that the user selected using Help menu search.  */
  id selectedHelpTopic;

  /* Search string for which the user selected "Show All Help
     Topics".  */
  NSString *searchStringForAllHelpTopics;

  /* Date of last flushWindow call.  */
  NSDate *lastFlushDate;

  /* Timer for deferring flushWindow call.  */
  NSTimer *flushTimer;

  /* Set of windows whose flush is deferred.  */
  NSMutableSet *deferredFlushWindows;
}
- (int)getAndClearMenuItemSelection;
- (void)storeInputEvent:(id)sender;
- (void)setMenuItemSelectionToTag:(id)sender;
- (void)storeEvent:(struct input_event *)bufp;
- (void)setTrackingObject:(id)object andResumeSelector:(SEL)selector;
- (int)handleQueuedNSEventsWithHoldingQuitIn:(struct input_event *)bufp;
- (void)cancelHelpEchoForEmacsFrame:(struct frame *)f;
- (BOOL)conflictingKeyBindingsDisabled;
- (void)setConflictingKeyBindingsDisabled:(BOOL)flag;
- (void)flushWindow:(NSWindow *)window force:(BOOL)flag;
- (void)updatePresentationOptions;
- (void)showMenuBar;
@end

/* Like NSWindow, but allows suspend/resume resize control tracking.  */

@interface EmacsWindow : NSWindow
{
  /* Left mouse up event used for suspending resize control
     tracking.  */
  NSEvent *mouseUpEvent;

  /* Pointer location of the left mouse down event that initiated the
     current resize control tracking session.  The value is in the
     base coordinate system of the window.  */
  NSPoint resizeTrackingStartLocation;

  /* Window size when the current resize control tracking session was
     started.  */
  NSSize resizeTrackingStartWindowSize;

  /* Event number of the current resize control tracking session.
     Don't compare this with the value of a drag event: the latter is
     is always 0 if the event comes via Screen Sharing.  */
  NSInteger resizeTrackingEventNumber;

  /* Whether the window should be made visible when the application
     gets unhidden next time.  */
  BOOL needsOrderFrontOnUnhide;

  /* Whether to suppress the usual -constrainFrameRect:toScreen:
     behavior.  */
  BOOL constrainingToScreenSuspended;
}
- (void)suspendResizeTracking:(NSEvent *)event
	   positionAdjustment:(NSPoint)adjustment;
- (void)resumeResizeTracking;
- (BOOL)needsOrderFrontOnUnhide;
- (void)setNeedsOrderFrontOnUnhide:(BOOL)flag;
- (void)setConstrainingToScreenSuspended:(BOOL)flag;
@end

@interface EmacsFullscreenWindow : EmacsWindow
@end

@interface NSObject (EmacsWindowDelegate)
- (BOOL)window:(NSWindow *)sender shouldForwardAction:(SEL)action to:(id)target;
- (NSRect)window:(NSWindow *)sender willConstrainFrame:(NSRect)frameRect
	toScreen:(NSScreen *)screen;
@end

@class EmacsView;
@class EmacsOverlayView;

/* Class for delegate of NSWindow and NSToolbar (see its Toolbar
   category declared later).  It also becomes that target of
   frame-dependent actions such as those from font panels.  */

@interface EmacsFrameController : NSObject <NSWindowDelegate>
{
  /* The Emacs frame corresponding to the NSWindow that
     EmacsFrameController object is associated with as delegate.  */
  struct frame *emacsFrame;

  /* The view for the Emacs frame.  */
  EmacsView *emacsView;

  /* Window and view overlaid on the Emacs frame window.  */
  NSWindow *overlayWindow;
  EmacsOverlayView *overlayView;

  /* The spinning progress indicator (corresponding to hourglass)
     shown at the upper-right corner of the window.  */
  NSProgressIndicator *hourglass;

  /* The current window manager state.  */
  WMState windowManagerState;

  /* The last window frame before maximize/fullscreen.  The position
     is relative to the top left corner of the screen.  */
  NSRect savedFrame;

#if MAC_OS_X_VERSION_MAX_ALLOWED >= 1050
  /* The view hosting Core Animation layers in the overlay window.  */
  NSView *layerHostingView;
#endif

#if MAC_OS_X_VERSION_MAX_ALLOWED >= 1060
  /* Window manager state after the full screen transition.  */
  WMState fullScreenTargetState;

  /* Pointer to the Lisp symbol that is set as `fullscreen' frame
     parameter after the full screen transition.  */
  Lisp_Object *fullscreenFrameParameterAfterTransition;

  /* Window used for the full screen transition.  */
  NSWindow *fullScreenTransitionWindow;
#endif
}
- (id)initWithEmacsFrame:(struct frame *)emacsFrame;
- (void)setupEmacsView;
- (void)setupWindow;
- (struct frame *)emacsFrame;
- (WMState)windowManagerState;
- (void)setWindowManagerState:(WMState)newState;
- (BOOL)emacsViewCanDraw;
- (void)lockFocusOnEmacsView;
- (void)unlockFocusOnEmacsView;
- (void)scrollEmacsViewRect:(NSRect)aRect by:(NSSize)offset;
- (NSPoint)convertEmacsViewPointToScreen:(NSPoint)point;
- (NSPoint)convertEmacsViewPointFromScreen:(NSPoint)point;
- (NSRect)convertEmacsViewRectToScreen:(NSRect)rect;
- (NSRect)centerScanEmacsViewRect:(NSRect)rect;
- (void)invalidateCursorRectsForEmacsView;
- (void)storeModifyFrameParametersEvent:(Lisp_Object)alist;
@end

/* Class for Emacs view that handles drawing events only.  It is used
   directly by tooltip frames, and indirectly by ordinary frames via
   inheritance.  */

@interface EmacsTipView : NSView
- (struct frame *)emacsFrame;
@end

/* Class for Emacs view that also handles input events.  Used by
   ordinary frames.  */

@interface EmacsView : EmacsTipView <NSTextInput, NSTextInputClient>
{
  /* Target object to which the EmacsView object sends actions.  */
  __unsafe_unretained id target;

  /* Message selector of the action the EmacsView object sends.  */
  SEL action;

  /* Stores the Emacs input event that the action method is expected
     to process.  */
  struct input_event inputEvent;

  /* Whether key events were interpreted by intepretKeyEvents:.  */
  BOOL keyEventsInterpreted;

  /* Raw key event that is interpreted by intepretKeyEvents:.  */
  NSEvent *rawKeyEvent;

  /* Saved marked text passed by setMarkedText:selectedRange:.  */
  id markedText;

  /* Position in the last normal (non-momentum) wheel event.  */
  NSPoint savedWheelPoint;

  /* Modifiers in the last normal (non-momentum) wheel event.  */
  int savedWheelModifiers;
}
- (id)target;
- (SEL)action;
- (void)setTarget:(id)anObject;
- (void)setAction:(SEL)aSelector;
- (BOOL)sendAction:(SEL)theAction to:(id)theTarget;
- (struct input_event *)inputEvent;
- (NSString *)string;
- (NSRect)firstRectForCharacterRange:(NSRange)aRange
			 actualRange:(NSRangePointer)actualRange;
- (void)viewFrameDidChange:(NSNotification *)notification;
@end

/* Class for view in the overlay window of an Emacs frame window.  */

@interface EmacsOverlayView : NSView
{
  /* Whether to highlight the area corresponding to the content of the
     Emacs frame window.  */
  BOOL highlighted;

  /* Whether to show the resize indicator.  */
  BOOL showsResizeIndicator;
}
- (void)setHighlighted:(BOOL)flag;
- (void)setShowsResizeIndicator:(BOOL)flag;
- (void)adjustWindowFrame;
@end

/* Class for scroller that doesn't do modal mouse tracking.  */

@interface NonmodalScroller : NSScroller
{
  /* Timer used for posting events periodically during mouse
     tracking.  */
  NSTimer *timer;

  /* Code for the scroller part that the user hit.  */
  NSScrollerPart hitPart;

  /* Whether the hitPart area should be highlighted.  */
  BOOL hilightsHitPart;

  /* If the scroller knob is currently being dragged by the user, this
     is the number of pixels from the top of the knob to the place
     where the user grabbed it.  If the knob is pressed but not
     dragged yet, this is a negative number whose absolute value is
     the number of pixels plus 1.  */
  CGFloat knobGrabOffset;

  /* The position of the top (for vertical scroller) or left (for
     horizontal, respectively) of the scroller knob in pixels,
     relative to the knob slot.  */
  CGFloat knobMinEdgeInSlot;
}
+ (void)updateBehavioralParameters;
- (BOOL)dragUpdatesFloatValue;
- (NSTimeInterval)buttonDelay;
- (NSTimeInterval)buttonPeriod;
- (BOOL)pagingBehavior;
@end

/* Just for avoiding warnings about undocumented methods in NSScroller.  */

@interface NSScroller (Undocumented)
- (void)drawArrow:(NSUInteger)position highlightPart:(NSInteger)part;
@end

/* Class for Scroller used for an Emacs window.  */

@interface EmacsScroller : NonmodalScroller
{
  /* Emacs scroll bar for the scroller.  */
  struct scroll_bar *emacsScrollBar;

  /* The size of the scroller knob track area in pixels.  */
  CGFloat knobSlotSpan;

  /* Minimum size of the scroller knob, in pixels.  */
  CGFloat minKnobSpan;

  /* The size of the whole scroller area in pixels.  */
  CGFloat frameSpan;

  /* The position the user clicked in pixels, relative to the whole
     scroller area.  */
  CGFloat clickPositionInFrame;

  /* For a scroller click with the control modifier, this becomes the
     value of the `code' member in struct input_event.  */
  int inputEventCode;

  /* For a scroller click with the control modifier, this becomes the
     value of the `modifiers' member in struct input_event.  */
  int inputEventModifiers;
}
- (void)setEmacsScrollBar:(struct scroll_bar *)bar;
- (struct scroll_bar *)emacsScrollBar;
- (CGFloat)knobSlotSpan;
- (CGFloat)minKnobSpan;
- (CGFloat)knobMinEdgeInSlot;
- (CGFloat)frameSpan;
- (CGFloat)clickPositionInFrame;
- (int)inputEventCode;
- (int)inputEventModifiers;
@end

@interface EmacsFrameController (ScrollBar)
- (void)addScrollerWithScrollBar:(struct scroll_bar *)bar;
@end

@interface EmacsToolbarItem : NSToolbarItem
{
  /* CoreGraphics image of the item.  */
  CGImageRef coreGraphicsImage;
}
- (void)setCoreGraphicsImage:(CGImageRef)cgImage;
@end

@interface EmacsFrameController (Toolbar) <NSToolbarDelegate>
- (NSToolbarItem *)toolbar:(NSToolbar *)toolbar
     itemForItemIdentifier:(NSString *)itemIdentifier
 willBeInsertedIntoToolbar:(BOOL)flag;
- (NSArray *)toolbarAllowedItemIdentifiers:(NSToolbar *)toolbar;
- (NSArray *)toolbarDefaultItemIdentifiers:(NSToolbar *)toolbar;
- (BOOL)validateToolbarItem:(NSToolbarItem *)theItem;
- (void)setupToolBarWithVisibility:(BOOL)visible;
- (void)updateToolbarDisplayMode;
- (void)storeToolBarEvent:(id)sender;
- (void)noteToolBarMouseMovement:(NSEvent *)event;
@end

/* Like NSFontPanel, but allows suspend/resume slider tracking.  */

@interface EmacsFontPanel : NSFontPanel
{
  /* Left mouse up event used for suspending slider tracking.  */
  NSEvent *mouseUpEvent;

  /* Slider being tracked.  */
  __unsafe_unretained NSSlider *trackedSlider;
}
- (void)suspendSliderTracking:(NSEvent *)event;
- (void)resumeSliderTracking;
@end

@interface EmacsController (FontPanel)
- (void)fontPanelWillClose:(NSNotification *)notification;
@end

@interface EmacsFrameController (FontPanel)
- (NSFont *)fontForFace:(int)faceId character:(int)c
	       position:(int)pos object:(Lisp_Object)object;
- (void)changeFont:(id)sender;
@end

@interface EmacsFrameController (EventHandling)
- (void)noteEnterEmacsView;
- (void)noteLeaveEmacsView;
- (int)noteMouseMovement:(NSPoint)point;
@end

@interface EmacsFrameController (Hourglass)
- (void)showHourglass:(id)sender;
- (void)hideHourglass:(id)sender;
@end

@interface EmacsSavePanel : NSSavePanel
@end

@interface EmacsOpenPanel : NSOpenPanel
@end

@interface EmacsFontDialogController : NSObject <NSWindowDelegate>
@end

@interface NSFontPanel (Emacs)
- (NSInteger)runModal;
@end

@interface NSMenu (Emacs)
- (NSMenuItem *)addItemWithWidgetValue:(widget_value *)wv;
- (void)fillWithWidgetValue:(widget_value *)first_wv;
@end

@interface EmacsMenu : NSMenu
@end

@interface NSEvent (Undocumented)
- (EventRef)_eventRef;
- (BOOL)_continuousScroll;
- (NSInteger)_scrollPhase;
- (CGFloat)deviceDeltaX;
- (CGFloat)deviceDeltaY;
- (CGFloat)deviceDeltaZ;
@end

@interface EmacsController (Menu) <NSMenuDelegate, NSUserInterfaceItemSearching>
- (void)trackMenuBar;
@end

@interface EmacsFrameController (Menu)
- (void)popUpMenu:(NSMenu *)menu atLocationInEmacsView:(NSPoint)location;
@end

@interface EmacsDialogView : NSView
- (id)initWithWidgetValue:(widget_value *)wv;
@end

@interface NSPasteboard (Emacs)
- (BOOL)setLispObject:(Lisp_Object)lispObject forType:(NSString *)dataType;
- (Lisp_Object)lispObjectForType:(NSString *)dataType;
@end

@interface NSAppleEventDescriptor (Emacs)
- (OSErr)copyDescTo:(AEDesc *)desc;
@end

@interface EmacsFrameController (DragAndDrop)
- (void)registerEmacsViewForDraggedTypes:(NSArray *)pboardTypes;
- (void)setOverlayViewHighlighted:(BOOL)flag;
@end

@interface EmacsController (AppleScript)
- (long)doAppleScript:(Lisp_Object)script result:(Lisp_Object *)result;
@end

#ifdef USE_MAC_IMAGE_IO
@interface NSView (Emacs)
- (XImagePtr)createXImageFromRect:(NSRect)rect backgroundColor:(NSColor *)color;
@end

/* Class for SVG frame load delegate.  */
@interface EmacsSVGLoader : NSObject
{
  /* Frame and image data structures to which the SVG image is
     loaded.  */
  struct frame *emacsFrame;
  struct image *emacsImage;

  /* Function called when checking image size.  */
  int (*checkImageSizeFunc) (struct frame *, int, int);

  /* Function called when reporting image load errors.  */
  void (*imageErrorFunc) (const char *, Lisp_Object, Lisp_Object);

  /* Whether a page load has completed.  */
  BOOL isLoaded;
}
- (id)initWithEmacsFrame:(struct frame *)f emacsImage:(struct image *)img
      checkImageSizeFunc:(int (*)(struct frame *, int, int))checkImageSize
	  imageErrorFunc:(void (*)(const char *, Lisp_Object, Lisp_Object))imageError;
- (int)loadData:(NSData *)data backgroundColor:(NSColor *)backgroundColor;
@end
#endif

@interface EmacsFrameController (Accessibility)
- (void)postAccessibilityNotificationsToEmacsView;
@end

#if MAC_OS_X_VERSION_MAX_ALLOWED >= 1050
@interface EmacsFrameController (Animation)
- (void)setupLayerHostingView;
- (CALayer *)layerForRect:(NSRect)rect;
- (void)addLayer:(CALayer *)layer;
- (CIFilter *)transitionFilterFromProperties:(Lisp_Object)properties;
- (void)adjustTransitionFilter:(CIFilter *)filter forLayer:(CALayer *)layer;
@end
#endif

#if MAC_OS_X_VERSION_MIN_REQUIRED < 1050

/* Class for locale objects used in kCTFontLanguagesAttribute
   emulation.  */

@interface EmacsLocale : NSObject
{
  /* Mac OS language and region codes for the locale.  */
  LangCode langCode;
  RegionCode regionCode;

#if MAC_OS_X_VERSION_MAX_ALLOWED >= 1040
  /* Exemplar character set for the locale.  */
  NSCharacterSet *exemplarCharacterSet;
#endif
}
- (id)initWithLocaleIdentifier:(NSString *)string;
- (BOOL)isCompatibleWithFont:(NSFont *)font;
@end

/* Class for CTFontDescriptor replacement for < 10.5 systems.  Some
   selectors are compatible with those for NSFontDescriptor, so
   toll-free bridged CTFontDescriptor can also respond to them.
   Implementations of some methods are dummy and each subclass
   (EmacsFDFontDescriptor or EmacsFMFontDescriptor below) should
   override them.  */

@interface EmacsFontDescriptor : NSObject
- (id)initWithFontAttributes:(NSDictionary *)attributes;
+ (id)fontDescriptorWithFontAttributes:(NSDictionary *)attributes;
+ (id)fontDescriptorWithFont:(NSFont *)font;
- (NSArray *)matchingFontDescriptorsWithMandatoryKeys:(NSSet *)mandatoryKeys;
- (NSArray *)matchingFontDescriptorsWithMandatoryKeys:(NSSet *)mandatoryKeys
					      locales:(NSArray *)locales;
- (EmacsFontDescriptor *)matchingFontDescriptorWithMandatoryKeys:(NSSet *)mandatoryKeys;
- (id)objectForKey:(NSString *)anAttribute;
@end

#if USE_NS_FONT_DESCRIPTOR
@interface EmacsFDFontDescriptor : EmacsFontDescriptor
{
  NSFontDescriptor *fontDescriptor;
}
- (id)initWithFontDescriptor:(NSFontDescriptor *)aFontDescriptor;
- (NSFontDescriptor *)fontDescriptor;
+ (id)fontDescriptorWithFontDescriptor:(NSFontDescriptor *)aFontDescriptor;
@end

#endif

#if USE_NS_FONT_MANAGER
@interface EmacsFMFontDescriptor : EmacsFontDescriptor
{
  NSMutableDictionary *fontAttributes;
}
@end

#endif

#endif	/* MAC_OS_X_VERSION_MIN_REQUIRED < 1050 */

/* Some methods that are not declared in older versions.  Should be
   used with some runtime check such as `respondsToSelector:'. */

#if MAC_OS_X_VERSION_MAX_ALLOWED < 1050
@interface NSNumber (AvailableOn1050AndLater)
+ (NSNumber *)numberWithInteger:(NSInteger)value;
@end
#endif

#if MAC_OS_X_VERSION_MAX_ALLOWED < 1060
@interface NSImage (AvailableOn1060AndLater)
- (id)initWithCGImage:(CGImageRef)cgImage size:(NSSize)size;
@end
#endif

#if MAC_OS_X_VERSION_MAX_ALLOWED < 1050
@interface NSBitmapImageRep (AvailableOn1050AndLater)
- (id)initWithCGImage:(CGImageRef)cgImage;
@end
#endif

#if MAC_OS_X_VERSION_MAX_ALLOWED < 1060
enum {
  NSApplicationPresentationDefault			= 0,
  NSApplicationPresentationAutoHideDock			= 1 << 0,
  NSApplicationPresentationHideDock			= 1 << 1,
  NSApplicationPresentationAutoHideMenuBar		= 1 << 2,
  NSApplicationPresentationHideMenuBar			= 1 << 3,
  NSApplicationPresentationDisableAppleMenu		= 1 << 4,
  NSApplicationPresentationDisableProcessSwitching	= 1 << 5,
  NSApplicationPresentationDisableForceQuit		= 1 << 6,
  NSApplicationPresentationDisableSessionTermination	= 1 << 7,
  NSApplicationPresentationDisableHideApplication	= 1 << 8,
  NSApplicationPresentationDisableMenuBarTransparency	= 1 << 9
};
typedef NSUInteger NSApplicationPresentationOptions;

@interface NSApplication (AvailableOn1060AndLater)
- (void)setPresentationOptions:(NSApplicationPresentationOptions)newOptions;
- (NSApplicationPresentationOptions)presentationOptions;
- (void)registerUserInterfaceItemSearchHandler:(id<NSUserInterfaceItemSearching>)handler;
- (BOOL)searchString:(NSString *)searchString inUserInterfaceItemString:(NSString *)stringToSearch
	 searchRange:(NSRange)searchRange foundRange:(NSRange *)foundRange;
@end
#endif

#if MAC_OS_X_VERSION_MAX_ALLOWED < 1070
enum {
  NSApplicationPresentationFullScreen			= 1 << 10,
  NSApplicationPresentationAutoHideToolbar		= 1 << 11
};
#endif

#if MAC_OS_X_VERSION_MAX_ALLOWED < 1040
@interface NSWindow (AvailableOn1040AndLater)
- (CGFloat)userSpaceScaleFactor;
@end
#endif

#if MAC_OS_X_VERSION_MAX_ALLOWED < 1050
enum {
  NSWindowCollectionBehaviorDefault		= 0,
  NSWindowCollectionBehaviorCanJoinAllSpaces	= 1 << 0,
  NSWindowCollectionBehaviorMoveToActiveSpace	= 1 << 1
};
typedef NSUInteger NSWindowCollectionBehavior;

@interface NSWindow (AvailableOn1050AndLater)
- (NSWindowCollectionBehavior)collectionBehavior;
- (void)setCollectionBehavior:(NSWindowCollectionBehavior)behavior;
@end
#endif

#if MAC_OS_X_VERSION_MAX_ALLOWED < 1060
typedef NSUInteger NSWindowNumberListOptions;

@interface NSWindow (AvailableOn1060AndLater)
- (void)setStyleMask:(NSUInteger)styleMask;
+ (NSArray *)windowNumbersWithOptions:(NSWindowNumberListOptions)options;
@end
#endif

#if MAC_OS_X_VERSION_MAX_ALLOWED < 1070
enum {
  NSWindowCollectionBehaviorFullScreenPrimary	= 1 << 7,
  NSWindowCollectionBehaviorFullScreenAuxiliary	= 1 << 8
};
#endif

#if MAC_OS_X_VERSION_MAX_ALLOWED < 1070
enum {
  NSWindowAnimationBehaviorDefault		= 0,
  NSWindowAnimationBehaviorNone			= 2,
  NSWindowAnimationBehaviorDocumentWindow	= 3,
  NSWindowAnimationBehaviorUtilityWindow	= 4,
  NSWindowAnimationBehaviorAlertPanel		= 5
};
typedef NSInteger NSWindowAnimationBehavior;

enum {
  NSFullScreenWindowMask = 1 << 14
};

@interface NSWindow (AvailableOn1070AndLater)
- (NSWindowAnimationBehavior)animationBehavior;
- (void)setAnimationBehavior:(NSWindowAnimationBehavior)newAnimationBehavior;
- (void)toggleFullScreen:(id)sender;
- (CGFloat)backingScaleFactor;
@end
#endif

#if MAC_OS_X_VERSION_MAX_ALLOWED < 1030
@interface NSSavePanel (AvailableOn1030AndLater)
- (void)setNameFieldLabel:(NSString *)label;
@end
#endif

#if MAC_OS_X_VERSION_MAX_ALLOWED < 1030
@interface NSMenu (AvailableOn1030AndLater)
- (void)setDelegate:(id)anObject;
@end
#endif

#if MAC_OS_X_VERSION_MAX_ALLOWED < 1060
@interface NSMenu (AvailableOn1060AndLater)
- (BOOL)popUpMenuPositioningItem:(NSMenuItem *)item
		      atLocation:(NSPoint)location inView:(NSView *)view;
@end
#endif

#if MAC_OS_X_VERSION_MAX_ALLOWED < 1040
@interface NSEvent (AvailableOn1040AndLater)
- (float)rotation;
@end
#endif

#if MAC_OS_X_VERSION_MAX_ALLOWED < 1060
enum {
    NSEventTypeGesture          = 29,
    NSEventTypeMagnify          = 30,
    NSEventTypeSwipe            = 31,
    NSEventTypeRotate           = 18
};

@interface NSEvent (AvailableOn1060AndLater)
- (CGFloat)magnification;
+ (NSUInteger)modifierFlags;
@end
#endif

#if MAC_OS_X_VERSION_MAX_ALLOWED < 1070
enum {
    NSEventPhaseNone        = 0,
    NSEventPhaseBegan       = 0x1 << 0,
    NSEventPhaseStationary  = 0x1 << 1,
    NSEventPhaseChanged     = 0x1 << 2,
    NSEventPhaseEnded       = 0x1 << 3,
    NSEventPhaseCancelled   = 0x1 << 4,
};
typedef NSUInteger NSEventPhase;

@interface NSEvent (AvailableOn1070AndLater)
- (BOOL)hasPreciseScrollingDeltas;
- (CGFloat)scrollingDeltaX;
- (CGFloat)scrollingDeltaY;
- (NSEventPhase)momentumPhase;
- (BOOL)isDirectionInvertedFromDevice;
- (NSEventPhase)phase;
@end
#endif

#if MAC_OS_X_VERSION_MAX_ALLOWED < 1030
@interface NSObject (AvailableOn1030AndLater)
- (id)accessibilityAttributeValue:(NSString *)attribute
		     forParameter:(id)parameter;
@end
#endif

#if MAC_OS_X_VERSION_MAX_ALLOWED < 1070
#if MAC_OS_X_VERSION_MAX_ALLOWED >= 1060
@interface NSAnimationContext (AvailableOn1070AndLater)
+ (void)runAnimationGroup:(void (^)(NSAnimationContext *context))changes
        completionHandler:(void (^)(void))completionHandler;
@end

@interface CALayer (AvailableOn1070AndLater)
@property CGFloat contentsScale;
@end
#endif
#endif
