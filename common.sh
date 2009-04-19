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

if [ -r "user.sh" ]; then
	. user.sh
fi

if [ -z "$LOAD_KDEBRICK" -o "$LOAD_KDEBRICK" = "0" ]; then
	if [ -z "$(lsmod | grep ppdev)" ]; then
		load_module parport
		load_module parport_pc
		unload_module kdebrick
		load_module ppdev
		sleep 1
	fi
else
	if [ -z "$(lsmod | grep kdebrick)" ]; then
		load_module parport
		load_module parport_pc
		unload_module lp
		unload_module ppdev
		insmod ./kernel/kdebrick.ko
		sleep 1
	fi
	args="$args --kdebrick"
fi
