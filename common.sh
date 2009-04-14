#!/bin/bash

set -e

function unload_module
{
	local module="$1"

	if [ -n "$(lsmod | grep $module)" ]; then
		rmmod $module
	fi
}

function load_module
{
	local module="$1"

	if [ -z "$(lsmod | grep $module)" ]; then
		modprobe $module
	fi
}

if [ -n "$LOAD_KDEBRICK" ]; then
	if [ -z "$(lsmod | grep kdebrick)" ]; then
		load_module parport
		load_module parport_pc
		unload_module lp
		unload_module ppdev
		insmod ./kernel/kdebrick.ko
		sleep 1
	fi
	args="$args --kdebrick"
else
	if [ -z "$(lsmod | grep ppdev)" ]; then
		load_module parport
		load_module parport_pc
		unload_module kdebrick
		load_module ppdev
		sleep 1
	fi
fi
