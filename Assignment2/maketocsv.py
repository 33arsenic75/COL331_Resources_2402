import re
import csv

with open('final.txt', 'r') as f:
    data = f.read()

# Split data by experiment
experiments = re.split(r"=== EXP (\d+) \| BETA = (\d+) ===", data)[1:]  # skip initial empty
rows = []

for i in range(0, len(experiments), 3):
    exp_num = int(experiments[i])
    beta = int(experiments[i + 1])
    content = experiments[i + 2]

    # Extract process blocks
    pids = re.findall(r"PID: (\d+).*?TAT: (\d+).*?WT: (\d+).*?RT: (\d+).*?#CS: (\d+)", content, re.DOTALL)
    
    # In order: CPU, IO, Parent — assuming fixed positions
    if len(pids) == 3:
        cpu, io, parent = pids
        rows.append({
            "EXP": exp_num,
            "BETA": beta,
            "CPU_TAT": cpu[1],
            "CPU_WT": cpu[2],
            "CPU_RT": cpu[3],
            "CPU_CS": cpu[4],
            "IO_TAT": io[1],
            "IO_WT": io[2],
            "IO_RT": io[3],
            "IO_CS": io[4],
            "Parent_TAT": parent[1],
            "Parent_WT": parent[2],
            "Parent_RT": parent[3],
            "Parent_CS": parent[4],
        })

# Write to CSV
with open('results.csv', 'w', newline='') as f:
    writer = csv.DictWriter(f, fieldnames=rows[0].keys())
    writer.writeheader()
    writer.writerows(rows)

print("✅ Parsed and saved as results.csv")
