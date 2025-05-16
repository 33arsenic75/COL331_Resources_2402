import pandas as pd
import seaborn as sns
import matplotlib.pyplot as plt

# Load CSV
df = pd.read_csv('results.csv')
df = df.drop(columns=['EXP'])

# Compute full correlation matrix (Pearson)
corr_matrix = df.corr(numeric_only=True)

# Print correlation matrix
print("📊 Full Pearson Correlation Matrix:\n")
print(corr_matrix)

# Optional: Save it as CSV
#corr_matrix.to_csv('correlation_matrix.csv')

# Optional: Plot it
plt.figure(figsize=(12, 10))
sns.heatmap(corr_matrix, annot=True, cmap='coolwarm', fmt=".2f", square=True)
plt.title("Correlation Matrix of All Numeric Fields")
plt.tight_layout()
plt.savefig("correlation_matrix_heatmap.png")
plt.show()
