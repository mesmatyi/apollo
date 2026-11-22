# How to Play a Record in Perception Mode

## Prerequisites

Before playing a record, you need to download the Dreamview+ plugin, start and open Dreamview+, and then play the record. You can enter the docker environment, install, start and open the Dreamview+ plugin through either source code or package management. For details, see [Start Dreamview+ Step 1 and Step 2](Apollo_alpha_doc/安装说明/软件包方式/快速上手/播放数据包.md).

## Method 1: Play a record through Dreamview+

1. Select the perception mode **Perception**.
1. Download the record.

   In **Resource Manager** > **Records**, first download the required perception record.

   ![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Doc_CN_9_0/image_2eda926.png)

1. Set the record playback parameters.

   - In the **Mode** drop-down box, select **Perception**.

   - In **Operation**, select **Record**.

   - In **Environment Resources**, select **Records**, and select the specific record to play, **sensor_rgb**.
   - In **Environment Resources**, select the corresponding **HDMap**; the map corresponding to this record is **Sunnyvale Big Loop**.

1. Add panels.

   On the left, select **Add Panel** and select the required panels, such as clicking to add the camera view **Camera View**, the point cloud view **Point Cloud**, the module latency panel **Module Delay** and the vehicle visualization panel **Vehicle Visulization**.

1. Click the play button in the bottom area.

   ![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Doc_CN_9_0/image_80d5943.png)

   You can see the record playback in the **Vehicle Visualization**, **Camera View** and **Point Cloud** panels.

## Method 2: Play a record through the command line

1. Start and open the Dreamview+ visualization tool in the docker environment.

   You can enter docker through either package management or source code. For how to start and open Dreamview+, see [Start Dreamview+](Apollo_alpha_doc/安装说明/软件包方式/快速上手/播放数据包.md).

2. Download the record.

   In Dreamview+, under **Resource Manager** > **Records**, first download the required perception record. Here **demo2_mkz_210_sunnybigloop** is taken as an example.

3. Open a new terminal, and after entering the Apollo Docker environment, enter the following command to play the record:

   ```bash
   cyber_record play -f  ~/.apollo/resources/records/demo2_mkz_210_sunnybigloop -l
   ```
