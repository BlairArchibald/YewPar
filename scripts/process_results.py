import matplotlib.pyplot as plt
import matplotlib.patches as mpatches
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

workers = [i for i in range(100, 251, 50)]

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

def draw_bucket_graph(times, times2, times3, title):
  """
  Draws a bar chart for the runtime regularity
  """
  fig1, ax1 = plt.subplots()
  plt.rc('xtick', labelsize=8)
  ax1.set_ylabel("Time (s)")
  ax1.set_xlabel("Workers")
  ax1.set_xticks([i for i in range(1,5)])
  ax1.set_xticklabels([i for i in range(100,251,75)])
  ax1.set_title(title)
  ax1.set_facecolor("grey")
  blue = mpatches.Patch(color='blue')
  orange = mpatches.Patch(color='orange')
  red = mpatches.Patch(color='red')
  white = mpatches.Patch(color='white')
  plt.grid()
  parts = ax1.violinplot([times, times2, times3])
  plt.show()

def read_search_metrics(filename, is_opt=False):
  """
  Reads in the results from the file and returns all necessary data
  at each depth
  """
  times = []
  searchTimes = np.zeros((NUM_SEARCHES,), dtype=np.uint64)
  nodeCounts = 0
  backtracks = 0
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
      if "Time" in line[0] and not "CPU" in line[0]:
        if not otherOnce:
          otherOnce = True
        times.append(int(line[1])/1000.)

      elif "CountNodes" not in line[0] and "Nodes" in line[0]:

        nodeCounts += int(line[1])
        once = True

      elif "Backtracks" in line[0]:
        if once:
          once = False
          nIdx += 1
        backtracks += int(line[1].replace(" ", ""))

      if is_opt:
        if "Prunes" in line[0]:
          if otherOnce:
            pIdx += 1
            otherOnce = False
            prunes[pIdx] += int(line[1])

  return times, nodeCounts/NUM_SEARCHES, backtracks, sorted(searchTimes), prunes

def draw_node_throughput(times, nodes, title, d, b):
  """
  Draws the node throughput graphs
  """
  """
  node_tput_b = [None for i in range(len(times))]
  node_tput_d = [None for i in range(len(times))]
  node_tput_s = [None for i in range(len(times))]

  # Compute the throughputs first
  for i in range(len(times)):
    node_tput_b[i] = nodes[i] / times[i]
    node_tput_d[i] = nodes[i] / times[i]
    node_tput_s[i] = nodes[i] / times[i]
  """
  fig1, ax1 = plt.subplots()
  ax1.set_ylabel("Node Throughput (Nodes/second)")
  ax1.set_xlabel("Workers")
  ax1.set_title(title)
  ax1.plot([50, 100, 200, 250], nodes, marker="x", color="red", label="DepthBounded d = {}".format(d))
  ax1.plot([50, 100, 200, 250], times, marker="x", color="blue", label="Budget b = {}".format(b))
  ax1.set_facecolor("grey")
  #ax1.plot(workers, node_tput_s, marker="x", color="purple", label="StackSteal with chunked")
  ax1.legend()
  plt.grid()
  plt.show()
"""
times, nodes, backtracks, searchTimes, prunes = read_search_metrics("~/Documents/Misc/MaxClique_depthbounded_search_metrics_100.txt")
times2, nodes2, backtracks, searchTimes2, prunes = read_search_metrics("~/Documents/Misc/MaxClique_depthbounded_search_metrics_175.txt")
times3, nodes4, backtracks, searchTimes4, prunes = read_search_metrics("~/Documents/Misc/MaxClique_depthbounded_search_metrics_250.txt")
#draw_bucket_graph(times, times2, times3, "Runtime Regularity on MaxClique, Depthbounded, d = 2, 100, 175 and 250 workers")

times, nodes, backtracks, searchTimes, prunes = read_search_metrics("~/Documents/Misc/MaxClique_budget_search_metrics_100.txt")
times2, nodes2, backtracks, searchTimes2, prunes = read_search_metrics("~/Documents/Misc/MaxClique_budget_search_metrics_175.txt")
times3, nodes4, backtracks, searchTimes4, prunes = read_search_metrics("../MaxClique_budget_search_metrics_250.txt")
#draw_bucket_graph(times, times2, times3, "Runtime Regularity on MaxClique, Budget, b = 1000000, 100, 175 and 250 workers")

times, nodes, backtracks, searchTimes, prunes = read_search_metrics("../NS-hivert_budget2_search_metrics_100.txt")
times2, nodes2, backtracks, searchTimes2, prunes = read_search_metrics("../NS-hivert_budget2_search_metrics_150.txt")
times3, nodes4, backtracks, searchTimes4, prunes = read_search_metrics("../NS-hivert_budget2_search_metrics_250.txt")
#draw_bucket_graph(times, times2, times3, "Runtime Regularity on NS-hivert, Budget, b = 10000, 100, 175 and 250 workers")

times, nodes, backtracks, searchTimes, prunes = read_search_metrics("../results/Depthbounded/SIP_depthbounded_search_metrics_32.txt")
print(backtracks)
times2, nodes2, backtracks, searchTimes2, prunes = read_search_metrics("../results/Depthbounded/SIP_depthbounded_search_metrics_64.txt")
print(backtracks)
times3, nodes4, backtracks, searchTimes4, prunes = read_search_metrics("../results/Depthbounded/SIP_depthbounded_search_metrics_128.txt")
print(backtracks)
times4, nodes5, backtracks, searchTimes4, prunes = read_search_metrics("../results/Depthbounded/SIP_depthbounded_search_metrics_128.txt")
print(backtracks)
"""

t, n, b, s, p = read_search_metrics("../results/SIP_depthbounded_search_metrics_32.txt")
print(s, n)
t1, n1, b1, s1, p1 = read_search_metrics("../results/SIP_depthbounded_search_metrics_64.txt")
print(s1, n)
t2, n2, b2, s2, p2 = read_search_metrics("../results/SIP_depthbounded_search_metrics_128.txt")
print(s2, n)
t4, n4, b4, s4, p4 = read_search_metrics("../results/SIP_depthbounded_search_metrics_250.txt")
print(s4, n)

N = 2311090
M = 21278925

t_puts = [N/233, N/386, N/664, N/938]
t_s = [M/1534, M/806, M/837, M/1017]
draw_node_throughput(t_puts, t_s, "Node throughput for SIP", 0, 100000)