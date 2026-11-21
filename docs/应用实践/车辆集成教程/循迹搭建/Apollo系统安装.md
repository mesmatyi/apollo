<!---Audience of this guide: advanced developers who develop for on-vehicle testing. This document does not involve modifying or interpreting source code, and focuses on usage only.-->

<!---This document is a template-->

<!---Using the template: writing guidance is given next to each paragraph; read the guidance before writing.-->

<!---Leave one blank line between headings and paragraphs, and between paragraphs.-->

<!--The file name of the md file is the default level-1 heading-->

<!--Before the whole document begins, it is recommended to give a picture describing the overall framework of what follows, for example where this module sits in the overall system-->

<!---The level-1 heading is the document name; within the document, level-2 headings are the highest-level headings.-->

<!---You can also write application scenarios, prerequisites, precautions, and the logical relationships among multiple subtasks.-->

<!---All headings should use a verb-object structure, for example: Create a simulation scenario, Download the branch repository.-->

<!---When a level-2 task has subtasks, continue with level-3 heading tasks. Use level-4 headings as little as possible. If there is too much content, consider starting another document-->

<!---All pictures must conform to the screenshot conventions; one screenshot may cover multiple steps.-->

<!---If you do not know how to use links, see the Markdown cheat sheet; links include: in-page links, inter-page links, links to other products, and image links-->

<!---The last step must give a viewable result, so that the final step can be accepted intuitively.-->

<!--Title: Prepare a chassis that conforms to the Apollo protocol -->

<!--If the content exceeds 10 pages, it is recommended to split it into several files-->

<!--# Apollo Software Integration Tutorial-->

This document takes the Lincoln MKZ as the target vehicle and describes the integrated installation of the Apollo software system. You will learn how to complete the integrated installation and configuration of the Apollo autonomous driving software system on the vehicle computing unit Hardware 3.0 (HW3.0). This document applies to Apollo 6.0 and above.

## Prerequisites

Before installing the software on the MKZ, you need to complete the vehicle hardware integration by following [Vehicle Integration](../应用实践/车辆集成教程/循迹搭建/车辆集成.md).

## Software Integration and Installation

Follow the steps below to integrate and install the software.

### 1. Install the Ubuntu System

Refer to the steps in the installation guide to install the Ubuntu system.

### 2. Install the Real-Time Kernel

You can build a real-time kernel by following [ROS2: Building Real-Time Linux](https://docs.ros.org/en/foxy/Tutorials/Building-Realtime-rt_preempt-kernel-for-ROS-2.html), or directly install the prebuilt version we provide:

```bash
wget https://apollo-system.cdn.bcebos.com/release_installer/linux-kernel-5.4.115-apollo-rt.zip
unzip linux-kernel-5.4.115-apollo-rt.zip
cd linux-kernel-5.4.115-apollo-rt
sudo dpkg -i *.deb
```

### 3. Install the CANBUS Driver

1. Download the source code under [apollo-contrib](https://github.com/ApolloAuto/apollo-contrib/tree/master/baidu) and compile it locally; after compiling, the basa.ko module file is generated.

2. Configure the system to load the driver automatically at power-on:

```bash
sudo su
mkdir -p /lib/modules/$(uname -r)/kernel/drivers/baidu/basa/
cp basa.ko /lib/modules/$(uname -r)/kernel/drivers/baidu/basa/
echo "basa" >>/etc/modules
depmod -a
modprobe basa
```

### 4. Install the Nvidia Driver under the Real-Time Kernel

First refer to the method in the installation guide steps to complete the preliminary installation of the Nvidia driver.

Note: Because the Nvidia driver does not support real-time kernels, running `sudo apt-get install nvidia-driver-455` above reports the following error message:

```
The kernel you are installing for is a PREEMPT_RT kernel!

The NVIDIA driver does not support real-time kernels. If you
are using a stock distribution kernel, please install
a variant of this kernel that does not have the PREEMPT_RT
patch set applied; if this is a custom kernel, please
install a standard Linux kernel.  Then try installing the
NVIDIA kernel module again.

*** Failed PREEMPT_RT sanity check. Bailing out! ***
```

You can get around this by setting `IGNORE_PREEMPT_RT_PRESENCE=1` when compiling the Nvidia driver. The steps are as follows:

1. Run the following commands to compile the Nvidia driver:

```bash
# Switch to the Nvidia driver source directory
cd "$(dpkg -L nvidia-kernel-source-455 | grep -m 1 "nvidia-drm" | xargs dirname)"

# Set IGNORE_PREEMPT_RT_PRESENCE=1 to compile the Nvidia driver
sudo env NV_VERBOSE=1 \
    make -j8 NV_EXCLUDE_BUILD_MODULES='' \
    KERNEL_UNAME=$(uname -r) \
    IGNORE_XEN_PRESENCE=1 \
    IGNORE_CC_MISMATCH=1 \
    IGNORE_PREEMPT_RT_PRESENCE=1 \
    SYSSRC=/lib/modules/$(uname -r)/build \
    LD=/usr/bin/ld.bfd \
    modules

sudo mv *.ko /lib/modules/$(uname -r)/updates/dkms/
sudo depmod -a
```

2. Restart the system.

3. Run `nvidia-smi` to check whether the `Nvidia` driver works normally.

### 5. Install Other Required Software

First refer to the method in the installation guide to install Docker Engine and the Nvidia Container Toolkit.

### 6. Install the Apollo System

Install by following the installation guide.

### 7. System Configuration

Before running the Apollo software, it is recommended to modify some system configurations first.

- Disable automatic software updates
  
  Ubuntu does not turn off the automatic software update option by default, and automatic software updates sometimes bring unpredictable consequences. For example, an automatic kernel upgrade can invalidate some installed drivers. You can disable automatic software updates by modifying configuration files.

Modify `/etc/apt/apt.conf.d/10periodic` to the following content:

```
APT::Periodic::Update-Package-Lists "0";
APT::Periodic::Download-Upgradeable-Packages "0";
APT::Periodic::AutocleanInterval "0";
APT::Periodic::Unattended-Upgrade "0";
```

Modify `/etc/apt/apt.conf.d/20auto-upgrades` to the following content:

```
APT::Periodic::Update-Package-Lists "0";
APT::Periodic::Download-Upgradeable-Packages "0";
APT::Periodic::AutocleanInterval "0";
APT::Periodic::Unattended-Upgrade "0";
```

- Set a fixed IP

Frequent IP changes are not conducive to debugging.

For Hardware 3.0, you can modify `/etc/netplan/01-network-manager-all.yaml` to the following content:

```
# Let NetworkManager manage all devices on this system
network:
 version: 2
 renderer: NetworkManager
 ethernets:
   eth0:
     addresses: [192.168.10.6/24]
     gateway4: 192.168.10.1
     nameservers:
       addresses: [192.168.10.1]
   eth1:
     addresses: [192.168.20.8/24, 192.168.1.6/24, 192.168.19.250/24]
     routes:
     - to: 224.100.100.0/24
       via: 192.168.20.1
```
