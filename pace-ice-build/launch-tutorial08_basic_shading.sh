#!/bin/sh
bindir=$(pwd)
cd /home/hice1/qchen438/6122/ogl-master/tutorial08_basic_shading/
export 

if test "x$1" = "x--debugger"; then
	shift
	if test "xYES" = "xYES"; then
		echo "r  " > $bindir/gdbscript
		echo "bt" >> $bindir/gdbscript
		/usr/bin/gdb -batch -command=$bindir/gdbscript --return-child-result /home/hice1/qchen438/6122/ogl-master/pace-ice-build/tutorial08_basic_shading 
	else
		"/home/hice1/qchen438/6122/ogl-master/pace-ice-build/tutorial08_basic_shading"  
	fi
else
	"/home/hice1/qchen438/6122/ogl-master/pace-ice-build/tutorial08_basic_shading"  
fi
