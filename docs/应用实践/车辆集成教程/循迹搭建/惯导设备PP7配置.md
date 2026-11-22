This document describes the configuration of the Novatel strapdown inertial navigation device PP7. With this document, you will learn how to configure and use the strapdown inertial navigation device PP7 so that it works properly in Apollo. The strapdown inertial navigation device PP7 is hereafter referred to as the navigation device PP7 or PP7.

## Prerequisites

To understand the content of this document faster and better, you need to do the following preparation in advance:

- Completed [Record-and-Replay Setup -- Vehicle Integration](../应用实践/车辆集成教程/循迹搭建/车辆集成.md).

- Measure the lever arm values of the two PP7 antennas

## Apollo Localization Module Configuration: PP7 Configuration

There are many ways to configure the PP7. You can use a command-line serial port tool such as minicom, or a graphical serial port tool such as cutecom, or of course the NovAtel Application Suite graphical tool provided on the NovAtel official website. The configuration tool introduced in this document is the NovAtel Application Suite provided on the NovAtel official website. NovAtel Application Suite comes in a Windows version and a Linux version. This document describes configuring the PP7 with the Linux version of NovAtel Application Suite, and uses the command-line tool integrated in NovAtel Application Suite to configure the PP7. Its graphical configuration method is not covered in this document; interested developers can explore it on their own. In addition, configuring the PP7 with the command-line tool integrated in NovAtel Application Suite is also a useful reference for developers who configure the PP7 with tools such as minicom and cutecom.

### 1. Install NovAtel Application Suite

Download the Linux version of the NovAtel Application Suite tool from the NovAtel official website download page (click ZIP next to it to download). The download is an archive; NovAtel Application Suite needs no installation and can be used directly after extraction.

Download address: https://novatel.com/support/support-materials/software-downloads .

![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Doc_CN_6_0/image_ed74e28.png)

### 2. Connect the PP7 Device to the Linux Host

- Use a USB cable to connect the `COM PORTS` interface of the PP7 to a `USB` port of the Linux host.

![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Doc_CN_6_0/image_99263b8.png)

- In the command-line terminal of the Linux host, enter and run the command `ls -al /dev/ttyUSB*` to view the PP7 device numbers.

![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Doc_CN_6_0/image_5dff4e5.png)

After the command runs, 3 USB devices are displayed: `/dev/ttyUSB0, /dev/ttyUSB1, /dev/ttyUSB2`. The device numbers displayed here may differ from what each developer sees; they depend on the number of USB devices connected to the Linux host. `/dev/ttyUSB0, /dev/ttyUSB1, /dev/ttyUSB2` is used as the example here.

- In the command-line terminal, enter and run the command `sudo chmod 777 /dev/ttyUSB*` to give all ttyUSB devices read, write and execute permissions.

![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Doc_CN_6_0/image_b21d8da.png)

### 3. Add the PP7 Device

- Enter the folder where NovAtel Application Suite was extracted and double-click the `NovAtelApplicationSuite_64bit.AppImage` application to start the NovAtel Application Suite software.

- On the interface that appears after startup, click the "Add Device" button. On the page that pops up, enter `PP7` in the connection Name box, select `USB` for Type, select `/dev/ttyUSB0` for Port, and then click the "`Add`" button to add the PP7 device.

![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Doc_CN_6_0/image_03aab1a.png)

![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Doc_CN_6_0/image_72fd04c.png)

![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Doc_CN_6_0/image_6aec44b.png)

Note: If `Error: Permission denied, cannot open /dev/ttyUSB0` appears, the addition failed. Run the following command in the command-line terminal: `sudo chmod 777 /dev/ttyUSB* ` to give all ttyUSB devices read, write and execute permissions.

![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Doc_CN_6_0/image_cdea464.png)

### 4. Configure the PP7 Device

- Click the connected PP7 device to open the PP7 device interface.

![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Doc_CN_6_0/image_6b669b5.png)

![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Doc_CN_6_0/image_1060fca.png)

- Click Tools on the page and select the terminal option from the drop-down menu to open the command terminal page.

![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Doc_CN_6_0/image_8200ba3.png)

![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Doc_CN_6_0/image_3523e07.png)

- In the command terminal, enter and run the command `freset ` to restore the default settings. After the command runs, the PP7 disconnects from NovAtel Application Suite; please reconnect before continuing with the following. The command terminal input box is the box marked with gray `Enter your command`; press Enter to run the entered command.

- In the command-line terminal, enter and run the following command: `log version`

a. In the terminal output, check whether the `HWCONFIG` configuration is `"EthPrioElev"`, which indicates the network-port priority mode. If not, configure this version with the following command: `CONFIGCODE ADD X3PD3C,7PKM76,D48WJD,GJ3HBM,9KFTM4,HWCT_70_0001`

![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Doc_CN_6_0/image_9f3f53b.png)

b. Check whether `PACKAGE` is `"EP7PR0800SN0005"`, which is the PP7 firmware version information in use. If not, contact the supplier to obtain the corresponding version and upgrade to it.

![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Doc_CN_6_0/image_bafca8c.png)

- On the command terminal page, run the following commands in order to configure the PP7 device:

```
SETALIGNMENTVEL 400.0
SETINSTRANSLATION ANT1 0.550 0.875 1.054 0.01 0.01 0.01
SETINSTRANSLATION ANT2 -0.550 0.875 1.054 0.01 0.01 0.01
SETINSROTATION RBV 0 0 0 1 1 1
SETINSPROFILE LAND_PLUS
ALIGNMENTMODE KINEMATIC
INSCOMMAND ENABLE
SERIALCONFIG COM1 9600 N 8 1 N OFF
SERIALCONFIG COM2 9600 N 8 1 N OFF
LOG COM2 GPRMC ONTIME 1 0.25
ETHCONFIG ETHA AUTO AUTO AUTO AUTO
ICOMCONFIG ICOM1 TCP :2000
ICOMCONFIG ICOM2 TCP :3000
IPCONFIG ETHA STATIC 192.168.10.4 255.255.255.0 192.168.10.1
IPSERVICE FTP_SERVER ENABLE
PPSCONTROL ENABLE POSITIVE 1.0 10000
EVENTINCONTROL MARK1 DISABLE
SETINSUPDATE ZUPT ENABLE
WIFIMODE OFF
dnsconfig 1 180.76.76.76
NTRIPCONFIG ncom1 disabled
rtksource auto any
psrdiffsource auto any
INTERFACEMODE ICOM2 RTCMV3 NOVATEL ON
SAVECONFIG
```

Note: Please run the above commands one by one, and after each command, confirm that the command-line terminal returns OK, which indicates that the command was configured successfully.

![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Doc_CN_6_0/image_0c1b838.png)

- After all commands have been configured, disconnect the PP7 power to save the configuration changes.

Brief description of the commands:

```
SETINSTRANSLATION ANT1 0.550 0.875 1.054 0.01 0.01 0.01
SETINSTRANSLATION ANT2 -0.550 0.875 1.054 0.01 0.01 0.01
```

The two commands above configure the PP7 lever arm values. Where:

- The 3 numbers after ANT1 are the lever arm values of the primary antenna;

- The 3 numbers after ANT2 are the lever arm values of the secondary antenna;

- The 3 values after the lever arm values are the accuracy errors of the 3 coordinate values.

The lever arm value is the coordinates of the corresponding antenna in the PP7 coordinate system, in meters. The xy plane of the PP7 coordinate system is printed on the top surface of the PP7, and the z axis is perpendicular to the xy plane, passing through the origin and pointing up. When configuring, change them according to the actual situation.

The PP7 is configured to use the single Ethernet cable data transmission mode. After the PP7 is configured, the USB cable can be removed from the Linux host. The IP address configured for the PP7 is 192.168.10.4, set by the configuration commands above. The network configuration of HW3.0 is mentioned here in passing.
HW3.0 network configuration:

- `Ethernet 1: manual mode, IP address 192.168.10.6, subnet mask 255.255.255.0, gateway 192.168.10.1, DNS 192.168.10.1`

- `Ethernet 2: manual mode, IP address 192.168.20.8, subnet mask 255.255.255.0, gateway empty, DNS empty`
