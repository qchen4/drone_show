#!/bin/sh
bindir=$(pwd)
cd /home/hice1/qchen438/6122/ogl-master/tutorial09_vbo_indexing/
export 

if test "x$1" = "x--debugger"; then
	shift
	if test "xYES" = "xYES"; then
		echo "r  " > $bindir/gdbscript
		echo "bt" >> $bindir/gdbscript
		/usr/bin/gdb -batch -command=$bindir/gdbscript --return-child-result /home/hice1/qchen438/6122/ogl-master/pace-ice-build/tutorial09_vbo_indexing 
	else
		"/home/hice1/qchen438/6122/ogl-master/pace-ice-build/tutorial09_vbo_indexing"  
	fi
else
	"/home/hice1/qchen438/6122/ogl-master/pace-ice-build/tutorial09_vbo_indexing"  
fi
