from collections import defaultdict
import pandas as pd
import matplotlib.pyplot as plt
import re

# File mapping
file_label_map = {
    "baseline_async.csv": "Baseline (Async)",
    "not-colocated_async.csv": "Not Colocated (Async)"
}

file_paths = list(file_label_map.keys())

# Metric patterns
metric_patterns = {
    "User Time": r"user_time_s = ([\d.]+)",
    "System Time": r"sys_time_s = ([\d.]+)"
}

# Role detection
def detect_role(line):
    if "client" in line:
        return "Client"
    elif "server" in line:
        return "Server"
    else:
        return None

# Store all individual runs: list of dicts
records = []

for file_path in file_paths:
    df = pd.read_csv(file_path)
    label = file_label_map[file_path]

    for run_id in df["Run"].unique():
        run_df = df[df["Run"] == run_id]
        run_metrics = {
            "Configuration": label,
            "Run": run_id,
            "User Time (Client)": 0.0,
            "System Time (Client)": 0.0,
            "User Time (Server)": 0.0,
            "System Time (Server)": 0.0
        }

        for line in run_df["Metric"]:
            role = detect_role(line)
            if role is None:
                continue
            for metric_base, pattern in metric_patterns.items():
                match = re.search(pattern, line)
                if match:
                    value = float(match.group(1))
                    key = f"{metric_base} ({role})"
                    run_metrics[key] += value

        records.append(run_metrics)

# Create DataFrame from all runs
df_all = pd.DataFrame(records)

# Average over runs
df_avg = df_all.groupby("Configuration").mean().round(2)

# Plot
ax = df_avg.plot(
    kind="bar",
    figsize=(10, 6),
    colormap="tab10",
    ylabel="Time (s)",
    xlabel="Configuration",
    rot=15,
    width=0.7
)

# Title and grid
plt.title("Average User and System Time (Client & Server) over 5 Runs", pad=20)
plt.grid(axis='y', linestyle='--', alpha=0.7)

# Label bars
for container in ax.containers:
    ax.bar_label(container, fmt='%.2f', label_type='edge', fontsize=8, padding=3)

# Legend outside
plt.legend(
    loc='upper left',
    bbox_to_anchor=(1.0, 1.0),
    fontsize='small',
    frameon=True,
    framealpha=0.9,
    edgecolor='gray'
)

plt.tight_layout(rect=[0, 0, 0.85, 1])
plt.savefig("user_sys_client_server_avg_5runs.png", dpi=300)
plt.clf()
