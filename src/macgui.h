/* Definitions and headers for communication on the Mac OS.
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

#ifndef EMACS_MACGUI_H
#define EMACS_MACGUI_H

typedef struct _XDisplay Display; /* opaque */

typedef Lisp_Object XrmDatabase;

#undef Z
#undef DEBUG
#undef free
#undef malloc
#undef realloc
/* Macros max and min defined in lisp.h conflict with those in
   precompiled header Carbon.h.  */
#undef max
#undef min
#undef init_process
#define __ASSERT_MACROS_DEFINE_VERSIONS_WITHOUT_UNDERSCORES 0
#include <Carbon/Carbon.h>
#ifdef check /* __ASSERT_MACROS_DEFINE_VERSIONS_WITHOUT_UNDERSCORES is
		not in effect.  */
#undef check
#undef verify
#undef _GL_VERIFY_H
#include <verify.h>
#endif
#undef free
#define free unexec_free
#undef malloc
#define malloc unexec_malloc
#undef realloc
#define realloc unexec_realloc
#undef min
#define min(a, b) ((a) < (b) ? (a) : (b))
#undef max
#define max(a, b) ((a) > (b) ? (a) : (b))
#undef init_process
#define init_process emacs_init_process
#undef INFINITY
#undef Z
#define Z (current_buffer->text->z)

#ifndef CGFLOAT_DEFINED
typedef float CGFloat;
#define CGFLOAT_MIN FLT_MIN
#define CGFLOAT_MAX FLT_MAX
#endif

typedef void *Window;
typedef void *Selection;
extern void mac_set_frame_window_title (struct frame *, CFStringRef);
extern void mac_set_frame_window_modified (struct frame *, Boolean);
extern Boolean mac_is_frame_window_visible (struct frame *);
extern Boolean mac_is_frame_window_collapsed (struct frame *);
extern void mac_bring_frame_window_to_front (struct frame *);
extern void mac_send_frame_window_behind (struct frame *);
extern void mac_hide_frame_window (struct frame *);
extern void mac_show_frame_window (struct frame *);
extern OSStatus mac_collapse_frame_window (struct frame *, Boolean);
extern Boolean mac_is_frame_window_front (struct frame *);
extern void mac_activate_frame_window (struct frame *);
extern OSStatus mac_move_frame_window_structure (struct frame *,
						 short, short);
extern void mac_move_frame_window (struct frame *, short, short, Boolean);
extern void mac_size_frame_window (struct frame *, short, short, Boolean);
extern OSStatus mac_set_frame_window_alpha (struct frame *, CGFloat);
extern OSStatus mac_get_frame_window_alpha (struct frame *, CGFloat *);
extern void mac_get_global_mouse (Point *);
extern Boolean mac_is_frame_window_toolbar_visible (struct frame *);
extern CGRect mac_rect_make (struct frame *, CGFloat, CGFloat,
			     CGFloat, CGFloat);

#ifdef USE_MAC_IMAGE_IO
typedef struct _XImage
{
  int width, height;		/* size of image */
  char *data;			/* pointer to image data */
  int bytes_per_line;		/* accelarator to next line */
  int bits_per_pixel;		/* bits per pixel (ZPixmap) */
} *Pixmap;
#else
typedef GWorldPtr Pixmap;
#endif

#define Cursor ThemeCursor
#define No_Cursor (-1)

#define FACE_DEFAULT (~0)


/* Structure borrowed from Xlib.h to represent two-byte characters.  */

typedef struct {
  unsigned char byte1;
  unsigned char byte2;
} XChar2b;

#define STORE_XCHAR2B(chp, b1, b2) \
  ((chp)->byte1 = (b1), (chp)->byte2 = (b2))

#define XCHAR2B_BYTE1(chp) \
  ((chp)->byte1)

#define XCHAR2B_BYTE2(chp) \
  ((chp)->byte2)


/* Emulate X GC's by keeping color and font info in a structure.  */
typedef struct _XGCValues
{
  unsigned long foreground;
  unsigned long background;
} XGCValues;

typedef struct _XGC
{
  /* Original value.  */
  XGCValues xgcv;

  /* Cached data members follow.  */

#if MAC_OS_X_VERSION_MAX_ALLOWED >= 1030
  /* Quartz 2D foreground color.  */
  CGColorRef cg_fore_color;

  /* Quartz 2D background color.  */
  CGColorRef cg_back_color;
#endif

#define MAX_CLIP_RECTS 2
  /* Number of clipping rectangles.  */
  int n_clip_rects;

  /* Clipping rectangles used in Quartz 2D drawing.  The y-coordinate
     is in QuickDraw's.  */
  CGRect clip_rects[MAX_CLIP_RECTS];
} *GC;

#define GCForeground            (1L<<2)
#define GCBackground            (1L<<3)
#define GCGraphicsExposures	0

/* Bit Gravity */

#define ForgetGravity		0
#define NorthWestGravity	1
#define NorthGravity		2
#define NorthEastGravity	3
#define WestGravity		4
#define CenterGravity		5
#define EastGravity		6
#define SouthWestGravity	7
#define SouthGravity		8
#define SouthEastGravity	9
#define StaticGravity		10

#define NoValue		0x0000
#define XValue  	0x0001
#define YValue		0x0002
#define WidthValue  	0x0004
#define HeightValue  	0x0008
#define AllValues 	0x000F
#define XNegative 	0x0010
#define YNegative 	0x0020

typedef struct {
    	long flags;	/* marks which fields in this structure are defined */
#if 0
	int x, y;		/* obsolete for new window mgrs, but clients */
	int width, height;	/* should set so old wm's don't mess up */
#endif
	int min_width, min_height;
#if 0
	int max_width, max_height;
#endif
    	int width_inc, height_inc;
#if 0
	struct {
		int x;	/* numerator */
		int y;	/* denominator */
	} min_aspect, max_aspect;
#endif
	int base_width, base_height;		/* added by ICCCM version 1 */
#if 0
	int win_gravity;			/* added by ICCCM version 1 */
#endif
} XSizeHints;

#define USPosition	(1L << 0) /* user specified x, y */
#define USSize		(1L << 1) /* user specified width, height */

#define PPosition	(1L << 2) /* program specified position */
#define PSize		(1L << 3) /* program specified size */
#define PMinSize	(1L << 4) /* program specified minimum size */
#define PMaxSize	(1L << 5) /* program specified maximum size */
#define PResizeInc	(1L << 6) /* program specified resize increments */
#define PAspect		(1L << 7) /* program specified min and max aspect ratios */
#define PBaseSize	(1L << 8) /* program specified base for incrementing */
#define PWinGravity	(1L << 9) /* program specified window gravity */

/* Constants corresponding to window state hint atoms in X11 Extended
   Window Manager Hints (without "_NET_" prefix).  Mostly unimplemented.  */

enum
{
  WM_STATE_MODAL		= 1 << 0,
  WM_STATE_STICKY		= 1 << 1,
  WM_STATE_MAXIMIZED_VERT	= 1 << 2,
  WM_STATE_MAXIMIZED_HORZ	= 1 << 3,
  WM_STATE_SHADED		= 1 << 4,
  WM_STATE_SKIP_TASKBAR		= 1 << 5,
  WM_STATE_SKIP_PAGER		= 1 << 6,
  WM_STATE_HIDDEN		= 1 << 7,
  WM_STATE_FULLSCREEN		= 1 << 8,
  WM_STATE_ABOVE		= 1 << 9,
  WM_STATE_BELOW		= 1 << 10,
  WM_STATE_DEMANDS_ATTENTION	= 1 << 11
};

/* These are not derived from X11 EWMH window state hints, but used
   like them.  */
enum
{
  WM_STATE_NO_MENUBAR		= 1 << 12,
  WM_STATE_DEDICATED_DESKTOP	= 1 << 13
};

typedef uint32_t WMState;

typedef struct {
    int x, y;
    unsigned width, height;
} XRectangle;

#define NativeRectangle XRectangle

#define STORE_NATIVE_RECT(nr,rx,ry,rwidth,rheight)	\
  ((nr).x = (rx),					\
   (nr).y = (ry),					\
   (nr).width = (rwidth),				\
   (nr).height = (rheight))

enum {
  CFOBJECT_TO_LISP_WITH_TAG			= 1 << 0,
  CFOBJECT_TO_LISP_DONT_DECODE_STRING		= 1 << 1,
  CFOBJECT_TO_LISP_DONT_DECODE_DICTIONARY_KEY	= 1 << 2
};

/* Definitions copied from lwlib.h */

typedef void * XtPointer;

enum button_type
{
  BUTTON_TYPE_NONE,
  BUTTON_TYPE_TOGGLE,
  BUTTON_TYPE_RADIO
};

/* This structure is based on the one in ../lwlib/lwlib.h, modified
   for Mac OS.  */
typedef struct _widget_value
{
  /* name of widget */
  Lisp_Object   lname;
  const char*	name;
  /* value (meaning depend on widget type) */
  const char*	value;
  /* keyboard equivalent. no implications for XtTranslations */
  Lisp_Object   lkey;
  const char*	key;
  /* Help string or nil if none.
     GC finds this string through the frame's menu_bar_vector
     or through menu_items.  */
  Lisp_Object	help;
  /* true if enabled */
  Boolean	enabled;
  /* true if selected */
  Boolean	selected;
  /* The type of a button.  */
  enum button_type button_type;
  /* true if menu title */
  Boolean       title;
#if 0
  /* true if was edited (maintained by get_value) */
  Boolean	edited;
  /* true if has changed (maintained by lw library) */
  change_type	change;
  /* true if this widget itself has changed,
     but not counting the other widgets found in the `next' field.  */
  change_type   this_one_change;
#endif
  /* Contents of the sub-widgets, also selected slot for checkbox */
  struct _widget_value*	contents;
  /* data passed to callback */
  XtPointer	call_data;
  /* next one in the list */
  struct _widget_value*	next;
#if 0
  /* slot for the toolkit dependent part.  Always initialize to NULL. */
  void* toolkit_data;
  /* tell us if we should free the toolkit data slot when freeing the
     widget_value itself. */
  Boolean free_toolkit_data;

  /* we resource the widget_value structures; this points to the next
     one on the free list if this one has been deallocated.
   */
  struct _widget_value *free_list;
#endif
} widget_value;
/* Assumed by other routines to zero area returned.  */
#define malloc_widget_value() (void *)memset (xmalloc (sizeof (widget_value)),\
                                              0, (sizeof (widget_value)))
#define free_widget_value(wv) xfree (wv)

#define DIALOG_LEFT_MARGIN (112)
#define DIALOG_TOP_MARGIN (24)
#define DIALOG_RIGHT_MARGIN (24)
#define DIALOG_BOTTOM_MARGIN (20)
#define DIALOG_MIN_INNER_WIDTH (338)
#define DIALOG_MAX_INNER_WIDTH (564)
#define DIALOG_BUTTON_BUTTON_HORIZONTAL_SPACE (12)
#define DIALOG_BUTTON_BUTTON_VERTICAL_SPACE (12)
#define DIALOG_BUTTON_MIN_WIDTH (68)
#define DIALOG_TEXT_MIN_HEIGHT (50)
#define DIALOG_TEXT_BUTTONS_VERTICAL_SPACE (10)
#define DIALOG_ICON_WIDTH (64)
#define DIALOG_ICON_HEIGHT (64)
#define DIALOG_ICON_LEFT_MARGIN (24)
#define DIALOG_ICON_TOP_MARGIN (15)

#endif /* EMACS_MACGUI_H */
