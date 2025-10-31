import numpy as np
from scipy.stats import norm

with open('data.bin', 'rb') as file:
    data = np.frombuffer(file.read(), dtype=np.uint8)
    data = data & 1

def runs_test_binary(data):
    n1 = np.sum(data == 1)
    n2 = np.sum(data == 0)
    runs = 1 + np.sum(data[1:] != data[:-1])
    expected_runs = 2 * n1 * n2 / (n1 + n2) + 1
    var_runs = (2 * n1 * n2 * (2 * n1 * n2 - n1 - n2)) / (((n1 + n2)**2) * (n1 + n2 - 1))
    z = (runs - expected_runs) / np.sqrt(var_runs)
    p = 2 * (1 - norm.cdf(abs(z)))
    return p

print(runs_test_binary(data))
