# How to play a record in the perception mode of Dreamview+

<!--
Step 1: Install the Dreamview plugin. See the quick start [Step 1: Install the Dreamview plugin](<Apollo_Doc_CN_9_0/安装说明/软件包方式/快速上手/步骤一：安装 Dreamview 插件.md>).

Step 2: Start Dreamview. See the quick start [Step 2: Start Dreamview.md](<Apollo_Doc_CN_9_0/安装说明/软件包方式/快速上手/步骤一：启动 Dreamview.md>).-->

## Method 1: Play a record through Dreamview+

1. Select the perception mode **Perception** .
2. Download the record.

   In **Resource Manager** > **Records**, first download the required perception record.

   ![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Doc_CN_9_0/image_2eda926.png)

3. Set the record playback parameters.

   - In the **Mode** drop-down box, select **Perception** .

   - In **Operation**, select **Record** .

   - In **Environment Resources**, select **Records** , and select the specific record to play, **sensor_rgb** .
   - In **Environment Resources**, select the corresponding **HDMap**; the map corresponding to this record is **Sunnyvale Big Loop** .

4. Add panels.

   On the left, select **Add Panel** and select the required panels, such as clicking to add the camera view **Camera View**, the point cloud view **Point Cloud**, the module latency panel **Module Delay** and the vehicle visualization panel **Vehicle Visulization** .

5. Click the play button in the bottom area.

   ![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Doc_CN_9_0/image_80d5943.png)

   You can see the record playback in the **Vehicle Visualization**, **Camera View** and **Point Cloud** panels.

## Method 2: Play a record through the command line

1. Start and open the Dreamview+ visualization tool.

   For how to start and open Dreamview+, see [Start Dreamview+](<Apollo_Doc_CN_9_0/安装说明/软件包方式/快速上手/步骤二：启动 Dreamview+.md>).

2. After entering the Apollo Docker environment, enter the following command to play the record:

   ```bash
   cyber_record play -f /home/apollo/.apollo/resources/records/sensor_rgb.record -l
   ```
