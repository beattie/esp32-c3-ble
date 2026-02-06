#!/bin/bash
# Start OpenOCD and wait until the GDB port is ready.
# Used as a GDB shell command so F5 "just works".

# Kill any previous instance
pkill -f "openocd.*esp32c3" 2>/dev/null
sleep 0.5

# Clear old log
> /tmp/openocd.log

# Start OpenOCD detached from this shell
nohup openocd -c "set ESP_RTOS none" -f board/esp32c3-builtin.cfg >>/tmp/openocd.log 2>&1 &
OPENOCD_PID=$!
disown $OPENOCD_PID

# Wait up to 10 seconds for GDB port to be ready (check log, not TCP)
for i in $(seq 1 50); do
    if ! kill -0 $OPENOCD_PID 2>/dev/null; then
        echo "ERROR: OpenOCD (pid $OPENOCD_PID) died. Log:"
        cat /tmp/openocd.log
        exit 1
    fi
    if grep -q 'Listening on port 3333' /tmp/openocd.log 2>/dev/null; then
        sleep 0.3
        echo "OpenOCD ready (pid $OPENOCD_PID)"
        exit 0
    fi
    sleep 0.2
done

echo "ERROR: OpenOCD timed out. Log:"
cat /tmp/openocd.log
kill $OPENOCD_PID 2>/dev/null
exit 1
