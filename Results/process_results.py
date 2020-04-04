import matplotlib.pyplot as plt
import matplotlib.patches as mpatches
import matplotlib
import numpy as np
from pprint import pprint
import sys
import matplotlib.patches as mpatches
from itertools import repeat

NUM_SEARCHES = 5
NUM_LOCALITIES_RUN = 5
NUM_BUCKETS = 13
font = {'family' : 'normal',
          'weight' : 'normal',
          'size'   : 12 }
matplotlib.rc('font', **font)

plt.style.use('seaborn-darkgrid')

timesMap = {
  "1" : 0,
  "2" : 1,
  "4" : 2,
  "8" : 3,
  "16" : 4
}
workers = [i for i in range(100, 251, 50)]

def draw_node_throughput(problem, d, b, depth, stack, budget, depthR, stackR, budgetR):
  """
  Draws the node throughput graph
  """
  fig1, ax1 = plt.subplots()
  #ax1[0].grid(color='grey', linestyle='-', linewidth=0.25, alpha=0.5)
  #ax1[0].set_ylabel("Runtime (s)")
  #ax1[0].set_xlabel("Localities")
  ax1.set_title(problem)
  ax1.grid(color='grey', linestyle='-', linewidth=0.25, alpha=0.5)
  ax1.set_ylabel("Node Throughput (Nodes/second)")
  ax1.set_xlabel("ParallelArchitecture(Cores)")
  plt.style.use('seaborn-darkgrid')
  x_axes = [1, 2, 4, 8, 16]
  plt.xticks(x_axes, ["{}({})".format(x_axes[i],x_axes[i]*16) for i in range(len(x_axes))])
  #if len(depthR) != 0: ax1.plot(x_axes, depthR, marker="x", color="red", label="DepthBounded d = {}".format(d))

  #if len(budgetR) != 0: ax1.plot(x_axes, budgetR, marker="x", color="blue", label="Budget b = {}".format(b))

#  if len(stackR) != 0: ax1.plot(x_axes, stackR, marker="x", color="purple", label="Stacksteal")

  if len(depth) != 0: ax1.plot(x_axes, depth, marker="x", color="red", label="DepthBounded d = {}".format(d))

  if len(budget) != 0: ax1.plot(x_axes, budget, marker="x", color="blue", label="Budget b = {}".format(b))

  if len(stack) != 0: ax1.plot(x_axes, stack, marker="x", color="purple", label="Stacksteal")


  #ax1[0].legend()
  ax1.legend()
  plt.show()

def draw_overhead():
  fig1, ax1 = plt.subplots()
  ax1.set_title("Overhead for Numerical Semigroups. Evaluated using only the Budget Skeleton")
  ax1.grid(color='grey', linestyle='-', linewidth=0.25, alpha=0.5)
  ax1.set_ylabel("Node Throughput (Nodes/second)")
  ax1.set_xlabel("Localities")
  plt.style.use('seaborn-darkgrid')
  ax1.set_xticks([1, 2, 4, 8, 16])

  budg_dat_49_nodes = [368, 186, 95, 50, 27]
  ax1.plot([2**i for i in range(5)], budg_dat_49_nodes, marker="x", color="blue", label="g = 49, Node throughput")
  ax1.plot(1, 364, marker="o", color="red", label="g = 49. Regularity")
  ax1.plot(1, 363, marker=".", color="purple", label="g = 49. No metrics")
  brock800_s_nodes = [425, 309, 171, 91, 63]
  ax1.legend()
  plt.show()  

  reg_ns = [364]

#draw_overhead()



def read_node_tput(filename):
  """
  Read a result file for the node throughput. Returns the throughput itself
  """
  node_counts = 0
  cpu_time = None
  # We are looking for pattern "Node:"
  with open(filename) as f:
    for line in f:
      if "Nodes" in line and "Depth" in line:
        line = line.split(":")
        node_counts += int(line[1])
      elif "CPU Time (" in line:
        line = line.split(" ")
        cpu_time = int(line[-1])/1000.
  return node_counts, cpu_time


def collect_node_tput(files):
  """
  Return the node throughput for a given instance and metrics. Files is a list of files representing each of the 5 measurements taken
  """
  nodes = []
  times = []
  
  for i in files:
    n, t = read_node_tput(i)
    nodes.append(n)
    times.append(t)
  average_nodes = sum(nodes)/len(nodes)
  ts = []
  if None in times:
    for i in range(len(times)):
      if times[i] != None:
        ts.append(times[i])
  median_times = sorted(ts)[len(ts)//2] if ts != [] else sorted(times)[len(times)//2]
  return average_nodes, median_times

def collect_all_tput(problem, d, b, files):
  """
  Collect all the throughput from varying core counts. Files is a 3d List containing 2D lists for each core count, for each skeleton
  """
  budget = [[] for i in range(5)]
  b_n = [[] for i in range(5)]
  med_budget = [[] for i in range(5)]
  stack = [[] for i in range(5)]
  s_n = [[] for i in range(5)]
  med_stack = [[] for i in range(5)]
  depth = [[] for i in range(5)]
  d_n = [[] for i in range(5)]
  med_depth = [[] for i in range(5)]

  i = 0
  for f in files:
    k = 0
    for j in f:
      if len(f) > 0:
        n, t = collect_node_tput(j)
        if i == 0:
          budget[k].append(n/t)
          med_budget[k].append(t)
        elif i == 1:
          stack[k].append(n/t)
          med_stack[k].append(t)
        elif i == 2:
          depth[k].append(n/t)
          med_depth[k].append(t)
      k += 1
    i += 1

  draw_node_throughput("Node throughput for Depthbounded, d = 2, brock800_2.clq. (Beowulf Cluster)", d, b, depth, [], [], med_depth, [], [])
  draw_node_throughput("Node throughput for Stacksteal, brock800_2.clq. (Beowulf Cluster)", d, b, [], stack, [], [], med_stack, [])
  draw_node_throughput(problem, d, b, [], [], budget, [], [], [])

skels = ["Budget", "Stacksteal", "Depthbounded"]

files = [ [[] for i in range(5)] for i in range(3) ]

for i in [1,2,4,8,16]:
  idx = timesMap[str(i)]
  for j in range(1,6):
    files[0][idx].append("Budget/NodeThroughput/ns_hivert_{}_{}_49.txt".format(i,j))
    files[1][idx].append("Budget/NodeThroughput/brock800_2_{}_{}.txt".format(j,i))
    files[2][idx].append("Depthbounded/NodeThroughput/brock800_2_{}_{}.txt".format(j,i))

collect_all_tput("Node throughput for Numerical Semigroups, g = 49, Budget b = 10000000. (Beowulf Cluster)", 2, 10**7, files)

"""for i in files:
  for j in i:
    for f in j:
      try:
        with open(f) as file:
          print("Opened {}".format(f))
      except:
        print("Failed to open {}".format(f))"""
#pprint(files)
def collect_reg(file):
  """
  Collect metrics for runtime regularity
  """
  times = [[] for i in range(50)]
  tasks = [0 for i in range(50)]
  with open(file) as f:
    for line in f:
      line = line.strip()
      if "Time" in line and "CPU" not in line:
        line = line.split(":")
        temp = line[1].split(" ")
        depth = int(temp[0])
        times[depth].append(int(line[-1])/1000.)

      elif "Tasks" in line:
        line[0] = line[0].split(" ")
        depth = int(line[0][-1])
        tasks[depth] += int(line[1])

  return times, tasks

ns_53_rt = [299, 149, 79, 44, 30]
ns_53_sc = [1, 299/149, 299/79, 299/44, 299/30]
ns_55_rt = [767, 372, 200, 104, 59]
ns_55_sc = [1, 767/372, 767/200, 767/104, 767/59]
ns_59_rt = [2566, 1338, 676, 339, 176]
ns_59_sc = [1, 2566/1338, 2566/676, 2566/339, 2566/176]
ns_61_rt = [3560, 1763, 890, 448, 302]
ns_61_sc = [1, 3560/1763, 3560/890, 3560/448, 3560/302]

mc_hat10003_rt = [1887, 827, 430, 279, 313]
mc_har_1003_sc = [1, 1887/827, 1887/430, 1887/279, 1887/313]

mc_b800_1_rt = [37, 19, 12, 7]
mc_b800_1_sc = [1, 37/19, 37/12, 37/7]
mc_rt = [73, 35, 21, 41]
mc_sc = [1, 73/35, 73/21, 73/41]

mc_b800_b = [750, 373, 200, 149, 96, 92]
mc_b800_sb = [750/i for i in mc_b800_b]

mc_b800_d = [327, 162, 75, 38, 23, 20]
mc_b800_dc = [327/i for i in mc_b800_d]

mc_b800_s = [462, 274, 198, 98, 41, 40]
mc_b800_sc = [462/i for i in mc_b800_s]

def draw_scaling_graph(data, speedups, labels, speedLabels, problem, x_axes, n_threads, y, prob, colours=["blue", "red", "purple", "orange"]):
  """
  Draw speedup graph
  """
  fig1, ax = plt.subplots(1)

  plt.setp(ax.get_xticklabels(), rotation=0, horizontalalignment='right')
  plt.style.use('seaborn-darkgrid')
  def set_style(ax, title, y):
    ax.set_title(title)
    ax.grid(color='grey', linestyle='-', linewidth=0.25, alpha=0.5)
    ax.set_xlabel("ParallelArchitecture(Cores)")
    ax.set_ylabel(y)
    plt.xticks(x_axes, ["{}({})".format(x_axes[i]//x_axes[0],x_axes[i]) for i in range(len(x_axes))])

  set_style(ax, "Relative Speedup from 1(16) for Subgraph Isomorphism, g34-g79 on Depthbounded, $d = 4$ (Beowulf Cluster)", "Relative Speedup (from 1({}))".format(n_threads))
  idx = 0
  maxScale = x_axes[-1]//x_axes[0]
  ax.plot([16, 32, 64, 128, 256], [1, 2, 4, 8, 16], linestyle="--", label="Ideal Speedup")
  for i in speedups:
    ax.plot(x_axes, i, marker="x", color=colours[idx], label=labels[idx])
    idx += 1
  ax.legend()
  plt.show()

  fig, ax = plt.subplots(1)
  idx = 0
  set_style(ax, problem, "Runtime (s)")
  for i in data:
    ax.plot(x_axes, i, marker="x", color=colours[idx], label=labels[idx])
    idx += 1

  ax.set_ylim(bottom=0)
  ax.legend()
  plt.show()

NS = "Numerical Semigroups"
MC = "Maximum Clique"
SIP = "Subgraph Isomorphism"
"""
draw_scaling_graph([ns_53_rt, ns_55_rt], [ns_53_sc, ns_55_sc], ["g = 53", "g = 55"], ["g = 53", "g = 55"],
                  "Runtime for Numerical Semigroups on Budget, $b = 10000000$. Measured on Cirrus.",
                  [128, 256, 512, 1024, 2048], 128, "Relative Speedup from 1(128)", NS)

draw_scaling_graph([ns_59_rt], [ns_59_sc], ["g = 59"], ["g = 59"],
                  "Runtime for Numerical Semigroups on Budget, $b = 10000000$. Measured on Cirrus",
                  [256, 512, 1024, 2048, 4096], 256, "Relative Speedup from 1(256)", NS)

draw_scaling_graph([mc_b800_1_rt, mc_rt], [mc_b800_1_sc, mc_sc], ["Depthbounded, d = 2", "StackSteal"], ["Depthbounded, d = 2", "Stacksteal"],
                  "Runtime for Maximum Clique, brock800_1.clq. Measured on Cirrus", [128, 256, 512, 1024], 128,
                  "elative Speedup from 1(128)", MC, ["red", "purple"])

draw_scaling_graph([mc_hat10003_rt], [mc_har_1003_sc], [""], [""],
                  "Runtime for Maximum Clique, p_hat1000-3. Depthbounded $d = 2$. Measured on Cirrus", [256, 512, 1024, 2048, 4096], 256,
                  "Relative Speedup from 1(256)", MC, ["red"])
"""

sip_rt = [78, 79, 79, 79, 81]
sip_sc = [1, 78/79, 78/79, 78/79, 78/81]


draw_scaling_graph([sip_rt], [sip_sc], [""], [""], "Runtime for Subgraph Isomorphism, g34-g79 on Depthbounded, $d = 4$. (Beowulf Cluster)",
                  [16, 32, 64, 128, 256], 16, "Relative Speedup from 1(16) for Subgraph Isomorphism, g34-g79 on Depthbounded, $d = 4$ (Beowulf Cluster)", SIP)

def get_speedups():
  """
  Returns the speedups for the times recorded for scaling
  """
  return (timesD[0]/timesD, timesB[0]/timesB, timesS[0]/timesS)

def draw_bucket_graph(times, title, axes, labels, diffAxlab=False):
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
    if diffAxlab:
#     ax.set_xticks([0] + [i for i in range(6,45)])
      ranges = [str(0)] + ["${}$".format(i) for i in range(6,45)] 
      ax2.set_xticklabels(ranges)
    else:  
      ax.set_xticks(np.arange(1, len(labels) + 1))
    ax.set_xlabel('Depth')
    ax.set_ylabel('Time (s)')
    ax.set_xticklabels(labels)
    ax.set_xlim(0.25, len(labels) + 0.75)

  data = []
  for time in times:
    if len(time) > 0:
      data.append(np.array(time, dtype=np.float64))

  fig, ax2 = plt.subplots(sharey=True)
  ax2.set_title(title)
  pprint(data)
  parts = ax2.violinplot(data, showmeans=False, showmedians=False, showextrema=False)

  for pc in parts['bodies']:
      pc.set_facecolor('#D43F3A')
      pc.set_edgecolor('black')
      pc.set_alpha(1)

  lower_quartiles = []
  medians = []
  upper_quartiles = []

  red_patch = mpatches.Patch(color='red')

  fake_handles = repeat(red_patch, len(labels))

  ax2.legend(fake_handles, labels)

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

  if diffAxlab:
    ranges = [str(0)] + ["{}".format(i) for i in range(6,45)] 
    ax2.set_xticklabels(ranges)
  # set style for the axes
  for ax in [ax2]:
      set_axis_style(ax, axes)

  plt.subplots_adjust(bottom=0.15, wspace=0.05)
  plt.show()

def read_search_metrics(filename, is_opt=False):
  """
  Reads in the results from the file and returns all necessary data
  at each depth
  """
  times = [[] for i in range(52)]
  tasks = [0 for i in range(100)]
  with open(filename, "r") as f:
    for line in f:
      maxDT = 0
      matDepth = 0

      if "Time" in line and not "CPU" in line:

        line = line.strip().split(":")
        depth = int(line[1].split(" ")[0])
        if maxDT < depth:
          maxDT = depth

        times[depth].append(int(line[-1])/1000000.)

      elif "tasks" in line:
        line = line.strip().split(":")
        temp = line[0].split(" ")
        if matDepth < int(temp[-1]):
          matDepth = int(temp[-1])

        tasks[int(temp[-1])-1] += int(line[1])


  return times[maxDT+1], tasks

def draw_bar_chart(data, data2, data3, title, yaxes):
  """
  Draw bar chart for metric
  """
  fig, ax = plt.subplots(1)
  ax.grid(color='grey', linestyle='-', linewidth=0.25, alpha=0.5)
  plt.style.use('seaborn-darkgrid')
  ax.set_ylabel(yaxes)
  ax.set_title(title)
  ax.set_xlabel("Depth")
  x = np.arange(len(data))
  x2 = np.arange(len(data2))
  x3 = np.arange(len(data3))
  ax.set_xticks([i for i in range(max(len(x), len(x2), len(x3)))])
  print(x2, data2)
  ax.bar(x+0.00, data, width=0.25, color='blue', align='center', label='Budget b = 10000000')
  ax.bar(x2+0.25, data2, width=0.25, color='red', align='center', label='Depthbounded, d = 2')
  ax.bar(x3+0.50, data3, width=0.25, color='purple', align='center', label='Stacksteal')
  ax.legend()
  plt.show()



#times, ts = collect_reg("reg_depth.txt")
#draw_bucket_graph(times, "Runtime regularity on MaxClique brock400_1.clq, Depthbounded d = 2, 1 Locality", [i for i in range(3)], [len(i) for i in times[1:4]])


#times, ts = collect_reg("b800_reg_bug.txt")
#draw_bucket_graph(times[:-1], "Runtime regularity on MaxClique brock400_1.clq,Budget b = 10000000, 1 Locality", [i for i in range(6)],  [len(i) for i in times[1:7]])

#times, ts = read_search_metrics("rs_sip_d_6.txt")
#draw_bucket_graph(times, "Runtime regularity on Subgraph Isomporhism g34-g79, Depthbounded d = 6, 1 Locality", [i for i in range(7)], [len(i) for i in times[1:7]])

#times, ts = collect_reg("rs_sip_stack.txt")
#draw_bucket_graph(times, "Runtime regularity on Subgraph Isomporhism g34-g79, Stacksteal, 1 Locality", [i for i in range(2)], [len(i) for i in times[0:2]])

#times, ts = collect_reg("rs_sip_budg.txt")
#draw_bucket_graph(times, "Runtime regularity on Subgraph Isomporhism g34-g79, Budget b = 10000000, 1 Locality", [i for i in range(7)], [len(i) for i in times[1:7]])

"""
times, nodes, backtracks, sTimes, p, ns, bs, ts = read_search_metrics("ns_hivert_budget__52_metrics.txt")
draw_bucket_graph(times, "Runtime regularity on Numerical Semigroups g = 52, Budget b = 10000000, 16 Localities", [i for i in range(52)], [len(i) for i in ts])
"""
"""
"""
times, ts = collect_reg("ns_hivert_budget_1_48_metrics.txt")
for i in range(len(times)):
  print("{} & {} \\\\".format(i, len(times[i])))
#draw_bucket_graph(times, "Runtime regularity on Numerical Semigroups g = 48, Budget b = 10000000, 1 Localities", [i for i in range(49)], [])
"""
times, nodes, backtracks, sTimes, p, ns, bs, ts = read_search_metrics("ns_hivert_budget_2_48_metrics.txt")
draw_bucket_graph(times, "Runtime regularity on Numerical Semigroups g = 48, Budget b = 10000000, 2 Localities", [i for i in range(49)])

times, nodes, backtracks, sTimes, p, ns, bs, ts = read_search_metrics("ns_hivert_budget_4_48_metrics.txt")
draw_bucket_graph(times, "Runtime regularity on Numerical Semigroups g = 48, Budget b = 10000000, 4 Localities", [i for i in range(49)])
"""
#times, nodes, backtracks, sTimes, p, ns, bs, ts = read_search_metrics("../ns_hivert_budget_8_48_metrics.txt")
#print(ts)

#draw_bucket_graph(times, "Runtime regularity on Numerical Semigroups g = 48, Budget b = 10000000, 8 Localities", [i for i in range(49)])
