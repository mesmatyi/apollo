# Apollo Development Tools - aem

## Overview

`aem` is a command-line tool (full name `apollo environment manager`) that provides the ability to manage Apollo containers.

## Installation

### 1. Install Dependencies

```shell
sudo apt-get update
sudo apt-get install ca-certificates curl gnupg
```

### 2. Add the Apollo Package Source's gpg Key on the Host, and Set Up the Source and Update

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

> Note: If you have previously installed Apollo 8.0, the `/etc/apt/sources.list` file on the host will contain a line like
> `deb https://apollo-pkg-beta.cdn.bcebos.com/neo/beta bionic main`, which can be deleted directly. The Apollo source configuration on the host is only used to install
> the `aem` tool.

### 3. Install aem:

```shell
sudo apt install apollo-neo-env-manager-dev --reinstall
```

After a successful installation, you can use it

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
    init : init single workspace.
    build : build package in workspace.
    create : create a new container with apollo development image.
    setup_host : setup host
    list : list all apollo development container.
```

## Quick Start

This section mainly introduces the usage flow and conventions of the `aem` tool.

### 1. Create a Mount Folder
Apollo needs to be built and run inside the container. To develop and manage code on the host, a mount folder needs to be created in advance, before entering the container.
mkdir workspace &&  cd workspace (there is no special requirement for the folder name; set it according to your preference)

### 2. Start an Apollo Container

Use the following command to automatically pull the image and start the container:
aem start
The command above is suitable for the first time you enter the container. If you have already started an Apollo environment container, this command tries to restart the previously existing Apollo environment container.
If you want to forcibly delete the previous container and start a brand-new one, use the following command:
aem start -f
The program outputs the following, indicating the container started successfully:
```shell
[ OK ] Congratulations! You have successfully finished setting up Apollo Dev Environment.
[ OK ] To login into the newly created apollo_neo_dev_apollo container, please run the following command:
[ OK ]   aem enter
[ OK ] Enjoy!
```

### 3. Enter the Container

After the container starts, you can enter it with the following command:
aem enter
After entering the container, you will be in the apollo_workspace directory, which is commonly used as Apollo's workspace; subsequent operations are all located in this directory.

### 4. Initialize the Workspace
Run aem init inside the container to initialize the current directory as Apollo's workspace. After initialization, a WORKSPACE file will appear in the current directory.

### 5. Download and Install Source Code
After initializing the workspace, only an empty workspace is generated, with no source code in it. Therefore, to do secondary development on a module, you also need to download the module's source code and dependencies with the following command:
aem install xx (module name)
Download the module source code. After the download completes, the workspace automatically creates some folders, among which the xx source code is located under the module/xx path of the workspace.

### 6. Build and Install Source Code
After modifying the module source code, the modified module needs to be built and installed.
aem build -p modules/xx

### 7. Run and Debug
After the build and installation succeed, start dreamview for debugging.
aem bootstrap start

Note: The dreamview and monitor modules need to be downloaded on first startup.
aem install dreamview-plus monitor

### 8. Stop the Container
Run aem stopall on the host to stop the Apollo container. A stopped container can be started again through aem enter.

### 9. Delete the Container
Run aem remove on the host to delete the Apollo container. After the container is deleted, the project configuration mounted in the container still exists in the host's workspace directory. After running the aem start command again, all configuration in the current directory is remounted into the new container. If you do not need to mount the current configuration, delete it manually first.

## Usage

### Subcommands

Similar to common command-line tools such as git or apt, aem's functions are organized into subcommands; for example, start is responsible for starting a container, enter is responsible for entering a started container, and so on.
Subcommands may require some parameters; you can enter -h or -help after the subcommand to view the detailed parameters.

### start, start_gpu

This subcommand starts an Apollo environment container. The command checks whether the apollo container has already been started. If the container has been started or stopped, it uses the already existing container; if the container has never been started, it starts a new container.

#### Usage

##### Start a container:

```shell
aem start
```

##### Start a container with gpu support, in which you can build or run modules that require gpu support:

```
aem start_gpu
```

> Note: The NVIDIA graphics driver and the nvidia container toolkit must be installed correctly.

##### Start a container with a local image:

```shell
aem start_gpu -l
```

##### Forcibly start a new container:

```shell
aem start_gpu -f
```

> Note: This command deletes the container but keeps the Apollo modules previously installed in the container. To delete completely, use the `remove` command.

##### Specify a name for the started container:

```shell
aem start_gpu -n apollo_container
```

> If no container name is specified, the current username is used as the container name by default.

> The start command reads the .env file in the current path, which specifies the container name, the image to use, and so on. For example, if the content of the .env file is:
>   APOLLO_ENV_NAME=pnc
>   APOLLO_ENV_WORKLOCAL=1
>   APOLLO_ENV_CONTAINER_REPO='registry.baidubce.com/apollo/apollo-env-gpu'
>   APOLLO_ENV_CONTAINER_TAG='9.0-latest'
> aem will use the registry.baidubce.com/apollo/apollo-env-gpu:9.0-latest image to start a container named apollo_neo_pnc.
> When there is no .env file in the current path, the default image (the 8.0 image) is used to start a container named apollo_neo_${USER}.

##### Specify a mount point:

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

> Like the start command, this command tries to read the .env file in the current path and enters the container with the name defined by APOLLO_ENV_NAME in the file.
> If there is no .env file in the current path, aem tries to enter the container named apollo_neo_${USER}.

##### Detailed Parameters

```shell
Usage: aem enter [options] ...
OPTIONS:
    -h, --help                    Display this help and exit.
    -n, --name                    specify container name to enter.
    --user                        specify container user to enter
```

### remove

This command is used to delete an environment container.

#### Usage

##### Delete the container with a specified name

```shell
# Delete the container named `pnc`
aem remove --name pnc
```

> Like the start command, this command tries to read the .env file in the current path and deletes the container with the name defined by APOLLO_ENV_NAME in the file.
> If there is no .env file in the current path, aem tries to delete the container named apollo_neo_${USER}.

##### Detailed Parameters

```shell
Usage: aem remove [options] ...
OPTIONS:
    -h, --help                    Display this help and exit.
    -n, --name                    specify container name to remove.
    -w, --workspace               specify container workspace.
    --worklocal                   env data is store in workspace
```
Note: If developers want to delete the project directory on the host, they must first run aem remove to delete the container before deleting the project; otherwise, dependencies will be lost.

### bootstrap

This subcommand is used to start `dreamview 1.0` or `dreamview 2.0`.

#### Usage

##### Start dreamview 1.0 inside the container

```shell
aem bootstrap start
```

##### Start dreamview 2.0 inside the container

```shell
aem bootstrap start --plus
```

##### Stop dreamview 1.0 or dreamview 2.0 that has been started in the current container

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

This subcommand lists all containers started by the aem tool.

#### Usage

```shell
aem list
```

The command above produces the following output:

```
[INFO] Environment containers:
apollo_neo_dev /home/apollo/sample/.aem
```

This indicates that aem has currently started a container named `apollo_neo_dev`, and the container's project path is under `/home/apollo/sample`.

### profile

This subcommand is used to select the configuration file to use.

#### Usage

The following command lists the configuration file sets available under the project:

```shell
aem profile list
```

The command above produces the following output:

```
sample
```

This indicates that the configuration file named sample can currently be used.

The following command uses the configuration file set named sample:

```shell
aem profile use sample
```

### build

This subcommand is a simple wrapper of the buildtool build action. For detailed usage, see the [buildtool documentation](../user_guidelines/tool_buildtool_cn.md).

### install

This subcommand is a simple wrapper of the buildtool install action. For detailed usage, see the [buildtool documentation](../user_guidelines/tool_buildtool_cn.md).

### init

This subcommand is a simple wrapper of the buildtool init action. For detailed usage, see the [buildtool documentation](../user_guidelines/tool_buildtool_cn.md).

### Environment Variables

aem uses some environment variables when executing subcommands. Some functions that cannot be controlled by parameters can also be achieved by modifying these environment variables. When aem runs, if there is a `.env` file in the current directory, that file is loaded.

| Variable                        | Description                                                                                                        |
| --------------------------- | ----------------------------------------------------------------------------------------------------------- |
| `APOLLO_ENVS_ROOT`          | Location where container-related directories are created, default `$HOME/.aem/envs`                                                              |
| `APOLLO_ENV_NAME`           | Container name, default is the current user's username                                                                              |
| `APOLLO_ENV_WORKROOT`       | Working directory path inside the container, default `/apollo_workspace`                                                            |
| `APOLLO_ENV_WORKLOCAL`      | Whether container-related directories are stored in the current directory; `1` for yes, `0` for no; if no, they are created under `$APOLLO_ENVS_ROOT`                |
| `APOLLO_ENV_CONTAINER_REPO` | Image repo used to create the container, default `registry.baidubce.com/apollo/apollo-env-gpu`; a custom image can be specified by modifying this |
| `APOLLO_ENV_CONTAINER_TAG`  | Image tag used to create the container, default `latest`                                                                    |
