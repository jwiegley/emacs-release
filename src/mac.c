/* Unix emulation routines for GNU Emacs on the Mac OS.
   Copyright (C) 2000-2008  Free Software Foundation, Inc.
   Copyright (C) 2009-2014  YAMAMOTO Mitsuharu

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
#include <errno.h>

#include "lisp.h"
#include "process.h"
#include "systime.h"
#include "sysselect.h"
#include "blockinput.h"

#include "macterm.h"

#include "charset.h"
#include "coding.h"

#include <sys/stat.h>
#include <sys/param.h>
#include <fcntl.h>

#include <libkern/OSByteOrder.h>

#include <mach/mach.h>
#include <servers/bootstrap.h>

#if MAC_OS_X_VERSION_MIN_REQUIRED >= 1060
#ifndef SELECT_USE_GCD
#define SELECT_USE_GCD 1
#endif
#endif

#include <sys/socket.h>
#if !SELECT_USE_GCD
#include <pthread.h>
#endif

/* An instance of the AppleScript component.  */
static ComponentInstance as_scripting_component;
/* The single script context used for all script executions.  */
static OSAID as_script_context;


/***********************************************************************
			  Utility functions
 ***********************************************************************/

/* Return the length of the cdr chain of the given LIST.  Return -1 if
   LIST is circular.  */

static EMACS_INT
cdr_chain_length (Lisp_Object list)
{
  EMACS_INT result = 0;
  Lisp_Object tortoise, hare;

  hare = tortoise = list;

  while (CONSP (hare))
    {
      hare = XCDR (hare);
      result++;
      if (!CONSP (hare))
	break;

      hare = XCDR (hare);
      result++;
      tortoise = XCDR (tortoise);

      if (EQ (hare, tortoise))
	return -1;
    }

  return result;
}

/* Binary search tree to record Lisp objects on the traversal stack,
   used for checking circularity in the conversion from a Lisp object.
   We assume deletion of a node happens only if its children are
   leaves. */

struct bstree_node
{
  Lisp_Object obj;
  struct bstree_node *left, *right;
};

/* Find OBJ in the binary search tree *BSTREE.  If found, the return
   value points to the variable whose value points to the node
   containing OBJ.  Otherwise, the return value points to the variable
   whose value would point to a new node containing OBJ if we added it
   to *BSTREE.  In the latter case, the variable pointed to by the
   return value contains NULL.  */

static struct bstree_node **
bstree_find (struct bstree_node **bstree, Lisp_Object obj)
{
  while (*bstree)
    if (XHASH (obj) < XHASH ((*bstree)->obj))
      bstree = &(*bstree)->left;
    else if (XHASH (obj) > XHASH ((*bstree)->obj))
      bstree = &(*bstree)->right;
    else
      break;

  return bstree;
}

/* Return unibyte Lisp string representing four char code CODE.  */

Lisp_Object
mac_four_char_code_to_string (FourCharCode code)
{
  Lisp_Object string = make_uninit_string (sizeof (FourCharCode));

  OSWriteBigInt32 (SDATA (string), 0, code);

  return string;
}

/* Store four char code corresponding to Lisp string STRING to *CODE.
   Return non-zero if and only if STRING correctly represents four
   char code (i.e., 4-byte Lisp string).  */

Boolean
mac_string_to_four_char_code (Lisp_Object string, FourCharCode *code)
{
  if (!(STRINGP (string) && SBYTES (string) == sizeof (FourCharCode)))
    return false;

  *code = OSReadBigInt32 (SDATA (string), 0);

  return true;
}


/***********************************************************************
		  Conversions on Apple event objects
 ***********************************************************************/

static Lisp_Object Qundecoded_file_name;

static struct {
  AEKeyword keyword;
  const char *name;
  Lisp_Object symbol;
} ae_attr_table [] =
  {{keyTransactionIDAttr,	"transaction-id"},
   {keyReturnIDAttr,		"return-id"},
   {keyEventClassAttr,		"event-class"},
   {keyEventIDAttr,		"event-id"},
   {keyAddressAttr,		"address"},
   {keyOptionalKeywordAttr,	"optional-keyword"},
   {keyTimeoutAttr,		"timeout"},
   {keyInteractLevelAttr,	"interact-level"},
   {keyEventSourceAttr,		"event-source"},
   /* {keyMissedKeywordAttr,	"missed-keyword"}, */
   {keyOriginalAddressAttr,	"original-address"},
   {keyReplyRequestedAttr,	"reply-requested"},
   {KEY_EMACS_SUSPENSION_ID_ATTR, "emacs-suspension-id"}
  };

static Lisp_Object
mac_aelist_to_lisp (const AEDescList *desc_list)
{
  OSErr err;
  long count;
  Lisp_Object result, elem;
  DescType desc_type;
  Size size;
  AEKeyword keyword;
  AEDesc desc;
  bool attribute_p = false;

  err = AECountItems (desc_list, &count);
  if (err != noErr)
    return Qnil;
  result = Qnil;

 again:
  while (count > 0)
    {
      if (attribute_p)
	{
	  keyword = ae_attr_table[count - 1].keyword;
	  err = AESizeOfAttribute (desc_list, keyword, &desc_type, &size);
	}
      else
	err = AESizeOfNthItem (desc_list, count, &desc_type, &size);

      if (err == noErr)
	switch (desc_type)
	  {
	  case typeAEList:
	  case typeAERecord:
	  case typeAppleEvent:
	    if (attribute_p)
	      err = AEGetAttributeDesc (desc_list, keyword, typeWildCard,
					&desc);
	    else
	      err = AEGetNthDesc (desc_list, count, typeWildCard,
				  &keyword, &desc);
	    if (err != noErr)
	      break;
	    elem = mac_aelist_to_lisp (&desc);
	    AEDisposeDesc (&desc);
	    break;

	  default:
	    if (desc_type == typeNull)
	      elem = Qnil;
	    else
	      {
		elem = make_uninit_string (size);
		if (attribute_p)
		  err = AEGetAttributePtr (desc_list, keyword, typeWildCard,
					   &desc_type, SDATA (elem),
					   size, &size);
		else
		  err = AEGetNthPtr (desc_list, count, typeWildCard, &keyword,
				     &desc_type, SDATA (elem), size, &size);
	      }
	    if (err != noErr)
	      break;
	    elem = Fcons (mac_four_char_code_to_string (desc_type), elem);
	    break;
	  }

      if (err == noErr || desc_list->descriptorType == typeAEList)
	{
	  if (err != noErr)
	    elem = Qnil;	/* Don't skip elements in AEList.  */
	  else if (desc_list->descriptorType != typeAEList)
	    {
	      if (attribute_p)
		elem = Fcons (ae_attr_table[count-1].symbol, elem);
	      else
		elem = Fcons (mac_four_char_code_to_string (keyword), elem);
	    }

	  result = Fcons (elem, result);
	}

      count--;
    }

  if (desc_list->descriptorType == typeAppleEvent && !attribute_p)
    {
      attribute_p = true;
      count = sizeof (ae_attr_table) / sizeof (ae_attr_table[0]);
      goto again;
    }

  return Fcons (mac_four_char_code_to_string (desc_list->descriptorType),
		result);
}

Lisp_Object
mac_aedesc_to_lisp (const AEDesc *desc)
{
  OSErr err = noErr;
  DescType desc_type = desc->descriptorType;
  Lisp_Object result;

  switch (desc_type)
    {
    case typeNull:
      result = Qnil;
      break;

    case typeAEList:
    case typeAERecord:
    case typeAppleEvent:
      return mac_aelist_to_lisp (desc);
#if 0
      /* The following one is much simpler, but creates and disposes
	 of Apple event descriptors many times.  */
      {
	long count;
	Lisp_Object elem;
	AEKeyword keyword;
	AEDesc desc1;

	err = AECountItems (desc, &count);
	if (err != noErr)
	  break;
	result = Qnil;
	while (count > 0)
	  {
	    err = AEGetNthDesc (desc, count, typeWildCard, &keyword, &desc1);
	    if (err != noErr)
	      break;
	    elem = mac_aedesc_to_lisp (&desc1);
	    AEDisposeDesc (&desc1);
	    if (desc_type != typeAEList)
	      elem = Fcons (mac_four_char_code_to_string (keyword), elem);
	    result = Fcons (elem, result);
	    count--;
	  }
      }
#endif
      break;

    default:
      result = make_uninit_string (AEGetDescDataSize (desc));
      err = AEGetDescData (desc, SDATA (result), SBYTES (result));
      break;
    }

  if (err != noErr)
    return Qnil;

  return Fcons (mac_four_char_code_to_string (desc_type), result);
}

static OSErr
mac_ae_put_lisp_1 (AEDescList *desc, UInt32 keyword_or_index, Lisp_Object obj,
		   struct bstree_node **ancestors)
{
  OSErr err;
  DescType desc_type1;

  if (CONSP (obj) && mac_string_to_four_char_code (XCAR (obj), &desc_type1))
    {
      Lisp_Object data = XCDR (obj), rest;
      AEDesc desc1;
      struct bstree_node **bstree_ref;

      switch (desc_type1)
	{
	case typeNull:
	case typeAppleEvent:
	  break;

	case typeAEList:
	case typeAERecord:
	  if (cdr_chain_length (data) < 0)
	    break;
	  bstree_ref = bstree_find (ancestors, obj);
	  if (*bstree_ref)
	    break;
	  else
	    {
	      struct bstree_node node;

	      node.obj = obj;
	      node.left = node.right = NULL;
	      *bstree_ref = &node;

	      err = AECreateList (NULL, 0, desc_type1 == typeAERecord, &desc1);
	      if (err == noErr)
		{
		  for (rest = data; CONSP (rest); rest = XCDR (rest))
		    {
		      UInt32 keyword_or_index1 = 0;
		      Lisp_Object elem = XCAR (rest);

		      if (desc_type1 == typeAERecord)
			{
			  if (CONSP (elem)
			      && mac_string_to_four_char_code (XCAR (elem),
							       &keyword_or_index1))
			    elem = XCDR (elem);
			  else
			    continue;
			}

		      err = mac_ae_put_lisp_1 (&desc1, keyword_or_index1, elem,
					       ancestors);
		      if (err != noErr)
			break;
		    }

		  if (err == noErr)
		    {
		      if (desc->descriptorType == typeAEList)
			err = AEPutDesc (desc, keyword_or_index, &desc1);
		      else
			err = AEPutParamDesc (desc, keyword_or_index, &desc1);
		    }

		  AEDisposeDesc (&desc1);
		}

	      *bstree_ref = NULL;
	    }
	  return err;

	default:
	  if (!STRINGP (data))
	    break;
	  if (desc->descriptorType == typeAEList)
	    err = AEPutPtr (desc, keyword_or_index, desc_type1,
			    SDATA (data), SBYTES (data));
	  else
	    err = AEPutParamPtr (desc, keyword_or_index, desc_type1,
				 SDATA (data), SBYTES (data));
	  return err;
	}
    }

  if (desc->descriptorType == typeAEList)
    err = AEPutPtr (desc, keyword_or_index, typeNull, NULL, 0);
  else
    err = AEPutParamPtr (desc, keyword_or_index, typeNull, NULL, 0);

  return err;
}

OSErr
mac_ae_put_lisp (AEDescList *desc, UInt32 keyword_or_index, Lisp_Object obj)
{
  struct bstree_node *root = NULL;

  if (!(desc->descriptorType == typeAppleEvent
	|| desc->descriptorType == typeAERecord
	|| desc->descriptorType == typeAEList))
    return errAEWrongDataType;

  return mac_ae_put_lisp_1 (desc, keyword_or_index, obj, &root);
}

OSErr
create_apple_event_from_lisp (Lisp_Object apple_event, AppleEvent *result)
{
  OSErr err;

  if (!(CONSP (apple_event) && STRINGP (XCAR (apple_event))
	&& SBYTES (XCAR (apple_event)) == 4
	&& strcmp (SSDATA (XCAR (apple_event)), "aevt") == 0
	&& cdr_chain_length (XCDR (apple_event)) >= 0))
    return errAEBuildSyntaxError;

  err = create_apple_event (0, 0, result);
  if (err == noErr)
    {
      Lisp_Object rest;

      for (rest = XCDR (apple_event); CONSP (rest); rest = XCDR (rest))
	{
	  Lisp_Object attr = XCAR (rest), name, type, data;
	  DescType desc_type;
	  int i;

	  if (!(CONSP (attr) && SYMBOLP (XCAR (attr)) && CONSP (XCDR (attr))))
	    continue;
	  name = XCAR (attr);
	  type = XCAR (XCDR (attr));
	  data = XCDR (XCDR (attr));
	  if (!mac_string_to_four_char_code (type, &desc_type))
	    continue;
	  for (i = 0; i < sizeof (ae_attr_table) / sizeof (ae_attr_table[0]);
	       i++)
	    if (EQ (name, ae_attr_table[i].symbol))
	      {
		switch (desc_type)
		  {
		  case typeNull:
		    AEPutAttributePtr (result, ae_attr_table[i].keyword,
				       desc_type, NULL, 0);
		    break;

		  case typeAppleEvent:
		  case typeAEList:
		  case typeAERecord:
		    /* We assume there's no composite attribute value.  */
		    break;

		  default:
		    if (STRINGP (data))
		      AEPutAttributePtr (result, ae_attr_table[i].keyword,
					 desc_type,
					 SDATA (data), SBYTES (data));
		    break;
		  }
		break;
	      }
	}

      for (rest = XCDR (apple_event); CONSP (rest); rest = XCDR (rest))
	{
	  Lisp_Object param = XCAR (rest);
	  AEKeyword keyword;

	  if (CONSP (param)
	      && mac_string_to_four_char_code (XCAR (param), &keyword))
	    mac_ae_put_lisp (result, keyword, XCDR (param));
	}
    }

  return err;
}

static pascal OSErr
mac_coerce_file_name_ptr (DescType type_code, const void *data_ptr,
			  Size data_size, DescType to_type, long handler_refcon,
			  AEDesc *result)
{
  OSErr err;

  if (type_code == typeNull)
    err = errAECoercionFail;
  else if (type_code == to_type || to_type == typeWildCard)
    err = AECreateDesc (TYPE_FILE_NAME, data_ptr, data_size, result);
  else if (type_code == TYPE_FILE_NAME)
    /* Coercion from undecoded file name.  */
    {
      CFURLRef url;
      CFDataRef data = NULL;

      url = CFURLCreateFromFileSystemRepresentation (NULL, data_ptr,
						     data_size, false);
      if (url)
	{
	  data = CFURLCreateData (NULL, url, kCFStringEncodingUTF8, true);
	  CFRelease (url);
	}
      if (data)
	{
	  err = AECoercePtr (typeFileURL, CFDataGetBytePtr (data),
			     CFDataGetLength (data), to_type, result);
	  CFRelease (data);
	}
      else
	err = memFullErr;
    }
  else if (to_type == TYPE_FILE_NAME)
    /* Coercion to undecoded file name.  */
    {
      CFURLRef url = NULL;
      CFStringRef str = NULL;
      CFDataRef data = NULL;

      if (type_code == typeFileURL)
	{
	  url = CFURLCreateWithBytes (NULL, data_ptr, data_size,
				      kCFStringEncodingUTF8, NULL);
	  err = noErr;
	}
      else
	{
	  AEDesc desc;
	  Size size;
	  UInt8 *buf;

	  err = AECoercePtr (type_code, data_ptr, data_size,
			     typeFileURL, &desc);
	  if (err == noErr)
	    {
	      size = AEGetDescDataSize (&desc);
	      buf = xmalloc (size);
	      err = AEGetDescData (&desc, buf, size);
	      if (err == noErr)
		url = CFURLCreateWithBytes (NULL, buf, size,
					    kCFStringEncodingUTF8, NULL);
	      xfree (buf);
	      AEDisposeDesc (&desc);
	    }
	}
      if (url)
	{
	  str = CFURLCopyFileSystemPath (url, kCFURLPOSIXPathStyle);
	  CFRelease (url);
	}
      if (str)
	{
	  data = CFStringCreateExternalRepresentation (NULL, str,
						       kCFStringEncodingUTF8,
						       '\0');
	  CFRelease (str);
	}
      if (data)
	{
	  err = AECreateDesc (TYPE_FILE_NAME, CFDataGetBytePtr (data),
			      CFDataGetLength (data), result);
	  CFRelease (data);
	}
    }
  else
    emacs_abort ();

  if (err != noErr)
    return errAECoercionFail;
  return noErr;
}

static pascal OSErr
mac_coerce_file_name_desc (const AEDesc *from_desc, DescType to_type,
			   long handler_refcon, AEDesc *result)
{
  OSErr err = noErr;
  DescType from_type = from_desc->descriptorType;

  if (from_type == typeNull)
    err = errAECoercionFail;
  else if (from_type == to_type || to_type == typeWildCard)
    err = AEDuplicateDesc (from_desc, result);
  else
    {
      char *data_ptr;
      Size data_size;

      data_size = AEGetDescDataSize (from_desc);
      data_ptr = xmalloc (data_size);
      err = AEGetDescData (from_desc, data_ptr, data_size);
      if (err == noErr)
	err = mac_coerce_file_name_ptr (from_type, data_ptr,
					data_size, to_type,
					handler_refcon, result);
      xfree (data_ptr);
    }

  if (err != noErr)
    return errAECoercionFail;
  return noErr;
}

OSErr
init_coercion_handler (void)
{
  OSErr err;

  static AECoercePtrUPP coerce_file_name_ptrUPP = NULL;
  static AECoerceDescUPP coerce_file_name_descUPP = NULL;

  if (coerce_file_name_ptrUPP == NULL)
    {
      coerce_file_name_ptrUPP = NewAECoercePtrUPP (mac_coerce_file_name_ptr);
      coerce_file_name_descUPP = NewAECoerceDescUPP (mac_coerce_file_name_desc);
    }

  err = AEInstallCoercionHandler (TYPE_FILE_NAME, typeWildCard,
				  (AECoercionHandlerUPP)
				  coerce_file_name_ptrUPP, 0, false, false);
  if (err == noErr)
    err = AEInstallCoercionHandler (typeWildCard, TYPE_FILE_NAME,
				    (AECoercionHandlerUPP)
				    coerce_file_name_ptrUPP, 0, false, false);
  if (err == noErr)
    err = AEInstallCoercionHandler (TYPE_FILE_NAME, typeWildCard,
				    coerce_file_name_descUPP, 0, true, false);
  if (err == noErr)
    err = AEInstallCoercionHandler (typeWildCard, TYPE_FILE_NAME,
				    coerce_file_name_descUPP, 0, true, false);
  return err;
}

OSErr
create_apple_event (AEEventClass class, AEEventID id, AppleEvent *result)
{
  OSErr err;
  static const ProcessSerialNumber psn = {0, kCurrentProcess};
  AEAddressDesc address_desc;

  err = AECreateDesc (typeProcessSerialNumber, &psn,
		      sizeof (ProcessSerialNumber), &address_desc);
  if (err == noErr)
    {
      err = AECreateAppleEvent (class, id,
				&address_desc, /* NULL is not allowed
						  on Mac OS Classic. */
				kAutoGenerateReturnID,
				kAnyTransactionID, result);
      AEDisposeDesc (&address_desc);
    }

  return err;
}

Lisp_Object
mac_event_parameters_to_lisp (EventRef event, UInt32 num_params,
			      const EventParamName *names,
			      const EventParamType *types)
{
  OSStatus err;
  Lisp_Object result = Qnil;
  UInt32 i;
  ByteCount size;
  CFStringRef string;
  CFDataRef data;
  char *buf = NULL;

  for (i = 0; i < num_params; i++)
    {
      EventParamName name = names[i];
      EventParamType type = types[i];

      switch (type)
	{
	case typeCFStringRef:
	  err = GetEventParameter (event, name, typeCFStringRef, NULL,
				   sizeof (CFStringRef), NULL, &string);
	  if (err != noErr)
	    break;
	  data = CFStringCreateExternalRepresentation (NULL, string,
						       kCFStringEncodingUTF8,
						       '?');
	  if (data == NULL)
	    break;
	  result =
	    Fcons (Fcons (mac_four_char_code_to_string (name),
			  Fcons (mac_four_char_code_to_string (typeUTF8Text),
				 make_unibyte_string (((char *)
						       CFDataGetBytePtr (data)),
						      CFDataGetLength (data)))),
		   result);
	  CFRelease (data);
	  break;

	default:
	  err = GetEventParameter (event, name, type, NULL, 0, &size, NULL);
	  if (err != noErr)
	    break;
	  buf = xrealloc (buf, size);
	  err = GetEventParameter (event, name, type, NULL, size, NULL, buf);
	  if (err == noErr)
	    result =
	      Fcons (Fcons (mac_four_char_code_to_string (name),
			    Fcons (mac_four_char_code_to_string (type),
				   make_unibyte_string (buf, size))),
		     result);
	  break;
	}
    }
  xfree (buf);

  return result;
}


/***********************************************************************
	 Conversion between Lisp and Core Foundation objects
 ***********************************************************************/

Lisp_Object Qstring, Qnumber, Qboolean, Qdate, Qarray, Qdictionary;
Lisp_Object Qrange, Qpoint;
static Lisp_Object Qdescription;

struct cfdict_context
{
  Lisp_Object *result;
  int flags, hash_bound;
};

/* C string to CFString.  */

CFStringRef
cfstring_create_with_utf8_cstring (const char *c_str)
{
  CFStringRef str;

  str = CFStringCreateWithCString (NULL, c_str, kCFStringEncodingUTF8);
  if (str == NULL)
    /* Failed to interpret as UTF 8.  Fall back on Mac Roman.  */
    str = CFStringCreateWithCString (NULL, c_str, kCFStringEncodingMacRoman);

  return str;
}


/* Lisp string containing a UTF-8 byte sequence to CFString.  Unlike
   cfstring_create_with_utf8_cstring, this function preserves NUL
   characters.  */

CFStringRef
cfstring_create_with_string_noencode (Lisp_Object s)
{
  CFStringRef string = CFStringCreateWithBytes (NULL, SDATA (s), SBYTES (s),
						kCFStringEncodingUTF8, false);

  if (string == NULL)
    /* Failed to interpret as UTF 8.  Fall back on Mac Roman.  */
    string = CFStringCreateWithBytes (NULL, SDATA (s), SBYTES (s),
				      kCFStringEncodingMacRoman, false);

  return string;
}

/* Lisp string to CFString.  */

CFStringRef
cfstring_create_with_string (Lisp_Object s)
{
  if (STRING_MULTIBYTE (s))
    {
      char *p, *end = SSDATA (s) + SBYTES (s);

      for (p = SSDATA (s); p < end; p++)
	if (!isascii (*p))
	  {
	    s = ENCODE_UTF_8 (s);
	    break;
	  }
      return cfstring_create_with_string_noencode (s);
    }
  else
    return CFStringCreateWithBytes (NULL, SDATA (s), SBYTES (s),
				    kCFStringEncodingMacRoman, false);
}


/* From CFData to a lisp string.  Always returns a unibyte string.  */

Lisp_Object
cfdata_to_lisp (CFDataRef data)
{
  CFIndex len = CFDataGetLength (data);
  Lisp_Object result = make_uninit_string (len);

  CFDataGetBytes (data, CFRangeMake (0, len), SDATA (result));

  return result;
}


/* From CFString to a lisp string.  Returns a unibyte string
   containing a UTF-8 byte sequence.  */

Lisp_Object
cfstring_to_lisp_nodecode (CFStringRef string)
{
  Lisp_Object result = Qnil;
  CFDataRef data;
  const char *s = CFStringGetCStringPtr (string, kCFStringEncodingUTF8);

  if (s)
    {
      CFIndex i, length = CFStringGetLength (string);

      for (i = 0; i < length; i++)
	if (CFStringGetCharacterAtIndex (string, i) == 0)
	  break;

      if (i == length)
	return make_unibyte_string (s, strlen (s));
    }

  data = CFStringCreateExternalRepresentation (NULL, string,
					       kCFStringEncodingUTF8, '?');
  if (data)
    {
      result = cfdata_to_lisp (data);
      CFRelease (data);
    }

  return result;
}


/* From CFString to a lisp string.  Never returns a unibyte string
   (even if it only contains ASCII characters).
   This may cause GC during code conversion. */

Lisp_Object
cfstring_to_lisp (CFStringRef string)
{
  Lisp_Object result = cfstring_to_lisp_nodecode (string);

  if (!NILP (result))
    {
      result = code_convert_string_norecord (result, Qutf_8, 0);
      /* This may be superfluous.  Just to make sure that the result
	 is a multibyte string.  */
      result = string_to_multibyte (result);
    }

  return result;
}


/* From CFString to a lisp string.  Returns a unibyte string
   containing a UTF-16 byte sequence in native byte order, no BOM.  */

Lisp_Object
cfstring_to_lisp_utf_16 (CFStringRef string)
{
  Lisp_Object result = Qnil;
  CFIndex len, buf_len;

  len = CFStringGetLength (string);
  if (CFStringGetBytes (string, CFRangeMake (0, len), kCFStringEncodingUnicode,
			0, false, NULL, 0, &buf_len) == len)
    {
      result = make_uninit_string (buf_len);
      CFStringGetBytes (string, CFRangeMake (0, len), kCFStringEncodingUnicode,
			0, false, SDATA (result), buf_len, NULL);
    }

  return result;
}


/* CFNumber to a lisp integer, float, or string in decimal.  */

Lisp_Object
cfnumber_to_lisp (CFNumberRef number)
{
  Lisp_Object result = Qnil;
#if BITS_PER_EMACS_INT > 32
  SInt64 int_val;
  CFNumberType emacs_int_type = kCFNumberSInt64Type;
#else
  SInt32 int_val;
  CFNumberType emacs_int_type = kCFNumberSInt32Type;
#endif
  double float_val;

  if (CFNumberGetValue (number, emacs_int_type, &int_val)
      && !FIXNUM_OVERFLOW_P (int_val))
    result = make_number (int_val);
  else if (CFNumberGetValue (number, kCFNumberDoubleType, &float_val))
    result = make_float (float_val);
  else
    {
      CFStringRef string = CFStringCreateWithFormat (NULL, NULL,
						     CFSTR ("%@"), number);
      if (string)
	{
	  result = cfstring_to_lisp_nodecode (string);
	  CFRelease (string);
	}
    }
  return result;
}


/* CFDate to a list of four integers as in a return value of
   `current-time'.  */

Lisp_Object
cfdate_to_lisp (CFDateRef date)
{
  CFTimeInterval sec, frac;
  int high, low, microsec, picosec;

  sec = CFDateGetAbsoluteTime (date) + kCFAbsoluteTimeIntervalSince1970;
  frac = modf (sec, &sec);
  high = sec / 65536.0;
  low = sec - high * 65536.0;
  frac = modf (frac * 1000000.0, &sec);
  microsec = sec;
  picosec = frac * 1000000.0;

  return list4 (make_number (high), make_number (low),
		make_number (microsec), make_number (picosec));
}


/* CFBoolean to a lisp symbol, `t' or `nil'.  */

Lisp_Object
cfboolean_to_lisp (CFBooleanRef boolean)
{
  return CFBooleanGetValue (boolean) ? Qt : Qnil;
}


/* Any Core Foundation object to a (lengthy) lisp string.  */

Lisp_Object
cfobject_desc_to_lisp (CFTypeRef object)
{
  Lisp_Object result = Qnil;
  CFStringRef desc = CFCopyDescription (object);

  if (desc)
    {
      result = cfstring_to_lisp (desc);
      CFRelease (desc);
    }

  return result;
}


/* Callback functions for cfobject_to_lisp.  */

static void
cfdictionary_add_to_list (const void *key, const void *value, void *context)
{
  struct cfdict_context *cxt = (struct cfdict_context *)context;
  Lisp_Object lisp_key;

  if (CFGetTypeID (key) != CFStringGetTypeID ())
    lisp_key = cfobject_to_lisp (key, cxt->flags, cxt->hash_bound);
  else if (cxt->flags & CFOBJECT_TO_LISP_DONT_DECODE_DICTIONARY_KEY)
    lisp_key = cfstring_to_lisp_nodecode (key);
  else
    lisp_key = cfstring_to_lisp (key);

  *cxt->result =
    Fcons (Fcons (lisp_key,
		  cfobject_to_lisp (value, cxt->flags, cxt->hash_bound)),
	   *cxt->result);
}

static void
cfdictionary_puthash (const void *key, const void *value, void *context)
{
  Lisp_Object lisp_key;
  struct cfdict_context *cxt = (struct cfdict_context *)context;
  struct Lisp_Hash_Table *h = XHASH_TABLE (*(cxt->result));
  EMACS_UINT hash_code;

  if (CFGetTypeID (key) != CFStringGetTypeID ())
    lisp_key = cfobject_to_lisp (key, cxt->flags, cxt->hash_bound);
  else if (cxt->flags & CFOBJECT_TO_LISP_DONT_DECODE_DICTIONARY_KEY)
    lisp_key = cfstring_to_lisp_nodecode (key);
  else
    lisp_key = cfstring_to_lisp (key);

  hash_lookup (h, lisp_key, &hash_code);
  hash_put (h, lisp_key,
	    cfobject_to_lisp (value, cxt->flags, cxt->hash_bound),
	    hash_code);
}


/* Convert Core Foundation Object OBJ to a Lisp object.

   FLAGS is bitwise-or of some of the following flags.
   If CFOBJECT_TO_LISP_WITH_TAG is set, a symbol that represents the
   type of the original Core Foundation object is prepended.
   If CFOBJECT_TO_LISP_DONT_DECODE_STRING is set, CFStrings (except
   dictionary keys) are not decoded and the resulting Lisp objects are
   unibyte strings as UTF-8 byte sequences.
   If CFOBJECT_TO_LISP_DONT_DECODE_DICTIONARY_KEY is set, dictionary
   key CFStrings are not decoded.

   HASH_BOUND specifies which kinds of the lisp objects, alists or
   hash tables, are used as the targets of the conversion from
   CFDictionary.  If HASH_BOUND is negative, always generate alists.
   If HASH_BOUND >= 0, generate an alist if the number of keys in the
   dictionary is smaller than HASH_BOUND, and a hash table
   otherwise.  */

Lisp_Object
cfobject_to_lisp (CFTypeRef obj, int flags, int hash_bound)
{
  CFTypeID type_id = CFGetTypeID (obj);
  Lisp_Object tag = Qnil, result = Qnil;
  struct gcpro gcpro1, gcpro2;

  GCPRO2 (tag, result);

  if (type_id == CFStringGetTypeID ())
    {
      tag = Qstring;
      if (flags & CFOBJECT_TO_LISP_DONT_DECODE_STRING)
	result = cfstring_to_lisp_nodecode (obj);
      else
	result = cfstring_to_lisp (obj);
    }
  else if (type_id == CFNumberGetTypeID ())
    {
      tag = Qnumber;
      result = cfnumber_to_lisp (obj);
    }
  else if (type_id == CFBooleanGetTypeID ())
    {
      tag = Qboolean;
      result = cfboolean_to_lisp (obj);
    }
  else if (type_id == CFDateGetTypeID ())
    {
      tag = Qdate;
      result = cfdate_to_lisp (obj);
    }
  else if (type_id == CFDataGetTypeID ())
    {
      tag = Qdata;
      result = cfdata_to_lisp (obj);
    }
  else if (type_id == CFArrayGetTypeID ())
    {
      CFIndex index, count = CFArrayGetCount (obj);

      tag = Qarray;
      result = Fmake_vector (make_number (count), Qnil);
      for (index = 0; index < count; index++)
	ASET (result, index,
	      cfobject_to_lisp (CFArrayGetValueAtIndex (obj, index),
				flags, hash_bound));
    }
  else if (type_id == CFDictionaryGetTypeID ())
    {
      struct cfdict_context context;
      CFIndex count = CFDictionaryGetCount (obj);

      tag = Qdictionary;
      context.result  = &result;
      context.flags = flags;
      context.hash_bound = hash_bound;
      if (hash_bound < 0 || count < hash_bound)
	{
	  result = Qnil;
	  CFDictionaryApplyFunction (obj, cfdictionary_add_to_list,
				     &context);
	}
      else
	{
	  result = make_hash_table (hashtest_equal,
				    make_number (count),
				    make_float (DEFAULT_REHASH_SIZE),
				    make_float (DEFAULT_REHASH_THRESHOLD),
				    Qnil);
	  CFDictionaryApplyFunction (obj, cfdictionary_puthash,
				     &context);
	}
    }
  else
    {
      Lisp_Object tag_result = mac_nsobject_to_lisp (obj);

      if (CONSP (tag_result))
	{
	  tag = XCAR (tag_result);
	  result = XCDR (tag_result);
	}
      else
	{
	  CFStringRef desc = CFCopyDescription (obj);

	  tag = Qdescription;
	  if (desc)
	    {
	      if (flags & CFOBJECT_TO_LISP_DONT_DECODE_STRING)
		result = cfstring_to_lisp_nodecode (desc);
	      else
		result = cfstring_to_lisp (desc);

	      CFRelease (desc);
	    }
	}
    }

  UNGCPRO;

  if (flags & CFOBJECT_TO_LISP_WITH_TAG)
    result = Fcons (tag, result);

  return result;
}

/* Convert CFPropertyList PLIST to a lisp object.  If WITH_TAG is
   non-zero, a symbol that represents the type of the original Core
   Foundation object is prepended.  HASH_BOUND specifies which kinds
   of the lisp objects, alists or hash tables, are used as the targets
   of the conversion from CFDictionary.  If HASH_BOUND is negative,
   always generate alists.  If HASH_BOUND >= 0, generate an alist if
   the number of keys in the dictionary is smaller than HASH_BOUND,
   and a hash table otherwise.  */

Lisp_Object
cfproperty_list_to_lisp (CFPropertyListRef plist, int with_tag, int hash_bound)
{
  return cfobject_to_lisp (plist, with_tag ? CFOBJECT_TO_LISP_WITH_TAG : 0,
			   hash_bound);
}

static CFPropertyListRef
cfproperty_list_create_with_lisp_1 (Lisp_Object obj,
				    struct bstree_node **ancestors)
{
  CFPropertyListRef result = NULL;
  Lisp_Object type, data;
  struct bstree_node **bstree_ref;

  if (!CONSP (obj))
    return NULL;

  type = XCAR (obj);
  data = XCDR (obj);
  if (EQ (type, Qstring))
    {
      if (STRINGP (data))
	result = cfstring_create_with_string (data);
    }
  else if (EQ (type, Qnumber))
    {
      if (INTEGERP (data))
	{
	  long value = XINT (data);

	  result = CFNumberCreate (NULL, kCFNumberLongType, &value);
	}
      else if (FLOATP (data))
	{
	  double value = XFLOAT_DATA (data);

	  result = CFNumberCreate (NULL, kCFNumberDoubleType, &value);
	}
      else if (STRINGP (data))
	{
	  SInt64 value = strtoll (SSDATA (data), NULL, 0);

	  result = CFNumberCreate (NULL, kCFNumberSInt64Type, &value);
	}
    }
  else if (EQ (type, Qboolean))
    {
      if (NILP (data))
	result = kCFBooleanFalse;
      else if (EQ (data, Qt))
	result = kCFBooleanTrue;
    }
  else if (EQ (type, Qdate))
    {
      if (CONSP (data) && INTEGERP (XCAR (data))
	  && CONSP (XCDR (data)) && INTEGERP (XCAR (XCDR (data)))
	  && CONSP (XCDR (XCDR (data)))
	  && INTEGERP (XCAR (XCDR (XCDR (data)))))
	{
	  CFAbsoluteTime at;

	  at = (XINT (XCAR (data)) * 65536.0 + XINT (XCAR (XCDR (data)))
		+ XINT (XCAR (XCDR (XCDR (data)))) * 0.000001
		- kCFAbsoluteTimeIntervalSince1970);
	  if (CONSP (XCDR (XCDR (XCDR (data))))
	      && INTEGERP (XCAR (XCDR (XCDR (XCDR (data))))))
	    at += XINT (XCAR (XCDR (XCDR (XCDR (data))))) * 1.0e-12;
	  result = CFDateCreate (NULL, at);
	}
    }
  else if (EQ (type, Qdata))
    {
      if (STRINGP (data))
	result = CFDataCreate (NULL, SDATA (data), SBYTES (data));
    }
  /* Recursive cases follow.  */
  else if ((bstree_ref = bstree_find (ancestors, obj),
	    *bstree_ref == NULL))
    {
      struct bstree_node node;

      node.obj = obj;
      node.left = node.right = NULL;
      *bstree_ref = &node;

      if (EQ (type, Qarray))
	{
	  if (VECTORP (data))
	    {
	      EMACS_INT size = ASIZE (data);
	      CFMutableArrayRef array =
		CFArrayCreateMutable (NULL, size, &kCFTypeArrayCallBacks);

	      if (array)
		{
		  EMACS_INT i;

		  for (i = 0; i < size; i++)
		    {
		      CFPropertyListRef value =
			cfproperty_list_create_with_lisp_1 (AREF (data, i),
							    ancestors);

		      if (value)
			{
			  CFArrayAppendValue (array, value);
			  CFRelease (value);
			}
		      else
			break;
		    }
		  if (i < size)
		    {
		      CFRelease (array);
		      array = NULL;
		    }
		}
	      result = array;
	    }
	}
      else if (EQ (type, Qdictionary))
	{
	  CFMutableDictionaryRef dictionary = NULL;

	  if (CONSP (data) || NILP (data))
	    {
	      EMACS_INT size = cdr_chain_length (data);

	      if (size >= 0)
		dictionary =
		  CFDictionaryCreateMutable (NULL, size,
					     &kCFTypeDictionaryKeyCallBacks,
					     &kCFTypeDictionaryValueCallBacks);
	      if (dictionary)
		{
		  for (; CONSP (data); data = XCDR (data))
		    {
		      CFPropertyListRef value = NULL;

		      if (CONSP (XCAR (data)) && STRINGP (XCAR (XCAR (data))))
			{
			  CFStringRef key =
			    cfstring_create_with_string (XCAR (XCAR (data)));

			  if (key)
			    {
			      value = cfproperty_list_create_with_lisp_1 (XCDR (XCAR (data)),
									  ancestors);
			      if (value)
				{
				  CFDictionaryAddValue (dictionary, key, value);
				  CFRelease (value);
				}
			      CFRelease (key);
			    }
			}
		      if (value == NULL)
			break;
		    }
		  if (!NILP (data))
		    {
		      CFRelease (dictionary);
		      dictionary = NULL;
		    }
		}
	    }
	  else if (HASH_TABLE_P (data))
	    {
	      struct Lisp_Hash_Table *h = XHASH_TABLE (data);

	      dictionary =
		CFDictionaryCreateMutable (NULL, h->count,
					   &kCFTypeDictionaryKeyCallBacks,
					   &kCFTypeDictionaryValueCallBacks);
	      if (dictionary)
		{
		  int i, size = HASH_TABLE_SIZE (h);

		  for (i = 0; i < size; ++i)
		    if (!NILP (HASH_HASH (h, i)))
		      {
			CFPropertyListRef value = NULL;

			if (STRINGP (HASH_KEY (h, i)))
			  {
			    CFStringRef key =
			      cfstring_create_with_string (HASH_KEY (h, i));

			    if (key)
			      {
				value = cfproperty_list_create_with_lisp_1 (HASH_VALUE (h, i),
									    ancestors);
				if (value)
				  {
				    CFDictionaryAddValue (dictionary,
							  key, value);
				    CFRelease (value);
				  }
				CFRelease (key);
			      }
			  }
			if (value == NULL)
			  break;
		      }
		  if (i < size)
		    {
		      CFRelease (dictionary);
		      dictionary = NULL;
		    }
		}
	    }
	  result = dictionary;
	}

      *bstree_ref = NULL;
    }

  return result;
}

/* Create CFPropertyList from a Lisp object OBJ, which must be a form
   of a return value of cfproperty_list_to_lisp with with_tag set.  */

CFPropertyListRef
cfproperty_list_create_with_lisp (Lisp_Object obj)
{
  struct bstree_node *root = NULL;

  return cfproperty_list_create_with_lisp_1 (obj, &root);
}

/* Convert CFPropertyList PLIST to a unibyte string in FORMAT, which
   is either kCFPropertyListXMLFormat_v1_0 or
   kCFPropertyListBinaryFormat_v1_0.  Return nil if an error has
   occurred.  */

Lisp_Object
cfproperty_list_to_string (CFPropertyListRef plist, CFPropertyListFormat format)
{
  Lisp_Object result = Qnil;
  CFDataRef data = NULL;

#if MAC_OS_X_VERSION_MAX_ALLOWED >= 1060
#if MAC_OS_X_VERSION_MIN_REQUIRED < 1060
  if (CFPropertyListCreateData != NULL)
#endif
    {
      data = CFPropertyListCreateData (NULL, plist, format, 0, NULL);
    }
#if MAC_OS_X_VERSION_MIN_REQUIRED < 1060
  else				/* CFPropertyListCreateData == NULL */
#endif
#endif	/* MAC_OS_X_VERSION_MAX_ALLOWED >= 1060 */
#if MAC_OS_X_VERSION_MIN_REQUIRED < 1060
    {
      CFWriteStreamRef stream;

      switch (format)
	{
	case kCFPropertyListXMLFormat_v1_0:
	  data = CFPropertyListCreateXMLData (NULL, plist);
	  break;

	case kCFPropertyListBinaryFormat_v1_0:
	  stream = CFWriteStreamCreateWithAllocatedBuffers (NULL, NULL);

	  if (stream)
	    {
	      CFWriteStreamOpen (stream);
	      if (CFPropertyListWriteToStream (plist, stream, format, NULL) > 0)
		data = CFWriteStreamCopyProperty (stream,
						  kCFStreamPropertyDataWritten);
	      CFWriteStreamClose (stream);
	      CFRelease (stream);
	    }
	  break;
	}
    }
#endif
  if (data)
    {
      result = cfdata_to_lisp (data);
      CFRelease (data);
    }

  return result;
}

/* Create CFPropertyList from a Lisp string in either
   kCFPropertyListXMLFormat_v1_0 or kCFPropertyListBinaryFormat_v1_0.
   Return NULL if an error has occurred.  */

CFPropertyListRef
cfproperty_list_create_with_string (Lisp_Object string)
{
  CFPropertyListRef result = NULL;
  CFDataRef data;

  string = Fstring_as_unibyte (string);
  data = CFDataCreateWithBytesNoCopy (NULL, SDATA (string), SBYTES (string),
				      kCFAllocatorNull);
  if (data)
    {
#if MAC_OS_X_VERSION_MAX_ALLOWED >= 1060
#if MAC_OS_X_VERSION_MIN_REQUIRED < 1060
      if (CFPropertyListCreateWithData != NULL)
#endif
	{
	  result = CFPropertyListCreateWithData (NULL, data,
						 kCFPropertyListImmutable,
						 NULL, NULL);
	}
#if MAC_OS_X_VERSION_MIN_REQUIRED < 1060
      else		    /* CFPropertyListCreateWithData == NULL */
#endif
#endif	/* MAC_OS_X_VERSION_MAX_ALLOWED >= 1060 */
#if MAC_OS_X_VERSION_MIN_REQUIRED < 1060
	{
	  result = CFPropertyListCreateFromXMLData (NULL, data,
						    kCFPropertyListImmutable,
						    NULL);
	}
#endif
      CFRelease (data);
    }

  return result;
}

/* Create CFPropertyList from the contents of the file specified by
   URL.  Return NULL if the creation failed.  */

static CFPropertyListRef
cfproperty_list_create_with_url (CFURLRef url)
{
  CFPropertyListRef result = NULL;
  CFReadStreamRef stream = CFReadStreamCreateWithFile (NULL, url);

  if (stream)
    {
      if (CFReadStreamOpen (stream))
	{
#if MAC_OS_X_VERSION_MIN_REQUIRED >= 1060
#if MAC_OS_X_VERSION_MIN_REQUIRED < 1060
	  if (CFPropertyListCreateWithStream != NULL)
#endif
	    {
	      result = CFPropertyListCreateWithStream (NULL, stream, 0,
						       kCFPropertyListImmutable,
						       NULL, NULL);
	    }
#if MAC_OS_X_VERSION_MIN_REQUIRED < 1060
	  else		  /* CFPropertyListCreateWithStream == NULL */
#endif
#endif	/* MAC_OS_X_VERSION_MAX_ALLOWED >= 1060 */
#if MAC_OS_X_VERSION_MIN_REQUIRED < 1060
	    {
	      result = CFPropertyListCreateFromStream (NULL, stream, 0,
						       kCFPropertyListImmutable,
						       NULL, NULL);
	    }
#endif
	  CFReadStreamClose (stream);
	}
      CFRelease (stream);
    }

  return result;
}


/***********************************************************************
		 Emulation of the X Resource Manager
 ***********************************************************************/

/* Parser functions for resource lines.  Each function takes an
   address of a variable whose value points to the head of a string.
   The value will be advanced so that it points to the next character
   of the parsed part when the function returns.

   A resource name such as "Emacs*font" is parsed into a non-empty
   list called `quarks'.  Each element is either a Lisp string that
   represents a concrete component, a Lisp symbol LOOSE_BINDING
   (actually Qlambda) that represents any number (>=0) of intervening
   components, or a Lisp symbol SINGLE_COMPONENT (actually Qquote)
   that represents as any single component.  */

#define P (*p)

#define LOOSE_BINDING    Qlambda /* '*' ("L"oose) */
#define SINGLE_COMPONENT Qquote	 /* '?' ("Q"uestion) */

static void
skip_white_space (const char **p)
{
  /* WhiteSpace = {<space> | <horizontal tab>} */
  while (*P == ' ' || *P == '\t')
    P++;
}

static bool
parse_comment (const char **p)
{
  /* Comment = "!" {<any character except null or newline>} */
  if (*P == '!')
    {
      P++;
      while (*P)
	if (*P++ == '\n')
	  break;
      return true;
    }
  else
    return false;
}

/* Don't interpret filename.  Just skip until the newline.  */
static bool
parse_include_file (const char **p)
{
  /* IncludeFile = "#" WhiteSpace "include" WhiteSpace FileName WhiteSpace */
  if (*P == '#')
    {
      P++;
      while (*P)
	if (*P++ == '\n')
	  break;
      return true;
    }
  else
    return false;
}

static char
parse_binding (const char **p)
{
  /* Binding = "." | "*"  */
  if (*P == '.' || *P == '*')
    {
      char binding = *P++;

      while (*P == '.' || *P == '*')
	if (*P++ == '*')
	  binding = '*';
      return binding;
    }
  else
    return '\0';
}

static Lisp_Object
parse_component (const char **p)
{
  /*  Component = "?" | ComponentName
      ComponentName = NameChar {NameChar}
      NameChar = "a"-"z" | "A"-"Z" | "0"-"9" | "_" | "-" */
  if (*P == '?')
    {
      P++;
      return SINGLE_COMPONENT;
    }
  else if (isalnum (*P) || *P == '_' || *P == '-')
    {
      const char *start = P++;

      while (isalnum (*P) || *P == '_' || *P == '-')
	P++;

      return make_unibyte_string (start, P - start);
    }
  else
    return Qnil;
}

static Lisp_Object
parse_resource_name (const char **p)
{
  Lisp_Object result = Qnil, component;
  char binding;

  /* ResourceName = [Binding] {Component Binding} ComponentName */
  if (parse_binding (p) == '*')
    result = Fcons (LOOSE_BINDING, result);

  component = parse_component (p);
  if (NILP (component))
    return Qnil;

  result = Fcons (component, result);
  while ((binding = parse_binding (p)) != '\0')
    {
      if (binding == '*')
	result = Fcons (LOOSE_BINDING, result);
      component = parse_component (p);
      if (NILP (component))
	return Qnil;
      else
	result = Fcons (component, result);
    }

  /* The final component should not be '?'.  */
  if (EQ (component, SINGLE_COMPONENT))
    return Qnil;

  return Fnreverse (result);
}

static Lisp_Object
parse_value (const char **p)
{
  char *q, *buf;
  Lisp_Object seq = Qnil, result;
  int buf_len, total_len = 0, len;

  q = strchr (P, '\n');
  buf_len = q ? q - P : strlen (P);
  buf = xmalloc (buf_len);

  while (1)
    {
      bool continue_p = false;

      q = buf;
      while (*P)
	{
	  if (*P == '\n')
	    {
	      P++;
	      break;
	    }
	  else if (*P == '\\')
	    {
	      P++;
	      if (*P == '\0')
		break;
	      else if (*P == '\n')
		{
		  P++;
		  continue_p = true;
		  break;
		}
	      else if (*P == 'n')
		{
		  *q++ = '\n';
		  P++;
		}
	      else if ('0' <= P[0] && P[0] <= '7'
		       && '0' <= P[1] && P[1] <= '7'
		       && '0' <= P[2] && P[2] <= '7')
		{
		  *q++ = ((P[0] - '0') << 6) + ((P[1] - '0') << 3) + (P[2] - '0');
		  P += 3;
		}
	      else
		*q++ = *P++;
	    }
	  else
	    *q++ = *P++;
	}
      len = q - buf;
      seq = Fcons (make_unibyte_string (buf, len), seq);
      total_len += len;

      if (continue_p)
	{
	  q = strchr (P, '\n');
	  len = q ? q - P : strlen (P);
	  if (len > buf_len)
	    {
	      xfree (buf);
	      buf_len = len;
	      buf = xmalloc (buf_len);
	    }
	}
      else
	break;
    }
  xfree (buf);

  if (SBYTES (XCAR (seq)) == total_len)
    return make_string (SSDATA (XCAR (seq)), total_len);
  else
    {
      buf = xmalloc (total_len);
      q = buf + total_len;
      for (; CONSP (seq); seq = XCDR (seq))
	{
	  len = SBYTES (XCAR (seq));
	  q -= len;
	  memcpy (q, SDATA (XCAR (seq)), len);
	}
      result = make_string (buf, total_len);
      xfree (buf);
      return result;
    }
}

static Lisp_Object
parse_resource_line (const char **p)
{
  Lisp_Object quarks, value;

  /* ResourceLine = Comment | IncludeFile | ResourceSpec | <empty line> */
  if (parse_comment (p) || parse_include_file (p))
    return Qnil;

  /* ResourceSpec = WhiteSpace ResourceName WhiteSpace ":" WhiteSpace Value */
  skip_white_space (p);
  quarks = parse_resource_name (p);
  if (NILP (quarks))
    goto cleanup;
  skip_white_space (p);
  if (*P != ':')
    goto cleanup;
  P++;
  skip_white_space (p);
  value = parse_value (p);
  return Fcons (quarks, value);

 cleanup:
  /* Skip the remaining data as a dummy value.  */
  parse_value (p);
  return Qnil;
}

#undef P

/* Equivalents of X Resource Manager functions.

   An X Resource Database acts as a collection of resource names and
   associated values.  It is implemented as a trie on quarks.  Namely,
   each edge is labeled by either a string, LOOSE_BINDING, or
   SINGLE_COMPONENT.  Each node has a node id, which is a unique
   nonnegative integer, and the root node id is 0.  A database is
   implemented as a hash table that maps a pair (SRC-NODE-ID .
   EDGE-LABEL) to DEST-NODE-ID.  It also holds a maximum node id used
   in the table as a value for HASHKEY_MAX_NID.  A value associated to
   a node is recorded as a value for the node id.

   A database also has a cache for past queries as a value for
   HASHKEY_QUERY_CACHE.  It is another hash table that maps
   "NAME-STRING\0CLASS-STRING" to the result of the query.  */

#define HASHKEY_MAX_NID (make_number (0))
#define HASHKEY_QUERY_CACHE (make_number (-1))

static XrmDatabase
xrm_create_database (void)
{
  XrmDatabase database;

  database = make_hash_table (hashtest_equal, make_number (DEFAULT_HASH_SIZE),
			      make_float (DEFAULT_REHASH_SIZE),
			      make_float (DEFAULT_REHASH_THRESHOLD), Qnil);
  Fputhash (HASHKEY_MAX_NID, make_number (0), database);
  Fputhash (HASHKEY_QUERY_CACHE, Qnil, database);

  return database;
}

static void
xrm_q_put_resource (XrmDatabase database, Lisp_Object quarks, Lisp_Object value)
{
  struct Lisp_Hash_Table *h = XHASH_TABLE (database);
  EMACS_UINT hash_code;
  int i;
  EMACS_INT max_nid;
  Lisp_Object node_id, key;

  max_nid = XINT (Fgethash (HASHKEY_MAX_NID, database, Qnil));

  XSETINT (node_id, 0);
  for (; CONSP (quarks); quarks = XCDR (quarks))
    {
      key = Fcons (node_id, XCAR (quarks));
      i = hash_lookup (h, key, &hash_code);
      if (i < 0)
	{
	  max_nid++;
	  XSETINT (node_id, max_nid);
	  hash_put (h, key, node_id, hash_code);
	}
      else
	node_id = HASH_VALUE (h, i);
    }
  Fputhash (node_id, value, database);

  Fputhash (HASHKEY_MAX_NID, make_number (max_nid), database);
  Fputhash (HASHKEY_QUERY_CACHE, Qnil, database);
}

/* Merge multiple resource entries specified by DATA into a resource
   database DATABASE.  DATA points to the head of a null-terminated
   string consisting of multiple resource lines.  It's like a
   combination of XrmGetStringDatabase and XrmMergeDatabases.  */

void
xrm_merge_string_database (XrmDatabase database, const char *data)
{
  Lisp_Object quarks_value;

  while (*data)
    {
      quarks_value = parse_resource_line (&data);
      if (!NILP (quarks_value))
	xrm_q_put_resource (database,
			    XCAR (quarks_value), XCDR (quarks_value));
    }
}

static Lisp_Object
xrm_q_get_resource_1 (XrmDatabase database, Lisp_Object node_id,
		      Lisp_Object quark_name, Lisp_Object quark_class)
{
  struct Lisp_Hash_Table *h = XHASH_TABLE (database);
  Lisp_Object key, labels[3], value;
  int i, k;

  if (!CONSP (quark_name))
    return Fgethash (node_id, database, Qnil);

  /* First, try tight bindings */
  labels[0] = XCAR (quark_name);
  labels[1] = XCAR (quark_class);
  labels[2] = SINGLE_COMPONENT;

  key = Fcons (node_id, Qnil);
  for (k = 0; k < sizeof (labels) / sizeof (*labels); k++)
    {
      XSETCDR (key, labels[k]);
      i = hash_lookup (h, key, NULL);
      if (i >= 0)
	{
	  value = xrm_q_get_resource_1 (database, HASH_VALUE (h, i),
					XCDR (quark_name), XCDR (quark_class));
	  if (!NILP (value))
	    return value;
	}
    }

  /* Then, try loose bindings */
  XSETCDR (key, LOOSE_BINDING);
  i = hash_lookup (h, key, NULL);
  if (i >= 0)
    {
      value = xrm_q_get_resource_1 (database, HASH_VALUE (h, i),
				    quark_name, quark_class);
      if (!NILP (value))
	return value;
      else
	return xrm_q_get_resource_1 (database, node_id,
				     XCDR (quark_name), XCDR (quark_class));
    }
  else
    return Qnil;
}

static Lisp_Object
xrm_q_get_resource (XrmDatabase database, Lisp_Object quark_name,
		    Lisp_Object quark_class)
{
  return xrm_q_get_resource_1 (database, make_number (0),
			       quark_name, quark_class);
}

/* Retrieve a resource value for the specified NAME and CLASS from the
   resource database DATABASE.  It corresponds to XrmGetResource.  */

Lisp_Object
xrm_get_resource (XrmDatabase database, const char *name, const char *class)
{
  Lisp_Object key, query_cache, quark_name, quark_class, tmp;
  int i, nn, nc;
  struct Lisp_Hash_Table *h;
  EMACS_UINT hash_code;

  nn = strlen (name);
  nc = strlen (class);
  key = make_uninit_string (nn + nc + 1);
  memcpy (SDATA (key), name, nn + 1);
  memcpy (SDATA (key) + nn + 1, class, nc);

  query_cache = Fgethash (HASHKEY_QUERY_CACHE, database, Qnil);
  if (NILP (query_cache))
    {
      query_cache = make_hash_table (hashtest_equal,
				     make_number (DEFAULT_HASH_SIZE),
				     make_float (DEFAULT_REHASH_SIZE),
				     make_float (DEFAULT_REHASH_THRESHOLD),
				     Qnil);
      Fputhash (HASHKEY_QUERY_CACHE, query_cache, database);
    }
  h = XHASH_TABLE (query_cache);
  i = hash_lookup (h, key, &hash_code);
  if (i >= 0)
    return HASH_VALUE (h, i);

  quark_name = parse_resource_name (&name);
  if (*name != '\0')
    return Qnil;
  for (tmp = quark_name, nn = 0; CONSP (tmp); tmp = XCDR (tmp), nn++)
    if (!STRINGP (XCAR (tmp)))
      return Qnil;

  quark_class = parse_resource_name (&class);
  if (*class != '\0')
    return Qnil;
  for (tmp = quark_class, nc = 0; CONSP (tmp); tmp = XCDR (tmp), nc++)
    if (!STRINGP (XCAR (tmp)))
      return Qnil;

  if (nn != nc)
    return Qnil;
  else
    {
      tmp = xrm_q_get_resource (database, quark_name, quark_class);
      hash_put (h, key, tmp, hash_code);
      return tmp;
    }
}

static Lisp_Object
xrm_cfproperty_list_to_value (CFPropertyListRef plist)
{
  CFTypeID type_id = CFGetTypeID (plist);

  if (type_id == CFStringGetTypeID ())
    return cfstring_to_lisp (plist);
  else if (type_id == CFNumberGetTypeID ())
    {
      CFStringRef string;
      Lisp_Object result = Qnil;

      string = CFStringCreateWithFormat (NULL, NULL, CFSTR ("%@"), plist);
      if (string)
	{
	  result = cfstring_to_lisp (string);
	  CFRelease (string);
	}
      return result;
    }
  else if (type_id == CFBooleanGetTypeID ())
    return build_string (CFBooleanGetValue (plist) ? "true" : "false");
  else if (type_id == CFDataGetTypeID ())
    return cfdata_to_lisp (plist);
  else
    return Qnil;
}

/* Create a new resource database from the preferences for the
   application APPLICATION.  APPLICATION is either a string that
   specifies an application ID, or NULL that represents the current
   application.  */

XrmDatabase
xrm_get_preference_database (const char *application)
{
  CFStringRef app_id, *keys, user_doms[2], host_doms[2];
  CFMutableSetRef key_set = NULL;
  CFArrayRef key_array;
  CFIndex index, count;
  XrmDatabase database;
  Lisp_Object quarks = Qnil, value = Qnil;
  CFPropertyListRef plist;
  int iu, ih;
  struct gcpro gcpro1, gcpro2, gcpro3;

  user_doms[0] = kCFPreferencesCurrentUser;
  user_doms[1] = kCFPreferencesAnyUser;
  host_doms[0] = kCFPreferencesCurrentHost;
  host_doms[1] = kCFPreferencesAnyHost;

  database = xrm_create_database ();

  GCPRO3 (database, quarks, value);

  app_id = kCFPreferencesCurrentApplication;
  if (application)
    {
      app_id = cfstring_create_with_utf8_cstring (application);
      if (app_id == NULL)
	goto out;
    }
  if (!CFPreferencesAppSynchronize (app_id))
    goto out;

  key_set = CFSetCreateMutable (NULL, 0, &kCFCopyStringSetCallBacks);
  if (key_set == NULL)
    goto out;
  for (iu = 0; iu < sizeof (user_doms) / sizeof (*user_doms) ; iu++)
    for (ih = 0; ih < sizeof (host_doms) / sizeof (*host_doms); ih++)
      {
	key_array = CFPreferencesCopyKeyList (app_id, user_doms[iu],
					      host_doms[ih]);
	if (key_array)
	  {
	    count = CFArrayGetCount (key_array);
	    for (index = 0; index < count; index++)
	      CFSetAddValue (key_set,
			     CFArrayGetValueAtIndex (key_array, index));
	    CFRelease (key_array);
	  }
      }

  count = CFSetGetCount (key_set);
  keys = xmalloc (sizeof (CFStringRef) * count);
  CFSetGetValues (key_set, (const void **)keys);
  for (index = 0; index < count; index++)
    {
      const char *res_name = SSDATA (cfstring_to_lisp_nodecode (keys[index]));

      quarks = parse_resource_name (&res_name);
      if (!(NILP (quarks) || *res_name))
	{
	  plist = CFPreferencesCopyAppValue (keys[index], app_id);
	  value = xrm_cfproperty_list_to_value (plist);
	  CFRelease (plist);
	  if (!NILP (value))
	    xrm_q_put_resource (database, quarks, value);
	}
    }

  xfree (keys);
 out:
  if (key_set)
    CFRelease (key_set);
  if (app_id)
    CFRelease (app_id);

  UNGCPRO;

  return database;
}


Lisp_Object Qmac_file_alias_p;

void
initialize_applescript (void)
{
  AEDesc null_desc;
  OSAError osaerror;

  /* if open fails, as_scripting_component is set to NULL.  Its
     subsequent use in OSA calls will fail with badComponentInstance
     error.  */
  as_scripting_component = OpenDefaultComponent (kOSAComponentType,
						 kAppleScriptSubtype);

  null_desc.descriptorType = typeNull;
  null_desc.dataHandle = 0;
  osaerror = OSAMakeContext (as_scripting_component, &null_desc,
			     kOSANullScript, &as_script_context);
  if (osaerror)
    as_script_context = kOSANullScript;
      /* use default context if create fails */
}


void
terminate_applescript (void)
{
  OSADispose (as_scripting_component, as_script_context);
  CloseComponent (as_scripting_component);
}

/* Convert a lisp string to the 4 byte character code.  */

OSType
mac_get_code_from_arg (Lisp_Object arg, OSType defCode)
{
  OSType result;
  if (NILP(arg))
    {
      result = defCode;
    }
  else
    {
      /* check type string */
      CHECK_STRING(arg);
      if (!mac_string_to_four_char_code (arg, &result))
	{
	  error ("Wrong argument: need string of length 4 for code");
	}
    }
  return result;
}

DEFUN ("mac-file-alias-p", Fmac_file_alias_p, Smac_file_alias_p, 1, 1, 0,
       doc: /* Return non-nil if file FILENAME is the name of an alias file.
The value is the file referred to by the alias file, as a string.
Otherwise it returns nil.

This function returns t when given the name of an alias file
containing an unresolvable alias.  */)
  (Lisp_Object filename)
{
  Lisp_Object handler, result = Qnil;

  CHECK_STRING (filename);
  filename = Fexpand_file_name (filename, Qnil);

  /* If the file name has special constructs in it,
     call the corresponding file handler.  */
  handler = Ffind_file_name_handler (filename, Qmac_file_alias_p);
  if (!NILP (handler))
    return call2 (handler, Qmac_file_alias_p, filename);

  block_input ();
#if MAC_OS_X_VERSION_MAX_ALLOWED >= 1060
#if MAC_OS_X_VERSION_MIN_REQUIRED < 1060
  if (CFURLCreateByResolvingBookmarkData != NULL)
#endif
    {
      Lisp_Object encoded_filename = ENCODE_FILE (filename);
      CFURLRef url =
	CFURLCreateFromFileSystemRepresentation (NULL, SDATA (encoded_filename),
						 SBYTES (encoded_filename),
						 false);

      if (url)
	{
	  CFBooleanRef is_alias_file = NULL, is_symbolic_link = NULL;

	  if (CFURLCopyResourcePropertyForKey (url, kCFURLIsAliasFileKey,
					       &is_alias_file, NULL)
	      && CFBooleanGetValue (is_alias_file)
	      /* kCFURLIsAliasFileKey returns true also for a symbolic
		 link.  */
	      && CFURLCopyResourcePropertyForKey (url, kCFURLIsSymbolicLinkKey,
						  &is_symbolic_link, NULL)
	      && !CFBooleanGetValue (is_symbolic_link))
	    {
	      CFDataRef data;
	      Boolean stale_p;
	      CFURLRef resolved_url = NULL;

	      data = CFURLCreateBookmarkDataFromFile (NULL, url, NULL);
	      if (data)
		{
		  CFURLBookmarkResolutionOptions options =
		    (kCFBookmarkResolutionWithoutUIMask
		     | kCFBookmarkResolutionWithoutMountingMask);

		  resolved_url =
		    CFURLCreateByResolvingBookmarkData (NULL, data, options,
							NULL, NULL,
							&stale_p, NULL);
		  CFRelease (data);
		}
	      if (resolved_url)
		{
		  char buf[MAXPATHLEN];

		  if (!stale_p
		      && CFURLGetFileSystemRepresentation (resolved_url, true,
							   (UInt8 *) buf,
							   sizeof (buf)))
		    result = make_unibyte_string (buf, strlen (buf));
		  CFRelease (resolved_url);
		}
	      if (!STRINGP (result))
		result = Qt;
	    }
	  if (is_alias_file)
	    CFRelease (is_alias_file);
	  if (is_symbolic_link)
	    CFRelease (is_symbolic_link);
	  CFRelease (url);
	}
    }
#if MAC_OS_X_VERSION_MIN_REQUIRED < 1060
  else		      /* CFURLCreateByResolvingBookmarkData == NULL */
#endif
#endif
#if MAC_OS_X_VERSION_MIN_REQUIRED < 1060
    {
      OSStatus err;
      FSRef fref;

      err = FSPathMakeRef (SDATA (ENCODE_FILE (filename)), &fref, NULL);
      if (err == noErr)
	{
	  Boolean alias_p = false, folder_p;

	  err = FSResolveAliasFileWithMountFlags (&fref, false,
						  &folder_p, &alias_p,
						  kResolveAliasFileNoUI);
	  if (err != noErr)
	    result = Qt;
	  else if (alias_p)
	    {
	      char buf[MAXPATHLEN];

	      err = FSRefMakePath (&fref, (UInt8 *) buf, sizeof (buf));
	      if (err == noErr)
		result = make_unibyte_string (buf, strlen (buf));
	    }
	}
    }
#endif
  unblock_input ();

  if (STRINGP (result))
    {
      char *p = SSDATA (result);

      if (p[0] == '/' && strchr (p, ':'))
	result = concat2 (build_string ("/:"), result);
      result = DECODE_FILE (result);
    }

  return result;
}

/* Moving files to the system recycle bin.
   Used by `move-file-to-trash' instead of the default moving to ~/.Trash  */
DEFUN ("system-move-file-to-trash", Fsystem_move_file_to_trash,
       Ssystem_move_file_to_trash, 1, 1, 0,
       doc: /* Move file or directory named FILENAME to the recycle bin.  */)
  (Lisp_Object filename)
{
  enum {NO_ERROR, POSIX_ERROR, OSSTATUS_ERROR, COCOA_ERROR, OTHER_ERROR} domain;
  CFIndex code;
  Lisp_Object errstring = Qnil;
  Lisp_Object handler;
  Lisp_Object encoded_file;
  Lisp_Object operation;
#if MAC_OS_X_VERSION_MAX_ALLOWED >= 1050
  bool use_finder_p;
#endif

  operation = Qdelete_file;
  if (!NILP (Ffile_directory_p (filename))
      && NILP (Ffile_symlink_p (filename)))
    {
      operation = intern ("delete-directory");
      filename = Fdirectory_file_name (filename);
    }
  filename = Fexpand_file_name (filename, Qnil);

  handler = Ffind_file_name_handler (filename, operation);
  if (!NILP (handler))
    return call2 (handler, operation, filename);

  encoded_file = ENCODE_FILE (filename);

  block_input ();
  domain = NO_ERROR;
#if MAC_OS_X_VERSION_MAX_ALLOWED >= 1050
  use_finder_p = mac_system_move_file_to_trash_use_finder;
  if (!use_finder_p)
    {
      CFErrorRef error;

      if (!mac_trash_file (SSDATA (encoded_file), &error))
	{
	  if (error == NULL)
	    use_finder_p = true;
	  else
	    {
	      CFStringRef error_domain = CFErrorGetDomain (error);
	      CFStringRef reason = CFErrorCopyFailureReason (error);

	      if (reason)
		{
		  errstring = cfstring_to_lisp (reason);
		  CFRelease (reason);
		}

	      code = CFErrorGetCode (error);
	      if (CFEqual (error_domain, kCFErrorDomainCocoa))
		domain = COCOA_ERROR;
	      else if (CFEqual (error_domain, kCFErrorDomainOSStatus))
		domain = OSSTATUS_ERROR;
	      else if (CFEqual (error_domain, kCFErrorDomainPOSIX))
		domain = POSIX_ERROR;
	      else
		domain = OTHER_ERROR;

	      CFRelease (error);
	    }
	}
    }
  if (use_finder_p)
#endif
    {
      OSStatus err;
      const OSType finderSignature = 'MACS';
      AEDesc desc;
      AppleEvent event, reply;

      err = AECoercePtr (TYPE_FILE_NAME, SDATA (encoded_file),
			 SBYTES (encoded_file), typeFileURL, &desc);
      if (err == noErr)
	{
	  err = AEBuildAppleEvent (kAECoreSuite, kAEDelete, typeApplSignature,
				   &finderSignature, sizeof (OSType),
				   kAutoGenerateReturnID, kAnyTransactionID,
				   &event, NULL, "'----':@", &desc);
	  AEDisposeDesc (&desc);
	}
      if (err == noErr)
	{
	  err = AESendMessage (&event, &reply, kAEWaitReply | kAENeverInteract,
			       kAEDefaultTimeout);
	  AEDisposeDesc (&event);
	}
      if (err == noErr)
	{
	  if (reply.descriptorType != typeNull)
	    {
	      OSStatus err1, handler_err;
	      AEDesc desc;

	      err1 = AEGetParamPtr (&reply, keyErrorNumber, typeSInt32, NULL,
				    &handler_err, sizeof (OSStatus), NULL);
	      if (err1 != errAEDescNotFound)
		err = handler_err;
	      err1 = AEGetParamDesc (&reply, keyErrorString, typeUTF8Text,
				     &desc);
	      if (err1 == noErr)
		{
		  errstring = make_uninit_string (AEGetDescDataSize (&desc));
		  err1 = AEGetDescData (&desc, SDATA (errstring),
					SBYTES (errstring));
		  if (err1 == noErr)
		    errstring =
		      code_convert_string_norecord (errstring, Qutf_8, 0);
		  else
		    errstring = Qnil;
		  AEDisposeDesc (&desc);
		}
	    }
	  AEDisposeDesc (&reply);
	}
      if (err != noErr)
	{
	  domain = OSSTATUS_ERROR;
	  code = err;
	}
    }
  unblock_input ();

  if (domain != NO_ERROR)
    {
      if ((domain == OSSTATUS_ERROR && code == fnfErr)
	  || (domain == COCOA_ERROR && code == 4)) /* NSFileNoSuchFileError */
	{
	  domain = POSIX_ERROR;
	  code = ENOENT;
	}
      else if ((domain == OSSTATUS_ERROR && code == afpAccessDenied)
	       || (domain == COCOA_ERROR
		   && code == 513)) /* NSFileWriteNoPermissionError */
	{
	  domain = POSIX_ERROR;
	  code = EACCES;
	}

      if (domain == POSIX_ERROR)
	report_file_errno ("Removing old name", list1 (filename), code);
      else
	{
	  if (NILP (errstring))
	    {
	      char *prefix =
		(domain == OSSTATUS_ERROR ? "Mac error "
		 : (domain == COCOA_ERROR ? "Cocoa error "
		    : "other error "));

	      errstring = concat2 (build_string (prefix),
				   Fnumber_to_string (make_number (code)));
	    }

	  xsignal (Qfile_error, list3 (build_string ("Removing old name"),
				       errstring, filename));
	}
    }

  return Qnil;
}


/* Compile and execute the AppleScript SCRIPT and return the error
   status as function value.  A zero is returned if compilation and
   execution is successful, in which case *RESULT is set to a Lisp
   string containing the resulting script value.  Otherwise, the Mac
   error code is returned and *RESULT is set to an error Lisp string.
   For documentation on the MacOS scripting architecture, see Inside
   Macintosh - Interapplication Communications: Scripting
   Components.  */

long
do_applescript (Lisp_Object script, Lisp_Object *result)
{
  AEDesc script_desc, result_desc, error_desc, *desc = NULL;
  OSErr error;
  OSAError osaerror;
  DescType desc_type;

  *result = Qnil;

  if (!as_scripting_component)
    initialize_applescript();

  if (STRING_MULTIBYTE (script))
    {
      desc_type = typeUnicodeText;
      script = code_convert_string_norecord (script,
#ifdef WORDS_BIGENDIAN
					     intern ("utf-16be"),
#else
					     intern ("utf-16le"),
#endif
					     1);
    }
  else
    desc_type = typeChar;

  error = AECreateDesc (desc_type, SDATA (script), SBYTES (script),
			&script_desc);
  if (error)
    return error;

  osaerror = OSADoScript (as_scripting_component, &script_desc, kOSANullScript,
			  desc_type, kOSAModeNull, &result_desc);

  if (osaerror == noErr)
    /* success: retrieve resulting script value */
    desc = &result_desc;
  else if (osaerror == errOSAScriptError)
    /* error executing AppleScript: retrieve error message */
    if (!OSAScriptError (as_scripting_component, kOSAErrorMessage, desc_type,
			 &error_desc))
      desc = &error_desc;

  if (desc)
    {
      *result = make_uninit_string (AEGetDescDataSize (desc));
      AEGetDescData (desc, SDATA (*result), SBYTES (*result));
      if (desc_type == typeUnicodeText)
	*result = code_convert_string_norecord (*result,
#ifdef WORDS_BIGENDIAN
						intern ("utf-16be"),
#else
						intern ("utf-16le"),
#endif
						0);
      AEDisposeDesc (desc);
    }

  AEDisposeDesc (&script_desc);

  return osaerror;
}


DEFUN ("do-applescript", Fdo_applescript, Sdo_applescript, 1, 1, 0,
       doc: /* Compile and execute AppleScript SCRIPT and return the result.
If compilation and execution are successful, the resulting script
value is returned as a string.  Otherwise the function aborts and
displays the error message returned by the AppleScript scripting
component.

If SCRIPT is a multibyte string, it is regarded as a Unicode text.
Otherwise, SCRIPT is regarded as a byte sequence in a Mac traditional
encoding specified by `mac-system-script-code', just as in Emacs 22.
Note that a unibyte ASCII-only SCRIPT does not always have the same
meaning as the multibyte counterpart.  For example, `\\x5c' in a
unibyte SCRIPT is interpreted as a yen sign when the value of
`mac-system-script-code' is 1 (smJapanese), but the same character in
a multibyte SCRIPT is interpreted as a reverse solidus.  You may want
to apply `string-to-multibyte' to the script if it is given as an
ASCII-only string literal.  */)
  (Lisp_Object script)
{
  Lisp_Object result;
  long status;

  CHECK_STRING (script);

  block_input ();
  if (!inhibit_window_system)
    status = mac_appkit_do_applescript (script, &result);
  else
    status = do_applescript (script, &result);
  unblock_input ();
  if (status == 0)
    return result;
  else if (!STRINGP (result))
    error ("AppleScript error %ld", status);
  else
    error ("%s", SDATA (result));
}


DEFUN ("mac-coerce-ae-data", Fmac_coerce_ae_data, Smac_coerce_ae_data, 3, 3, 0,
       doc: /* Coerce Apple event data SRC-DATA of type SRC-TYPE to DST-TYPE.
Each type should be a string of length 4 or the symbol
`undecoded-file-name'.  */)
  (Lisp_Object src_type, Lisp_Object src_data, Lisp_Object dst_type)
{
  OSErr err;
  Lisp_Object result = Qnil;
  DescType src_desc_type, dst_desc_type;
  AEDesc dst_desc;

  CHECK_STRING (src_data);
  if (EQ (src_type, Qundecoded_file_name))
    src_desc_type = TYPE_FILE_NAME;
  else
    src_desc_type = mac_get_code_from_arg (src_type, 0);

  if (EQ (dst_type, Qundecoded_file_name))
    dst_desc_type = TYPE_FILE_NAME;
  else
    dst_desc_type = mac_get_code_from_arg (dst_type, 0);

  block_input ();
  err = AECoercePtr (src_desc_type, SDATA (src_data), SBYTES (src_data),
		     dst_desc_type, &dst_desc);
  if (err == noErr)
    {
      result = Fcdr (mac_aedesc_to_lisp (&dst_desc));
      AEDisposeDesc (&dst_desc);
    }
  unblock_input ();

  return result;
}


static Lisp_Object Qxml, Qxml1, Qbinary1, QCmime_charset;
static Lisp_Object QNFD, QNFKD, QNFC, QNFKC, QHFS_plus_D, QHFS_plus_C;

DEFUN ("mac-get-preference", Fmac_get_preference, Smac_get_preference, 1, 4, 0,
       doc: /* Return the application preference value for KEY.
KEY is either a string specifying a preference key, or a list of key
strings.  If it is a list, the (i+1)-th element is used as a key for
the CFDictionary value obtained by the i-th element.  Return nil if
lookup is failed at some stage.

Optional arg APPLICATION is an application ID string.  If omitted or
nil, that stands for the current application.

Optional args FORMAT and HASH-BOUND specify the data format of the
return value (see `mac-convert-property-list').  FORMAT also accepts
`xml' as a synonym of `xml1' for compatibility.  */)
  (Lisp_Object key, Lisp_Object application, Lisp_Object format,
   Lisp_Object hash_bound)
{
  CFStringRef app_id, key_str;
  CFPropertyListRef app_plist = NULL, plist;
  Lisp_Object result = Qnil, tmp;
  struct gcpro gcpro1, gcpro2;

  if (STRINGP (key))
    key = Fcons (key, Qnil);
  else
    {
      CHECK_CONS (key);
      for (tmp = key; CONSP (tmp); tmp = XCDR (tmp))
	{
	  CHECK_STRING_CAR (tmp);
	  QUIT;
	}
      CHECK_TYPE (NILP (tmp), Qlistp, key);
    }
  if (!NILP (application))
    CHECK_STRING (application);
  CHECK_SYMBOL (format);
  if (!NILP (hash_bound))
    CHECK_NUMBER (hash_bound);

  GCPRO2 (key, format);

  block_input ();

  app_id = kCFPreferencesCurrentApplication;
  if (!NILP (application))
    {
      app_id = cfstring_create_with_string (application);
      if (app_id == NULL)
	goto out;
    }
  if (!CFPreferencesAppSynchronize (app_id))
    goto out;

  key_str = cfstring_create_with_string (XCAR (key));
  if (key_str == NULL)
    goto out;
  app_plist = CFPreferencesCopyAppValue (key_str, app_id);
  CFRelease (key_str);
  if (app_plist == NULL)
    goto out;

  plist = app_plist;
  for (key = XCDR (key); CONSP (key); key = XCDR (key))
    {
      if (CFGetTypeID (plist) != CFDictionaryGetTypeID ())
	break;
      key_str = cfstring_create_with_string (XCAR (key));
      if (key_str == NULL)
	goto out;
      plist = CFDictionaryGetValue (plist, key_str);
      CFRelease (key_str);
      if (plist == NULL)
	goto out;
    }

  if (NILP (key))
    {
      if (EQ (format, Qxml) || EQ (format, Qxml1))
	result = cfproperty_list_to_string (plist,
					    kCFPropertyListXMLFormat_v1_0);
      else if (EQ (format, Qbinary1))
	result = cfproperty_list_to_string (plist,
					    kCFPropertyListBinaryFormat_v1_0);
      else
	result =
	  cfproperty_list_to_lisp (plist, EQ (format, Qt),
				   NILP (hash_bound) ? -1 : XINT (hash_bound));
    }

 out:
  if (app_plist)
    CFRelease (app_plist);
  if (app_id)
    CFRelease (app_id);

  unblock_input ();

  UNGCPRO;

  return result;
}

DEFUN ("mac-convert-property-list", Fmac_convert_property_list, Smac_convert_property_list, 1, 3, 0,
       doc: /* Convert Core Foundation PROPERTY-LIST to FORMAT.
PROPERTY-LIST should be either a string whose data is in some Core
Foundation property list file format (e.g., XML or binary version 1),
or a Lisp representation of a property list with type tags.  Return
nil if PROPERTY-LIST is ill-formatted.

In the Lisp representation of a property list, each Core Foundation
object is converted into a corresponding Lisp object as follows:

  Core Foundation    Lisp                           Tag
  ------------------------------------------------------------
  CFString           Multibyte string               string
  CFNumber           Integer, float, or string      number
  CFBoolean          Symbol (t or nil)              boolean
  CFDate             List of three or four integers date
                       (cf. `current-time')
  CFData             Unibyte string                 data
  CFArray            Vector                         array
  CFDictionary       Alist or hash table            dictionary
                       (depending on HASH-BOUND)

If the representation has type tags, each object is a cons of the tag
symbol in the `Tag' row and a value of the type in the `Lisp' row.

Optional arg FORMAT specifies the data format of the return value.  If
omitted or nil, a Lisp representation without tags is returned.  If
FORMAT is t, a Lisp representation with tags is returned.  If FORMAT
is `xml1' or `binary1', a unibyte string is returned as an XML or
binary representation version 1, respectively.

Optional arg HASH-BOUND specifies which kinds of the Lisp objects,
alists or hash tables, are used as the targets of the conversion from
CFDictionary.  If HASH-BOUND is a negative integer or nil, always
generate alists.  If HASH-BOUND >= 0, generate an alist if the number
of keys in the dictionary is smaller than HASH-BOUND, and a hash table
otherwise.  */)
  (Lisp_Object property_list, Lisp_Object format, Lisp_Object hash_bound)
{
  Lisp_Object result = Qnil;
  CFPropertyListRef plist;
  struct gcpro gcpro1, gcpro2;

  if (!CONSP (property_list))
    CHECK_STRING (property_list);
  if (!NILP (hash_bound))
    CHECK_NUMBER (hash_bound);

  GCPRO2 (property_list, format);

  block_input ();

  if (CONSP (property_list))
    plist = cfproperty_list_create_with_lisp (property_list);
  else
    plist = cfproperty_list_create_with_string (property_list);
  if (plist)
    {
      if (EQ (format, Qxml1))
	result = cfproperty_list_to_string (plist,
					    kCFPropertyListXMLFormat_v1_0);
      else if (EQ (format, Qbinary1))
	result = cfproperty_list_to_string (plist,
					    kCFPropertyListBinaryFormat_v1_0);
      else
	result =
	  cfproperty_list_to_lisp (plist, EQ (format, Qt),
				   NILP (hash_bound) ? -1 : XINT (hash_bound));
      CFRelease (plist);
    }

  unblock_input ();

  UNGCPRO;

  return result;
}

static CFStringEncoding
get_cfstring_encoding_from_lisp (Lisp_Object obj)
{
  CFStringRef iana_name;
  CFStringEncoding encoding = kCFStringEncodingInvalidId;

  if (NILP (obj))
    return kCFStringEncodingUnicode;

  if (INTEGERP (obj))
    return XINT (obj);

  if (SYMBOLP (obj) && !NILP (Fcoding_system_p (obj)))
    {
      Lisp_Object attrs, plist;

      attrs = AREF (CODING_SYSTEM_SPEC (obj), 0);
      plist = CODING_ATTR_PLIST (attrs);
      obj = Fplist_get (plist, QCmime_charset);
    }

  if (SYMBOLP (obj))
    obj = SYMBOL_NAME (obj);

  if (STRINGP (obj))
    {
      iana_name = cfstring_create_with_string (obj);
      if (iana_name)
	{
	  encoding = CFStringConvertIANACharSetNameToEncoding (iana_name);
	  CFRelease (iana_name);
	}
    }

  return encoding;
}

static CFStringRef
cfstring_create_normalized (CFStringRef str, Lisp_Object symbol)
{
  int form = -1;
  TextEncodingVariant variant;
  float initial_mag = 0.0;
  CFStringRef result = NULL;

  if (EQ (symbol, QNFD))
    form = kCFStringNormalizationFormD;
  else if (EQ (symbol, QNFKD))
    form = kCFStringNormalizationFormKD;
  else if (EQ (symbol, QNFC))
    form = kCFStringNormalizationFormC;
  else if (EQ (symbol, QNFKC))
    form = kCFStringNormalizationFormKC;
  else if (EQ (symbol, QHFS_plus_D))
    {
      variant = kUnicodeHFSPlusDecompVariant;
      initial_mag = 1.5;
    }
  else if (EQ (symbol, QHFS_plus_C))
    {
      variant = kUnicodeHFSPlusCompVariant;
      initial_mag = 1.0;
    }

  if (form >= 0)
    {
      CFMutableStringRef mut_str = CFStringCreateMutableCopy (NULL, 0, str);

      if (mut_str)
	{
	  CFStringNormalize (mut_str, form);
	  result = mut_str;
	}
    }
  else if (initial_mag > 0.0)
    {
      UnicodeToTextInfo uni = NULL;
      UnicodeMapping map;
      CFIndex length;
      UniChar *in_text, *buffer = NULL, *out_buf = NULL;
      OSStatus err = noErr;
      ByteCount out_read, out_size, out_len;

      map.unicodeEncoding = CreateTextEncoding (kTextEncodingUnicodeDefault,
						kUnicodeNoSubset,
						kTextEncodingDefaultFormat);
      map.otherEncoding = CreateTextEncoding (kTextEncodingUnicodeDefault,
					      variant,
					      kTextEncodingDefaultFormat);
      map.mappingVersion = kUnicodeUseLatestMapping;

      length = CFStringGetLength (str);
      out_size = (int)((float)length * initial_mag) * sizeof (UniChar);
      if (out_size < 32)
	out_size = 32;

      in_text = (UniChar *)CFStringGetCharactersPtr (str);
      if (in_text == NULL)
	{
	  buffer = xmalloc (sizeof (UniChar) * length);
	  CFStringGetCharacters (str, CFRangeMake (0, length), buffer);
	  in_text = buffer;
	}

      if (in_text)
	err = CreateUnicodeToTextInfo (&map, &uni);
      while (err == noErr)
	{
	  out_buf = xmalloc (out_size);
	  err = ConvertFromUnicodeToText (uni, length * sizeof (UniChar),
					  in_text,
					  kUnicodeDefaultDirectionMask,
					  0, NULL, NULL, NULL,
					  out_size, &out_read, &out_len,
					  out_buf);
	  if (err == noErr && out_read < length * sizeof (UniChar))
	    {
	      xfree (out_buf);
	      out_size += length;
	    }
	  else
	    break;
	}
      if (err == noErr)
	result = CFStringCreateWithCharacters (NULL, out_buf,
					       out_len / sizeof (UniChar));
      if (uni)
	DisposeUnicodeToTextInfo (&uni);
      xfree (out_buf);
      xfree (buffer);
    }
  else
    {
      result = str;
      CFRetain (result);
    }

  return result;
}

DEFUN ("mac-code-convert-string", Fmac_code_convert_string, Smac_code_convert_string, 3, 4, 0,
       doc: /* Convert STRING from SOURCE encoding to TARGET encoding.
The conversion is performed using the converter provided by the system.
Each encoding is specified by either a coding system symbol, a mime
charset string, or an integer as a CFStringEncoding value.  An encoding
of nil means UTF-16 in native byte order, no byte order mark.
On Mac OS X 10.2 and later, you can do Unicode Normalization by
specifying the optional argument NORMALIZATION-FORM with a symbol NFD,
NFKD, NFC, NFKC, HFS+D, or HFS+C.
On successful conversion, return the result string, else return nil.  */)
  (Lisp_Object string, Lisp_Object source, Lisp_Object target,
   Lisp_Object normalization_form)
{
  Lisp_Object result = Qnil;
  struct gcpro gcpro1, gcpro2, gcpro3, gcpro4;
  CFStringEncoding src_encoding, tgt_encoding;
  CFStringRef str = NULL;

  CHECK_STRING (string);
  if (!INTEGERP (source) && !STRINGP (source))
    CHECK_SYMBOL (source);
  if (!INTEGERP (target) && !STRINGP (target))
    CHECK_SYMBOL (target);
  CHECK_SYMBOL (normalization_form);

  GCPRO4 (string, source, target, normalization_form);

  block_input ();

  src_encoding = get_cfstring_encoding_from_lisp (source);
  tgt_encoding = get_cfstring_encoding_from_lisp (target);

  /* We really want string_to_unibyte, but since it doesn't exist yet, we
     use string_as_unibyte which works as well, except for the fact that
     it's too permissive (it doesn't check that the multibyte string only
     contain single-byte chars).  */
  string = Fstring_as_unibyte (string);
  if (src_encoding != kCFStringEncodingInvalidId
      && tgt_encoding != kCFStringEncodingInvalidId)
    str = CFStringCreateWithBytes (NULL, SDATA (string), SBYTES (string),
				   src_encoding, !NILP (source));
  if (str)
    {
      CFStringRef saved_str = str;

      str = cfstring_create_normalized (saved_str, normalization_form);
      CFRelease (saved_str);
    }
  if (str)
    {
      CFIndex str_len, buf_len;

      str_len = CFStringGetLength (str);
      if (CFStringGetBytes (str, CFRangeMake (0, str_len), tgt_encoding, 0,
			    !NILP (target), NULL, 0, &buf_len) == str_len)
	{
	  result = make_uninit_string (buf_len);
	  CFStringGetBytes (str, CFRangeMake (0, str_len), tgt_encoding, 0,
			    !NILP (target), SDATA (result), buf_len, NULL);
	}
      CFRelease (str);
    }

  unblock_input ();

  UNGCPRO;

  return result;
}

static ScriptCode
mac_get_system_script_code (void)
{
  ScriptCode result;
  OSStatus err;

  err = RevertTextEncodingToScriptInfo (CFStringGetSystemEncoding (),
					&result, NULL, NULL);
  if (err != noErr)
    result = 0;

  return result;
}


/* Unlike in X11, window events in Carbon or Cocoa, whose event system
   is implemented on top of Carbon, do not come from sockets.  So we
   cannot simply use `select' to monitor two kinds of inputs: window
   events and process outputs.  We emulate such functionality by
   regarding fd 0 as the window event channel and simultaneously
   monitoring both kinds of input channels.  It is implemented by
   dividing into some cases:
   1. The window event channel is not involved.
      -> Use `select'.
   2. Sockets are not involved.
      -> Run the run loop in the main thread to wait for window events
         (and user signals, see below).
   3. Otherwise
      -> Run the run loop in the main thread while calling `select' in
         a secondary thread using either Pthreads with CFRunLoopSource
         or Grand Central Dispatch (GCD, on Mac OS X 10.6 or later).
         When the control returns from the `select' call, the
         secondary thread sends a wakeup notification to the main
         thread through either a CFSocket (for Pthreads with
         CFRunLoopSource) or a dispatch source (for GCD).
   For Case 2 and 3, user signals such as SIGUSR1 are also handled
   through either a CFSocket or a dispatch source.  */

static int wakeup_fds[2];
/* Whether we have read some input from wakeup_fds[0] after resetting
   this variable.  Don't access it outside the main thread.  */
static bool wokeup_from_run_loop_run_once_p;

static int
read_all_from_nonblocking_fd (int fd)
{
  int rtnval;
  char buf[64];

  do
    {
      rtnval = read (fd, buf, sizeof (buf));
    }
  while (rtnval > 0 || (rtnval < 0 && errno == EINTR));

  return rtnval;
}

static int
write_one_byte_to_fd (int fd)
{
  int rtnval;

  do
    {
      rtnval = write (fd, "", 1);
    }
  while (rtnval == 0 || (rtnval < 0 && errno == EINTR));

  return rtnval;
}

#if SELECT_USE_GCD
static dispatch_queue_t select_dispatch_queue;
#else
static void
wakeup_callback (CFSocketRef s, CFSocketCallBackType type, CFDataRef address,
		 const void *data, void *info)
{
  read_all_from_nonblocking_fd (CFSocketGetNative (s));
  wokeup_from_run_loop_run_once_p = true;
}
#endif

int
init_wakeup_fds (void)
{
  int result, i;

  result = socketpair (AF_UNIX, SOCK_STREAM, 0, wakeup_fds);
  if (result < 0)
    return result;
  for (i = 0; i < 2; i++)
    {
      int flags;

      if ((flags = fcntl (wakeup_fds[i], F_GETFL, 0)) < 0
	  || fcntl (wakeup_fds[i], F_SETFL, flags | O_NONBLOCK) == -1
	  || (flags = fcntl (wakeup_fds[i], F_GETFD, 0)) < 0
	  || fcntl (wakeup_fds[i], F_SETFD, flags | FD_CLOEXEC) == -1)
	return -1;
    }
#if SELECT_USE_GCD
  {
    dispatch_source_t source;

    source = dispatch_source_create (DISPATCH_SOURCE_TYPE_READ, wakeup_fds[0],
				     0, dispatch_get_main_queue ());
    if (source == NULL)
      return -1;
    dispatch_source_set_event_handler (source, ^{
	read_all_from_nonblocking_fd (dispatch_source_get_handle (source));
	wokeup_from_run_loop_run_once_p = true;
      });
    dispatch_resume (source);

    select_dispatch_queue = dispatch_queue_create ("org.gnu.Emacs.select",
						   NULL);
    if (select_dispatch_queue == NULL)
      return -1;
  }
#else
  {
    CFSocketRef socket;
    CFRunLoopSourceRef source;

    socket = CFSocketCreateWithNative (NULL, wakeup_fds[0],
				       kCFSocketReadCallBack,
				       wakeup_callback, NULL);
    if (socket == NULL)
      return -1;
    source = CFSocketCreateRunLoopSource (NULL, socket, 0);
    CFRelease (socket);
    if (source == NULL)
      return -1;
    CFRunLoopAddSource ((CFRunLoopRef)
			GetCFRunLoopFromEventLoop (GetCurrentEventLoop ()),
			source, kCFRunLoopDefaultMode);
    CFRelease (source);
  }
#endif
  return 0;
}

void
mac_wakeup_from_run_loop_run_once (void)
{
  /* This function may be called from a signal hander, so only
     async-signal safe functions can be used here.  */
  write_one_byte_to_fd (wakeup_fds[1]);
}

/* Return next event in the main queue if it exists.  Otherwise return
   NULL.  */

EventRef
mac_peek_next_event (void)
{
  EventRef event;

  event = AcquireFirstMatchingEventInQueue (GetCurrentEventQueue (), 0,
					    NULL, kEventQueueOptionsNone);
  if (event)
    ReleaseEvent (event);

  return event;
}

#if !SELECT_USE_GCD
static struct
{
  int value;
  pthread_mutex_t mutex;
  pthread_cond_t cond;
} select_sem = {0, PTHREAD_MUTEX_INITIALIZER, PTHREAD_COND_INITIALIZER};

static void
select_sem_wait (void)
{
  pthread_mutex_lock (&select_sem.mutex);
  while (select_sem.value <= 0)
    pthread_cond_wait (&select_sem.cond, &select_sem.mutex);
  select_sem.value--;
  pthread_mutex_unlock (&select_sem.mutex);
}

static void
select_sem_signal (void)
{
  pthread_mutex_lock (&select_sem.mutex);
  select_sem.value++;
  pthread_cond_signal (&select_sem.cond);
  pthread_mutex_unlock (&select_sem.mutex);
}

static CFRunLoopSourceRef select_run_loop_source = NULL;
static CFRunLoopRef select_run_loop = NULL;

static struct
{
  int nfds;
  fd_set *rfds, *wfds, *efds;
  struct timespec *timeout;
} select_args;

static void
select_perform (void *info)
{
  int qnfds = select_args.nfds;
  fd_set qrfds, qwfds, qefds;
  struct timespec qtimeout;
  int r;

  if (select_args.rfds)
    qrfds = *select_args.rfds;
  if (select_args.wfds)
    qwfds = *select_args.wfds;
  if (select_args.efds)
    qefds = *select_args.efds;
  if (select_args.timeout)
    qtimeout = *select_args.timeout;

  if (wakeup_fds[1] >= qnfds)
    qnfds = wakeup_fds[1] + 1;
  FD_SET (wakeup_fds[1], &qrfds);

  r = pselect (qnfds, select_args.rfds ? &qrfds : NULL,
	       select_args.wfds ? &qwfds : NULL,
	       select_args.efds ? &qefds : NULL,
	       select_args.timeout ? &qtimeout : NULL, NULL);
  if (r < 0 || (r > 0 && !FD_ISSET (wakeup_fds[1], &qrfds)))
    mac_wakeup_from_run_loop_run_once ();

  select_sem_signal ();
}

static void
select_fire (int nfds, fd_set *rfds, fd_set *wfds, fd_set *efds,
	     struct timespec *timeout)
{
  select_args.nfds = nfds;
  select_args.rfds = rfds;
  select_args.wfds = wfds;
  select_args.efds = efds;
  select_args.timeout = timeout;

  CFRunLoopSourceSignal (select_run_loop_source);
  CFRunLoopWakeUp (select_run_loop);
}

static void *
select_thread_main (void *arg)
{
  CFRunLoopSourceContext context = {0, NULL, NULL, NULL, NULL, NULL, NULL,
				    NULL, NULL, select_perform};

  select_run_loop = CFRunLoopGetCurrent ();
  select_run_loop_source = CFRunLoopSourceCreate (NULL, 0, &context);
  CFRunLoopAddSource (select_run_loop, select_run_loop_source,
		      kCFRunLoopDefaultMode);
  select_sem_signal ();
  CFRunLoopRun ();
}

static void
select_thread_launch (void)
{
  pthread_attr_t  attr;
  pthread_t       thread;

  pthread_attr_init (&attr);
  pthread_attr_setdetachstate (&attr, PTHREAD_CREATE_DETACHED);
  pthread_create (&thread, &attr, &select_thread_main, NULL);
}
#endif	/* !SELECT_USE_GCD */

static int
select_and_poll_event (int nfds, fd_set *rfds, fd_set *wfds,
		       fd_set *efds, struct timespec const *timeout)
{
  bool timedout_p = false;
  int r = 0;
  struct timespec select_timeout;
  EventTimeout timeoutval = (timeout ? timespectod (*timeout)
			     : kEventDurationForever);
  fd_set orfds, owfds, oefds;

  if (timeout == NULL)
    {
      if (rfds) orfds = *rfds;
      if (wfds) owfds = *wfds;
      if (efds) oefds = *efds;
    }

  /* Try detect_input_pending before mac_run_loop_run_once in the same
     BLOCK_INPUT block, in case that some input has already been read
     asynchronously.  */
  block_input ();
  while (1)
    {
      if (detect_input_pending ())
	break;

      select_timeout = make_timespec (0, 0);
      r = pselect (nfds, rfds, wfds, efds, &select_timeout, NULL);
      if (r != 0)
	break;

      if (timeoutval == 0.0)
	timedout_p = true;
      else
	{
	  /* On Mac OS X 10.7, delayed visible toolbar item validation
	     (see the documentation of -[NSToolbar
	     validateVisibleItems]) is treated as if it were an input
	     source firing rather than a timer function (as in Mac OS
	     X 10.6).  So it makes -[NSRunLoop runMode:beforeDate:],
	     which is used in the implementation of
	     mac_run_loop_run_once, return despite no available input
	     to process.  In such cases, we want to call
	     mac_run_loop_run_once again so as to avoid wasting CPU
	     time caused by vacuous reactivation of delayed visible
	     toolbar item validation via window update events issued
	     in the application event loop.  */
	  do
	    {
	      timeoutval = mac_run_loop_run_once (timeoutval);
	    }
	  while (timeoutval && !mac_peek_next_event ()
		 && !detect_input_pending ());
	  if (timeoutval == 0)
	    timedout_p = true;
	}

      if (timeout == NULL && timedout_p)
	{
	  if (rfds) *rfds = orfds;
	  if (wfds) *wfds = owfds;
	  if (efds) *efds = oefds;
	}
      else
	break;
    }
  unblock_input ();

  if (r != 0)
    return r;
  else if (!timedout_p)
    {
      /* Pretend that `select' is interrupted by a signal.  */
      detect_input_pending ();
      errno = EINTR;
      return -1;
    }
  else
    return 0;
}

int
mac_select (int nfds, fd_set *rfds, fd_set *wfds, fd_set *efds,
	    struct timespec const *timeout, sigset_t const *sigmask)
{
  bool timedout_p = false;
  int r;
  struct timespec select_timeout;
  fd_set orfds, owfds, oefds;
  EventTimeout timeoutval;

  if (inhibit_window_system || noninteractive
      || nfds < 1 || rfds == NULL || !FD_ISSET (0, rfds))
    return pselect (nfds, rfds, wfds, efds, timeout, sigmask);

  eassert (sigmask == NULL);

  FD_CLR (0, rfds);
  orfds = *rfds;

  if (wfds)
    owfds = *wfds;
  else
    FD_ZERO (&owfds);

  if (efds)
    oefds = *efds;
  else
    FD_ZERO (&oefds);

  timeoutval = (timeout ? timespectod (*timeout) : kEventDurationForever);

  FD_SET (0, rfds);		/* sentinel */
  do
    {
      nfds--;
    }
  while (!(FD_ISSET (nfds, rfds) || (wfds && FD_ISSET (nfds, wfds))
	   || (efds && FD_ISSET (nfds, efds))));
  nfds++;
  FD_CLR (0, rfds);

  if (nfds == 1)
    return select_and_poll_event (nfds, rfds, wfds, efds, timeout);

  /* Avoid initial overhead of RunLoop setup for the case that some
     input is already available.  */
  select_timeout = make_timespec (0, 0);
  r = select_and_poll_event (nfds, rfds, wfds, efds, &select_timeout);
  if (r != 0 || timeoutval == 0.0)
    return r;

  *rfds = orfds;
  if (wfds)
    *wfds = owfds;
  if (efds)
    *efds = oefds;

  /* Try detect_input_pending before mac_run_loop_run_once in the same
     BLOCK_INPUT block, in case that some input has already been read
     asynchronously.  */
  block_input ();
  if (!detect_input_pending ())
    {
#if SELECT_USE_GCD
      dispatch_sync (select_dispatch_queue, ^{});
      wokeup_from_run_loop_run_once_p = false;
      dispatch_async (select_dispatch_queue, ^{
	  fd_set qrfds = orfds, qwfds = owfds, qefds = oefds;
	  int qnfds = nfds;
	  int r;

	  if (wakeup_fds[1] >= qnfds)
	    qnfds = wakeup_fds[1] + 1;
	  FD_SET (wakeup_fds[1], &qrfds);

	  r = pselect (qnfds, &qrfds, wfds ? &qwfds : NULL,
		       efds ? &qefds : NULL, NULL, NULL);
	  if (r < 0 || (r > 0 && !FD_ISSET (wakeup_fds[1], &qrfds)))
	    mac_wakeup_from_run_loop_run_once ();
	});

      do
	{
	  timeoutval = mac_run_loop_run_once (timeoutval);
	}
      while (timeoutval && !wokeup_from_run_loop_run_once_p
	     && !mac_peek_next_event () && !detect_input_pending ());
      if (timeoutval == 0)
	timedout_p = true;

      write_one_byte_to_fd (wakeup_fds[0]);
      dispatch_async (select_dispatch_queue, ^{
	  read_all_from_nonblocking_fd (wakeup_fds[1]);
	});
#else
      if (select_run_loop == NULL)
	select_thread_launch ();

      select_sem_wait ();
      read_all_from_nonblocking_fd (wakeup_fds[1]);
      wokeup_from_run_loop_run_once_p = false;
      select_fire (nfds, rfds, wfds, efds, NULL);

      do
	{
	  timeoutval = mac_run_loop_run_once (timeoutval);
	}
      while (timeoutval && !wokeup_from_run_loop_run_once_p
	     && !mac_peek_next_event () && !detect_input_pending ());
      if (timeoutval == 0)
	timedout_p = true;

      write_one_byte_to_fd (wakeup_fds[0]);
#endif
    }
  unblock_input ();

  if (!timedout_p)
    {
      select_timeout = make_timespec (0, 0);
      r = select_and_poll_event (nfds, rfds, wfds, efds, &select_timeout);
      if (r != 0)
	return r;
      errno = EINTR;
      return -1;
    }
  else
    {
      FD_ZERO (rfds);
      if (wfds)
	FD_ZERO (wfds);
      if (efds)
	FD_ZERO (efds);
      return 0;
    }
}

/* Return whether the service provider for the current application is
   already registered.  */

bool
mac_service_provider_registered_p (void)
{
  name_t name = "org.gnu.Emacs";
  CFBundleRef bundle;
  mach_port_t port;
  kern_return_t kr;

  bundle = CFBundleGetMainBundle ();
  if (bundle)
    {
      CFStringRef identifier = CFBundleGetIdentifier (bundle);

      if (identifier)
	CFStringGetCString (identifier, name, sizeof (name),
			    kCFStringEncodingUTF8);
    }
  strlcat (name, ".ServiceProvider", sizeof (name));
  kr = bootstrap_look_up (bootstrap_port, name, &port);
  if (kr == KERN_SUCCESS)
    mach_port_deallocate (mach_task_self (), port);

  return kr == KERN_SUCCESS;
}

Lisp_Object
mac_carbon_version_string ()
{
  Lisp_Object result = Qnil;
  CFBundleRef bundle;
  CFTypeRef value;

  bundle = CFBundleGetBundleWithIdentifier (CFSTR ("com.apple.Carbon"));
  if (bundle == NULL)
    return result;

  value = CFBundleGetValueForInfoDictionaryKey (bundle, kCFBundleVersionKey);
  if (value && CFGetTypeID (value) == CFStringGetTypeID ())
    result = cfstring_to_lisp_nodecode (value);

#if MAC_OS_X_VERSION_MIN_REQUIRED < 1050
  if (NILP (result))
    {
      CFPropertyListRef plist = NULL;
      CFURLRef resource_url = CFBundleCopyResourceURL (bundle,
						       CFSTR ("version"),
						       CFSTR ("plist"), NULL);

      if (resource_url)
	{
	  plist = cfproperty_list_create_with_url (resource_url);
	  CFRelease (resource_url);
	}
      if (plist)
	{
	  if (CFGetTypeID (plist) == CFDictionaryGetTypeID ())
	    {
	      value = CFDictionaryGetValue (plist, CFSTR ("SourceVersion"));
	      if (value && CFGetTypeID (value) == CFStringGetTypeID ())
		{
		  result = cfstring_to_lisp_nodecode (value);
		  if (STRINGP (result)
		      && CFStringHasSuffix (value, CFSTR ("0000")))
		    result = Fsubstring (result, make_number (0),
					 make_number (-4));
		}
	    }
	  CFRelease (plist);
	}
    }
#endif

  return result;
}

struct mac_operating_system_version mac_operating_system_version;

static void
mac_initialize_operating_system_version ()
{
  const char *filename = "/System/Library/CoreServices/SystemVersion.plist";
  CFURLRef url;
  CFPropertyListRef plist = NULL;
  CFDataRef data = NULL;

  url = CFURLCreateFromFileSystemRepresentation (NULL, (const UInt8 *) filename,
						 strlen (filename), false);
  if (url)
    {
      plist = cfproperty_list_create_with_url (url);
      CFRelease (url);
    }
  if (plist)
    {
      if (CFGetTypeID (plist) == CFDictionaryGetTypeID ())
	{
	  CFStringRef value =
	    CFDictionaryGetValue (plist, CFSTR ("ProductVersion"));

	  if (value && CFGetTypeID (value) == CFStringGetTypeID ())
	    data = CFStringCreateExternalRepresentation (NULL, value,
							 kCFStringEncodingUTF8,
							 '\0');
	}
      CFRelease (plist);
    }
  if (data)
    {
      long major, minor, patch;
      int nmatches;

      nmatches = sscanf ((const char *) CFDataGetBytePtr (data),
			 "%ld.%ld.%ld", &major, &minor, &patch);
      if (nmatches == 3 || nmatches == 2)
	{
	  if (nmatches == 2)
	    patch = 0;
	  mac_operating_system_version.major = major;
	  mac_operating_system_version.minor = minor;
	  mac_operating_system_version.patch = patch;
	}

      CFRelease (data);
    }
}

const char *mac_exec_path, *mac_load_path, *mac_etc_directory;

/* Set up environment variables so that Emacs can correctly find its
   support files when packaged as an application bundle.  Directories
   placed in /usr/local/share/emacs/<emacs-version>/, /usr/local/bin,
   and /usr/local/libexec/emacs/<emacs-version>/<system-configuration>
   by `make install' by default can instead be placed in
   .../Emacs.app/Contents/Resources/ and
   .../Emacs.app/Contents/MacOS/.  Each of these environment variables
   is changed only if it is not already set.  Presumably if the user
   sets an environment variable, he will want to use files in his path
   instead of ones in the application bundle.  */
void
init_mac_osx_environment (void)
{
  CFBundleRef bundle;
  CFURLRef bundleURL;
  CFStringRef cf_app_bundle_pathname;
  int app_bundle_pathname_len;
  char *app_bundle_pathname;
  char *p, *q;
  struct stat st;

  /* Initialize the operating system version.  */
  mac_initialize_operating_system_version ();

  /* Initialize locale related variables.  */
  mac_system_script_code = mac_get_system_script_code ();

  /* Fetch the pathname of the application bundle as a C string into
     app_bundle_pathname.  */

  bundle = CFBundleGetMainBundle ();
  if (!bundle || CFBundleGetIdentifier (bundle) == NULL)
    {
      /* We could not find the bundle identifier.  For now, prevent
	 the fatal error by bringing it up in the terminal. */
      inhibit_window_system = 1;
      return;
    }

  bundleURL = CFBundleCopyBundleURL (bundle);
  if (!bundleURL)
    return;

  cf_app_bundle_pathname = CFURLCopyFileSystemPath (bundleURL,
						    kCFURLPOSIXPathStyle);
  CFRelease (bundleURL);
  {
    Lisp_Object temp = cfstring_to_lisp_nodecode (cf_app_bundle_pathname);

    app_bundle_pathname_len = SBYTES (temp);
    app_bundle_pathname = SSDATA (temp);
  }

  CFRelease (cf_app_bundle_pathname);

  /* P should have sufficient room for the pathname of the bundle plus
     the subpath in it leading to the respective directories.  Q
     should have three times that much room because EMACSLOADPATH can
     have the value "<path to site-lisp dir>:<path to lisp dir>:<path
     to leim dir>".  */
  p = (char *) alloca (app_bundle_pathname_len + 50);
  q = (char *) alloca (3 * app_bundle_pathname_len + 150);

  /* Set `mac_load_path'.  */
  q[0] = '\0';

  strcpy (p, app_bundle_pathname);
  strcat (p, "/Contents/Resources/site-lisp");
  if (stat (p, &st) == 0 && (st.st_mode & S_IFMT) == S_IFDIR)
    strcat (q, p);

  strcpy (p, app_bundle_pathname);
  strcat (p, "/Contents/Resources/lisp");
  if (stat (p, &st) == 0 && (st.st_mode & S_IFMT) == S_IFDIR)
    {
      if (q[0] != '\0')
	strcat (q, ":");
      strcat (q, p);
    }

  strcpy (p, app_bundle_pathname);
  strcat (p, "/Contents/Resources/leim");
  if (stat (p, &st) == 0 && (st.st_mode & S_IFMT) == S_IFDIR)
    {
      if (q[0] != '\0')
	strcat (q, ":");
      strcat (q, p);
    }

  if (q[0] != '\0')
    mac_load_path = strdup (q);

  /* Set `mac_exec_path'.  */
  q[0] = '\0';

  strcpy (p, app_bundle_pathname);
  strcat (p, "/Contents/MacOS/libexec");
  if (stat (p, &st) == 0 && (st.st_mode & S_IFMT) == S_IFDIR)
    strcat (q, p);

  strcpy (p, app_bundle_pathname);
  strcat (p, "/Contents/MacOS/bin");
  if (stat (p, &st) == 0 && (st.st_mode & S_IFMT) == S_IFDIR)
    {
      if (q[0] != '\0')
	strcat (q, ":");
      strcat (q, p);
    }

  if (q[0] != '\0')
    mac_exec_path = strdup (q);

  /* Set `mac_etc_directory'.  */
  strcpy (p, app_bundle_pathname);
  strcat (p, "/Contents/Resources/etc");
  if (stat (p, &st) == 0 && (st.st_mode & S_IFMT) == S_IFDIR)
    mac_etc_directory = strdup (p);

  if (IS_DAEMON)
    inhibit_window_system = 1;
  else
    {
      CFDictionaryRef session_dict = CGSessionCopyCurrentDictionary ();

      if (session_dict == NULL)
	/* No window server session.  */
	inhibit_window_system = 1;
      else
	CFRelease (session_dict);
    }
}


void
syms_of_mac (void)
{
  DEFSYM (Qundecoded_file_name, "undecoded-file-name");

  DEFSYM (Qstring, "string");
  DEFSYM (Qnumber, "number");
  DEFSYM (Qboolean, "boolean");
  DEFSYM (Qdate, "date");
  DEFSYM (Qarray, "array");
  DEFSYM (Qdictionary, "dictionary");
  DEFSYM (Qrange, "range");
  DEFSYM (Qpoint, "point");
  DEFSYM (Qdescription, "description");

  DEFSYM (Qmac_file_alias_p, "mac-file-alias-p");

  DEFSYM (Qxml, "xml");
  DEFSYM (Qxml1, "xml1");
  DEFSYM (Qbinary1, "binary1");

  DEFSYM (QCmime_charset, ":mime-charset");

  DEFSYM (QNFD, "NFD");
  DEFSYM (QNFKD, "NFKD");
  DEFSYM (QNFC, "NFC");
  DEFSYM (QNFKC, "NFKC");
  DEFSYM (QHFS_plus_D, "HFS+D");
  DEFSYM (QHFS_plus_C, "HFS+C");

  {
    int i;

    for (i = 0; i < sizeof (ae_attr_table) / sizeof (ae_attr_table[0]); i++)
      DEFSYM (ae_attr_table[i].symbol, ae_attr_table[i].name);
  }

  defsubr (&Smac_coerce_ae_data);
  defsubr (&Smac_get_preference);
  defsubr (&Smac_convert_property_list);
  defsubr (&Smac_code_convert_string);

  defsubr (&Smac_file_alias_p);
  defsubr (&Ssystem_move_file_to_trash);
  defsubr (&Sdo_applescript);

  DEFVAR_INT ("mac-system-script-code", mac_system_script_code,
    doc: /* The system script code.  */);
  mac_system_script_code = mac_get_system_script_code ();

  DEFVAR_BOOL ("mac-system-move-file-to-trash-use-finder",
	       mac_system_move_file_to_trash_use_finder,
     doc: /* *Non-nil means that `system-move-file-to-trash' uses the Finder.
Setting this variable non-nil enables us to use the `Put Back' context
menu for trashed items, but it also affects the `Edit' - `Undo' menu
in the Finder.  On Mac OS X 10.4 and earlier, this variable has no
effect and trashing is always done via the Finder.  */);
  mac_system_move_file_to_trash_use_finder = 0;
}
