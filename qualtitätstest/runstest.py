import numpy as np
from scipy.stats import norm
import random

def runs_test_binary(data):
    n1 = np.sum(data == 1)
    n2 = np.sum(data == 0)
    runs = 1 + np.sum(data[1:] != data[:-1])
    expected_runs = 2 * n1 * n2 / (n1 + n2) + 1
    var_runs = (2 * n1 * n2 * (2 * n1 * n2 - n1 - n2)) / (((n1 + n2)**2) * (n1 + n2 - 1))
    z = (runs - expected_runs) / np.sqrt(var_runs)
    p = 2 * (1 - norm.cdf(abs(z)))
    return p

with open('datatest.bin', 'rb') as file:
    datatrng = np.frombuffer(file.read(), dtype=np.uint8)
    datatrng = datatrng & 1


print(f"TRNG p-score: {runs_test_binary(datatrng)}")
