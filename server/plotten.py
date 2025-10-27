import numpy as np
import matplotlib.pyplot as plt
import csv

# CSV-Datei einlesen
filename = 'bins.csv'
data = []

with open(filename, newline='') as csvfile:
    reader = csv.reader(csvfile)
    for row in reader:
        if row:  # Zeile ist nicht leer
            try:
                data.append(double(row[0]))
            except ValueError:
                continue

# Lambda und Quantile
lambda_ = data[0]
quantiles = data[1:]
n = len(quantiles)

# x-Werte für die PDF
x_max = quantiles[-1] * 1.05  # etwas größer als das größte Quantil
x = np.linspace(0, x_max, 2000)
pdf = lambda_ * np.exp(-lambda_ * x)

# Farben in Lava-Abstufungen
colors = plt.cm.inferno(np.linspace(0.2, 1, n))

plt.figure(figsize=(8,5))
plt.plot(x, pdf, color='black', label='Exponential PDF')

x_start = 0
for q, c in zip(quantiles, colors):
    mask = (x >= x_start) & (x <= q)
    if np.any(mask):
        plt.fill_between(x[mask], pdf[mask], color=c, alpha=0.7)
    x_start = q

plt.title('Exponentialverteilung mit Quantilen aus bins.csv')
plt.xlabel('t')
plt.ylabel('f_X(t)')
plt.xlim(0, x_max*1.05)
plt.ylim(0, max(pdf)*1.05)
plt.show()

