# Apollo Development Tool - buildtool

## Overview

Apollo buildtool is a command-line tool that provides functions such as compiling, testing, installing, and running Apollo modules. With buildtool, you can not only conveniently install the binary packages of the individual Apollo modules, but also perform secondary development, compilation, and testing on their source code without downloading the entire Apollo. buildtool lets developers focus only on the modules they need to develop, improving overall development efficiency.

## Installation

buildtool currently only runs inside the Apollo environment container. If you have not started a container yet, you can use the apollo environment tool `aem` to start or enter an Apollo environment container.

The Apollo environment container comes with Apollo buildtool pre-installed. After entering the container, you can enter the following command to check whether buildtool is installed correctly.

```shell
buildtool -v
```

If everything goes well, you will see the currently installed version of buildtool:

```shell
buildtool -v
9.0.0-alpha3-r3
```

You can also reinstall or update buildtool with the following command

```shell
sudo apt update -y && sudo apt install --only-upgrade apollo-neo-buildtool
```

## Usage

### buildtool Subcommands

The subcommands of buildtool are the entry points to all the functions buildtool provides. They are invoked in the following form:

```shell
buildtool <action> [action arguments or options]
```

The different functions of buildtool are organized into different actions, similar to common command-line tools such as git or apt.

Actions cover all the functions of buildtool; for example, build handles compilation, install handles installation, and clean handles cleaning the compilation cache.

Some actions may require additional arguments; you can type `-h` or `--help` after the action to see the detailed arguments

### Current Subcommands

- config - Configure buildtool
- build - Compile and install the source code in the workspace
- test - Run the unit tests of the modules in the workspace
- install - Install the source code of a specific module into the workspace
- reinstall - Reinstall the package of a specific module
- clean - Clear the compilation cache in the workspace
- init - Initialize a workspace
- info - View information about a specific package
- bootstrap - Start a module; currently only `dreamview` and `monitor` are supported
- create - Create a package from a template
- release - Package the build outputs into a distributable format
- deploy - Deploy the package produced by `release`
- map - View and download public maps

### login

login is used for login authentication when enterprise users need to install packages from repositories other than apollo-core

```shell
buildtool login username password
```

If the username and password do not match, buildtool outputs the following error message:

```shell
[apollo@in-dev-docker:~/tmp]$ buildtool login **** ****
[buildtool] 2023-10-25 14:42:48 INFO Reconfigure apollo enviroment setup
[buildtool] 2023-10-25 14:42:48 ERROR Encounter ErrCode.NetworkIoError
[buildtool] 2023-10-25 14:42:48 ERROR hint: login failed, status: 401
```

If everything is normal, buildtool outputs the following message:

```shell
[apollo@in-dev-docker:~/tmp]$ buildtool login **** *******
[buildtool] 2023-10-24 12:12:26 INFO Reconfigure apollo enviroment setup
```

### config

config is used to configure user-level settings for buildtool; user configuration overrides the default configuration

User configuration is further divided into global configuration and local configuration

- Global configuration corresponds to the settings in the `$HOME/.apollo/buildtool/config` configuration file
- Local configuration corresponds to the settings in the `$PWD/.buildtool.conf` configuration file

#### Usage

##### Set the instruction set used for compilation

```shell
buildtool config set -- compile.march -march=native
```

> Note: Because the value has a `-` prefix, to avoid argument parsing errors you need to add `--` after `set`

##### Detailed Parameters

```shell
usage: buildtool config [-h] {set,get} ...

positional arguments:
  {set,get}   config command
    set       set buildtool config
    get       get buildtool config

optional arguments:
  -h, --help  show this help message and exit
```

### build

build compiles the source code of one or more packages in the workspace. build can only be executed in the workspace root directory. If it is executed outside the workspace or in a non-root directory of the workspace, build throws an error

#### Basic Usage

##### Compile the source code of all modules in the workspace

```shell
buildtool build
```

With no arguments, buildtool compiles all the source code of the workspace. During compilation, buildtool automatically creates the dev, third_party, and tools folders to save compilation information.

##### Compile the source code of modules at specified paths in the workspace

When the `-p` or `--packages` argument is received, buildtool can compile only the source code under the specified paths

```shell
# Compile all modules under modules
[apollo@in-dev-docker:~/tmp]$ mkdir demo && cd demo
[apollo@in-dev-docker:~/tmp]$ buildtool init
[apollo@in-dev-docker:~/tmp]$ tree
.
|-- WORKSPACE
|-- example_components
|   |-- BUILD
|   |-- common_component_example.cc
|   |-- common_component_example.h
|   |-- cyberfile.xml
|   |-- dag
|   |   `-- example.dag
|   |-- launch
|   |   `-- example.launch
|   |-- message_header_test.cc
|   |-- proto
|   |   |-- BUILD
|   |   `-- examples.proto
|   |-- timer_common_component_example.cc
|   `-- timer_common_component_example.h
`-- example_lib
    |-- BUILD
    |-- cyberfile.xml
    |-- external_lib.cc
    `-- external_lib.h
[apollo@in-dev-docker:~/tmp]$ buildtool build -p example_components example_lib
```

The above commands initialized the workspace, created a component example, and compiled the example source code. If everything goes well, you will see the following output after compilation finishes:

```shell
...
-- Installing: /opt/apollo/neo/include/example_components/proto/examples.pb.cc
-- Installing: /opt/apollo/neo/include/example_components/proto/lib_examples_proto.a
-- Installing: /opt/apollo/neo/include/example_components/proto/lib_examples_proto.so
-- Installing: /opt/apollo/neo/python/example_components/proto/examples_pb2.py
-- Installing: /opt/apollo/neo/lib/example_components/proto/lib_examples_proto_ep_bin.so
[buildtool] 2023-10-25 14:49:05 INFO PostProcess example-components
[apollo@in-dev-docker:~/tmp]$
```

#### Advanced Usage

##### Compile the source code in opt gpu mode

```shell
buildtool build --gpu --opt
```

##### Pass compilation arguments to bazel

```shell
buildtool build --arguments="--linkopts=-lz4"
```

##### Clear the previous compilation cache before compiling

```shell
buildtool build --expunge
```

##### Specify the number of processes and the percentage of memory used for compilation

```shell
buildtool build --jobs 4 --memories 0.5
```

The above command compiles using 4 processes and 50% of the current host's memory

##### Detailed Parameters

```shell
usage: buildtool build [-h] [-p [* [* ...]]] [-a [* [* ...]]] [--gpu] [--cpu]
                       [--dbg] [--opt] [--prof] [--teleop] [--expunge]
                       [-j JOBS] [-m MEMORIES] [--ci-force-gpu]
                       [--install_dep_only]

optional arguments:
  -h, --help            show this help message and exit
  -p [* [* ...]], --packages [* [* ...]]
                        Specify the package path.
  -a [* [* ...]], --arguments [* [* ...]]
                        Pass arguments to the build system.
  --gpu                 Run build in GPU mode
  --cpu                 Run build in cpu mode
  --dbg                 Build with debugging enabled
  --opt                 Build with optimization enabled
  --prof                Build with profiler enabled
  --teleop              Run build with teleop enabled
  --expunge             Expunge the building cache before build
  -j JOBS, --jobs JOBS  Specifies the number of threads to compile in parallel
  -m MEMORIES, --memories MEMORIES
                        Specifies the percentage of memory used by compilation
  --ci-force-gpu        Force using gpu mode to build(ci only)
  --install_dep_only    Specifies only install depends
```

### test

This command runs unit tests on the source code of the workspace; what it actually executes is all the apollo_cc_test targets defined in the BUILD files of the source code

#### Usage

Run unit tests:

```shell
buildtool init
buildtool install planning
buildtool test -p modules/planning
```

In the above operations, buildtool downloads the planning module, copies the planning module's source code to the workspace, and finally compiles and tests it.

##### Compile and test in gpu mode

```shell
buildtool test --gpu --p modules/planning
```

##### Pass arguments to bazel

```shell
buildtool test --a "--linkopts=-llz4" --p modules/planning
```

##### Detailed Parameters

```shell
usage: buildtool test [-h] [-p [* [* ...]]] [--gpu] [--cpu]
                      [--arguments [* [* ...]]]

optional arguments:
  -h, --help            show this help message and exit
  -p [* [* ...]], --package_paths [* [* ...]]
                        Specify the package path.
  --gpu                 Run build in gpu mode
  --cpu                 Run build in cpu mode
  --arguments [* [* ...]]
                        Pass arguments to the build system.
```

### install

This command installs the package/source code of a specific module

#### Usage

```shell
buildtool install planning
```

In the above operation, buildtool downloads the planning module and copies the planning module's source code to the workspace.

Note: buildtool install planning only downloads the source code of the planning_component module. If you want to download all planning-related modules, you can use the wildcard "*"
```shell
buildtool install planning*
```

```shell
buildtool install --legacy planning
```

In the above operation, buildtool only downloads the planning module and does not copy the source code to the workspace.

##### Detailed Parameters

```shell
usage: buildtool install [-h] [--legacy] [packages [packages ...]]

positional arguments:
  packages Install the packages

optional arguments:
  -h, --help show this help message and exit
  --legacy legacy way to install package
```

### reinstall

This command reinstalls the package of a specific module

#### Usage

```shell
buildtool reinstall planning
```

In the above operation, buildtool downloads the planning module and copies the planning module's source code to the workspace.

##### Detailed Parameters

```shell
usage: buildtool reinstall [-h] [packages [packages ...]]

positional arguments:
  packages    Reinstall the packages

optional arguments:
  -h, --help  show this help message and exit
```

### clean

This action is used to clear the compilation cache and outputs of the workspace source code.

#### Usage

When the clean action takes no arguments, it clears all compilation caches of the workspace, which is equivalent to executing `bazel clean --expunge`

```shell
buildtool clean
```

The `packages_path` argument can specify deleting the compilation outputs of the module source code under the corresponding paths:

```shell
buildtool clean --packages_path modules/planning
```

The above operation deletes the compilation output of the planning module.

The `expunge` argument deletes the compilation outputs of all module source code on the machine.

```shell
buildtool clean --expunge
```

##### Detailed Parameters

```
usage: buildtool clean [-h] [-p [* [* ...]]] [-e]

optional arguments:
  -h, --help            show this help message and exit
  -p [* [* ...]], --packages_path [* [* ...]]
                        clean specified module build production.
  -e, --expunge         clean the build cache including production
```

### init

This action initializes a workspace.

#### Usage

When the init action has no arguments, it creates a basic workspace in the current directory:

```shell
buildtool init
```

You can also add the -w argument to the init action to copy the example_component source code into the workspace:

```shell
buildtool init -w
```

The `path` argument can specify the path where the workspace is created:

```shell
buildtool init --path ～/demo
```

##### Detailed Parameters

```shell
usage: buildtool init [-h] [-p PATH] [-w]

optional arguments:
  -h, --help            show this help message and exit
  -p PATH, --path PATH  specify workspace path
  -w, --with-examples   init workspace with example component and lib
```

### info

This action is used to list the dependencies of a specified package.

#### Usage

##### List the direct dependencies of the requested package:

```shell
buildtool info planning-dev
[buildtool] INFO Reconfigure apollo enviroment setup
[buildtool] INFO Compile parameters:
[buildtool] INFO using gpu: False
[buildtool] INFO using debug mode: False
[buildtool] INFO According parameters above to analysis depends of planning-dev
[buildtool] INFO Analyzing dependencies topological graph...
[buildtool] INFO planning-dev directly depends on the following packages:
[buildtool] INFO (3rd-gflags-dev|1.0.0.1), (3rd-absl-dev|1.0.0.1), (3rd-osqp-dev|1.0.0.1), (3rd-glog-dev|1.0.0.1)
[buildtool] INFO (3rd-proj-dev|1.0.0.1), (libtinyxml2-dev|None), (3rd-boost-dev|1.0.0.1), (3rd-opencv-dev|1.0.0.1)
[buildtool] INFO (3rd-ipopt-dev|1.0.0.1), (3rd-eigen3-dev|1.0.0.1), (libadolc-dev|None), (3rd-ad-rss-lib-dev|1.0.0.1)
[buildtool] INFO (cyber-dev|1.0.0.1), (common-dev|1.0.0.1), (map-dev|1.0.0.2), (common-msgs-dev|1.0.0.1)
[buildtool] INFO (bazel-extend-tools-dev|1.0.0.1), (3rd-mkl-dev|1.0.0.1), (3rd-libtorch-cpu-dev|1.0.0.1), (3rd-protobuf-dev|1.0.0.1)
[buildtool] INFO (3rd-rules-python-dev|1.0.0.1), (3rd-grpc-dev|1.0.0.1), (3rd-bazel-skylib-dev|1.0.0.1), (3rd-rules-proto-dev|1.0.0.1)
[buildtool] INFO (3rd-py-dev|1.0.0.1), (3rd-gpus-dev|1.0.0.1), (3rd-gtest-dev|1.0.0.1)
[buildtool] INFO Done, Enjoy!
[buildtool] INFO apollo build tool exit.
```

##### List all dependencies of the requested package:

```shell
buildtool info --with-indirect planning-dev
[buildtool] INFO Reconfigure apollo enviroment setup
[buildtool] INFO Compile parameters:
[buildtool] INFO using gpu: False
[buildtool] INFO using debug mode: False
[buildtool] INFO According parameters above to analysis depends of planning-dev
[buildtool] INFO Analyzing dependencies topological graph...
[buildtool] INFO planning-dev depends on these following packages:
[buildtool] INFO (3rd-gflags-dev|1.0.0.1), (3rd-absl-dev|1.0.0.1), (3rd-osqp-dev|1.0.0.1), (3rd-glog-dev|1.0.0.1)
[buildtool] INFO (3rd-proj-dev|1.0.0.1), (libtinyxml2-dev|None), (3rd-boost-dev|1.0.0.1), (3rd-opencv-dev|1.0.0.1)
[buildtool] INFO (3rd-ipopt-dev|1.0.0.1), (3rd-eigen3-dev|1.0.0.1), (libadolc-dev|None), (3rd-ad-rss-lib-dev|1.0.0.1)
[buildtool] INFO (cyber-dev|1.0.0.1), (libncurses5-dev|None), (libuuid1|None), (3rd-rules-python-dev|1.0.0.1)
[buildtool] INFO (3rd-grpc-dev|1.0.0.1), (3rd-rules-proto-dev|1.0.0.1), (3rd-py-dev|1.0.0.1), (3rd-bazel-skylib-dev|1.0.0.1)
[buildtool] INFO (3rd-protobuf-dev|1.0.0.1), (3rd-fastrtps-dev|1.0.0.1), (common-msgs-dev|1.0.0.1), (bazel-extend-tools-dev|1.0.0.1)
[buildtool] INFO (common-dev|1.0.0.1), (libsqlite3-dev|None), (3rd-gtest-dev|1.0.0.1), (3rd-nlohmann-json-dev|1.0.0.1)
[buildtool] INFO (map-dev|1.0.0.2), (3rd-mkl-dev|1.0.0.1), (3rd-libtorch-cpu-dev|1.0.0.1), (3rd-gpus-dev|1.0.0.1)
[buildtool] INFO Done, Enjoy!
[buildtool] INFO apollo build tool exit.
```

##### List the dependency information of the package source code under a specified path:

```shell
buildtool info --directory example_components/
[buildtool] INFO Reconfigure apollo enviroment setup
[buildtool] INFO Compile parameters:
[buildtool] INFO using gpu: False
[buildtool] INFO using debug mode: False
[buildtool] INFO According parameters above to analysis depends of example-components-dev
[buildtool] INFO Analyzing dependencies topological graph...
[buildtool] INFO example-components-dev directly depends on the following packages:
[buildtool] INFO (cyber-dev|1.0.0.1), (bazel-extend-tools-dev|1.0.0.1), (3rd-protobuf-dev|1.0.0.1)
[buildtool] INFO Done, Enjoy!
[buildtool] INFO apollo build tool exit.
```

##### Detailed Parameters

```
usage: buildtool info [-h] [--depends-on] [--with-indirect] [--directory]
[--gpu] [--cpu] [--dbg]
[query]

positional arguments:
[query] the package name or stored path of which package's depends
you want to list

optional arguments:
-h, --help show this help message and exit
--depends-on list those packages information which are directly
dependent on the package
--with-indirect list the package all depends, direct and indirect
--directory list the package infomation which stored in the directory
--gpu with compilation in GPU parameter
--cpu with compilation in CPU parameter
--dbg with compilation in debugging parameter
```

### bootstrap

bootstrap is used to start the latest version of a specific package. The latest version refers to the version corresponding to the last operation on the package. If the last operation on the package was compilation, bootstrap starts the compilation output of the compiled package; if the last operation was installation or reinstallation, for example executing a command such as `buildtool reinstall`, then `bootstrap` starts the binaries or dynamic libraries of the installed or reinstalled package. Note: Currently, the modules that bootstrap can start are `dreamview` and `monitor`.

#### Usage

##### Start dreamview

```shell
buildtool bootstrap start dreamview
```

##### Stop dreamview:

```shell
buildtool bootstrap stop dreamview
```

##### Detailed Parameters

```shell
usage: buildtool bootstrap [-h] {start,stop} ...

positional arguments:
  {start,stop}  options
    start       start module
    stop        stop module

optional arguments:
  -h, --help    show this help message and exit
```

### create

This action is used to create packages. The currently supported package templates are `component` and `plugin`

#### Usage

##### Create a component

```shell
buildtool create --template component \
    --namespaces mycomponent \
    modules/mycomponent/xxx_yyy
```

> The above command generates the component package files in the `modules/mycomponent/xxx_yyy` directory

##### Create a plugin

```shell
buildtool create --template plugin \
    --namespaces planning \
    --dependencies planning:binary:planning \
    --build_dependencies //modules/planning/planning_base/scenario_base:stage //modules/planning/planning_base/common:planning_common \
    --includes modules/planning_base/scenario_base/scenario.h modules/planning/planning_base/common/frame.h \
    --base_class_name apollo::planning::Scenario \
    --class_name MyLaneFollowScenario \
    modules/scenarios/my_lany_follow
```

> The above command generates the plugin files in the `modules/scenarios/my_lany_follow` directory
>
> Where:
>
> `--namespaces planning` specifies that the code namespace is `planning`, and the generated plugin will be under the namespace `apollo::planning`
>
> `--dependencies planning:binary:planning` inserts the dependency `<depend type="binary" repo_name="planning">planning</depend>` into the generated `cyberfile.xml` > `--build_dependencies` specifies the build dependencies to be added in `BUILD`; in the example above, the dependencies are provided by the `planning` package
>
> `--includes` specifies the header files to be added, which are inserted into the header file of the generated plugin; in the example above, this is the `modules/scenarios/my_lane_follow/my_lane_follow.h` file
>
> `--base_class_name apollo::planning::Scenario` specifies that the base class inherited by the plugin is `apollo::planning::Scenario` > `--class_name MyLaneFollowScenario` specifies that the generated plugin class name is `MyLaneFollowScenario`; if not specified, it is generated from the directory name

##### Detailed Parameters

```shell
usage: buildtool create [-h] --template {component,plugin} [--name NAME]
                        [--class_name CLASS_NAME]
                        [--type {src,module,binary,wrapper}] [--author AUTHOR]
                        [--email EMAIL] [--description DESCRIPTION]
                        [--config_message_name CONFIG_MESSAGE_NAME]
                        [--channel_message_type CHANNEL_MESSAGE_TYPE]
                        [--channel_name CHANNEL_NAME]
                        [--namespaces [NAMESPACES [NAMESPACES ...]]]
                        [--includes [INCLUDES [INCLUDES ...]]]
                        [--dependencies [DEPENDENCIES [DEPENDENCIES ...]]]
                        [--build_dependencies [BUILD_DEPENDENCIES [BUILD_DEPENDENCIES ...]]]
                        [--base_class_name BASE_CLASS_NAME]
                        package_path

positional arguments:
  package_path          specify the path of this package needed to create,
                        default to `modules/demo_<template>`

optional arguments:
  -h, --help            show this help message and exit
  --template {component,plugin}
                        specify the template of this package
  --name NAME           specify the name of this package, default to
                        `demo-<template>`
  --class_name CLASS_NAME
                        override default class name
  --type {src,module,binary,wrapper}
                        specify the type of this package
  --author AUTHOR       specify the author of this package
  --email EMAIL         specify the contacted email
  --description DESCRIPTION
                        specify the description of this package
  --config_message_name CONFIG_MESSAGE_NAME
                        specify the proto message name
  --channel_message_type CHANNEL_MESSAGE_TYPE
                        specify the channel message type
  --channel_name CHANNEL_NAME
                        specify the channel name
  --namespaces [NAMESPACES [NAMESPACES ...]]
                        specify the namespace list
  --includes [INCLUDES [INCLUDES ...]]
                        specify the extra include file list
  --dependencies [DEPENDENCIES [DEPENDENCIES ...]]
                        specify the extra dependency list
  --build_dependencies [BUILD_DEPENDENCIES [BUILD_DEPENDENCIES ...]]
                        specify the extra build dependency list
  --base_class_name BASE_CLASS_NAME
                        specify the base class name of plugin
```

### release

This command is used to package compiled modules. It generates a release.tar.gz in the current workspace, which can be used for subsequent deployment

#### Usage

```shell
buildtool release
```

In the above operation, buildtool packages the compilation outputs of all source code in the workspace.

```shell
buildtool release -p modules/planning/planning_base
```

In the above operation, buildtool packages the outputs of the planning source code in the workspace

##### Detailed Parameters

```shell
usage: buildtool release [-h] [-p [* [* ...]]]

optional arguments:
  -h, --help            show this help message and exit
  -p [* [* ...]], --packages [* [* ...]]
                        Specify the package path.
```

### deploy

This command is used to deploy the release.tar.gz generated by release

#### Usage

```shell
buildtool deploy -f release.tar.gz
```

### map

This command is used to view and download public maps

#### Usage

##### View the maps that exist locally

```shell
buildtool map list
```

##### View the maps that exist locally and the public maps on the remote

```shell
buildtool map list -r
```

##### Download a map

```shell
buildtool map get <map_name>
```

> Note: `<map_name>` is the name or index of the map (names and indices can be viewed through `buildtool map list -r`)
