# Package Management Tutorial - Basic Concepts and Building Custom Components and Plugins

Prerequisites

Before reading and using this document, make sure you have followed the [Essential Software Installation Guide](../installation_instructions/essential_software_installation_guide_cn.md) document and completed the installation of the Apollo environment manager and the container environment

## Overview

This tutorial covers the basics of using package management to download, install, and build Apollo modules. You will set up a workspace and build a simple C++ project that illustrates the key concepts of package management in Apollo, such as the .workspace.json and .env files. After completing this tutorial, see [Apollo Development Tools - buildtool](../user_guidelines/tools/tool_buildtool_cn.md), to learn about advanced usage of buildtool, the key tool in package management.

In this tutorial, you will learn:
* The new configuration information in Apollo's package management mode
* How to create a simple sample component, then build and run it
* How to install the source code of an existing package into the workspace, modify it, and build it
* How to package a module from secondary development and deploy it to another host

## Before You Begin

In package management mode, Apollo organizes and manages packages in `projects`. Apollo officially provides several sample projects, which can be found on Apollo's GitHub home page. In this tutorial, we use the `application-pnc` project. You can get the sample project from Apollo's GitHub repository:

```shell
git clone https://github.com/ApolloAuto/application-pnc.git
```

This project declares dependencies on pnc-related packages, and by deploying it you can quickly debug and simulate Apollo's pnc packages. The directory structure of this project is as follows:

```shell
application-pnc
├── .workspace.json
├── .env
├── WORKSPACE
├── core
│   ├── BUILD
│   └── cyberfile.xml
└── profiles
    └── default
```

As you can see, this project contains no Apollo source code, only some configuration files (.workspace.json and .env) and the core module. Below we will deploy this project and introduce the concepts mentioned above step by step.

## Deploying application-pnc

Before deploying the whole project, you need to have some understanding of the files in the workspace. The workspace, that is, the whole project directory, holds the module source code. It also contains some special files recognized by the Apollo build toolchain:

`WORKSPACE`: This is the WORKSPACE file in bazel. It identifies this directory and its contents as a Bazel workspace and is located at the root of the project directory structure. Unless you need to additionally introduce third-party libraries recognizable by bazel, this file generally does not need to be modified.

`.workspace.json`: This file is also located at the root of the project. It indicates where the project downloads its packages from, as well as the package version. Its content is as follows:

```json
{"repositories" : [{"name": "apollo-core", "version": "9.0.0-alpha2-r28"}]}
```

The `repositories` field contains package repository information, and `name` is the name of the package repository. Currently, Apollo has two repositories for users to choose from:
* apollo-core:      All package repositories of the Apollo open-source codebase (X86 architecture)
* apollo-core-arm:  All package repositories of the Apollo open-source codebase (ARM architecture)

And `version` is the version of the packages downloaded for this project.

When you want to upgrade the whole project from version `9.0.0-alpha2-r28` to version `9.0.0-alpha2-r29`, simply modify this file to the following content:

```json
{"repositories" : [{"name": "apollo-core", "version": "9.0.0-alpha2-r29"}]}
```

Then redeploy the project, and the packages the project depends on will be automatically upgraded to version `9.0.0-alpha2-r29`.

> Note: Since the external interfaces of Apollo modules may change between versions, the project's source code may need additional adaptation after upgrading the version.

`.env`: Apollo currently still needs to run in a docker container. This file specifies information such as the repository and tag of the image used when starting the container. Its content is as follows:

```shell
APOLLO_ENV_NAME=pnc
APOLLO_ENV_WORKLOCAL=1
APOLLO_ENV_CONTAINER_REPO='registry.baidubce.com/apollo/apollo-env-gpu'
APOLLO_ENV_CONTAINER_TAG='9.0-latest'
```

Where:
* `APOLLO_ENV_NAME` specifies the name of the container. In actual development, multiple Apollo projects are often started in one development environment. Configuring this variable solves the problem of conflicting container names.
* `APOLLO_ENV_CONTAINER_REPO` specifies the image repository
* `APOLLO_ENV_CONTAINER_TAG` specifies the tag of the image

In addition to the files mentioned above, the project also includes the "source code" of the `core` module under the `core` path, although this module does not include any source code in the traditional C++ sense.

In Apollo's package management mode, the source code of a module includes the following:
* `cyberfile.xml` (required): the description file of the module, describing the module's name, path in the workspace, dependencies, version information, and so on
* `BUILD` (required): the bazel BUILD file, describing in what form the C++ source code, Python source code, configuration files, data files, and so on in the module should be organized for building
* A series of source code, data files, configuration files, and so on (optional)

Next, we will take the core module as an example to briefly introduce the source files involved in a module.

### Understanding cyberfile

cyberfile.xml is located in the root directory of a workspace module. The directory containing the cyberfile.xml file is the module's source path, and its subdirectories are not allowed to have cyberfile.xml files; that is, packages do not support nesting.

The content of core's cyberfile.xml is as follows:

```xml
<package>
  <name>core</name>
  <version>local</version>
  <description>
      depends of apollo core
  </description>
  <maintainer email="apollo-support@baidu.com">Apollo Maintainer</maintainer>
  <type>module</type>
  <src_path>//core</src_path>
  <license>Apache License 2.0</license>
  <author>Apollo</author>

  <!-- basic -->
  <depend repo_name="common" type="binary">common</depend>
  <depend repo_name="cyber" type="binary">cyber</depend>
  <depend repo_name="common-msgs" type="binary">common-msgs</depend>

  <!-- dreamview && monitor -->
  <depend repo_name="dreamview" type="binary">dreamview</depend>
  <depend repo_name="monitor" type="binary">monitor</depend>
  <depend repo_name="studio-connector" type="binary">studio-connector</depend>
  <depend repo_name="sim-obstacle" type="binary">sim-obstacle</depend>

  <!-- external_command -->
  <depend repo_name="external-command-action" type="binary">external-command-action</depend>
  <depend repo_name="external-command-demo" type="binary">external-command-demo</depend>
  <depend repo_name="external-command-lane-follow" type="binary">external-command-lane-follow</depend>
  <depend repo_name="external-command-process" type="binary">external-command-process</depend>
  <depend repo_name="external-command-processor-base" type="binary">external-command-processor-base</depend>
  <depend repo_name="external-command-valet-parking" type="binary">external-command-valet-parking</depend>
  <depend repo_name="old-routing-adpter" type="binary">old-routing-adpter</depend>
  <depend repo_name="routing" type="binary">routing</depend>

  <!-- planning -->
  <depend repo_name="planning" type="binary">planning</depend>
  <depend repo_name="planning-scenario-bare-intersection-unprotected" type="binary">planning-scenario-bare-intersection-unprotected</depend>
  <depend repo_name="planning-scenario-emergency-pull-over" type="binary">planning-scenario-emergency-pull-over</depend>
  <depend repo_name="planning-scenario-emergency-stop" type="binary">planning-scenario-emergency-stop</depend>
  <depend repo_name="planning-scenario-lane-follow" type="binary">planning-scenario-lane-follow</depend>
  <depend repo_name="planning-scenario-park-and-go" type="binary">planning-scenario-park-and-go</depend>                                                                                         
  <depend repo_name="planning-lane-follow-map" type="binary">planning-lane-follow-map</depend>
  <depend repo_name="planning-scenario-pull-over" type="binary">planning-scenario-pull-over</depend>
  <depend repo_name="planning-scenario-stop-sign-unprotected" type="binary">planning-scenario-stop-sign-unprotected</depend>
  <depend repo_name="planning-scenario-traffic-light-protected" type="binary">planning-scenario-traffic-light-protected</depend>
  <depend repo_name="planning-scenario-traffic-light-unprotected-left-turn" type="binary">planning-scenario-traffic-light-unprotected-left-turn</depend>
  <depend repo_name="planning-scenario-traffic-light-unprotected-right-turn" type="binary">planning-scenario-traffic-light-unprotected-right-turn</depend>
  <depend repo_name="planning-scenario-valet-parking" type="binary">planning-scenario-valet-parking</depend>
  <depend repo_name="planning-scenario-yield-sign" type="binary">planning-scenario-yield-sign</depend>
  <depend repo_name="planning-task-fallback-path" type="binary">planning-task-fallback-path</depend>
  <depend repo_name="planning-task-lane-borrow-path" type="binary">planning-task-lane-borrow-path</depend>
  <depend repo_name="planning-task-lane-change-path" type="binary">planning-task-lane-change-path</depend>
  <depend repo_name="planning-task-lane-follow-path" type="binary">planning-task-lane-follow-path</depend>
  <depend repo_name="planning-task-open-space-fallback-decider" type="binary">planning-task-open-space-fallback-decider</depend>
  <depend repo_name="planning-task-open-space-pre-stop-decider" type="binary">planning-task-open-space-pre-stop-decider</depend>
  <depend repo_name="planning-task-open-space-roi-decider" type="binary">planning-task-open-space-roi-decider</depend>
  <depend repo_name="planning-task-open-space-trajectory-partition" type="binary">planning-task-open-space-trajectory-partition</depend>
  <depend repo_name="planning-task-open-space-trajectory-provider" type="binary">planning-task-open-space-trajectory-provider</depend>
  <depend repo_name="planning-task-path-decider" type="binary">planning-task-path-decider</depend>
  <depend repo_name="planning-task-path-reference-decider" type="binary">planning-task-path-reference-decider</depend>
  <depend repo_name="planning-task-path-time-heuristic" type="binary">planning-task-path-time-heuristic</depend>
  <depend repo_name="planning-task-piecewise-jerk-speed" type="binary">planning-task-piecewise-jerk-speed</depend>
  <depend repo_name="planning-task-piecewise-jerk-speed-nonlinear" type="binary">planning-task-piecewise-jerk-speed-nonlinear</depend>
  <depend repo_name="planning-task-pull-over-path" type="binary">planning-task-pull-over-path</depend>
  <depend repo_name="planning-task-reuse-path" type="binary">planning-task-reuse-path</depend>
  <depend repo_name="planning-task-rss-decider" type="binary">planning-task-rss-decider</depend>
  <depend repo_name="planning-task-rule-based-stop-decider" type="binary">planning-task-rule-based-stop-decider</depend>
  <depend repo_name="planning-task-speed-bounds-decider" type="binary">planning-task-speed-bounds-decider</depend>
  <depend repo_name="planning-task-speed-decider" type="binary">planning-task-speed-decider</depend>
  <depend repo_name="planning-task-st-bounds-decider" type="binary">planning-task-st-bounds-decider</depend>
  <depend repo_name="planning-traffic-rules-backside-vehicle" type="binary">planning-traffic-rules-backside-vehicle</depend>
  <depend repo_name="planning-traffic-rules-crosswalk" type="binary">planning-traffic-rules-crosswalk</depend>
  <depend repo_name="planning-traffic-rules-destination" type="binary">planning-traffic-rules-destination</depend>
  <depend repo_name="planning-traffic-rules-keepclear" type="binary">planning-traffic-rules-keepclear</depend>
  <depend repo_name="planning-traffic-rules-rerouting" type="binary">planning-traffic-rules-rerouting</depend>
  <depend repo_name="planning-traffic-rules-stop-sign" type="binary">planning-traffic-rules-stop-sign</depend>
  <depend repo_name="planning-traffic-rules-traffic-light" type="binary">planning-traffic-rules-traffic-light</depend>
  <depend repo_name="planning-traffic-rules-yield-sign" type="binary">planning-traffic-rules-yield-sign</depend>
  <depend repo_name="planning-traffic-rules-reference-line-end" type="binary">planning-traffic-rules-reference-line-end</depend>

  <!-- transform -->
  <depend repo_name="transform" type="binary">transform</depend>

  <!-- prediction -->
  <depend repo_name="prediction" type="binary">prediction</depend>

  <!-- control -->
  <depend repo_name="control-controller-demo-control-task" type="binary">control-controller-demo-control-task</depend>
  <depend repo_name="control-controller-lat-based-lqr-controller" type="binary">control-controller-lat-based-lqr-controller</depend>
  <depend repo_name="control-controller-lon-based-pid-controller" type="binary">control-controller-lon-based-pid-controller</depend>
  <depend repo_name="control-controller-mpc-controller" type="binary">control-controller-mpc-controller</depend>
  <depend repo_name="control" type="binary">control</depend>

  <!-- map -->
  <depend repo_name="map" type="binary">map</depend>

  <builder>bazel</builder>
</package>
```

Some of the more important attributes:

- name: the name of the module.
- version: the module version number; since it is local source code, the version is local.
- type: the type of the module, which must be module.
- src_path: the path of the module's source code in the workspace.
- depend: the dependencies of this module; the depend tag has some attributes:
    - repo_name: the same as the name of the dependent module.
    - type: the form in which the dependency is introduced; src and binary are available. src introduces the dependency in source form, that is, the build toolchain copies the dependency's source code into the workspace and builds it together; binary introduces the dependency in binary form, that is, when building the current module, the build toolchain imports the dependent module's dynamic libraries and header files for building.
- builder: the underlying build tool; currently only bazel is supported.

### Understanding BUILD

The BUILD file is bazel's BUILD file. It tells Bazel how to build the required outputs, such as executable binaries or libraries.

In package management mode, Apollo provides some macros and rules on top of bazel, for example the BUILD file of core:

```python
load("//tools:apollo_package.bzl", "apollo_package")

package(default_visibility = ["//visibility:public"])

apollo_package()
```

Here `apollo_package` is a macro added in package management mode. In package management mode, this macro must be added to the BUILD file of all module source code to ensure that the module source code can be built and deployed properly.

In addition, Apollo provides corresponding rules for C++ source code, Python source code, and proto source code, for example in the BUILD file of the `cyber` source code:

```python
load("//tools:apollo_package.bzl", "apollo_cc_library", "apollo_package")

apollo_cc_library(
    name = "cyber_state",
    srcs = ["state.cc",],
    hdrs = ["state.h",], 
    deps = ["//cyber/common:cyber_common",],
)
```

In package management mode, please use the rules provided by Apollo, which are:
- apollo_cc_library: declares that this target generates a dynamic library, replacing bazel's native cc_library
- apollo_cc_binary: declares that this target generates an executable, replacing bazel's native cc_binary
- apollo_cc_test: declares that this target generates a test executable, replacing bazel's native cc_test
- apollo_component: declares that this target generates an Apollo component
- apollo_plugin: declares that this target generates an Apollo plugin

The apollo_cc_library, apollo_cc_binary, and apollo_cc_test rules are used in the same way as bazel's native rules.

How to use the two new rules apollo_component and apollo_plugin will be introduced below.

### Starting the Apollo Container

Starting the Apollo container requires the Apollo environment manager aem. For the installation of this tool, see the [Essential Software Installation Guide](../installation_instructions/essential_software_installation_guide_cn.md) document.

#### Start the Container

```shell
cd application-pnc && aem start
```

After the container is started, aem produces output similar to the following:

```shell
The home directory `/home/apollo' already exists.  Not copying from `/etc/skel'.
[ OK ] Congratulations! You have successfully finished setting up Apollo Dev Environment.
[ OK ] To login into the newly created apollo_neo_dev_pnc container, please run the following command:
[ OK ]   aem enter
[ OK ] Enjoy!
```

#### Enter the Container Environment

```shell
aem enter
```

After entering the container, you can see the text `in-dev-docker`.

#### Deploy the Project

As mentioned above, the application-pnc project contains no C++ source code, only a core module that declares its dependencies. By building this module, you can quickly download the required packages in the current environment, achieving the purpose of deploying the project.

Build the core module with the following command:

```shell
buildtool build -p core
```
> The -p parameter specifies the path of the module to build. If this parameter is not specified, buildtool will build all modules in the workspace.

After the command is entered, buildtool automatically downloads the dependencies. Depending on network conditions, this process may last 30 - 60 minutes, so please be patient.

```shell
[buildtool] 2023-10-27 14:15:04 INFO Import depends...
[buildtool] 2023-10-27 14:15:04 INFO Preprocess 3rd-osqp
[buildtool] 2023-10-27 14:15:04 INFO Install 3rd-osqp...
[buildtool] 2023-10-27 14:15:04 INFO install 3rd-osqp...
[buildtool] 2023-10-27 14:15:07 INFO 3rd-osqp successfully installed
[buildtool] 2023-10-27 14:15:07 INFO PostProcess 3rd-osqp
[buildtool] 2023-10-27 14:15:07 INFO Import depends...
[buildtool] 2023-10-27 14:15:07 INFO Preprocess libhdf5-dev
[buildtool] 2023-10-27 14:15:08 INFO Install libhdf5-dev...
[buildtool] 2023-10-27 14:15:16 INFO libhdf5-dev successfully installed
[buildtool] 2023-10-27 14:15:16 INFO PostProcess libhdf5-dev
```

After buildtool finishes deploying the project, the software and tools required for pnc simulation are installed in the current environment, and you can start dreamview to debug planning.

Start dreamview with the following command:

```shell
aem bootstrap
```

Then access `localhost:8888` from a browser to reach dreamview, and start the simulation and pnc for debugging.

Or start the planning component manually:

```shell
mainboard -d /apollo/modules/planning/planning_base/dag/planning.dag
```

#### Install Source Code into the Workspace

Besides deployment, developers may want to view or modify the source code of a module or plugin, and can install a module with the following command:

```shell
buildtool install planning-task-lane-follow-path 
```

This command installs the source code of `planning-task-lane-follow-path` into the workspace, and the source code can be found under `modules/planning/planning_base/tasks/lane_follow_path`.

You can modify the code above to change the behavior of the task, and then build it to make it take effect.

```shell
buildtool build -p modules/planning/
```

The command above builds all modules under the `modules/planning` path.

Then restart planning through `dreamview`, or rerun planning from the command line, to debug further.

When you want to discard the locally built result and restore the original behavior of `planning-task-lane-follow-path`, you can reinstall the module with the following command:

```shell
rm -f modules/planning/planning_base/tasks/lane_follow_path && buildtool reinstall planning-task-lane-follow-path 
```

Then restart planning.

#### Create an Apollo Component

Besides modifying Apollo source code, you may want to create an Apollo component of your own, that is, a component.

buildtool provides a convenient command to create a component template:

```shell
buildtool create --template component sample_component
```
> The --template parameter declares that the template to create is component, and sample_component declares the source path of this component.

After execution, there will be output similar to the following:

```shell
[buildtool] 2023-10-27 14:54:25 INFO Reconfigure apollo enviroment setup
template_vars:  {'package_path': 'sample_component', 'package_name': 'sample-component', 'target_name': 'sample_component', 'class_name': 'SampleComponent', 'base_class_name': 'SampleComponentBase', 'config_message_name': 'SampleComponentConfig', 'namespaces': [], 'includes': [], 'dependencies': [], 'build_dependencies': [], 'channel_name': '/apollo/sample_component', 'channel_message_type': 'apollo::SampleComponentMsg', 'package_type': 'module', 'email': 'sample@sample.com', 'author': 'Apollo Developer', 'description': 'This is a demo package', 'config_file_name': 'sample_component'}
[buildtool] 2023-10-27 14:54:25 INFO sample_component/proto/sample_component.proto create success
[buildtool] 2023-10-27 14:54:25 INFO sample_component/sample_component.h create success
[buildtool] 2023-10-27 14:54:25 INFO sample_component/sample_component.cc create success
[buildtool] 2023-10-27 14:54:25 INFO sample_component/proto/BUILD create success
[buildtool] 2023-10-27 14:54:25 INFO sample_component/BUILD create success
[buildtool] 2023-10-27 14:54:25 INFO sample_component/dag/sample_component.dag create success
[buildtool] 2023-10-27 14:54:25 INFO sample_component/launch/sample_component.launch create success
[buildtool] 2023-10-27 14:54:25 INFO sample_component/cyberfile.xml create success
[buildtool] 2023-10-27 14:54:25 INFO sample_component/conf/sample_component.pb.txt create success
[buildtool] 2023-10-27 14:54:25 INFO sample_component/conf/sample_component.conf create success
[buildtool] 2023-10-27 14:54:25 INFO create component package sample-component on sample_component success
```

A sample_component directory will appear in the workspace:

```shell
sample_component/
|-- BUILD
|-- conf
|   |-- sample_component.conf
|   `-- sample_component.pb.txt
|-- cyberfile.xml
|-- dag
|   `-- sample_component.dag
|-- launch
|   `-- sample_component.launch
|-- proto
|   |-- BUILD
|   `-- sample_component.proto
|-- sample_component.cc
`-- sample_component.h
```

Where:
* BUILD: tells bazel how to build this component
* conf: holds the configuration files of this component; the content of sample_component.pb.txt is parsed into the SampleComponentConfig type defined by sample_component.proto, and sample_component.conf declares some gflags
* dag, launch: hold the startup files of this component
* proto: holds the proto message definitions of the component's configuration and messages
* C++ source code: describes the component's initialization and its behavior upon receiving the corresponding messages

This sample component listens to the `/apollo/sample_component` channel and prints the corresponding message in the terminal after receiving a message.

Since this sample is a component, the apollo_component rule is used in the BUILD file:

```python
apollo_component(
    name = "libsample_component_component.so",
    srcs = [
        "sample_component.cc",
    ],
    hdrs = [
        "sample_component.h",
    ],
    deps = [
        "//cyber",
        "//sample_component/proto:sample_component_proto",
    ],
)
```

The apollo_component rule declares that this target is a component, where:

* name: the name of the dynamic library produced by this component, consistent with the dynamic library name read in the dag file
* srcs: the source files of this component
* hdrs: the header files of this component
* deps: the dependencies of this component; here it depends on the cyber interfaces and the target declared under the component's own proto

This sample can be built directly:

```shell
buildtool build -p sample_component
```

After the build completes, simply run it:

```shell
mainboard -d sample_component/dag/sample_component.dag
```

You can also modify the component's initialization and its behavior upon receiving messages; after modifying the code, just build it.

#### Create an Apollo Plugin

As with components, you can use buildtool's create command to create a plugin:

```shell
buildtool create --template plugin --base_class_name apollo::planning::Task sample_plugin --dependencies planning
```
> A plugin needs to inherit from a plugin base class. The --base_class_name parameter in the command above declares inheriting the Task class in planning, sample_plugin declares the source path of this plugin, and the --dependencies parameter adds a dependency on planning in cyberfile.xml.

A sample_plugin directory will appear in the workspace:

```shell
sample_plugin/
|-- BUILD
|-- conf
|   `-- sample_plugin.pb.txt
|-- cyberfile.xml
|-- plugin_sample_plugin_description.xml
|-- proto
|   |-- BUILD
|   `-- sample_plugin.proto
|-- sample_plugin.cc
`-- sample_plugin.h
```

Compared with components, plugins add a description file `plugin_sample_plugin_description.xml`:
```xml
<library path="sample_plugin/libsample_plugin.so">
    <class type="apollo::SamplePlugin" base_class="apollo::planning::Task"></class>
</library>
```

This file describes the name of the plugin's dynamic library, the plugin's class name, and the name of the base class it inherits from, so that cyber can load the plugin correctly.

Unlike components, because plugin base classes are defined differently, the plugin template created by the command above is only a most basic template that cannot be built directly; developers need to fill in the corresponding information before it can be built. The information developers need to fill in is described below.

##### Included Header Files

In the create command, the base class to inherit from was specified as apollo::planning::Task. In fact, sample_plugin.h merely inherits from apollo::planning::Task and does not include the corresponding header file:

```c++
#pragma once

#include <memory>
#include "cyber/plugin_manager/plugin_manager.h"

namespace apollo {

class SamplePlugin : public apollo::planning::Task {
 public:
  bool Init();
};

CYBER_PLUGIN_MANAGER_REGISTER_PLUGIN(apollo::SamplePlugin, apollo::planning::Task)

} // namespace apollo
```

Developers can refer to the planning interface documentation to add the header file corresponding to `apollo::planning::Task`, and modify the function signatures according to the base class definition:

```c++
#pragma once

#include <memory>
#include "cyber/plugin_manager/plugin_manager.h"
#include "modules/planning/planning_base/task_base/task.h"

namespace apollo {

class SamplePlugin : public apollo::planning::Task {
 public:
  bool Init(const std::string &config_dir, const std::string &name,
            const std::shared_ptr<apollo::planning::DependencyInjector> &injector) override;

  apollo::common::Status Execute(apollo::planning::Frame *frame, 
    apollo::planning::ReferenceLineInfo *reference_line_info) override;
};

CYBER_PLUGIN_MANAGER_REGISTER_PLUGIN(apollo::SamplePlugin, apollo::planning::Task)

} // namespace apollo
```

##### Modify the Source Implementation File

Since the function signatures have been modified, the corresponding implementations also need to be modified accordingly. The following is the cc file initialized by the create command:

```c++
#include <memory>
#include "sample_plugin/sample_plugin.h"

namespace apollo {

bool SamplePlugin::Init() {
  return true;
}

} // namespace apollo
```

The modified cc file:

```c++
#include <memory>
#include "sample_plugin/sample_plugin.h"

namespace apollo {

bool SamplePlugin::Init(const std::string &config_dir, const std::string &name,
            const std::shared_ptr<apollo::planning::DependencyInjector> &injector) {
  return true;
}

apollo::common::Status SamplePlugin::Execute(apollo::planning::Frame *frame,
        apollo::planning::ReferenceLineInfo *reference_line_info) {
    apollo::planning::Task::Execute(frame, reference_line_info);
    return apollo::common::Status::OK();
}

} // namespace apollo
```


##### Modify the BUILD File and Add the Dependent Target

Likewise, the plugin's BUILD file itself does not include the target corresponding to the base class. Developers can refer to the BUILD file of the dependent module to add the target corresponding to the header file.

The following is the BUILD file initialized by the create command:
```python
load("//tools:apollo.bzl", "cyber_plugin_description")
load("//tools:apollo_package.bzl", "apollo_cc_library", "apollo_package", "apollo_plugin")
load("//tools:cpplint.bzl", "cpplint")

package(default_visibility = ["//visibility:public"])

filegroup(
    name = "sample_plugin_files",
    srcs = glob([
        "conf/**",
    ]),
)

apollo_plugin(
    name = "libsample_plugin.so",
    srcs = [
        "sample_plugin.cc",
    ],
    hdrs = [
        "sample_plugin.h",
    ],
    description = ":plugin_sample_plugin_description.xml",
    deps = [
        "//cyber",
        "//sample_plugin/proto:sample_plugin_proto",

    ],
)

apollo_package()
```

The target corresponding to `apollo::planning::Task` is `"//modules/planning/planning_base:apollo_planning_planning_base"`, so after modifying the `BUILD` file:

```python
load("//tools:apollo.bzl", "cyber_plugin_description")
load("//tools:apollo_package.bzl", "apollo_cc_library", "apollo_package", "apollo_plugin")
load("//tools:cpplint.bzl", "cpplint")

package(default_visibility = ["//visibility:public"])

filegroup(
    name = "sample_plugin_files",
    srcs = glob([
        "conf/**",
    ]),
)

apollo_plugin(
    name = "libsample_plugin.so",
    srcs = [
        "sample_plugin.cc",
    ],
    hdrs = [
        "sample_plugin.h",
    ],
    description = ":plugin_sample_plugin_description.xml",
    deps = [
        "//cyber",
        "//sample_plugin/proto:sample_plugin_proto",
        "//modules/planning/planning_base:apollo_planning_planning_base",
    ],
)

apollo_package()
```

This BUILD file uses the apollo_plugin rule to declare that this target generates a plugin, where:

* name: the name of the dynamic library produced by this component, consistent with the dynamic library name read in the plugin description file
* description: the name of the plugin description file
* srcs: the source files of this component
* hdrs: the header files of this component
* deps: the dependencies of this component; here it depends on the cyber interfaces and the target declared under the component's own proto

After the modifications, you can build with the following command:

```shell
buildtool build -p sample_plugin
```

After the build is complete, the plugin can be loaded and used by the planning component. The plugin built following the steps above is only an empty plugin with no behavior of its own; to have the plugin plan vehicle trajectories, you also need to write the corresponding business code. In addition, for the plugin to be loaded and called by planning, the planning configuration file needs to be modified; for details, see planning's getting-started tutorial, which is not elaborated here.

#### Package a Locally Developed Module and Deploy It to Another Host

After finishing local development, developers may need to package the locally developed module and deploy it to another host. Here, taking the plugin built above as an example, we describe how to package and deploy in package management mode.

##### Packaging

buildtool packages modules with the following command:

```shell
buildtool release -p sample_plugin core
```
> The -p parameter specifies the source path of the module to package. When this parameter is not added, buildtool tries to package all modules in the workspace.

After packaging is complete, buildtool produces output similar to the following:

```shell
[buildtool] 2023-10-27 19:21:53 INFO update the local cache
[buildtool] 2023-10-27 19:21:53 INFO update complete
[buildtool] 2023-10-27 19:21:53 INFO Reconfigure apollo enviroment setup
[buildtool] 2023-10-27 19:21:53 INFO use src code of planning-task-lane-follow-path in workspace
[buildtool] 2023-10-27 19:21:56 INFO Analyzing dependencies topological graph...
[buildtool] 2023-10-27 19:21:56 INFO building package process started, it will take while...
dpkg-deb: building package 'apollo-neo-sample-plugin' in 'apollo-neo-sample-plugin_9.0.0-alpha2-r28_amd64.deb'.
dpkg-deb: building package 'apollo-neo-core' in 'apollo-neo-core_9.0.0-alpha2-r28_amd64.deb'.
[buildtool] 2023-10-27 19:21:57 INFO Done! Productions is in /apollo_workspace/.deb_local
[buildtool] 2023-10-27 19:21:57 INFO Compress the release files...
[buildtool] 2023-10-27 19:21:57 INFO Release complete, the output files: release.tar.gz
```

A release.tar.gz file is generated in the workspace, which can be used for subsequent deployment on other hosts.

##### Deployment

> Before deployment, make sure the aem tool has been downloaded on the other host to start the Apollo container.

First, create a workspace for deployment:

```shell
mkdir deployment_workspace
```

Then copy the release.tar.gz and .env files from the previous step to the new host.

Next, start the container through aem in the same way:

```shell
cd deployment_workspace && aem start
```

After startup, call buildtool inside the container to deploy the project:

```shell
buildtool deploy -f release.tar.gz 
```

In this process buildtool automatically deploys the two modules sample-plugin and core and automatically downloads the required dependencies from the network. Depending on network conditions, this process may take 30-60 minutes.

After deployment, you can start dreamview with the `aem bootstrap start` command, or start planning manually through mainboard, just like in the environment you just developed in.


## What's Next...
Congratulations! By reading this document, you have mastered some basic concepts and development modes of Apollo package management. Next, you can:
* Read the introduction to the aem development tool to learn more about aem's container management functions
* Read the buildtool development tool documentation to learn more about advanced usage of buildtool
* Read the tutorials for Apollo modules to learn how to do secondary development
