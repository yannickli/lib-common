##########################################################################
#                                                                        #
#  Copyright (C) INTERSEC SA                                             #
#                                                                        #
#  Should you receive a copy of this source code, you must check you     #
#  have a proper, written authorization of INTERSEC to hold it. If you   #
#  don't have such an authorization, you must DELETE all source code     #
#  files in your possession, and inform INTERSEC of the fact you obtain  #
#  these files. Should you not comply to these terms, you can be         #
#  prosecuted in the extent permitted by applicable law.                 #
#                                                                        #
##########################################################################
import numpy as np
import matplotlib.pyplot as plt
import sys

COLUMNS = [
    # alloc block
    "alloc_nb", "alloc_slow",
    "alloc_nbtimer", "alloc_min", "alloc_max",
    "alloc_tot",
    # realloc block
    "realloc_nb", "realloc_slow",
    "realloc_nbtimer", "realloc_min", "realloc_max",
    "realloc_tot",
    # free block
    "free_nb", "free_slow",
    "free_nbtimer", "free_min", "free_max",
    "free_tot",
    # memory block
    "total_allocated", "total_requested",
    "max_allocated", "max_unused", "max_used",
    "malloc_calls", "current_used", "current_allocated"
]
POSITION = dict((s, i) for (i, s) in enumerate(COLUMNS))
NUM_COLUMNS = len(COLUMNS)


def plot(filename):
    data = np.loadtxt(filename, delimiter=',') #pylint: disable=E1101
    print data.shape
    print NUM_COLUMNS

    # adding "time"
    time = np.array(xrange(len(data[:,0]))) #pylint: disable=E1101

    # generate figure
    (_, ax1) = plt.subplots()
    ax1.plot(time, data[:,POSITION["current_used"]] / (1024 * 1024))
    ax1.plot(time, data[:,POSITION["current_allocated"]] / (1024 * 1024))
    ax1.set_xlabel('saves')
    ax1.set_ylabel('memory (MB)')
    ax1.legend(('Used memory', 'Available memory'), loc=0)
    ax2 = ax1.twinx()
    ax2.plot(time, data[:,POSITION["malloc_calls"]], 'r')
    ax2.set_ylabel('malloc calls', color='r')
    plt.show()



FILENAME = sys.argv[1]
if __name__ == "__main__":
    plot(FILENAME)
