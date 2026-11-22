## Overview

First, what is a plugin? It is simply a program written following a certain application programming interface specification, which can extend features or functions that did not originally exist for our application or system. Once an application has a plugin mechanism, both the application maintainers and third-party developers can extend functionality in a simpler way to handle more and more complex scenarios. At the same time, a plugin mechanism keeps the core logic of the application leaner and easier to understand and use.

In Cyber, the Component we are familiar with is actually also a kind of plugin. We only need to override the apollo::cyber::Component::Init and apollo::cyber::Component::Proc functions to easily write programs that can communicate with components such as perception and planning, thereby extending new autonomous driving capabilities.

![apollo-plugins](./images/apollo_plugins.png)

## Plugin Mechanism Design

Overall, the plugin mechanism has several important concepts: the plugin manager class, the plugin base class and plugin class, and the plugin description file.

### Plugin Manager Class

![apollo_plugins_class_plugin_manager](./images/apollo_plugins_class_plugin_manager.png)

The plugin manager class is implemented in the cyber/plugin_manager directory. It is a singleton, containing two core methods and a plugin registration macro.

- `apollo::cyber::plugin_manager::PluginManager::LoadInstalledPlugins`: used to scan and load plugins. During loading, an index between plugin base classes and plugin classes is also built, which is convenient for later instantiation. This method is already executed by default in mainboard's startup process, so users who want to use it in a Component do not need to call it themselves; if they want to use it in other custom binary programs, they need to call it actively;

- `apollo::cyber::plugin_manager::PluginManager::CreateInstance`: used to create plugin instances. This method is a template method that takes a string parameter, the plugin class name, and returns a pointer to the plugin base class. Therefore, for the logic of the core flow, only the base class needs to be known, and there is no need to know the definition of the plugin class, which decouples and isolates the plugin functionality from the application;

- `CYBER_PLUGIN_MANAGER_REGISTER_PLUGIN`: used to bind a plugin class to its plugin base class. Its logic is similar to component registration: when the dynamic library file is loaded, it is executed automatically and calls the registration method in class_loader. Only registered plugins can be created by the CreateInstance method

### Plugin Base Class and Plugin Class

![apollo_plugins_class_plugin](./images/apollo_plugin_class_plugin.png)

The plugin base class is the plugin designer's abstraction of the business logic.

For example, for `apollo:planning::TrafficRule` in the planning module, the two core interface methods are `apollo::planning::TrafficRule::Init` and `apollo::planning::TrafficRule::ApplyRule`. Each rule plugin calls the `Init` method when initialized, and calls the `ApplyRule ` method when the rule is applied;

As another example, for `apollo::planning::Task`, the core interfaces are `apollo::planning::Task::Init` and `apollo::planning::Task::Execute`. Likewise, the `Init` method is called at initialization, and the `Execute` method is called when the plugin is invoked (that is, when a planning task is executed);

As you can see, the plugin base class is actually the interface specification between the plugin and the application. It defines the basic ... of the plugin and how we should write plugins, while the plugin class is the concrete plugin implementation, as long as it inherits the base class and calls the plugin registration

### Plugin Description File

This is the file that describes the relationships among the plugin class, the base class, and the library they reside in. At load time, the plugin description file is parsed into the apollo::cyber::plugin_manager::PluginDescription structure and indexed by plugin class name, so the base class and the library file a plugin belongs to can be found quickly through the plugin class name.

> Example: `modules/planning/scenarios/lane_follow/plugins.xml`

## How Plugins Work

The plugin workflow is basically divided into 3 phases: the loading phase, the instantiation phase, and the invocation phase.

![apollo_plugins_phases](./images/apollo_plugins_phases.png)

- First is the loading phase. Through the loading method of the plugin manager class, the plugin installation directory can be scanned and the plugin description files read. In this phase, the plugin index is built according to information such as the dynamic library path of the plugin class, the class name, and the base class name.

  In the code implementation in mainboard (cyber/mainboard/mainboard.cc), when mainboard loads component modules (apollo::cyber::mainboard::ModuleController::LoadAll), it calls `apollo::cyber::plangin_manager::PluginManager::LoadPlugin` according to the command-line arguments to load a specified plugin individually, or calls `apollo::cyber::plugin_manager::PluginManager::LoadInstalledPlugins` to scan the plugin installation directory

- Next is instantiation, or the initialization phase. In this phase, the plugin designer can determine which plugins are enabled through configuration files, flags, and so on, and then instantiate the plugins for use through the plugin manager class's instance creation method.

  In the initialization process of Scenario in the planning module (apollo::planning::ScenarioManager::Init), ScenarioManager reads the Scenario plugin list from the configuration, then initializes them one by one through CreateInstance and stores them in `scenario_list_` to be used later

- Finally is the invocation phase. In this phase, if the conditions for using a plugin are met, the plugin's methods are called directly through the already instantiated plugin pointer.

  Taking the Scenario of the planning module as an example again, in the apolo::planning::ScenarioManager::Update method, all plugins in `scenario_list_` are traversed, and for the plugin that satisfies the condition `scenario->IsTransferable(current_scenario_.get(), frame)`, the `Scenario` switching logic is executed, that is, the `apollo::planning::Scenario::Exit` method is called to exit the current `Scenario`, and then the Enter method of the scenario that satisfies the condition is called to enter the new Scenario

## What Are Plugins Used For

Plugins are in fact very widely used. For the Linux kernel, for example, modules such as various hardware drivers are plugins of the kernel; or, DLC and MOD in games are also a kind of plugin.

Back to Apollo: we have made the planning module pluggable, abstracting base classes such as Scenario, Task, and TrafficRule. Developers only need to override these base class methods as needed to easily implement planning capabilities for different scenarios;

## NEXT

Now that we have a basic understanding of the plugin concept, you can practice and learn by following the steps in the [Monitoring CPU and Memory Information Through Plugins](https://apollo.baidu.com/community/course/47) experiment project on the cloud experiment platform of Apollo Studio, or after installing Apollo 9.0 in your local environment
