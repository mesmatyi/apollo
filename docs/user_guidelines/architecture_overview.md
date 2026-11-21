# Overall Architecture

## Apollo Open Platform Architecture

The Apollo open platform provides a four-layer architecture that integrates a vehicle certification platform, a hardware development platform, an open-source software platform, and a cloud service platform. Based on this four-layer architecture, developers can quickly build their own autonomous driving system.

![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Doc_CN_8_0/image_e49df47.png)

## Vehicle Certification Platform

The vehicle certification platform helps developers solve the drive-by-wire vehicle problem in building an Apollo autonomous driving system. It consists of two parts: certified drive-by-wire vehicles and the open vehicle interface standard.

### Open Vehicle Interface Standard

The open vehicle interface standard provides the Apollo drive-by-wire protocol and a development guide, based on which developers can develop their own drive-by-wire chassis and adapt it to the Apollo system. For the detailed Apollo drive-by-wire protocol and development guide, see [How to Develop a Chassis Adapted to the Apollo System](https://developer.apollo.auto/vehicle/certificate_cn.html).

### Certified Drive-by-Wire Vehicles

Certified drive-by-wire vehicles are drive-by-wire vehicles certified by the Apollo open platform as complying with the Apollo open vehicle interface standard. Developers can build an autonomous driving system by purchasing the corresponding vehicles. If you are interested in certified drive-by-wire vehicles, see the [Apollo Open Vehicle Certification Platform](https://developer.apollo.auto/vehicle/certificate_cn.html) for more information.

## Hardware Development Platform

The hardware development platform helps developers solve the problems of sensors, computing units, and other hardware devices in building an Apollo autonomous driving system. It provides various hardware devices and drivers adapted to Apollo. The adapted hardware falls into two categories: Apollo platform adaptation and Apollo hardware development platform adaptation. For details, see the [Apollo Hardware Development Platform](https://developer.apollo.auto/platform/hardware_cn.html).

### Apollo Platform Adaptation

Hardware adapted to the Apollo platform has passed the Apollo team's software-hardware closed-loop testing and can fully support the upper-layer software.

### Apollo Hardware Development Platform Adaptation

Hardware adapted to the Apollo hardware development platform has passed the Apollo team's data collection testing, but developers need to complete work such as closed-loop test verification with the upper-layer Apollo software on their own.

## Apollo Open-Source Software Platform

The Apollo open-source software platform provides the on-vehicle autonomous driving software framework and technology stack. It includes the underlying operating system, the middle-layer real-time communication framework, and the upper-layer autonomous driving application layer.

### Underlying Operating System

The underlying operating system is Ubuntu, and Ubuntu 18.04 and above is currently supported.

### Real-Time Communication Layer

The middle real-time communication layer uses Cyber RT, which Apollo designed specifically for autonomous driving applications. Apollo Cyber RT is an open-source, high-performance runtime framework designed specifically for autonomous driving scenarios. It is heavily optimized for the high concurrency, low latency, and high throughput of autonomous driving. For the features and usage of Cyber RT, see [Real-Time Communication Framework Cyber RT](<Apollo_Doc_CN_8_0/使用指南/整体架构/实时通信框架 Cyber RT.md>).

### Autonomous Driving Application Layer

The top-level autonomous driving application layer provides the core modules of the autonomous driving control flow, such as perception, prediction, planning, and control, the V2X cooperation module, and functional safety modules such as Monitor and Guardian. For a detailed introduction to these modules, see [Core Autonomous Driving Modules](Apollo_Doc_CN_8_0/使用指南/整体架构/自动驾驶应用层核心模块.md#自动驾驶核心模块). In addition, at the application layer Apollo provides DreamView, a visual human-machine interaction module. Through DreamView, developers can view visualized output of the current autonomous vehicle's modules, monitor the status of software and hardware modules, debug autonomous driving modules, control the vehicle in real time, and more.

## Apollo Cloud Service Platform

The Apollo cloud service platform provides the R&D infrastructure for autonomous driving development and improves R&D efficiency. Autonomous driving R&D differs from traditional internet software development: first, real-vehicle testing is costly, and second, the volume of data is very large. An R&D infrastructure that meets the needs of the autonomous driving development process and improves R&D efficiency is therefore very important. The Apollo cloud service platform solves the problem of data utilization efficiency through the cloud, reduces real-vehicle testing costs by combining with simulation, and can greatly improve the efficiency of Apollo-based autonomous driving R&D. In terms of the R&D process, the Apollo vehicle side generates open datasets through a data collector, trains various models and vehicle configurations on large-scale cloud clusters, verifies them through simulation, and finally deploys them to the Apollo vehicle side, with seamless connection throughout. This whole process actually contains two iteration loops, one for model configuration iteration and one for code iteration, both driven by data.

![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Doc_CN_8_0/image_50def8d.png)

### Data-Driven Model Configuration Iteration

This mainly includes the Apollo Fuel R&D cloud, which serves autonomous vehicle integration and model training. The Apollo Fuel cloud provides integrated tool services such as vehicle calibration, sensor calibration, and control evaluation, as well as tool services such as virtual lane line creation. Data collected by the Fuel Client intelligent data collector on the vehicle drives the cloud tool services to generate vehicle model configurations, which are delivered over the air (OTA) to the vehicle to complete the iteration loop.

### Data-Driven Code Iteration

This mainly includes the Apollo Dreamland simulation cloud, which serves development debugging and regression testing. The Apollo Dreamland simulation cloud provides simulation debugging scenarios that can be downloaded to a local development machine, as well as large-scale concurrent cluster testing in the cloud. By continuously collecting vehicle-side data to enrich simulation scenarios, it drives continuous code improvement and verification, and finally delivers the code to the vehicle over the air (OTA) to complete the code iteration loop.

For more about the introduction and use of the Apollo cloud service platform, see the [Apollo Studio documentation](https://studio.apollo.auto/Apollo-Homepage-Document/Apollo_Studio/).

## Documentation Feedback

If you encounter any problems while using the documentation, please go to the [feedback collection Q&A page](https://studio.apollo.auto/community/article/163) we have set up in the [developer community] to report them. We will iteratively improve the documentation based on your feedback.
