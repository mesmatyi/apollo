 
![Apollo_logo.png](./附录/images/Apollo_logo.png)
Hello, developer, and welcome to the Apollo autonomous driving open platform.


## Introduction

Apollo is an open, complete and safe platform that helps partners in the automotive industry and the autonomous driving field combine vehicle and hardware systems to quickly build
their own autonomous driving system.

Open capabilities, shared resources, accelerated innovation and continuous win-win are the slogans of the Apollo open platform. Baidu opens up its powerful, mature and safe autonomous driving technology and data to the industry, aiming to build an ecosystem centered on cooperation, leverage Baidu's technical strengths in artificial intelligence, empower partners, and jointly promote the development
and innovation of the autonomous driving industry.


The Apollo autonomous driving open platform provides developers with a rich choice of vehicles and hardware, powerful autonomous driving software capabilities such as environment perception, high-precision localization, path planning and vehicle control, as well as autonomous driving cloud services such as HD maps, simulation and data pipelines, helping developers quickly build an autonomous driving system from 0 to 1.


## Apollo milestones

Since the Apollo open program was announced in April 2017, over 9 years Apollo has released 15 versions up to Apollo 11.0, continuously innovating in multiple dimensions. These 9 years
fall into 3 stages. The first stage runs from the earliest record-and-replay autonomous driving in closed venues to the simple urban road conditions of 2.0, in which Apollo built its basic autonomous driving capabilities. The second stage runs
from the 2.5 vision-based highway driving in restricted areas to the 6.0 EDU education edition, in which Apollo accumulated rich scenario capabilities. In the third stage, starting from 7.0, Apollo focused on improving engineering usability, starting from developers' actual needs to help more developers use Apollo better and faster. Each upgrade has extended the capability boundary of the autonomous driving open platform outward in multiple dimensions.

Apollo 11.0 is committed to promoting the large-scale deployment of functional unmanned vehicles in high-value application scenarios. By comprehensively upgrading perception, localization, planning and the development toolchain, it greatly lowers the threshold for software and hardware development and gives developers a platform for efficiently building end-to-end autonomous driving solutions. With hardware-software coordination and rich ecosystem tool support, Apollo 11.0 not only lowers the R&D difficulty and deployment cost of functional unmanned vehicles, but also speeds up their commercialization, injecting new vitality into the development of autonomous driving technology.
## Open platform architecture

To make better use of Apollo, you need to have a brief understanding of the architecture of the open platform. The following is the latest Apollo architecture diagram, which consists of four layers: the hardware device platform, the core software platform,
the software application platform and the cloud service platform.

- **Hardware device platform**: helps developers solve the hardware problems, such as drive-by-wire vehicles and sensors, in building an Apollo autonomous driving system. For vehicle hardware, it includes two parts: certified drive-by-wire vehicles and the open vehicle interface standard.
  For other hardware, it includes reference hardware such as sensors and computing units, and hardware standards.
  For details, see the [Apollo hardware development platform](https://apollo.baidu.com/community/hardware).

- **Core software platform**: the Apollo core software platform provides the on-vehicle autonomous driving software system framework and technology stack. It includes the underlying operating system, the middle-layer real-time communication framework,
  and the upper autonomous driving application layer, such as perception, prediction, planning, control and localization.

- **Software application platform**: the Apollo software application platform provides engineering for different application scenarios and capability extensions of the autonomous driving application modules. Through the application platform layer, developers
  can more conveniently tailor, combine and extend the capabilities of the Apollo modules.

- **Cloud service platform**: the Apollo cloud service platform provides the R&D infrastructure in the autonomous driving development process and improves autonomous driving R&D efficiency.

Autonomous driving differs from traditional internet software development: first, real-vehicle testing is costly, and second, the data volume is very large. An R&D infrastructure that meets the needs of the autonomous driving development process and improves R&D efficiency
is therefore very important.

The Apollo cloud service platform solves the problem of data utilization efficiency through the cloud, and lowers the cost of real-vehicle testing by combining with simulation, greatly improving the efficiency of Apollo-based
autonomous driving R&D. In terms of the R&D process, the Apollo vehicle side generates open datasets through data collectors, generates various models and vehicle
configurations through large-scale cloud cluster training, then verifies them through simulation, and finally deploys them back to the Apollo vehicle side, in a seamless loop. This whole process actually contains 2 iteration loops, one for model configuration iteration and one
for code iteration, both driven by data.

![Apollo_11.0.png](./附录/images/Apollo_11_0.png)

With the capabilities that Apollo opens up and the services it provides, developers can quickly build and deploy their own autonomous driving systems with both peace of mind and little effort.

For architecture diagrams of earlier versions, see [architecture diagrams of each version](https://github.com/ApolloAuto/apollo/blob/master/README.md).

## Apollo open platform documentation

This documentation aims to give developers a comprehensive understanding of learning and using Apollo.

The Apollo open platform provides developers with precompiled Apollo installation packages, saving a great deal of build time and reducing environment deployment preparation time from days to hours. After downloading the installation package and installing Apollo,
developers can practice Apollo on their local computers.

- Local PNC simulation practice based on Dreamview, Apollo's visual interaction tool.
- Online simulation practice based on the Dreamland cloud simulation platform.
- Perception development and debugging practice based on Dreamview, Apollo's visual interaction tool.

For hardware selection, developers can refer to the [Apollo hardware development platform](https://apollo.baidu.com/community/hardware) and
the [Apollo Open Vehicle Certification Platform](https://apollo.baidu.com/community/certificate) to build the hardware of their own autonomous driving system. For more peace of mind, we
recommend the [Apollo D-KIT](https://apollo.baidu.com/community/apollo_d_kit), which integrates the chassis and sensor kit. While building an autonomous driving vehicle,
developers can also use [Apollo Fuel, the autonomous driving R&D cloud](https://bce.apollo.auto/user-manual/fuel-service?locale=zh-cn) to complete build processes such
as vehicle dynamics calibration and sensor calibration, to improve R&D efficiency.

### If you want to start using Apollo quickly

You can follow this documentation step by step to install Apollo, get started with Apollo and use it on the vehicle. We believe these documents will help you gain a deeper understanding of the capabilities of each of Apollo's core
modules.

Documentation structure

[Release notes](./发版说明/新版说明.md): a brief summary of the updates in each release.

[Installation guide](./安装指南/安装指南.md): documentation on how to install the Apollo software packages and source code.

[Application practices](./应用实践/开发调试教程/Apollo规划实践/规划模块综述.md): introduces application practice cases of autonomous driving communication and on-vehicle practice operations.

[Framework design](./框架设计/系统与环境/概述.md): introduces the framework design of Apollo

[Tool usage](./工具使用/Dreamview+/Dreamview+概述.md): introduces the use of Apollo-related tools

[Source code documentation](files.html): documentation generated from the source code and comments, convenient for reading and searching

### If you want to learn about the Apollo source code

If you want to understand Apollo from the bottom layer or even contribute to the Apollo codebase, you can refer to the source code and related materials in the
[Apollo Github repository](https://github.com/ApolloAuto/apollo).

The Apollo Github repository provides developers with rich technical documentation, usage guides and open-source code.

## Next steps

Now you have learned about the Apollo open platform and the Apollo documentation. Next, developers can refer to the [Installation instructions](../安装指南/包管理安装方式.md) to install Apollo.

Finally, don't forget to join the [Apollo developer community](https://apollo.baidu.com/), where developers can find a wealth of materials to read and use.

## Disclaimer

The Apollo open source platform only contains the source code of models, algorithms and processes; in commercial and product deployment it will be combined with network security defense strategy.

For Apollo's disclaimer about Apollo, see [Apollo Disclaimer](https://developer.apollo.auto/docs/disclaimer.html)
