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
<<<<<<< HEAD
      if "cpu = " in line:
        line = line.strip().split(" = ")
        times.append(int(line[-1])/1000.)
      elif "Nodes Depth" in line:
        line = line.strip().split(":")
        nodes += int(line[-1])

  return np.median(times), nodes

def draw_scaling_graph(times, title, d, b, ylabel):
=======
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
>>>>>>> cfcd49cc6bf7448ac95ff2a7bcee1d82dc8af8ce
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
<<<<<<< HEAD
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
  labels = [0, 1, 2, 3, 4, 5, 6]
  for ax in [ax2]:
      set_axis_style(ax, labels)

  plt.subplots_adjust(bottom=0.15, wspace=0.05)
=======
  fig1, ax1 = plt.subplots()
  x_axes = [2**i for i in range(1,5)]
  ax1.plot(x_axes, metric, marker="x", color="red", label="DepthBounded d = {}".format(d))
  ax1.set_ylabel(ylabel)
  ax1.set_xlabel("Localities")
  ax1.set_title(title)
  ax1.set_facecolor("grey")
  ax1.legend(["150 workers", "250 workers"])
  plt.grid()
  plt.show()

def draw_bucket_graph(buckets, buckets2, buckets3, buckets4, title):
  """
  Draws a bar chart for the runtime regularity
  """
  fig1, ax1 = plt.subplots()
  plt.rc('xtick', labelsize=8)
  ax1.set_xlabel("Number of tasks")
  ax1.set_ylabel("Workers")
  ax1.set_yticks([i for i in range(1,5)])
  ax1.set_yticklabels(workers)
  ax1.set_title(title)
  axes = ["[0.01,0.05)",
          "[0.05,0.10)",
          "[0.10,0.25)",
          "[0.25,0.50)",
          "[0.50,1.00)",
          "[1.00,2.00)",
          "[2.00,4.00)",
          "[4.00,8.00)",
          "[8.00,16.0)",
          "[16.0, 32.0)",
          "[32.0,64.0)",
          "> 64.0"]
  ax1.set_facecolor("grey")
  blue = mpatches.Patch(color='blue')
  orange = mpatches.Patch(color='orange')
  red = mpatches.Patch(color='red')
  white = mpatches.Patch(color='white')
  plt.grid()
  parts = ax1.violinplot([buckets[1:], buckets2[1:], buckets3[1:], buckets4[1:]], vert=False, showmeans=True, showextrema=True, showmedians=True)
  quartile1, medians, quartile3 = np.percentile(buckets, [25, 50, 75], axis=1)
>>>>>>> cfcd49cc6bf7448ac95ff2a7bcee1d82dc8af8ce
  plt.show()


def read_search_metrics(filename, is_opt=False):
  """
  Reads in the results from the file and returns all necessary data
  at each depth
  """
<<<<<<< HEAD
  times = [[] for i in range(8)]
=======
  buckets = []
>>>>>>> cfcd49cc6bf7448ac95ff2a7bcee1d82dc8af8ce
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
<<<<<<< HEAD
      if len(line) > 2:
        if "Time" in line[1].split(" ")[1]:
          depth = int(line[1].split(" ")[0])
          times[depth].append(int(line[-1]))        

      elif "Time" in line[0]:
        times.append(int(line[-1]))
      elif "CountNodes" not in line[0] and "Nodes" in line[0]:

        nodeCounts += int(line[1])
=======
      if "Time :" in line[0]:
        if not otherOnce:
          otherOnce = True
        buckets.append(int(line[1]))

      elif "Nodes" in line[0]:
        if not once:
          bIdx += 1
        nodeCounts[nIdx] += int(line[1])
>>>>>>> cfcd49cc6bf7448ac95ff2a7bcee1d82dc8af8ce
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
<<<<<<< HEAD
  fig1, ax1 = plt.subplots()
  ax1.set_ylabel("Node Throughput (Nodes/second)")
  ax1.set_xlabel("Workers")
  ax1.set_title(title)
  ax1.plot([50, 100, 200, 250], nodes, marker="x", color="red", label="DepthBounded d = {}".format(d))
  ax1.plot([50, 100, 200, 250], times, marker="x", color="blue", label="Budget b = {}".format(b))
  #ax1.plot(workers, node_tput_s, marker="x", color="purple", label="StackSteal with chunked")
  ax1.grid(color='grey', linestyle='-', linewidth=0.25, alpha=0.5)
=======
  node_tput_b = [None for i in range(len(times))]
  node_tput_d = [None for i in range(len(times))]
  node_tput_s = [None for i in range(len(times))]

  # Compute the throughputs first
  for i in range(len(times)):
    node_tput_b[i] = nodes[i] / times[i]
    node_tput_d[i] = nodes[i] / times[i]
    node_tput_s[i] = nodes[i] / times[i]

  fig1, ax1 = plt.subplots()
  ax1.set_ylabel("Node Throughput (Nodes/second)")
  ax1.set_xlabel("Workers")
  ax1.set_xticks([i for i in range(1,5)])
  ax1.set_xticklabels(workers)
  ax1.set_title(title)
  ax1.set_facecolor("grey")
  ax1.plot(workers, node_tput_b, marker="x", color="red", label="DepthBounded d = {}".format(d))
  ax1.plot(workers, node_tput_d, marker="x", color="blue", label="Budget b = {}".format(b))
  print(node_tput_b, node_tput_d, node_tput_s)
  ax1.plot(workers, node_tput_s, marker="x", color="purple", label="StackSteal with chunked")
>>>>>>> cfcd49cc6bf7448ac95ff2a7bcee1d82dc8af8ce
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

<<<<<<< HEAD
for i in range(6):
  for j in range(3):
    speedUps[j,i] = medians[j,0] / medians[j,i]

draw_scaling_graph(medians, "Runtimes on MaxClique on brock800_1.clq", 2, 10000000, "Runtime (s)")
draw_scaling_graph(speedUps, "Scaling on MaxClique on brock800_1.clq", 2, 10000000, "Relative SpeedUp (1 locality)")
=======
#timesD = read_scaling_results("../results/Depthbounded/MaxClique_depthbounded_scaling.txt")
#timesB = read_scaling_results("../results/Budget/MaxClique_budget_scaling.txt")
#timesS = read_scaling_results("../results/StackSteal/MaxClique_stacksteal_scaling.txt")
#draw_scaling_graph(timesD, timesB, timesS[0:-1], "Maximum Clique scaling up to 250 workers on 16 localities brock800_4.clq", 2, 10**7, "Relative Speedup (1 locality)")
#speedUpsD, speedUpsB, speedUpsS = get_speedups(timesD, timesB, timesS)
#draw_scaling_graph(speedUpsD, speedUpsB, speedUpsS[0:-1], "Maximum Clique scaling up to 250 workers on 16 localities brock800_4.clq", 2, 10**7, "Relative Speedup (1 locality)")

buckets, nodes, backtracks, searchTimes, prunes = read_search_metrics("../NS-hivert_budget_search_metrics_100.txt")
pprint(np.median(searchTimes))
buckets2, nodes2, backtracks, searchTimes2, prunes = read_search_metrics("../NS-hivert_budget_search_metrics_150.txt")
pprint(np.median(searchTimes2))
buckets3, nodes3, backtracks, searchTimes3, prunes = read_search_metrics("../NS-hivert_budget_search_metrics_200.txt")
pprint(np.median(searchTimes3))
buckets4, nodes4, backtracks, searchTimes4, prunes = read_search_metrics("../NS-hivert_budget_search_metrics_250.txt")
pprint(np.median(searchTimes4))
draw_bucket_graph(buckets, buckets2, buckets3, buckets4, "Runtime Regularity on NS-hivert, g = 50, Budget = 10000, 150 workers and 250 workers")

median_times = np.median(np.array([searchTimes, searchTimes2, searchTimes3, searchTimes4]), axis=1)
avg_nodes = np.mean([nodes, nodes2, nodes3, nodes4], axis=1)
draw_node_throughput(median_times, avg_nodes, "Node throughput for NS-hivert, g = 50", 35, 10000)

buckets, nodes, backtracks, searchTimes, prunes = read_search_metrics("../NS-hivert_depthbounded_search_metrics_100.txt")
pprint(np.median(searchTimes))
buckets2, nodes, backtracks, searchTimes, prunes = read_search_metrics("../NS-hivert_depthbounded_search_metrics_150.txt")
pprint(np.median(searchTimes))
buckets3, nodes, backtracks, searchTimes, prunes = read_search_metrics("../NS-hivert_depthbounded_search_metrics_200.txt")
pprint(np.median(searchTimes))
buckets4, nodes, backtracks, searchTimes, prunes = read_search_metrics("../NS-hivert_depthbounded_search_metrics_250.txt")
pprint(np.median(searchTimes))
draw_bucket_graph(buckets, buckets2, buckets3, buckets4, "Runtime Regularity on NS-hivert, g = 50, Depthbounded, d = 35, 150 & 250 workers")
>>>>>>> cfcd49cc6bf7448ac95ff2a7bcee1d82dc8af8ce
