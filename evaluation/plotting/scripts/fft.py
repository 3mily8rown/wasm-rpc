import pandas as pd
import matplotlib.pyplot as plt
import numpy as np
import re
from io import StringIO

# --- Load and clean your RTT data from file ---
with open('metrics/done/round_trip_log_colocated_docker_singleMessage_wasm.txt', 'r') as f:
    lines = f.readlines()

data_lines = [line for line in lines if line.count(',') == 2]
df = pd.read_csv(StringIO(''.join(data_lines)))
df['RTT_μs'] = df['Metric'].apply(lambda x: int(re.search(r'RTT\s*=\s*(\d+)μs', x).group(1)))

# --- 1. Autocorrelation plot ---
from pandas.plotting import autocorrelation_plot
plt.figure(figsize=(10, 4))
autocorrelation_plot(df['RTT_μs'])
plt.title("Autocorrelation of RTT")
plt.tight_layout()
plt.savefig('evaluation/plotting/plots/rtt_autocorrelation.png')
plt.show()

# --- 2. FFT (frequency domain) analysis ---
rtt_array = df['RTT_μs'].values
fft_vals = np.fft.fft(rtt_array - np.mean(rtt_array))  # Remove DC offset
fft_freq = np.fft.fftfreq(len(rtt_array))

# Only use the positive half of frequencies
positive_freqs = fft_freq > 0
plt.figure(figsize=(10, 4))
plt.plot(fft_freq[positive_freqs], np.abs(fft_vals)[positive_freqs])
plt.xlabel('Frequency (1/run)')
plt.ylabel('Magnitude')
plt.title('FFT of RTT Signal')
plt.tight_layout()
plt.savefig('evaluation/plotting/plots/rtt_fft.png')
plt.show()
