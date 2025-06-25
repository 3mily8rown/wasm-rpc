#!/bin/bash

output_file="data/localVsRemote/round_trip_log_colocated_multiple_msg.csv"
echo "Run,Timestamp,Metric" > "$output_file"

# Single run
run_id=1
timestamp=$(date +"%Y-%m-%dT%H:%M:%S")
echo "Starting single run at $timestamp"

# Run host container and capture output
output=$(docker compose up --build wasm_rpc_host --abort-on-container-exit 2>&1)

docker compose down --volumes --remove-orphans

# Extract and log [METRICS] lines from host output
echo "$output" | grep "\[METRICS\]" | while IFS= read -r line; do
    metric_only=$(echo "$line" | sed 's/\[METRICS\] //')
    echo "$run_id,$timestamp,$metric_only" >> "$output_file"
done

echo "Single run complete. Metrics logged to $output_file"

# Compute and print/append the average RTT
awk -F, '
    NR>1 {
        sub(/.*RTT = /, "", $3)
        sub(/μs.*/, "", $3)
        sum += $3
        count++
    }
    END {
        if (count > 0) {
            avg = sum / count
            printf "Total measurements: %d\nAverage RTT: %.2f μs\n", count, avg
        } else {
            print "No RTT metrics found to average."
        }
    }
' "$output_file" | tee -a "$output_file"
