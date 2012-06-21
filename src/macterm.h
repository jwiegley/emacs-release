/* Display module for Mac OS.
   Copyright (C) 2000-2008 Free Software Foundation, Inc.
   Copyright (C) 2009-2012  YAMAMOTO Mitsuharu

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

/* Originally contributed by Andrew Choi (akochoi@mac.com) for Emacs 21.  */

#include "macgui.h"
#include "frame.h"

#define RGB_TO_ULONG(r, g, b) (((r) << 16) | ((g) << 8) | (b))
#define ARGB_TO_ULONG(a, r, g, b) (((a) << 24) | ((r) << 16) | ((g) << 8) | (b))

#define ALPHA_FROM_ULONG(color) ((color) >> 24)
#define RED_FROM_ULONG(color) (((color) >> 16) & 0xff)
#define GREEN_FROM_ULONG(color) (((color) >> 8) & 0xff)
#define BLUE_FROM_ULONG(color) ((color) & 0xff)

/* Do not change `* 0x101' in the following lines to `<< 8'.  If
   changed, image masks in 1-bit depth will not work. */
#define RED16_FROM_ULONG(color) (RED_FROM_ULONG(color) * 0x101)
#define GREEN16_FROM_ULONG(color) (GREEN_FROM_ULONG(color) * 0x101)
#define BLUE16_FROM_ULONG(color) (BLUE_FROM_ULONG(color) * 0x101)

#define BLACK_PIX_DEFAULT(f) RGB_TO_ULONG(0,0,0)
#define WHITE_PIX_DEFAULT(f) RGB_TO_ULONG(255,255,255)

#define FONT_WIDTH(f)	((f)->max_width)
#define FONT_HEIGHT(f)	((f)->height)
#define FONT_BASE(f)    ((f)->ascent)
#define FONT_DESCENT(f) ((f)->descent)

/* Structure recording bitmaps and reference count.
   If REFCOUNT is 0 then this record is free to be reused.  */

struct mac_bitmap_record
{
  char *bitmap_data;
  char *file;
  int refcount;
  int height, width;
};


/* For each display (currently only one on mac), we have a structure that
   records information about it.  */

struct mac_display_info
{
  /* Chain of all mac_display_info structures.  */
  struct mac_display_info *next;

  /* The generic display parameters corresponding to this display. */
  struct terminal *terminal;

  /* This is a cons cell of the form (NAME . FONT-LIST-CACHE).
     The same cons cell also appears in x_display_name_list.  */
  Lisp_Object name_list_element;

  /* Number of frames that are on this display.  */
  int reference_count;

  /* Dots per inch of the screen.  */
  double resx, resy;

  /* Number of planes on this screen.  */
  int n_planes;

  /* Whether the screen supports color */
  int color_p;

  /* Dimensions of this screen.  */
  int height, width;

  /* Mask of things that cause the mouse to be grabbed.  */
  int grabbed;

#if 0
  /* Emacs bitmap-id of the default icon bitmap for this frame.
     Or -1 if none has been allocated yet.  */
  ptrdiff_t icon_bitmap_id;

#endif
  /* The root window of this screen.  */
  Window root_window;

  /* The cursor to use for vertical scroll bars.  */
  Cursor vertical_scroll_bar_cursor;

  /* Resource data base */
  XrmDatabase xrdb;

  /* Minimum width over all characters in all fonts in font_table.  */
  int smallest_char_width;

  /* Minimum font height over all fonts in font_table.  */
  int smallest_font_height;

  /* Reusable Graphics Context for drawing a cursor in a non-default face. */
  GC scratch_cursor_gc;

  /* Information about the range of text currently shown in
     mouse-face.  */
  Mouse_HLInfo mouse_highlight;

  char *mac_id_name;

  /* The number of fonts opened for this display.  */
  int n_fonts;

  /* Pointer to bitmap records.  */
  struct mac_bitmap_record *bitmaps;

  /* Allocated size of bitmaps field.  */
  ptrdiff_t bitmaps_size;

  /* Last used bitmap index.  */
  ptrdiff_t bitmaps_last;

  /* The frame (if any) which has the window that has keyboard focus.
     Zero if none.  This is examined by Ffocus_frame in macfns.c.  Note
     that a mere EnterNotify event can set this; if you need to know the
     last frame specified in a FocusIn or FocusOut event, use
     x_focus_event_frame.  */
  struct frame *x_focus_frame;

  /* The last frame mentioned in a FocusIn or FocusOut event.  This is
     separate from x_focus_frame, because whether or not LeaveNotify
     events cause us to lose focus depends on whether or not we have
     received a FocusIn event for it.  */
  struct frame *x_focus_event_frame;

  /* The frame which currently has the visual highlight, and should get
     keyboard input (other sorts of input have the frame encoded in the
     event).  It points to the focus frame's selected window's
     frame.  It differs from x_focus_frame when we're using a global
     minibuffer.  */
  struct frame *x_highlight_frame;

  /* This is a button event that wants to activate the menubar.
     We save it here until the command loop gets to think about it.  */
  EventRef saved_menu_event;
};

/* This checks to make sure we have a display.  */
extern void check_mac (void);

#define x_display_info mac_display_info

/* This is a chain of structures for all the X displays currently in use.  */
extern struct x_display_info *x_display_list;

/* This is a chain of structures for all the displays currently in use.  */
extern struct mac_display_info one_mac_display_info;

/* This is a list of cons cells, each of the form (NAME . FONT-LIST-CACHE),
   one for each element of x_display_list and in the same order.
   NAME is the name of the frame.
   FONT-LIST-CACHE records previous values returned by x-list-fonts.  */
extern Lisp_Object x_display_name_list;

extern void x_set_frame_alpha (struct frame *);

extern struct mac_display_info *mac_term_init (Lisp_Object, char *, char *);


struct font;

#if 0
/* When Emacs uses a tty window, tty_display in frame.c points to an
   x_output struct .  */
struct x_output
{
  unsigned long background_pixel;
  unsigned long foreground_pixel;
};
#endif

/* The collection of data describing a window on the Mac.  */
struct mac_output
{
#if 0
  /* Placeholder for things accessed through output_data.x.  Must
     appear first.  */
  struct x_output x_compatible;
#endif

  /* Menubar "widget" handle.  */
  int menubar_widget;

  /* Here are the Graphics Contexts for the default font.  */
  GC normal_gc;				/* Normal video */
  GC reverse_gc;			/* Reverse video */
  GC cursor_gc;				/* cursor drawing */

  /* The window used for this frame.
     May be zero while the frame object is being created
     and the window has not yet been created.  */
  Window window_desc;

  /* The window that is the parent of this window.
     Usually this is a window that was made by the window manager,
     but it can be the root window, and it can be explicitly specified
     (see the explicit_parent field, below).  */
  Window parent_desc;

  /* Default ASCII font of this frame. */
  struct font *font;

  /* The baseline offset of the default ASCII font.  */
  int baseline_offset;

  /* If a fontset is specified for this frame instead of font, this
     value contains an ID of the fontset, else -1.  */
  int fontset;

  /* Pixel values used for various purposes.
     border_pixel may be -1 meaning use a gray tile.  */
  unsigned long cursor_pixel;
  unsigned long border_pixel;
  unsigned long mouse_pixel;
  unsigned long cursor_foreground_pixel;

#if 0
  /* Foreground color for scroll bars.  A value of -1 means use the
     default (black for non-toolkit scroll bars).  */
  unsigned long scroll_bar_foreground_pixel;

  /* Background color for scroll bars.  A value of -1 means use the
     default (background color of the frame for non-toolkit scroll
     bars).  */
  unsigned long scroll_bar_background_pixel;
#endif

  /* Descriptor for the cursor in use for this window.  */
  Cursor text_cursor;
  Cursor nontext_cursor;
  Cursor modeline_cursor;
  Cursor hand_cursor;
  Cursor hourglass_cursor;
  Cursor horizontal_drag_cursor;
  Cursor current_cursor;
#if 0
  /* Window whose cursor is hourglass_cursor.  This window is temporarily
     mapped to display a hourglass-cursor.  */
  Window hourglass_window;

  /* Non-zero means hourglass cursor is currently displayed.  */
  unsigned hourglass_p : 1;

  /* Flag to set when the window needs to be completely repainted.  */
  int needs_exposure;

#endif

  /* This is the Emacs structure for the display this frame is on.  */
  /* struct w32_display_info *display_info; */

  /* Nonzero means our parent is another application's window
     and was explicitly specified.  */
  char explicit_parent;

  /* Nonzero means tried already to make this frame visible.  */
  char asked_for_visible;

  /* Nonzero if this frame is for tooltip.  */
  unsigned tooltip_p : 1;

  /* Nonzero if x_check_fullscreen is not called yet after fullscreen
     request for this frame.  */
  unsigned check_fullscreen_needed_p : 1;

  /* Nonzero if this frame uses a native tool bar (as opposed to a
     toolkit one).  */
  unsigned native_tool_bar_p : 1;

  /* Relief GCs, colors etc.  */
  struct relief
  {
    GC gc;
    unsigned long pixel;
    int allocated_p;
  }
  black_relief, white_relief;

  /* The background for which the above relief GCs were set up.
     They are changed only when a different background is involved.  */
  unsigned long relief_background;

  /* Width of the internal border.  */
  int internal_border_width;

  /* Hints for the size and the position of a window.  */
  XSizeHints *size_hints;

  /* This variable records the gravity value of the window position if
     the window has an external tool bar when it is created.  The
     position of the window is adjusted using this information when
     the tool bar is first redisplayed.  Once the tool bar is
     redisplayed, it is set to 0 in order to avoid further
     adjustment.  */
  int toolbar_win_gravity;

  /* Quartz 2D graphics context.  */
  CGContextRef cg_context;
};

/* Return the X output data for frame F.  */
#define FRAME_X_OUTPUT(f) ((f)->output_data.mac)

/* Return the Mac window used for displaying data in frame F.  */
#define FRAME_MAC_WINDOW(f) ((f)->output_data.mac->window_desc)
#define FRAME_X_WINDOW(f) ((f)->output_data.mac->window_desc)

#define FRAME_FONT(f) ((f)->output_data.mac->font)
#define FRAME_FONTSET(f) ((f)->output_data.mac->fontset)

#define FRAME_BASELINE_OFFSET(f) ((f)->output_data.mac->baseline_offset)

#define FRAME_SIZE_HINTS(f) ((f)->output_data.mac->size_hints)
#define FRAME_TOOLTIP_P(f) ((f)->output_data.mac->tooltip_p)
#define FRAME_CHECK_FULLSCREEN_NEEDED_P(f) \
  ((f)->output_data.mac->check_fullscreen_needed_p)
#define FRAME_NATIVE_TOOL_BAR_P(f) ((f)->output_data.mac->native_tool_bar_p)

/* This gives the mac_display_info structure for the display F is on.  */
#define FRAME_MAC_DISPLAY_INFO(f) (&one_mac_display_info)
#define FRAME_X_DISPLAY_INFO(f) (&one_mac_display_info)

/* This is the `Display *' which frame F is on.  */
#define FRAME_MAC_DISPLAY(f) (0)
#define FRAME_X_DISPLAY(f) (0)

/* This is the 'font_info *' which frame F has.  */
#define FRAME_MAC_FONT_TABLE(f) (FRAME_MAC_DISPLAY_INFO (f)->font_table)

/* Value is the smallest width of any character in any font on frame F.  */

#define FRAME_SMALLEST_CHAR_WIDTH(F) \
     FRAME_MAC_DISPLAY_INFO(F)->smallest_char_width

/* Value is the smallest height of any font on frame F.  */

#define FRAME_SMALLEST_FONT_HEIGHT(F) \
     FRAME_MAC_DISPLAY_INFO(F)->smallest_font_height

/* Mac-specific scroll bar stuff.  */

/* We represent scroll bars as lisp vectors.  This allows us to place
   references to them in windows without worrying about whether we'll
   end up with windows referring to dead scroll bars; the garbage
   collector will free it when its time comes.

   We use struct scroll_bar as a template for accessing fields of the
   vector.  */

struct scroll_bar {

  /* These fields are shared by all vectors.  */
  EMACS_INT size_from_Lisp_Vector_struct;
  struct Lisp_Vector *next_from_Lisp_Vector_struct;

  /* The window we're a scroll bar for.  */
  Lisp_Object window;

  /* The next and previous in the chain of scroll bars in this frame.  */
  Lisp_Object next, prev;

  /* Fields from `mac_control_ref' down will not be traced by the GC.  */

  /* The Mac control reference of this scroll bar.  */
  void *mac_control_ref;

  /* The position and size of the scroll bar in pixels, relative to the
     frame.  */
  int top, left, width, height;

  /* 1 if the background of the fringe that is adjacent to a scroll
     bar is extended to the gap between the fringe and the bar.  */
  unsigned int fringe_extended_p : 1;

  /* 1 if redraw needed in the next XTset_vertical_scroll_bar call.  */
  unsigned int redraw_needed_p : 1;
};

/* Turning a lisp vector value into a pointer to a struct scroll_bar.  */
#define XSCROLL_BAR(vec) ((struct scroll_bar *) XVECTOR (vec))


/* Extract the reference to the scroller control from a struct
   scroll_bar.  */
#define SCROLL_BAR_SCROLLER(ptr) ((__bridge EmacsScroller *) (ptr)->mac_control_ref)

/* Store the reference to a scroller control in a struct
   scroll_bar.  */
#define SET_SCROLL_BAR_SCROLLER(ptr, ref) ((ptr)->mac_control_ref = (__bridge void *) (ref))

/* Trimming off a few pixels from each side prevents
   text from glomming up against the scroll bar */
#define VERTICAL_SCROLL_BAR_WIDTH_TRIM (0)

/* Variations of possible Aqua scroll bar width.  */
#define MAC_AQUA_VERTICAL_SCROLL_BAR_WIDTH (15)
#define MAC_AQUA_SMALL_VERTICAL_SCROLL_BAR_WIDTH (11)

/* Size of hourglass controls */
#define HOURGLASS_WIDTH (18)
#define HOURGLASS_HEIGHT (18)
#define HOURGLASS_TOP_MARGIN (2)
#define HOURGLASS_RIGHT_MARGIN (32)

/* Some constants that are used locally.  */
/* Creator code for Emacs on Mac OS.  */
enum {
  MAC_EMACS_CREATOR_CODE	= 'EMAx'
};

/* Apple event descriptor types */
enum {
  TYPE_FILE_NAME		= 'fNam'
};

/* Keywords for Apple event attributes */
enum {
  KEY_EMACS_SUSPENSION_ID_ATTR	= 'esId' /* typeUInt32 */
};

/* Some constants that are not defined in older versions.  */
#if MAC_OS_X_VERSION_MAX_ALLOWED < 1030
/* Keywords for Apple event attributes */
enum {
  keyReplyRequestedAttr		= 'repq'
};
#endif

#if 0
/* We can't determine the availability of these enumerators by
   MAC_OS_X_VERSION_MAX_ALLOWED, because they are defined in
   MacOSX10.3.9.sdk for Mac OS X 10.4, but not in Mac OS X 10.3.  */
#if MAC_OS_X_VERSION_MAX_ALLOWED < 1040
/* Gestalt selectors */
enum {
  gestaltSystemVersionMajor	= 'sys1',
  gestaltSystemVersionMinor	= 'sys2',
  gestaltSystemVersionBugFix	= 'sys3'
};
#endif
#endif

/* kCGBitmapByteOrder32Host is defined in Universal SDK for 10.4 but
   not in PPC SDK for 10.4.0.  */
#if MAC_OS_X_VERSION_MAX_ALLOWED < 1050 && !defined (kCGBitmapByteOrder32Host)
#define kCGBitmapByteOrder32Host 0
#endif

#if MAC_OS_X_VERSION_MAX_ALLOWED < 1030
/* System UI mode constants */
enum {
  kUIModeAllSuppressed = 4
};

/* System UI options */
enum {
  kUIOptionDisableHide = 1 << 6
};
#endif

#if MAC_OS_X_VERSION_MAX_ALLOWED == 1060
BLOCK_EXPORT void _Block_object_assign (void *, const void *, const int) AVAILABLE_MAC_OS_X_VERSION_10_6_AND_LATER;
BLOCK_EXPORT void _Block_object_dispose (const void *, const int) AVAILABLE_MAC_OS_X_VERSION_10_6_AND_LATER;
BLOCK_EXPORT void * _NSConcreteGlobalBlock[32] AVAILABLE_MAC_OS_X_VERSION_10_6_AND_LATER;
BLOCK_EXPORT void * _NSConcreteStackBlock[32] AVAILABLE_MAC_OS_X_VERSION_10_6_AND_LATER;
#endif

struct frame;
struct face;
struct image;

struct frame *check_x_frame (Lisp_Object);
EXFUN (Fx_display_grayscale_p, 1);
EXFUN (Fx_display_planes, 1);
extern void x_free_gcs (struct frame *);
extern int XParseGeometry (char *, int *, int *, unsigned int *,
			   unsigned int *);

/* Defined in macterm.c.  */

extern void x_set_window_size (struct frame *, int, int, int);
extern void x_set_mouse_position (struct frame *, int, int);
extern void x_set_mouse_pixel_position (struct frame *, int, int);
extern void x_raise_frame (struct frame *);
extern void x_lower_frame (struct frame *);
extern void x_make_frame_visible (struct frame *);
extern void x_make_frame_invisible (struct frame *);
extern void x_iconify_frame (struct frame *);
extern void x_free_frame_resources (struct frame *);
extern void x_wm_set_size_hint (struct frame *, long, int);
extern void x_delete_terminal (struct terminal *terminal);
extern Pixmap mac_create_pixmap (Window, unsigned int, unsigned int,
				 unsigned int);
extern Pixmap mac_create_pixmap_from_bitmap_data (Window, char *,
						  unsigned int,
						  unsigned int,
						  unsigned long,
						  unsigned long,
						  unsigned int);
extern void mac_free_pixmap (Pixmap);
extern GC mac_create_gc (unsigned long, XGCValues *);
extern void mac_free_gc (GC);
extern void mac_set_foreground (GC, unsigned long);
extern void mac_set_background (GC, unsigned long);
extern void mac_draw_line_to_pixmap (Pixmap, GC, int, int, int, int);
extern void mac_clear_area (struct frame *, int, int,
			    unsigned int, unsigned int);
extern int mac_quit_char_key_p (UInt32, UInt32);
#define x_display_pixel_height(dpyinfo)	((dpyinfo)->height)
#define x_display_pixel_width(dpyinfo)	((dpyinfo)->width)
#define XCreatePixmap(display, w, width, height, depth) \
  mac_create_pixmap (w, width, height, depth)
#define XCreatePixmapFromBitmapData(display, w, data, width, height, fg, bg, depth) \
  mac_create_pixmap_from_bitmap_data (w, data, width, height, fg, bg, depth)
#define XFreePixmap(display, pixmap)	mac_free_pixmap (pixmap)
#define XChangeGC(display, gc, mask, xgcv)	mac_change_gc (gc, mask, xgcv)
#define XCreateGC(display, d, mask, xgcv)	mac_create_gc (mask, xgcv)
#define XFreeGC(display, gc)	mac_free_gc (gc)
#define XGetGCValues(display, gc, mask, xgcv)	\
  mac_get_gc_values (gc, mask, xgcv)
#define XSetForeground(display, gc, color)	mac_set_foreground (gc, color)
#define XSetBackground(display, gc, color)	mac_set_background (gc, color)
#define XDrawLine(display, p, gc, x1, y1, x2, y2)	\
  mac_draw_line_to_pixmap (p, gc, x1, y1, x2, y2)

#ifdef USE_MAC_IMAGE_IO
extern CGColorSpaceRef mac_cg_color_space_rgb;
#endif

extern void x_set_sticky (struct frame *, Lisp_Object, Lisp_Object);

/* Defined in macselect.c */

extern void x_clear_frame_selections (struct frame *);
EXFUN (Fx_selection_owner_p, 2);

/* Defined in macfns.c */

extern void x_real_positions (struct frame *, int *, int *);
extern void x_set_menu_bar_lines (struct frame *, Lisp_Object, Lisp_Object);
extern int x_pixel_width (struct frame *);
extern int x_pixel_height (struct frame *);
extern int x_char_width (struct frame *);
extern int x_char_height (struct frame *);
extern void x_sync (struct frame *);
extern int mac_defined_color (struct frame *, const char *, XColor *, int);
extern void x_set_tool_bar_lines (struct frame *, Lisp_Object, Lisp_Object);
extern void mac_update_title_bar (struct frame *, int);
extern Lisp_Object x_get_focus_frame (struct frame *);

/* Defined in mac.c.  */

extern Lisp_Object mac_aedesc_to_lisp (const AEDesc *);
extern OSErr mac_ae_put_lisp (AEDescList *, UInt32, Lisp_Object);
extern OSErr create_apple_event_from_lisp (Lisp_Object, AppleEvent *);
extern OSErr create_apple_event (AEEventClass, AEEventID, AppleEvent *);
extern Lisp_Object mac_event_parameters_to_lisp (EventRef, UInt32,
						 const EventParamName *,
						 const EventParamType *);
extern CFStringRef cfstring_create_with_utf8_cstring (const char *);
extern CFStringRef cfstring_create_with_string_noencode (Lisp_Object);
extern CFStringRef cfstring_create_with_string (Lisp_Object);
extern Lisp_Object cfdata_to_lisp (CFDataRef);
extern Lisp_Object cfstring_to_lisp_nodecode (CFStringRef);
extern Lisp_Object cfstring_to_lisp (CFStringRef);
extern Lisp_Object cfstring_to_lisp_utf_16 (CFStringRef);
extern Lisp_Object cfnumber_to_lisp (CFNumberRef);
extern Lisp_Object cfdate_to_lisp (CFDateRef);
extern Lisp_Object cfboolean_to_lisp (CFBooleanRef);
extern Lisp_Object cfobject_desc_to_lisp (CFTypeRef);
extern Lisp_Object cfobject_to_lisp (CFTypeRef, int, int);
extern Lisp_Object cfproperty_list_to_lisp (CFPropertyListRef, int, int);
extern CFPropertyListRef cfproperty_list_create_with_lisp (Lisp_Object);
extern Lisp_Object cfproperty_list_to_string (CFPropertyListRef,
					      CFPropertyListFormat);
extern CFPropertyListRef cfproperty_list_create_with_string (Lisp_Object);
extern int init_wakeup_fds (void);
extern void xrm_merge_string_database (XrmDatabase, const char *);
extern Lisp_Object xrm_get_resource (XrmDatabase, const char *,
				     const char *);
extern XrmDatabase xrm_get_preference_database (const char *);
EXFUN (Fmac_get_preference, 4);
extern int mac_service_provider_registered_p (void);

/* Defined in macmenu.c.  */

extern void x_activate_menubar (struct frame *);
extern void free_frame_menubar (struct frame *);

/* Defined in macappkit.m.  */

extern Lisp_Object mac_nsvalue_to_lisp (CFTypeRef);
extern void mac_alert_sound_play (void);
extern OSStatus install_application_handler (void);
extern int mac_close_display (void);
extern void mac_get_window_structure_bounds (struct frame *,
					     NativeRectangle *);
extern void mac_get_frame_mouse (struct frame *, Point *);
extern void mac_convert_frame_point_to_global (struct frame *, int *,
					       int *);
extern void mac_update_proxy_icon (struct frame *);
extern void mac_set_frame_window_background (struct frame *,
					     unsigned long);
extern void mac_update_begin (struct frame *);
extern void mac_update_end (struct frame *);
extern void mac_update_window_end (struct window *);
extern void mac_cursor_to (int, int, int, int);
extern void x_flush (struct frame *);
extern void mac_flush (struct frame *);
extern void mac_create_frame_window (struct frame *);
extern void mac_dispose_frame_window (struct frame *);
extern void mac_change_frame_window_wm_state (struct frame *, WMState,
					      WMState);
extern CGContextRef mac_begin_cg_clip (struct frame *, GC);
extern void mac_end_cg_clip (struct frame *);
extern void mac_create_scroll_bar (struct scroll_bar *);
extern void mac_dispose_scroll_bar (struct scroll_bar *);
extern void mac_update_scroll_bar_bounds (struct scroll_bar *);
extern void mac_redraw_scroll_bar (struct scroll_bar *);
extern void x_set_toolkit_scroll_bar_thumb (struct scroll_bar *,
					    int, int, int);
extern int mac_font_panel_visible_p (void);
extern OSStatus mac_show_hide_font_panel (void);
extern OSStatus mac_set_font_info_for_selection (struct frame *, int, int,
						     int, Lisp_Object);
extern EventTimeout mac_run_loop_run_once (EventTimeout);
extern void update_frame_tool_bar (FRAME_PTR f);
extern void free_frame_tool_bar (FRAME_PTR f);
extern void mac_show_hourglass (struct frame *);
extern void mac_hide_hourglass (struct frame *);
extern Lisp_Object mac_file_dialog (Lisp_Object, Lisp_Object, Lisp_Object,
				    Lisp_Object, Lisp_Object);
extern Lisp_Object mac_font_dialog (FRAME_PTR f);
extern int mac_activate_menubar (struct frame *);
extern void mac_fill_menubar (widget_value *, int);
extern int create_and_show_popup_menu (FRAME_PTR, widget_value *,
					   int, int, int);
extern int create_and_show_dialog (FRAME_PTR, widget_value *);
extern OSStatus mac_get_selection_from_symbol (Lisp_Object, int,
					       Selection *);
extern int mac_valid_selection_target_p (Lisp_Object);
extern OSStatus mac_clear_selection (Selection *);
extern Lisp_Object mac_get_selection_ownership_info (Selection);
extern int mac_valid_selection_value_p (Lisp_Object, Lisp_Object);
extern OSStatus mac_put_selection_value (Selection, Lisp_Object,
					     Lisp_Object);
extern int mac_selection_has_target_p (Selection, Lisp_Object);
extern Lisp_Object mac_get_selection_value (Selection, Lisp_Object);
extern Lisp_Object mac_get_selection_target_list (Selection);
extern Lisp_Object mac_dnd_default_known_types (void);

#if defined (__clang__) && MAC_OS_X_VERSION_MIN_REQUIRED >= 1060
#define MAC_USE_AUTORELEASE_LOOP 1
extern void mac_autorelease_loop (Lisp_Object (^) (void));
#else
extern void *mac_alloc_autorelease_pool (void);
extern void mac_release_autorelease_pool (void *);
#endif

extern int mac_tracking_area_works_with_cursor_rects_invalidation_p (void);
extern void mac_invalidate_frame_cursor_rects (struct frame *f);
#ifdef USE_MAC_IMAGE_IO
extern int mac_webkit_supports_svg_p (void);
#endif

#define CG_SET_FILL_COLOR(context, color)				\
  CGContextSetRGBFillColor (context,					\
			    RED_FROM_ULONG (color) / 255.0f,		\
			    GREEN_FROM_ULONG (color) / 255.0f,		\
			    BLUE_FROM_ULONG (color) / 255.0f, 1.0f)
#if MAC_OS_X_VERSION_MAX_ALLOWED >= 1030
#if MAC_OS_X_VERSION_MIN_REQUIRED == 1020
#define CG_SET_FILL_COLOR_MAYBE_WITH_CGCOLOR(context, color, cg_color) \
  do {								       \
    if (CGColorGetTypeID != NULL)				       \
      CGContextSetFillColorWithColor (context, cg_color);	       \
    else							       \
      CG_SET_FILL_COLOR (context, color);			       \
  } while (0)
#else
#define CG_SET_FILL_COLOR_MAYBE_WITH_CGCOLOR(context, color, cg_color)	\
  CGContextSetFillColorWithColor (context, cg_color)
#endif
#else
#define CG_SET_FILL_COLOR_MAYBE_WITH_CGCOLOR(context, color, cg_color)	\
  CG_SET_FILL_COLOR (context, color)
#endif
#define CG_SET_FILL_COLOR_WITH_GC_FOREGROUND(context, gc)		\
  CG_SET_FILL_COLOR_MAYBE_WITH_CGCOLOR (context, (gc)->xgcv.foreground,	\
					(gc)->cg_fore_color)
#define CG_SET_FILL_COLOR_WITH_GC_BACKGROUND(context, gc)		\
  CG_SET_FILL_COLOR_MAYBE_WITH_CGCOLOR (context, (gc)->xgcv.background,	\
					(gc)->cg_back_color)


#define CG_SET_STROKE_COLOR(context, color)				\
  CGContextSetRGBStrokeColor (context,					\
			      RED_FROM_ULONG (color) / 255.0f,		\
			      GREEN_FROM_ULONG (color) / 255.0f,	\
			      BLUE_FROM_ULONG (color) / 255.0f, 1.0f)
#if MAC_OS_X_VERSION_MAX_ALLOWED >= 1030
#if MAC_OS_X_VERSION_MIN_REQUIRED == 1020
#define CG_SET_STROKE_COLOR_MAYBE_WITH_CGCOLOR(context, color, cg_color) \
  do {								       \
    if (CGColorGetTypeID != NULL)				       \
      CGContextSetStrokeColorWithColor (context, cg_color);	       \
    else							       \
      CG_SET_STROKE_COLOR (context, color);			       \
  } while (0)
#else
#define CG_SET_STROKE_COLOR_MAYBE_WITH_CGCOLOR(context, color, cg_color) \
  CGContextSetStrokeColorWithColor (context, cg_color)
#endif
#else
#define CG_SET_STROKE_COLOR_MAYBE_WITH_CGCOLOR(context, color, cg_color) \
  CG_SET_STROKE_COLOR (context, color)
#endif
#define CG_SET_STROKE_COLOR_WITH_GC_FOREGROUND(context, gc) \
  CG_SET_STROKE_COLOR_MAYBE_WITH_CGCOLOR (context, (gc)->xgcv.foreground, \
					  (gc)->cg_fore_color)

/* Defined in macfont.c */
extern void macfont_update_antialias_threshold (void);
extern void *macfont_get_nsctfont (struct font *);
extern Lisp_Object macfont_nsctfont_to_spec (void *);

/* Defined in xdisp.c */
extern struct glyph *x_y_to_hpos_vpos (struct window *, int, int,
				       int *, int *, int *, int *, int *);
extern void frame_to_window_pixel_xy (struct window *, int *, int *);
extern void rows_from_pos_range (struct window *, EMACS_INT , EMACS_INT,
				 Lisp_Object, struct glyph_row **,
				 struct glyph_row **);
