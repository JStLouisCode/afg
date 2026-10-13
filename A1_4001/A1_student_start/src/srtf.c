#include "scheduler_internal.h"

/*
 * Preemptive Shortest Remaining Time First.
 */

/*
 * Return the ready_queue position of the process to dispatch next.
 * Called only when the Ready queue is non-empty.
 */
size_t srtf_select_next(const Simulation *sim)
{
    /*
     * TODO (SRTF selection): return the ready_queue position of the
     * process with the least remaining_cpu_time. Break ties by
     * Ready-entry order, then by lower PID.
     */
    (void)sim;
    return 0;
}

/*
 * Step 4 of the event loop. Called only when a process is RUNNING and no
 * context switch is in progress.
 */
void srtf_maybe_preempt(Simulation *sim, int time)
{
    /*
     * TODO (SRTF preemption): if a Ready process has strictly less
     * remaining_cpu_time than the running process, move the running
     * process back to Ready with REASON_SRTF_PREEMPTION.
     */
    (void)sim;
    (void)time;
}
