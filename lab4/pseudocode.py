import random
import matplotlib.pyplot as plt
import matplotlib.gridspec as gridspec
import numpy as np
from matplotlib.ticker import MaxNLocator
import math

SIM_TIME = 10000
PLOTS_PER_ROW = 2

def slotted_aloha(n_nodes, new_frame_prob, old_frame_prob):
    blocked = [False for n in range(n_nodes)]
    successes = 0
    for i in range(SIM_TIME):
        nodes_transmitting = 0
        last_transmitting = -1
        for j in range(n_nodes):

            if blocked[j]:
                if random.uniform(0, 1) <= old_frame_prob:
                    nodes_transmitting += 1
            else:
                if random.uniform(0, 1) <= new_frame_prob:
                    nodes_transmitting += 1
                    last_transmitting = j
                    blocked[j] = True

        if nodes_transmitting == 1:
            blocked[last_transmitting] = False
            successes += 1
    return(successes / SIM_TIME)


def plot_n_nodes(max_nodes, interval):
    vals_per_plot = math.ceil(max_nodes / interval)
    n_cols = 10
    n_rows = 10
    fig = plt.figure()
    gs = gridspec.GridSpec(n_rows, n_cols, wspace=0.5, hspace=0.5)
    new_frame_prob = 0.1
    old_frame_prob = 0.1
    for k in range(n_rows):
        for i in range(n_cols):
            proportions = []
            n_nodes = []
            for j in range(1, max_nodes + 1, interval):
                n_nodes.append(j)
                prop = slotted_aloha(j, new_frame_prob, old_frame_prob)
                proportions.append(prop)
                ax = fig.add_subplot(gs[k, i])
                ax.bar(n_nodes, proportions, width=0.4)
                axes = plt.gca()
                axes.set_ylim([0, 1])
                axes.xaxis.set_major_locator(MaxNLocator(integer=True))
            new_frame_prob += 0.1
        old_frame_prob += 0.1
    plt.show()


plot_n_nodes(5, 1)
