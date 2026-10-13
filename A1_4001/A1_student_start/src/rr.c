#include "scheduler_internal.h"

/*
 * Round Robin.
 * Selection is FIFO, like FCFS. dispatch_to_running() sets time_left_in_cpu
 * to the quantum, and record_tick_and_advance() counts it down each CPU tick.
 */
size_t rr_select_next(const Simulation *sim)
{
    (void)sim;
    return 0;
}

/*
 * The running process has used its whole quantum. resolve_cpu_interval()
 * checks burst completion first, so a burst ending exactly on the quantum
 * boundary completes normally instead of expiring.
 */
bool rr_quantum_expired(const PCB *running)
{
    return running->time_left_in_cpu == 0;
}
