#!/bin/bash

output_file="metrics/throughput_rtt_batch_log.txt"
runs=10  # Number of test runs

mkdir -p metrics
echo "Run,Timestamp,Throughput_μs,AverageRTT_μs" > "$output_file"

for ((i=1; i<=runs; i++)); do
    timestamp=$(date +"%Y-%m-%dT%H:%M:%S")
    echo "=== Run $i at $timestamp ==="

    # Run container and capture output
    output=$(docker compose up --build wasm_rpc_host --abort-on-container-exit 2>&1)
    docker compose down --volumes --remove-orphans

    # --- Extract Throughput ---
    throughput_line=$(echo "$output" | grep "\[METRICS\] THROUGHPUT =")
    if [[ -n "$throughput_line" ]]; then
        throughput_val=$(echo "$throughput_line" | sed 's/.*THROUGHPUT = //' | sed 's/μs//')
    else
        throughput_val="N/A"
        echo "⚠️  No throughput metric found in run $i"
    fi

    # --- Extract RTT values and compute average ---
    rtt_lines=$(echo "$output" | grep "\[METRICS\] RTT =")
    total_rtt=0
    rtt_count=0
    while IFS= read -r rtt_line; do
        val=$(echo "$rtt_line" | sed 's/.*RTT = //' | sed 's/μs//')
        total_rtt=$((total_rtt + val))
        ((rtt_count++))
    done <<< "$rtt_lines"

    if [[ $rtt_count -gt 0 ]]; then
        avg_rtt=$(awk "BEGIN { printf \"%.2f\", $total_rtt / $rtt_count }")
    else
        avg_rtt="N/A"
        echo "⚠️  No RTT metrics found in run $i"
    fi

    # --- Write result ---
    echo "$i,$timestamp,$throughput_val,$avg_rtt" >> "$output_file"
done

echo "Batch complete. Logged to $output_file"
