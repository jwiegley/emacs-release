/* Menu support for GNU Emacs on Mac OS.
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

#include <stdio.h>
#include <setjmp.h>

#include "lisp.h"
#include "keyboard.h"
#include "keymap.h"
#include "frame.h"
#include "termhooks.h"
#include "window.h"
#include "blockinput.h"
#include "buffer.h"
#include "charset.h"
#include "coding.h"

/* This may include sys/types.h, and that somehow loses
   if this is not done before the other system files.  */
#include "macterm.h"

/* Load sys/types.h if not already loaded.
   In some systems loading it twice is suicidal.  */
#ifndef makedev
#include <sys/types.h>
#endif

#include "dispextern.h"

#define HAVE_DIALOGS 1

#undef HAVE_MULTILINGUAL_MENU

#include "menu.h"

#ifndef TRUE
#define TRUE 1
#define FALSE 0
#endif /* no TRUE */

static Lisp_Object Qdebug_on_next_call;

extern Lisp_Object Qmac_apple_event;

void set_frame_menubar (FRAME_PTR, int, int);

#ifdef HAVE_DIALOGS
static Lisp_Object mac_dialog_show (FRAME_PTR, int, Lisp_Object, Lisp_Object,
				    const char **);
#endif

/* Nonzero means a menu is currently active.  */
int popup_activated_flag;


static Lisp_Object
cleanup_popup_menu (Lisp_Object arg)
{
  discard_menu_items ();
  return Qnil;
}

#ifdef HAVE_MENUS

DEFUN ("x-popup-dialog", Fx_popup_dialog, Sx_popup_dialog, 2, 3, 0,
       doc: /* Pop up a dialog box and return user's selection.
POSITION specifies which frame to use.
This is normally a mouse button event or a window or frame.
If POSITION is t, it means to use the frame the mouse is on.
The dialog box appears in the middle of the specified frame.

CONTENTS specifies the alternatives to display in the dialog box.
It is a list of the form (DIALOG ITEM1 ITEM2...).
Each ITEM is a cons cell (STRING . VALUE).
The return value is VALUE from the chosen item.

An ITEM may also be just a string--that makes a nonselectable item.
An ITEM may also be nil--that means to put all preceding items
on the left of the dialog box and all following items on the right.
\(By default, approximately half appear on each side.)

If HEADER is non-nil, the frame title for the box is "Information",
otherwise it is "Question".

If the user gets rid of the dialog box without making a valid choice,
for instance using the window manager, then this produces a quit and
`x-popup-dialog' does not return.  */)
  (Lisp_Object position, Lisp_Object contents, Lisp_Object header)
{
  FRAME_PTR f = NULL;
  Lisp_Object window;

  check_mac ();

  /* Decode the first argument: find the window or frame to use.  */
  if (EQ (position, Qt)
      || (CONSP (position) && (EQ (XCAR (position), Qmenu_bar)
			       || EQ (XCAR (position), Qtool_bar)
			       || EQ (XCAR (position), Qmac_apple_event))))
    {
#if 0 /* Using the frame the mouse is on may not be right.  */
      /* Use the mouse's current position.  */
      FRAME_PTR new_f = SELECTED_FRAME ();
      Lisp_Object bar_window;
      enum scroll_bar_part part;
      Time time;
      Lisp_Object x, y;

      (*mouse_position_hook) (&new_f, 1, &bar_window, &part, &x, &y, &time);

      if (new_f != 0)
	XSETFRAME (window, new_f);
      else
	window = selected_window;
#endif
      window = selected_window;
    }
  else if (CONSP (position))
    {
      Lisp_Object tem;
      tem = Fcar (position);
      if (CONSP (tem))
	window = Fcar (Fcdr (position));
      else
	{
	  tem = Fcar (Fcdr (position));  /* EVENT_START (position) */
	  window = Fcar (tem);	     /* POSN_WINDOW (tem) */
	}
    }
  else if (WINDOWP (position) || FRAMEP (position))
    window = position;
  else
    window = Qnil;

  /* Decode where to put the menu.  */

  if (FRAMEP (window))
    f = XFRAME (window);
  else if (WINDOWP (window))
    {
      CHECK_LIVE_WINDOW (window);
      f = XFRAME (WINDOW_FRAME (XWINDOW (window)));
    }
  else
    /* ??? Not really clean; should be CHECK_WINDOW_OR_FRAME,
       but I don't want to make one now.  */
    CHECK_WINDOW (window);

  if (! FRAME_MAC_P (f))
    error ("Can not put dialog on this terminal");

  /* Force a redisplay before showing the dialog.  If a frame is created
     just before showing the dialog, its contents may not have been fully
     drawn.

     Do this before creating the widget value that points to Lisp
     string contents, because Fredisplay may GC and relocate them.  */
  Fredisplay (Qt);

#ifndef HAVE_DIALOGS
  /* Display a menu with these alternatives
     in the middle of frame F.  */
  {
    Lisp_Object x, y, frame, newpos;
    XSETFRAME (frame, f);
    XSETINT (x, x_pixel_width (f) / 2);
    XSETINT (y, x_pixel_height (f) / 2);
    newpos = Fcons (Fcons (x, Fcons (y, Qnil)), Fcons (frame, Qnil));

    return Fx_popup_menu (newpos,
			  Fcons (Fcar (contents), Fcons (contents, Qnil)));
  }
#else /* HAVE_DIALOGS */
  {
    Lisp_Object title;
    const char *error_name;
    Lisp_Object selection;
    int specpdl_count = SPECPDL_INDEX ();

    /* Decode the dialog items from what was specified.  */
    title = Fcar (contents);
    CHECK_STRING (title);
    record_unwind_protect (unuse_menu_items, Qnil);

    list_of_panes (Fcons (contents, Qnil));

    /* Display them in a dialog box.  */
    record_unwind_protect (cleanup_popup_menu, Qnil);
    BLOCK_INPUT;
    selection = mac_dialog_show (f, 0, title, header, &error_name);
    UNBLOCK_INPUT;
    unbind_to (specpdl_count, Qnil);

    if (error_name) error ("%s", error_name);
    return selection;
  }
#endif /* HAVE_DIALOGS */
}


DEFUN ("x-menu-bar-open-internal", Fx_menu_bar_open_internal, Sx_menu_bar_open_internal, 0, 1, "i",
       doc: /* Start key navigation of the menu bar in FRAME.
This initially opens the first menu bar item and you can then navigate with the
arrow keys, select a menu entry with the return key or cancel with the
escape key.  If FRAME has no menu bar this function does nothing.

If FRAME is nil or not given, use the selected frame.  */)
  (Lisp_Object frame)
{
  int selection;
  FRAME_PTR f = check_x_frame (frame);

  set_frame_menubar (f, 0, 1);
  BLOCK_INPUT;
  selection = mac_activate_menubar (f);
  UNBLOCK_INPUT;

  if (selection)
    find_and_call_menu_selection (f, f->menu_bar_items_used, f->menu_bar_vector,
				  (void *) (intptr_t) selection);

  return Qnil;
}

/* Activate the menu bar of frame F.
   This is called from keyboard.c when it gets the
   MENU_BAR_ACTIVATE_EVENT out of the Emacs event queue.

   To activate the menu bar, we call mac_activate_menubar.

   But first we recompute the menu bar contents (the whole tree).

   The reason for saving the button event until here, instead of
   passing it to the toolkit right away, is that we can safely
   execute Lisp code.  */

void
x_activate_menubar (FRAME_PTR f)
{
  int selection;

  if (! FRAME_MAC_P (f))
    abort ();

  set_frame_menubar (f, 0, 1);
  BLOCK_INPUT;
  selection = mac_activate_menubar (f);
  UNBLOCK_INPUT;

  if (selection)
    find_and_call_menu_selection (f, f->menu_bar_items_used, f->menu_bar_vector,
				  (void *) (intptr_t) selection);
}


/* Set the contents of the menubar widgets of frame F.
   The argument FIRST_TIME is currently ignored;
   it is set the first time this is called, from initialize_frame_menubar.  */

void
set_frame_menubar (FRAME_PTR f, int first_time, int deep_p)
{
  int menubar_widget = f->output_data.mac->menubar_widget;
  Lisp_Object items;
  widget_value *wv, *first_wv, *prev_wv = 0;
  int i;
  int *submenu_start, *submenu_end;
  int *submenu_top_level_items, *submenu_n_panes;

  if (! FRAME_MAC_P (f))
    abort ();

  XSETFRAME (Vmenu_updating_frame, f);

  /* This seems to be unnecessary for Carbon.  */
#if 0
  if (! menubar_widget)
    deep_p = 1;
#endif

  if (deep_p)
    {
      /* Make a widget-value tree representing the entire menu trees.  */

      struct buffer *prev = current_buffer;
      Lisp_Object buffer;
      int specpdl_count = SPECPDL_INDEX ();
      int previous_menu_items_used = f->menu_bar_items_used;
      Lisp_Object *previous_items
	= (Lisp_Object *) alloca (previous_menu_items_used
				  * sizeof (Lisp_Object));
      int subitems;

      /* If we are making a new widget, its contents are empty,
	 do always reinitialize them.  */
      if (! menubar_widget)
	previous_menu_items_used = 0;

      buffer = XWINDOW (FRAME_SELECTED_WINDOW (f))->buffer;
      specbind (Qinhibit_quit, Qt);
      /* Don't let the debugger step into this code
	 because it is not reentrant.  */
      specbind (Qdebug_on_next_call, Qnil);

      record_unwind_save_match_data ();
      if (NILP (Voverriding_local_map_menu_flag))
	{
	  specbind (Qoverriding_terminal_local_map, Qnil);
	  specbind (Qoverriding_local_map, Qnil);
	}

      set_buffer_internal_1 (XBUFFER (buffer));

      /* Run the Lucid hook.  */
      safe_run_hooks (Qactivate_menubar_hook);

      /* If it has changed current-menubar from previous value,
	 really recompute the menubar from the value.  */
      if (! NILP (Vlucid_menu_bar_dirty_flag))
	call0 (Qrecompute_lucid_menubar);
      safe_run_hooks (Qmenu_bar_update_hook);
      FRAME_MENU_BAR_ITEMS (f) = menu_bar_items (FRAME_MENU_BAR_ITEMS (f));

      items = FRAME_MENU_BAR_ITEMS (f);

      /* Save the frame's previous menu bar contents data.  */
      if (previous_menu_items_used)
	memcpy (previous_items, XVECTOR (f->menu_bar_vector)->contents,
		previous_menu_items_used * sizeof (Lisp_Object));

      /* Fill in menu_items with the current menu bar contents.
	 This can evaluate Lisp code.  */
      save_menu_items ();

      menu_items = f->menu_bar_vector;
      menu_items_allocated = VECTORP (menu_items) ? ASIZE (menu_items) : 0;
      subitems = ASIZE (items) / 4;
      submenu_start = (int *) alloca ((subitems + 1) * sizeof (int));
      submenu_end = (int *) alloca (subitems * sizeof (int));
      submenu_n_panes = (int *) alloca (subitems * sizeof (int));
      submenu_top_level_items = (int *) alloca (subitems * sizeof (int));
      init_menu_items ();
      for (i = 0; i < subitems; i++)
	{
	  Lisp_Object key, string, maps;

	  key = XVECTOR (items)->contents[4 * i];
	  string = XVECTOR (items)->contents[4 * i + 1];
	  maps = XVECTOR (items)->contents[4 * i + 2];
	  if (NILP (string))
	    break;

	  submenu_start[i] = menu_items_used;

	  menu_items_n_panes = 0;
	  submenu_top_level_items[i]
	    = parse_single_submenu (key, string, maps);
	  submenu_n_panes[i] = menu_items_n_panes;

	  submenu_end[i] = menu_items_used;
	}

      submenu_start[i] = -1;
      finish_menu_items ();

      /* Convert menu_items into widget_value trees
	 to display the menu.  This cannot evaluate Lisp code.  */

      wv = xmalloc_widget_value ();
      wv->name = "menubar";
      wv->value = 0;
      wv->enabled = 1;
      wv->button_type = BUTTON_TYPE_NONE;
      wv->help = Qnil;
      first_wv = wv;

      for (i = 0; 0 <= submenu_start[i]; i++)
	{
	  menu_items_n_panes = submenu_n_panes[i];
	  wv = digest_single_submenu (submenu_start[i], submenu_end[i],
				      submenu_top_level_items[i]);
	  if (prev_wv)
	    prev_wv->next = wv;
	  else
	    first_wv->contents = wv;
	  /* Don't set wv->name here; GC during the loop might relocate it.  */
	  wv->enabled = 1;
	  wv->button_type = BUTTON_TYPE_NONE;
	  prev_wv = wv;
	}

      set_buffer_internal_1 (prev);

      /* If there has been no change in the Lisp-level contents
	 of the menu bar, skip redisplaying it.  Just exit.  */

      /* Compare the new menu items with the ones computed last time.  */
      for (i = 0; i < previous_menu_items_used; i++)
	if (menu_items_used == i
	    || (!EQ (previous_items[i], XVECTOR (menu_items)->contents[i])))
	  break;
      if (i == menu_items_used && i == previous_menu_items_used && i != 0)
	{
	  /* The menu items have not changed.  Don't bother updating
	     the menus in any form, since it would be a no-op.  */
	  free_menubar_widget_value_tree (first_wv);
	  discard_menu_items ();
	  unbind_to (specpdl_count, Qnil);
	  return;
	}

      /* The menu items are different, so store them in the frame.  */
      f->menu_bar_vector = menu_items;
      f->menu_bar_items_used = menu_items_used;

      /* This undoes save_menu_items.  */
      unbind_to (specpdl_count, Qnil);

      /* Now GC cannot happen during the lifetime of the widget_value,
	 so it's safe to store data from a Lisp_String.  */
      wv = first_wv->contents;
      for (i = 0; i < ASIZE (items); i += 4)
	{
	  Lisp_Object string;
	  string = XVECTOR (items)->contents[i + 1];
	  if (NILP (string))
            break;
          wv->name = SSDATA (string);
          update_submenu_strings (wv->contents);
          wv = wv->next;
	}

    }
  else
    {
      /* Make a widget-value tree containing
	 just the top level menu bar strings.  */

      wv = xmalloc_widget_value ();
      wv->name = "menubar";
      wv->value = 0;
      wv->enabled = 1;
      wv->button_type = BUTTON_TYPE_NONE;
      wv->help = Qnil;
      first_wv = wv;

      items = FRAME_MENU_BAR_ITEMS (f);
      for (i = 0; i < ASIZE (items); i += 4)
	{
	  Lisp_Object string;

	  string = XVECTOR (items)->contents[i + 1];
	  if (NILP (string))
	    break;

	  wv = xmalloc_widget_value ();
	  wv->name = SSDATA (string);
	  wv->value = 0;
	  wv->enabled = 1;
	  wv->button_type = BUTTON_TYPE_NONE;
	  wv->help = Qnil;
	  /* This prevents lwlib from assuming this
	     menu item is really supposed to be empty.  */
	  /* The intptr_t cast avoids a warning.
	     This value just has to be different from small integers.  */
	  wv->call_data = (void *) (intptr_t) (-1);

	  if (prev_wv)
	    prev_wv->next = wv;
	  else
	    first_wv->contents = wv;
	  prev_wv = wv;
	}

      /* Forget what we thought we knew about what is in the
	 detailed contents of the menu bar menus.
	 Changing the top level always destroys the contents.  */
      f->menu_bar_items_used = 0;
    }

  /* Create or update the menu bar widget.  */

  BLOCK_INPUT;

  /* Non-null value to indicate menubar has already been "created".  */
  f->output_data.mac->menubar_widget = 1;

  mac_fill_menubar (first_wv->contents, deep_p);

  free_menubar_widget_value_tree (first_wv);

  UNBLOCK_INPUT;
}

/* Get rid of the menu bar of frame F, and free its storage.
   This is used when deleting a frame, and when turning off the menu bar.  */

void
free_frame_menubar (FRAME_PTR f)
{
  f->output_data.mac->menubar_widget = 0;
}


/* Mac_menu_show actually displays a menu using the panes and items in
   menu_items and returns the value selected from it; we assume input
   is blocked by the caller.  */

/* F is the frame the menu is for.
   X and Y are the frame-relative specified position,
   relative to the inside upper left corner of the frame F.
   FOR_CLICK is nonzero if this menu was invoked for a mouse click.
   KEYMAPS is 1 if this menu was specified with keymaps;
    in that case, we return a list containing the chosen item's value
    and perhaps also the pane's prefix.
   TITLE is the specified menu title.
   ERROR is a place to store an error message string in case of failure.
   (We return nil on failure, but the value doesn't actually matter.)  */

Lisp_Object
mac_menu_show (FRAME_PTR f, int x, int y, int for_click, int keymaps,
	       Lisp_Object title, const char **error_name)
{
  int i, selection;
  widget_value *wv, *save_wv = 0, *first_wv = 0, *prev_wv = 0;
  widget_value **submenu_stack
    = (widget_value **) alloca (menu_items_used * sizeof (widget_value *));
  Lisp_Object *subprefix_stack
    = (Lisp_Object *) alloca (menu_items_used * sizeof (Lisp_Object));
  int submenu_depth = 0;

  int first_pane;

  if (! FRAME_MAC_P (f))
    abort ();

  *error_name = NULL;

  if (menu_items_used <= MENU_ITEMS_PANE_LENGTH)
    {
      *error_name = "Empty menu";
      return Qnil;
    }

  /* Create a tree of widget_value objects
     representing the panes and their items.  */
  wv = xmalloc_widget_value ();
  wv->name = "menu";
  wv->value = 0;
  wv->enabled = 1;
  wv->button_type = BUTTON_TYPE_NONE;
  wv->help = Qnil;
  first_wv = wv;
  first_pane = 1;

  /* Loop over all panes and items, filling in the tree.  */
  i = 0;
  while (i < menu_items_used)
    {
      if (EQ (XVECTOR (menu_items)->contents[i], Qnil))
	{
	  submenu_stack[submenu_depth++] = save_wv;
	  save_wv = prev_wv;
	  prev_wv = 0;
	  first_pane = 1;
	  i++;
	}
      else if (EQ (XVECTOR (menu_items)->contents[i], Qlambda))
	{
	  prev_wv = save_wv;
	  save_wv = submenu_stack[--submenu_depth];
	  first_pane = 0;
	  i++;
	}
      else if (EQ (XVECTOR (menu_items)->contents[i], Qt)
	       && submenu_depth != 0)
	i += MENU_ITEMS_PANE_LENGTH;
      /* Ignore a nil in the item list.
	 It's meaningful only for dialog boxes.  */
      else if (EQ (XVECTOR (menu_items)->contents[i], Qquote))
	i += 1;
      else if (EQ (XVECTOR (menu_items)->contents[i], Qt))
	{
	  /* Create a new pane.  */
	  Lisp_Object pane_name, prefix;
	  const char *pane_string;

	  pane_name = AREF (menu_items, i + MENU_ITEMS_PANE_NAME);
	  prefix = AREF (menu_items, i + MENU_ITEMS_PANE_PREFIX);

#ifndef HAVE_MULTILINGUAL_MENU
	  if (STRINGP (pane_name) && STRING_MULTIBYTE (pane_name))
	    {
	      pane_name = ENCODE_MENU_STRING (pane_name);
	      ASET (menu_items, i + MENU_ITEMS_PANE_NAME, pane_name);
	    }
#endif
	  pane_string = (NILP (pane_name)
			 ? "" : SSDATA (pane_name));
	  /* If there is just one top-level pane, put all its items directly
	     under the top-level menu.  */
	  if (menu_items_n_panes == 1)
	    pane_string = "";

	  /* If the pane has a meaningful name,
	     make the pane a top-level menu item
	     with its items as a submenu beneath it.  */
	  if (!keymaps && strcmp (pane_string, ""))
	    {
	      wv = xmalloc_widget_value ();
	      if (save_wv)
		save_wv->next = wv;
	      else
		first_wv->contents = wv;
	      wv->name = pane_string;
	      if (keymaps && !NILP (prefix))
		wv->name++;
	      wv->value = 0;
	      wv->enabled = 1;
	      wv->button_type = BUTTON_TYPE_NONE;
	      wv->help = Qnil;
	      save_wv = wv;
	      prev_wv = 0;
	    }
	  else if (first_pane)
	    {
	      save_wv = wv;
	      prev_wv = 0;
	    }
	  first_pane = 0;
	  i += MENU_ITEMS_PANE_LENGTH;
	}
      else
	{
	  /* Create a new item within current pane.  */
	  Lisp_Object item_name, enable, descrip, def, type, selected, help;
	  item_name = AREF (menu_items, i + MENU_ITEMS_ITEM_NAME);
	  enable = AREF (menu_items, i + MENU_ITEMS_ITEM_ENABLE);
	  descrip = AREF (menu_items, i + MENU_ITEMS_ITEM_EQUIV_KEY);
	  def = AREF (menu_items, i + MENU_ITEMS_ITEM_DEFINITION);
	  type = AREF (menu_items, i + MENU_ITEMS_ITEM_TYPE);
	  selected = AREF (menu_items, i + MENU_ITEMS_ITEM_SELECTED);
	  help = AREF (menu_items, i + MENU_ITEMS_ITEM_HELP);

#ifndef HAVE_MULTILINGUAL_MENU
          if (STRINGP (item_name) && STRING_MULTIBYTE (item_name))
	    {
	      item_name = ENCODE_MENU_STRING (item_name);
	      ASET (menu_items, i + MENU_ITEMS_ITEM_NAME, item_name);
	    }

          if (STRINGP (descrip) && STRING_MULTIBYTE (descrip))
	    {
	      descrip = ENCODE_MENU_STRING (descrip);
	      ASET (menu_items, i + MENU_ITEMS_ITEM_EQUIV_KEY, descrip);
	    }
#endif /* not HAVE_MULTILINGUAL_MENU */

	  wv = xmalloc_widget_value ();
	  if (prev_wv)
	    prev_wv->next = wv;
	  else
	    save_wv->contents = wv;
	  wv->name = SSDATA (item_name);
	  if (!NILP (descrip))
	    wv->key = SSDATA (descrip);
	  wv->value = 0;
	  /* Use the contents index as call_data, since we are
             restricted to 16-bits.  */
	  wv->call_data = !NILP (def) ? (void *) (intptr_t) i : 0;
	  wv->enabled = !NILP (enable);

	  if (NILP (type))
	    wv->button_type = BUTTON_TYPE_NONE;
	  else if (EQ (type, QCtoggle))
	    wv->button_type = BUTTON_TYPE_TOGGLE;
	  else if (EQ (type, QCradio))
	    wv->button_type = BUTTON_TYPE_RADIO;
	  else
	    abort ();

	  wv->selected = !NILP (selected);

          if (! STRINGP (help))
	    help = Qnil;

	  wv->help = help;

	  prev_wv = wv;

	  i += MENU_ITEMS_ITEM_LENGTH;
	}
    }

  /* Deal with the title, if it is non-nil.  */
  if (!NILP (title))
    {
      widget_value *wv_title = xmalloc_widget_value ();
      widget_value *wv_sep = xmalloc_widget_value ();

      /* Maybe replace this separator with a bitmap or owner-draw item
	 so that it looks better.  Having two separators looks odd.  */
      wv_sep->name = "--";
      wv_sep->next = first_wv->contents;
      wv_sep->help = Qnil;

#ifndef HAVE_MULTILINGUAL_MENU
      if (STRING_MULTIBYTE (title))
	title = ENCODE_MENU_STRING (title);
#endif

      wv_title->name = SSDATA (title);
      wv_title->enabled = FALSE;
      wv_title->title = TRUE;
      wv_title->button_type = BUTTON_TYPE_NONE;
      wv_title->help = Qnil;
      wv_title->next = wv_sep;
      first_wv->contents = wv_title;
    }

  /* Actually create and show the menu until popped down.  */
  selection = create_and_show_popup_menu (f, first_wv, x, y, for_click);

  /* Free the widget_value objects we used to specify the contents.  */
  free_menubar_widget_value_tree (first_wv);

  /* Find the selected item, and its pane, to return
     the proper value.  */
  if (selection != 0)
    {
      Lisp_Object prefix, entry;

      prefix = entry = Qnil;
      i = 0;
      while (i < menu_items_used)
	{
	  if (EQ (XVECTOR (menu_items)->contents[i], Qnil))
	    {
	      subprefix_stack[submenu_depth++] = prefix;
	      prefix = entry;
	      i++;
	    }
	  else if (EQ (XVECTOR (menu_items)->contents[i], Qlambda))
	    {
	      prefix = subprefix_stack[--submenu_depth];
	      i++;
	    }
	  else if (EQ (XVECTOR (menu_items)->contents[i], Qt))
	    {
	      prefix
		= XVECTOR (menu_items)->contents[i + MENU_ITEMS_PANE_PREFIX];
	      i += MENU_ITEMS_PANE_LENGTH;
	    }
	  /* Ignore a nil in the item list.
	     It's meaningful only for dialog boxes.  */
	  else if (EQ (XVECTOR (menu_items)->contents[i], Qquote))
	    i += 1;
	  else
	    {
	      entry
		= XVECTOR (menu_items)->contents[i + MENU_ITEMS_ITEM_VALUE];
	      if (selection == i)
		{
		  if (keymaps != 0)
		    {
		      int j;

		      entry = Fcons (entry, Qnil);
		      if (!NILP (prefix))
			entry = Fcons (prefix, entry);
		      for (j = submenu_depth - 1; j >= 0; j--)
			if (!NILP (subprefix_stack[j]))
			  entry = Fcons (subprefix_stack[j], entry);
		    }
		  return entry;
		}
	      i += MENU_ITEMS_ITEM_LENGTH;
	    }
	}
    }
  else if (!for_click)
    /* Make "Cancel" equivalent to C-g.  */
    Fsignal (Qquit, Qnil);

  return Qnil;
}


#ifdef HAVE_DIALOGS
/* Construct native Mac OS dialog based on widget_value tree.  */

static const char * button_names [] = {
  "button1", "button2", "button3", "button4", "button5",
  "button6", "button7", "button8", "button9", "button10" };

static Lisp_Object
cleanup_widget_value_tree (Lisp_Object arg)
{
  struct Lisp_Save_Value *p = XSAVE_VALUE (arg);
  widget_value *wv = p->pointer;

  free_menubar_widget_value_tree (wv);

  return Qnil;
}

static Lisp_Object
mac_dialog_show (FRAME_PTR f,
		 int keymaps,
		 Lisp_Object title,
		 Lisp_Object header,
		 const char **error_name)
{
  int i, selection, nb_buttons=0;
  char dialog_name[6];

  widget_value *wv, *first_wv = 0, *prev_wv = 0;

  /* Number of elements seen so far, before boundary.  */
  int left_count = 0;
  /* 1 means we've seen the boundary between left-hand elts and right-hand.  */
  int boundary_seen = 0;

  int specpdl_count = SPECPDL_INDEX ();

  if (! FRAME_MAC_P (f))
    abort ();

  *error_name = NULL;

  if (menu_items_n_panes > 1)
    {
      *error_name = "Multiple panes in dialog box";
      return Qnil;
    }

  /* Create a tree of widget_value objects
     representing the text label and buttons.  */
  {
    Lisp_Object pane_name, prefix;
    const char *pane_string;
    pane_name = XVECTOR (menu_items)->contents[MENU_ITEMS_PANE_NAME];
    prefix = XVECTOR (menu_items)->contents[MENU_ITEMS_PANE_PREFIX];
    pane_string = (NILP (pane_name)
		   ? "" : SSDATA (pane_name));
    prev_wv = xmalloc_widget_value ();
    prev_wv->value = pane_string;
    if (keymaps && !NILP (prefix))
      prev_wv->name++;
    prev_wv->enabled = 1;
    prev_wv->name = "message";
    prev_wv->help = Qnil;
    first_wv = prev_wv;

    /* Loop over all panes and items, filling in the tree.  */
    i = MENU_ITEMS_PANE_LENGTH;
    while (i < menu_items_used)
      {

	/* Create a new item within current pane.  */
	Lisp_Object item_name, enable, descrip;
	item_name = XVECTOR (menu_items)->contents[i + MENU_ITEMS_ITEM_NAME];
	enable = XVECTOR (menu_items)->contents[i + MENU_ITEMS_ITEM_ENABLE];
	descrip
	  = XVECTOR (menu_items)->contents[i + MENU_ITEMS_ITEM_EQUIV_KEY];

	if (NILP (item_name))
	  {
	    free_menubar_widget_value_tree (first_wv);
	    *error_name = "Submenu in dialog items";
	    return Qnil;
	  }
	if (EQ (item_name, Qquote))
	  {
	    /* This is the boundary between left-side elts
	       and right-side elts.  Stop incrementing right_count.  */
	    boundary_seen = 1;
	    i++;
	    continue;
	  }
	if (nb_buttons >= 9)
	  {
	    free_menubar_widget_value_tree (first_wv);
	    *error_name = "Too many dialog items";
	    return Qnil;
	  }

	wv = xmalloc_widget_value ();
	prev_wv->next = wv;
	wv->name = button_names[nb_buttons];
	if (!NILP (descrip))
	  wv->key = SSDATA (descrip);
	wv->value = SSDATA (item_name);
	wv->call_data = (void *) (intptr_t) i;
	  /* menu item is identified by its index in menu_items table */
	wv->enabled = !NILP (enable);
	wv->help = Qnil;
	prev_wv = wv;

	if (! boundary_seen)
	  left_count++;

	nb_buttons++;
	i += MENU_ITEMS_ITEM_LENGTH;
      }

    /* If the boundary was not specified,
       by default put half on the left and half on the right.  */
    if (! boundary_seen)
      left_count = nb_buttons - nb_buttons / 2;

    wv = xmalloc_widget_value ();
    wv->name = dialog_name;
    wv->help = Qnil;

    /*  Frame title: 'Q' = Question, 'I' = Information.
        Can also have 'E' = Error if, one day, we want
        a popup for errors. */
    if (NILP (header))
      dialog_name[0] = 'Q';
    else
      dialog_name[0] = 'I';

    /* Dialog boxes use a really stupid name encoding
       which specifies how many buttons to use
       and how many buttons are on the right. */
    dialog_name[1] = '0' + nb_buttons;
    dialog_name[2] = 'B';
    dialog_name[3] = 'R';
    /* Number of buttons to put on the right.  */
    dialog_name[4] = '0' + nb_buttons - left_count;
    dialog_name[5] = 0;
    wv->contents = first_wv;
    first_wv = wv;
  }

  /* Make sure to free the widget_value objects we used to specify the
     contents even with longjmp.  */
  record_unwind_protect (cleanup_widget_value_tree,
			 make_save_value (first_wv, 0));

  /* Actually create and show the dialog.  */
  selection = create_and_show_dialog (f, first_wv);

  unbind_to (specpdl_count, Qnil);

  /* Find the selected item, and its pane, to return
     the proper value.  */
  if (selection != 0)
    {
      Lisp_Object prefix;

      prefix = Qnil;
      i = 0;
      while (i < menu_items_used)
	{
	  Lisp_Object entry;

	  if (EQ (XVECTOR (menu_items)->contents[i], Qt))
	    {
	      prefix
		= XVECTOR (menu_items)->contents[i + MENU_ITEMS_PANE_PREFIX];
	      i += MENU_ITEMS_PANE_LENGTH;
	    }
	  else if (EQ (XVECTOR (menu_items)->contents[i], Qquote))
	    {
	      /* This is the boundary between left-side elts and
		 right-side elts.  */
	      ++i;
	    }
	  else
	    {
	      entry
		= XVECTOR (menu_items)->contents[i + MENU_ITEMS_ITEM_VALUE];
	      if (selection == i)
		{
		  if (keymaps != 0)
		    {
		      entry = Fcons (entry, Qnil);
		      if (!NILP (prefix))
			entry = Fcons (prefix, entry);
		    }
		  return entry;
		}
	      i += MENU_ITEMS_ITEM_LENGTH;
	    }
	}
    }
  else
    /* Make "Cancel" equivalent to C-g.  */
    Fsignal (Qquit, Qnil);

  return Qnil;
}
#endif  /* HAVE_DIALOGS  */


/* Is this item a separator? */
int
name_is_separator (const char *name)
{
  const char *start = name;

  /* Check if name string consists of only dashes ('-').  */
  while (*name == '-') name++;
  /* Separators can also be of the form "--:TripleSuperMegaEtched"
     or "--deep-shadow".  We don't implement them yet, se we just treat
     them like normal separators.  */
  return (*name == '\0' || start + 2 == name);
}
#endif /* HAVE_MENUS */

/* Detect if a menu is currently active.  */

int
popup_activated (void)
{
  return popup_activated_flag;
}

/* The following is used by delayed window autoselection.  */

DEFUN ("menu-or-popup-active-p", Fmenu_or_popup_active_p, Smenu_or_popup_active_p, 0, 0, 0,
       doc: /* Return t if a menu or popup dialog is active.  */)
  (void)
{
  return (popup_activated ()) ? Qt : Qnil;
}

void
syms_of_macmenu (void)
{
  DEFSYM (Qdebug_on_next_call, "debug-on-next-call");

  defsubr (&Smenu_or_popup_active_p);

  defsubr (&Sx_menu_bar_open_internal);
  Ffset (intern_c_string ("accelerate-menu"),
	 intern_c_string (Sx_menu_bar_open_internal.symbol_name));

#ifdef HAVE_MENUS
  defsubr (&Sx_popup_dialog);
#endif

  DEFVAR_LISP ("mac-help-topics", Vmac_help_topics,
    doc: /* List of strings shown as Help topics by Help menu search.
Each element should be a unibyte string in UTF-8.  The special value t
means not to recalculate help topics.  This customizable Help menu
search feature works only when compiled and run on Mac OS X 10.6 and
later, and otherwise the value is kept to t so as to avoid needless
recalculation.  */);
  Vmac_help_topics = Qt;
}
