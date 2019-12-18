import matplotlib.pyplot as plt
import numpy as np
from pprint import pprint
import sys

NUM_SEARCHES = 5
NUM_LOCALITIES_RUN = 5
NUM_BUCKETS = 13

timesMap = {
  "1" : 0,
  "2" : 1,
  "4" : 2,
  "8" : 3,
  "16" : 4
}

def read_scaling_results(filename):
  """
  Read results from scaling file
  """
  times = [[] for j in range(NUM_LOCALITIES_RUN)]
  with open(filename, "r") as f:
    for line in f:
      if "cpu =" in line:
        line = line.strip().split(" ")
        idx = timesMap[line[-1]]
        times[idx].append(int(line[-2])/1000)
  medianTimes = [0 for i in range(NUM_LOCALITIES_RUN)]
  for i in range(NUM_LOCALITIES_RUN):
    if len(times[i]) > 0:
      times[i] = sorted(times[i])
      medianTimes[i] = (times[i][1] + times[i][2])/2
      print(medianTimes[i])

  return np.array(medianTimes)

def draw_scaling_graph(timesD, timesB, timesS, title, d, b, ylabel):
  """
  Draw speedup graph
  """
  fig1, ax1 = plt.subplots()
  x_axes = np.array([2**i for i in range(5)])
  ax1.plot(x_axes, timesD, marker="x", color="red", label="DepthBounded d = {}".format(d))
  ax1.plot(x_axes, timesB, marker="x", color="blue", label="Budget b = {}".format(b))
  ax1.plot(x_axes[0:-1], timesS, marker="x", color="purple", label="StackSteal with chunked")
  ax1.set_ylabel(ylabel) 
  ax1.set_xlabel("Localities")
  ax1.set_title(title)
  ax1.set_facecolor("grey")
  ax1.legend()
  plt.grid()
  plt.show() 

def get_speedups(timesD, timesB, timesS):
  """
  Returns the speedups for the times recorded for scaling
  """
  return (timesD[0]/timesD, timesB[0]/timesB, timesS[0]/timesS)

def draw_metrics_graph(metric, label, d, title, ylabel):
  """
  Draws a graph of a metric recorded during a search
  """
  fig1, ax1 = plt.subplots()
  x_axes = [2**i for i in range(1,5)]
  ax1.plot(x_axes, metric, marker="x", color="red", label="DepthBounded d = {}".format(d))
  ax1.set_ylabel(ylabel)
  ax1.set_xlabel("Localities")
  ax1.set_title(title)
  ax1.set_facecolor("grey")
  ax1.legend()
  plt.grid()
  plt.show()

def read_search_metrics(filename, is_opt=False):
  """
  Reads in the results from the file and returns all necessary data
  at each depth
  """
  buckets = np.array((NUM_BUCKETS,), dtype=np.uint64)
  searchTimes = np.zeros((NUM_SEARCHES,), dtype=np.uint64)
  nodeCounts = [0 for i in range(NUM_SEARCHES)]
  backtracks = [0 for i in range(NUM_SEARCHES)]
  prunes = [0 for i in range(NUM_SEARCHES)]
  sIdx = 0
  nIdx = 0
  bIdx = 0
  pIdx = 0
  once = True
  otherOnce = True
  with open(filename, "r") as f:
    for line in f:

      line = line.strip()
      if "CPU Time" in line:
        line = line.split(" ")
        searchTimes[sIdx] = int(line[-1])
        sIdx += 1
        continue

      line = line.split(":")
      if "Bucket" in line[0]:
        if not otherOnce:
          otherOnce = True
        buckets[int(line[0][-1])] += int(line[1])

      elif "Nodes" in line[0]:
        if not once:
          bIdx += 1
        nodeCounts[nIdx] += int(line[1])
        once = True

      elif "Backtracks" in line[0]:
        if once:
          once = False
          nIdx += 1
        backtracks[bIdx] += int(line[1].replace(" ", ""))

      if is_opt:
        if "Prunes" in line[0]:
          if otherOnce:
            pIdx += 1
            otherOnce = False
            prunes[pIdx] += int(line[1])

  for i in range(NUM_SEARCHES):
    nodeCounts[i] /= searchTimes[i]

  return buckets, nodeCounts, backtracks, searchTimes, prunes

def output_results(data, clique_size, exec_time, num_nodes, prunes, backtracks):
  """
  Outputs all results collected from an experiment
  """
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

timesD = read_scaling_results("../results/Depthbounded/MaxClique_depthbounded_scaling.txt")
timesB = read_scaling_results("../results/Budget/MaxClique_budget_scaling.txt")
timesS = read_scaling_results("../results/StackSteal/MaxClique_stacksteal_scaling.txt")
draw_scaling_graph(timesD, timesB, timesS[0:-1], "Maximum Clique scaling up to 250 workers on 16 localities brock800_4.clq", 2, 10**7, "Time (s)")
speedUpsD, speedUpsB, speedUpsS = get_speedups(timesD, timesB, timesS)
draw_scaling_graph(speedUpsD, speedUpsB, speedUpsS[0:-1], "Maximum Clique scaling up to 250 workers on 16 localities brock800_4.clq", 2, 10**7, "Relative Speedup (1 locality)")

pprint(read_search_metrics("../results/Budget/NS-hivert_budget_search_metrics_32.txt"))
buckets, nodes, backtracks, searchMetrics, prunes = read_search_metrics("../results/Budget/NS-hivert_budget_search_metrics_64.txt")
