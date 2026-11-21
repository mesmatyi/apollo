## What Is Dreamview+?

Dreamview+ is a web application that provides the following functions:

- Visualizes the output of the current autonomous vehicle modules. For example: planned path, vehicle localization, chassis information, and so on.
- Provides users with a human-machine interface to monitor vehicle hardware status, switch modules on and off, start the autonomous vehicle, and so on.
- Provides debugging tools. For example: the PnC Monitor can efficiently track problems in module output.
  However, most of the features of Dreamview+ are mainly intended for use during debugging on a real vehicle.
  In the quick start, only the visualization feature of Dreamview+ is used to visualize the data in a record data package.

## Overall Layout

![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Doc_CN_9_0/image_1445a63.png)

- Sidebar: displays the menu buttons: **Mode Settings**, **Add Panel**, **Resource Manager**, and **Personal Center**.

- Menu window: displays the details of each menu button.
  
  - Mode Settings: the mode settings toolbar. For details, see [Mode Settings](<Apollo_alpha_doc/使用指南/可视化交互工具 Dreamview+/Mode Settings.md>).
  
  - Add Panel: adds panels, including Console, Module Delay, Vehicle Visualization, Camera View, Point Cloud View, Vehicle Dashboard View, and others. For details about panels, see [Add Panel](<Apollo_alpha_doc/使用指南/可视化交互工具 Dreamview+/Add Panel.md>).
  
  - Resource Manager: the vehicle-cloud integrated configuration center provided by Apollo, offering downloads of assets such as vehicles, models, data packages and simulation scenarios. You can click the data packages, vehicles, models, simulation scenarios and other elements you need to download them. For details, see [Resource Manager](<Apollo_alpha_doc/使用指南/可视化交互工具 Dreamview+/Resource Manager.md>).
  
  - Personal Center:
    
    - Settings:
      
      - General: in the general settings, you can set the language to Chinese or English.
      - Privacy: here you can choose to provide us with feedback on personalized content and features, which is used to improve your experience.
      - About: Dreamview version and copyright information.
    
    - Cloud Profile: log in to the cloud Studio.
    
    - Use Guide: if you are a new user, you can follow the prompts step by step.
    
    - Product Documentation: product introduction and user guides.
    
    - Apollo Developer Community: the Apollo developer community.
    
    - Advice and Suggestion: if you run into any problems during use, please submit feedback, and we will optimize and improve based on your suggestions.

- Panel area: displays the visualization panels for autonomous driving development and debugging. Different panel types are displayed in different modes, and users can also add custom panel types.

- Bottom operation bar: the bottom operation bar is the area for operating on and processing data during autonomous driving development and debugging.
  
  - Play data package: starts the simulation scenario after the route is set,
  - Dump: downloads the channel information at a given moment to the local machine,
  - Clear: clears all information in the Simulation world and resets the dynamics model.

## Usage Scenarios

### Default Mode

Applicable to all scenarios when you start debugging.

### Perception Development and Debugging

Applicable to developers working on the perception module. It provides data operation workflow options, visualization panels and debugging information panels related to perception development and debugging.

### PnC Development and Debugging

The PnC development and debugging mode is applicable to developers working on the planning and control modules. It provides data operation workflow options, visualization panels and debugging information panels related to PnC development and debugging.

<!-- Real-vehicle road test mode
The real-vehicle road test mode is applicable to developers performing real-vehicle verification tests. It provides data operation workflow options, visualization panels and debugging information panels related to real-vehicle testing. Vehicle integration mode-->

## Glossary

- Mode Settings: mode settings.

- Mode: mode.

- Modules: modules.

- Operations: operations.

- Current Resources: the resources currently in use.

- Environment Resources: environment resources.

- Records: data packages.

- Scenarios: scenarios.

- ADS Resources: autonomous driving system resources.

- HDMap: HD map.

- Vehicle: vehicle.

- V2X: vehicle-road cooperation.

- Panel: panel.

- Console: console.

- Module Delay: module latency.

- Vehicle Visualization: vehicle visualization.

- Camera View: camera view.

- Point Cloud: point cloud.

- Vehicle Dashboard: vehicle dashboard panel.

## Port Number Update

The port number of Dreamview 1.0 is 8899, and the port number of Dreamview+ is 8888.
