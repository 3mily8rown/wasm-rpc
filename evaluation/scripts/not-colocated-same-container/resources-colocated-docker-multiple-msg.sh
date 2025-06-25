#!/bin/bash

output_file="data/resources/resources_log_same_vm_multiple_msg.csv"
echo "Run,Timestamp,Metric" > "$output_file"

# Single run
run_id=1
timestamp=$(date +"%Y-%m-%dT%H:%M:%S")
echo "Starting single run at $timestamp"

# Run host container and capture output
output=$(docker compose up --build server_client_same_vm --abort-on-container-exit 2>&1)

docker compose down --volumes --remove-orphans

# Extract and log [METRICS] lines from host output
echo "$output" | grep "\[METRICS\]" | while IFS= read -r line; do
    metric_only=$(echo "$line" | sed 's/\[METRICS\] //')
    echo "$run_id,$timestamp,$metric_only" >> "$output_file"
done

echo "Single run complete. Metrics logged to $output_file"