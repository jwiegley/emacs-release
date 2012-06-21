/* Selection processing for Emacs on Mac OS.
   Copyright (C) 2005-2008 Free Software Foundation, Inc.
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

#include <config.h>
#include <setjmp.h>

#include "lisp.h"
#include "macterm.h"
#include "blockinput.h"
#include "keymap.h"
#include "termhooks.h"
#include "keyboard.h"

static void x_own_selection (Lisp_Object, Lisp_Object, Lisp_Object);
static Lisp_Object x_get_local_selection (Lisp_Object, Lisp_Object, int,
					  struct mac_display_info *);
static Lisp_Object x_get_foreign_selection (Lisp_Object, Lisp_Object,
					    Lisp_Object, Lisp_Object);

static Lisp_Object QSECONDARY, QTIMESTAMP, QTARGETS;

static Lisp_Object Qforeign_selection;
static Lisp_Object Qx_lost_selection_functions;

#define LOCAL_SELECTION(selection_symbol,dpyinfo)			\
  assq_no_quit (selection_symbol, dpyinfo->terminal->Vselection_alist)

/* A selection name (represented as a Lisp symbol) can be associated
   with a pasteboard via `mac-pasteboard-name' property.  Likewise for
   a selection type with a pasteboard data type via
   `mac-pasteboard-data-type'.  */
Lisp_Object Qmac_pasteboard_name, Qmac_pasteboard_data_type;


static int
x_selection_owner_p (Lisp_Object selection, struct mac_display_info *dpyinfo)
{
  OSStatus err;
  Selection sel;
  Lisp_Object local_selection_data;
  int result = 0;

  local_selection_data = LOCAL_SELECTION (selection, dpyinfo);

  if (NILP (local_selection_data))
    return 0;

  BLOCK_INPUT;

  err = mac_get_selection_from_symbol (selection, 0, &sel);
  if (err == noErr && sel)
    {
      Lisp_Object ownership_info;

      ownership_info = XCAR (XCDR (XCDR (XCDR (XCDR (local_selection_data)))));
      if (!NILP (Fequal (ownership_info,
			 mac_get_selection_ownership_info (sel))))
	result = 1;
    }
  else
    result = 1;

  UNBLOCK_INPUT;

  return result;
}

/* Do protocol to assert ourself as a selection owner.
   FRAME shall be the owner; it must be a valid X frame.
   Update the Vselection_alist so that we can reply to later requests for
   our selection.  */

static void
x_own_selection (Lisp_Object selection_name, Lisp_Object selection_value,
		 Lisp_Object frame)
{
  struct frame *f = XFRAME (frame);
  struct mac_display_info *dpyinfo = FRAME_MAC_DISPLAY_INFO (f);
  Time timestamp = last_event_timestamp;
  OSStatus err;
  Selection sel;
  struct gcpro gcpro1, gcpro2;
  Lisp_Object rest, handler_fn, value, target_type;
  int count;

  GCPRO2 (selection_name, selection_value);

  BLOCK_INPUT;

  err = mac_get_selection_from_symbol (selection_name, 1, &sel);
  if (err == noErr && sel)
    {
      /* Don't allow a quit within the converter.
	 When the user types C-g, he would be surprised
	 if by luck it came during a converter.  */
      count = SPECPDL_INDEX ();
      specbind (Qinhibit_quit, Qt);

      for (rest = Vselection_converter_alist; CONSP (rest); rest = XCDR (rest))
	{
	  if (!(CONSP (XCAR (rest))
		&& (target_type = XCAR (XCAR (rest)),
		    SYMBOLP (target_type))
		&& mac_valid_selection_target_p (target_type)
		&& (handler_fn = XCDR (XCAR (rest)),
		    SYMBOLP (handler_fn))))
	    continue;

	  if (!NILP (handler_fn))
	    value = call3 (handler_fn, selection_name,
			   target_type, selection_value);
	  else
	    value = Qnil;

	  if (NILP (value))
	    continue;

	  if (mac_valid_selection_value_p (value, target_type))
	    err = mac_put_selection_value (sel, target_type, value);
	  else if (CONSP (value)
		   && EQ (XCAR (value), target_type)
		   && mac_valid_selection_value_p (XCDR (value), target_type))
	    err = mac_put_selection_value (sel, target_type, XCDR (value));
	}

      unbind_to (count, Qnil);
    }

  UNBLOCK_INPUT;

  UNGCPRO;

  if (sel && err != noErr)
    error ("Can't set selection");

  /* Now update the local cache */
  {
    Lisp_Object selection_data;
    Lisp_Object ownership_info;
    Lisp_Object prev_value;

    if (sel)
      {
	BLOCK_INPUT;
	ownership_info = mac_get_selection_ownership_info (sel);
	UNBLOCK_INPUT;
      }
    else
      ownership_info = Qnil; 	/* dummy value for local-only selection */
    selection_data = list5 (selection_name, selection_value,
			    INTEGER_TO_CONS (timestamp), frame, ownership_info);
    prev_value = LOCAL_SELECTION (selection_name, dpyinfo);

    dpyinfo->terminal->Vselection_alist
      = Fcons (selection_data, dpyinfo->terminal->Vselection_alist);

    /* If we already owned the selection, remove the old selection
       data.  Don't use Fdelq as that may QUIT.  */
    if (!NILP (prev_value))
      {
	/* We know it's not the CAR, so it's easy.  */
	Lisp_Object rest = dpyinfo->terminal->Vselection_alist;
	for (; CONSP (rest); rest = XCDR (rest))
	  if (EQ (prev_value, Fcar (XCDR (rest))))
	    {
	      XSETCDR (rest, XCDR (XCDR (rest)));
	      break;
	    }
      }
  }
}

/* Given a selection-name and desired type, look up our local copy of
   the selection value and convert it to the type.
   Return nil, a string, a vector, a symbol, an integer, or a cons
   that CONS_TO_INTEGER could plausibly handle.
   This function is used both for remote requests (LOCAL_REQUEST is zero)
   and for local x-get-selection-internal (LOCAL_REQUEST is nonzero).

   This calls random Lisp code, and may signal or gc.  */

static Lisp_Object
x_get_local_selection (Lisp_Object selection_symbol, Lisp_Object target_type,
		       int local_request, struct mac_display_info *dpyinfo)
{
  Lisp_Object local_value;
  Lisp_Object handler_fn, value, type, check;
  int count;

  if (!x_selection_owner_p (selection_symbol, dpyinfo))
    return Qnil;

  local_value = LOCAL_SELECTION (selection_symbol, dpyinfo);

  /* TIMESTAMP is a special case.  */
  if (EQ (target_type, QTIMESTAMP))
    {
      handler_fn = Qnil;
      value = XCAR (XCDR (XCDR (local_value)));
    }
  else
    {
      /* Don't allow a quit within the converter.
	 When the user types C-g, he would be surprised
	 if by luck it came during a converter.  */
      count = SPECPDL_INDEX ();
      specbind (Qinhibit_quit, Qt);

      CHECK_SYMBOL (target_type);
      handler_fn = Fcdr (Fassq (target_type, Vselection_converter_alist));
      /* gcpro is not needed here since nothing but HANDLER_FN
	 is live, and that ought to be a symbol.  */

      if (!NILP (handler_fn))
	value = call3 (handler_fn,
		       selection_symbol, (local_request ? Qnil : target_type),
		       XCAR (XCDR (local_value)));
      else
	value = Qnil;
      unbind_to (count, Qnil);
    }

  if (local_request)
    return value;

  /* Make sure this value is of a type that we could transmit
     to another application.  */

  type = target_type;
  check = value;
  if (CONSP (value)
      && SYMBOLP (XCAR (value)))
    type = XCAR (value),
    check = XCDR (value);

  if (NILP (value) || mac_valid_selection_value_p (check, type))
    return value;

  signal_error ("Invalid data returned by selection-conversion function",
		list2 (handler_fn, value));
}


/* Clear all selections that were made from frame F.
   We do this when about to delete a frame.  */

void
x_clear_frame_selections (FRAME_PTR f)
{
  Lisp_Object frame;
  Lisp_Object rest;
  struct mac_display_info *dpyinfo = FRAME_MAC_DISPLAY_INFO (f);
  struct terminal *t = dpyinfo->terminal;

  XSETFRAME (frame, f);

  /* Delete elements from the beginning of Vselection_alist.  */
  while (CONSP (t->Vselection_alist)
	 && EQ (frame, XCAR (XCDR (XCDR (XCDR (XCAR (t->Vselection_alist)))))))
    {
      /* Run the `x-lost-selection-functions' abnormal hook.  */
      Lisp_Object args[2];
      args[0] = Qx_lost_selection_functions;
      args[1] = Fcar (Fcar (t->Vselection_alist));
      if (x_selection_owner_p (args[1], dpyinfo))
	Frun_hook_with_args (2, args);

      t->Vselection_alist = XCDR (t->Vselection_alist);
    }

  /* Delete elements after the beginning of Vselection_alist.  */
  for (rest = t->Vselection_alist; CONSP (rest); rest = XCDR (rest))
    if (CONSP (XCDR (rest))
	&& EQ (frame, XCAR (XCDR (XCDR (XCDR (XCAR (XCDR (rest))))))))
      {
	Lisp_Object args[2];
	args[0] = Qx_lost_selection_functions;
	args[1] = XCAR (XCAR (XCDR (rest)));
	if (x_selection_owner_p (args[1], dpyinfo))
	  Frun_hook_with_args (2, args);
	XSETCDR (rest, XCDR (XCDR (rest)));
	break;
      }
}

/* Do protocol to read selection-data from the server.
   Converts this to Lisp data and returns it.  */

static Lisp_Object
x_get_foreign_selection (Lisp_Object selection_symbol, Lisp_Object target_type,
			 Lisp_Object time_stamp, Lisp_Object frame)
{
  struct frame *f = XFRAME (frame);
  OSStatus err;
  Selection sel;
  Lisp_Object result = Qnil;

  if (!FRAME_LIVE_P (f))
    return Qnil;

  BLOCK_INPUT;

  err = mac_get_selection_from_symbol (selection_symbol, 0, &sel);
  if (err == noErr && sel)
    {
      if (EQ (target_type, QTARGETS))
	{
	  Lisp_Object args[2];

	  args[0] = list1 (QTARGETS);
	  args[1] = mac_get_selection_target_list (sel);
	  result = Fvconcat (2, args);
	}
      else
	{
	  result = mac_get_selection_value (sel, target_type);
	  if (STRINGP (result))
	    Fput_text_property (make_number (0), make_number (SBYTES (result)),
				Qforeign_selection, target_type, result);
	}
    }

  UNBLOCK_INPUT;

  return result;
}


/* From a Lisp_Object, return a suitable frame for selection
   operations.  OBJECT may be a frame, a terminal object, or nil
   (which stands for the selected frame--or, if that is not a Mac
   frame, the first Mac display on the list).  If no suitable frame can
   be found, return NULL.  */

static struct frame *
frame_for_x_selection (Lisp_Object object)
{
  Lisp_Object tail;
  struct frame *f;

  if (NILP (object))
    {
      f = XFRAME (selected_frame);
      if (FRAME_MAC_P (f) && FRAME_LIVE_P (f))
	return f;

      for (tail = Vframe_list; CONSP (tail); tail = XCDR (tail))
	{
	  f = XFRAME (XCAR (tail));
	  if (FRAME_MAC_P (f) && FRAME_LIVE_P (f))
	    return f;
	}
    }
  else if (TERMINALP (object))
    {
      struct terminal *t = get_terminal (object, 1);
      if (t->type == output_mac)
	{
	  for (tail = Vframe_list; CONSP (tail); tail = XCDR (tail))
	    {
	      f = XFRAME (XCAR (tail));
	      if (FRAME_LIVE_P (f) && f->terminal == t)
		return f;
	    }
	}
    }
  else if (FRAMEP (object))
    {
      f = XFRAME (object);
      if (FRAME_MAC_P (f) && FRAME_LIVE_P (f))
	return f;
    }

  return NULL;
}


DEFUN ("x-own-selection-internal", Fx_own_selection_internal,
       Sx_own_selection_internal, 2, 3, 0,
       doc: /* Assert a selection of type SELECTION and value VALUE.
SELECTION is a symbol, typically `PRIMARY', `SECONDARY', or `CLIPBOARD'.
VALUE is typically a string, or a cons of two markers, but may be
anything that the functions on `selection-converter-alist' know about.

FRAME should be a frame that should own the selection.  If omitted or
nil, it defaults to the selected frame.  */)
  (Lisp_Object selection, Lisp_Object value, Lisp_Object frame)
{
  if (NILP (frame)) frame = selected_frame;
  if (!FRAME_LIVE_P (XFRAME (frame)) || !FRAME_MAC_P (XFRAME (frame)))
    error ("Selection unavailable for this frame");

  CHECK_SYMBOL (selection);
  if (NILP (value)) error ("VALUE may not be nil");
  x_own_selection (selection, value, frame);
  return value;
}


/* Request the selection value from the owner.  If we are the owner,
   simply return our selection value.  If we are not the owner, this
   will block until all of the data has arrived.  */

DEFUN ("x-get-selection-internal", Fx_get_selection_internal,
       Sx_get_selection_internal, 2, 4, 0,
       doc: /* Return text selected from some Mac application.
SELECTION is a symbol, typically `PRIMARY', `SECONDARY', or `CLIPBOARD'.
TYPE is the type of data desired, typically `STRING'.
TIME_STAMP is ignored on Mac.

TERMINAL should be a terminal object or a frame specifying the
server to query.  If omitted or nil, that stands for the selected
frame's display, or the first available display.  */)
  (Lisp_Object selection_symbol, Lisp_Object target_type,
   Lisp_Object time_stamp, Lisp_Object terminal)
{
  Lisp_Object val = Qnil;
  struct gcpro gcpro1, gcpro2;
  struct frame *f = frame_for_x_selection (terminal);
  GCPRO2 (target_type, val); /* we store newly consed data into these */

  CHECK_SYMBOL (selection_symbol);
  CHECK_SYMBOL (target_type);
  if (!f)
    error ("Selection unavailable for this frame");

  val = x_get_local_selection (selection_symbol, target_type, 1,
			       FRAME_MAC_DISPLAY_INFO (f));

  if (NILP (val) && FRAME_LIVE_P (f))
    {
      Lisp_Object frame;
      XSETFRAME (frame, f);
      RETURN_UNGCPRO (x_get_foreign_selection (selection_symbol, target_type,
					       time_stamp, frame));
    }

  if (CONSP (val) && SYMBOLP (XCAR (val)))
    {
      val = XCDR (val);
      if (CONSP (val) && NILP (XCDR (val)))
	val = XCAR (val);
    }
  RETURN_UNGCPRO (val);
}

DEFUN ("x-disown-selection-internal", Fx_disown_selection_internal,
       Sx_disown_selection_internal, 1, 3, 0,
       doc: /* If we own the selection SELECTION, disown it.
Disowning it means there is no such selection.

TERMINAL should be a terminal object or a frame specifying the
server to query.  If omitted or nil, that stands for the selected
frame's display, or the first available display.  */)
  (Lisp_Object selection, Lisp_Object time_object, Lisp_Object terminal)
{
  struct frame *f = frame_for_x_selection (terminal);
  OSStatus err;
  Selection sel;
  Lisp_Object local_selection_data;
  struct x_display_info *dpyinfo;
  Lisp_Object Vselection_alist;

  if (!f)
    return Qnil;

  dpyinfo = FRAME_MAC_DISPLAY_INFO (f);
  CHECK_SYMBOL (selection);

  if (!x_selection_owner_p (selection, dpyinfo))
    return Qnil;  /* Don't disown the selection when we're not the owner.  */

  local_selection_data = LOCAL_SELECTION (selection, dpyinfo);

  /* Don't use Fdelq as that may QUIT;.  */

  Vselection_alist = dpyinfo->terminal->Vselection_alist;
  if (EQ (local_selection_data, CAR (Vselection_alist)))
    Vselection_alist = XCDR (Vselection_alist);
  else
    {
      Lisp_Object rest;
      for (rest = Vselection_alist; CONSP (rest); rest = XCDR (rest))
	if (EQ (local_selection_data, CAR (XCDR (rest))))
	  {
	    XSETCDR (rest, XCDR (XCDR (rest)));
	    break;
	  }
    }

  dpyinfo->terminal->Vselection_alist = Vselection_alist;

  /* Run the `x-lost-selection-functions' abnormal hook.  */

  {
    Lisp_Object args[2];
    args[0] = Qx_lost_selection_functions;
    args[1] = selection;
    Frun_hook_with_args (2, args);
  }

  prepare_menu_bars ();
  redisplay_preserve_echo_area (20);

  BLOCK_INPUT;

  err = mac_get_selection_from_symbol (selection, 0, &sel);
  if (err == noErr && sel)
    mac_clear_selection (&sel);

  UNBLOCK_INPUT;

  return Qt;
}

DEFUN ("x-selection-owner-p", Fx_selection_owner_p, Sx_selection_owner_p,
       0, 2, 0,
       doc: /* Whether the current Emacs process owns the given SELECTION.
The arg should be the name of the selection in question, typically one of
the symbols `PRIMARY', `SECONDARY', or `CLIPBOARD'.
For convenience, the symbol nil is the same as `PRIMARY',
and t is the same as `SECONDARY'.

TERMINAL should be a terminal object or a frame specifying the
server to query.  If omitted or nil, that stands for the selected
frame's display, or the first available display.  */)
  (Lisp_Object selection, Lisp_Object terminal)
{
  struct frame *f = frame_for_x_selection (terminal);

  CHECK_SYMBOL (selection);
  if (EQ (selection, Qnil)) selection = QPRIMARY;
  if (EQ (selection, Qt)) selection = QSECONDARY;

  if (f && x_selection_owner_p (selection, FRAME_MAC_DISPLAY_INFO (f)))
    return Qt;
  else
    return Qnil;
}

DEFUN ("x-selection-exists-p", Fx_selection_exists_p, Sx_selection_exists_p,
       0, 2, 0,
       doc: /* Whether there is an owner for the given selection.
SELECTION should be the name of the selection in question, typically
one of the symbols `PRIMARY', `SECONDARY', or `CLIPBOARD'.
The symbol nil is the same as `PRIMARY', and t is the same as `SECONDARY'.

TERMINAL should be a terminal object or a frame specifying the
server to query.  If omitted or nil, that stands for the selected
frame's display, or the first available display.  */)
  (Lisp_Object selection, Lisp_Object terminal)
{
  OSStatus err;
  Selection sel;
  Lisp_Object result = Qnil, rest;
  struct frame *f = frame_for_x_selection (terminal);
  struct mac_display_info *dpyinfo;

  CHECK_SYMBOL (selection);
  if (EQ (selection, Qnil)) selection = QPRIMARY;
  if (EQ (selection, Qt)) selection = QSECONDARY;

  if (!f)
    return Qnil;

  dpyinfo = FRAME_MAC_DISPLAY_INFO (f);

  if (x_selection_owner_p (selection, dpyinfo))
    return Qt;

  BLOCK_INPUT;

  err = mac_get_selection_from_symbol (selection, 0, &sel);
  if (err == noErr && sel)
    for (rest = Vselection_converter_alist; CONSP (rest); rest = XCDR (rest))
      {
	if (CONSP (XCAR (rest)) && SYMBOLP (XCAR (XCAR (rest)))
	    && mac_selection_has_target_p (sel, XCAR (XCAR (rest))))
	  {
	    result = Qt;
	    break;
	  }
      }

  UNBLOCK_INPUT;

  return result;
}


/***********************************************************************
			 Apple event support
***********************************************************************/
int mac_ready_for_apple_events = 0;
Lisp_Object Qmac_apple_event_class, Qmac_apple_event_id;
static Lisp_Object Qemacs_suspension_id;
extern Lisp_Object Qundefined;
extern void mac_store_apple_event (Lisp_Object, Lisp_Object,
				   const AEDesc *);

#if __LP64__ && MAC_OS_X_VERSION_MIN_REQUIRED < 1060
static AEEventHandlerUPP AE_USE_STANDARD_DISPATCH;
#else
#define AE_USE_STANDARD_DISPATCH ((AEEventHandlerUPP) kAEUseStandardDispatch)
#endif

struct apple_event_binding
{
  UInt32 code;			/* Apple event class or ID.  */
  Lisp_Object key, binding;
};

struct suspended_ae_info
{
  UInt32 expiration_tick, suspension_id;
  AppleEvent apple_event, reply;
  struct suspended_ae_info *next;
};

/* List of apple events deferred at the startup time.  */
static struct suspended_ae_info *deferred_apple_events = NULL;

/* List of suspended apple events, in order of expiration_tick.  */
static struct suspended_ae_info *suspended_apple_events = NULL;

static void
find_event_binding_fun (Lisp_Object key, Lisp_Object binding, Lisp_Object args,
			void *data)
{
  struct apple_event_binding *event_binding =
    (struct apple_event_binding *)data;
  Lisp_Object code_string;

  if (!SYMBOLP (key))
    return;
  code_string = Fget (key, args);
  if (STRINGP (code_string) && SBYTES (code_string) == 4
      && (EndianU32_BtoN (*((UInt32 *) SDATA (code_string)))
	  == event_binding->code))
    {
      event_binding->key = key;
      event_binding->binding = binding;
    }
}

static void
find_event_binding (Lisp_Object keymap,
		    struct apple_event_binding *event_binding, int class_p)
{
  if (event_binding->code == 0)
    event_binding->binding =
      access_keymap (keymap, event_binding->key, 0, 1, 0);
  else
    {
      event_binding->binding = Qnil;
      map_keymap (keymap, find_event_binding_fun,
		  class_p ? Qmac_apple_event_class : Qmac_apple_event_id,
		  event_binding, 0);
    }
}

void
mac_find_apple_event_spec (AEEventClass class, AEEventID id,
			   Lisp_Object *class_key, Lisp_Object *id_key,
			   Lisp_Object *binding)
{
  struct apple_event_binding event_binding;
  Lisp_Object keymap;

  *binding = Qnil;

  keymap = get_keymap (Vmac_apple_event_map, 0, 0);
  if (NILP (keymap))
    return;

  event_binding.code = class;
  event_binding.key = *class_key;
  event_binding.binding = Qnil;
  find_event_binding (keymap, &event_binding, 1);
  *class_key = event_binding.key;
  keymap = get_keymap (event_binding.binding, 0, 0);
  if (NILP (keymap))
    return;

  event_binding.code = id;
  event_binding.key = *id_key;
  event_binding.binding = Qnil;
  find_event_binding (keymap, &event_binding, 0);
  *id_key = event_binding.key;
  *binding = event_binding.binding;
}

static OSErr
defer_apple_events (const AppleEvent *apple_event, const AppleEvent *reply)
{
  OSErr err;
  struct suspended_ae_info *new;

  new = xmalloc (sizeof (struct suspended_ae_info));
  memset (new, 0, sizeof (struct suspended_ae_info));
  new->apple_event.descriptorType = typeNull;
  new->reply.descriptorType = typeNull;

  err = AESuspendTheCurrentEvent (apple_event);

  /* Mac OS 10.3 Xcode manual says AESuspendTheCurrentEvent makes
     copies of the Apple event and the reply, but Mac OS 10.4 Xcode
     manual says it doesn't.  Anyway we create copies of them and save
     them in `deferred_apple_events'.  */
  if (err == noErr)
    err = AEDuplicateDesc (apple_event, &new->apple_event);
  if (err == noErr)
    err = AEDuplicateDesc (reply, &new->reply);
  if (err == noErr)
    {
      new->next = deferred_apple_events;
      deferred_apple_events = new;
    }
  else
    {
      AEDisposeDesc (&new->apple_event);
      AEDisposeDesc (&new->reply);
      xfree (new);
    }

  return err;
}

static OSErr
mac_handle_apple_event_1 (Lisp_Object class, Lisp_Object id,
			  const AppleEvent *apple_event, AppleEvent *reply)
{
  OSErr err;
  static UInt32 suspension_id = 0;
  struct suspended_ae_info *new;

  new = xmalloc (sizeof (struct suspended_ae_info));
  memset (new, 0, sizeof (struct suspended_ae_info));
  new->apple_event.descriptorType = typeNull;
  new->reply.descriptorType = typeNull;

  err = AESuspendTheCurrentEvent (apple_event);
  if (err == noErr)
    err = AEDuplicateDesc (apple_event, &new->apple_event);
  if (err == noErr)
    err = AEDuplicateDesc (reply, &new->reply);
  if (err == noErr)
    err = AEPutAttributePtr (&new->apple_event, KEY_EMACS_SUSPENSION_ID_ATTR,
			     typeUInt32, &suspension_id, sizeof (UInt32));
  if (err == noErr)
    {
      OSErr err1;
      SInt32 reply_requested;

      err1 = AEGetAttributePtr (&new->apple_event, keyReplyRequestedAttr,
				typeSInt32, NULL, &reply_requested,
				sizeof (SInt32), NULL);
      if (err1 != noErr)
	{
	  /* Emulate keyReplyRequestedAttr in older versions.  */
	  reply_requested = reply->descriptorType != typeNull;
	  err = AEPutAttributePtr (&new->apple_event, keyReplyRequestedAttr,
				   typeSInt32, &reply_requested,
				   sizeof (SInt32));
	}
    }
  if (err == noErr)
    {
      SInt32 timeout = 0;
      struct suspended_ae_info **p;

      new->suspension_id = suspension_id;
      suspension_id++;
      err = AEGetAttributePtr (apple_event, keyTimeoutAttr, typeSInt32,
			       NULL, &timeout, sizeof (SInt32), NULL);
      new->expiration_tick = TickCount () + timeout;

      for (p = &suspended_apple_events; *p; p = &(*p)->next)
	if ((*p)->expiration_tick >= new->expiration_tick)
	  break;
      new->next = *p;
      *p = new;

      mac_store_apple_event (class, id, &new->apple_event);
    }
  else
    {
      AEDisposeDesc (&new->reply);
      AEDisposeDesc (&new->apple_event);
      xfree (new);
    }

  return err;
}

pascal OSErr
mac_handle_apple_event (const AppleEvent *apple_event, AppleEvent *reply,
			SInt32 refcon)
{
  OSErr err;
  UInt32 suspension_id;
  AEEventClass event_class;
  AEEventID event_id;
  Lisp_Object class_key, id_key, binding;

  if (!mac_ready_for_apple_events)
    {
      err = defer_apple_events (apple_event, reply);
      if (err != noErr)
	return errAEEventNotHandled;
      return noErr;
    }

  err = AEGetAttributePtr (apple_event, KEY_EMACS_SUSPENSION_ID_ATTR,
			   typeUInt32, NULL,
			   &suspension_id, sizeof (UInt32), NULL);
  if (err == noErr)
    /* Previously suspended event.  Pass it to the next handler.  */
    return errAEEventNotHandled;

  err = AEGetAttributePtr (apple_event, keyEventClassAttr, typeType, NULL,
			   &event_class, sizeof (AEEventClass), NULL);
  if (err == noErr)
    err = AEGetAttributePtr (apple_event, keyEventIDAttr, typeType, NULL,
			     &event_id, sizeof (AEEventID), NULL);
  if (err == noErr)
    {
      mac_find_apple_event_spec (event_class, event_id,
				 &class_key, &id_key, &binding);
      if (!NILP (binding) && !EQ (binding, Qundefined))
	{
	  if (INTEGERP (binding))
	    return XINT (binding);
	  err = mac_handle_apple_event_1 (class_key, id_key,
					  apple_event, reply);
	}
      else
	err = errAEEventNotHandled;
    }
  if (err == noErr)
    return noErr;
  else
    return errAEEventNotHandled;
}

static int
cleanup_suspended_apple_events (struct suspended_ae_info **head, int all_p)
{
  UInt32 current_tick = TickCount (), nresumed = 0;
  struct suspended_ae_info *p, *next;

  for (p = *head; p; p = next)
    {
      if (!all_p && p->expiration_tick > current_tick)
	break;
      AESetTheCurrentEvent (&p->apple_event);
      AEResumeTheCurrentEvent (&p->apple_event, &p->reply,
			       (AEEventHandlerUPP) kAENoDispatch, 0);
      AEDisposeDesc (&p->reply);
      AEDisposeDesc (&p->apple_event);
      nresumed++;
      next = p->next;
      xfree (p);
    }
  *head = p;

  return nresumed;
}

void
cleanup_all_suspended_apple_events (void)
{
  cleanup_suspended_apple_events (&deferred_apple_events, 1);
  cleanup_suspended_apple_events (&suspended_apple_events, 1);
}

static UInt32
get_suspension_id (Lisp_Object apple_event)
{
  Lisp_Object tem;

  CHECK_CONS (apple_event);
  CHECK_STRING_CAR (apple_event);
  if (SBYTES (XCAR (apple_event)) != 4
      || strcmp (SSDATA (XCAR (apple_event)), "aevt") != 0)
    error ("Not an apple event");

  tem = assq_no_quit (Qemacs_suspension_id, XCDR (apple_event));
  if (NILP (tem))
    error ("Suspension ID not available");

  tem = XCDR (tem);
  if (!(CONSP (tem)
	&& STRINGP (XCAR (tem)) && SBYTES (XCAR (tem)) == 4
	&& strcmp (SSDATA (XCAR (tem)), "magn") == 0
	&& STRINGP (XCDR (tem)) && SBYTES (XCDR (tem)) == 4))
    error ("Bad suspension ID format");

  return *((UInt32 *) SDATA (XCDR (tem)));
}


DEFUN ("mac-process-deferred-apple-events", Fmac_process_deferred_apple_events, Smac_process_deferred_apple_events, 0, 0, 0,
       doc: /* Process Apple events that are deferred at the startup time.  */)
  (void)
{
  if (mac_ready_for_apple_events)
    return Qnil;

  BLOCK_INPUT;
  mac_ready_for_apple_events = 1;
#if __LP64__ && MAC_OS_X_VERSION_MIN_REQUIRED < 1060
  {
    SInt32 response;
    OSErr err;

    err = Gestalt (gestaltSystemVersion, &response);
    if (err == noErr && response < 0x1060)
      AE_USE_STANDARD_DISPATCH = (AEEventHandlerUPP) (long) 0xFFFFFFFF;
    else
      AE_USE_STANDARD_DISPATCH = (AEEventHandlerUPP) (int) 0xFFFFFFFF;
  }
#endif
  if (deferred_apple_events)
    {
      struct suspended_ae_info *prev, *tail, *next;

      /* `nreverse' deferred_apple_events.  */
      prev = NULL;
      for (tail = deferred_apple_events; tail; tail = next)
	{
	  next = tail->next;
	  tail->next = prev;
	  prev = tail;
	}

      /* Now `prev' points to the first cell.  */
      for (tail = prev; tail; tail = next)
	{
	  next = tail->next;
	  AEResumeTheCurrentEvent (&tail->apple_event, &tail->reply,
				   AE_USE_STANDARD_DISPATCH, 0);
	  AEDisposeDesc (&tail->reply);
	  AEDisposeDesc (&tail->apple_event);
	  xfree (tail);
	}

      deferred_apple_events = NULL;
    }
  UNBLOCK_INPUT;

  return Qt;
}

DEFUN ("mac-cleanup-expired-apple-events", Fmac_cleanup_expired_apple_events, Smac_cleanup_expired_apple_events, 0, 0, 0,
       doc: /* Clean up expired Apple events.
Return the number of expired events.   */)
  (void)
{
  int nexpired;

  BLOCK_INPUT;
  nexpired = cleanup_suspended_apple_events (&suspended_apple_events, 0);
  UNBLOCK_INPUT;

  return make_number (nexpired);
}

DEFUN ("mac-ae-set-reply-parameter", Fmac_ae_set_reply_parameter, Smac_ae_set_reply_parameter, 3, 3, 0,
       doc: /* Set parameter KEYWORD to DESCRIPTOR on reply of APPLE-EVENT.
KEYWORD is a 4-byte string.  DESCRIPTOR is a Lisp representation of an
Apple event descriptor.  It has the form of (TYPE . DATA), where TYPE
is a 4-byte string.  Valid format of DATA is as follows:

  * If TYPE is "null", then DATA is nil.
  * If TYPE is "list", then DATA is a list (DESCRIPTOR1 ... DESCRIPTORn).
  * If TYPE is "reco", then DATA is a list ((KEYWORD1 . DESCRIPTOR1)
    ... (KEYWORDn . DESCRIPTORn)).
  * If TYPE is "aevt", then DATA is ignored and the descriptor is
    treated as null.
  * Otherwise, DATA is a string.

If a (sub-)descriptor is in an invalid format, it is silently treated
as null.

Return t if the parameter is successfully set.  Otherwise return nil.  */)
  (Lisp_Object apple_event, Lisp_Object keyword, Lisp_Object descriptor)
{
  Lisp_Object result = Qnil;
  UInt32 suspension_id;
  struct suspended_ae_info *p;

  suspension_id = get_suspension_id (apple_event);

  CHECK_STRING (keyword);
  if (SBYTES (keyword) != 4)
    error ("Apple event keyword must be a 4-byte string: %s",
	   SDATA (keyword));

  BLOCK_INPUT;
  for (p = suspended_apple_events; p; p = p->next)
    if (p->suspension_id == suspension_id)
      break;
  if (p && p->reply.descriptorType != typeNull)
    {
      OSErr err;

      err = mac_ae_put_lisp (&p->reply,
			     EndianU32_BtoN (*((UInt32 *) SDATA (keyword))),
			     descriptor);
      if (err == noErr)
	result = Qt;
    }
  UNBLOCK_INPUT;

  return result;
}

DEFUN ("mac-resume-apple-event", Fmac_resume_apple_event, Smac_resume_apple_event, 1, 2, 0,
       doc: /* Resume handling of APPLE-EVENT.
Every Apple event handled by the Lisp interpreter is suspended first.
This function resumes such a suspended event either to complete Apple
event handling to give a reply, or to redispatch it to other handlers.

If optional ERROR-CODE is an integer, it specifies the error number
that is set in the reply.  If ERROR-CODE is t, the resumed event is
handled with the standard dispatching mechanism, but it is not handled
by Emacs again, thus it is redispatched to other handlers.

Return t if APPLE-EVENT is successfully resumed.  Otherwise return
nil, which means the event is already resumed or expired.  */)
  (Lisp_Object apple_event, Lisp_Object error_code)
{
  Lisp_Object result = Qnil;
  UInt32 suspension_id;
  struct suspended_ae_info **p, *ae;

  suspension_id = get_suspension_id (apple_event);

  BLOCK_INPUT;
  for (p = &suspended_apple_events; *p; p = &(*p)->next)
    if ((*p)->suspension_id == suspension_id)
      break;
  if (*p)
    {
      ae = *p;
      *p = (*p)->next;
      if (INTEGERP (error_code)
	  && ae->reply.descriptorType != typeNull)
	{
	  SInt32 errn = XINT (error_code);

	  AEPutParamPtr (&ae->reply, keyErrorNumber, typeSInt32,
			 &errn, sizeof (SInt32));
	}
      AESetTheCurrentEvent (&ae->apple_event);
      AEResumeTheCurrentEvent (&ae->apple_event, &ae->reply,
			       (EQ (error_code, Qt)
				? AE_USE_STANDARD_DISPATCH
				: (AEEventHandlerUPP) kAENoDispatch), 0);
      AEDisposeDesc (&ae->reply);
      AEDisposeDesc (&ae->apple_event);
      xfree (ae);
      result = Qt;
    }
  UNBLOCK_INPUT;

  return result;
}

DEFUN ("mac-send-apple-event-internal", Fmac_send_apple_event_internal, Smac_send_apple_event_internal, 1, 2, 0,
       doc: /* Send APPLE-EVENT with SEND-MODE.
This is for internal use only.  Use `mac-send-apple-event' instead.

APPLE-EVENT is a Lisp representation of an Apple event.  SEND-MODE
specifies a send mode for the Apple event.  It must be either an
integer, nil for kAENoReply, or t for kAEQueueReply.

If sent successfully, return the Lisp representation of the sent event
so the reply handler can use the value of the `return-id' attribute.
Otherwise, return the error code as an integer.  */)
  (Lisp_Object apple_event, Lisp_Object send_mode)
{
  OSStatus err;
  Lisp_Object result;
  AESendMode mode;
  AppleEvent event;

  CHECK_CONS (apple_event);
  CHECK_STRING_CAR (apple_event);
  if (SBYTES (XCAR (apple_event)) != 4
      || strcmp (SSDATA (XCAR (apple_event)), "aevt") != 0)
    error ("Not an apple event");

  if (NILP (send_mode))
    mode = kAENoReply;
  else if (EQ (send_mode, Qt))
    mode = kAEQueueReply;
  else
    {
      CHECK_NUMBER (send_mode);
      mode = XINT (send_mode);
      mode &= ~kAEWaitReply;
      if ((mode & (kAENoReply | kAEQueueReply)) == 0)
	mode |= kAENoReply;
    }

  BLOCK_INPUT;
  err = create_apple_event_from_lisp (apple_event, &event);
  if (err == noErr)
    {
      err = AESendMessage (&event, NULL, mode, kAEDefaultTimeout);
      if (err == noErr)
	result = mac_aedesc_to_lisp (&event);
      else
	result = make_number (err);
      AEDisposeDesc (&event);
    }
  else
    result = make_number (err);
  UNBLOCK_INPUT;

  return result;
}


/***********************************************************************
                      Drag and drop support
***********************************************************************/
Lisp_Object QCactions, Qcopy, Qlink, Qgeneric, Qprivate, Qmove, Qdelete;


void
syms_of_macselect (void)
{
  defsubr (&Sx_get_selection_internal);
  defsubr (&Sx_own_selection_internal);
  defsubr (&Sx_disown_selection_internal);
  defsubr (&Sx_selection_owner_p);
  defsubr (&Sx_selection_exists_p);
  defsubr (&Smac_process_deferred_apple_events);
  defsubr (&Smac_cleanup_expired_apple_events);
  defsubr (&Smac_resume_apple_event);
  defsubr (&Smac_ae_set_reply_parameter);
  defsubr (&Smac_send_apple_event_internal);

  DEFVAR_LISP ("selection-converter-alist", Vselection_converter_alist,
	       doc: /* An alist associating selection-types with functions.
These functions are called to convert the selection, with three args:
the name of the selection (typically `PRIMARY', `SECONDARY', or `CLIPBOARD');
a desired type to which the selection should be converted;
and the local selection value (whatever was given to `x-own-selection').

The function should return the value to send to the Scrap Manager
\(must be a string).  A return value of nil
means that the conversion could not be done.  */);
  Vselection_converter_alist = Qnil;

  DEFVAR_LISP ("x-lost-selection-functions", Vx_lost_selection_functions,
	       doc: /* A list of functions to be called when Emacs loses a selection.
\(This happens when a Lisp program explicitly clears the selection.)
The functions are called with one argument, the selection type
\(a symbol, typically `PRIMARY', `SECONDARY', or `CLIPBOARD').  */);
  Vx_lost_selection_functions = Qnil;

  DEFVAR_LISP ("mac-apple-event-map", Vmac_apple_event_map,
	       doc: /* Keymap for Apple events handled by Emacs.  */);
  Vmac_apple_event_map = Qnil;

  DEFVAR_LISP ("mac-dnd-known-types", Vmac_dnd_known_types,
	       doc: /* The types accepted by default for dropped data.
The types are chosen in the order they appear in the list.  */);
  Vmac_dnd_known_types = mac_dnd_default_known_types ();

  DEFVAR_LISP ("mac-service-selection", Vmac_service_selection,
	       doc: /* Selection name for communication via Services menu.  */);
  Vmac_service_selection = intern_c_string ("PRIMARY");

  DEFSYM (QSECONDARY, "SECONDARY");
  DEFSYM (QTIMESTAMP, "TIMESTAMP");
  DEFSYM (QTARGETS, "TARGETS");
  DEFSYM (Qforeign_selection, "foreign-selection");
  DEFSYM (Qx_lost_selection_functions, "x-lost-selection-functions");
  DEFSYM (Qmac_pasteboard_name, "mac-pasteboard-name");
  DEFSYM (Qmac_pasteboard_data_type, "mac-pasteboard-data-type");
  DEFSYM (Qmac_apple_event_class, "mac-apple-event-class");
  DEFSYM (Qmac_apple_event_id, "mac-apple-event-id");
  DEFSYM (Qemacs_suspension_id, "emacs-suspension-id");
  DEFSYM (QCactions, ":actions");
  DEFSYM (Qcopy, "copy");
  DEFSYM (Qlink, "link");
  DEFSYM (Qgeneric, "generic");
  DEFSYM (Qprivate, "private");
  DEFSYM (Qmove, "move");
  DEFSYM (Qdelete, "delete");
}
