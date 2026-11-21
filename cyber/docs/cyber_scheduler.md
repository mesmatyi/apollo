# Apollo Cyber RT Scheduling Policies

The scheduling system provides two scheduling policies: the `classic` policy and the `choreography` policy.

- The `classic` policy is a fairly general-purpose scheduling policy. If you are unsure about the DAG structure on the current vehicle, this policy is recommended.

- The `choreography` policy is for when you are sufficiently familiar with the tasks on the vehicle. Based on the task dependencies and execution order, task execution time, task CPU consumption, message frequency and so on, the tasks are
  orchestrated.

## 2. classic policy

### 2.1 classic policy configuration example

cyber/conf/example_classic_sched.conf:

```bash
scheduler_conf {
    policy: "classic"
    process_level_cpuset: "0-7,16-23"
    threads: [
        {
            name: "async_log"
            cpuset: "1"
            policy: "SCHED_OTHER"
            prio: 0
        }, {
            name: "shm"
            cpuset: "2"
            policy: "SCHED_FIFO"
            prio: 10
        }
    ]
    classic_conf {
        groups: [
            {
                name: "group1"
                processor_num: 16
                affinity: "range"
                cpuset: "0-7,16-23"
                processor_policy: "SCHED_OTHER"
                processor_prio: 0
                tasks: [
                    {
                        name: "E"
                        prio: 0
                    }
                ]
            },{
                name: "group2"
                processor_num: 16
                affinity: "1to1"
                cpuset: "8-15,24-31"
                processor_policy: "SCHED_OTHER"
                processor_prio: 0
                tasks: [
                    {
                        name: "A"
                        prio: 0
                    },{
                        name: "B"
                        prio: 1
                    },{
                        name: "C"
                        prio: 2
                    },{
                        name: "D"
                        prio: 3
                    }
                ]
            }
        ]
    }
}
```

### 2.2 Configuration property reference

The meaning and effect of each configuration property:

- groups: As in the example in 2.1, the classic policy can configure multiple groups, mainly to achieve resource isolation and to address cross-NUMA issues. For example, if all tasks produced by a process execute on CPUs 0-31, the kernel scheduler will switch threads among CPUs 0-31, and crossing NUMA nodes adds extra overhead to the system. Groups can be used here to isolate NUMA nodes: CPUs 0-7 and 16-23 under one NUMA node are put into one group, and CPUs 8-15 and 24-31 under the other NUMA node are put into another group. This both makes full use of resources and avoids the overhead of crossing NUMA nodes.




- process_level_cpuset: controls which CPUs the mainboard process uses

- affinity: The value is range or 1to1. For example, the first group creates 16 threads and sets affinity on CPUs 0-7 and 16-23, so each thread can execute on any of cores 0-7 and 16-23. In the second group, affinity is 1to1, meaning the 16 threads correspond to CPUs 8-15 and 24-31, with each thread bound by affinity to one CPU. This reduces the overhead of threads switching among CPUs, but only if the number of threads started equals the number of CPUs.



- processor_policy and processor_prio: These two usually appear in pairs. processor_policy sets the thread scheduling policy, with values SCHED_FIFO (real-time scheduling policy, first come first served), SCHED_RR (real-time scheduling policy, round-robin time slices), and SCHED_OTHER (time-sharing scheduling policy, the default). Threads set to SCHED_FIFO or SCHED_RR get CPU execution with higher priority. The background for setting processor_policy in the scheduling model is to ensure that tasks on the main chain, or other real-time tasks, are executed first. If processor_policy is set to SCHED_FIFO/SCHED_RR, processor_prio takes a value of 1-99, and the larger the value, the higher the priority and the greater the chance of grabbing the CPU. If processor_policy is set to SCHED_OTHER, processor_prio takes a value of -20 to 19 (0 is the default); this is the nice value. The nice value does not affect the priority of being allocated a CPU, but it affects the size of the CPU time slice allocated: the smaller the nice value, the larger the time slice.






- tasks: This configures the tasks. name is the task name and prio is the task priority. To improve performance and reduce the granularity of task queue locks, the scheduling model uses multiple priority queues, that is, tasks of the same priority are in the same queue, and when the system schedules, tasks of higher priority are executed first.


### 2.3 Detailed configuration example

For example, with the topology shown in the figure below and the configuration file in 2.1, tasks A, B, C and D execute in the first group and E executes in the second group. Tasks that do not appear in the configuration, such as F, are placed in the first group by default. In the configuration we also set task priorities: the priorities of A, B, C and D increase in that order, which matches the topological dependencies in the figure below, so tasks later in the chain have higher priority. The purpose of this setting is to solve the problem of task priority inversion.



<img src="./images/topo_sched.png" width="30%" height="30%" />

## 3. choreography policy

### 3.1 choreography policy configuration example

cyber/conf/example_choreography_sched.conf:

```bash
scheduler_conf {
    policy: "choreography"
    process_level_cpuset: "0-7,16-23"
    threads: [
        {
            name: "lidar"
            cpuset: "1"
            policy: "SCHED_RR"
            prio: 10
        }, {
            name: "shm"
            cpuset: "2"
            policy: "SCHED_FIFO"
            prio: 10
        }
    ]
    choreography_conf {
        choreography_processor_num: 8
        choreography_affinity: "range"
        choreography_cpuset: "0-7"
        choreography_processor_policy: "SCHED_FIFO"
        choreography_processor_prio: 10

        pool_processor_num: 8
        pool_affinity: "range"
        pool_cpuset: "16-23"
        pool_processor_policy: "SCHED_OTHER"
        pool_processor_prio: 0

        tasks: [
            {
                name: "A"
                processor: 0
                prio: 1
            },
            {
                name: "B"
                processor: 0
                prio: 2
            },
            {
                name: "C"
                processor: 1
                prio: 1
            },
            {
                name: "D"
                processor: 1
                prio: 2
            },
            {
                name: "E"
            }
        ]
    }
}
```

### 3.2 Configuration property reference

The choreography policy mainly orchestrates the tasks on the main chain (configuration items starting with choreography), and puts tasks that are not on the main chain into a thread pool executed under the classic policy (configuration items starting with pool). The reason a classic thread pool exists within the choreography policy: the execution order of tasks on the main chain is clear, but tasks on other chains whose topological relationships are unclear, or tasks that have not been orchestrated (including asynchronous tasks created by Async), are put into the classic thread pool for execution. Regarding the
configuration properties:



- affinity: explained in 2.2.
- choreography_processor_policy and choreography_processor_prio set the scheduling policy and priority of the orchestrated threads; SCHED_FIFO is set here to ensure that the main chain can promptly preempt the CPU and execute. pool_processor_policy and pool_processor_prio set the scheduling policy and priority of the classic threads.


### 3.3 Detailed configuration example

Using the topology diagram from 2.3 as an illustration, A, B, C and D are main chain tasks and all have the processor property set, so A and B execute on CPU 0, and C and D execute on CPU 1. On the same core, A and B also have priorities set, so B executes first and then A. Task E, which has no processor property configured, and tasks not appearing in the task configuration, such as F, go into the classic thread pool for execution by default. Considering the relationships among task priority, execution time, frequency and scheduling, task orchestration follows these principles:



- Tasks on the same path should be orchestrated onto the same processor as far as possible; if a processor is overloaded, split some of the tasks onto other processors
- Along the same path, task priority increases step by step from start to end
- Tasks on different paths should not be mixed as far as possible
- High-frequency and short-duration tasks should be placed on the same processor as far as possible

## 4. Default configuration

If a process has no corresponding scheduling configuration file, the classic scheduling policy is used by default, and the number of scheduling task threads started is the value of default_proc_num set in conf/cyber.pb.conf


## 5. Example of a process loading a scheduling configuration file

In modules/dreamview/conf/hmi_modes/mkz_close_loop.pb.txt, two process_groups are set, compute_sched and control_sched. That is, when these two processes start, they load the scheduling configuration files compute_sched.conf and control_sched.conf respectively. Also note that the process_group of the dreamview process, dreamview_sched, is set in modules/dreamview/backend/main.cc, and loads the corresponding scheduling configuration file
dreamview_sched.conf.



## 6. Switching scheduling policy

The default scheduling policy is classic; the compute_sched.conf and control_sched.conf symlinks point to compute_sched_classic.conf and control_sched_classic.conf respectively. You can switch to the choreography policy by pointing the symlinks at the compute_sched_choreography.conf and control_sched_choreography.conf configuration
files.

