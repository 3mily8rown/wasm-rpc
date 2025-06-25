#!/bin/bash

output_file="metrics/round_trip_cpp_log.txt"
echo "Run,Timestamp,Metric" > "$output_file"

num_runs=10  # Set how many runs you want

for ((i=1; i<=num_runs; i++)); do
    timestamp=$(date +"%Y-%m-%dT%H:%M:%S")
    echo "Starting run $i at $timestamp"

    # Start the native server in the background
    ./build/native_server &
    server_pid=$!

    # Wait for the server to start listening
    sleep 1

    # Run the client and capture its output
    output=$(./build/native_client 2>&1)

    # Kill the server if it's still alive
    if kill -0 "$server_pid" 2>/dev/null; then
        kill "$server_pid"
        wait "$server_pid" 2>/dev/null
    fi

    # Extract [METRICS] line(s) from client output
    metric_lines=$(echo "$output" | grep "\[METRICS\]")
    while IFS= read -r line; do
        metric_only=$(echo "$line" | sed 's/\[METRICS\] //')
        echo "$i,$timestamp,$metric_only" >> "$output_file"
    done <<< "$metric_lines"

    echo "Run $i complete."
done

echo "✅ All runs complete. Metrics logged to $output_file"
