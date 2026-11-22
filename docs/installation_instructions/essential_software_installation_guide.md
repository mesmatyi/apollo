# Essential Software Installation Guide

## Step 1: Install basic software

### 1. Install Ubuntu Linux

For the steps to install Ubuntu 18.04+, see the [official installation guide](https://ubuntu.com/tutorials/install-ubuntu-desktop).

> Note: Although other Linux distributions may also work, we have only tested Apollo on
> [Ubuntu 18.04.5 LTS (Bionic Beaver)](https://releases.ubuntu.com/18.04.5/). Therefore, we recommend using Ubuntu
> 18.04.5 as the host operating system.

After the installation is complete, update the relevant software:

```shell
sudo apt-get update
sudo apt-get upgrade
```

> Note: To complete the update, a network connection is required.

### 2. Install Docker Engine

Apollo depends on Docker 19.03+. To install Docker Engine, you can follow the official documentation:

- See [Install Docker Engine on Ubuntu](https://docs.docker.com/engine/install/ubuntu/).

- You can also install it directly with the installation script provided by Apollo:

```shell
wget http://apollo-pkg-beta.bj.bcebos.com/docker_install.sh
bash docker_install.sh
```

This process may run the script multiple times; just follow the script prompts.

## Step 2: Install the Apollo Environment Manager

The Apollo Environment Manager is a command-line tool that helps manage and start Apollo environment containers.

### 1. Install dependencies

```shell
sudo apt-get update
sudo apt-get install ca-certificates curl gnupg
```

### 2. Add the Apollo software source gpg key on the host machine, and set up the source and update

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

> Note: If apollo version 8.0 has been installed before, the `/etc/apt/sources.list` file on the host machine will contain a configuration like
> `deb https://apollo-pkg-beta.cdn.bcebos.com/neo/beta bionic main`, which can be deleted directly. The apollo source configuration on the host machine is used only to install
> the `aem` tool

### 3. Install aem:

```shell
sudo apt install apollo-neo-env-manager-dev --reinstall
```

After the installation succeeds, you can use it

```shell
aem -h
```

## Step 3 (optional): Get GPU support

Some Apollo modules need GPU support to be built and run (for example the perception module). If you need to use such modules, you need to install the Nvidia graphics driver and the Nvidia
container toolkit to get GPU support.

> Note: If you have already installed the Nvidia graphics driver, that is, entering `nvidia-smi` in the terminal produces normal output, you can skip the `1. Install the graphics driver` section

### 1. Install the graphics driver

For 10, 20 and 30 series graphics cards, driver version 470.63.01 is recommended. You can [download the driver](https://www.nvidia.cn/Download/driverResults.aspx/179605/cn/) from the Nvidia website

After downloading, find the corresponding folder, open a terminal and enter the installation command:

```shell
sudo chmod 777 NVIDIA-Linux-x86_64-470.63.01.run
sudo ./NVIDIA-Linux-x86_64-470.63.01.run
```

After the installation is complete, you can use the nvidia-smi command to check whether the driver was installed successfully. If everything is normal, you will see output similar to the following:

```shell
Tue Jan  3 12:04:21 2023
+-----------------------------------------------------------------------------+
| NVIDIA-SMI 460.91.03    Driver Version: 470.63.01    CUDA Version: 11.4     |
|-------------------------------+----------------------+----------------------+
| GPU  Name        Persistence-M| Bus-Id        Disp.A | Volatile Uncorr. ECC |
| Fan  Temp  Perf  Pwr:Usage/Cap|         Memory-Usage | GPU-Util  Compute M. |
|                               |                      |               MIG M. |
|===============================+======================+======================|
|   0  GeForce GTX 1080    Off  | 00000000:01:00.0 Off |                  N/A |
|  0%   38C    P8     7W / 198W |    239MiB /  8118MiB |      0%      Default |
|                               |                      |                  N/A |
+-------------------------------+----------------------+----------------------+

+-----------------------------------------------------------------------------+
| Processes:                                                                  |
|  GPU   GI   CI        PID   Type   Process name                  GPU Memory |
|        ID   ID                                                   Usage      |
|=============================================================================|
|    0   N/A  N/A      2566      G   /usr/lib/xorg/Xorg                 18MiB |
|    0   N/A  N/A      2657      G   /usr/bin/gnome-shell               67MiB |
|    0   N/A  N/A      6104      G   /usr/lib/xorg/Xorg                132MiB |
|    0   N/A  N/A      6234      G   /usr/bin/gnome-shell               13MiB |
|    0   N/A  N/A      7440      G   gnome-control-center                1MiB |
+-----------------------------------------------------------------------------+
```

### 2. Install the Nvidia container toolkit

To get GPU support inside containers, you need to install the NVIDIA Container Toolkit after installing docker. Run the following commands to install the NVIDIA Container
Toolkit:

```
distribution=$(. /etc/os-release;echo $ID$VERSION_ID)
curl -s -L https://nvidia.github.io/nvidia-docker/gpgkey | sudo apt-key add -
curl -s -L https://nvidia.github.io/nvidia-docker/$distribution/nvidia-docker.list | sudo tee /etc/apt/sources.list.d/nvidia-docker.list
sudo apt-get -y update
sudo apt-get install -y nvidia-docker2
```

After the installation is complete, you need to manually restart docker:

```shell
sudo systemctl restart docker
```

At this point, the basic software and the Apollo Environment Manager have been installed. Next, follow the [Quick Start](../installation_instructions/quick_start_cn.md) document to install different modules as needed for different usage scenarios.

