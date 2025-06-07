from collections import defaultdict
import pandas as pd
import matplotlib.pyplot as plt
import re

# File mapping
file_label_map = {
    "baseline_async.csv": "Baseline (Async)",
    "colocated_async.csv": "Colocated (Async)",
    "not-colocated_async.csv": "Not Colocated (Async)"
}

file_paths = list(file_label_map.keys())

# Regex patterns
metric_patterns = {
    "User Time (s)": r"user_time_s = ([\d.]+)",
    "System Time (s)": r"sys_time_s = ([\d.]+)"
}

# Role detection
def detect_role(line):
    if "client" in line or "wasm_rpc_host_container" in line:
        return "Client"
    elif "server" in line:
        return "Server"
    else:
        return None

# Collect per-run metrics
records = []

for file_path in file_paths:
    df = pd.read_csv(file_path)
    config_label = file_label_map[file_path]

    for run_id in df["Run"].unique():
        run_df = df[df["Run"] == run_id]
        user_time = 0.0
        system_time = 0.0

        for line in run_df["Metric"]:
            for metric, pattern in metric_patterns.items():
                match = re.search(pattern, line)
                if match:
                    value = float(match.group(1))
                    if metric == "User Time (s)":
                        user_time += value
                    elif metric == "System Time (s)":
                        system_time += value

        records.append({
            "Configuration": config_label,
            "Run": run_id,
            "User Time (s)": user_time,
            "System Time (s)": system_time
        })

# Convert to DataFrame
df_all = pd.DataFrame(records)

# Compute average per configuration
df_mean = df_all.groupby("Configuration")[["User Time (s)", "System Time (s)"]].mean()
df_mean.index.name = "Configuration"

# Plot average values
ax = df_mean.plot(
    kind="bar",
    figsize=(10, 6),
    colormap="tab10",
    title="User and System Time by Configuration (Average of 5 Runs)",
    ylabel="Time (s)",
    xlabel="Configuration",
    rot=15,
    width=0.7
)

# Add value labels on top of bars
for container in ax.containers:
    ax.bar_label(container, fmt="%.2f", label_type="edge", fontsize=8, padding=3)

plt.tight_layout()
plt.savefig("user_system_time_average.png", dpi=300)
plt.clf()
