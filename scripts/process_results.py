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

      if "Accumulated" in line:
        split_line = line.split(" ")
        depth = int(split_line[-2])
        print(depth)
        if depth == 0:
          continue
        timeStr = split_line[-1].replace("ms", "")
        results[depthCounter].append(int(timeStr))
        lineCounter += 1
      elif "Prunes" in line:
        lineCounter = 0
        depthCounter += 1
        split_line = line.split(" ")
        prunes = int(split_line[-1])
      elif "Nodes" in line:
        split_line = line.split(" ")
        nodes = int(split_line[-1])
      elif "Backtracks" in line:
        split_line = line.split(" ")
        backtracks = int(split_line[-1])
      elif "cpu" in line:
        split_line = line.split(" ")
        time = int(split_line[-1])


  return (results, size, time, nodes, prunes, backtracks)

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

results, size, time, nodes, prunes, backtracks = read_results("out.txt", 14)
print(results)
output_results(results, size, time, nodes, prunes, backtracks)
