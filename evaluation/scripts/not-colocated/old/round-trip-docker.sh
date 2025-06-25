#!/bin/bash

output_file="metrics/round_trip_log_docker.txt"
echo "Run,Timestamp,Metric" > "$output_file"

num_runs=10  # Set how many runs you want

for ((i=1; i<=num_runs; i++)); do
    timestamp=$(date +"%Y-%m-%dT%H:%M:%S")
    echo "Starting run $i at $timestamp"

    # Run client and capture output
    output=$(docker compose up --build server client --abort-on-container-exit 2>&1)

    docker compose down --volumes --remove-orphans

    # Extract and log [METRICS] lines from client output
    metric_lines=$(echo "$output" | grep "\[METRICS\]")
    while IFS= read -r line; do
        metric_only=$(echo "$line" | sed 's/\[METRICS\] //')
        echo "$i,$timestamp,$metric_only" >> "$output_file"
    done <<< "$metric_lines"

    echo "Run $i complete."
done

echo "All runs complete. Metrics logged to $output_file"

# --------------------------------------------
# Compute and print/append the average RTT:
# --------------------------------------------
awk -F, '
    NR>1 {
        # $3 is like: "client_container  | RTT = 5245μs"
        sub(/.*RTT = /, "", $3)        # remove everything before “RTT = ”
        sub(/μs.*/, "", $3)           # remove trailing “μs”
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
