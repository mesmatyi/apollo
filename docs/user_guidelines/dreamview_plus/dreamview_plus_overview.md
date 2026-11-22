# Overview

## What Is Dreamview+?

Dreamview+ is a web application that provides the following functions:

- Visualizes the output of the current autonomous vehicle's modules. For example: planned path, vehicle localization, chassis information, and so on.
- Provides users with a human-machine interaction interface to monitor vehicle hardware status, switch modules on and off, start the autonomous vehicle, and so on.
- Provides debugging tools. For example: the PnC Monitor can efficiently track down problems in module output.
  However, most of the functions of Dreamview+ are mainly intended for use in on-vehicle debugging.
  In the Quick Start, only the visualization function of Dreamview+ is used, to visualize the data of a record data package.

<!--

## How Does It Solve Problems?
### Lowering the Barrier

  Evolving from a single-function tool into a multi-scenario tool, from supporting only real-vehicle road testing at the beginning to handling multiple scenarios and workflows, it meets the usage characteristics and habits of different types of developers, optimizes the interactions and steps for different scenarios, and lowers the onboarding cost for different types of developers, especially beginners, reducing heavy dependence on documentation and improving user experience.

### Secondary Development

  In multiple scenarios, developers in different roles have different visualization needs and habits, and fixed functions and layouts cannot simultaneously satisfy the usage habits of perception developers, PNC developers, and customers' real-vehicle autonomous driving demonstrations. Dreamview 2.0 provides the extensibility of a general framework and support for more scenarios and toolboxes. Through flexible custom panel functionality, panel layout and size can be customized to match the habits of different types of developers, lowering secondary development costs and improving secondary development efficiency.

### Ecosystem Foundation

  As autonomous driving is gradually deployed in the future, an explosive volume of data will arrive. Dreamview 2.0 strengthens the vehicle-cloud integrated linkage of the configuration center: Studio adds more types and quantities of data resources, and Dreamview enhances the ability of the configuration center to configure resources.
-->

## Overall Layout

![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Doc_CN_9_0/image_1445a63.png)

- Sidebar: displays the menu buttons: **Mode Settings**, **Add Panel**, **Resource Manager**, **Personnal Center**.

- Menu window: displays the details of each menu button.

  - Mode Settings: the mode settings toolbar. For details, see [Mode Settings](<Apollo_alpha_doc/使用指南/可视化交互工具 Dreamview+/Mode Settings.md>).
  - Add Panel: adds panels, including Console, Module Delay, Vehicle Visualization, Camera View, Point Cloud View, Vehicle Dashboard View, and other panels. For a detailed introduction to panels, see [Add Panel](<Apollo_alpha_doc/使用指南/可视化交互工具 Dreamview+/Add Panel.md>).
  - Resource Manager: the vehicle-cloud integrated configuration center provided by Apollo, which offers downloads of materials such as vehicles, models, data packages, and simulation scenarios. You can click the data packages, vehicles, models, simulation scenarios, and other elements you need to download them. For a detailed introduction, see [Resource Manager](<Apollo_alpha_doc/使用指南/可视化交互工具 Dreamview+/Resource Manager.md>).
  - Personal Center:

    - Settings:

      - General: in the general settings, you can set the language to Chinese or English.
      - Privacy: here you can choose to provide us with feedback on personalized content and features, used to improve your experience.
      - About: Dreamview version and copyright information.

    - Cloud Profile: log in to the cloud Studio.
    - Use Guide: if you are a new user, you can follow the prompts step by step.
    - Product Documentation: product introduction and product usage guide.
    - Apollo Developer Community: the Apollo developer community.
    - Advice and Suggestion: if you encounter any problems during use, please submit feedback, and we will optimize and improve based on your suggestions.

- Panel area: displays the visualization panels for autonomous driving development and debugging. Different panel types are displayed in different modes, and users can also add custom panel types.
- Operation bar: the operation bar is the area for operating on and processing data during autonomous driving development and debugging.

  - Play data package: starts the simulation scenario after the route is set,
  - Dump: downloads the channel information at a given moment to local,
  - Clear: clears all information in the Simulation world and resets the dynamics model.

## Usage Scenarios

### Default Mode

Suitable for all scenarios where you start debugging.

### Perception Development and Debugging

Suitable for developers working on the perception module. It provides data operation workflow options, visualization display panels, and debugging information panels related to perception development and debugging.

### PnC Development and Debugging

The PnC development and debugging mode is suitable for developers working on the planning and control modules. It provides data operation workflow options, visualization display panels, and debugging information panels related to PnC development and debugging.

<!-- Real-vehicle road test mode
The real-vehicle road test mode is suitable for developers performing real-vehicle verification testing. It provides data operation workflow options, visualization display panels, and debugging information panels related to real-vehicle testing. Vehicle integration mode-->

## Glossary

- Mode Settings: mode settings.
- Mode: mode.
- Modules: modules.
- Operations: operations.
- Current Resources: resources currently in use.
- Environment Resources: environment resources.
- Records: data packages.
- Scenarios: scenarios.
- ADS Resources: autonomous driving system resources.
- HDMap: HD map.
- Vehicle: vehicle.
- V2X: vehicle-road cooperation.
- Panel: panel.
- Console: console.
- Module Delay: module delay.
- Vehicle Visualization: vehicle visualization.
- Camera View: camera view.
- Point Cloud: point cloud.
- Vehicle Dashboard: vehicle dashboard panel.
