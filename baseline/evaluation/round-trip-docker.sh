#!/bin/bash

output_file="metrics/round_trip_cpp_same_docker_log_xms_xms.txt"
echo "Run,Timestamp,Metric" > "$output_file"

num_runs=20

for ((i=1; i<=num_runs; i++)); do
    timestamp=$(date +"%Y-%m-%dT%H:%M:%S")
    echo "Starting run $i at $timestamp"

    # Run the containers (with tc simulated latency), capture output
    output=$(docker compose up --build --abort-on-container-exit cpp_server cpp_client 2>&1)

    # Clean up containers to reset for next run
    docker compose down --volumes --remove-orphans

    # Extract [METRICS] lines
    metric_lines=$(echo "$output" | grep "\[METRICS\]")
    while IFS= read -r line; do
        metric_only=$(echo "$line" | sed 's/\[METRICS\] //')
        echo "$i,$timestamp,$metric_only" >> "$output_file"
    done <<< "$metric_lines"

    echo "Run $i complete."
done

echo "✅ All runs complete. Metrics logged to $output_file"
