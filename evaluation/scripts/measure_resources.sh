#!/bin/bash
TARGET="$1"
/usr/bin/time -v "$TARGET" 2>&1 | tee /tmp/time_output.txt

CONTAINER_NAME=$(hostname)

grep "Maximum resident set size" /tmp/time_output.txt | awk -v name="$CONTAINER_NAME" '{ print "[METRICS] " name " max_rss_kb = " $NF }'
grep "User time (seconds)"       /tmp/time_output.txt | awk -v name="$CONTAINER_NAME" '{ print "[METRICS] " name " user_time_s = " $NF }'
grep "System time (seconds)"     /tmp/time_output.txt | awk -v name="$CONTAINER_NAME" '{ print "[METRICS] " name " sys_time_s = " $NF }'

# #!/bin/bash
# TARGET="$1"
# shift

# CONTAINER_NAME=$(hostname)
# TMP_STDOUT="/tmp/target_output.txt"
# SMAPS_OUT="/tmp/smaps_dump.txt"
# PMAP_OUT="/tmp/pmap_dump.txt"
# START_TIME=$(date +%s.%N)

# # Start target in background and grab PID
# "$TARGET" "$@" > "$TMP_STDOUT" 2>&1 &
# APP_PID=$!

# # Wait for it to actually start and allocate memory
# sleep 0.2

# # Sample memory while it's still alive
# if ps -p "$APP_PID" > /dev/null 2>&1; then
#     cp /proc/$APP_PID/smaps "$SMAPS_OUT"
#     pmap -x "$APP_PID" > "$PMAP_OUT"
#     RSS_KB=$(grep "VmRSS" /proc/$APP_PID/status | awk '{print $2}')
# else
#     RSS_KB=0
# fi

# # Wait for process to finish
# wait "$APP_PID"
# EXIT_CODE=$?
# END_TIME=$(date +%s.%N)
# RUNTIME=$(echo "$END_TIME - $START_TIME" | bc)

# echo "[METRICS] $CONTAINER_NAME runtime_s = $RUNTIME"
# echo "[METRICS] $CONTAINER_NAME exit_code = $EXIT_CODE"
# echo "[METRICS] $CONTAINER_NAME rss_kb_sampled = ${RSS_KB}"
# echo "[METRICS] $CONTAINER_NAME smaps_dump = $SMAPS_OUT"
# echo "[METRICS] $CONTAINER_NAME pmap_dump = $PMAP_OUT"

