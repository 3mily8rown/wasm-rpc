#!/bin/bash

output_file="data/async/not-colocated_async.csv"
num_runs=5

# Init output
echo "Run,Timestamp,Metric" > "$output_file"

for run_id in $(seq 1 $num_runs); do
    timestamp=$(date +"%Y-%m-%dT%H:%M:%S")
    echo "========== Run $run_id at $timestamp =========="

    # Clean up beforehand (in case anything is stuck)
    docker compose down --volumes --remove-orphans
    docker container prune -f

    # Run benchmark
    docker compose up --build server client --abort-on-container-exit > compose_output.txt 2>&1 &
    compose_pid=$!

    # Wait for docker compose to exit (in background)
    wait $compose_pid

    # Confirm exit
    echo "Compose finished. Collecting logs..."

    # Map container IDs
    declare -A id_to_name
    for container_id in $(docker ps -a -q --filter "name=server_container" --filter "name=client_container"); do
        name=$(docker inspect --format='{{.Name}}' "$container_id" | sed 's|/||')
        id_to_name[$container_id]=$name
    done

    # Extract logs
    for container_id in "${!id_to_name[@]}"; do
        name=${id_to_name[$container_id]}
        docker logs "$container_id" | grep "\[METRICS\]" | while IFS= read -r line; do
            metric_only=$(echo "$line" | sed 's/\[METRICS\] //')
            echo "$run_id,$timestamp,$name $metric_only" >> "$output_file"
        done
    done

    # Force cleanup
    docker compose down --volumes --remove-orphans
    docker container prune -f

    echo "Run $run_id complete."
    sleep 2
done

echo "All $num_runs runs complete. Metrics saved to $output_file"
