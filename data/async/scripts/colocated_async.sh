#!/bin/bash

output_file="data/async/colocated_async.csv"
num_runs=5  # Change this to however many runs you want

# Initialize the CSV header
echo "Run,Timestamp,Metric" > "$output_file"

for run_id in $(seq 1 $num_runs); do
    timestamp=$(date +"%Y-%m-%dT%H:%M:%S")
    echo "Starting run $run_id at $timestamp"

    # Run the host container and capture all logs
    output=$(docker compose up --build wasm_rpc_host --abort-on-container-exit 2>&1)

    # Tear down the container environment
    docker compose down --volumes --remove-orphans

    # Extract and write metrics
    echo "$output" | grep "\[METRICS\]" | while IFS= read -r line; do
        metric_only=$(echo "$line" | sed 's/\[METRICS\] //')
        echo "$run_id,$timestamp,$metric_only" >> "$output_file"
    done

    echo "Run $run_id complete."
    sleep 2  # Optional delay between runs
done

echo "All $num_runs runs complete. Metrics saved to $output_file"
