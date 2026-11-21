# Free Simulation

## Overview

Used to run and monitor the effect of planning and control algorithms. You can enable and disable the planning, control, and record-and-replay algorithms, add driving trajectories, and, while running, view module delay and console logs, view the PnC algorithm data records through the monitoring module, and view the message information in each channel.

## Step 1: Install and Start Dreamview+

See [Play a Data Package](Apollo_alpha_doc/安装说明/软件包方式/快速上手/播放数据包.md).

## Step 2: Select a Mode

1. In the left navigation bar, select **Mode Settings**.
2. In **Mode**, select **PnC** mode.
3. In **Operations**, select **Sim_Control**.

## Step 3: Select a Map and Vehicle

1. In the **Environment Resources** > **HDMap** list, select a map. This section uses **Sunnyvale Loop** as an example.
2. In **ADS Resources**, select a vehicle. This section uses **Mkz Example** as an example.

   > Note: Selecting a vehicle is optional.

## Step 4: Enable Modules

Enable the **Planning** and **Prediction** modules.

![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_alpha_doc/image_92a573f.png)

## Step 5: Draw a Trajectory

Edit the driving trajectory/route. See the document [Trajectory Drawing](Apollo_alpha_doc/使用指南/可视化交互工具 Dreamview+/PNC可视化调试/轨迹绘制.md).

## Step 6: Run the Simulation

Click the play button in the bottom bar to run the simulation.

## Step 7: View Data

1. Add the PnC Monitor, Module Delay, and Console panels.
2. Open the PnC Monitor to view planning and control data.
3. View Channel information, module delay, and console information.

## Step 8: Debug and Modify Code

You can debug and modify code based on the planning results.
