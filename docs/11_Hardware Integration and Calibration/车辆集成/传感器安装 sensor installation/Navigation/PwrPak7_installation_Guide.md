## NovAtel PwrPak7 Installation Guide

The NovAtel PwrPak7 is an integrated INS receiver that contains a GNSS receiver and an IMU.

![novatel_pp7](images/pp7_pic.jpg)

### Installing the GPS receiver

An overview of the Novatel PwrPak7 is shown below.

![pp7_poweron](images/pp7_power.jpg)

To power the receiver, connect +VIN_A and +VIN_B on the power extension cable to the positive terminal of the power supply (that is, 12 VDC), and connect -VIN to the negative terminal. For details, see the following link about the PwrPak7 power cable.

* [NovAtel ProPak7 Power Cable](https://docs.novatel.com/OEM7/Content/Technical_Specs_Receiver/PwrPak7_Power_Cable.htm)

The PwrPak7 receiver supports dual antennas. ANT1 is the upper antenna connector and serves as the primary antenna (required by the Apollo system). ANT2 is the lower antenna connector and serves as the auxiliary antenna (optional).

Similar to the Novatel ProPak6, to send RTK corrections and log data, we can use a USB cable to connect the MicroUSB port on the PwrPak7 to the IPC on the vehicle.

The 26-pin connector on the PwrPak7 can be connected to the LiDAR system through an extension cable.
Connecting to the LiDAR system requires the COM2 port and the PPS signal. For details about the extension cable, see the website below.

* [NovAtel ProPak7 All I/O Cable](https://docs.novatel.com/OEM7/Content/Technical_Specs_Receiver/PwrPak7_All_IO_Cable.htm?tocpath=Specifications%7CPwrPak7%20Technical%20Specifications%7C_____11)

You can place the PwrPak7 in most locations on the vehicle, but we recommend that you follow these suggestions:

- Place and secure the PwrPak7 in the trunk, with the Y axis pointing straight ahead.

- Use the magnetic adapter to firmly attach the antenna to the trunk lid.

- Open the trunk lid and place the antenna cable in the free area between the trunk lid and the vehicle body.

### Taking lever arm measurements

Measure the lever arm values as follows:

* Turn on the IPC before taking measurements.
* After the PwrPak7 and the GPS antenna are in place, the distance from the PwrPak7 to the GPS antenna must be measured. The center of the PwrPak7 IMU and the center of the antenna are marked on the outside of the device.
* The distance should be measured as: X offset, Y offset and Z offset. The axes should be determined by the IMU. The offset error must be within 1 cm to achieve high-precision localization.

### Configuring the PwrPak7

Configure the GPS and IMU as shown below. This can be done by typing the commands or by loading a batch configuration file in NovAtel Connect.

For the PwrPak7:

```
WIFICONFIG OFF
UNLOGALL THISPORT
INSCOMMAND ENABLE
SETIMUORIENTATION 5
ALIGNMENTMODE AUTOMATIC
VEHICLEBODYROTATION 0 0 0
SERIALCONFIG COM1 9600 N 8 1 N OFF
SERIALCONFIG COM2 9600 N 8 1 N OFF
INTERFACEMODE COM1 NOVATEL NOVATEL ON
INTERFACEMODE COM2 NOVATEL NOVATEL ON
INTERFACEMODE USB2 RTCMV3 NONE OFF
PPSCONTROL ENABLE POSITIVE 1.0 10000
MARKCONTROL MARK1 ENABLE POSITIVE
EVENTINCONTROL MARK1 ENABLE POSITIVE 0 2
RTKSOURCE AUTO ANY
PSRDIFFSOURCE AUTO ANY
SETINSTRANSLATION ANT1 0.00 1.10866 1.14165 0.05 0.05 0.08
SETINSTRANSLATION ANT2 0.00 1.10866 1.14165 0.05 0.05 0.08
SETINSTRANSLATION USER 0.00 0.00 0.00
EVENTOUTCONTROL MARK2 ENABLE POSITIVE 999999990 10
EVENTOUTCONTROL MARK1 ENABLE POSITIVE 500000000 500000000

LOG COM2 GPRMC ONTIME 1.0 0.25
LOG USB1 GPGGA ONTIME 1.0
LOG USB1 BESTGNSSPOSB ONTIME 1
LOG USB1 BESTGNSSVELB ONTIME 1
LOG USB1 BESTPOSB ONTIME 1
LOG USB1 INSPVAXB ONTIME 1
LOG USB1 INSPVASB ONTIME 0.01
LOG USB1 CORRIMUDATASB ONTIME 0.01
LOG USB1 RAWIMUSXB ONNEW 0 0
LOG USB1 MARK1PVAB ONNEW

LOG USB1 RANGEB ONTIME 1
LOG USB1 BDSEPHEMERISB ONTIME 15
LOG USB1 GPSEPHEMB ONTIME 15
LOG USB1 GLOEPHEMERISB ONTIME 15

LOG USB1 INSCONFIGB ONCE
LOG USB1 VEHICLEBODYROTATIONB ONCHANGED

SAVECONFIG
```

**![warning_icon](images/warning_icon.png) WARNING:** Modify the content of the **<u>SETINSTRANSLATION</u>** line according to the actual measured values. ANT1 is used for the required primary antenna, while ANT2 is used for the auxiliary antenna, which is optional.

For example:

```
SETINSTRANSLATION ANT1 -0.05 0.5 0.8 0.05 0.05 0.08
```

The first three numbers represent the measured lever arm distances. The last three numbers are the measurement uncertainties.

### References

For more information about the NovAtel PwrPak7, see:
* [NovAtel PwrPak7 Installation & Operation User Manual](https://docs.novatel.com/OEM7/Content/PDFs/PwrPak7_Install_Ops_Manual.pdf)
* [NovAtel PwrPak7 Installation Overview](https://docs.novatel.com/OEM7/Content/PwrPak_Install/PwrPak7_Install_Overview.htm?TocPath=Installation%7CPwrPak7%20Installation%7C_____4)

## Disclaimer

This device is `Apollo Platform Supported`
