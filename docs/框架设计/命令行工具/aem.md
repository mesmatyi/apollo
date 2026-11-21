# Apollo Development Tool - aem

## Overview

`aem` is a command-line tool (short for `apollo environment manager`) that provides the ability to manage Apollo containers.

## Installation

### 1. Install Dependencies

```shell
sudo apt-get update
sudo apt-get install ca-certificates curl gnupg
```

### 2. Add the Apollo software source gpg key on the host, then set up the source and update

```shell
# Add the gpg key
sudo install -m 0755 -d /etc/apt/keyrings
curl -fsSL https://apollo-pkg-beta.cdn.bcebos.com/neo/beta/key/deb.gpg.key | sudo gpg --dearmor -o /etc/apt/keyrings/apolloauto.gpg
sudo chmod a+r /etc/apt/keyrings/apolloauto.gpg

# Set up the source and update
echo \
    "deb [arch="$(dpkg --print-architecture)" signed-by=/etc/apt/keyrings/apolloauto.gpg] https://apollo-pkg-beta.cdn.bcebos.com/apollo/core"\
    $(. /etc/os-release && echo "$VERSION_CODENAME") "main" | \
    sudo tee /etc/apt/sources.list.d/apolloauto.list
sudo apt-get update
```

> Note: If Apollo 8.0 has been installed before, the `/etc/apt/sources.list` file on the host may contain an entry like
> `deb https://apollo-pkg-beta.cdn.bcebos.com/neo/beta bionic main`, which can be deleted directly. The Apollo source configuration on the host is only used to install
> the `aem` tool.

### 3. Install aem:

```shell
sudo apt install apollo-neo-env-manager-dev --reinstall
```

After a successful installation, you can use

```shell
aem -h
```

Seeing output similar to the following means the installation succeeded

```shell

Usage:
    aem [OPTION]

Options:
    stopall : stop all apollo development container.
    enter : enter into the apollo development container.
    profile : profiles management
    remove : remove the apollo development container.
    install : install source code of specified package to workspace.
    start : start the created container.
    bootstrap : run dreamview and monitor module.
    start_gpu : start a created gpu container.
    build : build package in workspace.
    create : create a new container with apollo development image.
    setup_host : setup host
    list : list all apollo development container.
```

## Quick Start

This section mainly describes the workflow and conventions for using the `aem` tool.

### 1. Clone the Project Directory
Apollo 9.0 no longer initializes the workspace through aem init. It is recommended to clone the GitHub project by following the tutorial.

Apollo currently provides 3 example projects, and you can choose one as needed

- [application-core](https://github.com/ApolloAuto/application-core) , which contains all Apollo open-source packages and can be used as a basis for building your own applications

- [application-pnc](https://github.com/ApolloAuto/application-pnc) , which contains only planning and control related packages, suitable for users who focus only on planning and control

- [application-perception](https://github.com/ApolloAuto/application-perception) , which contains only perception related packages, suitable for users who focus only on perception

Note: If you encounter network problems when cloning the project directory, refer to the [FAQ on pulling the project directory](https://apollo.baidu.com/community/article/1158)

### 2. Start an Apollo Container

Use the following command to automatically pull the image and start the container:
aem start
The above command is suitable for entering the container for the first time. If you have already started an Apollo environment container, this command will try to restart the previously existing Apollo environment container.
If you want to forcibly delete the previous container and start a brand-new one, use the following command:
aem start -f
The following output indicates that the container started successfully:

```shell
[ OK ] Congratulations! You have successfully finished setting up Apollo Dev Environment.
[ OK ] To login into the newly created apollo_neo_dev_apollo container, please run the following command:
[ OK ]   aem enter
[ OK ] Enjoy!
```

### 3. Enter the Container

After the container starts, you can enter it with the following command:
aem enter
After entering the container, you will be in the apollo_workspace directory. This directory is commonly used as Apollo's workspace, and all subsequent operations take place in it.

### 4. Download and Install Source Code

For secondary development of a module, you also need to download the module's source code and dependencies with the following command:
aem install xx (module name)
This downloads the module's source code. After the download completes, the workspace automatically creates some folders, and the source code of xx is located under the module/xx path in the workspace.

### 5. Compile and Install Source Code

After modifying the module source code, the modified module needs to be compiled and installed:
aem build -p modules/xx

### 6. Run and Debug

After compilation and installation succeed, start dreamview for debugging:
aem bootstrap start

Note: The dreamview and monitor modules need to be downloaded for the first start:
aem install dreamview-plus monitor

### 7. Stop the Container

Run aem stopall on the host to stop the Apollo container. A stopped container can be started again with aem enter.

### 8. Delete the Container

Run aem remove on the host to delete the Apollo container. After the container is deleted, the project configuration mounted in the container still remains in the host's workspace directory. After running the aem start command again, all configuration in the current directory is mounted into the new container. If you do not want to mount the current configuration, please delete it manually first.

## Usage

### Subcommands

Like common command-line tools such as git or apt, aem's functionality is organized into subcommands. For example, start starts a container, enter enters a started container, and so on.
A subcommand may require some arguments; you can type -h or -help after the subcommand to see the detailed arguments.

### start, start_gpu

This subcommand starts an Apollo environment container. It checks whether the apollo container has already been started. If the container has been started or stopped, it uses the existing container; if the container has never been started, it starts a new one.

#### Usage

##### Start a container:

```shell
aem start
```

##### Start a container with gpu support, in which you can build or run modules that need gpu support:

```
aem start_gpu
```

> Note: The NVIDIA graphics driver and the nvidia container toolkit must be installed correctly.

##### Start a container using a local image:

```shell
aem start_gpu -l
```

##### Forcibly start a new container:

```shell
aem start_gpu -f
```

> Note: This command deletes the container, but keeps the Apollo modules previously installed in the container. To delete everything completely, use the `remove` command.

##### Specify a name for the started container:

```shell
aem start_gpu -n apollo_container
```

> If no container name is specified, the current username is used as the container name by default

> The start command reads the .env file in the current path, which specifies the container name, the image to use, and so on. For example, if the content of the .env file is:
> APOLLO*ENV_NAME=pnc
> APOLLO_ENV_WORKLOCAL=1
> APOLLO_ENV_CONTAINER_REPO='registry.baidubce.com/apollo/apollo-env-gpu'
> APOLLO_ENV_CONTAINER_TAG='9.0-latest'
> aem uses the registry.baidubce.com/apollo/apollo-env-gpu:9.0-latest image to start a container named apollo_neo_pnc
> When there is no .env file in the current path, the default image (the 8.0 image) is used to start a container named apollo_neo*${USER}

##### Specify the mount point:

```shell
aem start_gpu -m /home/apollo/workspace:/apollo_workspace
```

##### Detailed Parameters

```shell
Usage: aem start [options] ...
OPTIONS:
    -h, --help                    Display this help and exit.
    -f, --force                   force to restart the container.
    -n, --name                    specify container name to start a container.
    -m, --mount                   specify the mount point in container, such as /home/apollo/workspace:/apollo_workspace
    -g, --geo <us|cn|none>        Pull docker image from geolocation specific registry mirror.
    -t, --tag <TAG>               Specify docker image with tag <TAG> to start.
    -c, --cross-platform <arch>   Run a cross-platform image
    -y                            Agree to Apollo License Agreement non-interactively.
    --shm-size <bytes>            Size of /dev/shm . Passed directly to "docker run"
    --gpu                         Use gpu image instead of cpu image.
```

### enter

This subcommand is used to enter the Apollo environment container.

#### Usage

##### Enter the container

```shell
aem enter
```

##### Specify the container name

```shell
aem enter -n apollo_container
```

##### Enter the container as a specified user

```shell
aem enter -n apollo_container --user root
```

> This command is the same as the start command: it also tries to read the .env file in the current path and enters the container with the corresponding name according to the APOLLO*ENV_NAME defined in the file
> If there is no .env file in the current path, aem tries to enter the container named apollo_neo*${USER}

##### Detailed Parameters

```shell
Usage: aem enter [options] ...
OPTIONS:
    -h, --help                    Display this help and exit.
    -n, --name                    specify container name to enter.
    --user                        specify container user to enter
```

### remove

This command is used to delete the environment container

#### Usage

##### Delete the container with a specified name

```shell
# Delete the container named `pnc`
aem remove --name pnc
```

> This command is the same as the start command: it also tries to read the .env file in the current path and deletes the container with the corresponding name according to the APOLLO*ENV_NAME defined in the file
> If there is no .env file in the current path, aem tries to delete the container named apollo_neo*${USER}

##### Detailed Parameters

```shell
Usage: aem remove [options] ...
OPTIONS:
    -h, --help                    Display this help and exit.
    -n, --name                    specify container name to remove.
    -w, --workspace               specify container workspace.
    --worklocal                   env data is store in workspace
```

Note: If developers want to delete the project directory on the host, they must first run aem remove to delete the container before deleting the project. Otherwise, dependencies will be lost.

### bootstrap

This subcommand is used to start `dreamview 1.0` or `dreamview 2.0`

#### Usage

##### Start dreamview 1.0 inside the container

```shell
aem bootstrap start
```

##### Start dreamview 2.0 inside the container

```shell
aem bootstrap start --plus
```

##### Stop the dreamview 1.0 or dreamview 2.0 currently started in the container

```shell
aem bootstrap stop
```

##### Detailed Parameters

```shell
Usage: aem bootstrap [options] [argument] ...
OPTIONS:
    start --plus                    Start dreamview 2.0.
    start                           Start dreamview.
    stop                            Stop dreamview or dreamview 2.0.
```

### list

This subcommand lists all containers started by the aem tool

#### Usage

```shell
aem list
```

The above command produces the following output

```
[INFO] Environment containers:
apollo_neo_dev /home/apollo/sample/.aem
```

This means aem currently has one container started, named `apollo_neo_dev`, and the container's project path is under `/home/apollo/sample`

### profile

This subcommand is used to select the configuration profile to use

#### Usage

The following command lists which configuration profile sets are available in the project

```shell
aem profile list
```

The above command produces the following output

```
sample
```

This means the configuration profile named sample can currently be used

The following command uses the configuration profile set named sample

```shell
aem profile use sample
```

### build

This subcommand is a simple wrapper around the buildtool build action. For detailed usage, refer to the [buildtool documentation](../框架设计/命令行工具/buildtool.md).

### install

This subcommand is a simple wrapper around the buildtool install action. For detailed usage, refer to the [buildtool documentation](../框架设计/命令行工具/buildtool.md).

### Environment Variables

aem uses some environment variables when executing subcommands. Some features that cannot be controlled through parameters can also be achieved by modifying these environment variables. When aem runs, if there is a `.env` file in the current directory, it loads that file.

| Variable                    | Description                                                                  |
| --------------------------- |------------------------------------------------------------------------------|
| `APOLLO_ENVS_ROOT`          | Location where container-related directories are created, `$HOME/.aem/envs` by default |
| `APOLLO_ENV_NAME`           | Container name, the current user's username by default                       |
| `APOLLO_ENV_WORKROOT`       | Path of the working directory inside the container, `/apollo_workspace` by default |
| `APOLLO_ENV_WORKLOCAL`      | Whether the container-related directories are stored under the current directory: `1` for yes, `0` for no; if no, they are created under `$APOLLO_ENVS_ROOT` |
| `APOLLO_ENV_CONTAINER_REPO` | The image repo used to create the container, `registry.baidubce.com/apollo/apollo-env-gpu` by default; a custom image can be used by changing this |
| `APOLLO_ENV_CONTAINER_TAG`  | The image tag used to create the container, `latest` by default              |
