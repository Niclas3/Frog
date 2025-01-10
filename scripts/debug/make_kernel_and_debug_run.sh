#!/bin/bash
make clean-all && 
make newimg &&
make newhd80img && #FIX me free lfb <-- this is a bug 
make debug_run
