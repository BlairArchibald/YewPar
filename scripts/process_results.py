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
  times = []
  nodes = 0
  with open(filename, "r") as f:
    for line in f:
      if "cpu = " in line:
        line = line.strip().split(" = ")
        times.append(int(line[-1])/1000.)
      elif "Nodes Depth" in line:
        line = line.strip().split(":")
        nodes += int(line[-1])

  return np.median(times), nodes

def draw_scaling_graph(times, title, d, b, ylabel):
  """
  Draw speedup graph
  """
  fig1, ax1 = plt.subplots()

  ax1.grid(color='grey', linestyle='-', linewidth=0.25, alpha=0.5)
  x_axes = np.array([1, 2, 4, 8, 16, 17])
  ax1.plot(x_axes, times[1], marker="x", color="blue", label="Budget b = {}".format(b))
  ax1.plot(x_axes, times[2], marker="x", color="red", label="DepthBounded d = {}".format(d))
  ax1.plot(x_axes, times[0], marker="x", color="purple", label="StackSteal")
  ax1.set_ylabel(ylabel) 
  ax1.grid(color='grey', linestyle='-', linewidth=0.25, alpha=0.5)
  ax1.set_xlabel("Localities")
  plt.style.use('seaborn-darkgrid')
  ax1.set_title(title)
  ax1.legend()
  plt.show() 

def get_speedups():
  """
  Returns the speedups for the times recorded for scaling
  """
  return (timesD[0]/timesD, timesB[0]/timesB, timesS[0]/timesS)

def draw_bucket_graph(times, title):
  """
  Draws a bar chart for the runtime regularity
  """
  def adjacent_values(vals, q1, q3):
    upper_adjacent_value = q3 + (q3 - q1) * 1.5
    upper_adjacent_value = np.clip(upper_adjacent_value, q3, vals[-1])

    lower_adjacent_value = q1 - (q3 - q1) * 1.5
    lower_adjacent_value = np.clip(lower_adjacent_value, vals[0], q1)
    return lower_adjacent_value, upper_adjacent_value

  def set_axis_style(ax, labels):
      ax.get_xaxis().set_tick_params(direction='out')
      ax.xaxis.set_ticks_position('bottom')
      ax.set_xticks(np.arange(1, len(labels) + 1))
      ax.set_xticklabels(labels)
      ax.set_xlim(0.25, len(labels) + 0.75)
      ax.set_xlabel('Depth')
      ax.set_ylabel('Time (s)')

  data = []
  for time in times:
    if len(time) > 0:
      data.append(np.array(time, dtype=np.float64))

  fig, ax2 = plt.subplots(sharey=True)
  ax2.set_title(title)
  parts = ax2.violinplot(data, showmeans=False, showmedians=False, showextrema=False)

  for pc in parts['bodies']:
      pc.set_facecolor('#D43F3A')
      pc.set_edgecolor('black')
      pc.set_alpha(1)

  lower_quartiles = []
  medians = []
  upper_quartiles = []

  for dat in data:
    l, m, u = np.percentile(dat, [25, 50, 75])
    lower_quartiles.append(l)
    medians.append(m)
    upper_quartiles.append(u)

  whiskers = np.array([
      adjacent_values(sorted_array, q1, q3)
      for sorted_array, q1, q3 in zip(data, lower_quartiles, upper_quartiles)])

  whiskersMin, whiskersMax = whiskers[:, 0], whiskers[:, 1]
  inds = np.arange(1, len(medians) + 1)
  ax2.scatter(inds, medians, marker='x', color='black', s=10, zorder=3)
  ax2.grid(color='grey', linestyle='-', linewidth=0.25, alpha=0.5)
  ax2.vlines(inds, lower_quartiles, upper_quartiles, color='k', linestyle='-', lw=5)
  ax2.vlines(inds, whiskersMin, whiskersMax, color='k', linestyle='-', lw=1)

  # set style for the axes
  labels = [0, 1, 2]
  for ax in [ax2]:
      set_axis_style(ax, labels)

  plt.subplots_adjust(bottom=0.15, wspace=0.05)
  plt.show()


def read_search_metrics(filename, is_opt=False):
  """
  Reads in the results from the file and returns all necessary data
  at each depth
  """
  times = [[] for i in range(8)]
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
        #searchTimes[sIdx] = int(line[-1])
        sIdx += 1
        continue

      line = line.split(":")
      if len(line) > 2:
        if "Time" in line[1].split(" ")[1]:
          depth = int(line[1].split(" ")[0])
          times[depth].append(int(line[-1]))        

      elif "Time" in line[0]:
        times.append(int(line[-1]))
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
  fig1, ax1 = plt.subplots()
  ax1.set_ylabel("Node Throughput (Nodes/second)")
  ax1.set_xlabel("Workers")
  ax1.set_title(title)
  ax1.plot([50, 100, 200, 250], nodes, marker="x", color="red", label="DepthBounded d = {}".format(d))
  ax1.plot([50, 100, 200, 250], times, marker="x", color="blue", label="Budget b = {}".format(b))
  #ax1.plot(workers, node_tput_s, marker="x", color="purple", label="StackSteal with chunked")
  ax1.grid(color='grey', linestyle='-', linewidth=0.25, alpha=0.5)
  ax1.legend()
  plt.show()
"""
times, nodes, backtracks, searchTimes, prunes = read_search_metrics("../rs_mc.txt")
times2, nodes, backtracks, searchTimes, prunes = read_search_metrics("../rs_mc_1.txt")

draw_bucket_graph(times2[:-1], "Runtime Regularity on MaxClique, Budget, b = 1000000, brock400_4.clq")

times3, nodes, backtracks, searchTimes, prunes = read_search_metrics("../rs_mc3.txt")
times4, nodes, backtracks, searchTimes, prunes = read_search_metrics("../rs_mc4.txt")"""
"""times2, nodes2, backtracks, searchTimes2, prunes = read_search_metrics("../../../../Misc/MaxClique_depthbounded_search_metrics_175.txt")
times3, nodes4, backtracks, searchTimes4, prunes = read_search_metrics("../../../../Misc/MaxClique_depthbounded_search_metrics_250.txt")"""

"""
times, nodes, backtracks, searchTimes, prunes = read_search_metrics("../../../../Misc/MaxClique_budget_search_metrics_100.txt")
times2, nodes2, backtracks, searchTimes2, prunes = read_search_metrics("../../../../Misc/MaxClique_budget_search_metrics_175.txt")
times3, nodes4, backtracks, searchTimes4, prunes = read_search_metrics("../../../../Misc/MaxClique_budget_search_metrics_250.txt")
draw_bucket_graph(times, times2, times3, "Runtime Regularity on MaxClique, brock800_1.clq, Budget, b = 1000000, 100, 175 and 250 workers")

times, nodes, backtracks, searchTimes, prunes = read_search_metrics("../../../../Misc/NS-hivert_budget2_search_metrics_100.txt")
times2, nodes2, backtracks, searchTimes2, prunes = read_search_metrics("../../../../Misc/NS-hivert_budget2_search_metrics_150.txt")
times3, nodes4, backtracks, searchTimes4, prunes = read_search_metrics("../../../../Misc/NS-hivert_budget2_search_metrics_250.txt")
draw_bucket_graph(times, times2, times3, "Runtime Regularity on NS-hivert, g = 47, Budget, b = 10000, 100, 175 and 250 workers")

times, nodes, backtracks, searchTimes, prunes = read_search_metrics("../results/Depthbounded/SIP_depthbounded_search_metrics_32.txt")
print(backtracks)
times2, nodes2, backtracks, searchTimes2, prunes = read_search_metrics("../results/Depthbounded/SIP_depthbounded_search_metrics_64.txt")
print(backtracks)
times3, nodes4, backtracks, searchTimes4, prunes = read_search_metrics("../results/Depthbounded/SIP_depthbounded_search_metrics_128.txt")
print(backtracks)
times4, nodes5, backtracks, searchTimes4, prunes = read_search_metrics("../results/Depthbounded/SIP_depthbounded_search_metrics_128.txt")
print(backtracks)"""

"""t, n, b, s, p = read_search_metrics("../results/SIP_budget_search_metrics_32.txt")
print(s, n)
t1, n1, b1, s1, p1 = read_search_metrics("../results/SIP_budget_search_metrics_50.txt")
print(s1, n1)
t2, n2, b2, s2, p2 = read_search_metrics("../results/SIP_budget_search_metrics_100.txt")
print(s2, n2)
t4, n4, b4, s4, p4 = read_search_metrics("../results/SIP_budget_search_metrics_250.txt")
print(s4, n4)"""

N = 2311090
M = 21278925

t_puts = [N/233, 15951030/386, 16455409/664, 17303299/938]
t_s = [M/1534, 18650461/806, 17925657/837, 14784145/1017]
#draw_node_throughput(t_puts, t_s, "Node throughput for SIP", 0, 100000)

medians = np.zeros((3,6), dtype=np.float64)
nodes = np.zeros((3,6), dtype=np.float64)
ranges = [1, 2, 4, 8, 16, 17]
j, o = 0, 0
skels = ["stacksteal", "budget", "depthbounded"]
for i in ranges:
  for k in skels:
    median, node = read_scaling_results("../max_clique_scaling_{}_{}.txt".format(k, i))
    medians[j,o] = np.float64(median)
    nodes[j,o] = node
    j += 1
  o += 1
  j = 0

speedUps = np.zeros(medians.shape, dtype=np.float64)
throughPut = nodes / medians

for i in range(6):
  for j in range(3):
    speedUps[j,i] = medians[j,0] / medians[j,i]

draw_scaling_graph(medians, "Runtimes on MaxClique on brock800_1.clq", 2, 10000000, "Runtime (s)")
draw_scaling_graph(speedUps, "Scaling on MaxClique on brock800_1.clq", 2, 10000000, "Relative SpeedUp (1 locality)")
times, nodes, backtracks, sTimes, p = read_search_metrics("../b401_d.txt")
draw_bucket_graph(times, "Runtime regularity on MaxClique brock400_1.clq, Depthbounded d = 2")