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

def draw_scaling_graph(times, title, d, b, ylabel, incDep=True):
  """
  Draw speedup graph
  """
  fig1, ax1 = plt.subplots()

  ax1.grid(color='grey', linestyle='-', linewidth=0.25, alpha=0.5)
  x_axes = np.array([1, 2, 4, 8, 16, 17])
  ax1.plot(x_axes, times[1], marker="x", color="blue", label="Budget b = {}".format(b))
  if incDep:
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

def draw_bucket_graph(times, title, axes):
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
  ax2.scatter(inds, medians, marker='x', color='white', s=10, zorder=3)
  ax2.grid(color='grey', linestyle='-', linewidth=0.25, alpha=0.5)
  ax2.vlines(inds, lower_quartiles, upper_quartiles, color='k', linestyle='-', lw=5)
  ax2.vlines(inds, whiskersMin, whiskersMax, color='k', linestyle='-', lw=1)

  # set style for the axes
  for ax in [ax2]:
      set_axis_style(ax, axes)

  plt.subplots_adjust(bottom=0.15, wspace=0.05)
  plt.show()


def read_search_metrics(filename, is_opt=False, maxDepth=):
  """
  Reads in the results from the file and returns all necessary data
  at each depth
  """
  times = [[] for i in range(52)]
  searchTimes = 0
  nodeCounts = 0
  backtracks = 0
  nodes = [0 for i in range()]
  b_tracks_arr = [0 for i in range(3)]
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
        searchTimes = int(line[-1])
        sIdx += 1
        continue

      line = line.split(":")
      if len(line) > 2:
        if "Time" in line[1].split(" ")[1]:
          depth = int(line[1].split(" ")[0])
          times[depth].append(int(line[-1])/1000.)        

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

  return times, nodeCounts, backtracks, searchTimes, prunes

def draw_node_throughput(times, nodes, title, d, b, y="Node Throughput (Nodes/second)", once=False):
  """
  Draws the node throughput graphs
  """
  fig1, ax1 = plt.subplots()
  ax1.set_ylabel(y)
  ax1.set_xlabel("Localities")
  ax1.set_title(title)
  if not once:
    ax1.plot([1, 2, 4, 8], nodes, marker="x", color="red", label="DepthBounded d = {}".format(d))
  ax1.plot([1, 2, 4, 8], times, marker="x", color="blue", label="Budget b = {}".format(b))
  #ax1.plot(workers, node_tput_s, marker="x", color="purple", label="StackSteal with chunked")
  ax1.grid(color='grey', linestyle='-', linewidth=0.25, alpha=0.5)
  ax1.legend()
  plt.show()

times = [0 for i in range(4)]
b_times = [0 for i in range(4)]
b_nodes = [0 for i in range(4)]
b_backtracks = [0 for i in range(4)]
nodes = [0 for i in range(4)]
backtracks = [0 for i in range(4)]
skels = ["depthbounded"]
ranges = [1, 2, 4, 8]
for i in range(len(ranges)):
  t, n, b, s, p = read_search_metrics("max_clique_metrics_depth_b800_{}.txt".format(ranges[i]))
  times[i] = s
  nodes[i] = n
  backtracks[i] = b
  t, n, b, s, p = read_search_metrics("max_clique_metrics_budget_b800_{}.txt".format(ranges[i]))
  b_times[3-i] = s
  b_nodes[3-i] = n
  b_backtracks[3-i] = b
  

print(nodes)
t_puts = []
b_tracks = []
b_tputs = []
b_btracks = []
for i in range(4):
  t_puts.append(nodes[i]/times[i])
  b_tracks.append(backtracks[i]/times[i])
  b_tputs.append(b_nodes[i]/b_times[i])
  b_btracks.append(b_backtracks[i]/b_times[i])

draw_node_throughput(t_puts, b_tputs, "Node throughput for Maximum Clique, brock800_4.clq", 2, 10000000)
draw_node_throughput(b_tracks, b_btracks, "Backtracks/Second for Maximum Clique, brock800_4.clq", 2, 10000000, "Backtracks/Second")

times = [0 for i in range(4)]
nodes = [0 for i in range(4)]
backtracks = [0 for i in range(4)]
skels = ["budget"]
ranges = [1, 2, 4, 8]
for i in range(len(ranges)):
  t, n, b, s, p = read_search_metrics("ns_hivert_budget_{}_48_metrics.txt".format(ranges[i]))
  times[i] = s
  nodes[i] = n
  backtracks[i] = b

t_puts = []
b_tracks = []
for i in range(4):
  t_puts.append(nodes[i]/times[i])
  b_tracks.append(backtracks[i]/times[i])

draw_node_throughput(t_puts, [], "Node throughput for NS-hivert g = 48, budget b = 10000000", None, 10000000, once=True)
draw_node_throughput(b_tracks, [], "Backtracks/Second for NS-hivert g = 48, budget b = 10000000", None, 10000000, "Backtracks/Second", once=True)

medians = np.zeros((3,6), dtype=np.float64)
nodes = np.zeros((3,6), dtype=np.float64)
ranges = [1, 2, 4, 8, 16, 17]
j, o = 0, 0
skels = ["stack_steal", "budget"]#, "depthbounded"]
for i in ranges:
  for k in skels:
    try:
      median, node = read_scaling_results("ns_hivert_{}_{}_52.txt".format(k, i))
      medians[j,o] = np.float64(median)
      nodes[j,o] = node
    except:
      j += 1
      continue
    j += 1
  o += 1
  j = 0

speedUps = np.zeros(medians.shape, dtype=np.float64)
throughPut = nodes / medians

for i in range(6):
  for j in range(3):
    speedUps[j,i] = medians[j,0] / medians[j,i]

draw_scaling_graph(medians, "Runtimes on NS-hivert, g = 52", 2, 100000, "Runtime (s)", incDep=False)
pprint(medians)
draw_scaling_graph(speedUps, "Scaling on NS-hivert, g = 52", 2, 100000, "Relative SpeedUp (1 locality)", incDep=False)


medians = np.zeros((3,6), dtype=np.float64)
nodes = np.zeros((3,6), dtype=np.float64)
ranges = [1, 2, 4, 8, 16, 17]
j, o = 0, 0
skels = ["stacksteal", "budget", "depthbounded"]
for i in ranges:
  for k in skels:
    median, node = read_scaling_results("max_clique_scaling_{}_{}.txt".format(k, i))
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

medians = np.zeros((3,6), dtype=np.float64)
nodes = np.zeros((3,6), dtype=np.float64)
ranges = [1, 2]#, 4, 8, 16, 17]
j, o = 0, 0
skels = ["stacksteal", "budget", "depthbounded"]
for i in ranges:
  for k in skels:
    median, node = read_scaling_results("sip_scaling_{}_{}.txt".format(k, i))
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

draw_scaling_graph(medians, "Runtimes on Subgraph Isomorphism si4_m4Dr2_m1296.06", 2, 10000000, "Runtime (s)")
draw_scaling_graph(speedUps, "Scaling on Subgraph Isomporhism on si4_m4Dr2_m1296.06", 2, 10000000, "Relative SpeedUp (1 locality)")

times, nodes, backtracks, sTimes, p = read_search_metrics("b401_d.txt")
draw_bucket_graph(times, "Runtime regularity on MaxClique brock400_1.clq, Depthbounded d = 2, 1 Locality", [i for i in range(3)])

times, nodes, backtracks, sTimes, p = read_search_metrics("b800_reg.txt")
draw_bucket_graph(times, "Runtime regularity on MaxClique brock800_4.clq, Depthbounded d = 2, 1 Locality", [i for i in range(3)])

times, nodes, backtracks, sTimes, p = read_search_metrics("b800_reg_bug.txt")
draw_bucket_graph(times, "Runtime regularity on MaxClique brock800_4.clq,Budget b = 10000000, 1 Locality", [i for i in range(7)])

times, nodes, backtracks, sTimes, p = read_search_metrics("rs_sip_d_6.txt")
draw_bucket_graph(times, "Runtime regularity on Subgraph Isomporhism g34-g79, Depthbounded d = 6, 1 Locality", [i for i in range(7)])

times, nodes, backtracks, sTimes, p = read_search_metrics("rs_sip_stack.txt")
draw_bucket_graph(times, "Runtime regularity on Subgraph Isomporhism g34-g79, Stacksteal, 1 Locality", [i for i in range(2)])

times, nodes, backtracks, sTimes, p = read_search_metrics("rs_sip_budg.txt")
draw_bucket_graph(times, "Runtime regularity on Subgraph Isomporhism g34-g79, Budget b = 10000000, 1 Locality", [i for i in range(7)])

times, nodes, backtracks, sTimes, p = read_search_metrics("ns_hivert_budget__52_metrics.txt")
draw_bucket_graph(times, "Runtime regularity on Numerical Semigroups g = 52, Budget b = 10000000, 16 Localities", [i for i in range(52)])

times, nodes, backtracks, sTimes, p = read_search_metrics("ns_hivert_budget_1_48_metrics.txt")
draw_bucket_graph(times, "Runtime regularity on Numerical Semigroups g = 48, Budget b = 10000000, 1 Localities", [i for i in range(49)])

times, nodes, backtracks, sTimes, p = read_search_metrics("ns_hivert_budget_2_48_metrics.txt")
draw_bucket_graph(times, "Runtime regularity on Numerical Semigroups g = 48, Budget b = 10000000, 2 Localities", [i for i in range(49)])

times, nodes, backtracks, sTimes, p = read_search_metrics("ns_hivert_budget_4_48_metrics.txt")
draw_bucket_graph(times, "Runtime regularity on Numerical Semigroups g = 48, Budget b = 10000000, 4 Localities", [i for i in range(49)])

times, nodes, backtracks, sTimes, p = read_search_metrics("ns_hivert_budget_8_48_metrics.txt")
draw_bucket_graph(times, "Runtime regularity on Numerical Semigroups g = 48, Budget b = 10000000, 8 Localities", [i for i in range(49)])

