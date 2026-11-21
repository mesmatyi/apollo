## Introduction to Cyber RT

Apollo Cyber RT is an open-source, high-performance runtime framework designed specifically for autonomous driving scenarios. It is heavily optimized for the high concurrency, low latency, and high throughput of autonomous driving.

![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Doc_CN_8_0/image_36f0fb8.png)

Main benefits of using Apollo Cyber RT:

- Accelerate development
  - Well-defined task interfaces with data fusion functionality
  - A series of development tools
  - A large number of sensor drivers
- Simplify deployment
  - Efficient and adaptive message communication
  - Configurable user-level scheduler with resource awareness
  - Portable, with fewer dependencies
- Empower autonomous driving
  - The default open-source runtime framework
  - Purpose-built modules for autonomous driving

### Common Terms

#### Component

In an autonomous driving system, modules (such as perception, localization, and control) exist in the form of Components under Cyber RT. Different Components communicate with each other through Channels. The Component concept not only decouples modules, but also provides the flexibility to split a module into multiple submodules.

#### Channel

Channels are used to manage data communication in Cyber RT. Users can publish/subscribe to the same Channel to achieve p2p communication.

#### Task

Task is the abstract description of an asynchronous computing task in Cyber RT.

#### Node

Node is the basic building block of Cyber RT; every module contains a Node and communicates through it. By defining Readers/Writers or Services/Clients in a node, a module can have different types of communication.

#### Reader/Writer

Readers/Writers are usually created within a Node and serve as the main message transmission interfaces in Cyber RT.

#### Service/Client

In addition to Reader/Writer, Cyber RT also provides the Service/Client pattern for module communication. It supports two-way communication between nodes. When a request is made to a service, the client node receives a response.

#### Parameter

The parameter service provides a global parameter access interface in Cyber RT. It is built on the Service/Client pattern.

#### Service Discovery

As a decentralized framework, Cyber RT has no master/central node for service registration. All nodes are treated equally and can find other service nodes through "service discovery". `UDP` is used for service discovery.

#### CRoutine

Referring to the concept of coroutines, Cyber RT implements Coroutines to optimize thread usage and system resource allocation.

#### Scheduler

To better support autonomous driving scenarios, Cyber RT provides a variety of resource scheduling algorithms for developers to choose from.

#### Message

Message is the data unit used for data transmission between modules in Cyber RT.

#### DAG File

A DAG file is the configuration file for the topology of modules. In a dag file you can define the Components used and the upstream/downstream channels.

#### Launch File

A Launch file provides a simple way to start modules. By defining one or more dag files in a launch file, multiple modules can be started at the same time.

#### Record File

Record files are used to record messages sent/received on Channels in Cyber RT. Replaying a Record file helps reproduce the behavior of previous Cyber RT operations.

#### Mainboard

The main entry point of Cyber RT. A module process can be started with `mainboard -d xxx.dag`.

### Development Tools

The CyberRT framework also provides a series of practical tools to assist daily development, including command-line tools such as cyber_monitor and cyber_recorder.

> Note: These tools need to run inside the Apollo Docker environment.

All the tools above depend on the CyberRT software library, so before using them you need to configure the runtime environment of the CyberRT tools as follows:

```bash
username@computername:~$: source /apollo/cyber/setup.bash
```

The following focuses on how to use cyber_monitor.

#### Viewing Channel Data with Cyber Monitor in Practice

The command-line tool `cyber_monitor` provides the ability to display a real-time list of channel information in the terminal.

##### Running Cyber Monitor

Run `cyebr_monitor` with the following command in the Apollo Docker environment:

```bash
username@computername:~$: cyber_monitor
```

##### Useful Commands

You can get help information with the `-h` option:

```bash
username@computername:~$: cyber_monitor -h
```

With the `-c` option, you can make `cyber_monitor` monitor only one specified channel:

```bash
username@computername:~$: cyber_monitor -c ChannelName
```

##### Getting Familiar with the Cyber Monitor Interface

After `cyber_monitor` starts, an interactive interface is displayed in the terminal. `cyber_monitor` automatically collects information about all channels from the topology and displays it in two columns (channel name, data frequency).

Channel information is displayed in red by default. When data flows through a channel, the corresponding row is displayed in green, as shown in the figure below:

![cyber_monitor.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Doc_CN_6_0/cyber_monitor_4365395.png)

##### Interacting with Cyber Monitor

###### Command Introduction

```
ESC | q key ---- Exit
Backspace ---- Go back
h | H ---- Show the help page
PageDown | Ctrl+d ---- Previous page
PageUp | Ctrl+u ---- Next page
Up, down or w, s keys ---- Move the currently highlighted row up or down
Right arrow or d key ---- Enter the highlighted row and show the detailed information of its data
Left arrow or a key ---- Return from the current screen to the previous screen
Enter key ---- Same as the d key
f | F ---- Show the data frame frequency
t | T ---- Show the channel message type
Space ---- Close|open a channel (only effective when data arrives on the channel; a closed channel turns yellow)
i | I ---- Show the Reader and Writer information of the channel
b | B ---- Show the channel message content
n | N ---- Show the next item of a RepeatedField in the message
m | M ---- Show the previous item of a RepeatedField in the message
```
