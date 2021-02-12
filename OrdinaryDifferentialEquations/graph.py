import matplotlib.pyplot as plt
from ast import literal_eval

plt.rcParams['toolbar'] = 'None'

with open('data.txt') as f:
    points = [list(literal_eval(line)) for line in f]

for x in range(0, len(points) - (len(points) % 2), 2):
    plt.plot(points[x], points[x+1])

plt.show()