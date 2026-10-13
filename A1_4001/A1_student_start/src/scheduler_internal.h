#ifndef SCHEDULER_INTERNAL_H
#define SCHEDULER_INTERNAL_H

/*
 * Private declarations shared by student_scheduler.c and the algorithm
 * files (fcfs.c, rr.c, srtf.c). Not part of the supplied scheduler.h API.
 */

#include "scheduler.h"

typedef struct {
    ProcessSpec spec;
    ProcessState state;
    int remaining_cpu_time;     /* time left in the current CPU burst */
    int completed_cpu_bursts;   /* CPU bursts finished so far */
    int io_completion_time;     /* clock time the I/O ends (-1 if none) */
    int time_left_in_cpu;       /* RR: ticks left in the current quantum */
} PCB;

typedef struct {
    size_t process_count;
    PCB *processes;                    /* one PCB per process, in PID order */
    const SimulationConfig *config;
    OutputContext *output;
    size_t ready_queue[MAX_PROCESSES]; /* READY: PCB indices in FIFO order */
    size_t ready_count;
    int running;                       /* RUNNING: PCB index, or -1 if none */
    bool switching;                    /* a context switch is in progress */
    bool dispatch_pending;             /* a dispatch is waiting after a completed switch */
    int switch_time_left;              /* context-switch ticks still to run */
    size_t terminated;
} Simulation;

/* Shared helper (student_scheduler.c). */
void add_to_ready_queue(Simulation *sim,
                        int time,
                        size_t i,
                        TransitionReason reason);

/* First-Come, First-Served (fcfs.c). */
size_t fcfs_select_next(const Simulation *sim);

/* Round Robin (rr.c). */
size_t rr_select_next(const Simulation *sim);
bool rr_quantum_expired(const PCB *running);

/* Shortest Remaining Time First (srtf.c). */
size_t srtf_select_next(const Simulation *sim);
void srtf_maybe_preempt(Simulation *sim, int time);

#endif
