#include "scheduler.h"
#include "scheduler_internal.h"

#include <stdio.h>
#include <stdlib.h>

/*
 * This is the source file you must complete.
 *
 * You are responsible for implementing the simulated operating-system
 * behaviour, including:
 *   - PCBs and process states;
 *   - the ready queue and waiting processes;
 *   - CPU and I/O burst accounting;
 *   - FCFS, Round Robin, and preemptive SRTF;
 *   - dispatch/context-switch overhead; and
 *   - calls to the supplied metrics and CSV logging functions.
 *
 * You may replace this file completely and add any .c/.h files you need.
 * The types and helper functions below are only a suggested decomposition.
 * You may rename, replace, combine, or remove them.
 */

/*
 * Suggested PCB starting point. Add the fields required by the handout, such
 * as remaining CPU time, completed bursts, I/O completion time, Ready-entry
 * order, and RR quantum usage.
 */
/* PCB is defined in scheduler_internal.h (shared by all algorithm files). */

/* You may choose to maintain a structure for your queues.
 * Do so in whichever way you see fit.
 */

/* Keeping whole-simulation state together makes helper signatures smaller. 
 * Store whatever you feel is necessary for your implementation.
 */
/* Simulation is defined in scheduler_internal.h. */

/*
 * Supplied output API examples
 * ----------------------------
 * These functions are declared in scheduler.h and implemented in io.c. They
 * format the CSV rows for you and calculate the metrics. You must use them
 * to ensure your output can be correctly processed by the test harness.
 *
 * State transition:
 *
 *   log_transition(output, time, pid,
 *                  STATE_NEW, STATE_READY, REASON_ARRIVAL);
 *
 * One timeline interval [time, time + 1):
 *
 *  If the CPU is running a process:
 *      record_timeline_tick(output, time, ACTIVITY_CPU, running_pid);
 *
 *  If the CPU is performing a context switch:
 *      record_timeline_tick(output, time, ACTIVITY_CONTEXT_SWITCH, 0);
 *
 *  If the CPU is idle:
 *      record_timeline_tick(output, time, ACTIVITY_IDLE, 0);
 *
 * Final metrics (one call after every process terminates):
 *
 *   if (!write_metrics(output, specs, count)) {
 *       return false;
 *   }
 *
 */

/*
 * Suggested helper decomposition (optional)
 * -----------------------------------------
 * You may find helpers with responsibilities like these useful:
 *
 *   initialize_pcbs(...)
 *   ready_enqueue(...)
 *   choose_next_process(...)
 *   admit_arrivals(...)
 *   complete_io(...)
 *   maybe_preempt_srtf(...)
 *   update_dispatch(...)
 *   advance_one_interval(...)
 *
 * Required event-loop order
 * -------------------------
 *
 *   while not all processes are terminated:
 *       resolve the CPU interval that ended at the current time
 *       admit arrivals in ascending PID order
 *       complete I/O in ascending PID order
 *       apply the SRTF preemption rule, if applicable
 *       complete or begin dispatch/context-switch activity
 *       record exactly one CPU, context-switch, or idle timeline tick
 *       advance time by one
 *
 * You must design and implement the loop yourself. The reference outputs care
 * about observable timing and ordering, not the names of your private helpers.
 */

static bool init_pcbs(Simulation *sim,
                      const ProcessSpec *specs,
                      size_t count)
{
    size_t i;

    if (count > MAX_PROCESSES) {
        return false;
    }

    sim->processes = calloc(count, sizeof(*sim->processes));
    if (sim->processes == NULL && count != 0) {
        return false;
    }

    sim->process_count = count;
    sim->ready_count = 0;
    sim->running = -1;
    sim->switching = false;
    sim->dispatch_pending = false;
    sim->switch_time_left = 0;
    sim->terminated = 0;

    for (i = 0; i < count; i++) {
        sim->processes[i].spec = specs[i];
        sim->processes[i].state = STATE_NEW;
        sim->processes[i].remaining_cpu_time = specs[i].cpu_burst;
        sim->processes[i].completed_cpu_bursts = 0;
        sim->processes[i].io_completion_time = -1;
        sim->processes[i].time_left_in_cpu = 0;
    }

    return true;
}

static void transition(Simulation *sim,
                      int time,
                      size_t i,
                      ProcessState new_state,
                      TransitionReason reason)
{
    ProcessState old_state = sim->processes[i].state;

    sim->processes[i].state = new_state;
    log_transition(sim->output, time, sim->processes[i].spec.pid,
                   old_state, new_state, reason);
}

void add_to_ready_queue(Simulation *sim,
                        int time,
                        size_t i,
                        TransitionReason reason)
{
    sim->ready_queue[sim->ready_count++] = i;
    transition(sim, time, i, STATE_READY, reason);
}

static size_t remove_from_ready_queue(Simulation *sim, size_t pos)
{
    size_t i;
    size_t removed_index = sim->ready_queue[pos];

    for (i = pos; i + 1 < sim->ready_count; i++) {
        sim->ready_queue[i] = sim->ready_queue[i + 1];
    }
    sim->ready_count--;
    return removed_index;
}

/* Ask the active algorithm which ready_queue position to dispatch next. */
static size_t select_next_process(const Simulation *sim)
{
    switch (sim->config->algorithm) {
    case ALG_RR:
        return rr_select_next(sim);
    case ALG_SRTF:
        return srtf_select_next(sim);
    case ALG_FCFS:
    default:
        return fcfs_select_next(sim);
    }
}

static void dispatch_to_running(Simulation *sim, int time)
{
    size_t selected_pos = select_next_process(sim);
    size_t next = remove_from_ready_queue(sim, selected_pos);

    sim->running = (int)next;
    sim->processes[next].time_left_in_cpu = sim->config->quantum;
    transition(sim, time, next, STATE_RUNNING, REASON_DISPATCH);
}

static void resolve_cpu_interval(Simulation *sim, int time)
{
    int running_index;
    PCB *current;

    if (sim->running < 0) {
        return;
    }

    running_index = sim->running;
    current = &sim->processes[running_index];
    if (current->remaining_cpu_time == 0) {
        sim->running = -1;
        if (current->completed_cpu_bursts + 1 >= current->spec.num_cpu_bursts) {
            current->completed_cpu_bursts = current->spec.num_cpu_bursts;
            current->remaining_cpu_time = 0;
            transition(sim, time, (size_t)running_index, STATE_TERMINATED,
                       REASON_PROCESS_COMPLETE);
            sim->terminated++;
        } else {
            current->completed_cpu_bursts++;
            current->remaining_cpu_time = current->spec.cpu_burst;
            current->io_completion_time = time + current->spec.io_burst;
            transition(sim, time, (size_t)running_index, STATE_WAITING,
                       REASON_CPU_BURST_COMPLETE);
        }
        return;
    }

    if (sim->config->algorithm == ALG_RR && rr_quantum_expired(current)) {
        sim->running = -1;
        add_to_ready_queue(sim, time, (size_t)running_index,
                           REASON_QUANTUM_EXPIRED);
        return;
    }
}

static void admit_arrivals(Simulation *sim, int time)
{
    size_t i;

    for (i = 0; i < sim->process_count; i++) {
        PCB *current = &sim->processes[i];

        if (current->state == STATE_NEW && current->spec.arrival_time == time) {
            add_to_ready_queue(sim, time, i, REASON_ARRIVAL);
        }
    }
}

static void complete_io(Simulation *sim, int time)
{
    size_t i;

    for (i = 0; i < sim->process_count; i++) {
        PCB *current = &sim->processes[i];

        if (current->state == STATE_WAITING && current->io_completion_time == time) {
            current->io_completion_time = -1;
            current->remaining_cpu_time = current->spec.cpu_burst;
            add_to_ready_queue(sim, time, i, REASON_IO_COMPLETE);
        }
    }
}

static void maybe_preempt_srtf(Simulation *sim, int time)
{
    if (sim->config->algorithm != ALG_SRTF || sim->switching || sim->running < 0) {
        return;
    }

    srtf_maybe_preempt(sim, time);
}

static void update_dispatch(Simulation *sim, int time)
{
    if (sim->running >= 0 || sim->switching || sim->ready_count == 0) {
        return;
    }

    if (sim->dispatch_pending) {
        dispatch_to_running(sim, time);
        sim->dispatch_pending = false;
        return;
    }

    if (sim->config->context_switch > 0) {
        sim->switching = true;
        sim->switch_time_left = sim->config->context_switch;
        return;
    }

    dispatch_to_running(sim, time);
}

static void record_tick_and_advance(Simulation *sim, int time)
{
    if (sim->switching) {
        record_timeline_tick(sim->output, time, ACTIVITY_CONTEXT_SWITCH, 0);
        sim->switch_time_left--;
        if (sim->switch_time_left == 0) {
            sim->switching = false;
            sim->dispatch_pending = true;
        }
        return;
    }

    if (sim->running >= 0) {
        PCB *current = &sim->processes[sim->running];

        record_timeline_tick(sim->output, time, ACTIVITY_CPU,
                             current->spec.pid);
        current->remaining_cpu_time--;
        current->time_left_in_cpu--;
        return;
    }

    record_timeline_tick(sim->output, time, ACTIVITY_IDLE, 0);
}

#ifdef DEBUG
static void debug_print_pcb(const PCB *p)
{
    fprintf(stderr,
            "pid=%d state=%d remaining_cpu_time=%d completed_cpu_bursts=%d "
            "io_completion_time=%d time_left_in_cpu=%d\n",
            p->spec.pid,
            p->state,
            p->remaining_cpu_time,
            p->completed_cpu_bursts,
            p->io_completion_time,
            p->time_left_in_cpu);
}
#endif

/*
 * Run one complete scheduling simulation.
 *
 * Parameters
 * ----------
 * specs:
 *     A read-only array of validated process descriptions, sorted by ascending
 *     PID. Each entry supplies the arrival time, CPU- and I/O-burst lengths,
 *     and number of CPU bursts for one process. Copy entries into your PCBs if
 *     you need mutable per-process state. Do not modify or free this array.
 *
 * count:
 *     The number of ProcessSpec entries in specs.
 *
 * config:
 *     A read-only SimulationConfig containing the selected algorithm, the RR
 *     quantum, and the context-switch duration. The quantum affects scheduling
 *     only when config->algorithm is ALG_RR. Do not modify or free config.
 *
 * output:
 *     An already-open output context. Pass it to log_transition(),
 *     record_timeline_tick(), and write_metrics() as described above. main.c
 *     owns this context and will close it; do not close or free it here.
 *
 * Return true after the simulation and final metrics are written successfully.
 * Return false if initialization, simulation, or output generation fails.
 * main.c retains ownership of specs, config, and output in either case.
 */
bool run_simulation(const ProcessSpec *specs,
                    size_t count,
                    const SimulationConfig *config,
                    OutputContext *output)
{
    Simulation sim;
    int clock_time;
    bool ok = true;

    sim.processes = NULL;
    sim.config = config;
    sim.output = output;
    sim.ready_count = 0;
    sim.running = -1;
    sim.switching = false;
    sim.dispatch_pending = false;
    sim.switch_time_left = 0;
    sim.terminated = 0;

    if (!init_pcbs(&sim, specs, count)) {
        return false;
    }

    for (clock_time = 0; sim.terminated < count; clock_time++) {
        resolve_cpu_interval(&sim, clock_time);
        if (sim.terminated == count) {
            break;
        }
        admit_arrivals(&sim, clock_time);
        complete_io(&sim, clock_time);
        maybe_preempt_srtf(&sim, clock_time);
        update_dispatch(&sim, clock_time);
        record_tick_and_advance(&sim, clock_time);
    }

#ifdef DEBUG
    {
        size_t i;

        for (i = 0; i < count; i++) {
            debug_print_pcb(&sim.processes[i]);
        }
    }
#endif

    free(sim.processes);
    ok = write_metrics(output, specs, count);
    return ok;
}
