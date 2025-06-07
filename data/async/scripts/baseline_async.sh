#!/bin/bash

output_file="data/async/baseline_async.csv"
num_runs=5  # Set how many times to run the benchmark

# Initialize CSV header
echo "Run,Timestamp,Metric" > "$output_file"

for run_id in $(seq 1 $num_runs); do
    timestamp=$(date +"%Y-%m-%dT%H:%M:%S")
    echo "Starting run $run_id at $timestamp"

    # Run the benchmark
    docker compose up --build cpp_server cpp_client --abort-on-container-exit > compose_output.txt 2>&1

    # Get container names before removal
    declare -A id_to_name
    for container_id in $(docker ps -a -q --filter "name=cpp_server_container" --filter "name=cpp_client_container"); do
        name=$(docker inspect --format='{{.Name}}' "$container_id" | sed 's|/||')
        id_to_name[$container_id]=$name
    done

    # Extract metrics from logs
    for container_id in "${!id_to_name[@]}"; do
        name=${id_to_name[$container_id]}
        docker logs "$container_id" | grep "\[METRICS\]" | while IFS= read -r line; do
            metric_only=$(echo "$line" | sed 's/\[METRICS\] //')
            echo "$run_id,$timestamp,$name $metric_only" >> "$output_file"
        done
    done

    # Cleanup
    docker compose down --volumes --remove-orphans

    echo "Run $run_id complete"
    sleep 2  # Optional: wait before next run
done
