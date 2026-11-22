## Overview of Cyber RT Scheduling

With the continuous development of artificial intelligence technology, autonomous vehicles have begun to become possible. An autonomous vehicle needs to complete multiple tasks at the same time, including localization, perception, planning, control, and so on. How to schedule and orchestrate these tasks reasonably so that they run stably and efficiently is the main topic we introduce today.

### Operating System Scheduling Policies

The purpose of operating system scheduling is to maximize the use of system resources (especially CPU resources). Scheduling is often a compromise over limited resources, and its essence is: maximize efficiency while taking fairness into account.

#### What Does Scheduling Change?

Consider the following example. There are 3 tasks, and when one task finishes, another begins. The execution times of the 3 tasks are 10s, 6s, and 4s. How should we arrange the tasks?

![cyber_scheduler](./images/cyber_scheduler_1.jpeg)

From the example above, you can see that the total CPU execution time of strategy 1 and strategy 2 is unchanged, but the waiting time is reduced. That is to say, the scheduling system helps us reduce the total waiting time of tasks.
In supermarkets we often encounter queuing scenarios: when someone who buys only a few items queues in the same line as someone who buys a lot, the waiting time may be too long.

![cyber_scheduler](./images/cyber_scheduler_2.jpeg)

#### Shortest Time First?

Back to the example above, the optimal scheduling policy is born: shortest time first. The task with the shortest time is executed first each time, which ensures the shortest total waiting time of tasks.

![cyber_scheduler](./images/cyber_scheduler_3.jpeg)

#### How to Ensure Fairness?

Someone may immediately think of only creating small tasks each time, so that their tasks can always jump the queue and be executed first. But the problem is that this causes large tasks never to be executed, leaving them starved. Oh!!! Maximizing efficiency leads to unfair CPU utilization, and we must take fairness into account.

#### Can Program Time Be Predicted?

The shortest-time-first strategy is great, but we forgot an important problem: before a task finishes, we don't know how long it will run!!! The strategy fails!!!

![cyber_scheduler](./images/cyber_scheduler_4.jpeg)

Don't be discouraged, there is a new method: round-robin time slicing. Execute the tasks alternately, allocating each task a small slice of CPU time; when the time is used up, it exits and yields to other tasks.

![cyber_scheduler](./images/cyber_scheduler_5.jpeg)

Tips

> To support round-robin time slicing, the operating system and hardware cooperate to implement task preemption.

### Improvements in Cyber RT

#### Real-Time Operating System

- Real-time operating system: by applying real-time patches to linux, preemption is supported.
- Interrupt binding: peripherals such as cameras, LiDARs, and serial ports need to handle interrupts continuously, so their interrupt handlers can be bound to one core.

Tips

> A real-time operating system is not a panacea. Many people may ask: if I have a real-time system, does that mean tasks can always be completed on time? The idea is beautiful and also fits the definition of a real-time operating system, but resources are limited; for example, you cannot eat and drink at the same time.
> Relying on limited resources alone to guarantee the completion of unlimited things — if there were such a system, I think its achievement would be no less than a perpetual motion machine. Therefore, when a program cannot keep up, we need to check whether resources are sufficient, especially when busy.

#### Resource Limits & Priority

Cgroup is a Linux kernel feature used to limit, record, and isolate the resource usage (CPU, memory, disk I/O, network, etc.) of a group of processes. Cgroup has the following characteristics:

- Resource limits — you can configure cgroups to limit how much of a particular resource (such as memory or CPU) a process can use.
- Priority — when resource conflicts occur, you can control how many resources (CPU, disk, or network) a process can use compared with processes in another cgroup.
- Accounting — monitor and report resource limits at the cgroup level.
- Control — you can change the state of all processes in a cgroup (freeze, stop, or restart) with a single command.

```text
pthread_setaffinity_np

    choreography_conf {
        choreography_processor_num: 8
        choreography_affinity: "range"
        choreography_cpuset: "0-7" # bind CPU cores
        choreography_processor_policy: "SCHED_FIFO" # policy: SCHED_OTHER,SCHED_RR,SCHED_FIFO
```

#### Coroutines

Coroutines are user-mode threads whose switching is controlled by the user. For the definition of coroutines, refer to the GMP model in the go language

- M, Machine, represents a system-level thread, and goroutines run on M. For a thread to run tasks it must acquire a P and fetch a G from P's local queue. When P's queue is empty, M also tries to take a batch of Gs from the global queue and put them into P's local queue, or steal half from another P's local queue and put them into its own P's local queue. M runs G, and after G finishes executing, M fetches the next G from P, repeating continuously.
- P, processor, is the context environment required for goroutine execution. It can be understood as a coroutine processor, used to execute goroutines. The processor maintains a queue of runnable goroutines, which stores all the goroutines that need it to execute them.
- G, goroutine, the coroutine.

![cyber_scheduler](./images/cyber_scheduler_6.jpeg)

Advantages of coroutines over threads

- Lower system overhead. A thread switch goes user mode -> interrupt -> kernel mode, while a coroutine switch is completed entirely in user mode, which reduces system overhead.
- Lightweight. A coroutine occupies fewer resources than a thread; a thread often consumes several megabytes of memory.

Tips

> Coroutines can only yield by themselves; it is best not to use sleep or io, otherwise they will go to sleep actively

Why introduce coroutines? Because an autonomous driving system involves a lot of message transmission, which leads to a large amount of IO consumption, so how to reduce the overhead caused by task switching is especially important.

Introducing coroutines can reduce the overhead caused by task switching, thereby improving the efficiency of the program. In an autonomous driving system, a large amount of IO consumption degrades system performance. By using coroutines, the system's IO operations and non-IO operations can be handled separately, reducing the number of task switches and thus improving the system's response speed and efficiency.

In addition, using coroutines can also simplify the code structure and improve the readability and maintainability of the code. In an autonomous driving system, a large amount of data is processed and transmitted; if the traditional multi-threaded programming approach is used, the code structure becomes complex and hard to maintain. Using coroutines simplifies the code structure and makes the code clearer and easier to understand, thereby improving its readability and maintainability.

In summary, introducing coroutines can reduce the overhead caused by task switching, improve the system's response speed and efficiency, and also simplify the code structure and improve the code's readability and maintainability.

![cyber_scheduler](./images/cyber_scheduler_7.png)

## Cyber RT Scheduling Policies

### Multi-Priority Queues

Corresponding to the GMP model, each task in Cyber is treated as a coroutine, coroutines run on threads, and the priority of a coroutine can be set. Coroutines are managed through a multi-priority queue, and execution always starts from the coroutine with the highest priority.

![cyber_scheduler](./images/cyber_scheduler_8.png)

### Task Stealing

Interestingly, some Processors are assigned relatively few tasks, while others may be assigned relatively many, which leaves some Processors idle while others are saturated. To improve efficiency, an idle Processor steals half of the tasks from a saturated Processor to execute, thereby improving CPU utilization.

### Two Task Types

#### Component

A component is the smallest execution unit in Apollo, and each component corresponds to a specific task, such as localization or control. Multiple components combined can implement more complex functionality, such as perception. Apollo has 2 types of components in total: message-triggered and timer-triggered.

```cpp
class CommonComponentSample : public Component<Driver> {
 public:
  bool Init() override;
  bool Proc(const std::shared_ptr<Driver>& msg0) override;
};
CYBER_REGISTER_COMPONENT(CommonComponentSample)
```

#### Custom Tasks

If you want to start a new task for concurrent processing in your program, you can use the cyber::Async interface. The created task is uniformly scheduled and managed by cyber in the coroutine pool.

```cpp
class Foo {
 public:
  void RunOnce() {
    auto res = Async(&Foo::Task, this, 10);
    EXPECT_EQ(res.get(), 10);
  }

  uint32_t Task(const uint32_t& input) { return input; }
};
```

Tips

> Manually created threads do not have a cgroup set, so it is best not to create threads manually. If necessary, cgroups can be set through cyber configuration.
