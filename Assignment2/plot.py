import pandas as pd
import matplotlib.pyplot as plt
import os

# Load the CSV
df = pd.read_csv('results.csv')

# Drop the 'EXP' column
df = df.drop(columns=['EXP'])

# Create the 'plots_2' folder if it doesn't exist
os.makedirs('plots_3', exist_ok=True)

for col in df.columns:
    if col != 'BETA':
        fig, (ax1, ax2, ax3) = plt.subplots(1, 3, sharey=True, figsize=(16, 6), gridspec_kw={'width_ratios': [1, 2, 3]})

        # BETA <= 100
        part1 = df[df['BETA'] <= 100].copy()
        part1[col] = part1[col].rolling(window=20).mean()
        ax1.plot(part1['BETA'], part1[col], marker='o')
        ax1.set_xlim(min(part1['BETA']), 100)
        ax1.set_xlabel('BETA')
        ax1.set_ylabel(col)
        ax1.set_title(f'BETA vs {col}')
        ax1.grid(True)

        # 1000 <= BETA < 27000
        part2 = df[(df['BETA'] >= 1000) & (df['BETA'] < 27000)].copy()
        part2[col] = part2[col].rolling(window=20).mean()
        ax2.plot(part2['BETA'], part2[col], marker='o')
        ax2.set_xlim(1000, 27000)
        ax2.set_xlabel('BETA')
        ax2.grid(True)

        # BETA >= 50000
        part3 = df[df['BETA'] >= 50000].copy()
        part3[col] = part3[col].rolling(window=20).mean()
        ax3.plot(part3['BETA'], part3[col], marker='o')
        ax3.set_xlim(50000, max(part3['BETA']))
        ax3.set_xlabel('BETA')
        ax3.grid(True)

        # Axis breaks
        d = .015  # size of diagonal lines

        kwargs = dict(transform=ax1.transAxes, color='k', clip_on=False)
        ax1.plot((1 - d, 1 + d), (-d, +d), **kwargs)
        ax1.plot((1 - d, 1 + d), (1 - d, 1 + d), **kwargs)

        kwargs.update(transform=ax2.transAxes)
        ax2.plot((-d, +d), (-d, +d), **kwargs)
        ax2.plot((-d, +d), (1 - d, 1 + d), **kwargs)

        kwargs = dict(transform=ax2.transAxes, color='k', clip_on=False)
        ax2.plot((1 - d, 1 + d), (-d, +d), **kwargs)
        ax2.plot((1 - d, 1 + d), (1 - d, 1 + d), **kwargs)

        kwargs.update(transform=ax3.transAxes)
        ax3.plot((-d, +d), (-d, +d), **kwargs)
        ax3.plot((-d, +d), (1 - d, 1 + d), **kwargs)

        plt.tight_layout()
        plt.savefig(f'plots_3/{col}.png')
        plt.close()
