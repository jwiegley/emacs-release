;;; loaddefs.el --- automatically extracted autoloads
;;
;;; Code:


;;;### (autoloads (ede-project-sort-targets ede-customize-current-target
;;;;;;  ede-customize-project) "ede/custom" "custom.el" "ed4daba88d971827bf292ab1d90d0228")
;;; Generated autoloads from custom.el

(autoload 'ede-customize-project "ede/custom" "\
Edit fields of the current project through EIEIO & Custom.

\(fn)" t nil)

(defalias 'customize-project 'ede-customize-project)

(autoload 'ede-customize-current-target "ede/custom" "\
Edit fields of the current target through EIEIO & Custom.

\(fn)" t nil)

(defalias 'customize-target 'ede-customize-current-target)

(autoload 'ede-project-sort-targets "ede/custom" "\
Create a custom-like buffer for sorting targets of current project.

\(fn)" t nil)

;;;***

;;;### (autoloads (ede-cpp-root-project-root) "ede/cpp-root" "cpp-root.el"
;;;;;;  "363924d6f9d6c7c2b60270433cc92d08")
;;; Generated autoloads from cpp-root.el

(autoload 'ede-cpp-root-project-root "ede/cpp-root" "\
Get the root directory for DIR.

\(fn &optional DIR)" nil nil)

(add-to-list 'ede-project-class-files (ede-project-autoload "cpp-root" :name "CPP ROOT" :file 'ede/cpp-root :proj-file 'ede-cpp-root-project-file-for-dir :proj-root 'ede-cpp-root-project-root :load-type 'ede-cpp-root-load :class-sym 'ede-cpp-root :new-p nil) t)

;;;***

;;;### (autoloads (ede-emacs-project-root) "ede/emacs" "emacs.el"
;;;;;;  "9244f68928ca30fffd3250df72110b5c")
;;; Generated autoloads from emacs.el

(autoload 'ede-emacs-project-root "ede/emacs" "\
Get the root directory for DIR.

\(fn &optional DIR)" nil nil)

(add-to-list 'ede-project-class-files (ede-project-autoload "emacs" :name "EMACS ROOT" :file 'ede/emacs :proj-file "src/emacs.c" :proj-root 'ede-emacs-project-root :load-type 'ede-emacs-load :class-sym 'ede-emacs-project :new-p nil) t)

;;;***

;;;### (autoloads (ede-find-file) "ede/files" "files.el" "d7075d1c9311ee346169bd3a5c356e58")
;;; Generated autoloads from files.el

(autoload 'ede-find-file "ede/files" "\
Find FILE in project.  FILE can be specified without a directory.
There is no completion at the prompt.  FILE is searched for within
the current EDE project.

\(fn FILE)" t nil)

;;;***

;;;### (autoloads (ede-enable-generic-projects) "ede/generic" "generic.el"
;;;;;;  "4073988d456691c0caba912d0073630d")
;;; Generated autoloads from generic.el

(autoload 'ede-enable-generic-projects "ede/generic" "\
Enable generic project loaders.

\(fn)" t nil)

;;;***

;;;### (autoloads (ede-linux-project-root) "ede/linux" "linux.el"
;;;;;;  "f9da2f6cfe535e4a2f3d84e3607ef0f0")
;;; Generated autoloads from linux.el

(autoload 'ede-linux-project-root "ede/linux" "\
Get the root directory for DIR.

\(fn &optional DIR)" nil nil)

(add-to-list 'ede-project-class-files (ede-project-autoload "linux" :name "LINUX ROOT" :file 'ede/linux :proj-file "scripts/ver_linux" :proj-root 'ede-linux-project-root :load-type 'ede-linux-load :class-sym 'ede-linux-project :new-p nil) t)

;;;***

;;;### (autoloads (ede-enable-locate-on-project) "ede/locate" "locate.el"
;;;;;;  "2f0716f8a2cf316d9c10d32ec282d82c")
;;; Generated autoloads from locate.el

(autoload 'ede-enable-locate-on-project "ede/locate" "\
Enable an EDE locate feature on PROJECT.
Attempt to guess which project locate style to use
based on `ede-locate-setup-options'.

\(fn &optional PROJECT)" t nil)

;;;***

;;;### (autoloads (ede-make-check-version) "ede/make" "make.el" "871da41de6323f74b60f3f1193602b08")
;;; Generated autoloads from make.el

(autoload 'ede-make-check-version "ede/make" "\
Check the version of GNU Make installed.
The check passes if the MAKE version is no high enough, or if it
is not GNU make.
If NOERROR is non-nil, return t for success, nil for failure.
If NOERROR is nil, then throw an error on failure.  Return t otherwise.

\(fn &optional NOERROR)" t nil)

;;;***

;;;### (autoloads (ede-speedbar-file-setup) "ede/speedbar" "speedbar.el"
;;;;;;  "3abc0de29d513cc626bbe28f7ed76279")
;;; Generated autoloads from speedbar.el

(autoload 'ede-speedbar-file-setup "ede/speedbar" "\
Setup some keybindings in the Speedbar File display.

\(fn)" nil nil)

;;;***

;;;### (autoloads (ede-vc-project-directory ede-upload-html-documentation
;;;;;;  ede-upload-distribution ede-edit-web-page ede-web-browse-home)
;;;;;;  "ede/system" "system.el" "05f88141948fb083afedb8abf05e6fdb")
;;; Generated autoloads from system.el

(autoload 'ede-web-browse-home "ede/system" "\
Browse the home page of the current project.

\(fn)" t nil)

(autoload 'ede-edit-web-page "ede/system" "\
Edit the web site for this project.

\(fn)" t nil)

(autoload 'ede-upload-distribution "ede/system" "\
Upload the current distribution to the correct location.
Use /user@ftp.site.com: file names for FTP sites.
Download tramp, and use /r:machine: for names on remote sites w/out FTP access.

\(fn)" t nil)

(autoload 'ede-upload-html-documentation "ede/system" "\
Upload the current distributions documentation as HTML.
Use /user@ftp.site.com: file names for FTP sites.
Download tramp, and use /r:machine: for names on remote sites w/out FTP access.

\(fn)" t nil)

(autoload 'ede-vc-project-directory "ede/system" "\
Run `vc-dir' on the current project.

\(fn)" t nil)

;;;***

;;;### (autoloads (ede-update-version) "ede/util" "util.el" "624b54f348f8052d05babb76f4d2047a")
;;; Generated autoloads from util.el

(autoload 'ede-update-version "ede/util" "\
Update the current projects main version number.
Argument NEWVERSION is the version number to use in the current project.

\(fn NEWVERSION)" t nil)

;;;***

(provide 'loaddefs)
;; Local Variables:
;; version-control: never
;; no-byte-compile: t
;; no-update-autoloads: t
;; coding: utf-8
;; End:
;;; loaddefs.el ends here
