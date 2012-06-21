/* Implementation of GUI terminal on the Mac OS.
   Copyright (C) 2000-2008  Free Software Foundation, Inc.
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

#include <config.h>
#include <signal.h>
#include <stdio.h>
#include <setjmp.h>

#include "lisp.h"
#include "blockinput.h"

#include "macterm.h"

#include "systime.h"

#include <ctype.h>
#include <errno.h>
#include <sys/stat.h>

#include "charset.h"
#include "character.h"
#include "coding.h"
#include "frame.h"
#include "dispextern.h"
#include "fontset.h"
#include "termhooks.h"
#include "termopts.h"
#include "termchar.h"
#include "disptab.h"
#include "buffer.h"
#include "window.h"
#include "keyboard.h"
#include "intervals.h"
#include "atimer.h"
#include "keymap.h"
#include "font.h"



/* This is a chain of structures for all the X displays currently in
   use.  */

struct x_display_info *x_display_list;

/* This is a list of cons cells, each of the form (NAME
   . FONT-LIST-CACHE), one for each element of x_display_list and in
   the same order.  NAME is the name of the frame.  FONT-LIST-CACHE
   records previous values returned by x-list-fonts.  */

Lisp_Object x_display_name_list;

/* This is a list of X Resource Database equivalents, each of which is
   implemented with a Lisp object.  They are stored in parallel with
   x_display_name_list.  */

static Lisp_Object x_display_rdb_list;

/* This is display since Mac does not support multiple ones.  */
struct mac_display_info one_mac_display_info;

/* Frame being updated by update_frame.  This is declared in term.c.
   This is set by update_begin and looked at by all the XT functions.
   It is zero while not inside an update.  In that case, the XT
   functions assume that `selected_frame' is the frame to apply to.  */

extern struct frame *updating_frame;

/* This is a frame waiting to be auto-raised, within XTread_socket.  */

struct frame *pending_autoraise_frame;

/* Mouse movement.

   Formerly, we used PointerMotionHintMask (in standard_event_mask)
   so that we would have to call XQueryPointer after each MotionNotify
   event to ask for another such event.  However, this made mouse tracking
   slow, and there was a bug that made it eventually stop.

   Simply asking for MotionNotify all the time seems to work better.

   In order to avoid asking for motion events and then throwing most
   of them away or busy-polling the server for mouse positions, we ask
   the server for pointer motion hints.  This means that we get only
   one event per group of mouse movements.  "Groups" are delimited by
   other kinds of events (focus changes and button clicks, for
   example), or by XQueryPointer calls; when one of these happens, we
   get another MotionNotify event the next time the mouse moves.  This
   is at least as efficient as getting motion events when mouse
   tracking is on, and I suspect only negligibly worse when tracking
   is off.  */

/* Where the mouse was last time we reported a mouse event.  */

NativeRectangle last_mouse_glyph;
FRAME_PTR last_mouse_glyph_frame;

/* The scroll bar in which the last X motion event occurred.

   If the last X motion event occurred in a scroll bar, we set this so
   XTmouse_position can know whether to report a scroll bar motion or
   an ordinary motion.

   If the last X motion event didn't occur in a scroll bar, we set
   this to Qnil, to tell XTmouse_position to return an ordinary motion
   event.  */

/* This is a hack.  We would really prefer that XTmouse_position would
   return the time associated with the position it returns, but there
   doesn't seem to be any way to wrest the time-stamp from the server
   along with the position query.  So, we just keep track of the time
   of the last movement we received, and return that in hopes that
   it's somewhat accurate.  */

Time last_mouse_movement_time;

/* Incremented by XTread_socket whenever it really tries to read
   events.  */

#ifdef __STDC__
int volatile input_signal_count;
#else
int input_signal_count;
#endif

/* The keysyms to use for the various modifiers.  */

static Lisp_Object Qalt, Qhyper, Qsuper, Qcontrol, Qmeta, Qmodifier_value;

extern int inhibit_window_system;

static void x_update_window_end (struct window *, int, int);
static struct terminal *mac_create_terminal (struct mac_display_info *dpyinfo);
static void x_update_end (struct frame *);
static void XTframe_up_to_date (struct frame *);
static void XTset_terminal_modes (struct terminal *);
static void XTreset_terminal_modes (struct terminal *);
static void x_clear_frame (struct frame *);
static void x_ins_del_lines (struct frame *, int, int) NO_RETURN;
static void frame_highlight (struct frame *);
static void frame_unhighlight (struct frame *);
static void x_new_focus_frame (struct x_display_info *, struct frame *);
static void XTframe_rehighlight (struct frame *);
static void x_frame_rehighlight (struct x_display_info *);
static void x_draw_hollow_cursor (struct window *, struct glyph_row *);
static void x_draw_bar_cursor (struct window *, struct glyph_row *, int,
                               enum text_cursor_kinds);

static void x_clip_to_row (struct window *, struct glyph_row *, int, GC);
static void x_update_begin (struct frame *);
static void x_update_window_begin (struct window *);
static void x_after_update_window_line (struct glyph_row *);
static void x_check_fullscreen (struct frame *);
static void mac_initialize (void);

#ifndef USE_MAC_IMAGE_IO
static RGBColor *
GC_FORE_COLOR (GC gc)
{
  static RGBColor color;

  color.red = RED16_FROM_ULONG (gc->xgcv.foreground);
  color.green = GREEN16_FROM_ULONG (gc->xgcv.foreground);
  color.blue = BLUE16_FROM_ULONG (gc->xgcv.foreground);

  return &color;
}

static RGBColor *
GC_BACK_COLOR (GC gc)
{
  static RGBColor color;

  color.red = RED16_FROM_ULONG (gc->xgcv.background);
  color.green = GREEN16_FROM_ULONG (gc->xgcv.background);
  color.blue = BLUE16_FROM_ULONG (gc->xgcv.background);

  return &color;
}
#endif
#define GC_FONT(gc)		((gc)->xgcv.font)
#define FRAME_NORMAL_GC(f)	((f)->output_data.mac->normal_gc)

/* Fringe bitmaps.  */

static int max_fringe_bmp = 0;
static CGImageRef *fringe_bmp = 0;

CGColorSpaceRef mac_cg_color_space_rgb;
#if MAC_OS_X_VERSION_MAX_ALLOWED >= 1030
static CGColorRef mac_cg_color_black;
#endif

static void
init_cg_color (void)
{
  mac_cg_color_space_rgb = CGColorSpaceCreateDeviceRGB ();
#if MAC_OS_X_VERSION_MAX_ALLOWED >= 1030
#if MAC_OS_X_VERSION_MIN_REQUIRED == 1020
  /* Don't check the availability of CGColorCreate; this symbol is
     defined even in Mac OS X 10.1.  */
  if (CGColorGetTypeID != NULL)
#endif
    {
      CGFloat rgba[] = {0.0f, 0.0f, 0.0f, 1.0f};

      mac_cg_color_black = CGColorCreate (mac_cg_color_space_rgb, rgba);
    }
#endif
}

/* X display function emulation */

/* Mac version of XDrawLine (to Pixmap).  */

void
mac_draw_line_to_pixmap (Pixmap p, GC gc, int x1, int y1, int x2, int y2)
{
#ifdef USE_MAC_IMAGE_IO
  CGContextRef context;
  XImagePtr ximg = p;
  CGColorSpaceRef color_space;
  CGImageAlphaInfo alpha_info;
  CGFloat gx1 = x1, gy1 = y1, gx2 = x2, gy2 = y2;

  if (y1 != y2)
    gx1 += 0.5f, gx2 += 0.5f;
  if (x1 != x2)
    gy1 += 0.5f, gy2 += 0.5f;

  if (ximg->bits_per_pixel == 32)
    {
      color_space = mac_cg_color_space_rgb;
      alpha_info = kCGImageAlphaNoneSkipFirst | kCGBitmapByteOrder32Host;
    }
  else
    {
      color_space = NULL;
      alpha_info = kCGImageAlphaOnly;
    }
  context = CGBitmapContextCreate (ximg->data, ximg->width, ximg->height, 8,
				   ximg->bytes_per_line, color_space,
				   alpha_info);
  if (ximg->bits_per_pixel == 32)
    CG_SET_STROKE_COLOR_WITH_GC_FOREGROUND (context, gc);
  else
    CGContextSetGrayStrokeColor (context, gc->xgcv.foreground / 255.0f, 1.0);
  CGContextMoveToPoint (context, gx1, gy1);
  CGContextAddLineToPoint (context, gx2, gy2);
  CGContextClosePath (context);
  CGContextStrokePath (context);
  CGContextRelease (context);
#else
  CGrafPtr old_port;
  GDHandle old_gdh;

  if (x1 == x2)
    {
      if (y1 > y2)
	y1--;
      else if (y2 > y1)
	y2--;
    }
  else if (y1 == y2)
    {
      if (x1 > x2)
	x1--;
      else
	x2--;
    }

  GetGWorld (&old_port, &old_gdh);
  SetGWorld (p, NULL);

  RGBForeColor (GC_FORE_COLOR (gc));

  LockPixels (GetGWorldPixMap (p));
  MoveTo (x1, y1);
  LineTo (x2, y2);
  UnlockPixels (GetGWorldPixMap (p));

  SetGWorld (old_port, old_gdh);
#endif
}


static void
mac_erase_rectangle (struct frame *f, GC gc, int x, int y,
		     unsigned int width, unsigned int height)
{
  CGContextRef context;

  context = mac_begin_cg_clip (f, gc);
  CG_SET_FILL_COLOR_WITH_GC_BACKGROUND (context, gc);
  CGContextFillRect (context, mac_rect_make (f, x, y, width, height));
  mac_end_cg_clip (f);
}


/* Mac version of XClearArea.  */

void
mac_clear_area (struct frame *f, int x, int y,
		unsigned int width, unsigned int height)
{
  mac_erase_rectangle (f, FRAME_NORMAL_GC (f), x, y, width, height);
}

/* Mac version of XClearWindow.  */

static void
mac_clear_window (struct frame *f)
{
  CGContextRef context;
  GC gc = FRAME_NORMAL_GC (f);

  context = mac_begin_cg_clip (f, NULL);
  CG_SET_FILL_COLOR_WITH_GC_BACKGROUND (context, gc);
  CGContextFillRect (context, CGRectMake (0, 0, FRAME_PIXEL_WIDTH (f),
					  FRAME_PIXEL_HEIGHT (f)));
  mac_end_cg_clip (f);
}


/* Mac replacement for XCopyArea.  */

static void
mac_draw_cg_image (CGImageRef image, struct frame *f, GC gc,
		   int src_x, int src_y,
		   unsigned int width, unsigned int height,
		   int dest_x, int dest_y, int overlay_p)
{
  CGContextRef context;
  CGRect dest_rect, bounds;

  context = mac_begin_cg_clip (f, gc);
  dest_rect = mac_rect_make (f, dest_x, dest_y, width, height);
  bounds = mac_rect_make (f, dest_x - src_x, dest_y - src_y,
			  CGImageGetWidth (image), CGImageGetHeight (image));
  if (!overlay_p)
    {
      CG_SET_FILL_COLOR_WITH_GC_BACKGROUND (context, gc);
      CGContextFillRect (context, dest_rect);
    }
  CGContextClipToRect (context, dest_rect);
  CGContextTranslateCTM (context,
			 CGRectGetMinX (bounds), CGRectGetMaxY (bounds));
  CGContextScaleCTM (context, 1, -1);
  if (CGImageIsMask (image))
    CG_SET_FILL_COLOR_WITH_GC_FOREGROUND (context, gc);
  bounds.origin = CGPointZero;
  CGContextDrawImage (context, bounds, image);
  mac_end_cg_clip (f);
}


/* Mac replacement for XCreateBitmapFromBitmapData.  */

static void
mac_create_bitmap_from_bitmap_data (BitMap *bitmap, char *bits, int w, int h)
{
  static const unsigned char swap_nibble[16]
    = { 0x0, 0x8, 0x4, 0xc,    /* 0000 1000 0100 1100 */
	0x2, 0xa, 0x6, 0xe,    /* 0010 1010 0110 1110 */
	0x1, 0x9, 0x5, 0xd,    /* 0001 1001 0101 1101 */
	0x3, 0xb, 0x7, 0xf };  /* 0011 1011 0111 1111 */
  int i, j, w1;
  char *p;

  w1 = (w + 7) / 8;         /* nb of 8bits elt in X bitmap */
  bitmap->rowBytes = ((w + 15) / 16) * 2; /* nb of 16bits elt in Mac bitmap */
  bitmap->baseAddr = xmalloc (bitmap->rowBytes * h);
  memset (bitmap->baseAddr, 0, bitmap->rowBytes * h);
  for (i = 0; i < h; i++)
    {
      p = bitmap->baseAddr + i * bitmap->rowBytes;
      for (j = 0; j < w1; j++)
	{
	  /* Bitswap XBM bytes to match how Mac does things.  */
	  unsigned char c = *bits++;
	  *p++ = (unsigned char)((swap_nibble[c & 0xf] << 4)
				 | (swap_nibble[(c>>4) & 0xf]));
	}
    }

  bitmap->bounds.left = bitmap->bounds.top = 0;
  bitmap->bounds.right = w;
  bitmap->bounds.bottom = h;
}


static void
mac_free_bitmap (BitMap *bitmap)
{
  xfree (bitmap->baseAddr);
}


Pixmap
mac_create_pixmap (Window w, unsigned int width, unsigned int height,
		   unsigned int depth)
{
#ifdef USE_MAC_IMAGE_IO
  XImagePtr ximg;

  ximg = xmalloc (sizeof (*ximg));
  ximg->width = width;
  ximg->height = height;
  ximg->bits_per_pixel = depth == 1 ? 8 : 32;
  ximg->bytes_per_line = width * (ximg->bits_per_pixel / 8);
  ximg->data = xmalloc (ximg->bytes_per_line * height);
  return ximg;
#else
  Pixmap pixmap;
  Rect r;
  QDErr err;

  SetRect (&r, 0, 0, width, height);
#ifndef WORDS_BIG_ENDIAN
  if (depth == 1)
#endif
    err = NewGWorld (&pixmap, depth, &r, NULL, NULL, 0);
#ifndef WORDS_BIG_ENDIAN
  else
    /* CreateCGImageFromPixMaps requires ARGB format.  */
    err = QTNewGWorld (&pixmap, k32ARGBPixelFormat, &r, NULL, NULL, 0);
#endif
  if (err != noErr)
    return NULL;
  return pixmap;
#endif
}


Pixmap
mac_create_pixmap_from_bitmap_data (Window w, char *data,
				    unsigned int width, unsigned int height,
				    unsigned long fg, unsigned long bg,
				    unsigned int depth)
{
  Pixmap pixmap;
  BitMap bitmap;
#ifdef USE_MAC_IMAGE_IO
  CGDataProviderRef provider;
  CGImageRef image_mask;
  CGContextRef context;

  pixmap = XCreatePixmap (display, w, width, height, depth);
  if (pixmap == NULL)
    return NULL;

  mac_create_bitmap_from_bitmap_data (&bitmap, data, width, height);
  provider = CGDataProviderCreateWithData (NULL, bitmap.baseAddr,
					   bitmap.rowBytes * height, NULL);
  image_mask = CGImageMaskCreate (width, height, 1, 1, bitmap.rowBytes,
				  provider, NULL, 0);
  CGDataProviderRelease (provider);

  context = CGBitmapContextCreate (pixmap->data, width, height, 8,
				   pixmap->bytes_per_line,
				   mac_cg_color_space_rgb,
				   kCGImageAlphaNoneSkipFirst
				   | kCGBitmapByteOrder32Host);

  CG_SET_FILL_COLOR (context, fg);
  CGContextFillRect (context, CGRectMake (0, 0, width, height));
  CG_SET_FILL_COLOR (context, bg);
  CGContextDrawImage (context, CGRectMake (0, 0, width, height), image_mask);
  CGContextRelease (context);
  CGImageRelease (image_mask);
#else
  CGrafPtr old_port;
  GDHandle old_gdh;
  static GC gc = NULL;

  if (gc == NULL)
    gc = XCreateGC (display, w, 0, NULL);

  pixmap = XCreatePixmap (display, w, width, height, depth);
  if (pixmap == NULL)
    return NULL;

  GetGWorld (&old_port, &old_gdh);
  SetGWorld (pixmap, NULL);
  mac_create_bitmap_from_bitmap_data (&bitmap, data, width, height);
  XSetForeground (display, gc, fg);
  XSetBackground (display, gc, bg);
  RGBForeColor (GC_FORE_COLOR (gc));
  RGBBackColor (GC_BACK_COLOR (gc));
  LockPixels (GetGWorldPixMap (pixmap));
  CopyBits (&bitmap, GetPortBitMapForCopyBits (pixmap),
	    &bitmap.bounds, &bitmap.bounds, srcCopy, 0);
  UnlockPixels (GetGWorldPixMap (pixmap));
  SetGWorld (old_port, old_gdh);
#endif
  mac_free_bitmap (&bitmap);

  return pixmap;
}


void
mac_free_pixmap (Pixmap pixmap)
{
#ifdef USE_MAC_IMAGE_IO
  if (pixmap)
    {
      xfree (pixmap->data);
      xfree (pixmap);
    }
#else
  DisposeGWorld (pixmap);
#endif
}


/* Mac replacement for XFillRectangle.  */

static void
mac_fill_rectangle (struct frame *f, GC gc, int x, int y,
		    unsigned int width, unsigned int height)
{
  CGContextRef context;

  context = mac_begin_cg_clip (f, gc);
  CG_SET_FILL_COLOR_WITH_GC_FOREGROUND (context, gc);
  CGContextFillRect (context, mac_rect_make (f, x, y, width, height));
  mac_end_cg_clip (f);
}


/* Mac replacement for XDrawRectangle: dest is a window.  */

static void
mac_draw_rectangle (struct frame *f, GC gc, int x, int y,
		    unsigned int width, unsigned int height)
{
  CGContextRef context;
  CGRect rect;

  context = mac_begin_cg_clip (f, gc);
  CG_SET_STROKE_COLOR_WITH_GC_FOREGROUND (context, gc);
  rect = mac_rect_make (f, x, y, width + 1, height + 1);
  CGContextStrokeRect (context, CGRectInset (rect, 0.5f, 0.5f));
  mac_end_cg_clip (f);
}


static void
mac_fill_trapezoid_for_relief (struct frame *f, GC gc, int x, int y,
			       unsigned int width, unsigned int height,
			       int top_p)
{
  CGContextRef context;
  CGRect rect = mac_rect_make (f, x, y, width, height);
  CGPoint points[4];

  points[0].x = points[1].x = CGRectGetMinX (rect);
  points[2].x = points[3].x = CGRectGetMaxX (rect);
  points[0].y = points[3].y = CGRectGetMinY (rect);
  points[1].y = points[2].y = CGRectGetMaxY (rect);

  if (top_p)
    points[2].x -= CGRectGetHeight (rect);
  else
    points[0].x += CGRectGetHeight (rect);

  context = mac_begin_cg_clip (f, gc);
  CG_SET_FILL_COLOR_WITH_GC_FOREGROUND (context, gc);
  CGContextBeginPath (context);
  CGContextAddLines (context, points, 4);
  CGContextClosePath (context);
  CGContextFillPath (context);
  mac_end_cg_clip (f);
}

enum corners
  {
    CORNER_TOP_LEFT,		/* 00 */
    CORNER_TOP_RIGHT,		/* 01 */
    CORNER_BOTTOM_LEFT,		/* 10 */
    CORNER_BOTTOM_RIGHT,	/* 11 */
    CORNER_LAST
  };

static void
mac_erase_corners_for_relief (struct frame *f, GC gc, int x, int y,
			      unsigned int width, unsigned int height,
			      int radius, int corners)
{
  CGContextRef context;
  int i;

  context = mac_begin_cg_clip (f, gc);
  CG_SET_FILL_COLOR_WITH_GC_BACKGROUND (context, gc);
  CGContextBeginPath (context);
  for (i = 0; i < CORNER_LAST; i++)
    if (corners & (1 << i))
      {
	CGFloat x1, y1, x2, y2;

	x1 = x + (i % 2) * width;
	y1 = y + (i / 2) * height;
	x2 = x1 + (1 - 2 * (i % 2)) * radius;
	y2 = y1 + (1 - 2 * (i / 2)) * radius;

	CGContextMoveToPoint (context, x1, y2);
	CGContextAddArcToPoint (context, x1, y1, x2, y1, radius);
	CGContextAddLineToPoint (context, x1, y1);
      }
  CGContextClosePath (context);
  CGContextFillPath (context);
  mac_end_cg_clip (f);
}


static void
mac_invert_rectangle (struct frame *f, int x, int y,
		      unsigned int width, unsigned int height)
{
#if MAC_OS_X_VERSION_MAX_ALLOWED >= 1040
#if MAC_OS_X_VERSION_MIN_REQUIRED < 1040 && MAC_OS_X_VERSION_MIN_REQUIRED >= 1020
  if (CGContextSetBlendMode != NULL)
#endif
    {
      CGContextRef context;

      context = mac_begin_cg_clip (f, NULL);
      CGContextSetRGBFillColor (context, 1.0f, 1.0f, 1.0f, 1.0f);
      CGContextSetBlendMode (context, kCGBlendModeDifference);
      CGContextFillRect (context, mac_rect_make (f, x, y, width, height));
      mac_end_cg_clip (f);
    }
#if MAC_OS_X_VERSION_MIN_REQUIRED < 1040 && MAC_OS_X_VERSION_MIN_REQUIRED >= 1020
  else				/* CGContextSetBlendMode == NULL */
#endif
#endif	/* MAC_OS_X_VERSION_MAX_ALLOWED >= 1040 */
#if MAC_OS_X_VERSION_MAX_ALLOWED < 1040 || (MAC_OS_X_VERSION_MIN_REQUIRED < 1040 && MAC_OS_X_VERSION_MIN_REQUIRED >= 1020)
    {
      extern void mac_appkit_invert_rectangle (struct frame *, int, int,
					       unsigned int, unsigned int);

      mac_appkit_invert_rectangle (f, x, y, width, height);
    }
#endif
}

/* Invert rectangles RECTANGLES[0], ..., RECTANGLES[N-1] in the frame F,
   excluding scroll bar area.  */

static void
mac_invert_rectangles (struct frame *f, NativeRectangle *rectangles, int n)
{
  int i;

  for (i = 0; i < n; i++)
    mac_invert_rectangle (f, rectangles[i].x, rectangles[i].y,
			  rectangles[i].width, rectangles[i].height);
  if (FRAME_HAS_VERTICAL_SCROLL_BARS (f))
    {
      Lisp_Object bar;

      for (bar = FRAME_SCROLL_BARS (f); !NILP (bar);
	   bar = XSCROLL_BAR (bar)->next)
	{
	  struct scroll_bar *b = XSCROLL_BAR (bar);
	  NativeRectangle bar_rect, r;

	  STORE_NATIVE_RECT (bar_rect, b->left, b->top, b->width, b->height);
	  for (i = 0; i < n; i++)
	    if (x_intersect_rectangles (rectangles + i, &bar_rect, &r))
	      mac_invert_rectangle (f, r.x, r.y, r.width, r.height);
	}
    }
}


/* Mac replacement for XCopyArea: used only for scrolling.  */

/* Defined in macappkit.m.  */
extern void mac_scroll_area (struct frame *, GC, int, int,
			     unsigned int, unsigned int, int, int);


/* Mac replacement for XChangeGC.  */

static void
mac_change_gc (GC gc, unsigned long mask, XGCValues *xgcv)
{
  if (mask & GCForeground)
    XSetForeground (display, gc, xgcv->foreground);
  if (mask & GCBackground)
    XSetBackground (display, gc, xgcv->background);
}


/* Mac replacement for XCreateGC.  */

GC
mac_create_gc (unsigned long mask, XGCValues *xgcv)
{
  GC gc = xmalloc (sizeof (*gc));

  memset (gc, 0, sizeof (*gc));
#if MAC_OS_X_VERSION_MAX_ALLOWED >= 1030
#if MAC_OS_X_VERSION_MIN_REQUIRED == 1020
  if (CGColorGetTypeID != NULL)
#endif
    {
      gc->cg_fore_color = gc->cg_back_color = mac_cg_color_black;
      CGColorRetain (gc->cg_fore_color);
      CGColorRetain (gc->cg_back_color);
    }
#endif
  XChangeGC (display, gc, mask, xgcv);

  return gc;
}


/* Used in xfaces.c.  */

void
mac_free_gc (GC gc)
{
#if MAC_OS_X_VERSION_MAX_ALLOWED >= 1030
#if MAC_OS_X_VERSION_MIN_REQUIRED == 1020
  if (CGColorGetTypeID != NULL)
#endif
    {
      CGColorRelease (gc->cg_fore_color);
      CGColorRelease (gc->cg_back_color);
    }
#endif
  xfree (gc);
}


/* Mac replacement for XGetGCValues.  */

static void
mac_get_gc_values (GC gc, unsigned long mask, XGCValues *xgcv)
{
  if (mask & GCForeground)
    xgcv->foreground = gc->xgcv.foreground;
  if (mask & GCBackground)
    xgcv->background = gc->xgcv.background;
}


/* Mac replacement for XSetForeground.  */

void
mac_set_foreground (GC gc, unsigned long color)
{
  if (gc->xgcv.foreground != color)
    {
      gc->xgcv.foreground = color;
#if MAC_OS_X_VERSION_MAX_ALLOWED >= 1030
#if MAC_OS_X_VERSION_MIN_REQUIRED == 1020
      if (CGColorGetTypeID != NULL)
#endif
	{
	  CGColorRelease (gc->cg_fore_color);
	  if (color == 0)
	    {
	      gc->cg_fore_color = mac_cg_color_black;
	      CGColorRetain (gc->cg_fore_color);
	    }
	  else
	    {
	      CGFloat rgba[4];

	      rgba[0] = RED_FROM_ULONG (color) / 255.0f;
	      rgba[1] = GREEN_FROM_ULONG (color) / 255.0f;
	      rgba[2] = BLUE_FROM_ULONG (color) / 255.0f;
	      rgba[3] = 1.0f;
	      gc->cg_fore_color = CGColorCreate (mac_cg_color_space_rgb, rgba);
	    }
	}
#endif
    }
}


/* Mac replacement for XSetBackground.  */

void
mac_set_background (GC gc, unsigned long color)
{
  if (gc->xgcv.background != color)
    {
      gc->xgcv.background = color;
#if MAC_OS_X_VERSION_MAX_ALLOWED >= 1030
#if MAC_OS_X_VERSION_MIN_REQUIRED == 1020
      if (CGColorGetTypeID != NULL)
#endif
	{
	  CGColorRelease (gc->cg_back_color);
	  if (color == 0)
	    {
	      gc->cg_back_color = mac_cg_color_black;
	      CGColorRetain (gc->cg_back_color);
	    }
	  else
	    {
	      CGFloat rgba[4];

	      rgba[0] = RED_FROM_ULONG (color) / 255.0f;
	      rgba[1] = GREEN_FROM_ULONG (color) / 255.0f;
	      rgba[2] = BLUE_FROM_ULONG (color) / 255.0f;
	      rgba[3] = 1.0f;
	      gc->cg_back_color = CGColorCreate (mac_cg_color_space_rgb, rgba);
	    }
	}
#endif
    }
}


/* Mac replacement for XSetClipRectangles.  */

static void
mac_set_clip_rectangles (struct frame *f, GC gc,
			 NativeRectangle *rectangles, int n)
{
  int i;

  xassert (n >= 0 && n <= MAX_CLIP_RECTS);

  gc->n_clip_rects = n;
  for (i = 0; i < n; i++)
    {
      NativeRectangle *rect = rectangles + i;

      gc->clip_rects[i] = mac_rect_make (f, rect->x, rect->y,
					 rect->width, rect->height);
    }
}


/* Mac replacement for XSetClipMask.  */

static inline void
mac_reset_clip_rectangles (struct frame *f, GC gc)
{
  gc->n_clip_rects = 0;
}

/* Remove calls to XFlush by defining XFlush to an empty replacement.
   Calls to XFlush should be unnecessary because the X output buffer
   is flushed automatically as needed by calls to XPending,
   XNextEvent, or XWindowEvent according to the XFlush man page.
   XTread_socket calls XPending.  Removing XFlush improves
   performance.  */

#define XFlush(DISPLAY)	(void) 0


void
x_set_frame_alpha (struct frame *f)
{
  struct x_display_info *dpyinfo = FRAME_MAC_DISPLAY_INFO (f);
  double alpha = 1.0;
  double alpha_min = 1.0;

  if (dpyinfo->x_highlight_frame == f)
    alpha = f->alpha[0];
  else
    alpha = f->alpha[1];

  if (FLOATP (Vframe_alpha_lower_limit))
    alpha_min = XFLOAT_DATA (Vframe_alpha_lower_limit);
  else if (INTEGERP (Vframe_alpha_lower_limit))
    alpha_min = (XINT (Vframe_alpha_lower_limit)) / 100.0;

  if (alpha < 0.0)
    return;
  else if (alpha > 1.0)
    alpha = 1.0;
  else if (0.0 <= alpha && alpha < alpha_min && alpha_min <= 1.0)
    alpha = alpha_min;

  mac_set_frame_window_alpha (f, alpha);
}


/***********************************************************************
		    Starting and ending an update
 ***********************************************************************/

/* Start an update of frame F.  This function is installed as a hook
   for update_begin, i.e. it is called when update_begin is called.
   This function is called prior to calls to x_update_window_begin for
   each window being updated.  */

static void
x_update_begin (struct frame *f)
{
  BLOCK_INPUT;
  mac_update_begin (f);
  UNBLOCK_INPUT;
}


/* Start update of window W.  Set the global variable updated_window
   to the window being updated and set output_cursor to the cursor
   position of W.  */

static void
x_update_window_begin (struct window *w)
{
  struct frame *f = XFRAME (WINDOW_FRAME (w));
  Mouse_HLInfo *hlinfo = MOUSE_HL_INFO (f);

  updated_window = w;
  set_output_cursor (&w->cursor);

  BLOCK_INPUT;

  if (f == hlinfo->mouse_face_mouse_frame)
    {
      /* Don't do highlighting for mouse motion during the update.  */
      hlinfo->mouse_face_defer = 1;

      /* If F needs to be redrawn, simply forget about any prior mouse
	 highlighting.  */
      if (FRAME_GARBAGED_P (f))
	hlinfo->mouse_face_window = Qnil;
    }

  UNBLOCK_INPUT;
}


/* Draw a vertical window border from (x,y0) to (x,y1)  */

static void
mac_draw_vertical_window_border (struct window *w, int x, int y0, int y1)
{
  struct frame *f = XFRAME (WINDOW_FRAME (w));
  struct face *face;

  face = FACE_FROM_ID (f, VERTICAL_BORDER_FACE_ID);
  if (face)
    XSetForeground (FRAME_MAC_DISPLAY (f), f->output_data.mac->normal_gc,
		    face->foreground);

  mac_fill_rectangle (f, f->output_data.mac->normal_gc, x, y0, 1, y1 - y0);
}

/* End update of window W (which is equal to updated_window).

   Draw vertical borders between horizontally adjacent windows, and
   display W's cursor if CURSOR_ON_P is non-zero.

   MOUSE_FACE_OVERWRITTEN_P non-zero means that some row containing
   glyphs in mouse-face were overwritten.  In that case we have to
   make sure that the mouse-highlight is properly redrawn.

   W may be a menu bar pseudo-window in case we don't have X toolkit
   support.  Such windows don't have a cursor, so don't display it
   here.  */

static void
x_update_window_end (struct window *w, int cursor_on_p, int mouse_face_overwritten_p)
{
  Mouse_HLInfo *hlinfo = MOUSE_HL_INFO (XFRAME (w->frame));

  if (!w->pseudo_window_p)
    {
      BLOCK_INPUT;

      if (cursor_on_p)
	display_and_set_cursor (w, 1, output_cursor.hpos,
				output_cursor.vpos,
				output_cursor.x, output_cursor.y);

      if (draw_window_fringes (w, 1))
	x_draw_vertical_border (w);

      mac_update_window_end (w);

      UNBLOCK_INPUT;
    }

  /* If a row with mouse-face was overwritten, arrange for
     XTframe_up_to_date to redisplay the mouse highlight.  */
  if (mouse_face_overwritten_p)
    {
      hlinfo->mouse_face_beg_row = hlinfo->mouse_face_beg_col = -1;
      hlinfo->mouse_face_end_row = hlinfo->mouse_face_end_col = -1;
      hlinfo->mouse_face_window = Qnil;
    }

  updated_window = NULL;
}


/* End update of frame F.  This function is installed as a hook in
   update_end.  */

static void
x_update_end (struct frame *f)
{
  /* Mouse highlight may be displayed again.  */
  MOUSE_HL_INFO (f)->mouse_face_defer = 0;

  BLOCK_INPUT;
  mac_update_end (f);
  XFlush (FRAME_MAC_DISPLAY (f));
  UNBLOCK_INPUT;
}


/* This function is called from various places in xdisp.c whenever a
   complete update has been performed.  The global variable
   updated_window is not available here.  */

static void
XTframe_up_to_date (struct frame *f)
{
  if (FRAME_MAC_P (f))
    {
      Mouse_HLInfo *hlinfo = MOUSE_HL_INFO (f);

      if (hlinfo->mouse_face_deferred_gc
	  || f == hlinfo->mouse_face_mouse_frame)
	{
	  BLOCK_INPUT;
	  if (hlinfo->mouse_face_mouse_frame)
	    note_mouse_highlight (hlinfo->mouse_face_mouse_frame,
				  hlinfo->mouse_face_mouse_x,
				  hlinfo->mouse_face_mouse_y);
	  hlinfo->mouse_face_deferred_gc = 0;
	  UNBLOCK_INPUT;
	}
    }
}


/* Draw truncation mark bitmaps, continuation mark bitmaps, overlay
   arrow bitmaps, or clear the fringes if no bitmaps are required
   before DESIRED_ROW is made current.  The window being updated is
   found in updated_window.  This function is called from
   update_window_line only if it is known that there are differences
   between bitmaps to be drawn between current row and DESIRED_ROW.  */

static void
x_after_update_window_line (struct glyph_row *desired_row)
{
  struct window *w = updated_window;
  struct frame *f;
  int width, height;

  xassert (w);

  if (!desired_row->mode_line_p && !w->pseudo_window_p)
    desired_row->redraw_fringe_bitmaps_p = 1;

  /* When a window has disappeared, make sure that no rest of
     full-width rows stays visible in the internal border.  Could
     check here if updated_window is the leftmost/rightmost window,
     but I guess it's not worth doing since vertically split windows
     are almost never used, internal border is rarely set, and the
     overhead is very small.  */
  if (windows_or_buffers_changed
      && desired_row->full_width_p
      && (f = XFRAME (w->frame),
	  width = FRAME_INTERNAL_BORDER_WIDTH (f),
	  width != 0)
      && (height = desired_row->visible_height,
	  height > 0))
    {
      int y = WINDOW_TO_FRAME_PIXEL_Y (w, max (0, desired_row->y));

      BLOCK_INPUT;
      mac_clear_area (f, 0, y, width, height);
      mac_clear_area (f, FRAME_PIXEL_WIDTH (f) - width, y, width, height);
      UNBLOCK_INPUT;
    }
}

static void
x_draw_fringe_bitmap (struct window *w, struct glyph_row *row, struct draw_fringe_bitmap_params *p)
{
  struct frame *f = XFRAME (WINDOW_FRAME (w));
  Display *display = FRAME_MAC_DISPLAY (f);
  struct face *face = p->face;
  int overlay_p = p->overlay_p;

  if (!overlay_p)
    {
      int bx = p->bx, nx = p->nx;

#if 0  /* MAC_TODO: stipple */
      /* In case the same realized face is used for fringes and
	 for something displayed in the text (e.g. face `region' on
	 mono-displays, the fill style may have been changed to
	 FillSolid in x_draw_glyph_string_background.  */
      if (face->stipple)
	XSetFillStyle (FRAME_X_DISPLAY (f), face->gc, FillOpaqueStippled);
      else
	XSetForeground (FRAME_X_DISPLAY (f), face->gc, face->background);
#endif

      /* If the fringe is adjacent to the left (right) scroll bar of a
	 leftmost (rightmost, respectively) window, then extend its
	 background to the gap between the fringe and the bar.  */
      if ((WINDOW_LEFTMOST_P (w)
	   && WINDOW_HAS_VERTICAL_SCROLL_BAR_ON_LEFT (w))
	  || (WINDOW_RIGHTMOST_P (w)
	      && WINDOW_HAS_VERTICAL_SCROLL_BAR_ON_RIGHT (w)))
	{
	  int sb_width = WINDOW_CONFIG_SCROLL_BAR_WIDTH (w);

	  if (sb_width > 0)
	    {
	      int bar_area_x = WINDOW_SCROLL_BAR_AREA_X (w);
	      int bar_area_width = (WINDOW_CONFIG_SCROLL_BAR_COLS (w)
				    * FRAME_COLUMN_WIDTH (f));

	      if (bx < 0
		  && (bar_area_x + bar_area_width == p->x
		      || p->x + p->wd == bar_area_x))
		{
		  /* Bitmap fills the fringe and we need background
		     extension.  */
		  bx = p->x;
		  nx = p->wd;
		}

	      if (bx >= 0)
		{
		  if (bar_area_x + bar_area_width == bx)
		    {
		      bx = bar_area_x + sb_width;
		      nx += bar_area_width - sb_width;
		    }
		  else if (bx + nx == bar_area_x)
		    nx += bar_area_width - sb_width;
		}
	    }
	}

      if (bx >= 0)
	{
	  mac_erase_rectangle (f, face->gc, bx, p->by, nx, p->ny);
	  /* The fringe background has already been filled.  */
	  overlay_p = 1;
	}

#if 0  /* MAC_TODO: stipple */
      if (!face->stipple)
	XSetForeground (FRAME_X_DISPLAY (f), face->gc, face->foreground);
#endif
    }

  /* Must clip because of partially visible lines.  */
  x_clip_to_row (w, row, -1, face->gc);

  if (p->which && p->which < max_fringe_bmp)
    {
      XGCValues gcv;

      XGetGCValues (display, face->gc, GCForeground, &gcv);
      XSetForeground (display, face->gc,
		      (p->cursor_p
		       ? (p->overlay_p ? face->background
			  : f->output_data.mac->cursor_pixel)
		       : face->foreground));
      mac_draw_cg_image (fringe_bmp[p->which], f, face->gc, 0, p->dh,
			 p->wd, p->h, p->x, p->y, overlay_p);
      XSetForeground (display, face->gc, gcv.foreground);
    }

  mac_reset_clip_rectangles (f, face->gc);
}

static void
mac_define_fringe_bitmap (int which, unsigned short *bits, int h, int wd)
{
  int i;
  CGDataProviderRef provider;

  if (which >= max_fringe_bmp)
    {
      i = max_fringe_bmp;
      max_fringe_bmp = which + 20;
      fringe_bmp = (CGImageRef *) xrealloc (fringe_bmp, max_fringe_bmp * sizeof (CGImageRef));
      while (i < max_fringe_bmp)
	fringe_bmp[i++] = 0;
    }

  for (i = 0; i < h; i++)
    bits[i] = ~bits[i];

  BLOCK_INPUT;

  provider = CGDataProviderCreateWithData (NULL, bits,
					   sizeof (unsigned short) * h, NULL);
  if (provider)
    {
      fringe_bmp[which] = CGImageMaskCreate (wd, h, 1, 1,
					     sizeof (unsigned short),
					     provider, NULL, 0);
      CGDataProviderRelease (provider);
    }

  UNBLOCK_INPUT;
}

static void
mac_destroy_fringe_bitmap (int which)
{
  if (which >= max_fringe_bmp)
    return;

  if (fringe_bmp[which])
    {
      BLOCK_INPUT;
      CGImageRelease (fringe_bmp[which]);
      UNBLOCK_INPUT;
    }
  fringe_bmp[which] = 0;
}



/* This is called when starting Emacs and when restarting after
   suspend.  When starting Emacs, no window is mapped.  And nothing
   must be done to Emacs's own window if it is suspended (though that
   rarely happens).  */

static void
XTset_terminal_modes (struct terminal *terminal)
{
}

/* This is called when exiting or suspending Emacs.  Exiting will make
   the windows go away, and suspending requires no action.  */

static void
XTreset_terminal_modes (struct terminal *terminal)
{
}


/***********************************************************************
			    Glyph display
 ***********************************************************************/



static void x_set_glyph_string_clipping (struct glyph_string *);
static void x_set_glyph_string_gc (struct glyph_string *);
static void x_draw_glyph_string_background (struct glyph_string *,
                                            int);
static void x_draw_glyph_string_foreground (struct glyph_string *);
static void x_draw_composite_glyph_string_foreground (struct glyph_string *);
static void x_draw_glyph_string_box (struct glyph_string *);
static void x_draw_glyph_string  (struct glyph_string *);
static void x_delete_glyphs (struct frame *, int) NO_RETURN;
static void x_compute_glyph_string_overhangs (struct glyph_string *);
static void x_set_cursor_gc (struct glyph_string *);
static void x_set_mode_line_face_gc (struct glyph_string *);
static void x_set_mouse_face_gc (struct glyph_string *);
/*static int x_alloc_lighter_color (struct frame *, Display *, Colormap,
  unsigned long *, double, int);*/
static void x_setup_relief_color (struct frame *, struct relief *,
                                  double, int, unsigned long);
static void x_setup_relief_colors (struct glyph_string *);
static void x_draw_image_glyph_string (struct glyph_string *);
static void x_draw_image_relief (struct glyph_string *);
static void x_draw_image_foreground (struct glyph_string *);
static void x_clear_glyph_string_rect (struct glyph_string *, int,
                                       int, int, int);
static void x_draw_relief_rect (struct frame *, int, int, int, int,
                                int, int, int, int, int, int,
				NativeRectangle *);
static void x_draw_box_rect (struct glyph_string *, int, int, int, int,
			     int, int, int, NativeRectangle *);
static void x_scroll_bar_clear (struct frame *);

#if GLYPH_DEBUG
static void x_check_font (struct frame *, struct font *);
#endif


/* Set S->gc to a suitable GC for drawing glyph string S in cursor
   face.  */

static void
x_set_cursor_gc (struct glyph_string *s)
{
  if (s->font == FRAME_FONT (s->f)
      && s->face->background == FRAME_BACKGROUND_PIXEL (s->f)
      && s->face->foreground == FRAME_FOREGROUND_PIXEL (s->f)
      && !s->cmp)
    s->gc = s->f->output_data.mac->cursor_gc;
  else
    {
      /* Cursor on non-default face: must merge.  */
      XGCValues xgcv;
      unsigned long mask;

      xgcv.background = s->f->output_data.mac->cursor_pixel;
      xgcv.foreground = s->face->background;

      /* If the glyph would be invisible, try a different foreground.  */
      if (xgcv.foreground == xgcv.background)
	xgcv.foreground = s->face->foreground;
      if (xgcv.foreground == xgcv.background)
	xgcv.foreground = s->f->output_data.mac->cursor_foreground_pixel;
      if (xgcv.foreground == xgcv.background)
	xgcv.foreground = s->face->foreground;

      /* Make sure the cursor is distinct from text in this face.  */
      if (xgcv.background == s->face->background
	  && xgcv.foreground == s->face->foreground)
	{
	  xgcv.background = s->face->foreground;
	  xgcv.foreground = s->face->background;
	}

      IF_DEBUG (x_check_font (s->f, s->font));
      mask = GCForeground | GCBackground;

      if (FRAME_MAC_DISPLAY_INFO (s->f)->scratch_cursor_gc)
	XChangeGC (s->display, FRAME_MAC_DISPLAY_INFO (s->f)->scratch_cursor_gc,
		   mask, &xgcv);
      else
	FRAME_MAC_DISPLAY_INFO (s->f)->scratch_cursor_gc
	  = XCreateGC (s->display, s->window, mask, &xgcv);

      s->gc = FRAME_MAC_DISPLAY_INFO (s->f)->scratch_cursor_gc;
    }
}


/* Set up S->gc of glyph string S for drawing text in mouse face.  */

static void
x_set_mouse_face_gc (struct glyph_string *s)
{
  int face_id;
  struct face *face;

  /* What face has to be used last for the mouse face?  */
  face_id = MOUSE_HL_INFO (s->f)->mouse_face_face_id;
  face = FACE_FROM_ID (s->f, face_id);
  if (face == NULL)
    face = FACE_FROM_ID (s->f, MOUSE_FACE_ID);

  if (s->first_glyph->type == CHAR_GLYPH)
    face_id = FACE_FOR_CHAR (s->f, face, s->first_glyph->u.ch, -1, Qnil);
  else
    face_id = FACE_FOR_CHAR (s->f, face, 0, -1, Qnil);
  s->face = FACE_FROM_ID (s->f, face_id);
  PREPARE_FACE_FOR_DISPLAY (s->f, s->face);

  if (s->font == s->face->font)
    s->gc = s->face->gc;
  else
    {
      /* Otherwise construct scratch_cursor_gc with values from FACE
	 except for FONT.  */
      XGCValues xgcv;
      unsigned long mask;

      xgcv.background = s->face->background;
      xgcv.foreground = s->face->foreground;
      mask = GCForeground | GCBackground;

      if (FRAME_MAC_DISPLAY_INFO (s->f)->scratch_cursor_gc)
	XChangeGC (s->display, FRAME_MAC_DISPLAY_INFO (s->f)->scratch_cursor_gc,
		   mask, &xgcv);
      else
	FRAME_MAC_DISPLAY_INFO (s->f)->scratch_cursor_gc
	  = XCreateGC (s->display, s->window, mask, &xgcv);

      s->gc = FRAME_MAC_DISPLAY_INFO (s->f)->scratch_cursor_gc;
    }

  xassert (s->gc != 0);
}


/* Set S->gc of glyph string S to a GC suitable for drawing a mode line.
   Faces to use in the mode line have already been computed when the
   matrix was built, so there isn't much to do, here.  */

static inline void
x_set_mode_line_face_gc (struct glyph_string *s)
{
  s->gc = s->face->gc;
}


/* Set S->gc of glyph string S for drawing that glyph string.  Set
   S->stippled_p to a non-zero value if the face of S has a stipple
   pattern.  */

static inline void
x_set_glyph_string_gc (struct glyph_string *s)
{
  PREPARE_FACE_FOR_DISPLAY (s->f, s->face);

  if (s->hl == DRAW_NORMAL_TEXT)
    {
      s->gc = s->face->gc;
      s->stippled_p = s->face->stipple != 0;
    }
  else if (s->hl == DRAW_INVERSE_VIDEO)
    {
      x_set_mode_line_face_gc (s);
      s->stippled_p = s->face->stipple != 0;
    }
  else if (s->hl == DRAW_CURSOR)
    {
      x_set_cursor_gc (s);
      s->stippled_p = 0;
    }
  else if (s->hl == DRAW_MOUSE_FACE)
    {
      x_set_mouse_face_gc (s);
      s->stippled_p = s->face->stipple != 0;
    }
  else if (s->hl == DRAW_IMAGE_RAISED
	   || s->hl == DRAW_IMAGE_SUNKEN)
    {
      s->gc = s->face->gc;
      s->stippled_p = s->face->stipple != 0;
    }
  else
    {
      s->gc = s->face->gc;
      s->stippled_p = s->face->stipple != 0;
    }

  /* GC must have been set.  */
  xassert (s->gc != 0);
}


/* Set clipping for output of glyph string S.  S may be part of a mode
   line or menu if we don't have X toolkit support.  */

static inline void
x_set_glyph_string_clipping (struct glyph_string *s)
{
  NativeRectangle *r = s->clip;
  int n = get_glyph_string_clip_rects (s, r, MAX_CLIP_RECTS);

  mac_set_clip_rectangles (s->f, s->gc, r, n);
  s->num_clips = n;
}


/* Set SRC's clipping for output of glyph string DST.  This is called
   when we are drawing DST's left_overhang or right_overhang only in
   the area of SRC.  */

static void
x_set_glyph_string_clipping_exactly (struct glyph_string *src, struct glyph_string *dst)
{
  NativeRectangle r;

  STORE_NATIVE_RECT (r, src->x, src->y, src->width, src->height);
  dst->clip[0] = r;
  dst->num_clips = 1;
  mac_set_clip_rectangles (dst->f, dst->gc, &r, 1);
}


/* RIF:
   Compute left and right overhang of glyph string S.  */

static void
x_compute_glyph_string_overhangs (struct glyph_string *s)
{
  if (s->cmp == NULL
      && (s->first_glyph->type == CHAR_GLYPH
	  || s->first_glyph->type == COMPOSITE_GLYPH))
    {
      struct font_metrics metrics;

      if (s->first_glyph->type == CHAR_GLYPH)
	{
	  unsigned *code = alloca (sizeof (unsigned) * s->nchars);
	  struct font *font = s->font;
	  int i;

	  for (i = 0; i < s->nchars; i++)
	    code[i] = (s->char2b[i].byte1 << 8) | s->char2b[i].byte2;
	  font->driver->text_extents (font, code, s->nchars, &metrics);
	}
      else
	{
	  Lisp_Object gstring = composition_gstring_from_id (s->cmp_id);

	  composition_gstring_width (gstring, s->cmp_from, s->cmp_to, &metrics);
	}
      s->right_overhang = (metrics.rbearing > metrics.width
			   ? metrics.rbearing - metrics.width : 0);
      s->left_overhang = metrics.lbearing < 0 ? - metrics.lbearing : 0;
    }
  else if (s->cmp)
    {
      s->right_overhang = s->cmp->rbearing - s->cmp->pixel_width;
      s->left_overhang = - s->cmp->lbearing;
    }
}


/* Fill rectangle X, Y, W, H with background color of glyph string S.  */

static inline void
x_clear_glyph_string_rect (struct glyph_string *s, int x, int y, int w, int h)
{
  mac_erase_rectangle (s->f, s->gc, x, y, w, h);
}


/* Draw the background of glyph_string S.  If S->background_filled_p
   is non-zero don't draw it.  FORCE_P non-zero means draw the
   background even if it wouldn't be drawn normally.  This is used
   when a string preceding S draws into the background of S, or S
   contains the first component of a composition.  */

static void
x_draw_glyph_string_background (struct glyph_string *s, int force_p)
{
  /* Nothing to do if background has already been drawn or if it
     shouldn't be drawn in the first place.  */
  if (!s->background_filled_p)
    {
      int box_line_width = max (s->face->box_line_width, 0);

#if 0 /* MAC_TODO: stipple */
      if (s->stippled_p)
	{
	  /* Fill background with a stipple pattern.  */
	  XSetFillStyle (s->display, s->gc, FillOpaqueStippled);
	  XFillRectangle (s->display, s->window, s->gc, s->x,
			  s->y + box_line_width,
			  s->background_width,
			  s->height - 2 * box_line_width);
	  XSetFillStyle (s->display, s->gc, FillSolid);
	  s->background_filled_p = 1;
	}
      else
#endif
        if (FONT_HEIGHT (s->font) < s->height - 2 * box_line_width
	       || s->font_not_found_p
	       || s->extends_to_end_of_line_p
	       || force_p)
	{
	  x_clear_glyph_string_rect (s, s->x, s->y + box_line_width,
				     s->background_width,
				     s->height - 2 * box_line_width);
	  s->background_filled_p = 1;
	}
    }
}


/* Draw the foreground of glyph string S.  */

static void
x_draw_glyph_string_foreground (struct glyph_string *s)
{
  int i, x;

  /* If first glyph of S has a left box line, start drawing the text
     of S to the right of that box line.  */
  if (s->face->box != FACE_NO_BOX
      && s->first_glyph->left_box_line_p)
    x = s->x + eabs (s->face->box_line_width);
  else
    x = s->x;

  /* Draw characters of S as rectangles if S's font could not be
     loaded.  */
  if (s->font_not_found_p)
    {
      for (i = 0; i < s->nchars; ++i)
	{
	  struct glyph *g = s->first_glyph + i;
	  mac_draw_rectangle (s->f, s->gc, x, s->y,
			      g->pixel_width - 1, s->height - 1);
	  x += g->pixel_width;
	}
    }
  else
    {
      struct font *font = s->font;
      int boff = font->baseline_offset;
      int y;

      if (font->vertical_centering)
	boff = VCENTER_BASELINE_OFFSET (font, s->f) - boff;

      y = s->ybase - boff;
      if (s->for_overlaps
	  || (s->background_filled_p && s->hl != DRAW_CURSOR))
	font->driver->draw (s, 0, s->nchars, x, y, 0);
      else
	font->driver->draw (s, 0, s->nchars, x, y, 1);
      if (s->face->overstrike)
	font->driver->draw (s, 0, s->nchars, x + 1, y, 0);
    }
}

/* Draw the foreground of composite glyph string S.  */

static void
x_draw_composite_glyph_string_foreground (struct glyph_string *s)
{
  int i, j, x;
  struct font *font = s->font;

  /* If first glyph of S has a left box line, start drawing the text
     of S to the right of that box line.  */
  if (s->face && s->face->box != FACE_NO_BOX
      && s->first_glyph->left_box_line_p)
    x = s->x + eabs (s->face->box_line_width);
  else
    x = s->x;

  /* S is a glyph string for a composition.  S->cmp_from is the index
     of the first character drawn for glyphs of this composition.
     S->cmp_from == 0 means we are drawing the very first character of
     this composition.  */

  /* Draw a rectangle for the composition if the font for the very
     first character of the composition could not be loaded.  */
  if (s->font_not_found_p)
    {
      if (s->cmp_from == 0)
	mac_draw_rectangle (s->f, s->gc, x, s->y,
			    s->width - 1, s->height - 1);
    }
  else if (! s->first_glyph->u.cmp.automatic)
    {
      int y = s->ybase;

      for (i = 0, j = s->cmp_from; i < s->nchars; i++, j++)
	/* TAB in a composition means display glyphs with padding
	   space on the left or right.  */
	if (COMPOSITION_GLYPH (s->cmp, j) != '\t')
	  {
	    int xx = x + s->cmp->offsets[j * 2];
	    int yy = y - s->cmp->offsets[j * 2 + 1];

	    font->driver->draw (s, j, j + 1, xx, yy, 0);
	    if (s->face->overstrike)
	      font->driver->draw (s, j, j + 1, xx + 1, yy, 0);
	  }
    }
  else
    {
      Lisp_Object gstring = composition_gstring_from_id (s->cmp_id);
      Lisp_Object glyph;
      int y = s->ybase;
      int width = 0;

      for (i = j = s->cmp_from; i < s->cmp_to; i++)
	{
	  glyph = LGSTRING_GLYPH (gstring, i);
	  if (NILP (LGLYPH_ADJUSTMENT (glyph)))
	    width += LGLYPH_WIDTH (glyph);
	  else
	    {
	      int xoff, yoff, wadjust;

	      if (j < i)
		{
		  font->driver->draw (s, j, i, x, y, 0);
		  if (s->face->overstrike)
		    font->driver->draw (s, j, i, x + 1, y, 0);
		  x += width;
		}
	      xoff = LGLYPH_XOFF (glyph);
	      yoff = LGLYPH_YOFF (glyph);
	      wadjust = LGLYPH_WADJUST (glyph);
	      font->driver->draw (s, i, i + 1, x + xoff, y + yoff, 0);
	      if (s->face->overstrike)
		font->driver->draw (s, i, i + 1, x + xoff + 1, y + yoff, 0);
	      x += wadjust;
	      j = i + 1;
	      width = 0;
	    }
	}
      if (j < i)
	{
	  font->driver->draw (s, j, i, x, y, 0);
	  if (s->face->overstrike)
	    font->driver->draw (s, j, i, x + 1, y, 0);
	}
    }
}


/* Draw the foreground of glyph string S for glyphless characters.  */

static void
x_draw_glyphless_glyph_string_foreground (struct glyph_string *s)
{
  struct glyph *glyph = s->first_glyph;
  XChar2b char2b[8];
  int x, i, j;

  /* If first glyph of S has a left box line, start drawing the text
     of S to the right of that box line.  */
  if (s->face && s->face->box != FACE_NO_BOX
      && s->first_glyph->left_box_line_p)
    x = s->x + eabs (s->face->box_line_width);
  else
    x = s->x;

  s->char2b = char2b;

  for (i = 0; i < s->nchars; i++, glyph++)
    {
      char buf[7], *str = NULL;
      int len = glyph->u.glyphless.len;

      if (glyph->u.glyphless.method == GLYPHLESS_DISPLAY_ACRONYM)
	{
	  if (len > 0
	      && CHAR_TABLE_P (Vglyphless_char_display)
	      && (CHAR_TABLE_EXTRA_SLOTS (XCHAR_TABLE (Vglyphless_char_display))
		  >= 1))
	    {
	      Lisp_Object acronym
		= (! glyph->u.glyphless.for_no_font
		   ? CHAR_TABLE_REF (Vglyphless_char_display,
				     glyph->u.glyphless.ch)
		   : XCHAR_TABLE (Vglyphless_char_display)->extras[0]);
	      if (STRINGP (acronym))
		str = SSDATA (acronym);
	    }
	}
      else if (glyph->u.glyphless.method == GLYPHLESS_DISPLAY_HEX_CODE)
	{
	  sprintf ((char *) buf, "%0*X",
		   glyph->u.glyphless.ch < 0x10000 ? 4 : 6,
		   glyph->u.glyphless.ch);
	  str = buf;
	}

      if (str)
	{
	  int upper_len = (len + 1) / 2;
	  unsigned code;

	  /* It is assured that all LEN characters in STR is ASCII.  */
	  for (j = 0; j < len; j++)
	    {
	      code = s->font->driver->encode_char (s->font, str[j]);
	      STORE_XCHAR2B (char2b + j, code >> 8, code & 0xFF);
	    }
	  s->font->driver->draw (s, 0, upper_len,
				 x + glyph->slice.glyphless.upper_xoff,
				 s->ybase + glyph->slice.glyphless.upper_yoff,
				 0);
	  s->font->driver->draw (s, upper_len, len,
				 x + glyph->slice.glyphless.lower_xoff,
				 s->ybase + glyph->slice.glyphless.lower_yoff,
				 0);
	}
      if (glyph->u.glyphless.method != GLYPHLESS_DISPLAY_THIN_SPACE)
	mac_draw_rectangle (s->f, s->gc, x, s->ybase - glyph->ascent,
			    glyph->pixel_width - 1,
			    glyph->ascent + glyph->descent - 1);
      x += glyph->pixel_width;
   }
}


/* On frame F, translate pixel colors to RGB values for the NCOLORS
   colors in COLORS.  */

void
x_query_colors (struct frame *f, XColor *colors, int ncolors)
{
  int i;

  for (i = 0; i < ncolors; ++i)
    {
      unsigned long pixel = colors[i].pixel;

      colors[i].red = RED16_FROM_ULONG (pixel);
      colors[i].green = GREEN16_FROM_ULONG (pixel);
      colors[i].blue = BLUE16_FROM_ULONG (pixel);
    }
}


/* On frame F, translate pixel color to RGB values for the color in
   COLOR.  */

void
x_query_color (struct frame *f, XColor *color)
{
  x_query_colors (f, color, 1);
}


/* Brightness beyond which a color won't have its highlight brightness
   boosted.

   Nominally, highlight colors for `3d' faces are calculated by
   brightening an object's color by a constant scale factor, but this
   doesn't yield good results for dark colors, so for colors who's
   brightness is less than this value (on a scale of 0-255) have to
   use an additional additive factor.

   The value here is set so that the default menu-bar/mode-line color
   (grey75) will not have its highlights changed at all.  */
#define HIGHLIGHT_COLOR_DARK_BOOST_LIMIT 187


/* Allocate a color which is lighter or darker than *COLOR by FACTOR
   or DELTA.  Try a color with RGB values multiplied by FACTOR first.
   If this produces the same color as COLOR, try a color where all RGB
   values have DELTA added.  Return the allocated color in *COLOR.
   DISPLAY is the X display, CMAP is the colormap to operate on.
   Value is non-zero if successful.  */

static int
mac_alloc_lighter_color (struct frame *f, unsigned long *color, double factor,
			 int delta)
{
  unsigned long new;
  long bright;

  /* On Mac, RGB values are 0-255, not 0-65535, so scale delta. */
  delta /= 256;

  /* Change RGB values by specified FACTOR.  Avoid overflow!  */
  xassert (factor >= 0);
  new = RGB_TO_ULONG (min (0xff, (int) (factor * RED_FROM_ULONG (*color))),
                    min (0xff, (int) (factor * GREEN_FROM_ULONG (*color))),
                    min (0xff, (int) (factor * BLUE_FROM_ULONG (*color))));

  /* Calculate brightness of COLOR.  */
  bright = (2 * RED_FROM_ULONG (*color) + 3 * GREEN_FROM_ULONG (*color)
            + BLUE_FROM_ULONG (*color)) / 6;

  /* We only boost colors that are darker than
     HIGHLIGHT_COLOR_DARK_BOOST_LIMIT.  */
  if (bright < HIGHLIGHT_COLOR_DARK_BOOST_LIMIT)
    /* Make an additive adjustment to NEW, because it's dark enough so
       that scaling by FACTOR alone isn't enough.  */
    {
      /* How far below the limit this color is (0 - 1, 1 being darker).  */
      double dimness = 1 - (double)bright / HIGHLIGHT_COLOR_DARK_BOOST_LIMIT;
      /* The additive adjustment.  */
      int min_delta = delta * dimness * factor / 2;

      if (factor < 1)
        new = RGB_TO_ULONG (max (0, min (0xff, (int) (RED_FROM_ULONG (new)) - min_delta)),
			    max (0, min (0xff, (int) (GREEN_FROM_ULONG (new)) - min_delta)),
			    max (0, min (0xff, (int) (BLUE_FROM_ULONG (new)) - min_delta)));
      else
        new = RGB_TO_ULONG (max (0, min (0xff, (int) (min_delta + RED_FROM_ULONG (new)))),
			    max (0, min (0xff, (int) (min_delta + GREEN_FROM_ULONG (new)))),
			    max (0, min (0xff, (int) (min_delta + BLUE_FROM_ULONG (new)))));
    }

  if (new == *color)
    new = RGB_TO_ULONG (max (0, min (0xff, (int) (delta + RED_FROM_ULONG (*color)))),
                      max (0, min (0xff, (int) (delta + GREEN_FROM_ULONG (*color)))),
                      max (0, min (0xff, (int) (delta + BLUE_FROM_ULONG (*color)))));

  /* MAC_TODO: Map to palette and retry with delta if same? */
  /* MAC_TODO: Free colors (if using palette)? */

  if (new == *color)
    return 0;

  *color = new;

  return 1;
}


/* Set up the foreground color for drawing relief lines of glyph
   string S.  RELIEF is a pointer to a struct relief containing the GC
   with which lines will be drawn.  Use a color that is FACTOR or
   DELTA lighter or darker than the relief's background which is found
   in S->f->output_data.x->relief_background.  If such a color cannot
   be allocated, use DEFAULT_PIXEL, instead.  */

static void
x_setup_relief_color (struct frame *f, struct relief *relief, double factor,
		      int delta, unsigned long default_pixel)
{
  XGCValues xgcv;
  struct mac_output *di = f->output_data.mac;
  unsigned long mask = GCForeground;
  unsigned long pixel;
  unsigned long background = di->relief_background;
  struct mac_display_info *dpyinfo = FRAME_MAC_DISPLAY_INFO (f);

  /* MAC_TODO: Free colors (if using palette)? */

  /* Allocate new color.  */
  xgcv.foreground = default_pixel;
  pixel = background;
  if (dpyinfo->n_planes != 1
      && mac_alloc_lighter_color (f, &pixel, factor, delta))
    {
      relief->allocated_p = 1;
      xgcv.foreground = relief->pixel = pixel;
    }

  if (relief->gc == 0)
    {
#if 0 /* MAC_TODO: stipple */
      xgcv.stipple = dpyinfo->gray;
      mask |= GCStipple;
#endif
      relief->gc = XCreateGC (NULL, FRAME_MAC_WINDOW (f), mask, &xgcv);
    }
  else
    XChangeGC (NULL, relief->gc, mask, &xgcv);
}


/* Set up colors for the relief lines around glyph string S.  */

static void
x_setup_relief_colors (struct glyph_string *s)
{
  struct mac_output *di = s->f->output_data.mac;
  unsigned long color;

  if (s->face->use_box_color_for_shadows_p)
    color = s->face->box_color;
  else if (s->first_glyph->type == IMAGE_GLYPH
	   && s->img->pixmap
	   && !IMAGE_BACKGROUND_TRANSPARENT (s->img, s->f, 0))
    color = IMAGE_BACKGROUND (s->img, s->f, 0);
  else
    {
      XGCValues xgcv;

      /* Get the background color of the face.  */
      XGetGCValues (s->display, s->gc, GCBackground, &xgcv);
      color = xgcv.background;
    }

  if (di->white_relief.gc == 0
      || color != di->relief_background)
    {
      di->relief_background = color;
      x_setup_relief_color (s->f, &di->white_relief, 1.2, 0x8000,
			    WHITE_PIX_DEFAULT (s->f));
      x_setup_relief_color (s->f, &di->black_relief, 0.6, 0x4000,
			    BLACK_PIX_DEFAULT (s->f));
    }
}


/* Draw a relief on frame F inside the rectangle given by LEFT_X,
   TOP_Y, RIGHT_X, and BOTTOM_Y.  WIDTH is the thickness of the relief
   to draw, it must be >= 0.  RAISED_P non-zero means draw a raised
   relief.  LEFT_P non-zero means draw a relief on the left side of
   the rectangle.  RIGHT_P non-zero means draw a relief on the right
   side of the rectangle.  CLIP_RECT is the clipping rectangle to use
   when drawing.  */

static void
x_draw_relief_rect (struct frame *f,
		    int left_x, int top_y, int right_x, int bottom_y, int width,
		    int raised_p, int top_p, int bot_p, int left_p, int right_p,
		    NativeRectangle *clip_rect)
{
  GC top_left_gc, bottom_right_gc;
  int corners = 0;

  if (raised_p)
    {
      top_left_gc = f->output_data.mac->white_relief.gc;
      bottom_right_gc = f->output_data.mac->black_relief.gc;
    }
  else
    {
      top_left_gc = f->output_data.mac->black_relief.gc;
      bottom_right_gc = f->output_data.mac->white_relief.gc;
    }

  mac_set_clip_rectangles (f, top_left_gc, clip_rect, 1);
  mac_set_clip_rectangles (f, bottom_right_gc, clip_rect, 1);

  if (left_p)
    {
      mac_fill_rectangle (f, top_left_gc, left_x, top_y,
			  width, bottom_y + 1 - top_y);
      if (top_p)
	corners |= 1 << CORNER_TOP_LEFT;
      if (bot_p)
	corners |= 1 << CORNER_BOTTOM_LEFT;
    }
  if (right_p)
    {
      mac_fill_rectangle (f, bottom_right_gc, right_x + 1 - width, top_y,
			  width, bottom_y + 1 - top_y);
      if (top_p)
	corners |= 1 << CORNER_TOP_RIGHT;
      if (bot_p)
	corners |= 1 << CORNER_BOTTOM_RIGHT;
    }
  if (top_p)
    {
      if (!right_p)
	mac_fill_rectangle (f, top_left_gc, left_x, top_y,
			    right_x + 1 - left_x, width);
      else
	mac_fill_trapezoid_for_relief (f, top_left_gc, left_x, top_y,
				       right_x + 1 - left_x, width, 1);
    }
  if (bot_p)
    {
      if (!left_p)
	mac_fill_rectangle (f, bottom_right_gc, left_x, bottom_y + 1 - width,
			    right_x + 1 - left_x, width);
      else
	mac_fill_trapezoid_for_relief (f, bottom_right_gc,
				       left_x, bottom_y + 1 - width,
				       right_x + 1 - left_x, width, 0);
    }
  if (left_p && width != 1)
    mac_fill_rectangle (f, bottom_right_gc, left_x, top_y,
			1, bottom_y + 1 - top_y);
  if (top_p && width != 1)
    mac_fill_rectangle (f, bottom_right_gc, left_x, top_y,
			right_x + 1 - left_x, 1);
  if (corners)
    {
      XSetBackground (FRAME_MAC_DISPLAY (f), top_left_gc,
		      FRAME_BACKGROUND_PIXEL (f));
      mac_erase_corners_for_relief (f, top_left_gc, left_x, top_y,
				    right_x - left_x + 1, bottom_y - top_y + 1,
				    3, corners);
    }

  mac_set_clip_rectangles (f, top_left_gc, clip_rect, 1);
  mac_set_clip_rectangles (f, bottom_right_gc, clip_rect, 1);
}


/* Draw a box on frame F inside the rectangle given by LEFT_X, TOP_Y,
   RIGHT_X, and BOTTOM_Y.  WIDTH is the thickness of the lines to
   draw, it must be >= 0.  LEFT_P non-zero means draw a line on the
   left side of the rectangle.  RIGHT_P non-zero means draw a line
   on the right side of the rectangle.  CLIP_RECT is the clipping
   rectangle to use when drawing.  */

static void
x_draw_box_rect (struct glyph_string *s,
		 int left_x, int top_y, int right_x, int bottom_y, int width,
		 int left_p, int right_p, NativeRectangle *clip_rect)
{
  XGCValues xgcv;

  XGetGCValues (s->display, s->gc, GCForeground, &xgcv);
  XSetForeground (s->display, s->gc, s->face->box_color);
  mac_set_clip_rectangles (s->f, s->gc, clip_rect, 1);

  /* Top.  */
  mac_fill_rectangle (s->f, s->gc, left_x, top_y,
		      right_x - left_x + 1, width);

  /* Left.  */
  if (left_p)
    mac_fill_rectangle (s->f, s->gc, left_x, top_y,
			width, bottom_y - top_y + 1);

  /* Bottom.  */
  mac_fill_rectangle (s->f, s->gc, left_x, bottom_y - width + 1,
		      right_x - left_x + 1, width);

  /* Right.  */
  if (right_p)
    mac_fill_rectangle (s->f, s->gc, right_x - width + 1,
			top_y, width, bottom_y - top_y + 1);

  XSetForeground (s->display, s->gc, xgcv.foreground);
  mac_reset_clip_rectangles (s->f, s->gc);
}


/* Draw a box around glyph string S.  */

static void
x_draw_glyph_string_box (struct glyph_string *s)
{
  int width, left_x, right_x, top_y, bottom_y, last_x, raised_p;
  int left_p, right_p;
  struct glyph *last_glyph;
  NativeRectangle clip_rect;

  last_x = ((s->row->full_width_p && !s->w->pseudo_window_p)
	    ? WINDOW_RIGHT_EDGE_X (s->w)
	    : window_box_right (s->w, s->area));

  /* The glyph that may have a right box line.  */
  last_glyph = (s->cmp || s->img
		? s->first_glyph
		: s->first_glyph + s->nchars - 1);

  width = eabs (s->face->box_line_width);
  raised_p = s->face->box == FACE_RAISED_BOX;
  left_x = s->x;
  right_x = (s->row->full_width_p && s->extends_to_end_of_line_p
	     ? last_x - 1
	     : min (last_x, s->x + s->background_width) - 1);
  top_y = s->y;
  bottom_y = top_y + s->height - 1;

  left_p = (s->first_glyph->left_box_line_p
	    || (s->hl == DRAW_MOUSE_FACE
		&& (s->prev == NULL
		    || s->prev->hl != s->hl)));
  right_p = (last_glyph->right_box_line_p
	     || (s->hl == DRAW_MOUSE_FACE
		 && (s->next == NULL
		     || s->next->hl != s->hl)));

  get_glyph_string_clip_rect (s, &clip_rect);

  if (s->face->box == FACE_SIMPLE_BOX)
    x_draw_box_rect (s, left_x, top_y, right_x, bottom_y, width,
		     left_p, right_p, &clip_rect);
  else
    {
      x_setup_relief_colors (s);
      x_draw_relief_rect (s->f, left_x, top_y, right_x, bottom_y,
			  width, raised_p, 1, 1, left_p, right_p, &clip_rect);
    }
}


/* Draw foreground of image glyph string S.  */

static void
x_draw_image_foreground (struct glyph_string *s)
{
  int x = s->x;
  int y = s->ybase - image_ascent (s->img, s->face, &s->slice);

  /* If first glyph of S has a left box line, start drawing it to the
     right of that line.  */
  if (s->face->box != FACE_NO_BOX
      && s->first_glyph->left_box_line_p
      && s->slice.x == 0)
    x += eabs (s->face->box_line_width);

  /* If there is a margin around the image, adjust x- and y-position
     by that margin.  */
  if (s->slice.x == 0)
    x += s->img->hmargin;
  if (s->slice.y == 0)
    y += s->img->vmargin;

  if (s->img->pixmap)
    {
      x_set_glyph_string_clipping (s);

      mac_draw_cg_image (s->img->cg_image,
			 s->f, s->gc, s->slice.x, s->slice.y,
			 s->slice.width, s->slice.height, x, y, 1);
      if (!s->img->mask)
	{
	  /* When the image has a mask, we can expect that at
	     least part of a mouse highlight or a block cursor will
	     be visible.  If the image doesn't have a mask, make
	     a block cursor visible by drawing a rectangle around
	     the image.  I believe it's looking better if we do
	     nothing here for mouse-face.  */
	  if (s->hl == DRAW_CURSOR)
	    {
	      int relief = s->img->relief;
	      if (relief < 0) relief = -relief;
	      mac_draw_rectangle (s->f, s->gc, x - relief, y - relief,
				  s->slice.width + relief*2 - 1,
				  s->slice.height + relief*2 - 1);
	    }
	}
    }
  else
    /* Draw a rectangle if image could not be loaded.  */
    mac_draw_rectangle (s->f, s->gc, x, y,
			s->slice.width - 1, s->slice.height - 1);
}


/* Draw a relief around the image glyph string S.  */

static void
x_draw_image_relief (struct glyph_string *s)
{
  int x1, y1, thick, raised_p, top_p, bot_p, left_p, right_p;
  int extra_x, extra_y;
  NativeRectangle r;
  int x = s->x;
  int y = s->ybase - image_ascent (s->img, s->face, &s->slice);

  /* If first glyph of S has a left box line, start drawing it to the
     right of that line.  */
  if (s->face->box != FACE_NO_BOX
      && s->first_glyph->left_box_line_p
      && s->slice.x == 0)
    x += eabs (s->face->box_line_width);

  /* If there is a margin around the image, adjust x- and y-position
     by that margin.  */
  if (s->slice.x == 0)
    x += s->img->hmargin;
  if (s->slice.y == 0)
    y += s->img->vmargin;

  if (s->hl == DRAW_IMAGE_SUNKEN
      || s->hl == DRAW_IMAGE_RAISED)
    {
      thick = tool_bar_button_relief >= 0 ? tool_bar_button_relief : DEFAULT_TOOL_BAR_BUTTON_RELIEF;
      raised_p = s->hl == DRAW_IMAGE_RAISED;
    }
  else
    {
      thick = eabs (s->img->relief);
      raised_p = s->img->relief > 0;
    }

  x1 = x + s->slice.width - 1;
  y1 = y + s->slice.height - 1;

  extra_x = extra_y = 0;
  if (s->face->id == TOOL_BAR_FACE_ID)
    {
      if (CONSP (Vtool_bar_button_margin)
	  && INTEGERP (XCAR (Vtool_bar_button_margin))
	  && INTEGERP (XCDR (Vtool_bar_button_margin)))
	{
	  extra_x = XINT (XCAR (Vtool_bar_button_margin));
	  extra_y = XINT (XCDR (Vtool_bar_button_margin));
	}
      else if (INTEGERP (Vtool_bar_button_margin))
	extra_x = extra_y = XINT (Vtool_bar_button_margin);
    }

  top_p = bot_p = left_p = right_p = 0;

  if (s->slice.x == 0)
    x -= thick + extra_x, left_p = 1;
  if (s->slice.y == 0)
    y -= thick + extra_y, top_p = 1;
  if (s->slice.x + s->slice.width == s->img->width)
    x1 += thick + extra_x, right_p = 1;
  if (s->slice.y + s->slice.height == s->img->height)
    y1 += thick + extra_y, bot_p = 1;

  x_setup_relief_colors (s);
  get_glyph_string_clip_rect (s, &r);
  x_draw_relief_rect (s->f, x, y, x1, y1, thick, raised_p,
		      top_p, bot_p, left_p, right_p, &r);
}


/* Draw part of the background of glyph string S.  X, Y, W, and H
   give the rectangle to draw.  */

static void
x_draw_glyph_string_bg_rect (struct glyph_string *s, int x, int y, int w, int h)
{
#if 0 /* MAC_TODO: stipple */
  if (s->stippled_p)
    {
      /* Fill background with a stipple pattern.  */
      XSetFillStyle (s->display, s->gc, FillOpaqueStippled);
      XFillRectangle (s->display, s->window, s->gc, x, y, w, h);
      XSetFillStyle (s->display, s->gc, FillSolid);
    }
  else
#endif /* MAC_TODO */
    x_clear_glyph_string_rect (s, x, y, w, h);
}


/* Draw image glyph string S.

            s->y
   s->x      +-------------------------
	     |   s->face->box
	     |
	     |     +-------------------------
	     |     |  s->img->margin
	     |     |
	     |     |       +-------------------
	     |     |       |  the image

 */

static void
x_draw_image_glyph_string (struct glyph_string *s)
{
  int x, y;
  int box_line_hwidth = eabs (s->face->box_line_width);
  int box_line_vwidth = max (s->face->box_line_width, 0);
  int height;

  height = s->height - 2 * box_line_vwidth;


  /* Fill background with face under the image.  Do it only if row is
     taller than image or if image has a clip mask to reduce
     flickering.  */
  s->stippled_p = s->face->stipple != 0;
  if (height > s->slice.height
      || s->img->hmargin
      || s->img->vmargin
      || s->img->mask
      || s->img->pixmap == 0
      || s->width != s->background_width)
    {
      x = s->x;
      if (s->first_glyph->left_box_line_p
	  && s->slice.x == 0)
	x += box_line_hwidth;

      y = s->y;
      if (s->slice.y == 0)
	y += box_line_vwidth;

      x_draw_glyph_string_bg_rect (s, x, y, s->background_width, height);

      s->background_filled_p = 1;
    }

  /* Draw the foreground.  */
  x_draw_image_foreground (s);

  /* If we must draw a relief around the image, do it.  */
  if (s->img->relief
      || s->hl == DRAW_IMAGE_RAISED
      || s->hl == DRAW_IMAGE_SUNKEN)
    x_draw_image_relief (s);
}


/* Draw stretch glyph string S.  */

static void
x_draw_stretch_glyph_string (struct glyph_string *s)
{
  xassert (s->first_glyph->type == STRETCH_GLYPH);

  if (s->hl == DRAW_CURSOR
      && !x_stretch_cursor_p)
    {
      /* If `x-stretch-cursor' is nil, don't draw a block cursor as
	 wide as the stretch glyph.  */
      int width, background_width = s->background_width;
      int x = s->x;

      if (!s->row->reversed_p)
	{
	  int left_x = window_box_left_offset (s->w, TEXT_AREA);

	  if (x < left_x)
	    {
	      background_width -= left_x - x;
	      x = left_x;
	    }
	}
      else
	{
	  /* In R2L rows, draw the cursor on the right edge of the
	     stretch glyph.  */
	  int right_x = window_box_right_offset (s->w, TEXT_AREA);

	  if (x + background_width > right_x)
	    background_width -= x - right_x;
	  x += background_width;
	}
      width = min (FRAME_COLUMN_WIDTH (s->f), background_width);
      if (s->row->reversed_p)
	x -= width;

      /* Draw cursor.  */
      x_draw_glyph_string_bg_rect (s, x, s->y, width, s->height);

      /* Clear rest using the GC of the original non-cursor face.  */
      if (width < background_width)
	{
	  int y = s->y;
	  int w = background_width - width, h = s->height;
	  NativeRectangle r;
	  GC gc;

	  if (!s->row->reversed_p)
	    x += width;
	  else
	    x = s->x;
	  if (s->row->mouse_face_p
	      && cursor_in_mouse_face_p (s->w))
	    {
	      x_set_mouse_face_gc (s);
	      gc = s->gc;
	    }
	  else
	    gc = s->face->gc;

	  get_glyph_string_clip_rect (s, &r);
	  mac_set_clip_rectangles (s->f, gc, &r, 1);

#if 0 /* MAC_TODO: stipple */
	  if (s->face->stipple)
	    {
	      /* Fill background with a stipple pattern.  */
	      XSetFillStyle (s->display, gc, FillOpaqueStippled);
	      XFillRectangle (s->display, s->window, gc, x, y, w, h);
	      XSetFillStyle (s->display, gc, FillSolid);
	    }
	  else
#endif /* MAC_TODO */
	    mac_erase_rectangle (s->f, gc, x, y, w, h);
	}
    }
  else if (!s->background_filled_p)
    {
      int background_width = s->background_width;
      int x = s->x, left_x = window_box_left_offset (s->w, TEXT_AREA);

      /* Don't draw into left margin, fringe or scrollbar area
         except for header line and mode line.  */
      if (x < left_x && !s->row->mode_line_p)
	{
	  background_width -= left_x - x;
	  x = left_x;
	}
      if (background_width > 0)
	x_draw_glyph_string_bg_rect (s, x, s->y, background_width, s->height);
    }

  s->background_filled_p = 1;
}


/* Draw glyph string S.  */

static void
x_draw_glyph_string (struct glyph_string *s)
{
  int relief_drawn_p = 0;

  /* If S draws into the background of its successors, draw the
     background of the successors first so that S can draw into it.
     This makes S->next use XDrawString instead of XDrawImageString.  */
  if (s->next && s->right_overhang && !s->for_overlaps)
    {
      int width;
      struct glyph_string *next;

      for (width = 0, next = s->next;
	   next && width < s->right_overhang;
	   width += next->width, next = next->next)
	if (next->first_glyph->type != IMAGE_GLYPH)
	  {
	    x_set_glyph_string_gc (next);
	    x_set_glyph_string_clipping (next);
	    if (next->first_glyph->type == STRETCH_GLYPH)
	      x_draw_stretch_glyph_string (next);
	    else
	      x_draw_glyph_string_background (next, 1);
	    next->num_clips = 0;
	  }
    }

  /* Set up S->gc, set clipping and draw S.  */
  x_set_glyph_string_gc (s);

  /* Draw relief (if any) in advance for char/composition so that the
     glyph string can be drawn over it.  */
  if (!s->for_overlaps
      && s->face->box != FACE_NO_BOX
      && (s->first_glyph->type == CHAR_GLYPH
	  || s->first_glyph->type == COMPOSITE_GLYPH))

    {
      x_set_glyph_string_clipping (s);
      x_draw_glyph_string_background (s, 1);
      x_draw_glyph_string_box (s);
      x_set_glyph_string_clipping (s);
      relief_drawn_p = 1;
    }
  else if (!s->clip_head /* draw_glyphs didn't specify a clip mask. */
	   && !s->clip_tail
	   && ((s->prev && s->prev->hl != s->hl && s->left_overhang)
	       || (s->next && s->next->hl != s->hl && s->right_overhang)))
    /* We must clip just this glyph.  left_overhang part has already
       drawn when s->prev was drawn, and right_overhang part will be
       drawn later when s->next is drawn. */
    x_set_glyph_string_clipping_exactly (s, s);
  else
    x_set_glyph_string_clipping (s);

  switch (s->first_glyph->type)
    {
    case IMAGE_GLYPH:
      x_draw_image_glyph_string (s);
      break;

    case STRETCH_GLYPH:
      x_draw_stretch_glyph_string (s);
      break;

    case CHAR_GLYPH:
      if (s->for_overlaps)
	s->background_filled_p = 1;
      else
	x_draw_glyph_string_background (s, 0);
      x_draw_glyph_string_foreground (s);
      break;

    case COMPOSITE_GLYPH:
      if (s->for_overlaps || (s->cmp_from > 0
			      && ! s->first_glyph->u.cmp.automatic))
	s->background_filled_p = 1;
      else
	x_draw_glyph_string_background (s, 1);
      x_draw_composite_glyph_string_foreground (s);
      break;

    case GLYPHLESS_GLYPH:
      if (s->for_overlaps)
	s->background_filled_p = 1;
      else
	x_draw_glyph_string_background (s, 1);
      x_draw_glyphless_glyph_string_foreground (s);
      break;

    default:
      abort ();
    }

  if (!s->for_overlaps)
    {
      /* Draw underline.  */
      if (s->face->underline_p)
	{
	  unsigned long thickness, position;
	  int y;

	  if (s->prev && s->prev->face->underline_p)
	    {
	      /* We use the same underline style as the previous one.  */
	      thickness = s->prev->underline_thickness;
	      position = s->prev->underline_position;
	    }
	  else
	    {
	      /* Get the underline thickness.  Default is 1 pixel.  */
	      if (s->font && s->font->underline_thickness > 0)
		thickness = s->font->underline_thickness;
	      else
		thickness = 1;
	      if (x_underline_at_descent_line)
		position = (s->height - thickness) - (s->ybase - s->y);
	      else
		{
		  /* Get the underline position.  This is the recommended
		     vertical offset in pixels from the baseline to the top of
		     the underline.  This is a signed value according to the
		     specs, and its default is

		     ROUND ((maximum descent) / 2), with
		     ROUND(x) = floor (x + 0.5)  */

		  if (x_use_underline_position_properties
		      && s->font && s->font->underline_position >= 0)
		    position = s->font->underline_position;
		  else if (s->font)
		    position = (s->font->descent + 1) / 2;
		  else
		    position = underline_minimum_offset;
		}
	      position = max (position, underline_minimum_offset);
	    }
	  /* Check the sanity of thickness and position.  We should
	     avoid drawing underline out of the current line area.  */
	  if (s->y + s->height <= s->ybase + position)
	    position = (s->height - 1) - (s->ybase - s->y);
	  if (s->y + s->height < s->ybase + position + thickness)
	    thickness = (s->y + s->height) - (s->ybase + position);
	  s->underline_thickness = thickness;
	  s->underline_position = position;
	  y = s->ybase + position;
	  if (s->face->underline_defaulted_p)
	    mac_fill_rectangle (s->f, s->gc, s->x, y, s->width, thickness);
	  else
	    {
	      XGCValues xgcv;
	      XGetGCValues (s->display, s->gc, GCForeground, &xgcv);
	      XSetForeground (s->display, s->gc, s->face->underline_color);
	      mac_fill_rectangle (s->f, s->gc, s->x, y, s->width, thickness);
	      XSetForeground (s->display, s->gc, xgcv.foreground);
	    }
	}

      /* Draw overline.  */
      if (s->face->overline_p)
	{
	  unsigned long dy = 0, h = 1;

	  if (s->face->overline_color_defaulted_p)
	    mac_fill_rectangle (s->f, s->gc, s->x, s->y + dy, s->width, h);
	  else
	    {
	      XGCValues xgcv;
	      XGetGCValues (s->display, s->gc, GCForeground, &xgcv);
	      XSetForeground (s->display, s->gc, s->face->overline_color);
	      mac_fill_rectangle (s->f, s->gc, s->x, s->y + dy, s->width, h);
	      XSetForeground (s->display, s->gc, xgcv.foreground);
	    }
	}

      /* Draw strike-through.  */
      if (s->face->strike_through_p)
	{
	  unsigned long h = 1;
	  unsigned long dy = (s->height - h) / 2;

	  if (s->face->strike_through_color_defaulted_p)
	    mac_fill_rectangle (s->f, s->gc, s->x, s->y + dy, s->width, h);
	  else
	    {
	      XGCValues xgcv;
	      XGetGCValues (s->display, s->gc, GCForeground, &xgcv);
	      XSetForeground (s->display, s->gc, s->face->strike_through_color);
	      mac_fill_rectangle (s->f, s->gc, s->x, s->y + dy, s->width, h);
	      XSetForeground (s->display, s->gc, xgcv.foreground);
	    }
	}

      /* Draw relief if not yet drawn.  */
      if (!relief_drawn_p && s->face->box != FACE_NO_BOX)
	x_draw_glyph_string_box (s);

      if (s->prev)
	{
	  struct glyph_string *prev;

	  for (prev = s->prev; prev; prev = prev->prev)
	    if (prev->hl != s->hl
		&& prev->x + prev->width + prev->right_overhang > s->x)
	      {
		/* As prev was drawn while clipped to its own area, we
		   must draw the right_overhang part using s->hl now.  */
		enum draw_glyphs_face save = prev->hl;

		prev->hl = s->hl;
		x_set_glyph_string_gc (prev);
		x_set_glyph_string_clipping_exactly (s, prev);
		if (prev->first_glyph->type == CHAR_GLYPH)
		  x_draw_glyph_string_foreground (prev);
		else
		  x_draw_composite_glyph_string_foreground (prev);
		mac_reset_clip_rectangles (prev->f, prev->gc);
		prev->hl = save;
		prev->num_clips = 0;
	      }
	}

      if (s->next)
	{
	  struct glyph_string *next;

	  for (next = s->next; next; next = next->next)
	    if (next->hl != s->hl
		&& next->x - next->left_overhang < s->x + s->width)
	      {
		/* As next will be drawn while clipped to its own area,
		   we must draw the left_overhang part using s->hl now.  */
		enum draw_glyphs_face save = next->hl;

		next->hl = s->hl;
		x_set_glyph_string_gc (next);
		x_set_glyph_string_clipping_exactly (s, next);
		if (next->first_glyph->type == CHAR_GLYPH)
		  x_draw_glyph_string_foreground (next);
		else
		  x_draw_composite_glyph_string_foreground (next);
		mac_reset_clip_rectangles (next->f, next->gc);
		next->hl = save;
		next->num_clips = 0;
	      }
	}
    }

  /* Reset clipping.  */
  mac_reset_clip_rectangles (s->f, s->gc);
  s->num_clips = 0;
}

/* Shift display to make room for inserted glyphs.   */

static void
mac_shift_glyphs_for_insert (struct frame *f, int x, int y, int width, int height, int shift_by)
{
  mac_scroll_area (f, f->output_data.mac->normal_gc,
		   x, y, width, height,
		   x + shift_by, y);
}

/* Delete N glyphs at the nominal cursor position.  Not implemented
   for X frames.  */

static void
x_delete_glyphs (struct frame *f, register int n)
{
  abort ();
}


/* Clear an entire frame.  */

static void
x_clear_frame (struct frame *f)
{
  /* Clearing the frame will erase any cursor, so mark them all as no
     longer visible.  */
  mark_window_cursors_off (XWINDOW (FRAME_ROOT_WINDOW (f)));
  output_cursor.hpos = output_cursor.vpos = 0;
  output_cursor.x = -1;

  /* We don't set the output cursor here because there will always
     follow an explicit cursor_to.  */
  BLOCK_INPUT;

  mac_clear_window (f);

  /* We have to clear the scroll bars.  If we have changed colors or
     something like that, then they should be notified.  */
  x_scroll_bar_clear (f);

  XFlush (FRAME_MAC_DISPLAY (f));
  UNBLOCK_INPUT;
}



/* Invert the middle quarter of the frame for .15 sec.  */

/* We use the select system call to do the waiting, so we have to make
   sure it's available.  If it isn't, we just won't do visual bells.  */

#if defined (HAVE_TIMEVAL) && defined (HAVE_SELECT)


/* Subtract the `struct timeval' values X and Y, storing the result in
   *RESULT.  Return 1 if the difference is negative, otherwise 0.  */

static int
timeval_subtract (struct timeval *result, struct timeval x, struct timeval y)
{
  /* Perform the carry for the later subtraction by updating y.  This
     is safer because on some systems the tv_sec member is unsigned.  */
  if (x.tv_usec < y.tv_usec)
    {
      int nsec = (y.tv_usec - x.tv_usec) / 1000000 + 1;
      y.tv_usec -= 1000000 * nsec;
      y.tv_sec += nsec;
    }

  if (x.tv_usec - y.tv_usec > 1000000)
    {
      int nsec = (y.tv_usec - x.tv_usec) / 1000000;
      y.tv_usec += 1000000 * nsec;
      y.tv_sec -= nsec;
    }

  /* Compute the time remaining to wait.  tv_usec is certainly
     positive.  */
  result->tv_sec = x.tv_sec - y.tv_sec;
  result->tv_usec = x.tv_usec - y.tv_usec;

  /* Return indication of whether the result should be considered
     negative.  */
  return x.tv_sec < y.tv_sec;
}

static void
XTflash (struct frame *f)
{
  /* Get the height not including a menu bar widget.  */
  int height = FRAME_TEXT_LINES_TO_PIXEL_HEIGHT (f, FRAME_LINES (f));
  /* Height of each line to flash.  */
  int flash_height = FRAME_LINE_HEIGHT (f);
  /* These will be the left and right margins of the rectangles.  */
  int flash_left = FRAME_INTERNAL_BORDER_WIDTH (f);
  int flash_right = (FRAME_TEXT_COLS_TO_PIXEL_WIDTH (f, FRAME_COLS (f))
		     - FRAME_INTERNAL_BORDER_WIDTH (f));
  int width;
  NativeRectangle rects[2];
  int nrects;

  /* Don't flash the area between a scroll bar and the frame
     edge it is next to.  */
  switch (FRAME_VERTICAL_SCROLL_BAR_TYPE (f))
    {
    case vertical_scroll_bar_left:
      flash_left += VERTICAL_SCROLL_BAR_WIDTH_TRIM;
      break;

    case vertical_scroll_bar_right:
      flash_right -= VERTICAL_SCROLL_BAR_WIDTH_TRIM;
      break;

    default:
      break;
    }

  width = flash_right - flash_left;

  if (height > 3 * FRAME_LINE_HEIGHT (f))
    {
      /* If window is tall, flash top and bottom line.  */
      STORE_NATIVE_RECT (rects[0],
			 flash_left, (FRAME_INTERNAL_BORDER_WIDTH (f)
				      + FRAME_TOP_MARGIN_HEIGHT (f)),
			 width, flash_height);
      rects[1] = rects[0];
      rects[1].y = height - flash_height - FRAME_INTERNAL_BORDER_WIDTH (f);
      nrects = 2;
    }
  else
    {
      /* If it is short, flash it all.  */
      STORE_NATIVE_RECT (rects[0],
			 flash_left, FRAME_INTERNAL_BORDER_WIDTH (f),
			 width, height - 2 * FRAME_INTERNAL_BORDER_WIDTH (f));
      nrects = 1;
    }

  BLOCK_INPUT;

  mac_invert_rectangles (f, rects, nrects);

  x_flush (f);

  {
    struct timeval wakeup;

    EMACS_GET_TIME (wakeup);

    /* Compute time to wait until, propagating carry from usecs.  */
    wakeup.tv_usec += 150000;
    wakeup.tv_sec += (wakeup.tv_usec / 1000000);
    wakeup.tv_usec %= 1000000;

    /* Keep waiting until past the time wakeup or any input gets
       available.  */
    while (! detect_input_pending ())
      {
	struct timeval current;
	struct timeval timeout;

	EMACS_GET_TIME (current);

	/* Break if result would be negative.  */
	if (timeval_subtract (&current, wakeup, current))
	  break;

	/* How long `select' should wait.  */
	timeout.tv_sec = 0;
	timeout.tv_usec = 10000;

	/* Try to wait that long--but we might wake up sooner.  */
	select (0, NULL, NULL, NULL, &timeout);
      }
  }

  mac_invert_rectangles (f, rects, nrects);

  x_flush (f);

  UNBLOCK_INPUT;
}

#endif /* defined (HAVE_TIMEVAL) && defined (HAVE_SELECT) */


/* Make audible bell.  */

static void
XTring_bell (struct frame *f)
{
#if defined (HAVE_TIMEVAL) && defined (HAVE_SELECT)
  if (visible_bell)
    XTflash (f);
  else
#endif
    {
      BLOCK_INPUT;
      mac_alert_sound_play ();
      XFlush (FRAME_MAC_DISPLAY (f));
      UNBLOCK_INPUT;
    }
}


/* Specify how many text lines, from the top of the window,
   should be affected by insert-lines and delete-lines operations.
   This, and those operations, are used only within an update
   that is bounded by calls to x_update_begin and x_update_end.  */

static void
XTset_terminal_window (struct frame *f, int n)
{
  /* This function intentionally left blank.  */
}



/***********************************************************************
			      Line Dance
 ***********************************************************************/

/* Perform an insert-lines or delete-lines operation, inserting N
   lines or deleting -N lines at vertical position VPOS.  */

static void
x_ins_del_lines (struct frame *f, int vpos, int n)
{
  abort ();
}


/* Scroll part of the display as described by RUN.  */

static void
x_scroll_run (struct window *w, struct run *run)
{
  struct frame *f = XFRAME (w->frame);
  int x, y, width, height, from_y, to_y, bottom_y;

  /* Get frame-relative bounding box of the text display area of W,
     without mode lines.  Include in this box the left and right
     fringe of W.  */
  window_box (w, -1, &x, &y, &width, &height);

  /* If the fringe is adjacent to the left (right) scroll bar of a
     leftmost (rightmost, respectively) window, then extend its
     background to the gap between the fringe and the bar.  */
  if ((WINDOW_LEFTMOST_P (w)
       && WINDOW_HAS_VERTICAL_SCROLL_BAR_ON_LEFT (w))
      || (WINDOW_RIGHTMOST_P (w)
	  && WINDOW_HAS_VERTICAL_SCROLL_BAR_ON_RIGHT (w)))
    {
      int sb_width = WINDOW_CONFIG_SCROLL_BAR_WIDTH (w);

      if (sb_width > 0)
	{
	  int bar_area_x = WINDOW_SCROLL_BAR_AREA_X (w);
	  int bar_area_width = (WINDOW_CONFIG_SCROLL_BAR_COLS (w)
				* FRAME_COLUMN_WIDTH (f));

	  if (bar_area_x + bar_area_width == x)
	    {
	      x = bar_area_x + sb_width;
	      width += bar_area_width - sb_width;
	    }
	  else if (x + width == bar_area_x)
	    width += bar_area_width - sb_width;
	}
    }

  from_y = WINDOW_TO_FRAME_PIXEL_Y (w, run->current_y);
  to_y = WINDOW_TO_FRAME_PIXEL_Y (w, run->desired_y);
  bottom_y = y + height;

  if (to_y < from_y)
    {
      /* Scrolling up.  Make sure we don't copy part of the mode
	 line at the bottom.  */
      if (from_y + run->height > bottom_y)
	height = bottom_y - from_y;
      else
	height = run->height;
    }
  else
    {
      /* Scrolling down.  Make sure we don't copy over the mode line.
	 at the bottom.  */
      if (to_y + run->height > bottom_y)
	height = bottom_y - to_y;
      else
	height = run->height;
    }

  BLOCK_INPUT;

  /* Cursor off.  Will be switched on again in x_update_window_end.  */
  updated_window = w;
  x_clear_cursor (w);

  mac_scroll_area (f, f->output_data.mac->normal_gc,
		   x, from_y,
		   width, height,
		   x, to_y);

  UNBLOCK_INPUT;
}



/***********************************************************************
			   Exposure Events
 ***********************************************************************/


static void
frame_highlight (struct frame *f)
{
  x_update_cursor (f, 1);
  BLOCK_INPUT;
  x_set_frame_alpha (f);
  UNBLOCK_INPUT;
}

static void
frame_unhighlight (struct frame *f)
{
  x_update_cursor (f, 1);
  BLOCK_INPUT;
  x_set_frame_alpha (f);
  UNBLOCK_INPUT;
}

/* The focus has changed.  Update the frames as necessary to reflect
   the new situation.  Note that we can't change the selected frame
   here, because the Lisp code we are interrupting might become confused.
   Each event gets marked with the frame in which it occurred, so the
   Lisp code can tell when the switch took place by examining the events.  */

static void
x_new_focus_frame (struct x_display_info *dpyinfo, struct frame *frame)
{
  struct frame *old_focus = dpyinfo->x_focus_frame;

  if (frame != dpyinfo->x_focus_frame)
    {
      /* Set this before calling other routines, so that they see
	 the correct value of x_focus_frame.  */
      dpyinfo->x_focus_frame = frame;

      if (old_focus && old_focus->auto_lower)
	x_lower_frame (old_focus);

      if (dpyinfo->x_focus_frame && dpyinfo->x_focus_frame->auto_raise)
	pending_autoraise_frame = dpyinfo->x_focus_frame;
      else
	pending_autoraise_frame = 0;

      if (frame)
	mac_set_font_info_for_selection (frame, DEFAULT_FACE_ID, 0, -1, Qnil);
    }

  x_frame_rehighlight (dpyinfo);
}

/* Handle FocusIn and FocusOut state changes for FRAME.
   If FRAME has focus and there exists more than one frame, puts
   a FOCUS_IN_EVENT into *BUFP.  */

void
mac_focus_changed (int type, struct mac_display_info *dpyinfo, struct frame *frame, struct input_event *bufp)
{
  if (type == activeFlag)
    {
      if (dpyinfo->x_focus_event_frame != frame)
        {
          x_new_focus_frame (dpyinfo, frame);
          dpyinfo->x_focus_event_frame = frame;

          /* Don't stop displaying the initial startup message
             for a switch-frame event we don't need.  */
          if (NILP (Vterminal_frame)
              && CONSP (Vframe_list)
              && !NILP (XCDR (Vframe_list)))
            {
              bufp->kind = FOCUS_IN_EVENT;
              XSETFRAME (bufp->frame_or_window, frame);
            }
        }
    }
  else
    {
      if (dpyinfo->x_focus_event_frame == frame)
        {
          dpyinfo->x_focus_event_frame = 0;
          x_new_focus_frame (dpyinfo, 0);
        }
    }
}

/* Handle an event saying the mouse has moved out of an Emacs frame.  */

void
x_mouse_leave (struct x_display_info *dpyinfo)
{
  x_new_focus_frame (dpyinfo, dpyinfo->x_focus_event_frame);
}

/* The focus has changed, or we have redirected a frame's focus to
   another frame (this happens when a frame uses a surrogate
   mini-buffer frame).  Shift the highlight as appropriate.

   The FRAME argument doesn't necessarily have anything to do with which
   frame is being highlighted or un-highlighted; we only use it to find
   the appropriate X display info.  */

static void
XTframe_rehighlight (struct frame *frame)
{
  x_frame_rehighlight (FRAME_X_DISPLAY_INFO (frame));
}

static void
x_frame_rehighlight (struct x_display_info *dpyinfo)
{
  struct frame *old_highlight = dpyinfo->x_highlight_frame;

  if (dpyinfo->x_focus_frame)
    {
      dpyinfo->x_highlight_frame
	= ((FRAMEP (FRAME_FOCUS_FRAME (dpyinfo->x_focus_frame)))
	   ? XFRAME (FRAME_FOCUS_FRAME (dpyinfo->x_focus_frame))
	   : dpyinfo->x_focus_frame);
      if (! FRAME_LIVE_P (dpyinfo->x_highlight_frame))
	{
	  FRAME_FOCUS_FRAME (dpyinfo->x_focus_frame) = Qnil;
	  dpyinfo->x_highlight_frame = dpyinfo->x_focus_frame;
	}
    }
  else
    dpyinfo->x_highlight_frame = 0;

  if (dpyinfo->x_highlight_frame != old_highlight)
    {
      if (old_highlight)
	frame_unhighlight (old_highlight);
      if (dpyinfo->x_highlight_frame)
	frame_highlight (dpyinfo->x_highlight_frame);
    }
}



/* Convert a keysym to its name.  */

char *
x_get_keysym_name (int keysym)
{
  char *value;

  BLOCK_INPUT;
#if 0
  value = XKeysymToString (keysym);
#else
  value = 0;
#endif
  UNBLOCK_INPUT;

  return value;
}



/************************************************************************
			      Mouse Face
 ************************************************************************/

struct frame *
mac_focus_frame (struct mac_display_info *dpyinfo)
{
  if (dpyinfo->x_focus_frame)
    return dpyinfo->x_focus_frame;
  else
    /* Mac version may get events, such as a menu bar click, even when
       all the frames are invisible.  In this case, we regard the
       event came to the selected frame.  */
    return SELECTED_FRAME ();
}


/* Return the current position of the mouse.
   *FP should be a frame which indicates which display to ask about.

   If the mouse movement started in a scroll bar, set *FP, *BAR_WINDOW,
   and *PART to the frame, window, and scroll bar part that the mouse
   is over.  Set *X and *Y to the portion and whole of the mouse's
   position on the scroll bar.

   If the mouse movement started elsewhere, set *FP to the frame the
   mouse is on, *BAR_WINDOW to nil, and *X and *Y to the character cell
   the mouse is over.

   Set *TIMESTAMP to the server time-stamp for the time at which the mouse
   was at this position.

   Don't store anything if we don't have a valid set of values to report.

   This clears the mouse_moved flag, so we can wait for the next mouse
   movement.  */

static void
XTmouse_position (FRAME_PTR *fp, int insist, Lisp_Object *bar_window,
		  enum scroll_bar_part *part, Lisp_Object *x, Lisp_Object *y,
		  Time *timestamp)
{
  FRAME_PTR f1;

  BLOCK_INPUT;

  {
    Lisp_Object frame, tail;

    /* Clear the mouse-moved flag for every frame on this display.  */
    FOR_EACH_FRAME (tail, frame)
      if (FRAME_MAC_P (XFRAME (frame)))
	XFRAME (frame)->mouse_moved = 0;

    if (FRAME_MAC_DISPLAY_INFO (*fp)->grabbed && last_mouse_frame
	&& FRAME_LIVE_P (last_mouse_frame))
      f1 = last_mouse_frame;
    else
      f1 = mac_focus_frame (FRAME_MAC_DISPLAY_INFO (*fp));

    if (f1)
      {
	/* Ok, we found a frame.  Store all the values.
	   last_mouse_glyph is a rectangle used to reduce the
	   generation of mouse events.  To not miss any motion events,
	   we must divide the frame into rectangles of the size of the
	   smallest character that could be displayed on it, i.e. into
	   the same rectangles that matrices on the frame are divided
	   into.  */
	Point mouse_pos;

	mac_get_frame_mouse (f1, &mouse_pos);
	remember_mouse_glyph (f1, mouse_pos.h, mouse_pos.v,
			      &last_mouse_glyph);
	last_mouse_glyph_frame = f1;

	*bar_window = Qnil;
	*part = 0;
	*fp = f1;
	XSETINT (*x, mouse_pos.h);
	XSETINT (*y, mouse_pos.v);
	*timestamp = last_mouse_movement_time;
      }
  }

  UNBLOCK_INPUT;
}


/************************************************************************
			 Scroll bars, general
 ************************************************************************/

/* Create a scroll bar and return the scroll bar vector for it.  W is
   the Emacs window on which to create the scroll bar. TOP, LEFT,
   WIDTH and HEIGHT are the pixel coordinates and dimensions of the
   scroll bar. */

static struct scroll_bar *
x_scroll_bar_create (struct window *w, int top, int left, int width, int height)
{
  struct frame *f = XFRAME (w->frame);
  struct scroll_bar *bar
    = ALLOCATE_PSEUDOVECTOR (struct scroll_bar, mac_control_ref, PVEC_OTHER);

  BLOCK_INPUT;

  XSETWINDOW (bar->window, w);
  bar->top = top;
  bar->left = left;
  bar->width = width;
  bar->height = height;
  bar->fringe_extended_p = 0;
  bar->redraw_needed_p = 0;

  mac_create_scroll_bar (bar);

  /* Add bar to its frame's list of scroll bars.  */
  bar->next = FRAME_SCROLL_BARS (f);
  bar->prev = Qnil;
  XSETVECTOR (FRAME_SCROLL_BARS (f), bar);
  if (!NILP (bar->next))
    XSETVECTOR (XSCROLL_BAR (bar->next)->prev, bar);

  UNBLOCK_INPUT;
  return bar;
}


/* Destroy scroll bar BAR, and set its Emacs window's scroll bar to
   nil.  */

static void
x_scroll_bar_remove (struct scroll_bar *bar)
{
  BLOCK_INPUT;

  /* Destroy the Mac scroll bar control  */
  mac_dispose_scroll_bar (bar);

  /* Dissociate this scroll bar from its window.  */
  XWINDOW (bar->window)->vertical_scroll_bar = Qnil;

  UNBLOCK_INPUT;
}


/* Set the handle of the vertical scroll bar for WINDOW to indicate
   that we are displaying PORTION characters out of a total of WHOLE
   characters, starting at POSITION.  If WINDOW has no scroll bar,
   create one.  */

static void
XTset_vertical_scroll_bar (struct window *w, int portion, int whole, int position)
{
  struct frame *f = XFRAME (w->frame);
  struct scroll_bar *bar;
  int top, height, left, sb_left, width, sb_width;
  int window_y, window_height;
  int fringe_extended_p;

  /* Get window dimensions.  */
  window_box (w, -1, 0, &window_y, 0, &window_height);
  top = window_y;
  width = WINDOW_CONFIG_SCROLL_BAR_COLS (w) * FRAME_COLUMN_WIDTH (f);
  height = window_height;

  /* Compute the left edge of the scroll bar area.  */
  left = WINDOW_SCROLL_BAR_AREA_X (w);

  /* Compute the width of the scroll bar which might be less than
     the width of the area reserved for the scroll bar.  */
  if (WINDOW_CONFIG_SCROLL_BAR_WIDTH (w) > 0)
    sb_width = WINDOW_CONFIG_SCROLL_BAR_WIDTH (w);
  else
    sb_width = width;

  /* Compute the left edge of the scroll bar.  */
  if (WINDOW_HAS_VERTICAL_SCROLL_BAR_ON_RIGHT (w))
    sb_left = left + (WINDOW_RIGHTMOST_P (w) ? width - sb_width : 0);
  else
    sb_left = left + (WINDOW_LEFTMOST_P (w) ? 0 : width - sb_width);

  if (WINDOW_HAS_VERTICAL_SCROLL_BAR_ON_LEFT (w))
    fringe_extended_p = (WINDOW_LEFTMOST_P (w)
			 && WINDOW_LEFT_FRINGE_WIDTH (w)
			 && (WINDOW_HAS_FRINGES_OUTSIDE_MARGINS (w)
			     || WINDOW_LEFT_MARGIN_COLS (w) == 0));
  else
    fringe_extended_p = (WINDOW_RIGHTMOST_P (w)
			 && WINDOW_RIGHT_FRINGE_WIDTH (w)
			 && (WINDOW_HAS_FRINGES_OUTSIDE_MARGINS (w)
			     || WINDOW_RIGHT_MARGIN_COLS (w) == 0));

  /* Does the scroll bar exist yet?  */
  if (NILP (w->vertical_scroll_bar))
    {
      BLOCK_INPUT;
      if (fringe_extended_p)
	mac_clear_area (f, sb_left, top, sb_width, height);
      else
	mac_clear_area (f, left, top, width, height);
      UNBLOCK_INPUT;
      bar = x_scroll_bar_create (w, top, sb_left, sb_width, height);
      XSETVECTOR (w->vertical_scroll_bar, bar);
    }
  else
    {
      /* It may just need to be moved and resized.  */
      bar = XSCROLL_BAR (w->vertical_scroll_bar);

      BLOCK_INPUT;

      /* If already correctly positioned, do nothing.  */
      if (bar->left == sb_left && bar->top == top
	  && bar->width == sb_width && bar->height == height
	  && bar->fringe_extended_p == fringe_extended_p)
	{
	  if (bar->redraw_needed_p)
	    mac_redraw_scroll_bar (bar);
	}
      else
	{
	  /* Since toolkit scroll bars are smaller than the space reserved
	     for them on the frame, we have to clear "under" them.  */
	  if (fringe_extended_p)
	    mac_clear_area (f, sb_left, top, sb_width, height);
	  else
	    mac_clear_area (f, left, top, width, height);

          /* Remember new settings.  */
          bar->left = sb_left;
          bar->top = top;
          bar->width = sb_width;
          bar->height = height;

	  mac_update_scroll_bar_bounds (bar);
        }

      UNBLOCK_INPUT;
    }

  bar->fringe_extended_p = fringe_extended_p;
  bar->redraw_needed_p = 0;

  x_set_toolkit_scroll_bar_thumb (bar, portion, position, whole);
}


/* The following three hooks are used when we're doing a thorough
   redisplay of the frame.  We don't explicitly know which scroll bars
   are going to be deleted, because keeping track of when windows go
   away is a real pain - "Can you say set-window-configuration, boys
   and girls?"  Instead, we just assert at the beginning of redisplay
   that *all* scroll bars are to be removed, and then save a scroll bar
   from the fiery pit when we actually redisplay its window.  */

/* Arrange for all scroll bars on FRAME to be removed at the next call
   to `*judge_scroll_bars_hook'.  A scroll bar may be spared if
   `*redeem_scroll_bar_hook' is applied to its window before the judgment.  */

static void
XTcondemn_scroll_bars (FRAME_PTR frame)
{
  /* Transfer all the scroll bars to FRAME_CONDEMNED_SCROLL_BARS.  */
  while (! NILP (FRAME_SCROLL_BARS (frame)))
    {
      Lisp_Object bar;
      bar = FRAME_SCROLL_BARS (frame);
      FRAME_SCROLL_BARS (frame) = XSCROLL_BAR (bar)->next;
      XSCROLL_BAR (bar)->next = FRAME_CONDEMNED_SCROLL_BARS (frame);
      XSCROLL_BAR (bar)->prev = Qnil;
      if (! NILP (FRAME_CONDEMNED_SCROLL_BARS (frame)))
	XSCROLL_BAR (FRAME_CONDEMNED_SCROLL_BARS (frame))->prev = bar;
      FRAME_CONDEMNED_SCROLL_BARS (frame) = bar;
    }
}


/* Un-mark WINDOW's scroll bar for deletion in this judgment cycle.
   Note that WINDOW isn't necessarily condemned at all.  */

static void
XTredeem_scroll_bar (struct window *window)
{
  struct scroll_bar *bar;
  struct frame *f;

  /* We can't redeem this window's scroll bar if it doesn't have one.  */
  if (NILP (window->vertical_scroll_bar))
    abort ();

  bar = XSCROLL_BAR (window->vertical_scroll_bar);

  /* Unlink it from the condemned list.  */
  f = XFRAME (WINDOW_FRAME (window));
  if (NILP (bar->prev))
    {
      /* If the prev pointer is nil, it must be the first in one of
	 the lists.  */
      if (EQ (FRAME_SCROLL_BARS (f), window->vertical_scroll_bar))
	/* It's not condemned.  Everything's fine.  */
	return;
      else if (EQ (FRAME_CONDEMNED_SCROLL_BARS (f),
		   window->vertical_scroll_bar))
	FRAME_CONDEMNED_SCROLL_BARS (f) = bar->next;
      else
	/* If its prev pointer is nil, it must be at the front of
	   one or the other!  */
	abort ();
    }
  else
    XSCROLL_BAR (bar->prev)->next = bar->next;

  if (! NILP (bar->next))
    XSCROLL_BAR (bar->next)->prev = bar->prev;

  bar->next = FRAME_SCROLL_BARS (f);
  bar->prev = Qnil;
  XSETVECTOR (FRAME_SCROLL_BARS (f), bar);
  if (! NILP (bar->next))
    XSETVECTOR (XSCROLL_BAR (bar->next)->prev, bar);
}

/* Remove all scroll bars on FRAME that haven't been saved since the
   last call to `*condemn_scroll_bars_hook'.  */

static void
XTjudge_scroll_bars (FRAME_PTR f)
{
  Lisp_Object bar, next;

  bar = FRAME_CONDEMNED_SCROLL_BARS (f);

  /* Clear out the condemned list now so we won't try to process any
     more events on the hapless scroll bars.  */
  FRAME_CONDEMNED_SCROLL_BARS (f) = Qnil;

  for (; ! NILP (bar); bar = next)
    {
      struct scroll_bar *b = XSCROLL_BAR (bar);

      x_scroll_bar_remove (b);

      next = b->next;
      b->next = b->prev = Qnil;
    }

  /* Now there should be no references to the condemned scroll bars,
     and they should get garbage-collected.  */
}

/* The screen has been cleared so we may have changed foreground or
   background colors, and the scroll bars may need to be redrawn.
   Clear out the scroll bars, and ask for expose events, so we can
   redraw them.  */

void
x_scroll_bar_clear (FRAME_PTR f)
{
  Lisp_Object bar;

  /* We can have scroll bars even if this is 0,
     if we just turned off scroll bar mode.
     But in that case we should not clear them.  */
  if (FRAME_HAS_VERTICAL_SCROLL_BARS (f))
    for (bar = FRAME_SCROLL_BARS (f); !NILP (bar);
	 bar = XSCROLL_BAR (bar)->next)
      XSCROLL_BAR (bar)->redraw_needed_p = 1;
}


/***********************************************************************
			       Tool-bars
 ***********************************************************************/

void
mac_move_window_to_gravity_reference_point (struct frame *f, int win_gravity,
					    short x, short y)
{
  NativeRectangle bounds;
  short left, top;

  mac_get_window_structure_bounds (f, &bounds);

  switch (win_gravity)
    {
    case NorthWestGravity:
    case WestGravity:
    case SouthWestGravity:
      left = x;
      break;

    case NorthGravity:
    case CenterGravity:
    case SouthGravity:
      left = x - bounds.width / 2;
      break;

    case NorthEastGravity:
    case EastGravity:
    case SouthEastGravity:
      left = x - bounds.width;
      break;
    }

  switch (win_gravity)
    {
    case NorthWestGravity:
    case NorthGravity:
    case NorthEastGravity:
      top = y;
      break;

    case WestGravity:
    case CenterGravity:
    case EastGravity:
      top = y - bounds.height / 2;
      break;

    case SouthWestGravity:
    case SouthGravity:
    case SouthEastGravity:
      top = y - bounds.height;
      break;
    }

  mac_move_frame_window_structure (f, left, top);
}

void
mac_get_window_gravity_reference_point (struct frame *f, int win_gravity,
					short *x, short *y)
{
  NativeRectangle bounds;

  mac_get_window_structure_bounds (f, &bounds);

  switch (win_gravity)
    {
    case NorthWestGravity:
    case WestGravity:
    case SouthWestGravity:
      *x = bounds.x;
      break;

    case NorthGravity:
    case CenterGravity:
    case SouthGravity:
      *x = bounds.x + bounds.width / 2;
      break;

    case NorthEastGravity:
    case EastGravity:
    case SouthEastGravity:
      *x = bounds.x + bounds.width;
      break;
    }

  switch (win_gravity)
    {
    case NorthWestGravity:
    case NorthGravity:
    case NorthEastGravity:
      *y = bounds.y;
      break;

    case WestGravity:
    case CenterGravity:
    case EastGravity:
      *y = bounds.y + bounds.height / 2;
      break;

    case SouthWestGravity:
    case SouthGravity:
    case SouthEastGravity:
      *y = bounds.y + bounds.height;
      break;
    }
}

CGImageRef
mac_image_spec_to_cg_image (struct frame *f, Lisp_Object image)
{
  if (!valid_image_p (image))
    return NULL;
  else
    {
      int img_id = lookup_image (f, image);
      struct image *img = IMAGE_FROM_ID (f, img_id);

      prepare_image_for_display (f, img);

      return img->cg_image;
    }
}


/***********************************************************************
			     Text Cursor
 ***********************************************************************/

/* Set clipping for output in glyph row ROW.  W is the window in which
   we operate.  GC is the graphics context to set clipping in.

   ROW may be a text row or, e.g., a mode line.  Text rows must be
   clipped to the interior of the window dedicated to text display,
   mode lines must be clipped to the whole window.  */

static void
x_clip_to_row (struct window *w, struct glyph_row *row, int area, GC gc)
{
  struct frame *f = XFRAME (WINDOW_FRAME (w));
  NativeRectangle clip_rect;
  int window_x, window_y, window_width;

  window_box (w, area, &window_x, &window_y, &window_width, 0);

  clip_rect.x = window_x;
  clip_rect.y = WINDOW_TO_FRAME_PIXEL_Y (w, max (0, row->y));
  clip_rect.y = max (clip_rect.y, window_y);
  clip_rect.width = window_width;
  clip_rect.height = row->visible_height;

  mac_set_clip_rectangles (f, gc, &clip_rect, 1);
}


/* Draw a hollow box cursor on window W in glyph row ROW.  */

static void
x_draw_hollow_cursor (struct window *w, struct glyph_row *row)
{
  struct frame *f = XFRAME (WINDOW_FRAME (w));
  struct mac_display_info *dpyinfo = FRAME_MAC_DISPLAY_INFO (f);
  Display *dpy = FRAME_MAC_DISPLAY (f);
  int x, y, wd, h;
  XGCValues xgcv;
  struct glyph *cursor_glyph;
  GC gc;

  /* Get the glyph the cursor is on.  If we can't tell because
     the current matrix is invalid or such, give up.  */
  cursor_glyph = get_phys_cursor_glyph (w);
  if (cursor_glyph == NULL)
    return;

  /* Compute frame-relative coordinates for phys cursor.  */
  get_phys_cursor_geometry (w, row, cursor_glyph, &x, &y, &h);
  wd = w->phys_cursor_width;

  /* The foreground of cursor_gc is typically the same as the normal
     background color, which can cause the cursor box to be invisible.  */
  xgcv.foreground = f->output_data.mac->cursor_pixel;
  if (dpyinfo->scratch_cursor_gc)
    XChangeGC (dpy, dpyinfo->scratch_cursor_gc, GCForeground, &xgcv);
  else
    dpyinfo->scratch_cursor_gc = XCreateGC (dpy, FRAME_MAC_WINDOW (f),
					    GCForeground, &xgcv);
  gc = dpyinfo->scratch_cursor_gc;

  /* Set clipping, draw the rectangle, and reset clipping again.  */
  x_clip_to_row (w, row, TEXT_AREA, gc);
  mac_draw_rectangle (f, gc, x, y, wd, h - 1);
  mac_reset_clip_rectangles (f, gc);
}


/* Draw a bar cursor on window W in glyph row ROW.

   Implementation note: One would like to draw a bar cursor with an
   angle equal to the one given by the font property XA_ITALIC_ANGLE.
   Unfortunately, I didn't find a font yet that has this property set.
   --gerd.  */

static void
x_draw_bar_cursor (struct window *w, struct glyph_row *row, int width, enum text_cursor_kinds kind)
{
  struct frame *f = XFRAME (w->frame);
  struct glyph *cursor_glyph;

  /* If cursor is out of bounds, don't draw garbage.  This can happen
     in mini-buffer windows when switching between echo area glyphs
     and mini-buffer.  */
  cursor_glyph = get_phys_cursor_glyph (w);
  if (cursor_glyph == NULL)
    return;

  /* If on an image, draw like a normal cursor.  That's usually better
     visible than drawing a bar, esp. if the image is large so that
     the bar might not be in the window.  */
  if (cursor_glyph->type == IMAGE_GLYPH)
    {
      struct glyph_row *r;
      r = MATRIX_ROW (w->current_matrix, w->phys_cursor.vpos);
      draw_phys_cursor_glyph (w, r, DRAW_CURSOR);
    }
  else
    {
      Display *dpy = FRAME_MAC_DISPLAY (f);
      Window window = FRAME_MAC_WINDOW (f);
      GC gc = FRAME_MAC_DISPLAY_INFO (f)->scratch_cursor_gc;
      unsigned long mask = GCForeground | GCBackground;
      struct face *face = FACE_FROM_ID (f, cursor_glyph->face_id);
      XGCValues xgcv;

      /* If the glyph's background equals the color we normally draw
	 the bars cursor in, the bar cursor in its normal color is
	 invisible.  Use the glyph's foreground color instead in this
	 case, on the assumption that the glyph's colors are chosen so
	 that the glyph is legible.  */
      if (face->background == f->output_data.mac->cursor_pixel)
	xgcv.background = xgcv.foreground = face->foreground;
      else
	xgcv.background = xgcv.foreground = f->output_data.mac->cursor_pixel;

      if (gc)
	XChangeGC (dpy, gc, mask, &xgcv);
      else
	{
	  gc = XCreateGC (dpy, window, mask, &xgcv);
	  FRAME_MAC_DISPLAY_INFO (f)->scratch_cursor_gc = gc;
	}

      x_clip_to_row (w, row, TEXT_AREA, gc);

      if (kind == BAR_CURSOR)
	{
	  int x = WINDOW_TEXT_TO_FRAME_PIXEL_X (w, w->phys_cursor.x);

	  if (width < 0)
	    width = FRAME_CURSOR_WIDTH (f);
	  width = min (cursor_glyph->pixel_width, width);

	  w->phys_cursor_width = width;

	  /* If the character under cursor is R2L, draw the bar cursor
	     on the right of its glyph, rather than on the left.  */
	  if ((cursor_glyph->resolved_level & 1) != 0)
	    x += cursor_glyph->pixel_width - width;

	  mac_fill_rectangle (f, gc, x,
			      WINDOW_TO_FRAME_PIXEL_Y (w, w->phys_cursor.y),
			      width, row->height);
	}
      else
	{
	  int dummy_x, dummy_y, dummy_h;

	  if (width < 0)
	    width = row->height;

	  width = min (row->height, width);

	  get_phys_cursor_geometry (w, row, cursor_glyph, &dummy_x,
				    &dummy_y, &dummy_h);

	  mac_fill_rectangle (f, gc,
			      WINDOW_TEXT_TO_FRAME_PIXEL_X (w, w->phys_cursor.x),
			      WINDOW_TO_FRAME_PIXEL_Y (w, w->phys_cursor.y +
						       row->height - width),
			      w->phys_cursor_width, width);
	}

      mac_reset_clip_rectangles (f, gc);
    }
}


/* RIF: Define cursor CURSOR on frame F.  */

static void
mac_define_frame_cursor (struct frame *f, Cursor cursor)
{
#if MAC_OS_X_VERSION_MAX_ALLOWED >= 1050
#if MAC_OS_X_VERSION_MIN_REQUIRED < 1060
  if (mac_tracking_area_works_with_cursor_rects_invalidation_p ())
#endif
    {
      if (f->output_data.mac->current_cursor != cursor)
	{
	  f->output_data.mac->current_cursor = cursor;
	  mac_invalidate_frame_cursor_rects (f);
	}
    }
#if MAC_OS_X_VERSION_MIN_REQUIRED < 1060
  else
#endif
#endif
#if MAC_OS_X_VERSION_MIN_REQUIRED < 1060
    {
      struct mac_display_info *dpyinfo = FRAME_MAC_DISPLAY_INFO (f);

      if (dpyinfo->x_focus_frame == f)
	SetThemeCursor (cursor);
    }
#endif
}


/* RIF: Clear area on frame F.  */

static void
mac_clear_frame_area (struct frame *f, int x, int y, int width, int height)
{
  mac_clear_area (f, x, y, width, height);
}


/* RIF: Draw cursor on window W.  */

static void
mac_draw_window_cursor (struct window *w, struct glyph_row *glyph_row, int x, int y, int cursor_type, int cursor_width, int on_p, int active_p)
{
  if (on_p)
    {
      w->phys_cursor_type = cursor_type;
      w->phys_cursor_on_p = 1;

      if (glyph_row->exact_window_width_line_p
	  && (glyph_row->reversed_p
	      ? (w->phys_cursor.hpos < 0)
	      : (w->phys_cursor.hpos >= glyph_row->used[TEXT_AREA])))
	{
	  glyph_row->cursor_in_fringe_p = 1;
	  draw_fringe_bitmap (w, glyph_row, glyph_row->reversed_p);
	}
      else
	{
	  switch (cursor_type)
	    {
	    case HOLLOW_BOX_CURSOR:
	      x_draw_hollow_cursor (w, glyph_row);
	      break;

	    case FILLED_BOX_CURSOR:
	      draw_phys_cursor_glyph (w, glyph_row, DRAW_CURSOR);
	      break;

	    case BAR_CURSOR:
	      x_draw_bar_cursor (w, glyph_row, cursor_width, BAR_CURSOR);
	      break;

	    case HBAR_CURSOR:
	      x_draw_bar_cursor (w, glyph_row, cursor_width, HBAR_CURSOR);
	      break;

	    case NO_CURSOR:
	      w->phys_cursor_width = 0;
	      break;

	    default:
	      abort ();
	    }
	}
    }
}


/* Changing the font of the frame.  */

/* Give frame F the font FONT-OBJECT as its default font.  The return
   value is FONT-OBJECT.  FONTSET is an ID of the fontset for the
   frame.  If it is negative, generate a new fontset from
   FONT-OBJECT.  */

Lisp_Object
x_new_font (struct frame *f, Lisp_Object font_object, int fontset)
{
  struct font *font = XFONT_OBJECT (font_object);

  if (fontset < 0)
    fontset = fontset_from_font (font_object);
  FRAME_FONTSET (f) = fontset;
  if (FRAME_FONT (f) == font)
    /* This font is already set in frame F.  There's nothing more to
       do.  */
    return font_object;

  FRAME_FONT (f) = font;
  FRAME_BASELINE_OFFSET (f) = font->baseline_offset;
  FRAME_COLUMN_WIDTH (f) = font->average_width;
  FRAME_SPACE_WIDTH (f) = font->space_width;
  FRAME_LINE_HEIGHT (f) = FONT_HEIGHT (font);

  compute_fringe_widths (f, 1);

  /* Compute the scroll bar width in character columns.  */
  if (FRAME_CONFIG_SCROLL_BAR_WIDTH (f) > 0)
    {
      int wid = FRAME_COLUMN_WIDTH (f);
      FRAME_CONFIG_SCROLL_BAR_COLS (f)
	= (FRAME_CONFIG_SCROLL_BAR_WIDTH (f) + wid-1) / wid;
    }
  else
    {
      int wid = FRAME_COLUMN_WIDTH (f);
      FRAME_CONFIG_SCROLL_BAR_COLS (f) = (14 + wid - 1) / wid;
    }

  if (FRAME_MAC_WINDOW (f) != 0)
    {
      /* Don't change the size of a tip frame; there's no point in
	 doing it because it's done in Fx_show_tip, and it leads to
	 problems because the tip frame has no widget.  */
      if (NILP (tip_frame) || XFRAME (tip_frame) != f)
        x_set_window_size (f, 0, FRAME_COLS (f), FRAME_LINES (f));
    }

  return font_object;
}


void
mac_handle_origin_change (struct frame *f)
{
  x_real_positions (f, &f->left_pos, &f->top_pos);
}

void
mac_handle_size_change (struct frame *f, int pixelwidth, int pixelheight)
{
  int cols, rows;

  cols = FRAME_PIXEL_WIDTH_TO_TEXT_COLS (f, pixelwidth);
  rows = FRAME_PIXEL_HEIGHT_TO_TEXT_LINES (f, pixelheight);

  if (cols != FRAME_COLS (f)
      || rows != FRAME_LINES (f)
      || pixelwidth != FRAME_PIXEL_WIDTH (f)
      || pixelheight != FRAME_PIXEL_HEIGHT (f))
    {
      /* We pass 1 for DELAY since we can't run Lisp code inside of
	 a BLOCK_INPUT.  */
      change_frame_size (f, rows, cols, 0, 1, 0);
      FRAME_PIXEL_WIDTH (f) = pixelwidth;
      FRAME_PIXEL_HEIGHT (f) = pixelheight;

      /* If cursor was outside the new size, mark it as off.  */
      mark_window_cursors_off (XWINDOW (f->root_window));

      /* Clear out any recollection of where the mouse highlighting
	 was, since it might be in a place that's outside the new
	 frame size.  Actually checking whether it is outside is a
	 pain in the neck, so don't try--just let the highlighting be
	 done afresh with new size.  */
      cancel_mouse_face (f);
    }
}


/* Calculate the absolute position in frame F
   from its current recorded position values and gravity.  */

static void
x_calc_absolute_position (struct frame *f)
{
  int flags = f->size_hint_flags;
  NativeRectangle bounds;

  /* We have nothing to do if the current position
     is already for the top-left corner.  */
  if (! ((flags & XNegative) || (flags & YNegative)))
    return;

  /* Find the offsets of the outside upper-left corner of
     the inner window, with respect to the outer window.  */
  BLOCK_INPUT;
  mac_get_window_structure_bounds (f, &bounds);
  UNBLOCK_INPUT;

  /* Treat negative positions as relative to the leftmost bottommost
     position that fits on the screen.  */
  if (flags & XNegative)
    f->left_pos += (x_display_pixel_width (FRAME_MAC_DISPLAY_INFO (f))
		    - bounds.width);

  if (flags & YNegative)
    f->top_pos += (x_display_pixel_height (FRAME_MAC_DISPLAY_INFO (f))
		   - bounds.height);

  /* The left_pos and top_pos
     are now relative to the top and left screen edges,
     so the flags should correspond.  */
  f->size_hint_flags &= ~ (XNegative | YNegative);
}

/* CHANGE_GRAVITY is 1 when calling from Fset_frame_position,
   to really change the position, and 0 when calling from
   x_make_frame_visible (in that case, XOFF and YOFF are the current
   position values).  It is -1 when calling from x_set_frame_parameters,
   which means, do adjust for borders but don't change the gravity.  */

void
x_set_offset (struct frame *f, register int xoff, register int yoff, int change_gravity)
{
  if (change_gravity > 0)
    {
      f->top_pos = yoff;
      f->left_pos = xoff;
      f->size_hint_flags &= ~ (XNegative | YNegative);
      if (xoff < 0)
	f->size_hint_flags |= XNegative;
      if (yoff < 0)
	f->size_hint_flags |= YNegative;
      f->win_gravity = NorthWestGravity;
    }
  x_calc_absolute_position (f);

  BLOCK_INPUT;
  x_wm_set_size_hint (f, (long) 0, 0);

  mac_move_frame_window_structure (f, f->left_pos, f->top_pos);
  /* When the frame is maximized/fullscreen, the actual window will
     not be moved and mac_handle_origin_change will not be called via
     window system events.  */
  mac_handle_origin_change (f);
  UNBLOCK_INPUT;
}

void
x_set_sticky (struct frame *f, Lisp_Object new_value, Lisp_Object old_value)
{
  BLOCK_INPUT;
  mac_change_frame_window_wm_state (f, !NILP (new_value) ? WM_STATE_STICKY : 0,
				    NILP (new_value) ? WM_STATE_STICKY : 0);
  UNBLOCK_INPUT;
}

static void
XTfullscreen_hook (FRAME_PTR f)
{
  FRAME_CHECK_FULLSCREEN_NEEDED_P (f) = 1;
  if (f->async_visible)
    {
      BLOCK_INPUT;
      x_check_fullscreen (f);
      x_sync (f);
      UNBLOCK_INPUT;
    }
}

/* Check if we need to resize the frame due to a fullscreen request.
   If so needed, resize the frame. */
static void
x_check_fullscreen (struct frame *f)
{
  WMState flags_to_set, flags_to_clear;

  switch (f->want_fullscreen)
    {
    case FULLSCREEN_NONE:
      flags_to_set = 0;
      break;
    case FULLSCREEN_BOTH:
      flags_to_set = WM_STATE_FULLSCREEN;
      break;
    case FULLSCREEN_DEDICATED_DESKTOP:
      flags_to_set = (WM_STATE_FULLSCREEN | WM_STATE_DEDICATED_DESKTOP);
      break;
    case FULLSCREEN_WIDTH:
      flags_to_set = WM_STATE_MAXIMIZED_HORZ;
      break;
    case FULLSCREEN_HEIGHT:
      flags_to_set = WM_STATE_MAXIMIZED_VERT;
      break;
    case FULLSCREEN_MAXIMIZED:
      flags_to_set = (WM_STATE_MAXIMIZED_HORZ | WM_STATE_MAXIMIZED_VERT);
      break;
    }

  flags_to_clear = (flags_to_set ^ (WM_STATE_MAXIMIZED_HORZ
				    | WM_STATE_MAXIMIZED_VERT
				    | WM_STATE_FULLSCREEN
				    | WM_STATE_DEDICATED_DESKTOP));

  f->want_fullscreen = FULLSCREEN_NONE;
  FRAME_CHECK_FULLSCREEN_NEEDED_P (f) = 0;

  mac_change_frame_window_wm_state (f, flags_to_set, flags_to_clear);
}

/* Call this to change the size of frame F's x-window.
   If CHANGE_GRAVITY is 1, we change to top-left-corner window gravity
   for this size change and subsequent size changes.
   Otherwise we leave the window gravity unchanged.  */

void
x_set_window_size (struct frame *f, int change_gravity, int cols, int rows)
{
  int pixelwidth, pixelheight;

  BLOCK_INPUT;

  if (NILP (tip_frame) || XFRAME (tip_frame) != f)
    {
      int r, c;

      /* When the frame is maximized/fullscreen or running under for
	 example Xmonad, x_set_window_size will be a no-op.
	 In that case, the right thing to do is extend rows/cols to
	 the current frame size.  We do that first if x_set_window_size
	 turns out to not be a no-op (there is no way to know).
	 The size will be adjusted again if the frame gets a
	 ConfigureNotify event as a result of x_set_window_size.  */
      r = FRAME_PIXEL_HEIGHT_TO_TEXT_LINES (f, FRAME_PIXEL_HEIGHT (f));
      /* Update f->scroll_bar_actual_width because it is used in
	 FRAME_PIXEL_WIDTH_TO_TEXT_COLS.  */
      f->scroll_bar_actual_width
        = FRAME_SCROLL_BAR_COLS (f) * FRAME_COLUMN_WIDTH (f);
      c = FRAME_PIXEL_WIDTH_TO_TEXT_COLS (f, FRAME_PIXEL_WIDTH (f));
      change_frame_size (f, r, c, 0, 1, 0);
    }

  check_frame_size (f, &rows, &cols);
  f->scroll_bar_actual_width
    = FRAME_SCROLL_BAR_COLS (f) * FRAME_COLUMN_WIDTH (f);

  compute_fringe_widths (f, 0);

  pixelwidth = FRAME_TEXT_COLS_TO_PIXEL_WIDTH (f, cols);
  pixelheight = FRAME_TEXT_LINES_TO_PIXEL_HEIGHT (f, rows);

  f->win_gravity = NorthWestGravity;
  x_wm_set_size_hint (f, (long) 0, 0);

  mac_size_frame_window (f, pixelwidth, pixelheight, 0);

  if (f->output_data.mac->internal_border_width
      != FRAME_INTERNAL_BORDER_WIDTH (f))
    {
      if (f->async_visible)
	mac_clear_window (f);
      f->output_data.mac->internal_border_width
	= FRAME_INTERNAL_BORDER_WIDTH (f);
    }

  SET_FRAME_GARBAGED (f);

  UNBLOCK_INPUT;
}

/* Mouse warping.  */

void x_set_mouse_pixel_position (struct frame *f, int pix_x, int pix_y);

void
x_set_mouse_position (struct frame *f, int x, int y)
{
  int pix_x, pix_y;

  pix_x = FRAME_COL_TO_PIXEL_X (f, x) + FRAME_COLUMN_WIDTH (f) / 2;
  pix_y = FRAME_LINE_TO_PIXEL_Y (f, y) + FRAME_LINE_HEIGHT (f) / 2;

  if (pix_x < 0) pix_x = 0;
  if (pix_x > FRAME_PIXEL_WIDTH (f)) pix_x = FRAME_PIXEL_WIDTH (f);

  if (pix_y < 0) pix_y = 0;
  if (pix_y > FRAME_PIXEL_HEIGHT (f)) pix_y = FRAME_PIXEL_HEIGHT (f);

  x_set_mouse_pixel_position (f, pix_x, pix_y);
}

void
x_set_mouse_pixel_position (struct frame *f, int pix_x, int pix_y)
{
  BLOCK_INPUT;
  mac_convert_frame_point_to_global (f, &pix_x, &pix_y);
  CGWarpMouseCursorPosition (CGPointMake (pix_x, pix_y));
  UNBLOCK_INPUT;
}

/* Raise frame F.  */

void
x_raise_frame (struct frame *f)
{
  if (f->async_visible)
    {
      BLOCK_INPUT;
      mac_bring_frame_window_to_front (f);
      UNBLOCK_INPUT;
    }
}

/* Lower frame F.  */

void
x_lower_frame (struct frame *f)
{
  if (f->async_visible)
    {
      BLOCK_INPUT;
      mac_send_frame_window_behind (f);
      UNBLOCK_INPUT;
    }
}

static void
XTframe_raise_lower (FRAME_PTR f, int raise_flag)
{
  if (raise_flag)
    x_raise_frame (f);
  else
    x_lower_frame (f);
}

/* Change of visibility.  */

void
mac_handle_visibility_change (struct frame *f)
{
  int visible = 0, iconified = 0;
  struct input_event buf;

  if (mac_is_frame_window_visible (f))
    {
      if (mac_is_frame_window_collapsed (f))
	iconified = 1;
      else
	visible = 1;
    }

  if (!f->async_visible && visible)
    {
      if (FRAME_CHECK_FULLSCREEN_NEEDED_P (f))
	x_check_fullscreen (f);

      if (f->iconified)
	{
	  /* wait_reading_process_output will notice this and update
	     the frame's display structures.  If we were made
	     invisible, we should not set garbaged, because that stops
	     redrawing on Update events.  */
	  SET_FRAME_GARBAGED (f);

	  EVENT_INIT (buf);
	  buf.kind = DEICONIFY_EVENT;
	  XSETFRAME (buf.frame_or_window, f);
	  buf.arg = Qnil;
	  kbd_buffer_store_event (&buf);
	}
      else if (! NILP (Vframe_list) && ! NILP (XCDR (Vframe_list)))
	/* Force a redisplay sooner or later to update the
	   frame titles in case this is the second frame.  */
	record_asynch_buffer_change ();
    }
  else if (f->async_visible && !visible)
    if (iconified)
      {
	EVENT_INIT (buf);
	buf.kind = ICONIFY_EVENT;
	XSETFRAME (buf.frame_or_window, f);
	buf.arg = Qnil;
	kbd_buffer_store_event (&buf);
      }

  f->async_visible = visible;
  f->async_iconified = iconified;
}

/* This tries to wait until the frame is really visible.
   However, if the window manager asks the user where to position
   the frame, this will return before the user finishes doing that.
   The frame will not actually be visible at that time,
   but it will become visible later when the window manager
   finishes with it.  */

void
x_make_frame_visible (struct frame *f)
{
  BLOCK_INPUT;

  if (! FRAME_VISIBLE_P (f))
    {
      /* We test FRAME_GARBAGED_P here to make sure we don't
	 call x_set_offset a second time
	 if we get to x_make_frame_visible a second time
	 before the window gets really visible.  */
      if (! FRAME_ICONIFIED_P (f)
	  && ! f->output_data.mac->asked_for_visible)
	x_set_offset (f, f->left_pos, f->top_pos, 0);

      f->output_data.mac->asked_for_visible = 1;

      mac_collapse_frame_window (f, false);
      mac_show_frame_window (f);
    }

  XFlush (FRAME_MAC_DISPLAY (f));

  /* Synchronize to ensure Emacs knows the frame is visible
     before we do anything else.  We do this loop with input not blocked
     so that incoming events are handled.  */
  {
    Lisp_Object frame;
    int count;

    /* This must come after we set COUNT.  */
    UNBLOCK_INPUT;

    XSETFRAME (frame, f);

    /* Wait until the frame is visible.  Process X events until a
       MapNotify event has been seen, or until we think we won't get a
       MapNotify at all..  */
    for (count = input_signal_count + 10;
	 input_signal_count < count && !FRAME_VISIBLE_P (f);)
      {
	/* Force processing of queued events.  */
	x_sync (f);

	/* Machines that do polling rather than SIGIO have been
	   observed to go into a busy-wait here.  So we'll fake an
	   alarm signal to let the handler know that there's something
	   to be read.  We used to raise a real alarm, but it seems
	   that the handler isn't always enabled here.  This is
	   probably a bug.  */
	if (input_polling_used ())
	  {
	    /* It could be confusing if a real alarm arrives while
	       processing the fake one.  Turn it off and let the
	       handler reset it.  */
	    int old_poll_suppress_count = poll_suppress_count;
	    poll_suppress_count = 1;
	    poll_for_input_1 ();
	    poll_suppress_count = old_poll_suppress_count;
	  }

	/* See if a MapNotify event has been processed.  */
	FRAME_SAMPLE_VISIBILITY (f);
      }
  }
}

/* Change from mapped state to withdrawn state.  */

/* Make the frame visible (mapped and not iconified).  */

void
x_make_frame_invisible (struct frame *f)
{
  /* A deactivate event does not occur when the last visible frame is
     made invisible.  So if we clear the highlight here, it will not
     be rehighlighted when it is made visible.  */
#if 0
  /* Don't keep the highlight on an invisible frame.  */
  if (FRAME_MAC_DISPLAY_INFO (f)->x_highlight_frame == f)
    FRAME_MAC_DISPLAY_INFO (f)->x_highlight_frame = 0;
#endif

  BLOCK_INPUT;

  /* Before unmapping the window, update the WM_SIZE_HINTS property to claim
     that the current position of the window is user-specified, rather than
     program-specified, so that when the window is mapped again, it will be
     placed at the same location, without forcing the user to position it
     by hand again (they have already done that once for this window.)  */
  x_wm_set_size_hint (f, (long) 0, 1);

  mac_hide_frame_window (f);

  UNBLOCK_INPUT;

  mac_handle_visibility_change (f);
}

/* Change window state from mapped to iconified.  */

void
x_iconify_frame (struct frame *f)
{
  OSStatus err;

  /* A deactivate event does not occur when the last visible frame is
     iconified.  So if we clear the highlight here, it will not be
     rehighlighted when it is deiconified.  */
#if 0
  /* Don't keep the highlight on an invisible frame.  */
  if (FRAME_MAC_DISPLAY_INFO (f)->x_highlight_frame == f)
    FRAME_MAC_DISPLAY_INFO (f)->x_highlight_frame = 0;
#endif

  if (f->async_iconified)
    return;

  BLOCK_INPUT;

  FRAME_SAMPLE_VISIBILITY (f);

  if (! FRAME_VISIBLE_P (f))
    mac_show_frame_window (f);

  err = mac_collapse_frame_window (f, true);

  UNBLOCK_INPUT;

  if (err != noErr)
    error ("Can't notify window manager of iconification");

  mac_handle_visibility_change (f);
}


/* Free X resources of frame F.  */

void
x_free_frame_resources (struct frame *f)
{
  struct mac_display_info *dpyinfo = FRAME_MAC_DISPLAY_INFO (f);
  Mouse_HLInfo *hlinfo = &dpyinfo->mouse_highlight;

  BLOCK_INPUT;

  /* AppKit version of mac_dispose_frame_window, which is implemented
     as -[NSWindow close], will change the focus to the next window
     during its call.  So, unlike other platforms, we clean up the
     focus-related variables before calling mac_dispose_frame_window.  */
  if (f == dpyinfo->x_focus_frame)
    {
      dpyinfo->x_focus_frame = 0;
      mac_set_font_info_for_selection (NULL, DEFAULT_FACE_ID, 0, -1, Qnil);
    }
  if (f == dpyinfo->x_focus_event_frame)
    dpyinfo->x_focus_event_frame = 0;
  if (f == dpyinfo->x_highlight_frame)
    dpyinfo->x_highlight_frame = 0;

  if (f == hlinfo->mouse_face_mouse_frame)
    {
      hlinfo->mouse_face_beg_row
	= hlinfo->mouse_face_beg_col = -1;
      hlinfo->mouse_face_end_row
	= hlinfo->mouse_face_end_col = -1;
      hlinfo->mouse_face_window = Qnil;
      hlinfo->mouse_face_deferred_gc = 0;
      hlinfo->mouse_face_mouse_frame = 0;
    }

  mac_dispose_frame_window (f);

  free_frame_menubar (f);

  if (FRAME_FACE_CACHE (f))
    free_frame_faces (f);

  x_free_gcs (f);

  xfree (FRAME_SIZE_HINTS (f));

  xfree (f->output_data.mac);
  f->output_data.mac = NULL;

  UNBLOCK_INPUT;
}


/* Destroy the X window of frame F.  */

static void
x_destroy_window (struct frame *f)
{
  struct mac_display_info *dpyinfo = FRAME_MAC_DISPLAY_INFO (f);

  x_free_frame_resources (f);

  dpyinfo->reference_count--;
}


/* Setting window manager hints.  */

/* Set the normal size hints for the window manager, for frame F.
   FLAGS is the flags word to use--or 0 meaning preserve the flags
   that the window now has.
   If USER_POSITION is nonzero, we set the USPosition
   flag (this is useful when FLAGS is 0).  */
void
x_wm_set_size_hint (struct frame *f, long flags, int user_position)
{
  int base_width, base_height, width_inc, height_inc;
  int min_rows = 0, min_cols = 0;
  XSizeHints *size_hints;

  base_width = FRAME_TEXT_COLS_TO_PIXEL_WIDTH (f, 0);
  base_height = FRAME_TEXT_LINES_TO_PIXEL_HEIGHT (f, 0);
  width_inc = FRAME_COLUMN_WIDTH (f);
  height_inc = FRAME_LINE_HEIGHT (f);

  check_frame_size (f, &min_rows, &min_cols);

  size_hints = FRAME_SIZE_HINTS (f);
  if (size_hints == NULL)
    {
      size_hints = FRAME_SIZE_HINTS (f) = xmalloc (sizeof (XSizeHints));
      memset (size_hints, 0, sizeof (XSizeHints));
    }

  size_hints->flags |= PResizeInc | PMinSize | PBaseSize ;
  size_hints->width_inc  = width_inc;
  size_hints->height_inc = height_inc;
  size_hints->min_width  = base_width + min_cols * width_inc;
  size_hints->min_height = base_height + min_rows * height_inc;
  size_hints->base_width  = base_width;
  size_hints->base_height = base_height;

  if (flags)
    size_hints->flags = flags;
  else if (user_position)
    {
      size_hints->flags &= ~ PPosition;
      size_hints->flags |= USPosition;
    }
}

void
x_wm_set_icon_position (struct frame *f, int icon_x, int icon_y)
{
#if 0 /* MAC_TODO: no icons on Mac */
#ifdef USE_X_TOOLKIT
  Window window = XtWindow (f->output_data.x->widget);
#else
  Window window = FRAME_X_WINDOW (f);
#endif

  f->output_data.x->wm_hints.flags |= IconPositionHint;
  f->output_data.x->wm_hints.icon_x = icon_x;
  f->output_data.x->wm_hints.icon_y = icon_y;

  XSetWMHints (FRAME_X_DISPLAY (f), window, &f->output_data.x->wm_hints);
#endif /* MAC_TODO */
}


/***********************************************************************
				Fonts
 ***********************************************************************/

Lisp_Object Qpanel_closed, Qselection;

#if GLYPH_DEBUG

/* Check that FONT is valid on frame F.  It is if it can be found in F's
   font table.  */

static void
x_check_font (struct frame *f, struct font *font)
{
  Lisp_Object frame;

  xassert (font != NULL && ! NILP (font->props[FONT_TYPE_INDEX]));
  if (font->driver->check)
    xassert (font->driver->check (f, font) == 0);
}

#endif /* GLYPH_DEBUG != 0 */


/* The Mac Event loop code */

/* Contains the string "reverse", which is a constant for mouse button emu.*/
Lisp_Object Qreverse;

Lisp_Object Qkeyboard_modifiers;

/* Whether or not the screen configuration has changed.  */
int mac_screen_config_changed = 0;

/* Apple Events */
Lisp_Object Qtext_input;
static Lisp_Object saved_ts_script_language_on_focus;
static ScriptLanguageRecord saved_ts_language;
static Component saved_ts_component;
Lisp_Object Qinsert_text, Qset_marked_text;
Lisp_Object Qaction, Qmac_action_key_paths;
Lisp_Object Qaccessibility;
Lisp_Object Qservice, Qpaste, Qperform;

extern Lisp_Object Qundefined;
extern int XTread_socket (struct terminal *, int, struct input_event *);
extern void mac_find_apple_event_spec (AEEventClass, AEEventID,
				       Lisp_Object *, Lisp_Object *,
				       Lisp_Object *);
extern OSErr init_coercion_handler (void);

/* Table for translating Mac keycode to X keysym values.  Contributed
   by Sudhir Shenoy.
   Mapping for special keys is now identical to that in Apple X11
   except `clear' (-> <clear>) on the KeyPad, `enter' (-> <kp-enter>)
   on the right of the Cmd key on laptops, and fn + `enter' (->
   <linefeed>). */
const unsigned char keycode_to_xkeysym_table[] = {
  /*0x00*/ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  /*0x10*/ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  /*0x20*/ 0, 0, 0, 0, 0x0d /*return*/, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,

  /*0x30*/ 0x09 /*tab*/, 0 /*0x0020 space*/, 0, 0x08 /*backspace*/,
  /*0x34*/ 0x8d /*enter on laptops*/, 0x1b /*escape*/, 0, 0,
  /*0x38*/ 0, 0, 0, 0,
  /*0x3C*/ 0, 0, 0, 0,

  /*0x40*/ 0xce /*f17*/, 0xae /*kp-decimal*/, 0, 0xaa /*kp-multiply*/,
  /*0x44*/ 0, 0xab /*kp-add*/, 0, 0x0b /*clear*/,
  /*0x48*/ 0, 0, 0, 0xaf /*kp-divide*/,
  /*0x4C*/ 0x8d /*kp-enter*/, 0, 0xad /*kp-subtract*/, 0xcf /*f18*/,

  /*0x50*/ 0xd0 /*f19*/, 0xbd /*kp-equal*/, 0xb0 /*kp-0*/, 0xb1 /*kp-1*/,
  /*0x54*/ 0xb2 /*kp-2*/, 0xb3 /*kp-3*/, 0xb4 /*kp-4*/, 0xb5 /*kp-5*/,
  /*0x58*/ 0xb6 /*kp-6*/, 0xb7 /*kp-7*/, 0xd1 /*f20*/, 0xb8 /*kp-8*/,
  /*0x5C*/ 0xb9 /*kp-9*/, 0, 0, 0xac /*kp-separator*/,

  /*0x60*/ 0xc2 /*f5*/, 0xc3 /*f6*/, 0xc4 /*f7*/, 0xc0 /*f3*/,
  /*0x64*/ 0xc5 /*f8*/, 0xc6 /*f9*/, 0, 0xc8 /*f11*/,
  /*0x68*/ 0, 0xca /*f13*/, 0xcd /*f16*/, 0xcb /*f14*/,
  /*0x6C*/ 0, 0xc7 /*f10*/, 0x0a /*fn+enter on laptops*/, 0xc9 /*f12*/,

  /*0x70*/ 0, 0xcc /*f15*/, 0x6a /*help*/, 0x50 /*home*/,
  /*0x74*/ 0x55 /*pgup*/, 0xff /*delete*/, 0xc1 /*f4*/, 0x57 /*end*/,
  /*0x78*/ 0xbf /*f2*/, 0x56 /*pgdown*/, 0xbe /*f1*/, 0x51 /*left*/,
  /*0x7C*/ 0x53 /*right*/, 0x54 /*down*/, 0x52 /*up*/, 0
};

/* Table for translating Mac keycode with the laptop `fn' key to that
   without it.  Destination symbols in comments are keys on US
   keyboard, and they may not be the same on other types of keyboards.
   If the destination is identical to the source, it doesn't map `fn'
   key to a modifier.  */
static const unsigned char fn_keycode_to_keycode_table[] = {
  /*0x00*/ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  /*0x10*/ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  /*0x20*/ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,

  /*0x30*/ 0, 0, 0, 0,
  /*0x34*/ 0, 0, 0, 0,
  /*0x38*/ 0, 0, 0, 0,
  /*0x3C*/ 0, 0, 0, 0,

  /*0x40*/ 0x40 /*f17 = f17*/, 0x2f /*kp-decimal -> '.'*/, 0, 0x23 /*kp-multiply -> 'p'*/,
  /*0x44*/ 0, 0x2c /*kp-add -> '/'*/, 0, 0x16 /*clear -> '6'*/,
  /*0x48*/ 0, 0, 0, 0x1d /*kp-/ -> '0'*/,
  /*0x4C*/ 0x24 /*kp-enter -> return*/, 0, 0x29 /*kp-subtract -> ';'*/, 0x4f /*f18 = f18*/,

  /*0x50*/ 0x50 /*f19 = f19*/, 0x1b /*kp-equal -> '-'*/, 0x2e /*kp-0 -> 'm'*/, 0x26 /*kp-1 -> 'j'*/,
  /*0x54*/ 0x28 /*kp-2 -> 'k'*/, 0x25 /*kp-3 -> 'l'*/, 0x20 /*kp-4 -> 'u'*/, 0x22 /*kp-5 ->'i'*/,
  /*0x58*/ 0x1f /*kp-6 -> 'o'*/, 0x1a /*kp-7 -> '7'*/, 0x5a /*f20 = f20*/, 0x1c /*kp-8 -> '8'*/,
  /*0x5C*/ 0x19 /*kp-9 -> '9'*/, 0, 0, 0,

  /*0x60*/ 0x60 /*f5 = f5*/, 0x61 /*f6 = f6*/, 0x62 /*f7 = f7*/, 0x63 /*f3 = f3*/,
  /*0x64*/ 0x64 /*f8 = f8*/, 0x65 /*f9 = f9*/, 0, 0x67 /*f11 = f11*/,
  /*0x68*/ 0, 0x69 /*f13 = f13*/, 0x6a /*f16 = f16*/, 0x6b /*f14 = f14*/,
  /*0x6C*/ 0, 0x6d /*f10 = f10*/, 0, 0x6f /*f12 = f12*/,

  /*0x70*/ 0, 0x71 /*f15 = f15*/, 0x72 /*help = help*/, 0x7b /*home -> left*/,
  /*0x74*/ 0x7e /*pgup -> up*/, 0x33 /*delete -> backspace*/, 0x76 /*f4 = f4*/, 0x7c /*end -> right*/,
  /*0x78*/ 0x78 /*f2 = f2*/, 0x7d /*pgdown -> down*/, 0x7a /*f1 = f1*/, 0x7b /*left = left*/,
  /*0x7C*/ 0x7c /*right = right*/, 0x7d /*down = down*/, 0x7e /*up = up*/, 0
};

int
mac_to_emacs_modifiers (UInt32 mods, UInt32 unmapped_mods)
{
  unsigned int result = 0;
  if ((mods | unmapped_mods) & shiftKey)
    result |= shift_modifier;

  /* Deactivated to simplify configuration:
     if Vmac_option_modifier is non-NIL, we fully process the Option
     key. Otherwise, we only process it if an additional Ctrl or Command
     is pressed. That way the system may convert the character to a
     composed one.
     if ((mods & optionKey) &&
      (( !NILP(Vmac_option_modifier) ||
      ((mods & cmdKey) || (mods & controlKey))))) */

  if (!NILP (Vmac_option_modifier) && (mods & optionKey)) {
    Lisp_Object val = Fget(Vmac_option_modifier, Qmodifier_value);
    if (INTEGERP(val))
      result |= XUINT(val);
  }
  if (!NILP (Vmac_command_modifier) && (mods & cmdKey)) {
    Lisp_Object val = Fget(Vmac_command_modifier, Qmodifier_value);
    if (INTEGERP(val))
      result |= XUINT(val);
  }
  if (!NILP (Vmac_control_modifier) && (mods & controlKey)) {
    Lisp_Object val = Fget(Vmac_control_modifier, Qmodifier_value);
    if (INTEGERP(val))
      result |= XUINT(val);
  }
  if (!NILP (Vmac_function_modifier) && (mods & kEventKeyModifierFnMask)) {
    Lisp_Object val = Fget(Vmac_function_modifier, Qmodifier_value);
    if (INTEGERP(val))
      result |= XUINT(val);
  }

  return result;
}

UInt32
mac_mapped_modifiers (UInt32 modifiers, UInt32 key_code)
{
  UInt32 mapped_modifiers_all =
    (NILP (Vmac_control_modifier) ? 0 : controlKey)
    | (NILP (Vmac_option_modifier) ? 0 : optionKey)
    | (NILP (Vmac_command_modifier) ? 0 : cmdKey)
    | (NILP (Vmac_function_modifier) ? 0 : kEventKeyModifierFnMask);

  /* The meaning of kEventKeyModifierFnMask has changed in Mac OS X
     10.5, and it now behaves much like Cocoa's NSFunctionKeyMask.  It
     no longer means laptop's `fn' key is down for the following keys:
     F1, F2, and so on, Help, Forward Delete, Home, End, Page Up, Page
     Down, the arrow keys, and Clear.  We ignore the corresponding bit
     if that key can be entered without the `fn' key on laptops.  */
  if (modifiers & kEventKeyModifierFnMask
      && key_code <= 0x7f
      && fn_keycode_to_keycode_table[key_code] == key_code
      && key_code != 0)		/* kVK_ANSI_A */
    modifiers &= ~kEventKeyModifierFnMask;

  return mapped_modifiers_all & modifiers;
}

int
mac_get_emulated_btn (UInt32 modifiers)
{
  int result = 0;
  if (!NILP (Vmac_emulate_three_button_mouse)) {
    int cmdIs3 = !EQ (Vmac_emulate_three_button_mouse, Qreverse);
    if (modifiers & cmdKey)
      result = cmdIs3 ? 2 : 1;
    else if (modifiers & optionKey)
      result = cmdIs3 ? 1 : 2;
  }
  return result;
}

void
mac_get_selected_range (struct window *w, CFRange *range)
{
  struct buffer *b = XBUFFER (w->buffer);
  EMACS_INT begv = BUF_BEGV (b), zv = BUF_ZV (b);
  EMACS_INT start, end;

  if (w == XWINDOW (selected_window) && b == current_buffer)
    start = PT;
  else
    start = marker_position (w->pointm);

  if (NILP (Vtransient_mark_mode) || NILP (BVAR (b, mark_active)))
    end = start;
  else
    {
      EMACS_INT mark_pos = marker_position (BVAR (b, mark));

      if (start <= mark_pos)
	end = mark_pos;
      else
	{
	  end = start;
	  start = mark_pos;
	}
    }

  if (start != end)
    {
      if (start < begv)
	start = begv;
      else if (start > zv)
	start = zv;

      if (end < begv)
	end = begv;
      else if (end > zv)
	end = zv;
    }

  range->location = start - begv;
  range->length = end - start;
}

/* Store the text of the buffer BUF from START to END as Unicode
   characters in CHARACTERS.  Return non-zero if successful.  */

static int
mac_store_buffer_text_to_unicode_chars (struct buffer *buf, EMACS_INT start,
					EMACS_INT end, UniChar *characters)
{
  EMACS_INT start_byte = buf_charpos_to_bytepos (buf, start);

#define BUF_FETCH_CHAR_ADVANCE(OUTPUT, BUF, CHARIDX, BYTEIDX)	\
  do    							\
    {								\
      CHARIDX++;						\
      if (!NILP (BVAR (BUF, enable_multibyte_characters)))	\
	{							\
	  unsigned char *ptr = BUF_BYTE_ADDRESS (BUF, BYTEIDX);	\
	  int len;						\
								\
	  OUTPUT= STRING_CHAR_AND_LENGTH (ptr, len);		\
	  BYTEIDX += len;					\
	}							\
      else							\
	{							\
	  OUTPUT = BUF_FETCH_BYTE (BUF, BYTEIDX);		\
	  BYTEIDX++;						\
	}							\
    }								\
  while (0)

  while (start < end)
    {
      int c;

      BUF_FETCH_CHAR_ADVANCE (c, buf, start, start_byte);
      *characters++ = (c < 0xD800 || (c > 0xDFFF && c < 0x10000)) ? c : 0xfffd;
    }

  return 1;
}

CGRect
mac_get_first_rect_for_range (struct window *w, const CFRange *range,
			      CFRange *actual_range)
{
  struct buffer *b = XBUFFER (w->buffer);
  EMACS_INT start_charpos, end_charpos, min_charpos, max_charpos;
  struct glyph_row *row, *r2;
  struct glyph *glyph, *end, *left_glyph, *right_glyph;
  int x, left_x, right_x, text_area_width;

  start_charpos = BUF_BEGV (b) + range->location;
  end_charpos = start_charpos + range->length;
  if (range->length == 0)
    end_charpos++;

  /* Find the rows corresponding to START_CHARPOS and END_CHARPOS.  */
  rows_from_pos_range (w, start_charpos, end_charpos, Qnil, &row, &r2);
  if (row == NULL)
    row = MATRIX_ROW (w->current_matrix, XFASTINT (w->window_end_vpos));
  if (r2 == NULL)
    r2 = MATRIX_ROW (w->current_matrix, XFASTINT (w->window_end_vpos));
  if (row->y > r2->y)
    row = r2;

  if (!row->reversed_p)
    {
      /* This row is in a left to right paragraph.  Scan it left to
	 right.  */
      glyph = row->glyphs[TEXT_AREA];
      end = glyph + row->used[TEXT_AREA];
      x = row->x;

      /* Skip truncation glyphs at the start of the glyph row.  */
      if (row->displays_text_p)
	for (; glyph < end
	       && INTEGERP (glyph->object)
	       && glyph->charpos < 0;
	     ++glyph)
	  x += glyph->pixel_width;

      /* Scan the glyph row, looking for the first glyph from buffer
	 whose position is between START_CHARPOS and END_CHARPOS.  */
      for (; glyph < end
	     && !INTEGERP (glyph->object)
	     && !(BUFFERP (glyph->object)
		  && (glyph->charpos >= start_charpos
		      && glyph->charpos < end_charpos));
	   ++glyph)
	x += glyph->pixel_width;

      left_x = x;
      left_glyph = glyph;
    }
  else
    {
      /* This row is in a right to left paragraph.  Scan it right to
	 left.  */
      struct glyph *g;

      end = row->glyphs[TEXT_AREA] - 1;
      glyph = end + row->used[TEXT_AREA];

      /* Skip truncation glyphs at the start of the glyph row.  */
      if (row->displays_text_p)
	for (; glyph > end
	       && INTEGERP (glyph->object)
	       && glyph->charpos < 0;
	     --glyph)
	  ;

      /* Scan the glyph row, looking for the first glyph from buffer
	 whose position is between START_CHARPOS and END_CHARPOS.  */
      for (; glyph > end
	     && !INTEGERP (glyph->object)
	     && !(BUFFERP (glyph->object)
		  && (glyph->charpos >= start_charpos
		      && glyph->charpos < end_charpos));
	   --glyph)
	;

      glyph++; /* first glyph to the right of the first rect */
      for (g = row->glyphs[TEXT_AREA], x = row->x; g < glyph; g++)
	x += g->pixel_width;

      right_x = x;
      right_glyph = glyph;
    }

  if (range->length == 0)
    {
      min_charpos = max_charpos = start_charpos;
      if (!row->reversed_p)
	right_x = left_x;
      else
	left_x = right_x;
    }
  else
    {
      if (!row->reversed_p)
	{
	  if (MATRIX_ROW_END_CHARPOS (row) <= end_charpos)
	    {
	      min_charpos = max_charpos = MATRIX_ROW_END_CHARPOS (row);
	      right_x = INT_MAX;
	      right_glyph = end;
	    }
	  else
	    {
	      /* Skip truncation and continuation glyphs near the end
		 of the row, and also blanks and stretch glyphs
		 inserted by extend_face_to_end_of_line.  */
	      while (end > glyph
		     && INTEGERP ((end - 1)->object))
		--end;
	      /* Scan the rest of the glyph row from the end, looking
		 for the first glyph whose position is between
		 START_CHARPOS and END_CHARPOS */
	      for (--end;
		   end > glyph
		     && !INTEGERP (end->object)
		     && !(BUFFERP (end->object)
			  && (end->charpos >= start_charpos
			      && end->charpos < end_charpos));
		   --end)
		;
	      /* Find the X coordinate of the last glyph of the first
		 rect.  */
	      for (; glyph <= end; ++glyph)
		x += glyph->pixel_width;

	      min_charpos = end_charpos;
	      max_charpos = start_charpos;
	      right_x = x;
	      right_glyph = glyph;
	    }
	}
      else
	{
	  if (MATRIX_ROW_END_CHARPOS (row) <= end_charpos)
	    {
	      min_charpos = max_charpos = MATRIX_ROW_END_CHARPOS (row);
	      left_x = 0;
	      left_glyph = end + 1;
	    }
	  else
	    {
	      /* Skip truncation and continuation glyphs near the end
		 of the row, and also blanks and stretch glyphs
		 inserted by extend_face_to_end_of_line.  */
	      x = row->x;
	      end++;
	      while (end < glyph
		     && INTEGERP (end->object))
		{
		  x += end->pixel_width;
		  ++end;
		}
	      /* Scan the rest of the glyph row from the end, looking
		 for the first glyph whose position is between
		 START_CHARPOS and END_CHARPOS */
	      for ( ;
		    end < glyph
		      && !INTEGERP (end->object)
		      && !(BUFFERP (end->object)
			   && (end->charpos >= start_charpos
			       && end->charpos < end_charpos));
		    ++end)
		x += end->pixel_width;

	      min_charpos = end_charpos;
	      max_charpos = start_charpos;
	      left_x = x;
	      left_glyph = end;
	    }
	}

      for (glyph = left_glyph; glyph < right_glyph; ++glyph)
	if (!STRINGP (glyph->object) && glyph->charpos > 0)
	  {
	    if (glyph->charpos < min_charpos)
	      min_charpos = glyph->charpos;
	    if (glyph->charpos + 1 > max_charpos)
	      max_charpos = glyph->charpos + 1;
	  }
      if (min_charpos > max_charpos)
	min_charpos = max_charpos;
    }

  if (actual_range)
    {
      actual_range->location = min_charpos - BUF_BEGV (b);
      actual_range->length = max_charpos - min_charpos;
    }

  text_area_width = window_box_width (w, TEXT_AREA);
  if (left_x < 0)
    left_x = 0;
  else if (left_x > text_area_width)
    left_x = text_area_width;
  if (right_x < 0)
    right_x = 0;
  else if (right_x > text_area_width)
    right_x = text_area_width;

  return CGRectMake (WINDOW_TEXT_TO_FRAME_PIXEL_X (w, left_x),
		     WINDOW_TO_FRAME_PIXEL_Y (w, row->y),
		     right_x - left_x, row->height);
}

void
mac_ax_selected_text_range (struct frame *f, CFRange *range)
{
  mac_get_selected_range (XWINDOW (f->selected_window), range);
}

EMACS_INT
mac_ax_number_of_characters (struct frame *f)
{
  struct buffer *b = XBUFFER (XWINDOW (f->selected_window)->buffer);

  return BUF_ZV (b) - BUF_BEGV (b);
}

void
mac_ax_visible_character_range (struct frame *f, CFRange *range)
{
  struct window *w = XWINDOW (f->selected_window);
  struct buffer *b = XBUFFER (w->buffer);
  EMACS_INT start, end;

  /* XXX: Check validity of window_end_pos?  */
  start = marker_position (w->start);
  end = BUF_Z (b) - XFASTINT (w->window_end_pos);

  range->location = start - BUF_BEGV (b);
  range->length = end - start;
}

EMACS_INT
mac_ax_line_for_index (struct frame *f, EMACS_INT index)
{
  struct buffer *b = XBUFFER (XWINDOW (f->selected_window)->buffer);
  EMACS_INT line;
  const unsigned char *limit, *begv, *zv, *gap_end, *p;

  if (index >= 0)
    limit = BUF_CHAR_ADDRESS (b, BUF_BEGV (b) + index);
  else
    limit = BUF_BYTE_ADDRESS (b, BUF_PT_BYTE (b));
  begv = BUF_BYTE_ADDRESS (b, BUF_BEGV_BYTE (b));
  zv = BUF_BYTE_ADDRESS (b, BUF_ZV_BYTE (b));

  if (limit < begv || limit > zv)
    return -1;

  line = 0;
  gap_end = BUF_GAP_END_ADDR (b);
  if (begv < gap_end && gap_end <= limit)
    {
      for (p = gap_end; (p = memchr (p, '\n', limit - p)) != NULL; p++)
	line++;
      limit = BUF_GPT_ADDR (b);
    }

  for (p = begv; (p = memchr (p, '\n', limit - p)) != NULL; p++)
    line++;

  return line;
}

static const unsigned char *
mac_ax_buffer_skip_lines (struct buffer *buf, EMACS_INT n,
			  const unsigned char *start, const unsigned char *end)
{
  const unsigned char *gpt, *p, *limit;

  gpt = BUF_GPT_ADDR (buf);
  p = start;

  if (p <= gpt && gpt < end)
    limit = gpt;
  else
    limit = end;

  while (n > 0)
    {
      p = memchr (p, '\n', limit - p);
      if (p)
	p++;
      else if (limit == end)
	break;
      else
	{
	  p = BUF_GAP_END_ADDR (buf);
	  p = memchr (p, '\n', end - p);
	  if (p)
	    p++;
	  else
	    break;
	}
      n--;
    }

  return p;
}

int
mac_ax_range_for_line (struct frame *f, EMACS_INT line, CFRange *range)
{
  struct buffer *b = XBUFFER (XWINDOW (f->selected_window)->buffer);
  const unsigned char *begv, *zv, *p;
  EMACS_INT start, end;
  int i;

  if (line < 0)
    return 0;

  begv = BUF_BYTE_ADDRESS (b, BUF_BEGV_BYTE (b));
  zv = BUF_BYTE_ADDRESS (b, BUF_ZV_BYTE (b));

  p = mac_ax_buffer_skip_lines (b, line, begv, zv);
  if (p == NULL)
    return 0;

  start = buf_bytepos_to_charpos (b, BUF_PTR_BYTE_POS (b, p));
  p = mac_ax_buffer_skip_lines (b, 1, p, zv);
  if (p)
    end = buf_bytepos_to_charpos (b, BUF_PTR_BYTE_POS (b, p));
  else
    end = BUF_ZV (b);

  range->location = start - BUF_BEGV (b);
  range->length = end - start;

  return 1;
}

CFStringRef
mac_ax_string_for_range (struct frame *f, const CFRange *range,
			 CFRange *actual_range)
{
  struct buffer *b = XBUFFER (XWINDOW (f->selected_window)->buffer);
  CFStringRef result = NULL;
  EMACS_INT start, end, begv = BUF_BEGV (b), zv = BUF_ZV (b);

  start = begv + range->location;
  end = start + range->length;
  if (start < begv)
    start = begv;
  else if (start > zv)
    start = zv;
  if (end < begv)
    end = begv;
  else if (end > zv)
    end = zv;
  if (start <= end)
    {
      EMACS_INT length = end - start;
      UniChar *characters = xmalloc (length * sizeof (UniChar));

      if (mac_store_buffer_text_to_unicode_chars (b, start, end, characters))
	{
	  result = CFStringCreateWithCharacters (NULL, characters, length);
	  if (actual_range)
	    {
	      actual_range->location = start - begv;
	      actual_range->length = length;
	    }
	}
      xfree (characters);
    }

  return result;
}

OSStatus
mac_restore_keyboard_input_source (void)
{
  OSStatus err = noErr;
#if !__LP64__ // XXX
  ScriptLanguageRecord slrec, *slptr = NULL;

  if (EQ (Vmac_ts_script_language_on_focus, Qt)
      && EQ (saved_ts_script_language_on_focus, Qt))
    slptr = &saved_ts_language;
  else if (CONSP (Vmac_ts_script_language_on_focus)
	   && INTEGERP (XCAR (Vmac_ts_script_language_on_focus))
	   && INTEGERP (XCDR (Vmac_ts_script_language_on_focus))
	   && CONSP (saved_ts_script_language_on_focus)
	   && EQ (XCAR (saved_ts_script_language_on_focus),
		  XCAR (Vmac_ts_script_language_on_focus))
	   && EQ (XCDR (saved_ts_script_language_on_focus),
		  XCDR (Vmac_ts_script_language_on_focus)))
    {
      slrec.fScript = XINT (XCAR (Vmac_ts_script_language_on_focus));
      slrec.fLanguage = XINT (XCDR (Vmac_ts_script_language_on_focus));
      slptr = &slrec;
    }

  if (slptr)
    {
      err = SetDefaultInputMethodOfClass (saved_ts_component, slptr,
					  kKeyboardInputMethodClass);
      if (err == noErr)
	err = SetTextServiceLanguage (slptr);

      /* Seems to be needed on Mac OS X 10.2.  */
      if (err == noErr)
	KeyScript (slptr->fScript | smKeyForceKeyScriptMask);
    }
#endif	/* !__LP64__ */

  return err;
}

void
mac_save_keyboard_input_source (void)
{
#if !__LP64__ // XXX
  OSStatus err;
  ScriptLanguageRecord slrec, *slptr = NULL;

  saved_ts_script_language_on_focus = Vmac_ts_script_language_on_focus;

  if (EQ (Vmac_ts_script_language_on_focus, Qt))
    {
      err = GetTextServiceLanguage (&saved_ts_language);
      if (err == noErr)
	slptr = &saved_ts_language;
    }
  else if (CONSP (Vmac_ts_script_language_on_focus)
	   && INTEGERP (XCAR (Vmac_ts_script_language_on_focus))
	   && INTEGERP (XCDR (Vmac_ts_script_language_on_focus)))
    {
      slrec.fScript = XINT (XCAR (Vmac_ts_script_language_on_focus));
      slrec.fLanguage = XINT (XCDR (Vmac_ts_script_language_on_focus));
      slptr = &slrec;
    }

  if (slptr)
    {
      GetDefaultInputMethodOfClass (&saved_ts_component, slptr,
				    kKeyboardInputMethodClass);
    }
#endif	/* !__LP64__ */
}

/***** Code to handle C-g testing  *****/
extern int quit_char;
extern int make_ctrl_char (int);

int
mac_quit_char_key_p (UInt32 modifiers, UInt32 key_code)
{
  UInt32 char_code, mapped_modifiers;
  unsigned long some_state = 0;
  Ptr kchr_ptr = (Ptr) GetScriptManagerVariable (smKCHRCache);
  int c, emacs_modifiers;

  /* Mask off modifier keys that are mapped to some Emacs modifiers.  */
  mapped_modifiers = mac_mapped_modifiers (modifiers, key_code);
  key_code |= (modifiers & ~mapped_modifiers);
  char_code = KeyTranslate (kchr_ptr, key_code, &some_state);
  if (char_code & ~0xff)
    return 0;

  c = char_code;
  emacs_modifiers = mac_to_emacs_modifiers (mapped_modifiers, modifiers);
  if (emacs_modifiers & ctrl_modifier)
    c = make_ctrl_char (c);

  c |= (emacs_modifiers
	& (meta_modifier | alt_modifier
	   | hyper_modifier | super_modifier));

  return c == quit_char;
}

static void
mac_set_unicode_keystroke_event (UniChar code, struct input_event *buf)
{
  if (code < 0x80)
    buf->kind = ASCII_KEYSTROKE_EVENT;
  else
    buf->kind = MULTIBYTE_CHAR_KEYSTROKE_EVENT;
  buf->code = code;
}

void
do_keystroke (EventKind action, unsigned char char_code, UInt32 key_code,
	      UInt32 modifiers, unsigned long timestamp,
	      struct input_event *buf)
{
  static SInt16 last_key_script = -1;
  SInt16 current_key_script = GetScriptManagerVariable (smKeyScript);
  UInt32 mapped_modifiers = mac_mapped_modifiers (modifiers, key_code);

  if (mapped_modifiers & kEventKeyModifierFnMask
      && key_code <= 0x7f
      && fn_keycode_to_keycode_table[key_code])
    key_code = fn_keycode_to_keycode_table[key_code];

  if (key_code <= 0x7f && keycode_to_xkeysym_table[key_code])
    {
      buf->kind = NON_ASCII_KEYSTROKE_EVENT;
      buf->code = 0xff00 | keycode_to_xkeysym_table[key_code];
    }
  else if (mapped_modifiers)
    {
      /* translate the keycode back to determine the original key */
      UCKeyboardLayout *uchr_ptr = NULL;
#if __LP64__
      TISInputSourceRef source = TISCopyCurrentKeyboardLayoutInputSource ();
      CFDataRef uchr_data = NULL;

      if (source)
	uchr_data =
	  TISGetInputSourceProperty (source, kTISPropertyUnicodeKeyLayoutData);
      if (uchr_data)
	uchr_ptr = (UCKeyboardLayout *) CFDataGetBytePtr (uchr_data);
#else
      OSStatus err;
      KeyboardLayoutRef layout;

      err = KLGetCurrentKeyboardLayout (&layout);
      if (err == noErr)
	err = KLGetKeyboardLayoutProperty (layout, kKLuchrData,
					   (const void **) &uchr_ptr);
#endif

      if (uchr_ptr)
	{
	  OSStatus status;
	  UInt16 key_action = action - keyDown;
	  UInt32 modifier_key_state =
	    (modifiers & ~mapped_modifiers & ~alphaLock) >> 8;
	  UInt32 keyboard_type = LMGetKbdType ();
	  UInt32 dead_key_state = 0;
	  UniChar code;
	  UniCharCount actual_length;

	  status = UCKeyTranslate (uchr_ptr, key_code, key_action,
				   modifier_key_state, keyboard_type,
				   kUCKeyTranslateNoDeadKeysMask,
				   &dead_key_state,
				   1, &actual_length, &code);
	  if (status == noErr && actual_length == 1)
	    mac_set_unicode_keystroke_event (code, buf);
	}
#if __LP64__
      if (source)
	CFRelease (source);
#endif

      if (buf->kind == NO_EVENT)
	{
	  /* This code comes from Keyboard Resource, Appendix C of IM
	     - Text.  This is necessary since shift is ignored in KCHR
	     table translation when option or command is pressed.  It
	     also does not translate correctly control-shift chars
	     like C-% so mask off shift here also.  */
	  /* Mask off modifier keys that are mapped to some Emacs
	     modifiers.  */
	  int new_modifiers = modifiers & ~mapped_modifiers & ~alphaLock;
	  /* set high byte of keycode to modifier high byte*/
	  int new_key_code = key_code | new_modifiers;
	  Ptr kchr_ptr = (Ptr) GetScriptManagerVariable (smKCHRCache);
	  unsigned long some_state = 0;
	  UInt32 new_char_code;

	  new_char_code = KeyTranslate (kchr_ptr, new_key_code, &some_state);
	  if (new_char_code == 0)
	    /* Seems like a dead key.  Append up-stroke.  */
	    new_char_code = KeyTranslate (kchr_ptr, new_key_code | 0x80,
					  &some_state);
	  if (new_char_code)
	    {
	      buf->kind = ASCII_KEYSTROKE_EVENT;
	      buf->code = new_char_code & 0xff;
	    }
	}
    }

  if (buf->kind == NO_EVENT)
    {
      buf->kind = ASCII_KEYSTROKE_EVENT;
      buf->code = char_code;
    }

  buf->modifiers = mac_to_emacs_modifiers (mapped_modifiers, modifiers);
  buf->modifiers |= (extra_keyboard_modifiers
		     & (meta_modifier | alt_modifier
			| hyper_modifier | super_modifier));

  if (buf->kind == ASCII_KEYSTROKE_EVENT
      && buf->code >= 0x80 && buf->modifiers)
    {
      OSStatus err;
      TextEncoding encoding = kTextEncodingMacRoman;
      TextToUnicodeInfo ttu_info;

      UpgradeScriptInfoToTextEncoding (current_key_script,
				       kTextLanguageDontCare,
				       kTextRegionDontCare,
				       NULL, &encoding);
      err = CreateTextToUnicodeInfoByEncoding (encoding, &ttu_info);
      if (err == noErr)
	{
	  UniChar code;
	  Str255 pstr;
	  ByteCount unicode_len;

	  pstr[0] = 1;
	  pstr[1] = buf->code;
	  err = ConvertFromPStringToUnicode (ttu_info, pstr,
					     sizeof (UniChar),
					     &unicode_len, &code);
	  if (err == noErr && unicode_len == sizeof (UniChar))
	    mac_set_unicode_keystroke_event (code, buf);
	  DisposeTextToUnicodeInfo (&ttu_info);
	}
    }

  if (buf->kind == ASCII_KEYSTROKE_EVENT
      && buf->code >= 0x80
      && last_key_script != current_key_script)
    {
      struct input_event event;

      EVENT_INIT (event);
      event.kind = LANGUAGE_CHANGE_EVENT;
      event.arg = Qnil;
      event.code = current_key_script;
      event.timestamp = timestamp;
      kbd_buffer_store_event (&event);
      last_key_script = current_key_script;
    }
}

void
mac_store_apple_event (Lisp_Object class, Lisp_Object id, const AEDesc *desc)
{
  struct input_event buf;

  EVENT_INIT (buf);

  buf.kind = MAC_APPLE_EVENT;
  buf.x = class;
  buf.y = id;
  XSETFRAME (buf.frame_or_window,
	     mac_focus_frame (&one_mac_display_info));
  /* Now that Lisp object allocations are protected by BLOCK_INPUT, it
     is safe to use them during read_socket_hook.  */
  buf.arg = mac_aedesc_to_lisp (desc);
  kbd_buffer_store_event (&buf);
}

OSStatus
mac_store_event_ref_as_apple_event (AEEventClass class, AEEventID id,
				    Lisp_Object class_key, Lisp_Object id_key,
				    EventRef event, UInt32 num_params,
				    const EventParamName *names,
				    const EventParamType *types)
{
  OSStatus err = eventNotHandledErr;
  Lisp_Object binding;

  mac_find_apple_event_spec (class, id, &class_key, &id_key, &binding);
  if (!NILP (binding) && !EQ (binding, Qundefined))
    {
      if (INTEGERP (binding))
	err = XINT (binding);
      else
	{
	  struct input_event buf;

	  EVENT_INIT (buf);

	  buf.kind = MAC_APPLE_EVENT;
	  buf.x = class_key;
	  buf.y = id_key;
	  XSETFRAME (buf.frame_or_window,
		     mac_focus_frame (&one_mac_display_info));
	  /* Now that Lisp object allocations are protected by
	     BLOCK_INPUT, it is safe to use them during
	     read_socket_hook.  */
	  buf.arg = Fcons (build_string ("aevt"),
			   mac_event_parameters_to_lisp (event, num_params,
							 names, types));
	  kbd_buffer_store_event (&buf);
	  err = noErr;
	}
    }

  return err;
}

static pascal void
mac_handle_dm_notification (AppleEvent *event)
{
  mac_screen_config_changed = 1;
}

#if MAC_OS_X_VERSION_MAX_ALLOWED >= 1030
static void
mac_handle_cg_display_reconfig (CGDirectDisplayID display,
				CGDisplayChangeSummaryFlags flags,
				void *user_info)
{
  mac_screen_config_changed = 1;
}
#endif

static OSErr
init_dm_notification_handler (void)
{
  OSErr err = noErr;

#if MAC_OS_X_VERSION_MAX_ALLOWED >= 1030
#if MAC_OS_X_VERSION_MIN_REQUIRED == 1020
  if (CGDisplayRegisterReconfigurationCallback != NULL)
#endif
    {
      CGDisplayRegisterReconfigurationCallback (mac_handle_cg_display_reconfig,
						NULL);
    }
#if MAC_OS_X_VERSION_MIN_REQUIRED == 1020
  else		/* CGDisplayRegisterReconfigurationCallback == NULL */
#endif
#endif	/* MAC_OS_X_VERSION_MAX_ALLOWED >= 1030 */
#if MAC_OS_X_VERSION_MAX_ALLOWED < 1030 || MAC_OS_X_VERSION_MIN_REQUIRED == 1020
    {
      static DMNotificationUPP handle_dm_notificationUPP = NULL;
      ProcessSerialNumber psn;

      if (handle_dm_notificationUPP == NULL)
	handle_dm_notificationUPP =
	  NewDMNotificationUPP (mac_handle_dm_notification);

      err = GetCurrentProcess (&psn);
      if (err == noErr)
	err = DMRegisterNotifyProc (handle_dm_notificationUPP, &psn);
    }
#endif

  return err;
}


/***********************************************************************
			    Initialization
 ***********************************************************************/

static int mac_initialized = 0;
extern void mac_get_screen_info (struct mac_display_info *);

static XrmDatabase
mac_make_rdb (const char *xrm_option)
{
  XrmDatabase database;

  database = xrm_get_preference_database (NULL);
  if (xrm_option)
    xrm_merge_string_database (database, xrm_option);

  return database;
}

struct mac_display_info *
mac_term_init (Lisp_Object display_name, char *xrm_option, char *resource_name)
{
  struct terminal *terminal;
  struct mac_display_info *dpyinfo;
  Mouse_HLInfo *hlinfo;

  BLOCK_INPUT;

  if (!mac_initialized)
    {
      mac_initialize ();
      mac_initialized = 1;
    }

  if (x_display_list)
    error ("Sorry, this version can only handle one display");

  dpyinfo = &one_mac_display_info;
  memset (dpyinfo, 0, sizeof (*dpyinfo));
  hlinfo = &dpyinfo->mouse_highlight;

  terminal = mac_create_terminal (dpyinfo);

  /* Set the name of the terminal. */
  terminal->name = (char *) xmalloc (SBYTES (display_name) + 1);
  strncpy (terminal->name, SSDATA (display_name), SBYTES (display_name));
  terminal->name[SBYTES (display_name)] = 0;

  dpyinfo->mac_id_name
    = (char *) xmalloc (SBYTES (Vinvocation_name)
			+ SBYTES (Vsystem_name)
			+ 2);
  strcat (strcat (strcpy (dpyinfo->mac_id_name, SSDATA (Vinvocation_name)), "@"),
	  SSDATA (Vsystem_name));

  dpyinfo->reference_count = 0;
  dpyinfo->resx = 72.0;
  dpyinfo->resy = 72.0;

  mac_get_screen_info (dpyinfo);

  dpyinfo->grabbed = 0;
  dpyinfo->root_window = NULL;

  hlinfo->mouse_face_beg_row = hlinfo->mouse_face_beg_col = -1;
  hlinfo->mouse_face_end_row = hlinfo->mouse_face_end_col = -1;
  hlinfo->mouse_face_face_id = DEFAULT_FACE_ID;
  hlinfo->mouse_face_window = Qnil;
  hlinfo->mouse_face_overlay = Qnil;
  hlinfo->mouse_face_hidden = 0;

  dpyinfo->xrdb = mac_make_rdb (xrm_option);

  /* Put this display on the chain.  */
  dpyinfo->next = x_display_list;
  x_display_list = dpyinfo;

  /* Put it on x_display_name_list as well, to keep them parallel.  */
  x_display_name_list = Fcons (Fcons (display_name, Qnil),
                               x_display_name_list);
  dpyinfo->name_list_element = XCAR (x_display_name_list);

  x_display_rdb_list = Fcons (dpyinfo->xrdb, x_display_rdb_list);

  add_keyboard_wait_descriptor (0);

  /* In Mac GUI, asynchronous I/O (using SIGIO) can't be used for
     window events because they don't come from sockets, even though
     it works fine on tty's.  */
  Fset_input_interrupt_mode (Qnil);

  mac_init_fringe (terminal->rif);

  UNBLOCK_INPUT;

  return dpyinfo;
}

/* Get rid of display DPYINFO, assuming all frames are already gone.  */

static void
x_delete_display (struct mac_display_info *dpyinfo)
{
  struct terminal *t;

  /* Close all frames and delete the generic struct terminal for this
     display.  */
  for (t = terminal_list; t; t = t->next_terminal)
    if (t->type == output_mac && t->display_info.mac == dpyinfo)
      {
        delete_terminal (t);
        break;
      }

  delete_keyboard_wait_descriptor (0);

  /* Discard this display from x_display_name_list and x_display_list.
     We can't use Fdelq because that can quit.  */
  if (! NILP (x_display_name_list)
      && EQ (XCAR (x_display_name_list), dpyinfo->name_list_element))
    {
      x_display_name_list = XCDR (x_display_name_list);
      x_display_rdb_list = XCDR (x_display_rdb_list);
    }
  else
    {
      Lisp_Object tail, tail_rdb;

      tail = x_display_name_list;
      tail_rdb = x_display_rdb_list;
      while (CONSP (tail) && CONSP (XCDR (tail)))
	{
	  if (EQ (XCAR (XCDR (tail)), dpyinfo->name_list_element))
	    {
	      XSETCDR (tail, XCDR (XCDR (tail)));
	      XSETCDR (tail_rdb, XCDR (XCDR (tail_rdb)));
	      break;
	    }
	  tail = XCDR (tail);
	  tail_rdb = XCDR (tail_rdb);
	}
    }

  if (x_display_list == dpyinfo)
    x_display_list = dpyinfo->next;
  else
    {
      struct x_display_info *tail;

      for (tail = x_display_list; tail; tail = tail->next)
	if (tail->next == dpyinfo)
	  tail->next = tail->next->next;
    }

  xfree (dpyinfo->mac_id_name);
}


void
mac_handle_user_signal (int sig)
{
  extern void mac_wakeup_from_run_loop_run_once (void);

  mac_wakeup_from_run_loop_run_once ();
}

static void
record_startup_key_modifiers (void)
{
  Vmac_startup_options = Fcons (Fcons (Qkeyboard_modifiers,
				       make_number (GetCurrentKeyModifiers ())),
				Vmac_startup_options);
}

/* Set up use of X before we make the first connection.  */

extern frame_parm_handler mac_frame_parm_handlers[];

static struct redisplay_interface x_redisplay_interface =
{
  mac_frame_parm_handlers,
  x_produce_glyphs,
  x_write_glyphs,
  x_insert_glyphs,
  x_clear_end_of_line,
  x_scroll_run,
  x_after_update_window_line,
  x_update_window_begin,
  x_update_window_end,
  mac_cursor_to,
  mac_flush,
  0, /* flush_display_optional */
  x_clear_window_mouse_face,
  x_get_glyph_overhangs,
  x_fix_overlapping_area,
  x_draw_fringe_bitmap,
  mac_define_fringe_bitmap,
  mac_destroy_fringe_bitmap,
  x_compute_glyph_string_overhangs,
  x_draw_glyph_string,
  mac_define_frame_cursor,
  mac_clear_frame_area,
  mac_draw_window_cursor,
  mac_draw_vertical_window_border,
  mac_shift_glyphs_for_insert
};

/* This function is called when the last frame on a display is deleted. */
void
x_delete_terminal (struct terminal *terminal)
{
  struct mac_display_info *dpyinfo = terminal->display_info.mac;

  /* Protect against recursive calls.  delete_frame in
     delete_terminal calls us back when it deletes our last frame.  */
  if (!terminal->name)
    return;

  BLOCK_INPUT;
  x_destroy_all_bitmaps (dpyinfo);

  x_delete_display (dpyinfo);
  UNBLOCK_INPUT;
}

static struct terminal *
mac_create_terminal (struct mac_display_info *dpyinfo)
{
  struct terminal *terminal;

  terminal = create_terminal ();

  terminal->type = output_mac;
  terminal->display_info.mac = dpyinfo;
  dpyinfo->terminal = terminal;

  terminal->clear_frame_hook = x_clear_frame;
  terminal->ins_del_lines_hook = x_ins_del_lines;
  terminal->delete_glyphs_hook = x_delete_glyphs;
  terminal->ring_bell_hook = XTring_bell;
  terminal->reset_terminal_modes_hook = XTreset_terminal_modes;
  terminal->set_terminal_modes_hook = XTset_terminal_modes;
  terminal->update_begin_hook = x_update_begin;
  terminal->update_end_hook = x_update_end;
  terminal->set_terminal_window_hook = XTset_terminal_window;
  terminal->read_socket_hook = XTread_socket;
  terminal->frame_up_to_date_hook = XTframe_up_to_date;
  terminal->mouse_position_hook = XTmouse_position;
  terminal->frame_rehighlight_hook = XTframe_rehighlight;
  terminal->frame_raise_lower_hook = XTframe_raise_lower;
  terminal->fullscreen_hook = XTfullscreen_hook;
  terminal->set_vertical_scroll_bar_hook = XTset_vertical_scroll_bar;
  terminal->condemn_scroll_bars_hook = XTcondemn_scroll_bars;
  terminal->redeem_scroll_bar_hook = XTredeem_scroll_bar;
  terminal->judge_scroll_bars_hook = XTjudge_scroll_bars;

  terminal->delete_frame_hook = x_destroy_window;
  terminal->delete_terminal_hook = x_delete_terminal;

  terminal->rif = &x_redisplay_interface;
  terminal->scroll_region_ok = 1;    /* We'll scroll partial frames. */
  terminal->char_ins_del_ok = 1;
  terminal->line_ins_del_ok = 1;         /* We'll just blt 'em. */
  terminal->fast_clear_end_of_line = 1;  /* X does this well. */
  terminal->memory_below_frame = 0;   /* We don't remember what scrolls
                                        off the bottom. */

  /* FIXME: This keyboard setup is 100% untested, just copied from
     w32_create_terminal in order to set window-system now that it's
     a keyboard object.  */
  /* We don't yet support separate terminals on Mac, so don't try to share
     keyboards between virtual terminals that are on the same physical
     terminal like X does.  */
  terminal->kboard = (KBOARD *) xmalloc (sizeof (KBOARD));
  init_kboard (terminal->kboard);
  KVAR (terminal->kboard, Vwindow_system) = Qmac;
  terminal->kboard->next_kboard = all_kboards;
  all_kboards = terminal->kboard;
  /* Don't let the initial kboard remain current longer than necessary.
     That would cause problems if a file loaded on startup tries to
     prompt in the mini-buffer.  */
  if (current_kboard == initial_kboard)
    current_kboard = terminal->kboard;
  terminal->kboard->reference_count++;

  return terminal;
}

void
mac_initialize (void)
{
  baud_rate = 19200;

  last_tool_bar_item = -1;

  BLOCK_INPUT;

  if (init_wakeup_fds () < 0)
    abort ();

  handle_user_signal_hook = mac_handle_user_signal;

  init_coercion_handler ();

  init_dm_notification_handler ();

  install_application_handler ();

  init_cg_color ();

  record_startup_key_modifiers ();

  UNBLOCK_INPUT;
}


void
syms_of_macterm (void)
{
  DEFSYM (Qcontrol, "control");
  DEFSYM (Qmeta, "meta");
  DEFSYM (Qalt, "alt");
  DEFSYM (Qhyper, "hyper");
  DEFSYM (Qsuper, "super");
  DEFSYM (Qmodifier_value, "modifier-value");

  Fput (Qcontrol, Qmodifier_value, make_number (ctrl_modifier));
  Fput (Qmeta,    Qmodifier_value, make_number (meta_modifier));
  Fput (Qalt,     Qmodifier_value, make_number (alt_modifier));
  Fput (Qhyper,   Qmodifier_value, make_number (hyper_modifier));
  Fput (Qsuper,   Qmodifier_value, make_number (super_modifier));

  DEFSYM (Qpanel_closed, "panel-closed");
  DEFSYM (Qselection, "selection");

  DEFSYM (Qservice, "service");
  DEFSYM (Qpaste, "paste");
  DEFSYM (Qperform, "perform");

  DEFSYM (Qtext_input, "text-input");
  DEFSYM (Qinsert_text, "insert-text");
  DEFSYM (Qset_marked_text, "set-marked-text");

  DEFSYM (Qaction, "action");
  DEFSYM (Qmac_action_key_paths, "mac-action-key-paths");

  DEFSYM (Qaccessibility, "accessibility");

  DEFSYM (Qreverse, "reverse");

  DEFSYM (Qkeyboard_modifiers, "keyboard-modifiers");

  staticpro (&x_display_name_list);
  x_display_name_list = Qnil;

  staticpro (&x_display_rdb_list);
  x_display_rdb_list = Qnil;

  staticpro (&saved_ts_script_language_on_focus);
  saved_ts_script_language_on_focus = Qnil;

  /* We don't yet support this, but defining this here avoids whining
     from cus-start.el and other places, like "M-x set-variable".  */
  DEFVAR_BOOL ("x-use-underline-position-properties",
	       x_use_underline_position_properties,
     doc: /* *Non-nil means make use of UNDERLINE_POSITION font properties.
A value of nil means ignore them.  If you encounter fonts with bogus
UNDERLINE_POSITION font properties, for example 7x13 on XFree prior
to 4.1, set this to nil.  */);
  x_use_underline_position_properties = 1;

  DEFVAR_BOOL ("x-underline-at-descent-line",
	       x_underline_at_descent_line,
     doc: /* *Non-nil means to draw the underline at the same place as the descent line.
A value of nil means to draw the underline according to the value of the
variable `x-use-underline-position-properties', which is usually at the
baseline level.  The default value is nil.  */);
  x_underline_at_descent_line = 0;

  DEFVAR_LISP ("x-toolkit-scroll-bars", Vx_toolkit_scroll_bars,
    doc: /* If not nil, Emacs uses toolkit scroll bars.  */);
  Vx_toolkit_scroll_bars = Qt;

  DEFVAR_BOOL ("mac-redisplay-dont-reset-vscroll", mac_redisplay_dont_reset_vscroll,
	       doc: /* Non-nil means update doesn't reset vscroll.  */);
  mac_redisplay_dont_reset_vscroll = 0;

  DEFVAR_BOOL ("mac-ignore-momentum-wheel-events", mac_ignore_momentum_wheel_events,
	       doc: /* Non-nil means momentum wheel events are ignored.  */);
  mac_ignore_momentum_wheel_events = 0;

/* Variables to configure modifier key assignment.  */

  DEFVAR_LISP ("mac-control-modifier", Vmac_control_modifier,
    doc: /* *Modifier key assumed when the Mac control key is pressed.
The value can be `control', `meta', `alt', `hyper', or `super' for the
respective modifier.  The default is `control'.  */);
  Vmac_control_modifier = Qcontrol;

  DEFVAR_LISP ("mac-option-modifier", Vmac_option_modifier,
    doc: /* *Modifier key assumed when the Mac alt/option key is pressed.
The value can be `control', `meta', `alt', `hyper', or `super' for the
respective modifier.  If the value is nil then the key will act as the
normal Mac control modifier, and the option key can be used to compose
characters depending on the chosen Mac keyboard setting.  */);
  Vmac_option_modifier = Qnil;

  DEFVAR_LISP ("mac-command-modifier", Vmac_command_modifier,
    doc: /* *Modifier key assumed when the Mac command key is pressed.
The value can be `control', `meta', `alt', `hyper', or `super' for the
respective modifier.  The default is `meta'.  */);
  Vmac_command_modifier = Qmeta;

  DEFVAR_LISP ("mac-function-modifier", Vmac_function_modifier,
    doc: /* *Modifier key assumed when the Mac function key is pressed.
The value can be `control', `meta', `alt', `hyper', or `super' for the
respective modifier.  Note that remapping the function key may lead to
unexpected results for some keys on non-US/GB keyboards.  */);
  Vmac_function_modifier = Qnil;

  DEFVAR_LISP ("mac-emulate-three-button-mouse",
	       Vmac_emulate_three_button_mouse,
    doc: /* *Specify a way of three button mouse emulation.
The value can be nil, t, or the symbol `reverse'.
A value of nil means that no emulation should be done and the modifiers
should be placed on the mouse-1 event.
t means that when the option-key is held down while pressing the mouse
button, the click will register as mouse-2 and while the command-key
is held down, the click will register as mouse-3.
The symbol `reverse' means that the option-key will register for
mouse-3 and the command-key will register for mouse-2.  */);
  Vmac_emulate_three_button_mouse = Qnil;

  DEFVAR_BOOL ("mac-wheel-button-is-mouse-2", mac_wheel_button_is_mouse_2,
    doc: /* *Non-nil if the wheel button is mouse-2 and the right click mouse-3.
Otherwise, the right click will be treated as mouse-2 and the wheel
button will be mouse-3.  */);
  mac_wheel_button_is_mouse_2 = 1;

  DEFVAR_BOOL ("mac-pass-command-to-system", mac_pass_command_to_system,
    doc: /* *Non-nil if command key presses are passed on to the Mac Toolbox.  */);
  mac_pass_command_to_system = 1;

  DEFVAR_BOOL ("mac-pass-control-to-system", mac_pass_control_to_system,
    doc: /* *Non-nil if control key presses are passed on to the Mac Toolbox.  */);
  mac_pass_control_to_system = 1;

  DEFVAR_LISP ("mac-startup-options", Vmac_startup_options,
    doc: /* Alist of Mac-specific startup options.
Each element looks like (OPTION-TYPE . OPTIONS).
OPTION-TYPE is a symbol specifying the type of startup options:

  command-line -- List of Mac-specific command line options.
  apple-event -- Apple event that came with the \"Open Application\" event.
  keyboard-modifiers -- Number representing keyboard modifiers on startup.
    See also `mac-keyboard-modifier-mask-alist'.  */);
  Vmac_startup_options = Qnil;

  DEFVAR_LISP ("mac-ts-active-input-overlay", Vmac_ts_active_input_overlay,
    doc: /* Overlay used to display Mac TSM active input area.  */);
  Vmac_ts_active_input_overlay = Qnil;

  DEFVAR_LISP ("mac-ts-script-language-on-focus", Vmac_ts_script_language_on_focus,
    doc: /* *How to change Mac TSM script/language when a frame gets focus.
If the value is t, the input script and language are restored to those
used in the last focus frame.  If the value is a pair of integers, the
input script and language codes, which are defined in the Script
Manager, are set to its car and cdr parts, respectively.  Otherwise,
Emacs doesn't set them and thus follows the system default behavior.  */);
  Vmac_ts_script_language_on_focus = Qnil;
}
