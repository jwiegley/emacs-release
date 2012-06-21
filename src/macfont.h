/* Interface definition for Mac font backend.
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

#if MAC_OS_X_VERSION_MAX_ALLOWED >= 1050
#define USE_CORE_TEXT 1
#endif

#if MAC_OS_X_VERSION_MIN_REQUIRED < 1050

#if MAC_OS_X_VERSION_MAX_ALLOWED >= 1040
#define USE_NS_FONT_DESCRIPTOR 1
#endif

#if MAC_OS_X_VERSION_MAX_ALLOWED < 1040 || (MAC_OS_X_VERSION_MIN_REQUIRED < 1040 && MAC_OS_X_VERSION_MIN_REQUIRED >= 1020)
#define USE_NS_FONT_MANAGER 1
#endif

#endif

/* Symbolic types of this font-driver.  */
extern Lisp_Object macfont_driver_type;

#if USE_CORE_TEXT
/* Core Text, for Mac OS X 10.5 and later.  */
extern Lisp_Object Qmac_ct;
#endif
#if USE_NS_FONT_DESCRIPTOR
/* Core Text emulation by NSFontDescriptor, for Mac OS X 10.4. */
extern Lisp_Object Qmac_fd;
#endif
#if USE_NS_FONT_MANAGER
/* Core Text emulation by NSFontManager, for Mac OS X 10.2 - 10.3. */
extern Lisp_Object Qmac_fm;
#endif

/* Structure used by Mac `shape' functions for storing layout
   information for each glyph.  */
struct mac_glyph_layout
{
  /* Range of indices of the characters composed into the group of
     glyphs that share the cursor position with this glyph.  The
     members `location' and `length' are in UTF-16 indices.  */
  CFRange comp_range;

  /* UTF-16 index in the source string for the first character
     associated with this glyph.  */
  CFIndex string_index;

  /* Horizontal and vertical adjustments of glyph position.  The
     coordinate space is that of Core Text.  So, the `baseline_delta'
     value is negative if the glyph should be placed below the
     baseline.  */
  CGFloat advance_delta, baseline_delta;

  /* Typographical width of the glyph.  */
  CGFloat advance;

  /* Glyph ID of the glyph.  */
  CGGlyph glyph_id;
};

#if MAC_OS_X_VERSION_MIN_REQUIRED >= 1050

typedef CTFontDescriptorRef FontDescriptorRef;
typedef CTFontRef FontRef;
typedef CTFontSymbolicTraits FontSymbolicTraits;
typedef CTCharacterCollection CharacterCollection;

#define MAC_FONT_NAME_ATTRIBUTE kCTFontNameAttribute
#define MAC_FONT_FAMILY_NAME_ATTRIBUTE kCTFontFamilyNameAttribute
#define MAC_FONT_TRAITS_ATTRIBUTE kCTFontTraitsAttribute
#define MAC_FONT_SIZE_ATTRIBUTE kCTFontSizeAttribute
#define MAC_FONT_CASCADE_LIST_ATTRIBUTE kCTFontCascadeListAttribute
#define MAC_FONT_CHARACTER_SET_ATTRIBUTE kCTFontCharacterSetAttribute
#define MAC_FONT_LANGUAGES_ATTRIBUTE kCTFontLanguagesAttribute
#if MAC_OS_X_VERSION_MIN_REQUIRED >= 1060
#define MAC_FONT_FORMAT_ATTRIBUTE kCTFontFormatAttribute
#else
#define MAC_FONT_FORMAT_ATTRIBUTE (CFSTR ("NSCTFontFormatAttribute"))
#endif
#define MAC_FONT_SYMBOLIC_TRAIT kCTFontSymbolicTrait
#define MAC_FONT_WEIGHT_TRAIT kCTFontWeightTrait
#define MAC_FONT_WIDTH_TRAIT kCTFontWidthTrait
#define MAC_FONT_SLANT_TRAIT kCTFontSlantTrait

enum {
  MAC_FONT_ITALIC_TRAIT = kCTFontItalicTrait,
  MAC_FONT_BOLD_TRAIT = kCTFontBoldTrait,
  MAC_FONT_MONO_SPACE_TRAIT = kCTFontMonoSpaceTrait
};

enum {
#if MAC_OS_X_VERSION_MAX_ALLOWED >= 1060
  MAC_FONT_FORMAT_BITMAP = kCTFontFormatBitmap
#else
  MAC_FONT_FORMAT_BITMAP = 5
#endif
};

enum {
  MAC_IDENTITY_MAPPING_CHARACTER_COLLECTION = kCTIdentityMappingCharacterCollection,
  MAC_ADOBE_CNS1_CHARACTER_COLLECTION = kCTAdobeCNS1CharacterCollection,
  MAC_ADOBE_GB1_CHARACTER_COLLECTION = kCTAdobeGB1CharacterCollection,
  MAC_ADOBE_JAPAN1_CHARACTER_COLLECTION = kCTAdobeJapan1CharacterCollection,
  MAC_ADOBE_JAPAN2_CHARACTER_COLLECTION = kCTAdobeJapan2CharacterCollection,
  MAC_ADOBE_KOREA1_CHARACTER_COLLECTION = kCTAdobeKorea1CharacterCollection
};

#define mac_font_descriptor_create_with_attributes \
  CTFontDescriptorCreateWithAttributes
#define mac_font_descriptor_create_matching_font_descriptors \
  CTFontDescriptorCreateMatchingFontDescriptors
#define mac_font_descriptor_create_matching_font_descriptor \
  CTFontDescriptorCreateMatchingFontDescriptor
#define mac_font_descriptor_copy_attribute CTFontDescriptorCopyAttribute
#define mac_font_descriptor_supports_languages \
  mac_ctfont_descriptor_supports_languages
#define mac_font_create_with_name(name, size) \
  CTFontCreateWithName (name, size, NULL)
#define mac_font_get_size CTFontGetSize
#define mac_font_copy_family_name CTFontCopyFamilyName
#define mac_font_copy_character_set CTFontCopyCharacterSet
#define mac_font_get_glyphs_for_characters CTFontGetGlyphsForCharacters
#define mac_font_get_ascent CTFontGetAscent
#define mac_font_get_descent CTFontGetDescent
#define mac_font_get_leading CTFontGetLeading
#define mac_font_get_underline_position CTFontGetUnderlinePosition
#define mac_font_get_underline_thickness CTFontGetUnderlineThickness
#define mac_font_copy_graphics_font(font) CTFontCopyGraphicsFont (font, NULL)
#define mac_font_copy_non_synthetic_table(font, table) \
  CTFontCopyTable (font, table, kCTFontTableOptionExcludeSynthetic)

#define mac_font_create_preferred_family_for_attributes \
  mac_ctfont_create_preferred_family_for_attributes
#define mac_font_get_advance_width_for_glyph \
  mac_ctfont_get_advance_width_for_glyph
#define mac_font_get_bounding_rect_for_glyph \
  mac_ctfont_get_bounding_rect_for_glyph
#define mac_font_create_available_families mac_ctfont_create_available_families
#define mac_font_shape mac_ctfont_shape
#if USE_CT_GLYPH_INFO
#define mac_font_get_glyph_for_cid mac_ctfont_get_glyph_for_cid
#else
extern CGGlyph mac_font_get_glyph_for_cid (FontRef, CharacterCollection,
					   CGFontIndex);
#endif

#define mac_nsctfont_copy_font_descriptor CTFontCopyFontDescriptor

#else  /* MAC_OS_X_VERSION_MIN_REQUIRED < 1050 */

typedef const struct _EmacsFont *FontRef;		      /* opaque */
typedef const struct _EmacsFontDescriptor *FontDescriptorRef; /* opaque */
typedef uint32_t FontSymbolicTraits;
typedef uint16_t CharacterCollection;

extern const CFStringRef MAC_FONT_NAME_ATTRIBUTE;
extern const CFStringRef MAC_FONT_FAMILY_NAME_ATTRIBUTE;
extern const CFStringRef MAC_FONT_TRAITS_ATTRIBUTE;
extern const CFStringRef MAC_FONT_SIZE_ATTRIBUTE;
extern const CFStringRef MAC_FONT_CASCADE_LIST_ATTRIBUTE;
extern const CFStringRef MAC_FONT_CHARACTER_SET_ATTRIBUTE;
extern const CFStringRef MAC_FONT_LANGUAGES_ATTRIBUTE;
extern const CFStringRef MAC_FONT_FORMAT_ATTRIBUTE;
extern const CFStringRef MAC_FONT_SYMBOLIC_TRAIT;
extern const CFStringRef MAC_FONT_WEIGHT_TRAIT;
extern const CFStringRef MAC_FONT_WIDTH_TRAIT;
extern const CFStringRef MAC_FONT_SLANT_TRAIT;

#define kCFNumberCGFloatType kCFNumberFloatType

enum {
  MAC_FONT_ITALIC_TRAIT = (1 << 0),
  MAC_FONT_BOLD_TRAIT = (1 << 1),
  MAC_FONT_MONO_SPACE_TRAIT = (1 << 10)
};

enum {
  MAC_FONT_FORMAT_BITMAP = 5
};

enum {
  MAC_IDENTITY_MAPPING_CHARACTER_COLLECTION = 0,
  MAC_ADOBE_CNS1_CHARACTER_COLLECTION = 1,
  MAC_ADOBE_GB1_CHARACTER_COLLECTION = 2,
  MAC_ADOBE_JAPAN1_CHARACTER_COLLECTION = 3,
  MAC_ADOBE_JAPAN2_CHARACTER_COLLECTION = 4,
  MAC_ADOBE_KOREA1_CHARACTER_COLLECTION = 5
};

extern FontDescriptorRef mac_font_descriptor_create_with_attributes (CFDictionaryRef);
extern CFArrayRef mac_font_descriptor_create_matching_font_descriptors (FontDescriptorRef,
									CFSetRef);
extern FontDescriptorRef mac_font_descriptor_create_matching_font_descriptor (FontDescriptorRef,
									      CFSetRef);
extern CFTypeRef mac_font_descriptor_copy_attribute (FontDescriptorRef,
						     CFStringRef);
extern Boolean mac_font_descriptor_supports_languages (FontDescriptorRef,
						       CFArrayRef);
extern FontRef mac_font_create_with_name (CFStringRef, CGFloat);
extern CGFloat mac_font_get_size (FontRef);
extern CFStringRef mac_font_copy_family_name (FontRef);
extern CFCharacterSetRef mac_font_copy_character_set (FontRef);
extern Boolean mac_font_get_glyphs_for_characters (FontRef,
						   const UniChar *,
						   CGGlyph *, CFIndex);
extern CGFloat mac_font_get_ascent (FontRef);
extern CGFloat mac_font_get_descent (FontRef);
extern CGFloat mac_font_get_leading (FontRef);
extern CGFloat mac_font_get_underline_position (FontRef);
extern CGFloat mac_font_get_underline_thickness (FontRef);
extern CGFontRef mac_font_copy_graphics_font (FontRef);
extern CFDataRef mac_font_copy_non_synthetic_table (FontRef, FourCharCode);
extern ATSUTextLayout mac_font_get_text_layout_for_text_ptr (FontRef,
							     ConstUniCharArrayPtr,
							     UniCharCount);

extern CFStringRef mac_font_create_preferred_family_for_attributes (CFDictionaryRef);
extern CGFloat mac_font_get_advance_width_for_glyph (FontRef, CGGlyph);
extern CGRect mac_font_get_bounding_rect_for_glyph (FontRef, CGGlyph);
extern CFArrayRef mac_font_create_available_families (void);
extern CFIndex mac_font_shape (FontRef, CFStringRef,
			       struct mac_glyph_layout *, CFIndex);
extern CGGlyph mac_font_get_glyph_for_cid (FontRef, CharacterCollection,
					   CGFontIndex);

extern void *mac_font_get_nsctfont (FontRef);
extern FontDescriptorRef mac_nsctfont_copy_font_descriptor (void *);

#endif	/* MAC_OS_X_VERSION_MIN_REQUIRED < 1050 */

#define MAC_FONT_CHARACTER_SET_STRING_ATTRIBUTE \
  (CFSTR ("MAC_FONT_CHARACTER_SET_STRING_ATTRIBUTE"))

typedef const struct _EmacsScreenFont *ScreenFontRef; /* opaque */

extern CFComparisonResult mac_font_family_compare (const void *,
						   const void *, void *);
extern ScreenFontRef mac_screen_font_create_with_name (CFStringRef,
						       CGFloat);
extern CGFloat mac_screen_font_get_advance_width_for_glyph (ScreenFontRef,
							    CGGlyph);
Boolean mac_screen_font_get_metrics (ScreenFontRef, CGFloat *,
				     CGFloat *, CGFloat *);
CFIndex mac_screen_font_shape (ScreenFontRef, CFStringRef,
			       struct mac_glyph_layout *, CFIndex);
#if USE_CORE_TEXT
extern Boolean mac_ctfont_descriptor_supports_languages (CTFontDescriptorRef,
							 CFArrayRef);
extern CFStringRef mac_ctfont_create_preferred_family_for_attributes (CFDictionaryRef);
extern CFIndex mac_ctfont_shape (CTFontRef, CFStringRef,
				 struct mac_glyph_layout *, CFIndex);
#if USE_CT_GLYPH_INFO
extern CGGlyph mac_ctfont_get_glyph_for_cid (CTFontRef,
					     CTCharacterCollection,
					     CGFontIndex);
#endif
#endif
