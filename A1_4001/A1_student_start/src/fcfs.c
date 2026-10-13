#include "scheduler_internal.h"

/*
 * First-Come, First-Served (non-preemptive).
 * Dispatch the process that has waited in the Ready queue longest. The Ready
 * queue is kept in Ready-entry order, so that is always position 0.
 */
size_t fcfs_select_next(const Simulation *sim)
{
    (void)sim;
    return 0;
}
