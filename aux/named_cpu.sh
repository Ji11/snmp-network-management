#!/bin/sh
# Return CPU time (cputime) of the named process
PID=$(pidof named 2>/dev/null)
if [ -n "$PID" ]; then
    ps -o cputime= -p "$PID" 2>/dev/null | tr -d '[:space:]' || echo "0"
else
    echo "0"
fi
