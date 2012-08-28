### mac.mk --- src/Makefile fragment for GNU Emacs Mac port

## Copyright (C) 2012  YAMAMOTO Mitsuharu

## This file is part of GNU Emacs Mac port.

## GNU Emacs Mac port is free software: you can redistribute it and/or modify
## it under the terms of the GNU General Public License as published by
## the Free Software Foundation, either version 3 of the License, or
## (at your option) any later version.
## 
## GNU Emacs Mac port is distributed in the hope that it will be useful,
## but WITHOUT ANY WARRANTY; without even the implied warranty of
## MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
## GNU General Public License for more details.
## 
## You should have received a copy of the GNU General Public License
## along with GNU Emacs Mac port.  If not, see <http://www.gnu.org/licenses/>.

### Commentary:

## This is inserted in src/Makefile if HAVE_MACGUI. 

mac = ../mac/
emacsapp = $(PWD)/$(mac)Emacs.app/
emacsappsrc = ${srcdir}/../mac/Emacs.app/

lprojdirs = ${emacsapp}Contents/Resources/English.lproj \
  ${emacsapp}Contents/Resources/Dutch.lproj \
  ${emacsapp}Contents/Resources/French.lproj \
  ${emacsapp}Contents/Resources/German.lproj \
  ${emacsapp}Contents/Resources/Italian.lproj \
  ${emacsapp}Contents/Resources/Japanese.lproj \
  ${emacsapp}Contents/Resources/Spanish.lproj \
  ${emacsapp}Contents/Resources/ar.lproj \
  ${emacsapp}Contents/Resources/cs.lproj \
  ${emacsapp}Contents/Resources/da.lproj \
  ${emacsapp}Contents/Resources/fi.lproj \
  ${emacsapp}Contents/Resources/hu.lproj \
  ${emacsapp}Contents/Resources/ko.lproj \
  ${emacsapp}Contents/Resources/no.lproj \
  ${emacsapp}Contents/Resources/pl.lproj \
  ${emacsapp}Contents/Resources/pt.lproj \
  ${emacsapp}Contents/Resources/pt_PT.lproj \
  ${emacsapp}Contents/Resources/ru.lproj \
  ${emacsapp}Contents/Resources/sv.lproj \
  ${emacsapp}Contents/Resources/tr.lproj \
  ${emacsapp}Contents/Resources/zh_CN.lproj \
  ${emacsapp}Contents/Resources/zh_TW.lproj

${lprojdirs}:
	mkdir -p $@

ifneq (${emacsapp},${emacsappsrc})
${emacsapp}Contents/MacOS/Emacs.sh: ${emacsappsrc}Contents/MacOS/Emacs.sh
	mkdir -p ${emacsapp}Contents/MacOS/;
	cp $< $@
${emacsapp}Contents/Info.plist: ${emacsappsrc}Contents/Info.plist
	cp $< $@
${emacsapp}Contents/PkgInfo: ${emacsappsrc}Contents/PkgInfo
	cp $< $@
${emacsapp}Contents/Resources/Emacs.icns: ${emacsappsrc}Contents/Resources/Emacs.icns
	mkdir -p ${emacsapp}Contents/Resources
	cp $< $@
${emacsapp}Contents/Resources/English.lproj/InfoPlist.strings: ${emacsappsrc}Contents/Resources/English.lproj/InfoPlist.strings
	cp $< $@
endif

macosx-bundle: ${lprojdirs} ${emacsapp}Contents/MacOS/Emacs.sh \
	${emacsapp}Contents/Info.plist ${emacsapp}Contents/PkgInfo \
	${emacsapp}Contents/Resources/Emacs.icns \
	${emacsapp}Contents/Resources/English.lproj/InfoPlist.strings
macosx-app: macosx-bundle ${emacsapp}Contents/MacOS/Emacs
${emacsapp}Contents/MacOS/Emacs: emacs${EXEEXT}
	cd ${emacsapp}Contents/MacOS/; cp ../../../../src/emacs${EXEEXT} Emacs

### mac.mk ends here
