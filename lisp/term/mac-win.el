;;; mac-win.el --- parse switches controlling interface with Mac window system -*-coding: utf-8-*-

;; Copyright (C) 1999-2008  Free Software Foundation, Inc.
;; Copyright (C) 2009-2012  YAMAMOTO Mitsuharu

;; Author: Andrew Choi <akochoi@mac.com>
;;	YAMAMOTO Mitsuharu <mituharu@math.s.chiba-u.ac.jp>
;; Keywords: terminals

;; This file is part of GNU Emacs Mac port.

;; GNU Emacs Mac port is free software: you can redistribute it and/or modify
;; it under the terms of the GNU General Public License as published by
;; the Free Software Foundation, either version 3 of the License, or
;; (at your option) any later version.

;; GNU Emacs Mac port is distributed in the hope that it will be useful,
;; but WITHOUT ANY WARRANTY; without even the implied warranty of
;; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
;; GNU General Public License for more details.

;; You should have received a copy of the GNU General Public License
;; along with GNU Emacs Mac port.  If not, see <http://www.gnu.org/licenses/>.

;;; Commentary:

;; Mac-win.el:  this file is loaded from ../lisp/startup.el when it recognizes
;; that Mac windows are to be used.  Command line switches are parsed and those
;; pertaining to Mac are processed and removed from the command line.  The
;; Mac display is opened and hooks are set for popping up the initial window.

;; startup.el will then examine startup files, and eventually call the hooks
;; which create the first window(s).

;;; Code:

;; These are the standard X switches from the Xt Initialize.c file of
;; Release 4.

;; Command line		Resource Manager string

;; +rv			*reverseVideo
;; +synchronous		*synchronous
;; -background		*background
;; -bd			*borderColor
;; -bg			*background
;; -bordercolor		*borderColor
;; -borderwidth		.borderWidth
;; -bw			.borderWidth
;; -display		.display
;; -fg			*foreground
;; -fn			*font
;; -font		*font
;; -foreground		*foreground
;; -geometry		.geometry
;; -iconic		.iconic
;; -name		.name
;; -reverse		*reverseVideo
;; -rv			*reverseVideo
;; -selectionTimeout    .selectionTimeout
;; -synchronous		*synchronous
;; -xrm

;; An alist of X options and the function which handles them.  See
;; ../startup.el.

;; (if (not (eq window-system 'mac))
;;     (error "%s: Loading mac-win.el but not compiled for Mac" (invocation-name)))

(require 'frame)
(require 'mouse)
(require 'scroll-bar)
(require 'faces)
(require 'select)
(require 'menu-bar)
(require 'fontset)
(require 'dnd)

(defvar mac-service-selection)
(defvar mac-system-script-code)
(defvar mac-apple-event-map)
(defvar mac-ts-active-input-overlay)


;;
;; Standard Mac cursor shapes
;;

(defconst mac-pointer-arrow 0)
(defconst mac-pointer-copy-arrow 1)
(defconst mac-pointer-alias-arrow 2)
(defconst mac-pointer-contextual-menu-arrow 3)
(defconst mac-pointer-I-beam 4)
(defconst mac-pointer-cross 5)
(defconst mac-pointer-plus 6)
(defconst mac-pointer-watch 7)
(defconst mac-pointer-closed-hand 8)
(defconst mac-pointer-open-hand 9)
(defconst mac-pointer-pointing-hand 10)
(defconst mac-pointer-counting-up-hand 11)
(defconst mac-pointer-counting-down-hand 12)
(defconst mac-pointer-counting-up-and-down-hand 13)
(defconst mac-pointer-spinning 14)
(defconst mac-pointer-resize-left 15)
(defconst mac-pointer-resize-right 16)
(defconst mac-pointer-resize-left-right 17)
;; Mac OS X 10.2 and later
(defconst mac-pointer-not-allowed 18)
;; Mac OS X 10.3 and later
(defconst mac-pointer-resize-up 19)
(defconst mac-pointer-resize-down 20)
(defconst mac-pointer-resize-up-down 21)
(defconst mac-pointer-poof 22)

;;
;; Standard X cursor shapes that have Mac counterparts
;;

(defconst x-pointer-left-ptr mac-pointer-arrow)
(defconst x-pointer-xterm mac-pointer-I-beam)
(defconst x-pointer-crosshair mac-pointer-cross)
(defconst x-pointer-plus mac-pointer-plus)
(defconst x-pointer-watch mac-pointer-watch)
(defconst x-pointer-hand2 mac-pointer-pointing-hand)
(defconst x-pointer-left-side mac-pointer-resize-left)
(defconst x-pointer-right-side mac-pointer-resize-right)
(defconst x-pointer-sb-h-double-arrow mac-pointer-resize-left-right)
(defconst x-pointer-top-side mac-pointer-resize-up)
(defconst x-pointer-bottom-side mac-pointer-resize-down)
(defconst x-pointer-sb-v-double-arrow mac-pointer-resize-up-down)


;;;; Modifier keys

;; Modifier name `ctrl' is an alias of `control'.
(put 'ctrl 'modifier-value (get 'control 'modifier-value))


;;;; Script codes and coding systems
(defconst mac-script-code-coding-systems
  '((0 . mac-roman)			; smRoman
    (1 . japanese-shift-jis)		; smJapanese
    (2 . chinese-big5)			; smTradChinese
    (3 . korean-iso-8bit)		; smKorean
    (7 . mac-cyrillic)			; smCyrillic
    (25 . chinese-iso-8bit)		; smSimpChinese
    (29 . mac-centraleurroman)		; smCentralEuroRoman
    )
  "Alist of Mac script codes vs Emacs coding systems.")

(define-charset 'mac-centraleurroman
  "Mac Central European Roman"
  :short-name "Mac CE"
  :ascii-compatible-p t
  :code-space [0 255]
  :map
  (let ((tbl
	 [?\Ä ?\Ā ?\ā ?\É ?\Ą ?\Ö ?\Ü ?\á ?\ą ?\Č ?\ä ?\č ?\Ć ?\ć ?\é ?\Ź
	  ?\ź ?\Ď ?\í ?\ď ?\Ē ?\ē ?\Ė ?\ó ?\ė ?\ô ?\ö ?\õ ?\ú ?\Ě ?\ě ?\ü
	  ?\† ?\° ?\Ę ?\£ ?\§ ?\• ?\¶ ?\ß ?\® ?\© ?\™ ?\ę ?\¨ ?\≠ ?\ģ ?\Į
	  ?\į ?\Ī ?\≤ ?\≥ ?\ī ?\Ķ ?\∂ ?\∑ ?\ł ?\Ļ ?\ļ ?\Ľ ?\ľ ?\Ĺ ?\ĺ ?\Ņ
	  ?\ņ ?\Ń ?\¬ ?\√ ?\ń ?\Ň ?\∆ ?\« ?\» ?\… ?\  ?\ň ?\Ő ?\Õ ?\ő ?\Ō
	  ?\– ?\— ?\“ ?\” ?\‘ ?\’ ?\÷ ?\◊ ?\ō ?\Ŕ ?\ŕ ?\Ř ?\‹ ?\› ?\ř ?\Ŗ
	  ?\ŗ ?\Š ?\‚ ?\„ ?\š ?\Ś ?\ś ?\Á ?\Ť ?\ť ?\Í ?\Ž ?\ž ?\Ū ?\Ó ?\Ô
	  ?\ū ?\Ů ?\Ú ?\ů ?\Ű ?\ű ?\Ų ?\ų ?\Ý ?\ý ?\ķ ?\Ż ?\Ł ?\ż ?\Ģ ?\ˇ])
	(map (make-vector 512 nil)))
    (or (= (length tbl) 128)
	(error "Invalid vector length: %d" (length tbl)))
    (dotimes (i 128)
      (aset map (* i 2) i)
      (aset map (1+ (* i 2)) i))
    (dotimes (i 128)
      (aset map (+ 256 (* i 2)) (+ 128 i))
      (aset map (+ 256 (1+ (* i 2))) (aref tbl i)))
    map))

(define-coding-system 'mac-centraleurroman
  "Mac Central European Roman Encoding (MIME:x-mac-centraleurroman)."
  :coding-type 'charset
  :mnemonic ?*
  :charset-list '(mac-centraleurroman)
  :mime-charset 'x-mac-centraleurroman)

(define-charset 'mac-cyrillic
  "Mac Cyrillic"
  :short-name "Mac CYRILLIC"
  :ascii-compatible-p t
  :code-space [0 255]
  :map
  (let ((tbl
	 [?\А ?\Б ?\В ?\Г ?\Д ?\Е ?\Ж ?\З ?\И ?\Й ?\К ?\Л ?\М ?\Н ?\О ?\П
	  ?\Р ?\С ?\Т ?\У ?\Ф ?\Х ?\Ц ?\Ч ?\Ш ?\Щ ?\Ъ ?\Ы ?\Ь ?\Э ?\Ю ?\Я
	  ?\† ?\° ?\Ґ ?\£ ?\§ ?\• ?\¶ ?\І ?\® ?\© ?\™ ?\Ђ ?\ђ ?\≠ ?\Ѓ ?\ѓ
	  ?\∞ ?\± ?\≤ ?\≥ ?\і ?\µ ?\ґ ?\Ј ?\Є ?\є ?\Ї ?\ї ?\Љ ?\љ ?\Њ ?\њ
	  ?\ј ?\Ѕ ?\¬ ?\√ ?\ƒ ?\≈ ?\∆ ?\« ?\» ?\… ?\  ?\Ћ ?\ћ ?\Ќ ?\ќ ?\ѕ
	  ?\– ?\— ?\“ ?\” ?\‘ ?\’ ?\÷ ?\„ ?\Ў ?\ў ?\Џ ?\џ ?\№ ?\Ё ?\ё ?\я
	  ?\а ?\б ?\в ?\г ?\д ?\е ?\ж ?\з ?\и ?\й ?\к ?\л ?\м ?\н ?\о ?\п
	  ?\р ?\с ?\т ?\у ?\ф ?\х ?\ц ?\ч ?\ш ?\щ ?\ъ ?\ы ?\ь ?\э ?\ю ?\€])
	(map (make-vector 512 nil)))
    (or (= (length tbl) 128)
	(error "Invalid vector length: %d" (length tbl)))
    (dotimes (i 128)
      (aset map (* i 2) i)
      (aset map (1+ (* i 2)) i))
    (dotimes (i 128)
      (aset map (+ 256 (* i 2)) (+ 128 i))
      (aset map (+ 256 (1+ (* i 2))) (aref tbl i)))
    map))

(define-coding-system 'mac-cyrillic
  "Mac Cyrillic Encoding (MIME:x-mac-cyrillic)."
  :coding-type 'charset
  :mnemonic ?*
  :charset-list '(mac-cyrillic)
  :mime-charset 'x-mac-cyrillic)

(define-charset 'mac-symbol
  "Mac Symbol"
  :short-name "Mac SYMBOL"
  :code-space [32 254]
  :map
  (let ((tbl-32-126
	 [?\  ?\! ?\∀ ?\# ?\∃ ?\% ?\& ?\∍ ?\( ?\) ?\∗ ?\+ ?\, ?\− ?\. ?\/
	  ?0 ?1 ?2 ?3 ?4 ?5 ?6 ?7 ?8 ?9 ?\: ?\; ?\< ?\= ?\> ?\?
	  ?\≅ ?\Α ?\Β ?\Χ ?\Δ ?\Ε ?\Φ ?\Γ ?\Η ?\Ι ?\ϑ ?\Κ ?\Λ ?\Μ ?\Ν ?\Ο
	  ?\Π ?\Θ ?\Ρ ?\Σ ?\Τ ?\Υ ?\ς ?\Ω ?\Ξ ?\Ψ ?\Ζ ?\[ ?\∴ ?\] ?\⊥ ?\_
	  ?\ ?\α ?\β ?\χ ?\δ ?\ε ?\φ ?\γ ?\η ?\ι ?\ϕ ?\κ ?\λ ?\μ ?\ν ?\ο
	  ?\π ?\θ ?\ρ ?\σ ?\τ ?\υ ?\ϖ ?\ω ?\ξ ?\ψ ?\ζ ?\{ ?\| ?\} ?\∼])
	(map-32-126 (make-vector (* (1+ (- 126 32)) 2) nil))
	(tbl-160-254
	 ;; Mapping of the following characters are changed from the
	 ;; original one:
	 ;; 0xE2 0x00AE+0xF87F->0x00AE # REGISTERED SIGN, alternate: sans serif
	 ;; 0xE3 0x00A9+0xF87F->0x00A9 # COPYRIGHT SIGN, alternate: sans serif
	 ;; 0xE4 0x2122+0xF87F->0x2122 # TRADE MARK SIGN, alternate: sans serif
	 [?\€ ?\ϒ ?\′ ?\≤ ?\⁄ ?\∞ ?\ƒ ?\♣ ?\♦ ?\♥ ?\♠ ?\↔ ?\← ?\↑ ?\→ ?\↓
	  ?\° ?\± ?\″ ?\≥ ?\× ?\∝ ?\∂ ?\• ?\÷ ?\≠ ?\≡ ?\≈ ?\… ?\⏐ ?\⎯ ?\↵
	  ?\ℵ ?\ℑ ?\ℜ ?\℘ ?\⊗ ?\⊕ ?\∅ ?\∩ ?\∪ ?\⊃ ?\⊇ ?\⊄ ?\⊂ ?\⊆ ?\∈ ?\∉
	  ?\∠ ?\∇ ?\® ?\© ?\™ ?\∏ ?\√ ?\⋅ ?\¬ ?\∧ ?\∨ ?\⇔ ?\⇐ ?\⇑ ?\⇒ ?\⇓
	  ?\◊ ?\〈 ?\® ?\© ?\™ ?\∑ ?\⎛ ?\⎜ ?\⎝ ?\⎡ ?\⎢ ?\⎣ ?\⎧ ?\⎨ ?\⎩ ?\⎪
	  ?\ ?\〉 ?\∫ ?\⌠ ?\⎮ ?\⌡ ?\⎞ ?\⎟ ?\⎠ ?\⎤ ?\⎥ ?\⎦ ?\⎫ ?\⎬ ?\⎭])
	(map-160-254 (make-vector (* (1+ (- 254 160)) 2) nil)))
    (dotimes (i (1+ (- 126 32)))
      (aset map-32-126 (* i 2) (+ 32 i))
      (aset map-32-126 (1+ (* i 2)) (aref tbl-32-126 i)))
    (dotimes (i (1+ (- 254 160)))
      (aset map-160-254 (* i 2) (+ 160 i))
      (aset map-160-254 (1+ (* i 2)) (aref tbl-160-254 i)))
    (vconcat map-32-126 map-160-254)))

(define-charset 'mac-dingbats
  "Mac Dingbats"
  :short-name "Mac Dingbats"
  :code-space [32 254]
  :map
  (let ((tbl-32-126
	 [?\  ?\✁ ?\✂ ?\✃ ?\✄ ?\☎ ?\✆ ?\✇ ?\✈ ?\✉ ?\☛ ?\☞ ?\✌ ?\✍ ?\✎ ?\✏
	  ?\✐ ?\✑ ?\✒ ?\✓ ?\✔ ?\✕ ?\✖ ?\✗ ?\✘ ?\✙ ?\✚ ?\✛ ?\✜ ?\✝ ?\✞ ?\✟
	  ?\✠ ?\✡ ?\✢ ?\✣ ?\✤ ?\✥ ?\✦ ?\✧ ?\★ ?\✩ ?\✪ ?\✫ ?\✬ ?\✭ ?\✮ ?\✯
	  ?\✰ ?\✱ ?\✲ ?\✳ ?\✴ ?\✵ ?\✶ ?\✷ ?\✸ ?\✹ ?\✺ ?\✻ ?\✼ ?\✽ ?\✾ ?\✿
	  ?\❀ ?\❁ ?\❂ ?\❃ ?\❄ ?\❅ ?\❆ ?\❇ ?\❈ ?\❉ ?\❊ ?\❋ ?\● ?\❍ ?\■ ?\❏
	  ?\❐ ?\❑ ?\❒ ?\▲ ?\▼ ?\◆ ?\❖ ?\◗ ?\❘ ?\❙ ?\❚ ?\❛ ?\❜ ?\❝ ?\❞])
	(map-32-126 (make-vector (* (1+ (- 126 32)) 2) nil))
	(tbl-128-141
	 [?\❨ ?\❩ ?\❪ ?\❫ ?\❬ ?\❭ ?\❮ ?\❯ ?\❰ ?\❱ ?\❲ ?\❳ ?\❴ ?\❵])
	(map-128-141 (make-vector (* (1+ (- 141 128)) 2) nil))
	(tbl-161-239
	 [?\❡ ?\❢ ?\❣ ?\❤ ?\❥ ?\❦ ?\❧ ?\♣ ?\♦ ?\♥ ?\♠ ?\① ?\② ?\③ ?\④
	  ?\⑤ ?\⑥ ?\⑦ ?\⑧ ?\⑨ ?\⑩ ?\❶ ?\❷ ?\❸ ?\❹ ?\❺ ?\❻ ?\❼ ?\❽ ?\❾ ?\❿
	  ?\➀ ?\➁ ?\➂ ?\➃ ?\➄ ?\➅ ?\➆ ?\➇ ?\➈ ?\➉ ?\➊ ?\➋ ?\➌ ?\➍ ?\➎ ?\➏
	  ?\➐ ?\➑ ?\➒ ?\➓ ?\➔ ?\→ ?\↔ ?\↕ ?\➘ ?\➙ ?\➚ ?\➛ ?\➜ ?\➝ ?\➞ ?\➟
	  ?\➠ ?\➡ ?\➢ ?\➣ ?\➤ ?\➥ ?\➦ ?\➧ ?\➨ ?\➩ ?\➪ ?\➫ ?\➬ ?\➭ ?\➮ ?\➯])
	(map-161-239 (make-vector (* (1+ (- 239 161)) 2) nil))
	(tbl-241-254
	 [?\➱ ?\➲ ?\➳ ?\➴ ?\➵ ?\➶ ?\➷ ?\➸ ?\➹ ?\➺ ?\➻ ?\➼ ?\➽ ?\➾])
	(map-241-254 (make-vector (* (1+ (- 254 241)) 2) nil)))
    (dotimes (i (1+ (- 126 32)))
      (aset map-32-126 (* i 2) (+ 32 i))
      (aset map-32-126 (1+ (* i 2)) (aref tbl-32-126 i)))
    (dotimes (i (1+ (- 141 128)))
      (aset map-128-141 (* i 2) (+ 128 i))
      (aset map-128-141 (1+ (* i 2)) (aref tbl-128-141 i)))
    (dotimes (i (1+ (- 239 161)))
      (aset map-161-239 (* i 2) (+ 161 i))
      (aset map-161-239 (1+ (* i 2)) (aref tbl-161-239 i)))
    (dotimes (i (1+ (- 254 241)))
      (aset map-241-254 (* i 2) (+ 241 i))
      (aset map-241-254 (1+ (* i 2)) (aref tbl-241-254 i)))
    (vconcat map-32-126 map-128-141 map-161-239 map-241-254)))

(defvar mac-system-coding-system nil
  "Coding system derived from the system script code.")

(defun mac-setup-system-coding-system ()
  (setq mac-system-coding-system
	(or (cdr (assq mac-system-script-code mac-script-code-coding-systems))
	    'mac-roman))
  (set-selection-coding-system mac-system-coding-system))


;;;; Keyboard layout/language change events
(defun mac-handle-language-change (event)
  "Set keyboard coding system to what is specified in EVENT."
  (interactive "e")
  (let ((coding-system
	 (cdr (assq (car (cadr event)) mac-script-code-coding-systems))))
    (set-keyboard-coding-system (or coding-system 'mac-roman))
    ;; MacJapanese maps reverse solidus to ?\x80.
    (if (eq coding-system 'japanese-shift-jis)
	(define-key key-translation-map [?\x80] "\\"))))

(define-key special-event-map [language-change] 'mac-handle-language-change)


;;;; Conversion between common flavors and Lisp string.

(defconst mac-text-encoding-ascii #x600
  "ASCII text encoding.")

(defconst mac-text-encoding-mac-japanese-basic-variant #x20001
  "MacJapanese text encoding without Apple double-byte extensions.")

(declare-function mac-code-convert-string "mac.c"
		  (string source target &optional normalization-form))

(defun mac-utxt-to-string (data &optional coding-system source-encoding)
  (or coding-system (setq coding-system mac-system-coding-system))
  (let* ((encoding
	  (and (eq (coding-system-base coding-system) 'japanese-shift-jis)
	       mac-text-encoding-mac-japanese-basic-variant))
	 (str (let (bytes data1 data-normalized data1-normalized)
		(and (setq bytes (mac-code-convert-string
				  data source-encoding
				  (or encoding coding-system)))
		     ;; Check if round-trip conversion gives an
		     ;; equivalent result.  We use HFS+D instead of
		     ;; NFD so OHM SIGN and GREEK CAPITAL LETTER OMEGA
		     ;; may not be considered equivalent.
		     (setq data1 (mac-code-convert-string
				  bytes (or encoding coding-system)
				  source-encoding))
		     (or (string= data1 data)
			 (and (setq data-normalized (mac-code-convert-string
						     data source-encoding
						     source-encoding 'HFS+D))
			      (setq data1-normalized (mac-code-convert-string
						      data1 source-encoding
						      source-encoding 'HFS+D))
			      (string= data1-normalized data-normalized)))
		     (decode-coding-string bytes coding-system)))))
    (if (and str (eq encoding mac-text-encoding-mac-japanese-basic-variant))
	;; Does it contain Apple one-byte extensions other than
	;; reverse solidus?
	(if (string-match
	     ;; Character alternatives in multibyte eight-bit is unreliable.
	     ;; (Bug#3687)
	     ;; (string-to-multibyte "[\xa0\xfd-\xff]")
	     (string-to-multibyte "\xa0\\|\xfd\\|\xfe\\|\xff") str)
	    (setq str nil)
	  ;; ASCII-only?
	  (unless (mac-code-convert-string data source-encoding
					   mac-text-encoding-ascii)
	    (subst-char-in-string ?\x5c ?\¥ str t)
	    (subst-char-in-string (unibyte-char-to-multibyte ?\x80) ?\\
				  str t))))
    (or str (decode-coding-string data
				  (or source-encoding
				      (if (eq (byteorder) ?B)
					  'utf-16be 'utf-16le))))))

(defun mac-TIFF-to-string (data &optional text)
  (propertize (or text " ") 'display (create-image data 'tiff t)))

(defun mac-pasteboard-string-to-string (data &optional coding-system)
  (mac-utxt-to-string data coding-system 'utf-8))

(defun mac-string-to-pasteboard-string (string &optional coding-system)
  (or coding-system (setq coding-system mac-system-coding-system))
  (let (data encoding)
    (when (memq (coding-system-base coding-system)
		(find-coding-systems-string string))
      (let ((str string))
	(when (eq coding-system 'japanese-shift-jis)
	  (setq encoding mac-text-encoding-mac-japanese-basic-variant)
	  (setq str (subst-char-in-string ?\\ (unibyte-char-to-multibyte ?\x80)
					  str))
	  (subst-char-in-string ?\¥ ?\x5c str t)
	  ;; ASCII-only?
	  (if (string-match "\\`[\x00-\x7f]*\\'" str)
	      (setq str nil)))
	(and str
	     (setq data (mac-code-convert-string
			 (encode-coding-string str coding-system)
			 (or encoding coding-system) 'utf-8)))))
    (or data (encode-coding-string string 'utf-8))))

(defun mac-pasteboard-filenames-to-file-urls (data)
  ;; DATA is a property list (in Foundation terminology) of the form
  ;; (array . [(string . FILENAME1) ... (string . FILENAMEn)]), where
  ;; each FILENAME is a unibyte string in UTF-8.
  (when (eq (car-safe data) 'array)
    (let ((coding (or file-name-coding-system default-file-name-coding-system)))
      (mapcar
       (lambda (tag-data)
	 (when (eq (car tag-data) 'string)
	   (let ((filename (encode-coding-string
			    (mac-pasteboard-string-to-string (cdr tag-data))
			    coding)))
	     (concat "file://localhost"
		     (mapconcat 'url-hexify-string
				(split-string filename "/") "/")))))
       (cdr data)))))


;;;; Selections

;; We keep track of the last text selected here, so we can check the
;; current selection against it, and avoid passing back our own text
;; from x-selection-value.
(defvar x-last-selected-text-clipboard nil
  "The value of the CLIPBOARD selection last time we selected or
pasted text.")
(defvar x-last-selected-text-primary nil
  "The value of the PRIMARY selection last time we selected or
pasted text.")

(defcustom x-select-enable-primary nil
  "Non-nil means cutting and pasting uses the primary selection."
  :type 'boolean
  :group 'killing)

(declare-function x-get-selection-internal "macselect.c"
		  (selection-symbol target-type &optional time-stamp))

(defun x-get-selection (&optional type data-type)
  "Return the value of a selection.
The argument TYPE (default `PRIMARY') says which selection,
and the argument DATA-TYPE (default `STRING') says
how to convert the data.

TYPE may be any symbol \(but nil stands for `PRIMARY').  However,
only a few symbols are commonly used.  They conventionally have
all upper-case names.  The most often used ones, in addition to
`PRIMARY', are `SECONDARY' and `CLIPBOARD'.

DATA-TYPE is usually `STRING', but can also be one of the symbols
in `selection-converter-alist', which see."
  (let ((data (x-get-selection-internal (or type 'PRIMARY)
					(or data-type 'STRING)))
	(coding (or next-selection-coding-system
		    selection-coding-system)))
    (when (and (stringp data)
	       (setq data-type (get-text-property 0 'foreign-selection data)))
      (cond ((eq data-type 'NSStringPboardType)
	     (setq data (mac-pasteboard-string-to-string data coding))))
      (put-text-property 0 (length data) 'foreign-selection data-type data))
    data))

(defun x-selection-value-internal (type)
  (let ((data-types '(NSStringPboardType))
	text tiff-image)
    (while (and (null text) data-types)
      (setq text (condition-case nil
		     (x-get-selection type (car data-types))
		   (error nil)))
      (setq data-types (cdr data-types)))
    (if text
	(remove-text-properties 0 (length text) '(foreign-selection nil) text))
    (setq tiff-image (condition-case nil
			 (x-get-selection type 'NSTIFFPboardType)
		       (error nil)))
    (when tiff-image
      (remove-text-properties 0 (length tiff-image)
			      '(foreign-selection nil) tiff-image)
      (if text
	  (setq text (list text (mac-TIFF-to-string tiff-image text)))
	(setq text (mac-TIFF-to-string tiff-image))))
    text))

(defun mac-selection-value-equal (v1 v2)
  (let ((equal-so-far t))
    (while (and (consp v1) (consp v2) equal-so-far)
      (setq equal-so-far
	    (mac-selection-value-equal (car v1) (car v2)))
      (setq v1 (cdr v1) v2 (cdr v2)))
    (and equal-so-far
	 (if (not (and (stringp v1) (stringp v2)))
	     (equal v1 v2)
	   (and (string= v1 v2)
		(equal (get-text-property 0 'display v1)
		       (get-text-property 0 'display v2)))))))

;; Return the value of the current selection.
;; Treat empty strings as if they were unset.
;; If this function is called twice and finds the same text,
;; it returns nil the second time.  This is so that a single
;; selection won't be added to the kill ring over and over.
(defun x-selection-value ()
  ;; With multi-tty, this function may be called from a tty frame.
  (when (eq (framep (selected-frame)) 'mac)
    (let (clip-text primary-text selection-value)
      (when x-select-enable-clipboard
	(setq clip-text (x-selection-value-internal 'CLIPBOARD))
	(if (equal clip-text "") (setq clip-text nil))

	;; Check the CLIPBOARD selection for 'newness', is it different
	;; from what we remembered them to be last time we did a
	;; cut/paste operation.
	(setq clip-text
	      (cond ;; check clipboard
	       ((or (not clip-text) (equal clip-text ""))
		(setq x-last-selected-text-clipboard nil))
	       ((eq      clip-text x-last-selected-text-clipboard) nil)
	       ((mac-selection-value-equal clip-text
					   x-last-selected-text-clipboard)
		;; Record the newer string,
		;; so subsequent calls can use the `eq' test.
		(setq x-last-selected-text-clipboard clip-text)
		nil)
	       (t
		(setq x-last-selected-text-clipboard clip-text)))))

      (when x-select-enable-primary
	(setq primary-text (x-selection-value-internal 'PRIMARY))
	;; Check the PRIMARY selection for 'newness', is it different
	;; from what we remembered them to be last time we did a
	;; cut/paste operation.
	(setq primary-text
	      (cond ;; check primary selection
	       ((or (not primary-text) (equal primary-text ""))
		(setq x-last-selected-text-primary nil))
	       ((eq      primary-text x-last-selected-text-primary) nil)
	       ((mac-selection-value-equal primary-text
					   x-last-selected-text-primary)
		;; Record the newer string,
		;; so subsequent calls can use the `eq' test.
		(setq x-last-selected-text-primary primary-text)
		nil)
	       (t
		(setq x-last-selected-text-primary primary-text)))))

      ;; As we have done one selection, clear this now.
      (setq next-selection-coding-system nil)

      ;; At this point we have recorded the current values for the
      ;; selection from clipboard (if we are supposed to) and primary,
      ;; So return the first one that has changed (which is the first
      ;; non-null one).
      (setq selection-value (or clip-text primary-text))
      ;; If the selection-value contains multiple items, we need to
      ;; protect the saved x-last-selected-text-clipboard/primary from
      ;; caller's nreverse.
      (if (listp selection-value)
	  (setq selection-value (copy-sequence selection-value)))
      selection-value)))

(define-obsolete-function-alias 'x-cut-buffer-or-selection-value
  'x-selection-value "24.1")

;; Arrange for the kill and yank functions to set and check the clipboard.
(setq interprogram-cut-function 'x-select-text)
(setq interprogram-paste-function 'x-selection-value)

;; Make paste from other applications use the decoding in x-select-request-type
;; and not just STRING.
(defun x-get-selection-value ()
  "Get the current value of the PRIMARY selection.
Request data types in the order specified by `x-select-request-type'."
  (x-selection-value-internal 'PRIMARY))

(defun mac-setup-selection-properties ()
  (put 'CLIPBOARD 'mac-pasteboard-name
       "Apple CFPasteboard general")	; NSGeneralPboard
  (put 'FIND 'mac-pasteboard-name
       "Apple CFPasteboard find")	; NSFindPboard
  (put 'PRIMARY 'mac-pasteboard-name
       (format "GNU Emacs CFPasteboard primary %d" (emacs-pid)))
  (put 'NSStringPboardType 'mac-pasteboard-data-type "NSStringPboardType")
  (put 'NSTIFFPboardType 'mac-pasteboard-data-type
       "NeXT TIFF v4.0 pasteboard type")
  (put 'NSFilenamesPboardType 'mac-pasteboard-data-type
       "NSFilenamesPboardType"))

(defun mac-select-convert-to-string (selection type value)
  (let ((str (xselect-convert-to-string selection nil value))
	(coding (or next-selection-coding-system selection-coding-system)))
    (when str
      ;; If TYPE is nil, this is a local request; return STR as-is.
      (if (null type)
	  str
	;; Otherwise, encode STR.
	(let ((inhibit-read-only t))
	  (remove-text-properties 0 (length str) '(composition nil) str)
	  (cond
	   ((eq type 'NSStringPboardType)
	    (setq str (mac-string-to-pasteboard-string str coding)))
	   (t
	    (error "Unknown selection type: %S" type))))

	(setq next-selection-coding-system nil)
	(cons type str)))))

(defun mac-select-convert-to-pasteboard-filenames (selection type value)
  (let ((filename (xselect-convert-to-filename selection type value)))
    (and filename
	 (setq filename (mac-string-to-pasteboard-string filename))
	 (cons type `(array . [(string . ,filename)])))))

(setq selection-converter-alist
      (nconc
       '((NSStringPboardType . mac-select-convert-to-string)
	 (NSTIFFPboardType . nil)
	 (NSFilenamesPboardType . mac-select-convert-to-pasteboard-filenames)
	 )
       selection-converter-alist))

;;;; Apple events, HICommand events, and Services menu

(defvar mac-startup-options)

;;; Event classes
(put 'core-event     'mac-apple-event-class "aevt") ; kCoreEventClass
(put 'internet-event 'mac-apple-event-class "GURL") ; kAEInternetEventClass
(put 'odb-editor-suite  'mac-apple-event-class "R*ch") ; kODBEditorSuite

;;; Event IDs
;; kCoreEventClass
(put 'open-application     'mac-apple-event-id "oapp") ; kAEOpenApplication
(put 'reopen-application   'mac-apple-event-id "rapp") ; kAEReopenApplication
(put 'open-documents       'mac-apple-event-id "odoc") ; kAEOpenDocuments
(put 'print-documents      'mac-apple-event-id "pdoc") ; kAEPrintDocuments
(put 'open-contents        'mac-apple-event-id "ocon") ; kAEOpenContents
(put 'quit-application     'mac-apple-event-id "quit") ; kAEQuitApplication
(put 'application-died     'mac-apple-event-id "obit") ; kAEApplicationDied
(put 'show-preferences     'mac-apple-event-id "pref") ; kAEShowPreferences
(put 'autosave-now         'mac-apple-event-id "asav") ; kAEAutosaveNow
(put 'answer               'mac-apple-event-id "ansr") ; kAEAnswer
;; kAEInternetEventClass
(put 'get-url              'mac-apple-event-id "GURL") ; kAEGetURL
;; kODBEditorSuite
(put 'modified-file        'mac-apple-event-id "FMod") ; kAEModifiedFile
(put 'closed-file          'mac-apple-event-id "FCls") ; kAEClosedFile

(defmacro mac-event-spec (event)
  `(nth 1 ,event))

(defmacro mac-event-ae (event)
  `(nth 2 ,event))

(declare-function mac-coerce-ae-data "mac.c" (src-type src-data dst-type))

(defun mac-ae-parameter (ae &optional keyword type)
  (or keyword (setq keyword "----")) ;; Direct object.
  (if (not (and (consp ae) (equal (car ae) "aevt")))
      (error "Not an Apple event: %S" ae)
    (let ((type-data (cdr (assoc keyword (cdr ae))))
	  data)
      (when (and type type-data (not (equal type (car type-data))))
	(setq data (mac-coerce-ae-data (car type-data) (cdr type-data) type))
	(setq type-data (if data (cons type data) nil)))
      type-data)))

(defun mac-ae-list (ae &optional keyword type)
  (or keyword (setq keyword "----")) ;; Direct object.
  (let ((desc (mac-ae-parameter ae keyword "list")))
    (cond ((null desc)
	   nil)
	  ((not (equal (car desc) "list"))
	   (error "Parameter for \"%s\" is not a list" keyword))
	  (t
	   (if (null type)
	       (cdr desc)
	     (mapcar
	      (lambda (type-data)
		(mac-coerce-ae-data (car type-data) (cdr type-data) type))
	      (cdr desc)))))))

(defun mac-ae-number (ae keyword)
  (let ((type-data (mac-ae-parameter ae keyword))
	str)
    (if (and type-data
	     (setq str (mac-coerce-ae-data (car type-data)
					   (cdr type-data) "TEXT")))
	(let ((num (string-to-number str)))
	  ;; Mac OS Classic may return "0e+0" as the coerced value for
	  ;; the type "magn" and the data "\000\000\000\000".
	  (if (= num 0.0) 0 num))
      nil)))

(defun mac-bytes-to-integer (bytes &optional from to)
  (or from (setq from 0))
  (or to (setq to (length bytes)))
  (let* ((len (- to from))
	 (extended-sign-len (- (1+ (ceiling (log most-positive-fixnum 2)))
			       (* 8 len)))
	 (result 0))
    (dotimes (i len)
      (setq result (logior (lsh result 8)
			   (aref bytes (+ from (if (eq (byteorder) ?B) i
						 (- len i 1)))))))
    (if (> extended-sign-len 0)
	(ash (lsh result extended-sign-len) (- extended-sign-len))
      result)))

(defun mac-ae-selection-range (ae)
;; #pragma options align=mac68k
;; typedef struct SelectionRange {
;;   short unused1; // 0 (not used)
;;   short lineNum; // line to select (<0 to specify range)
;;   long startRange; // start of selection range (if line < 0)
;;   long endRange; // end of selection range (if line < 0)
;;   long unused2; // 0 (not used)
;;   long theDate; // modification date/time
;; } SelectionRange;
;; #pragma options align=reset
  (let ((range-bytes (cdr (mac-ae-parameter ae "kpos" "TEXT"))))
    (and range-bytes
	 (list (mac-bytes-to-integer range-bytes 2 4)
	       (mac-bytes-to-integer range-bytes 4 8)
	       (mac-bytes-to-integer range-bytes 8 12)
	       (mac-bytes-to-integer range-bytes 16 20)))))

;; On Mac OS X 10.4 and later, the `open-document' event contains an
;; optional parameter keyAESearchText from the Spotlight search.
(defun mac-ae-text-for-search (ae)
  (let ((utf8-text (cdr (mac-ae-parameter ae "stxt" "utf8"))))
    (and utf8-text
	 (decode-coding-string utf8-text 'utf-8))))

(defun mac-ae-text (ae)
  (or (cdr (mac-ae-parameter ae nil "TEXT"))
      (error "No text in Apple event.")))

(defun mac-ae-script-language (ae keyword)
;; struct WritingCode {
;;   ScriptCode          theScriptCode;
;;   LangCode            theLangCode;
;; };
  (let ((bytes (cdr (mac-ae-parameter ae keyword "intl"))))
    (and bytes
	 (cons (mac-bytes-to-integer bytes 0 2)
	       (mac-bytes-to-integer bytes 2 4)))))

(defconst mac-keyboard-modifier-mask-alist
  (mapcar
   (lambda (modifier-bit)
     (cons (car modifier-bit) (lsh 1 (cdr modifier-bit))))
   '((command  . 8)			; cmdKeyBit
     (shift    . 9)			; shiftKeyBit
     (option   . 11)			; optionKeyBit
     (control  . 12)			; controlKeyBit
     (function . 17)))			; kEventKeyModifierFnBit
  "Alist of Mac keyboard modifier symbols vs masks.")

(defun mac-ae-keyboard-modifiers (ae)
  (let ((modifiers-value (mac-ae-number ae "kmod"))
	modifiers)
    (if modifiers-value
	(dolist (modifier-mask mac-keyboard-modifier-mask-alist)
	  (if (/= (logand modifiers-value (cdr modifier-mask)) 0)
	      (setq modifiers (cons (car modifier-mask) modifiers)))))
    modifiers))

(defun mac-ae-type-string (ae keyword)
  (let ((bytes (cdr (mac-ae-parameter ae keyword "type"))))
    (and bytes
	 (if (eq (byteorder) ?B)
	     bytes
	   (apply 'unibyte-string (nreverse (append bytes '())))))))

(defun mac-ae-open-application (event)
  "Open the application Emacs with the Apple event EVENT.
It records the Apple event in `mac-startup-options' as a value
for the key symbol `apple-event' so it can be inspected later."
  (interactive "e")
  (push (cons 'apple-event (mac-event-ae event)) mac-startup-options))

(defun mac-ae-reopen-application (event)
  "Show some frame in response to the Apple event EVENT.
The frame to be shown is chosen from visible or iconified frames
if possible.  If there's no such frame, a new frame is created."
  (interactive "e")
  (unless (frame-visible-p (selected-frame))
    (let ((frame (or (car (visible-frame-list))
		     (car (filtered-frame-list 'frame-visible-p)))))
      (if frame
	  (select-frame frame)
	(switch-to-buffer-other-frame "*scratch*"))))
  (select-frame-set-input-focus (selected-frame)))

(defun mac-ae-open-documents (event)
  "Open the documents specified by the Apple event EVENT."
  (interactive "e")
  (let ((ae (mac-event-ae event)))
    (dolist (file-name (mac-ae-list ae nil 'undecoded-file-name))
      (if file-name
	  (dnd-open-local-file
	   (concat "file://"
		   (mapconcat 'url-hexify-string
			      (split-string file-name "/") "/")) nil)))
    (let ((selection-range (mac-ae-selection-range ae))
	  (search-text (mac-ae-text-for-search ae)))
      (cond (selection-range
	     (let ((line (car selection-range))
		   (start (cadr selection-range))
		   (end (nth 2 selection-range)))
	       (if (>= line 0)
		   (progn
		     (goto-char (point-min))
		     (forward-line line)) ; (1- (1+ line))
		 (if (and (>= start 0) (>= end 0))
		     (progn (set-mark (1+ start))
			    (goto-char (1+ end)))))))
	    ((stringp search-text)
	     (re-search-forward
	      (mapconcat 'regexp-quote (split-string search-text) "\\|")
	      nil t))))
    (mac-odb-setup-buffer ae))
  (select-frame-set-input-focus (selected-frame)))

(declare-function mac-resume-apple-event "macselect.c"
		  (apple-event &optional error-code))

(defun mac-ae-quit-application (event)
  "Quit the application Emacs with the Apple event EVENT."
  (interactive "e")
  (let ((ae (mac-event-ae event)))
    (unwind-protect
	(save-buffers-kill-emacs)
      ;; Reaches here if the user has canceled the quit.
      (mac-resume-apple-event ae -128)))) ; userCanceledErr

(defun mac-type-string-to-bytes (type-string)
  (unless (and (stringp type-string) (= (string-bytes type-string) 4))
    (error "Invalid type string (not a 4-byte string): %S" type-string))
  (if (eq (byteorder) ?l)
      (setq type-string
	    (apply 'unibyte-string (nreverse (append type-string '())))))
  type-string)

(defun mac-create-apple-event (event-class event-id target)
  "Create a Lisp representation of an Apple event.
EVENT-CLASS (or EVENT-ID) represents the Apple event class (or
ID, resp.).  It should be either a 4-byte string or a symbol
whose `mac-apple-event-class' (or `mac-apple-event-id', resp.)
property is a 4-byte string.

TARGET specifies the target application for the event.  It should
be either a Lisp representation of the target address Apple event
descriptor (e.g., (cons \"sign\" (mac-type-string-to-bytes
\"MACS\")) for Finder), an integer specifying the process ID, or
a string specifying bundle ID on Mac OS X 10.3 and later (e.g.,
\"com.apple.Finder\") or an application URL with the `eppc'
scheme."
  (if (symbolp event-class)
      (setq event-class (get event-class 'mac-apple-event-class)))
  (if (symbolp event-id)
      (setq event-id (get event-id 'mac-apple-event-id)))
  (cond ((numberp target)
	 (setq target (cons "kpid"	; typeKernelProcessID
			    (mac-coerce-ae-data
			     "TEXT" (number-to-string target) "long"))))
	((stringp target)
	 (if (string-match "\\`eppc://" target)
	     (setq target (cons "aprl"	; typeApplicationURL
				(encode-coding-string target 'utf-8)))
	   (setq target (cons "bund"	; typeApplicationBundleID
			      (encode-coding-string target 'utf-8)))))
	((not (and (consp target) (stringp (car target))
		   (= (string-bytes (car target)) 4)))
	 (error "Not an address descriptor: %S" target)))
  `("aevt" . ((event-class . ("type" . ,(mac-type-string-to-bytes event-class)))
	      (event-id . ("type" . ,(mac-type-string-to-bytes event-id)))
	      (address . ,target))))

(defun mac-ae-set-parameter (apple-event keyword descriptor)
  "Set parameter KEYWORD to DESCRIPTOR on APPLE-EVENT.
APPLE-EVENT is a Lisp representation of an Apple event,
preferably created with `mac-create-apple-event'.  See
`mac-ae-set-reply-parameter' for the formats of KEYWORD and
DESCRIPTOR.  If KEYWORD is a symbol starting with `:', then
DESCRIPTOR can be any Lisp value.  In that case, the value is not
sent as an Apple event, but you can use it in the reply handler.
See `mac-send-apple-event' for such usage.  This function
modifies APPLE-EVENT as a side effect and returns APPLE-EVENT."
  (if (not (and (consp apple-event) (equal (car apple-event) "aevt")))
      (error "Not an Apple event: %S" apple-event)
    (setcdr apple-event
	    (cons (cons keyword descriptor)
		  (delete keyword (cdr apple-event))))
    apple-event))

(defvar mac-sent-apple-events nil
  "List of previously sent Apple events.")

(defun mac-send-apple-event (apple-event &optional callback)
  "Send APPLE-EVENT.  CALLBACK is called when its reply arrives.
APPLE-EVENT is a Lisp representation of an Apple event,
preferably created with `mac-create-apple-event' and
`mac-ae-set-parameter'.

If CALLBACK is nil, then the reply is not requested.  If CALLBACK
is t, then the function does not return until the reply arrives.
Otherwise, the function returns without waiting for the arrival
of the reply, and afterwards CALLBACK is called with one
argument, the reply event, when the reply arrives.

The function returns an integer if some error has occurred in
sending, the Lisp representation of the sent Apple event if
CALLBACK is not t, and the Lisp representation of the reply Apple
event if CALLBACK is t.

With the reply event, which is given either as the argument to
CALLBACK or as the return value in the case that CALLBACK is t,
one can examine the originally sent Apple event via the
`original-apple-event' parameter.

Each event parameter whose keyword is a symbol starting with `:'
is not sent as an Apple event, but given as a parameter of the
original event to CALLBACK.  So, you can pass context information
through this without \"emulated closures\".  For example,

  (let ((apple-event (mac-create-apple-event ...)))
    (mac-ae-set-parameter apple-event :context1 SOME-LISP-VALUE1)
    (mac-ae-set-parameter apple-event :context2 SOME-LISP-VALUE2)
    (mac-send-apple-event
     apple-event
     (lambda (reply-ae)
       (let* ((orig-ae (mac-ae-parameter reply-ae 'original-apple-event))
              (context1-value (mac-ae-parameter orig-ae :context1))
	      (context2-value (mac-ae-parameter orig-ae :context2)))
	 ...))))"
  (let ((ae (mac-send-apple-event-internal apple-event (and callback t))))
    (when (consp ae)
      (let ((params (cdr ae)))
	(dolist (param (cdr apple-event))
	  (if (not (assoc (car param) params))
	      (setcdr ae (cons param (cdr ae))))))
      (cond ((null callback))		; no reply
	    ((not (eq callback t))	; asynchronous
	     (push (mac-ae-set-parameter ae 'callback callback)
		   mac-sent-apple-events))
	    (t				; synchronous
	     (push (mac-ae-set-parameter ae 'callback
					 (lambda (reply-ae)
					   (throw 'mac-send-apple-event
						  reply-ae)))
		   mac-sent-apple-events)
	     (let ((orig-ae ae))
	       (unwind-protect
		   (let (reply-ae events)
		     (while (null (setq reply-ae (catch 'mac-send-apple-event
						   (push (read-event) events)
						   nil))))
		     (setq unread-command-events (nreverse events))
		     (setq ae reply-ae))
		 (mac-ae-set-parameter orig-ae 'callback 'ignore))))))
    ae))

(defun mac-ae-answer (event)
  "Handle the reply EVENT for a previously sent Apple event."
  (interactive "e")
  (let* ((reply-ae (mac-event-ae event))
	 (return-id (mac-ae-parameter reply-ae 'return-id "shor"))
	 (rest mac-sent-apple-events)
	 prev)
    (while (and rest
		(not (equal (mac-ae-parameter (car rest) 'return-id "shor")
			    return-id)))
      (setq rest (cdr (setq prev rest))))
    (when rest
      (if prev
	  (setcdr prev (cdr rest))
	(setq mac-sent-apple-events (cdr mac-sent-apple-events)))
      (funcall (mac-ae-parameter (car rest) 'callback)
	       (mac-ae-set-parameter reply-ae 'original-apple-event
				     (car rest))))))

;; url-generic-parse-url is autoloaded from url-parse.
(declare-function url-type "url-parse" t t) ; defstruct

(defun mac-ae-get-url (event)
  "Open the URL specified by the Apple event EVENT.
Currently the `mailto' scheme is supported."
  (interactive "e")
  (let* ((ae (mac-event-ae event))
	 (parsed-url (url-generic-parse-url (mac-ae-text ae))))
    (if (string= (url-type parsed-url) "mailto")
	(progn
	  (url-mailto parsed-url)
	  (select-frame-set-input-focus (selected-frame)))
      (mac-resume-apple-event ae t))))

(setq mac-apple-event-map (make-sparse-keymap))

;; Received when Emacs is launched without associated documents.
;; We record the received Apple event in `mac-startup-options' as a
;; value for the key symbol `apple-event' so we can inspect some
;; parameters such as keyAEPropData ("prdt") that may contain
;; keyAELaunchedAsLogInItem ("lgit") or keyAELaunchedAsServiceItem
;; ("svit") on Mac OS X 10.4 and later.  You should use
;; `mac-ae-type-string' for extracting data for keyAEPropData in order
;; to take byte order into account.
(define-key mac-apple-event-map [core-event open-application]
  'mac-ae-open-application)

;; Received when a dock or application icon is clicked and Emacs is
;; already running.
(define-key mac-apple-event-map [core-event reopen-application]
  'mac-ae-reopen-application)

(define-key mac-apple-event-map [core-event open-documents]
  'mac-ae-open-documents)
(define-key mac-apple-event-map [core-event show-preferences] 'customize)
(define-key mac-apple-event-map [core-event quit-application]
  'mac-ae-quit-application)
(define-key mac-apple-event-map [core-event answer] 'mac-ae-answer)

(define-key mac-apple-event-map [internet-event get-url] 'mac-ae-get-url)

;;; ODB Editor Suite
(defvar mac-odb-received-apple-events nil
  "List of received Apple Events containing ODB Editor Suite parameters.")
(put 'mac-odb-received-apple-events 'permanent-local t)

(defun mac-odb-setup-buffer (apple-event)
  (when (or (mac-ae-parameter apple-event "FSnd") ; keyFileSender
	    ;; (mac-ae-parameter apple-event "FSfd") ; keyFileSenderDesc
	    )
    (make-local-variable 'mac-odb-received-apple-events)
    (let ((custom-path (mac-ae-parameter apple-event "Burl" "utxt")))
      (if custom-path
	  (rename-buffer (mac-utxt-to-string (cdr custom-path)) t)))
    (mac-ae-set-parameter apple-event :original-file-name buffer-file-name)
    (push apple-event mac-odb-received-apple-events)
    (add-hook 'after-save-hook 'mac-odb-send-modified-file-events nil t)
    (add-hook 'kill-buffer-hook 'mac-odb-send-closed-file-events nil t)
    (add-hook 'kill-emacs-hook 'mac-odb-send-closed-file-events-all-buffers)
    (run-hooks 'mac-odb-setup-buffer-hook)))

(defun mac-odb-cleanup-buffer ()
  (run-hooks 'mac-odb-cleanup-buffer-hook)
  (remove-hook 'after-save-hook 'mac-odb-send-modified-file-events t)
  (remove-hook 'kill-buffer-hook 'mac-odb-send-closed-file-events t)
  (kill-local-variable 'mac-odb-received-apple-events))

(defun mac-odb-file-sender-desc (apple-event)
  (or ;; (mac-ae-parameter apple-event "FSfd")
      (let ((sign-bytes (cdr (mac-ae-parameter apple-event "FSnd" "type"))))
	(and sign-bytes (cons "sign" sign-bytes)))))

(defun mac-odb-send-modified-file-events ()
  (dolist (orig-ae mac-odb-received-apple-events)
    (let ((original-file (mac-ae-parameter orig-ae))
	  (sender-token (mac-ae-parameter orig-ae "FTok"))
	  (original-file-name (mac-ae-parameter orig-ae :original-file-name))
	  (apple-event
	   (mac-create-apple-event 'odb-editor-suite 'modified-file
				   (mac-odb-file-sender-desc orig-ae))))
      (mac-ae-set-parameter apple-event "----" original-file)
      (if (not (equal original-file-name buffer-file-name))
	  (let* ((coding (or file-name-coding-system
			     default-file-name-coding-system))
		 (file-desc-type (car original-file))
		 (new-location
		  (cons file-desc-type
			(mac-coerce-ae-data 'undecoded-file-name
					    (encode-coding-string
					     buffer-file-name coding)
					    file-desc-type))))
	    (mac-ae-set-parameter apple-event "New?" new-location)))
      (if sender-token
	  (mac-ae-set-parameter apple-event "FTok" sender-token))
      (mac-send-apple-event apple-event))))

(defun mac-odb-send-closed-file-events ()
  (dolist (orig-ae mac-odb-received-apple-events)
    (let ((original-file (mac-ae-parameter orig-ae))
	  (sender-token (mac-ae-parameter orig-ae "FTok"))
	  (apple-event
	   (mac-create-apple-event 'odb-editor-suite 'closed-file
				   (mac-odb-file-sender-desc orig-ae))))
      (mac-ae-set-parameter apple-event "----" original-file)
      (if sender-token
	  (mac-ae-set-parameter apple-event "FTok" sender-token))
      (mac-send-apple-event apple-event)))
  (mac-odb-cleanup-buffer))

(defun mac-odb-send-closed-file-events-all-buffers ()
  (save-excursion
    (dolist (buffer (buffer-list))
      (set-buffer buffer)
      (if mac-odb-received-apple-events
	  (mac-odb-send-closed-file-events)))))

;;; Font panel
(declare-function mac-set-font-panel-visible-p "macfns.c" (flag))

(define-minor-mode mac-font-panel-mode
  "Toggle use of the font panel.
With numeric ARG, display the font panel if and only if ARG is positive."
  :init-value nil
  :global t
  :group 'mac
  (mac-set-font-panel-visible-p mac-font-panel-mode))

(defun mac-handle-font-panel-closed (event)
  "Update internal status in response to font panel closed EVENT."
  (interactive "e")
  ;; Synchronize with the minor mode variable.
  (mac-font-panel-mode 0))

(defun mac-handle-font-selection (event)
  "Change default face attributes according to font selection EVENT."
  (interactive "e")
  (let* ((ae (mac-event-ae event))
	 (font-spec (cdr (mac-ae-parameter ae 'font-spec))))
    (if font-spec
	(set-face-attribute 'default (selected-frame) :font font-spec))))

;; kEventClassFont/kEventFontPanelClosed
(define-key mac-apple-event-map [font panel-closed]
  'mac-handle-font-panel-closed)
;; kEventClassFont/kEventFontSelection
(define-key mac-apple-event-map [font selection] 'mac-handle-font-selection)

(define-key-after menu-bar-showhide-menu [mac-font-panel-mode]
  (menu-bar-make-mm-toggle mac-font-panel-mode
			   "Font Panel"
			   "Show the font panel as a floating dialog")
  'showhide-speedbar)

;;; Text Services
(setq mac-ts-active-input-overlay (make-overlay 1 1))
(overlay-put mac-ts-active-input-overlay 'display "")

(defface mac-ts-caret-position
  '((t :inverse-video t))
  "Face for caret position in Mac TSM active input area.
This is used when the active input area is displayed either in
the echo area or in a buffer where the cursor is not displayed."
  :group 'mac)

(defface mac-ts-raw-text
  '((t :underline t))
  "Face for raw text in Mac TSM active input area."
  :group 'mac)

(defface mac-ts-converted-text
  '((((background dark)) :underline "gray20")
    (t :underline "gray80"))
  "Face for converted text in Mac TSM active input area."
  :group 'mac)

(defface mac-ts-selected-converted-text
  '((t :underline t))
  "Face for selected converted text in Mac TSM active input area."
  :group 'mac)

(defun mac-split-string-by-property-change (string)
  (let ((tail (length string))
	head result)
    (unless (= tail 0)
      (while (setq head (previous-property-change tail string)
		   result (cons (substring string (or head 0) tail) result)
		   tail head)))
    result))

(defun mac-replace-untranslated-utf-8-chars (string &optional to-string)
  (or to-string (setq to-string "�"))
  (mapconcat
   (lambda (str)
     (if (get-text-property 0 'untranslated-utf-8 str) to-string str))
   (mac-split-string-by-property-change string)
   ""))

(defun mac-keyboard-translate-char (ch)
  (if (and (characterp ch)
	   (or (char-table-p keyboard-translate-table)
	       (and (or (stringp keyboard-translate-table)
			(vectorp keyboard-translate-table))
		    (> (length keyboard-translate-table) ch))))
      (or (aref keyboard-translate-table ch) ch)
    ch))

(defun mac-unread-string (string)
  ;; Unread characters and insert them in a keyboard macro being
  ;; defined.
  (apply 'isearch-unread
	 (mapcar 'mac-keyboard-translate-char
		 (mac-replace-untranslated-utf-8-chars string))))

(defconst mac-marked-text-underline-style-faces
  '((0 . mac-ts-raw-text)		  ; NSUnderlineStyleNone
    (1 . mac-ts-converted-text)		  ; NSUnderlineStyleSingle
    (2 . mac-ts-selected-converted-text)) ; NSUnderlineStyleThick
  "Alist of NSUnderlineStyle vs Emacs face in marked text.")

(defun mac-text-input-set-marked-text (event)
  (interactive "e")
  (let* ((ae (mac-event-ae event))
	 (text (cdr (mac-ae-parameter ae)))
	 (selected-range (cdr (mac-ae-parameter ae "selectedRange")))
	 (replacement-range (cdr (mac-ae-parameter ae "replacementRange")))
	 (script-language (mac-ae-script-language ae "tssl"))
	 (coding (and script-language
		      (or (cdr (assq (car script-language)
				     mac-script-code-coding-systems))
			  'mac-roman))))
    (let ((use-echo-area
	   (or isearch-mode
	       (and cursor-in-echo-area (current-message))
	       ;; Overlay strings are not shown in some cases.
	       (get-char-property (point) 'invisible)
	       (and (not (bobp))
		    (or (and (get-char-property (point) 'display)
			     (eq (get-char-property (1- (point)) 'display)
				 (get-char-property (point) 'display)))
			(and (get-char-property (point) 'composition)
			     (eq (get-char-property (1- (point)) 'composition)
				 (get-char-property (point) 'composition)))))))
	  active-input-string caret-seen)
      ;; Decode the active input area text with inheriting faces and
      ;; the caret position.
      (put-text-property (* (car selected-range) 2) (length text)
			 'cursor t text)
      (setq active-input-string
	    (mapconcat
	     (lambda (str)
	       (let* ((decoded (mac-utxt-to-string str coding))
		      (underline-style
		       (or (cdr (get-text-property 0 'NSUnderline str)) 0))
		      (face
		       (cdr (assq underline-style
				  mac-marked-text-underline-style-faces))))
		 (put-text-property 0 (length decoded) 'face face decoded)
		 (when (and (not caret-seen)
			    (get-text-property 0 'cursor str))
		   (setq caret-seen t)
		   (if (or use-echo-area (null cursor-type))
		       (put-text-property 0 1 'face 'mac-ts-caret-position
					  decoded)
		     (put-text-property 0 1 'cursor t decoded)))
		 decoded))
	     (mac-split-string-by-property-change text)
	     ""))
      (put-text-property 0 (length active-input-string)
			 'mac-ts-active-input-string t active-input-string)
      (if use-echo-area
	  (let ((msg (current-message))
		message-log-max)
	    (if (and msg
		     ;; Don't get confused by previously displayed
		     ;; `active-input-string'.
		     (null (get-text-property 0 'mac-ts-active-input-string
					      msg)))
		(setq msg (propertize msg 'display
				      (concat msg active-input-string)))
	      (setq msg active-input-string))
	    (message "%s" msg)
	    (move-overlay mac-ts-active-input-overlay 1 1)
	    (overlay-put mac-ts-active-input-overlay 'before-string nil))
	(move-overlay mac-ts-active-input-overlay
		      (point)
		      (if (and delete-selection-mode transient-mark-mode
			       mark-active (not buffer-read-only))
			  (mark)
			(point))
		      (current-buffer))
	(overlay-put mac-ts-active-input-overlay 'before-string
		     active-input-string)
	(if replacement-range
	    (condition-case nil
		;; Strictly speaking, the replacement range can be out
		;; of sync.
		(delete-region (+ (point-min) (car replacement-range))
			       (+ (point-min) (car replacement-range)
				  (cdr replacement-range)))
	      (error nil)))))))

(defun mac-text-input-insert-text (event)
  (interactive "e")
  (let* ((ae (mac-event-ae event))
	 (text (cdr (mac-ae-parameter ae)))
	 (replacement-range (cdr (mac-ae-parameter ae "replacementRange")))
	 (script-language (mac-ae-script-language ae "tssl"))
	 (coding (and script-language
		      (or (cdr (assq (car script-language)
				     mac-script-code-coding-systems))
			  'mac-roman))))
    (move-overlay mac-ts-active-input-overlay 1 1)
    (overlay-put mac-ts-active-input-overlay 'before-string nil)
    (let ((msg (current-message))
	  message-log-max)
      (when msg
	(if (get-text-property 0 'mac-ts-active-input-string msg)
	    (message nil)
	  (let ((disp-prop (get-text-property 0 'display msg)))
	    (when (and (stringp disp-prop)
		       (> (length disp-prop) 1)
		       (get-text-property (1- (length disp-prop))
					  'mac-ts-active-input-string))
	      (remove-text-properties 0 (length disp-prop)
				      '(mac-ts-active-input-string nil)
				      msg)
	      (message "%s" msg))))))
    (if replacement-range
	(condition-case nil
	    ;; Strictly speaking, the replacement range can be out of
	    ;; sync.
	    (delete-region (+ (point-min) (car replacement-range))
			   (+ (point-min) (car replacement-range)
			      (cdr replacement-range)))
	  (error nil)))
    (mac-unread-string (mac-utxt-to-string text coding))))

(define-key mac-apple-event-map [text-input set-marked-text]
  'mac-text-input-set-marked-text)
(define-key mac-apple-event-map [text-input insert-text]
  'mac-text-input-insert-text)

;;; Converted Actions
(defun mac-handle-about (event)
  "Display the *About GNU Emacs* buffer in response to EVENT."
  (interactive "e")
  (if (use-fancy-splash-screens-p)
      (mac-start-animation (fancy-splash-frame) :type 'mod :duration 0.5))
  ;; Convert a event bound in special-event-map to a normal event.
  (setq unread-command-events
	(append '(menu-bar help-menu about-emacs) unread-command-events)))

(defun mac-handle-copy (event)
  "Copy the selected text to the clipboard.
This is used in response to \"Speak selected text.\""
  (interactive "e")
  (let ((string
	 (condition-case nil
	     (buffer-substring-no-properties (region-beginning) (region-end))
	   (error ""))))
    (x-set-selection 'CLIPBOARD string)))

(defun mac-handle-preferences (event)
  "Display the `Mac' customization group in response to EVENT."
  (interactive "e")
  (mac-start-animation (selected-window) :type 'swipe :duration 0.5)
  (customize-group 'mac))

(defun mac-handle-toolbar-pill-button-clicked (event)
  "Toggle visibility of tool-bars in response to EVENT.
With no keyboard modifiers, it toggles the visibility of the
frame where the tool-bar toggle button was pressed.  With some
modifiers, it changes the global tool-bar visibility setting."
  (interactive "e")
  (let ((ae (mac-event-ae event)))
    (if (mac-ae-keyboard-modifiers ae)
	;; Globally toggle tool-bar-mode if some modifier key is pressed.
	(tool-bar-mode 'toggle)
      (let ((frame (cdr (mac-ae-parameter ae 'frame))))
	(select-frame-set-input-focus frame)
	(set-frame-parameter frame 'tool-bar-lines
			     (if (= (frame-parameter frame 'tool-bar-lines) 0)
				 1 0))))))

(defun mac-handle-change-toolbar-display-mode (event)
  "Change tool bar style in response to EVENT."
  (interactive "e")
  (let* ((ae (mac-event-ae event))
	 (tag-data (cdr (mac-ae-parameter ae "tag")))
	 style)
    (when (eq (car tag-data) 'number)
      (setq style (cond ((= (cdr tag-data) 1) ; NSToolbarDisplayModeIconAndLabel
			 'both)
			((= (cdr tag-data) 2) ; NSToolbarDisplayModeIconOnly
			 'image)
			((= (cdr tag-data) 3) ; NSToolbarDisplayModeLabelOnly
			 'text)))
      (setq tool-bar-style style)
      (force-mode-line-update t))))

;; Currently, this is called only when the zoom button is clicked
;; while the frame is in fullwidth/fullheight/maximized.
(defun mac-handle-zoom (event)
  "Cancel frame fullscreen status in response to EVENT."
  (interactive "e")
  (let ((ae (mac-event-ae event)))
    (let ((frame (cdr (mac-ae-parameter ae 'frame))))
      (set-frame-parameter frame 'fullscreen nil))))

(define-key mac-apple-event-map [action about] 'mac-handle-about)
(define-key mac-apple-event-map [action copy] 'mac-handle-copy)
(define-key mac-apple-event-map [action preferences] 'mac-handle-preferences)
(define-key mac-apple-event-map [action toolbar-pill-button-clicked]
 'mac-handle-toolbar-pill-button-clicked)
(define-key mac-apple-event-map [action change-toolbar-display-mode]
 'mac-handle-change-toolbar-display-mode)
(put 'change-toolbar-display-mode 'mac-action-key-paths '("tag"))
(define-key mac-apple-event-map [action zoom] 'mac-handle-zoom)

;;; Spotlight for Help (Mac OS X 10.6 and later)

(declare-function info-initialize "info" ())
(declare-function Info-find-file "info" (filename &optional noerror))
(declare-function Info-toc-build "info" (file))
(declare-function Info-virtual-index "info" (topic))

(defvar mac-help-topics)

(defun mac-setup-help-topics ()
  (unless mac-help-topics
    (require 'info)
    (info-initialize)
    (let ((filename (Info-find-file "Emacs" t)))
      (if (null filename)
	  (setq mac-help-topics t)
	(setq mac-help-topics
	      (mapcar (lambda (node-info)
			(encode-coding-string (car node-info) 'utf-8))
		      (Info-toc-build filename)))))))

(defun mac-handle-select-help-topic (event)
  (interactive "e")
  (let* ((ae (mac-event-ae event))
	 (tag-data (cdr (mac-ae-parameter ae "selectedHelpTopic"))))
    (if (eq (car tag-data) 'string)
	(info (format "(Emacs)%s"
		      (decode-coding-string (cdr tag-data) 'utf-8))))))

(define-key mac-apple-event-map [action select-help-topic]
 'mac-handle-select-help-topic)
(put 'select-help-topic 'mac-action-key-paths '("selectedHelpTopic"))

(defun mac-handle-show-all-help-topics (event)
  (interactive "e")
  (let* ((ae (mac-event-ae event))
	 (tag-data (cdr (mac-ae-parameter ae "searchStringForAllHelpTopics"))))
    (when (eq (car tag-data) 'string)
      (info "Emacs")
      (Info-virtual-index (decode-coding-string (cdr tag-data) 'utf-8)))))

(define-key mac-apple-event-map [action show-all-help-topics]
 'mac-handle-show-all-help-topics)
(put 'show-all-help-topics 'mac-action-key-paths
     '("searchStringForAllHelpTopics"))

;;; Frame events
(defun mac-handle-modify-frame-parameters-event (event)
  "Modify frame parameters according to EVENT."
  (interactive "e")
  (let ((ae (mac-event-ae event)))
    (let ((frame (cdr (mac-ae-parameter ae 'frame)))
	  (alist (cdr (mac-ae-parameter ae 'alist))))
      (modify-frame-parameters frame alist))))

(define-key mac-apple-event-map [frame modify-frame-parameters]
 'mac-handle-modify-frame-parameters-event)

;;; Accessibility
(defun mac-ax-set-selected-text-range (event)
  (interactive "e")
  (let* ((ae (mac-event-ae event))
	 (tag-data (cdr (mac-ae-parameter ae)))
	 (window (cdr (mac-ae-parameter ae 'window))))
    (if (and (eq window (posn-window (event-start event)))
	     (eq (car tag-data) 'range))
	(goto-char (+ (point-min) (car (cdr tag-data)))))))

(defun mac-ax-set-visible-character-range (event)
  (interactive "e")
  (let* ((ae (mac-event-ae event))
	 (tag-data (cdr (mac-ae-parameter ae)))
	 (window (cdr (mac-ae-parameter ae 'window))))
    (if (and (eq window (posn-window (event-start event)))
	     (eq (car tag-data) 'range))
	(set-window-start window (+ (point-min) (car (cdr tag-data)))))))

(defun mac-ax-show-menu (event)
  "Pop up major mode menu near the point in response to accessibility EVENT."
  (interactive "e")
  (let* ((ae (mac-event-ae event))
	 (window (cdr (mac-ae-parameter ae 'window))))
    ;; Check if a different window is selected after the event
    ;; occurred.
    (if (eq window (posn-window (event-start event)))
	(let* ((posn (posn-at-point))
	       (x-y (posn-x-y posn))
	       (width-height (posn-object-width-height posn))
	       (edges (window-pixel-edges))
	       (inside-edges (window-inside-pixel-edges))
	       (xoffset (+ (- (car inside-edges) (car edges))
			   (car x-y)
			   (min (car width-height) (frame-char-width))))
	       (yoffset (+ (cdr x-y)
			   (min (cdr width-height) (frame-char-height)))))
	  (popup-menu (mouse-menu-major-mode-map)
		      (list (list xoffset yoffset) window))))))

(define-key mac-apple-event-map [accessibility set-selected-text-range]
  'mac-ax-set-selected-text-range)
(define-key mac-apple-event-map [accessibility set-visible-character-range]
  'mac-ax-set-visible-character-range)
(define-key mac-apple-event-map [accessibility show-menu]
  'mac-ax-show-menu)

;;; Services
(defun mac-service-open-file ()
  "Open the file specified by the selection value for Services."
  (interactive)
  ;; The selection seems not to contain the file name as
  ;; NSStringPboardType data on Mac OS X 10.4, when the selected items
  ;; are file icons.
  (let (data file-urls)
    (setq data
	  (condition-case nil
	      (x-get-selection mac-service-selection 'NSFilenamesPboardType)
	    (error nil)))
    (if data
	(setq file-urls
	      (mac-pasteboard-filenames-to-file-urls data)))
    (when (null file-urls)
      (setq data (condition-case nil
		     (x-get-selection mac-service-selection
				      'NSStringPboardType)
		   (error nil)))
      (when data
	(if (string-match "\\`[[:space:]\n]*[^[:space:]\n]" data)
	    (setq data (substring data (1- (match-end 0)))))
	(if (string-match "[^[:space:]\n][[:space:]\n]*\\'" data)
	    (setq data (substring data 0 (1+ (match-beginning 0)))))
	(when (file-name-absolute-p data)
	  (let ((filename (expand-file-name data "/"))
		(coding (or file-name-coding-system
			    default-file-name-coding-system)))
	    (unless (and (eq (aref data 0) ?~)
			 (string-match "\\`/~" filename))
	      (setq filename (encode-coding-string filename coding))
	      (setq file-urls
		    (list
		     (concat "file://localhost"
			     (mapconcat 'url-hexify-string
					(split-string filename "/") "/")))))))))
    (when file-urls
      (dolist (file-url file-urls)
	(dnd-open-file file-url nil))
      (select-frame-set-input-focus (selected-frame)))))

(defun mac-service-open-selection ()
  "Create a new buffer containing the selection value for Services."
  (interactive)
  (switch-to-buffer (generate-new-buffer "*untitled*"))
  (insert (x-selection-value-internal mac-service-selection))
  (sit-for 0)
  (save-buffer) ; It pops up the save dialog.
  )

(defun mac-service-mail-selection ()
  "Prepare a mail buffer containing the selection value for Services."
  (interactive)
  (compose-mail)
  (rfc822-goto-eoh)
  (forward-line 1)
  (insert (x-selection-value-internal mac-service-selection) "\n"))

(defun mac-service-mail-to ()
  "Prepare a mail buffer to be sent to the selection value for Services."
  (interactive)
  (compose-mail (x-selection-value-internal mac-service-selection)))

(defun mac-service-insert-text ()
  "Insert the selection value for Services."
  (interactive)
  (let ((text (x-selection-value-internal mac-service-selection)))
    (if (not buffer-read-only)
	(insert text)
      (kill-new text)
      (message "%s"
       (substitute-command-keys
	"The text from the Services menu can be accessed with \\[yank]")))))

;; kEventClassService/kEventServicePaste
(define-key mac-apple-event-map [service paste] 'mac-service-insert-text)
;; kEventClassService/kEventServicePerform
(define-key mac-apple-event-map [service perform open-file]
  'mac-service-open-file)
(define-key mac-apple-event-map [service perform open-selection]
  'mac-service-open-selection)
(define-key mac-apple-event-map [service perform mail-selection]
  'mac-service-mail-selection)
(define-key mac-apple-event-map [service perform mail-to]
  'mac-service-mail-to)

(declare-function mac-ae-set-reply-parameter "macselect.c"
		  (apple-event keyword descriptor))

(defun mac-dispatch-apple-event (event)
  "Dispatch EVENT according to the keymap `mac-apple-event-map'."
  (interactive "e")
  (let* ((binding (lookup-key mac-apple-event-map (mac-event-spec event)))
	 (ae (mac-event-ae event))
	 (service-message (and (keymapp binding)
			       (cdr (mac-ae-parameter ae "svmg")))))
    (when service-message
      (setq service-message
	    (intern (decode-coding-string service-message 'utf-8)))
      (setq binding (lookup-key binding (vector service-message))))
    ;; Replace (cadr event) with a dummy position so that event-start
    ;; returns it.
    (setcar (cdr event) (list (selected-window) (point) '(0 . 0) 0))
    (if (null (mac-ae-parameter ae 'emacs-suspension-id))
	(command-execute binding nil (vector event) t)
      (condition-case err
	  (progn
	    (command-execute binding nil (vector event) t)
	    (mac-resume-apple-event ae))
	(error
	 (mac-ae-set-reply-parameter ae "errs"
				     (cons "TEXT" (error-message-string err)))
	 (mac-resume-apple-event ae -10000)))))) ; errAEEventFailed

(define-key special-event-map [mac-apple-event] 'mac-dispatch-apple-event)


;;;; Drag and drop

(defcustom mac-dnd-types-alist
  '(("NSFilenamesPboardType" . mac-dnd-handle-pasteboard-filenames)
					; NSFilenamesPboardType
    ("NSStringPboardType" . mac-dnd-insert-pasteboard-string)
					; NSStringPboardType
    ("NeXT TIFF v4.0 pasteboard type" . mac-dnd-insert-TIFF) ; NSTIFFPboardType
    )
  "Which function to call to handle a drop of that type.
The function takes three arguments, WINDOW, ACTION and DATA.
WINDOW is where the drop occurred, ACTION is always `private' on
Mac.  DATA is the drop data.  Unlike the x-dnd counterpart, the
return value of the function is not significant.

See also `mac-dnd-known-types'."
  :version "22.1"
  :type 'alist
  :group 'mac)

(defun mac-dnd-handle-pasteboard-filenames (window action data)
  (dolist (file-url (mac-pasteboard-filenames-to-file-urls data))
    (dnd-handle-one-url window action (dnd-get-local-file-uri file-url))))

(defun mac-dnd-insert-TIFF (window action data)
  (dnd-insert-text window action (mac-TIFF-to-string data)))

(defun mac-dnd-insert-pasteboard-string (window action data)
  (dnd-insert-text window action (mac-pasteboard-string-to-string data)))

(defun mac-dnd-drop-data (event frame window data type &optional action)
  (or action (setq action 'private))
  (let* ((type-info (assoc type mac-dnd-types-alist))
	 (handler (cdr type-info))
	 (w (posn-window (event-start event))))
    (when handler
      (if (and (window-live-p w)
	       (not (window-minibuffer-p w))
	       (not (window-dedicated-p w)))
	  ;; If dropping in an ordinary window which we could use,
	  ;; let dnd-open-file-other-window specify what to do.
	  (progn
	    (when (not mouse-yank-at-point)
	      (goto-char (posn-point (event-start event))))
	    (funcall handler window action data))
	;; If we can't display the file here,
	;; make a new window for it.
	(let ((dnd-open-file-other-window t))
	  (select-frame frame)
	  (funcall handler window action data))))))

(defun mac-dnd-handle-drag-n-drop-event (event)
  "Receive drag and drop events."
  (interactive "e")
  (let ((window (posn-window (event-start event)))
	(ae (mac-event-ae event))
	action)
    (when (windowp window) (select-window window))
    ;; NSPasteboard-style drag-n-drop event of the form:
    ;; (:type TYPE-STRING :actions ACTION-LIST :data OBJECT)
    (let ((type (plist-get ae :type))
	  (actions (plist-get ae :actions))
	  (data (plist-get ae :data)))
      (if (and (not (memq 'generic actions)) (memq 'copy actions))
	  (setq action 'copy))
      (mac-dnd-drop-data event (selected-frame) window data type action))))


(declare-function accelerate-menu "macmenu.c" (&optional frame) t)

(defun mac-menu-bar-open (&optional frame)
  "Open the menu bar if `menu-bar-mode' is on, otherwise call `tmm-menubar'."
  (interactive "i")
  (if (and menu-bar-mode
	   (fboundp 'accelerate-menu))
      (accelerate-menu frame)
    (tmm-menubar)))


;;; Mouse wheel smooth scroll

(defcustom mac-mouse-wheel-smooth-scroll t
  "Non-nil means the mouse wheel should scroll by pixel unit if possible."
  :group 'mac
  :type 'boolean)

(defvar mac-ignore-momentum-wheel-events)

(defun mac-mwheel-scroll (event)
  "Scroll up or down according to the EVENT.
Mostly like `mwheel-scroll', but try scrolling by pixel unit if
EVENT has no modifier keys, `mac-mouse-wheel-smooth-scroll' is
non-nil, and the input device supports it."
  (interactive (list last-input-event))
  (setq mac-ignore-momentum-wheel-events nil)
  ;; (nth 3 event) is a list of the following form:
  ;; (isDirectionInvertedFromDevice	; nil (normal) or t (inverted)
  ;;  (deltaX deltaY deltaZ)		; floats
  ;;  (scrollingDeltaX scrollingDeltaY) ; nil or floats
  ;;  (phase momentumPhase)		; nil, nil and an integer, or integers
  ;;  )
  ;; The list might end early if the remaining elements are all nil.
  ;; TODO: horizontal scrolling
  (if (not (memq (event-basic-type event) '(wheel-up wheel-down)))
      (when (and (memq (event-basic-type event) '(wheel-left wheel-right))
		 (eq (nth 1 (nth 3 (nth 3 event))) 1)) ;; NSEventPhaseBegan
	;; Post a swipe event when the momentum phase begins for
	;; horizontal wheel events.
	(setq mac-ignore-momentum-wheel-events t)
	(push (cons
	       (event-convert-list
		(nconc (delq 'click
			     (delq 'double
				   (delq 'triple (event-modifiers event))))
		       (if (eq (event-basic-type event) 'wheel-left)
			   '(swipe-left) '(swipe-right))))
	       (cdr event))
	      unread-command-events))
    (if (or (not mac-mouse-wheel-smooth-scroll)
	    (delq 'click (delq 'double (delq 'triple (event-modifiers event))))
	    (null (nth 1 (nth 2 (nth 3 event)))))
	(if (or (null (nth 3 event))
		(and (/= (nth 1 (nth 1 (nth 3 event))) 0.0)
		     (= (or (nth 1 (nth 3 (nth 3 event))) 0) 0)))
	    (mwheel-scroll event))
      ;; TODO: ignore momentum scroll events after buffer switch.
      (let* ((window-to-scroll (if mouse-wheel-follow-mouse
				   (posn-window (event-start event))))
	     (edges (window-inside-pixel-edges window-to-scroll))
	     (window-inside-pixel-height (- (nth 3 edges) (nth 1 edges)))
	     ;; Do redisplay and measure line heights before selecting
	     ;; the window to scroll.
	     (point-height
	      (or (progn
		    (redisplay t)
		    (window-line-height nil window-to-scroll))
		  ;; The above still sometimes return nil.
		  (progn
		    (redisplay t)
		    (window-line-height nil window-to-scroll))))
	     (header-line-height
	      (window-line-height 'header-line window-to-scroll))
	     (first-height (window-line-height 0 window-to-scroll))
	     (last-height (window-line-height -1 window-to-scroll))
	     ;; Now select the window to scroll.
	     (curwin (if window-to-scroll
			 (prog1
			     (selected-window)
			   (select-window window-to-scroll))))
	     (buffer (window-buffer curwin))
	     (opoint (with-current-buffer buffer
		       (when (eq (car-safe transient-mark-mode) 'only)
			 (point))))
	     (first-y (or (car header-line-height) 0))
	     (first-height-sans-hidden-top
	      (cond ((= (car first-height) 0) ; completely invisible.
		     0)
		    ((null header-line-height) ; no header line.
		     (+ (car first-height) (nth 3 first-height)))
		    (t	  ; might be partly hidden by the header line.
		     (+ (- (car first-height)
			   (- (car header-line-height) (nth 2 first-height)))
			(nth 3 first-height)))))
	     (scroll-amount (nth 3 event))
	     (delta-y (- (round (nth 1 (nth 2 scroll-amount)))))
	     (scroll-conservatively 0)
	     scroll-preserve-screen-position
	     auto-window-vscroll
	     redisplay-needed)
	(unwind-protect
	    ;; Check if it is sufficient to adjust window's vscroll
	    ;; value.  Because the vscroll value might be non-zero at
	    ;; this stage, `window-start' might return an invisible
	    ;; position (e.g., "*About GNU Emacs*" buffer.), and thus
	    ;; we cannot rely on `pos-visible-in-window-p' until we
	    ;; reset the vscroll value.  That's why we used
	    ;; `redisplay' followed by `window-line-height' above.
	    (if (cond ((< delta-y 0)	; scroll down
		       (and
			(> first-height-sans-hidden-top 0)
			;; First row has enough room?
			(<= (- (nth 2 first-height) first-y)
			    delta-y)
			(or
			 ;; Cursor is still fully visible if scrolled?
			 (<= (- (nth 2 point-height) delta-y)
			     (- (+ first-y window-inside-pixel-height)
				(frame-char-height)))
			 ;; Window has the only one row?
			 (= (nth 2 first-height) (nth 2 last-height)))))
		      ((> delta-y 0) ; scroll up
		       (and
			(> first-height-sans-hidden-top 0)
			;; First row has enough room?
			(< delta-y first-height-sans-hidden-top)
			(or
			 (and
			  ;; Cursor is still fully visible if scrolled?
			  (< (nth 2 first-height) (nth 2 point-height))
			  (not (and (eobp) (bolp)
				    ;; Cursor is at the second row?
				    (= (+ first-y first-height-sans-hidden-top)
				       (nth 2 point-height)))))
			 (and (>= (- first-height-sans-hidden-top delta-y)
				  (frame-char-height))
			      (> (window-vscroll nil t) 0))
			 ;; Window has the only one row?
			 (= (nth 2 first-height) (nth 2 last-height)))))
		      (t
		       t))
		(set-window-vscroll nil (+ (window-vscroll nil t) delta-y) t)
	      (when (> (window-vscroll nil t) 0)
		(setq delta-y (- delta-y first-height-sans-hidden-top))
		(condition-case nil
		    ;; XXX: may recenter around the end of buffer.
		    (scroll-up 1)
		  (end-of-buffer
		   (set-window-vscroll nil 0 t)
		   (condition-case nil
		       (funcall mwheel-scroll-up-function 1)
		     (end-of-buffer))
		   (setq delta-y 0)))
		(set-window-vscroll nil 0 t))
	      (condition-case nil
		  (if (< delta-y 0)
		      ;; Scroll down
		      (let (prev-first prev-point prev-first-y)
			(while (< delta-y 0)
			  ;; It might be the case that `point-min' is
			  ;; visible but `window-start' < `point-min'.
			  (if (pos-visible-in-window-p (point-min) nil t)
			      (signal 'beginning-of-buffer nil))
			  (setq prev-first (window-start))
			  (setq prev-point (point))
			  (let ((prev-first-prev-y
				 (cadr (pos-visible-in-window-p prev-first
								nil t))))
			    (scroll-down)
			    (while (null (setq prev-first-y
					       (cadr (pos-visible-in-window-p
						      prev-first nil t))))
			      ;; If the previous last position gets
			      ;; invisible after scroll down, scroll
			      ;; up by line and try again.
			      (scroll-up 1))
			    (when (= prev-first-y prev-first-prev-y)
			      ;; It might be the case that `point-min'
			      ;; is visible but `window-start' <
			      ;; `point-min'.
			      (if (pos-visible-in-window-p (point-min) nil t)
				  (signal 'beginning-of-buffer nil))
			      ;; We came back to the original
			      ;; position.  We were at a line that is
			      ;; taller than the window height before
			      ;; the last scroll up.
			      (scroll-down 1)
			      (while (not (pos-visible-in-window-p
					   prev-first nil t))
				(set-window-vscroll
				 nil (+ (window-vscroll nil t)
					window-inside-pixel-height) t))
			      (setq prev-first-y
				    (+ (window-vscroll nil t)
				       (cadr (pos-visible-in-window-p
					      prev-first nil t))))
			      (set-window-vscroll nil 0 t))
			    (setq delta-y
				  (+ delta-y
				     (- prev-first-y prev-first-prev-y)))))
			(if (>= delta-y window-inside-pixel-height)
			    (progn
			      ;; Cancel the last scroll.
			      (goto-char prev-point)
			      (set-window-start nil prev-first))
			  (let* ((target-posn
				  (posn-at-x-y 0 (+ first-y delta-y)))
				 (target-row
				  (cdr (posn-actual-col-row target-posn)))
				 (target-y
				  (cadr (pos-visible-in-window-p
					 (posn-point target-posn) nil t)))
				 (scrolled-pixel-height (- target-y first-y))
				 (prev-first-row
				  (cdr (posn-actual-col-row
					(posn-at-x-y 0 prev-first-y)))))
			    ;; Cancel the last scroll.
			    (goto-char prev-point)
			    (set-window-start nil prev-first)
			    (scroll-down (- prev-first-row target-row
					    (if (= delta-y
						   scrolled-pixel-height)
						0 1)))
			    (setq delta-y (- delta-y scrolled-pixel-height)))))
		    ;; Scroll up
		    (let (found)
		      (while (and (not found)
				  (>= delta-y window-inside-pixel-height))
			(let* ((prev-first-prev-coord
				(pos-visible-in-window-p (window-start) nil t))
			       (prev-last-prev-coord
				(pos-visible-in-window-p t nil t))
			       (prev-last-prev-posn
				(posn-at-x-y (car prev-last-prev-coord)
					     (max (cadr prev-last-prev-coord)
						  first-y)))
			       (prev-last
				(posn-point prev-last-prev-posn))
			       (prev-last-prev-y
				(cadr prev-last-prev-coord))
			       prev-last-y prev-first)
			  (scroll-up)
			  (while (null (setq prev-last-y
					     (cadr (pos-visible-in-window-p
						    prev-last nil t))))
			    ;; If the previous last position gets
			    ;; invisible after scroll up, scroll down
			    ;; by line and try again.
			    (setq prev-first (window-start))
			    (scroll-down 1))
			  (if (/= prev-last-y prev-last-prev-y)
			      (setq delta-y
				    (- delta-y
				       (- prev-last-prev-y prev-last-y)))
			    ;; We came back to the original position.
			    ;; We are at a line that is taller than
			    ;; the window height.
			    (while (and (>= (- delta-y (window-vscroll nil t))
					    window-inside-pixel-height)
					(not (pos-visible-in-window-p
					      prev-first nil t)))
			      (set-window-vscroll
			       nil (+ (window-vscroll nil t)
				      window-inside-pixel-height)
			       t))
			    (let ((prev-first-y (cadr (pos-visible-in-window-p
						       prev-first nil t))))
			      (if (not (and prev-first-y
					    (>= (- delta-y
						   (window-vscroll nil t))
						(- prev-first-y first-y))))
				  (progn
				    (setq found t)
				    (set-window-vscroll nil 0 t))
				(setq delta-y
				      (- delta-y
					 (+ (window-vscroll nil t)
					    (- prev-first-y first-y))))
				(set-window-vscroll nil 0 t)
				(scroll-up 1)))))))
		    (if (< delta-y window-inside-pixel-height)
			(let* ((target-posn
				(posn-at-x-y 0 (+ first-y delta-y)))
			       (target-row
				(cdr (posn-actual-col-row target-posn)))
			       (target-y
				(cadr (pos-visible-in-window-p
				       (posn-point target-posn) nil t)))
			       (scrolled-pixel-height (- target-y first-y)))
			  (scroll-up (if (= delta-y scrolled-pixel-height)
					 target-row
				       (1+ target-row)))
			  (setq delta-y (- delta-y scrolled-pixel-height)))))
		(beginning-of-buffer
		 (condition-case nil
		     (funcall mwheel-scroll-down-function 1)
		   (beginning-of-buffer))
		 (setq delta-y 0))
		(end-of-buffer
		 (condition-case nil
		     (funcall mwheel-scroll-up-function 1)
		   (end-of-buffer))
		 (setq delta-y 0)))
	      (when (> delta-y 0)
		(scroll-down 1)
		(set-window-vscroll nil delta-y t)
		(setq redisplay-needed t))
	      (if (> (count-screen-lines (window-start) (window-end)) 1)
		  ;; Make sure that the cursor is fully visible.
		  (while (and (< (window-start) (point))
			      (not (pos-visible-in-window-p)))
		    (vertical-motion -1))))
	  (if curwin (select-window curwin))
	  (if redisplay-needed
	      (let ((mac-redisplay-dont-reset-vscroll t))
		(redisplay))))
	;; If there is a temporarily active region, deactivate it iff
	;; scrolling moves point.
	(when opoint
	  (with-current-buffer buffer
	    (when (/= opoint (point))
	      (deactivate-mark))))))))

(defvar mac-mwheel-installed-bindings nil)

(define-minor-mode mac-mouse-wheel-mode
  "Toggle mouse wheel support with smooth scroll.
With prefix argument ARG, turn on if positive, otherwise off.
Return non-nil if the new state is enabled."
  :init-value nil
  :global t
  :group 'mac
  ;; Remove previous bindings, if any.
  (while mac-mwheel-installed-bindings
    (let ((key (pop mac-mwheel-installed-bindings)))
      (when (eq (lookup-key (current-global-map) key) 'mac-mwheel-scroll)
        (global-unset-key key))))
  ;; Setup bindings as needed.
  (when mac-mouse-wheel-mode
    (dolist (event '(wheel-down wheel-up wheel-left wheel-right))
      (dolist (key (mapcar (lambda (amt) `[(,@(if (consp amt) (car amt)) ,event)])
                           mouse-wheel-scroll-amount))
        (global-set-key key 'mac-mwheel-scroll)
	(push key mac-mwheel-installed-bindings)))))


;;; Swipe events
(declare-function mac-start-animation "macfns.c"
		  (frame-or-window &rest properties))

(defun mac-previous-buffer (event)
  "Like `previous-buffer', but operate on the window where EVENT occurred."
  (interactive "e")
  (let ((window (posn-window (event-start event))))
    (mac-start-animation window :type 'move-out :direction 'right)
    (with-selected-window window
      (previous-buffer))))

(defun mac-next-buffer (event)
  "Like `next-buffer', but operate on the window where EVENT occurred."
  (interactive "e")
  (let ((window (posn-window (event-start event))))
    (mac-start-animation window :type 'none)
    (with-selected-window window
      (next-buffer))
    (mac-start-animation window :type 'move-in :direction 'left)))

(global-set-key [swipe-left] 'mac-previous-buffer)
(global-set-key [swipe-right] 'mac-next-buffer)


;;; Trackpad events
(defvar text-scale-mode-step) ;; in face-remap.el
(defvar text-scale-mode-amount) ;; in face-remap.el

(defvar mac-text-scale-magnification 1.0
  "Magnification value for text scaling.
The variable `text-scale-mode-amount' is set to the rounded
logarithm of this value using the value of `text-scale-mode-step'
as base.")
(make-variable-buffer-local 'mac-text-scale-magnification)

(defcustom mac-text-scale-magnification-range '(0.1 . 20.0)
  "Pair of mininum and maximum values for `mac-text-scale-magnification'."
  :type '(cons number number)
  :group 'mac)

(defcustom mac-text-scale-standard-width 80
  "Number of columns window occupies in standard buffer text scaling.
On Mac OS X 10.7 and later, you can change buffer text scaling to
the standard state by double-tapping either a touch-sensitive
mouse with one finger or a trackpad with two fingers if the
buffer is previously unscaled.  In this standard buffer text
scaling state, the default font is scaled so at least this number
of columns can be shown in the tapped window."
  :type 'integer
  :group 'mac)

(defun mac-magnify-text-scale (event)
  "Magnify the height of the default face in the buffer where EVENT happened.
The actual magnification is performed by `text-scale-mode'."
  (interactive "e")
  (require 'face-remap)
  (let ((original-selected-window (selected-window)))
    (with-selected-window (posn-window (event-start event))
      (let ((magnification (car (nth 3 event)))
	    (level
	     (round (log mac-text-scale-magnification text-scale-mode-step))))
	(if (= magnification 0.0)
	    ;; This is double-tapping a mouse with a finger or
	    ;; double-tapping a trackpad with two fingers on Mac OS X
	    ;; 10.7
	    (if (/= level 0)
		(setq mac-text-scale-magnification 1.0)
	      (setq mac-text-scale-magnification
		    (expt text-scale-mode-step
			  (floor (log (/ (window-width)
					 (float mac-text-scale-standard-width))
				      text-scale-mode-step)))))
	  (if (/= level text-scale-mode-amount)
	      (setq mac-text-scale-magnification
		    (expt text-scale-mode-step text-scale-mode-amount)))
	  (setq mac-text-scale-magnification
		(* mac-text-scale-magnification (+ 1.0 magnification))))
	(setq mac-text-scale-magnification
	      (min (max mac-text-scale-magnification
			(car mac-text-scale-magnification-range))
		   (cdr mac-text-scale-magnification-range)))
	(setq level
	      (round (log mac-text-scale-magnification text-scale-mode-step)))
	(if (/= level text-scale-mode-amount)
	    (let ((inc (if (< level text-scale-mode-amount) -1 1)))
	      (unwind-protect
		  (while (and (not (input-pending-p))
			      (/= text-scale-mode-amount level))
		    (text-scale-increase inc)
		    (with-selected-window original-selected-window
		      (sit-for 0.017)))
		(if (/= text-scale-mode-amount level)
		    (text-scale-set level)))))))))

(defun mac-mouse-turn-on-fullscreen (event)
  "Turn on fullscreen in response to the mouse event EVENT."
  (interactive "e")
  (let ((frame (window-frame (posn-window (event-start event)))))
    (if (not (eq (frame-parameter frame 'fullscreen) 'fullboth))
	(set-frame-parameter frame 'fullscreen 'fullboth))))

(defun mac-mouse-turn-off-fullscreen (event)
  "Turn off fullscreen in response to the mouse event EVENT."
  (interactive "e")
  (let ((frame (window-frame (posn-window (event-start event)))))
    (if (frame-parameter frame 'fullscreen)
	(set-frame-parameter frame 'fullscreen nil))))

(global-set-key [magnify-up] 'mac-magnify-text-scale)
(global-set-key [magnify-down] 'mac-magnify-text-scale)
(global-set-key [S-magnify-up] 'mac-mouse-turn-on-fullscreen)
(global-set-key [S-magnify-down] 'mac-mouse-turn-off-fullscreen)
(global-set-key [rotate-left] 'ignore)
(global-set-key [rotate-right] 'ignore)
(global-set-key [S-rotate-left] 'ignore)
(global-set-key [S-rotate-right] 'ignore)


;;; Window system initialization.

(defun x-win-suspend-error ()
  (error "Suspending an Emacs running under Mac makes no sense"))

(defvar mac-initialized nil
  "Non-nil if the Mac window system has been initialized.")

(declare-function x-open-connection "macfns.c"
		  (display &optional xrm-string must-succeed))
(declare-function x-get-resource "frame.c"
		  (attribute class &optional component subclass))
(declare-function x-parse-geometry "frame.c" (string))

(defun mac-handle-args (args)
  "Process the Mac-related command line options in ARGS.
It records Mac-specific options in `mac-startup-options' as a
value for the key symbol `command-line' after processing the
standard ones in `x-handle-args'."
  (setq args (x-handle-args args))
  (let ((invocation-args args)
	mac-specific-args)
    (setq args nil)
    (while (and invocation-args
		(not (equal (car invocation-args) "--")))
      (let ((this-switch (car invocation-args))
	    case-fold-search)
	(setq invocation-args (cdr invocation-args))
	(cond ((string-match "\\`-psn_" this-switch)
	       (push this-switch mac-specific-args))
	      ;; Cocoa applications let you set temporary preferences
	      ;; on the command line.  We regard option names starting
	      ;; with a capital letter and containing at least 2
	      ;; letters as such preference settings.
	      ((and (string-match "\\`-[A-Z]." this-switch)
		    invocation-args
		    (not (string-match "\\`-" (car invocation-args))))
	       (setq mac-specific-args
		     (cons (car invocation-args)
			   (cons this-switch mac-specific-args)))
	       (setq invocation-args (cdr invocation-args)))
	      (t
	       (setq args (cons this-switch args))))))
    (when mac-specific-args
      (setq mac-specific-args (nreverse mac-specific-args))
      (push (cons 'command-line mac-specific-args) mac-startup-options))
    (nconc (nreverse args) invocation-args)))

(defun mac-handle-startup-keyboard-modifiers ()
  (let ((modifiers-value (cdr (assq 'keyboard-modifiers mac-startup-options)))
	(shift-mask (cdr (assq 'shift mac-keyboard-modifier-mask-alist))))
    (if (and modifiers-value (/= (logand modifiers-value shift-mask) 0))
	;; Shift modifier at startup means "minimum customizations"
	;; like `-Q' command-line option.
	(setq init-file-user nil
	      site-run-file nil
	      inhibit-x-resources t))))

(defun mac-exit-splash-screen ()
  "Stop displaying the splash screen buffer with possibly an animation."
  (interactive)
  (mac-start-animation (selected-window) :type 'fade-out)
  (exit-splash-screen))

(defun mac-initialize-window-system ()
  "Initialize Emacs for Mac GUI frames."
  ;; Make sure we have a valid resource name.
  (or (stringp x-resource-name)
      (let (i)
	(setq x-resource-name (invocation-name))

	;; Change any . or * characters in x-resource-name to hyphens,
	;; so as not to choke when we use it in X resource queries.
	(while (setq i (string-match "[.*]" x-resource-name))
	  (aset x-resource-name i ?-))))

  (x-open-connection "Mac"
		     x-command-line-resources
		     ;; Exit Emacs with fatal error if this fails.
		     t)

  (mac-handle-startup-keyboard-modifiers)

  ;; Create the default fontset.
  (create-default-fontset)

  (set-fontset-font t nil (font-spec :family "Apple Symbols") nil 'prepend)
  (if (string-match "darwin[1-9][1-9]" system-configuration)
      ;; Built on Mac OS X 10.7 or later.
      (set-fontset-font t nil (font-spec :family "Apple Color Emoji")
			nil 'prepend))
  ;; (set-fontset-font t nil (font-spec :family "LastResort") nil 'append)
  (set-fontset-font t '(#x20000 . #x2FFFF)
		    '("HanaMinB" . "unicode-sip") nil 'append)

  ;; Create fontset specified in X resources "Fontset-N" (N is 0, 1, ...).
  (create-fontset-from-x-resource)

  ;; Apply a geometry resource to the initial frame.  Put it at the end
  ;; of the alist, so that anything specified on the command line takes
  ;; precedence.
  (let* ((res-geometry (x-get-resource "geometry" "Geometry"))
	 parsed)
    (if res-geometry
	(progn
	  (setq parsed (x-parse-geometry res-geometry))
	  ;; If the resource specifies a position,
	  ;; call the position and size "user-specified".
	  (if (or (assq 'top parsed) (assq 'left parsed))
	      (setq parsed (cons '(user-position . t)
				 (cons '(user-size . t) parsed))))
	  ;; All geometry parms apply to the initial frame.
	  (setq initial-frame-alist (append initial-frame-alist parsed))
	  ;; The size parms apply to all frames.  Don't set it if there are
	  ;; sizes there already (from command line).
	  (if (and (assq 'height parsed)
		   (not (assq 'height default-frame-alist)))
	      (setq default-frame-alist
		    (cons (cons 'height (cdr (assq 'height parsed)))
			  default-frame-alist)))
	  (if (and (assq 'width parsed)
		   (not (assq 'width default-frame-alist)))
	      (setq default-frame-alist
		    (cons (cons 'width (cdr (assq 'width parsed)))
			  default-frame-alist))))))

  ;; Check the reverseVideo resource.
  (let ((case-fold-search t))
    (let ((rv (x-get-resource "reverseVideo" "ReverseVideo")))
      (if (and rv
	       (string-match "^\\(true\\|yes\\|on\\)$" rv))
	  (setq default-frame-alist
		(cons '(reverse . t) default-frame-alist)))))

  ;; Don't let Emacs suspend under Mac.
  (add-hook 'suspend-hook 'x-win-suspend-error)

  ;; Turn off window-splitting optimization; Mac is usually fast enough
  ;; that this is only annoying.
  (setq split-window-keep-point t)

  ;; Enable CLIPBOARD copy/paste through menu bar commands.
  (menu-bar-enable-clipboard)

  (mac-setup-system-coding-system)
  (mac-setup-selection-properties)

  ;; Processing of Apple events are deferred at the startup time.  For
  ;; example, files dropped onto the Emacs application icon can only
  ;; be processed when the initial frame has been created: this is
  ;; where the files should be opened.
  (add-hook 'after-init-hook 'mac-process-deferred-apple-events)
  (run-with-idle-timer 5 t 'mac-cleanup-expired-apple-events)

  ;; If Emacs is invoked from the command line, the initial frame
  ;; doesn't get focused.
  (add-hook 'after-init-hook
	    (lambda () (if (and (let ((first-option
				       (cadr (assq 'command-line
						   mac-startup-options))))
				  (not (and first-option
					    (string-match "\\`-psn_"
							  first-option))))
				(eq (frame-visible-p (selected-frame)) t))
			   (x-focus-frame (selected-frame)))))

  (add-hook 'after-init-hook
	    (lambda ()
	      (mouse-wheel-mode 0)
	      (mac-mouse-wheel-mode 1)))

  (add-hook 'menu-bar-update-hook 'mac-setup-help-topics)

  (substitute-key-definition 'exit-splash-screen 'mac-exit-splash-screen
			     splash-screen-keymap)

  (setq mac-initialized t))

(add-to-list 'handle-args-function-alist '(mac . mac-handle-args))
(add-to-list 'frame-creation-function-alist '(mac . x-create-frame-with-faces))
(add-to-list 'window-system-initialization-alist '(mac . mac-initialize-window-system))

;; Initiate drag and drop
(define-key special-event-map [drag-n-drop] 'mac-dnd-handle-drag-n-drop-event)

(provide 'mac-win)

;;; mac-win.el ends here
