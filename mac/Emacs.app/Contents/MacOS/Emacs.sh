#!/bin/bash
### Emacs.sh - GNU Emacs Mac port startup script.

## Copyright (C) 2012-2014  YAMAMOTO Mitsuharu

## This file is part of GNU Emacs Mac port.

## GNU Emacs Mac port is free software: you can redistribute it and/or modify
## it under the terms of the GNU General Public License as published by
## the Free Software Foundation, either version 3 of the License, or
## (at your option) any later version.

## GNU Emacs Mac port is distributed in the hope that it will be useful,
## but WITHOUT ANY WARRANTY; without even the implied warranty of
## MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
## GNU General Public License for more details.

## You should have received a copy of the GNU General Public License
## along with GNU Emacs.  If not, see <http://www.gnu.org/licenses/>.

### Code:

filename="$(basename "$0")"
filename="$(dirname "$0")/${filename%.sh}"

case $PWD in
    /)
	# As of OS X 10.8, this is always the case if invoked from the
	# launch service.  Just in case this behavior is changed on
	# future versions...
	cd
	case $filename in
	    /*) ;;
	    *) filename=/"$filename" ;;
	esac
	;;
esac

set "$filename" "$@"

case $(sw_vers -productVersion) in
    10.[0-7]|10.[0-7].*)
	# "$HOME/.MacOSX/environment.plist" is ignored on OS X 10.8.
	if [ -f "$HOME/.MacOSX/environment.plist" ]; then
	    # Invocation via "Login Items" or "Resume" resets PATH.
	    case ${SHLVL} in
		1)
		    p="$(defaults read "$HOME/.MacOSX/environment" PATH 2>/dev/null)" && PATH="$p"
		    ;;
	    esac
	    exec "$@"
	fi
	;;
esac

case ${SHLVL} in
    1) ;;
    *) exec "$@" ;;
esac

case $(basename "${SHELL}") in
    bash)	exec -l "${SHELL}" --login -c 'exec "$@"' - "$@" ;;
    ksh|sh|zsh)	exec -l "${SHELL}" -c 'exec "$@"' - "$@" ;;
    csh|tcsh)	exec -l "${SHELL}" -c 'exec $argv:q' "$@" ;;
    *)		exec "$@" ;;
esac
