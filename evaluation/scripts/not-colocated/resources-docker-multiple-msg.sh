#!/bin/bash

output_file="data/resources/resources_log_multiple_msg.csv"
echo "Run,Timestamp,Metric" > "$output_file"

# Single run
run_id=1
timestamp=$(date +"%Y-%m-%dT%H:%M:%S")
echo "Starting single run at $timestamp"

# # Run host container and capture output
# output=$(docker compose up --build server client --abort-on-container-exit 2>&1)

# docker compose down --volumes --remove-orphans

# # Extract and log [METRICS] lines from host output
# echo "$output" | grep "\[METRICS\]" | while IFS= read -r line; do
#     metric_only=$(echo "$line" | sed 's/\[METRICS\] //')
#     echo "$run_id,$timestamp,$metric_only" >> "$output_file"
# done

# echo "Single run complete. Metrics logged to $output_file"

# Run docker compose and wait
docker compose up --build server client --abort-on-container-exit > compose_output.txt 2>&1

# Now collect container logs before they're removed
declare -A id_to_name
for container_id in $(docker ps -a -q --filter "name=server_container" --filter "name=client_container"); do
    name=$(docker inspect --format='{{.Name}}' "$container_id" | sed 's|/||')
    id_to_name[$container_id]=$name
done

# Extract and save metrics
for container_id in "${!id_to_name[@]}"; do
    name=${id_to_name[$container_id]}
    docker logs "$container_id" | grep "\[METRICS\]" | while IFS= read -r line; do
        metric_only=$(echo "$line" | sed 's/\[METRICS\] //')
        echo "$run_id,$timestamp,$name $metric_only" >> "$output_file"
    done
done

# Now shut down
docker compose down --volumes --remove-orphans

