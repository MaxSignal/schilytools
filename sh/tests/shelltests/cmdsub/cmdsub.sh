#! /bin/sh
#
# @(#)cmdsub.sh	1.3 16/06/04 Copyright 2016 J. Schilling
#

# Read test core functions
. ../../common/test-common

docommand A1 "$SHELL cmdsub-A1" 0 "x\n" ""
docommand A2 "$SHELL cmdsub-A2" 0 "x\n" ""
docommand A3 "$SHELL cmdsub-A3" 0 "x\n" ""
docommand A4 "$SHELL cmdsub-A4" 0 "x\n" ""
docommand A5 "$SHELL cmdsub-A5" 0 "x\n" ""

docommand B "$SHELL cmdsub-B" 0 "quoted )\n" ""

docommand C "$SHELL cmdsub-C" 0 "comment\n" ""

docommand D1 "$SHELL cmdsub-D1" 0 "here-doc with )\n" ""
docommand D2 "$SHELL cmdsub-D2" 2 "" IGNORE
docommand D3 "$SHELL cmdsub-D3" 0 "here-doc with \\()\n" ""

docommand E "$SHELL cmdsub-E" 0 "here-doc terminated with a parenthesis\n" ""

docommand F "$SHELL cmdsub-F" 0 "' # or a single back- or doublequote\n" ""

docommand G "$SHELL cmdsub-G" 0 "\n" ""

docommand H "$SHELL cmdsub-H" 0 "\n" ""

#
# This command substitution is used by Sven Maschek in "whatshell.sh"
#
docommand cmdsub01 "$SHELL -c 'case \$( (:^times) 2>&1) in *0m*) echo OK;; *) echo FAIL;; esac'" 0 "OK\n" ""

success