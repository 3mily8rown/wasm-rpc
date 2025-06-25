import pandas as pd
import matplotlib.pyplot as plt
import re

metrics_path = 'metrics/done/'
plot_path = 'evaluation/plotting/plots'

# filename = 'round_trip_log_colocated_docker_singleMessage_aot_2core'
# filename = 'round_trip_log_colocated_docker_singleMessage_wasm_2core'
filename = 'round_trip_log_colocated_docker_singleMessage_wasm_3core'
# filename = 'round_trip_log_colocated_docker_singleMessage_aot'
# filename = 'round_trip_log_colocated_docker_singleMessage_wasm'
with open(metrics_path + filename + '.txt', 'r') as f:
    lines = f.readlines()

# Filter only lines that have 3 comma-separated values
data_lines = [line for line in lines if line.count(',') == 2]

# Create a temporary in-memory CSV-style string
from io import StringIO
cleaned_data = StringIO(''.join(data_lines))

# Load it with pandas
df = pd.read_csv(cleaned_data)

# Extract RTT in microseconds using regex
df['RTT_μs'] = df['Metric'].apply(lambda x: int(re.search(r'RTT\s*=\s*(\d+)μs', x).group(1)))

# Convert Timestamp to datetime
df['Timestamp'] = pd.to_datetime(df['Timestamp'])

# Plot RTT over Run
plt.figure(figsize=(10, 6))
plt.plot(df['Run'], df['RTT_μs'], marker='o', linestyle='-', markersize=4)
plt.xlabel('Run')
plt.ylabel('RTT (μs)')
plt.title('RTT over Run')
plt.grid(True)
plt.tight_layout()

# Save the plot
plt.savefig(plot_path + filename + '.png', dpi=300)

# Optionally display it
plt.show()
