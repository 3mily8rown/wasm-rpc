#!/bin/bash

output_csv="metrics/throughput_vs_size.csv"
mkdir -p metrics
echo "Run,SizeBytes,Throughput_us,Average_RTT_us" > "$output_csv"

# Message size doubles each run starting from 8 bytes
size=8
run_id=1

# Loop through sizes (adjust max as needed)
for i in {1..8}; do
    echo "=== Run $run_id | Size: ${size} bytes ==="

    # Set env var or file the WASM reads (assumes you use it to set size)
    export MESSAGE_SIZE=$size

    # Run and capture logs
    log=$(docker compose up --build wasm_rpc_host --abort-on-container-exit 2>&1)
    docker compose down --volumes --remove-orphans

    # Extract RTTs
    rtts=($(echo "$log" | grep "\[METRICS\] RTT" | sed -E 's/.*RTT = ([0-9]+)μs/\1/'))
    total_rtt=0
    count=0
    for rtt in "${rtts[@]}"; do
        total_rtt=$((total_rtt + rtt))
        count=$((count + 1))
    done
    avg_rtt=0
    if [ $count -gt 0 ]; then
        avg_rtt=$((total_rtt / count))
    fi

    # Extract last THROUGHPUT for that run
    throughput=$(echo "$log" | grep "\[METRICS\] THROUGHPUT" | tail -n 1 | sed -E 's/.*THROUGHPUT = ([0-9]+)μs/\1/')

    # Log to CSV
    echo "$run_id,$size,$throughput,$avg_rtt" >> "$output_csv"

    # Increment
    size=$((size * 2))
    run_id=$((run_id + 1))
done

echo "✅ Metrics written to $output_csv"
