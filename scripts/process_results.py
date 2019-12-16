import matplotlib.pyplot as plt
import numpy as np
import sys

def read_results(filename, n_searches):
  """
  Reads in the results from the file and returns all necessary data
  at each depth
  """
  results = [[] for i in range(n_searches)]
  afterMaxCliqueSize = False
  size = -1
  time = -1
  nodes = -1
  prunes = -1
  backtracks = -1
  lineCounter = 0
  depthCounter = 0
  with open(filename, "r") as f:
    for line in f:
      line = line.strip()

      if "=====" in line:
        continue
      


  return (results, size, time, nodes//5, prunes, backtracks//5)

def output_results(data, clique_size, exec_time, num_nodes, prunes, backtracks):
  """
#  Outputs all results collected from an experiment
#  """
  print("Number of nodes processed: {}".format(num_nodes))
  print("CPU execution time {}ms".format(exec_time))
  print("Node throughput {} per ms".format(int(num_nodes/exec_time)))
  print("Number of Prunes {}".format(prunes))
  print("Number of Backtracks {}".format(backtracks))
  fig1, ax1 = plt.subplots()
  plt.xticks([i for i in range(1,15)], [2, 4, 6, 8, 10, 20, 30, 40, 50, 60, 70, 80, 90, 100])
  ax1.set_ylabel("Time (\u03BCs)")
  ax1.set_xlabel("Depth Limit")
  ax1.set_title("Box Plots of Times recorded at each depth in Maximum Clique Search")
  ax1.boxplot(results)
  plt.show()

results, size, time, nodes, prunes, backtracks = read_results("", 14)
print(results)
output_results(results, size, time, nodes, prunes, backtracks)
