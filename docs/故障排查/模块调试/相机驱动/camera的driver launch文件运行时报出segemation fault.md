### Problem Description:

A segmentation fault occurs when running the camera driver launch file.

### Cause:

The camera resolution does not match the default configuration.

### Solution:

Modify the configuration files /apollo/modules/drivers/camera/conf/camera_front_6mm.pb.txt and /apollo/modules/drivers/camera/conf/camera_front_12mm.pb.txt

![](images/camera_conf.png)

**Note:** Apollo currently supports only some camera resolutions; the 4K resolution (3840×2160) is not supported yet.

Commonly supported camera resolutions are as follows

- 640×480
- 1280×720
- 1920×1080
- 2560×1920

The supported camera resolution is calculated as follows

![](images/camera_max_size.png)

![](images/camera_usage.png)
