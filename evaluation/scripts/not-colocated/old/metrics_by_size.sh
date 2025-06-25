#!/bin/bash

output_file="metrics/rtt_throughput_by_size.csv"
mkdir -p metrics
echo "Size,AverageRTT_us,Throughput_us" > "$output_file"

# Define message sizes
for size in 8 16 32 64 128 256 512 1024; do
    echo "=== Testing message size $size ==="

    export TEST_MSG_SIZE=$size  # If your app uses this env var

    # Run the containers and collect output
    output=$(docker compose up --build server client --abort-on-container-exit 2>&1)
    docker compose down --volumes --remove-orphans

    # Extract RTTs
    rtt_lines=$(echo "$output" | grep "\[METRICS\] RTT")
    total_rtt=0
    count=0

    while IFS= read -r line; do
        rtt=$(echo "$line" | sed -E 's/.*RTT = ([0-9]+)μs.*/\1/')
        total_rtt=$((total_rtt + rtt))
        count=$((count + 1))
    done <<< "$rtt_lines"

    if [ "$count" -gt 0 ]; then
        avg_rtt=$(awk "BEGIN { printf \"%.2f\", $total_rtt / $count }")
    else
        avg_rtt="N/A"
    fi

    # Extract last THROUGHPUT line
    throughput=$(echo "$output" | grep "\[METRICS\] THROUGHPUT" | tail -n 1 | sed -E 's/.*THROUGHPUT = ([0-9]+)μs.*/\1/')
    throughput=${throughput:-N/A}

    echo "$size,$avg_rtt,$throughput" >> "$output_file"
    echo "→ Size $size done: Avg RTT = $avg_rtt μs, Throughput = $throughput μs"
done

echo "✅ All sizes tested. Results saved to $output_file"
