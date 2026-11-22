Welcome to the Apollo sensor calibration service. This document describes the procedure for using the extrinsic calibration service between the 64-beam LiDAR and the GNSS/INS.

### Document overview

1. Service overview
2. Preparation
3. Recording calibration data
4. Uploading calibration data and creating a task
5. Obtaining the calibration result
6. Error descriptions

### Service overview

As part of Apollo's whole-vehicle sensor calibration functionality, this service provides extrinsic calibration between the Velodyne 64-beam LiDAR HDL-64ES3 and the IMU. The calibration result can be used to transform obstacles detected by the LiDAR into the IMU coordinate system, and then into the world coordinate system. The calibration result is returned as a `.yaml` file.


### Preparation

To make better use of this service, please complete the preparation in the following order:

1. Install the 64-beam LiDAR and the GNSS/INS supported by Apollo, and download the image to set up the Docker environment.

2. Power on and start the 64-beam LiDAR and the GNSS/INS. The Novatel GNSS/INS needs to be calibrated when first powered on. At this time, drive the vehicle in an open area, going straight and making left and right turns, until the INS initialization is complete.

3. Confirm that all the topics of the sensor data required by this service have output. [How to check that sensors have data output?](../../15_FAQS/Calibration_FAQs_cn.md)

The topics required by this service are shown in Table 1 below:

Table 1. Sensor topic names

| Sensor       | Topic name                                  | Topic publishing frequency (Hz) |
| --------- | ---------------------------------------- | ------------- |
| HDL-64ES3 | /apollo/sensor/velodyne64/VelodyneScanUnified | 10            |
| INS       | /apollo/sensor/gnss/odometry             | 100           |
| INS       | /apollo/sensor/gnss/ins_stat             | 2             |

4. Confirm that the localization status is 56 when the vehicle collects the calibration data. [How to check the vehicle localization status?](../../15_FAQS/Calibration_FAQs_cn.md)

5. Choose a suitable calibration site.

The calibration site should have no tall buildings blocking the sky, flat ground, and flat buildings around, and should allow driving in the figure-8 trajectory shown in Figure 1. A suitable calibration site is shown in Figure 2.

![](images/calibration/lidar_calibration/trajectory.png)
<p align="center">Figure 1 The trajectory the vehicle must drive for calibration.</p>

![](images/calibration/lidar_calibration/field.png)
<p align="center">Figure 2 Calibration site.</p>

### Recording calibration data

After the preparation is complete, drive the vehicle into the calibration site to record calibration data.

1. The recording script tool is `apollo/script/lidar_calibration.sh`.

2. Run the following command to start recording data:

```bash
bash lidar_calibration.sh start_record
```

The recorded bag is in the `apollo/data/bag` directory.

3. Drive the vehicle in a figure-8 trajectory, keeping the speed at 20-40 km/h and the turning radius as small as possible. Driving for 3 minutes is enough, but make sure the calibration data contains at least one complete figure-8.

4. After recording is complete, enter the following command to end data recording:

```bash
bash lidar_calibration.sh stop_record
```

5. The program then checks whether the recorded bag contains all the required topics. After the check passes, the bag is packaged into a `lidar_calib_data.tar.gz` file, whose contents include the recorded rosbag and the corresponding MD5 checksum file.

### Uploading calibration data and creating a task

After the calibration data is recorded, log in to the [calibration service page](https://console.bce.baidu.com/apollo/calibrator/index/list) to complete the calibration.

1. Enter the calibration service page and, under the **Task Management** list, click the **New Task** button to create a new calibration task.

2. After entering the new task page, first fill in a brief task description, then click the **Upload Data and Create Task** button and select the calibration file to upload; data upload then begins.

3. After the data upload starts, the page jumps to the task flow view. The flow view shows the upload progress page; once it reaches 100%, calibration can begin. Keep the network connection stable during upload.

4. After the data upload is complete, the data verification process starts, as shown in Figure 3. The verification process ensures the data is complete and suitable for calibration. The verification items are:

* Data package decompression check
* MD5 check
* Data format check
* Figure-8 path and GPS quality check
* Initial extrinsic evaluation passed

If data verification fails, the corresponding error is reported. For the causes of errors, see the error descriptions.

![](images/calibration/lidar_calibration/calib_valid_cn.png)
<p align="center">Figure 3 Calibration data verification process.</p>

6. After verification passes, the calibration process starts, and a calibration progress page is shown to the user, as shown in Figure 4. Depending on the data size and quality, the overall calibration takes about 10-30 minutes, and the user can enter this page at any time to view the current progress of the calibration task.

![](images/calibration/lidar_calibration/calib_progress_cn.png)
<p align="center">Figure 4 Calibration progress page.</p>

7. After calibration is complete, the manual quality inspection step begins. Click the [View] button and the stitched point cloud for quality inspection pops up, and manual inspection can begin. If the inspection passes, click the **Confirm and Save** button to save the calibration result. Finally, click the **Download Data** button to download the calibration result; the calibration process is then complete. [How to perform quality inspection?](../../15_FAQS/Calibration_FAQs_cn.md)

### Obtaining the calibration result

1. Before the calibration result is obtained, this service requires the user to confirm the quality of the calibration result based on the visualization.

2. After confirming that the quality of the calibration result is acceptable, the user can click the **Confirm and Save** button to store the calibration result. It can then be downloaded on the task page; calibration results that have not passed quality inspection and been stored will not show a download link on the task page.

3. Extrinsic format description. The extrinsics are returned to the user as a yaml file; the following is a sample extrinsic result file.
Table 2 explains the meaning of several fields.

```bash
header:
  seq: 0
  stamp:
    secs: 1504765807
    nsecs: 0
  frame_id: novatel
child_frame_id: velodyne64
transform:
  rotation:
    x: 0.02883904659307384
    y: -0.03212457531272153
    z: 0.697030811535172
    w: 0.7157404339725393
  translation:
    x: 0.000908140840832566
    y: 1.596564931858745
    z: 1
```

Table 2. Meaning of fields in the extrinsic YAML file

| Field             | Meaning                      |
| -------------- | ----------------------- |
| header         | Header information, mainly containing the calibration time            |
| child_frame_id | ID of the source sensor being calibrated, here HDL-64ES3 |
| frame_id       | ID of the target sensor being calibrated, here Novatel  |
| rotation       | The rotation part of the extrinsics, expressed as a quaternion           |
| translation    | The translation part of the extrinsics                 |

4. How to use the extrinsics

First, enter the following command in the `/apollo` directory to create the calibration file directory:

```bash
mkdir -p modules/calibration/data/[CAR_ID]/
```
Here, **CAR\_ID** is the vehicle ID of the calibrated vehicle. Then copy the downloaded extrinsic yaml file into the corresponding **CAR\_ID** folder. Finally, after starting the HMI, select the correct **CAR\_ID** to load the corresponding calibration yaml file.

### Error descriptions

1. Data unpacking error: the uploaded data is not a valid `tar.gz` file.
2. MD5 checksum error: the MD5 checksum of the uploaded data differs from the MD5 checksum computed on the server, usually caused by network transmission problems.
3. Data format error: the uploaded data is not a rosbag, or the bag lacks the specified topics or contains other unspecified topics, so the server-side calibration program failed to read it.
4. No figure-8 path error: no figure-8 path was found in the uploaded data. Confirm that the recorded data contains at least one figure-8 path.
5. Insufficient GNSS/INS localization accuracy: the localization status in the uploaded data does not meet the requirements. Confirm that the localization status was 56 during recording.

