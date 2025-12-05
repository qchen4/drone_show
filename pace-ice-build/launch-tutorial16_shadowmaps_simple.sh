#!/bin/sh
bindir=$(pwd)
cd /home/hice1/qchen438/6122/ogl-master/tutorial16_shadowmaps/
export 

if test "x$1" = "x--debugger"; then
	shift
	if test "xYES" = "xYES"; then
		echo "r  " > $bindir/gdbscript
		echo "bt" >> $bindir/gdbscript
		/usr/bin/gdb -batch -command=$bindir/gdbscript --return-child-result /home/hice1/qchen438/6122/ogl-master/pace-ice-build/tutorial16_shadowmaps_simple 
	else
		"/home/hice1/qchen438/6122/ogl-master/pace-ice-build/tutorial16_shadowmaps_simple"  
	fi
else
	"/home/hice1/qchen438/6122/ogl-master/pace-ice-build/tutorial16_shadowmaps_simple"  
fi
