## Overview

Cyber RT is a high-performance, high-throughput, low-latency computing runtime framework, in which dynamic loading technology and the directed acyclic graph (DAG) are among the important ways it achieves high performance.

Cyber RT adopts an engineering framework based on Component modules and dynamic loading configuration through a directed acyclic graph (DAG). That is, the relevant algorithm modules are created through Components, and the dependencies among the Components are dynamically loaded and configured through the DAG topology, thereby achieving unified scheduling of algorithms and unified allocation of resources. Adopting this engineering framework decouples algorithms from engineering, so that engineering can focus more on engineering and algorithms can focus more on algorithms.

## What Is a Component

Component is the base class provided by Cyber RT for building functional modules. It can be understood as Cyber RT's encapsulation of an algorithm functional module. Together with the DAG file corresponding to the Component, Cyber RT can dynamically load that functional module. Taking Apollo as an example, all modules in Apollo are built with Components.

The Components loaded by Cyber RT form a decentralized network. Each Component is an algorithm functional module whose dependencies are defined by the corresponding configuration file, and a system is composed of multiple components, which are connected through their mutual dependencies to form a computation graph.

The figure below shows a simple network with 3 components and 2 channels:

![cyber_component_dag](./images/cyber_component_dag.png)

## Types of Component

There are two types of Component: `apollo::cyber::Component` and `apollo::cyber::TimerComponent`.

- Component provides a message fusion mechanism that supports up to 4 channels of message fusion. When reading data from multiple Channels, the first Channel is the primary Channel. When a message arrives on the primary Channel, Cyber RT calls the Component's apollo::cyber::Component::Proc to process the data once.

- TimerComponent does not provide message fusion. Unlike Component, the apollo::cyber::TimerComponent::Proc function of a TimerComponent is not triggered by the primary channel but is called periodically by the system, and developers can determine the call interval in the configuration file.

## How a Component Is Created and Works

1. Include the header file;

2. Define a class and inherit from Component or TimerComponent; choose to inherit from Component or TimerComponent according to the functionality the Component needs.

3. Override the Init() and Proc() functions. The Init() function is executed when the Component is loaded and is used to initialize the Component, for example creating the Node, Node Reader, Node Writer, and so on; the Proc() function is the core function that implements the Component's functionality, and it implements the Component's core logic.

4. Register the Component in Cyber RT. Only if the Component is registered in Cyber RT can Cyber RT load it dynamically; otherwise, Cyber RT reports an error during dynamic loading.

```cpp
//1. Include the header file
#include "cyber/component/component.h"
#include "test/proto/examples.pb.h"

// 2. Define a class that inherits from the Component template class with Driver and Chatter as parameters
class ComponentSample : public Component<Driver, Chatter> {
public:
  //3. Override the Init() and Proc() functions
  bool Init() override;
  bool Proc(const std::shared_ptr<Driver>& msg0,
  const std::shared_ptr<Chatter>& msg1) override;
};
//4. Register ComopnentSample in cyber
CYBER_REGISTER_COMPONENT(ComponentSample)
```

> Component communication is implemented on top of Channel communication, using readers and writers to read from and write to channels to implement data reading and writing.

## How a Component Is Loaded

In Cyber RT, all Components are compiled into independent .so files, and Cyber RT loads the corresponding Components on demand according to the configuration files provided by developers. Therefore, developers need to write the configuration .dag file and .launch file for the .so file, so that Cyber RT can load and execute the Component correctly.

Cyber RT provides two ways to load and start a Component: using the cyber_launch tool to start the launch file corresponding to the component, and using mainboard to start the dag file corresponding to the component.

The cyber_launch tool can start dag files and binary files, while mainboard starts dag files.

![cyber_component_launch_dag](./images/cyber_component_launch_dag.png)

## Advantages of Component

Compared with writing communication logic in a main() function and compiling it into a separate executable, Component has the following advantages:

- It can be loaded into different processes by configuring the launch file, allowing flexible deployment.
- Parameter configuration, scheduling policy, and Channel names can be modified by configuring the DAG file.
- It can receive multiple kinds of messages and has multiple message fusion strategies.
- The interface is simple and can be dynamically loaded by the Cyber framework, making it more flexible and easier to use.
