# Scenario Simulation

## Overview

Used to run and monitor the effect of planning and control algorithms. You can enable and disable the planning, control, and routing algorithms, sync scenarios from the cloud and run them, add different driving trajectories to a scenario, and, while running, view module delay and console logs, view the PnC algorithm data records through the monitoring module, and view the message information in each channel.

## Step 1: Install and Start Dreamview+

See [Play a Data Package](Apollo_alpha_doc/安装说明/软件包方式/快速上手/播放数据包.md).

## Step 2: Select a Mode

1. In the left navigation bar, select **Mode Settings**.
2. In **Mode**, select **PnC** mode.
3. In **Operations**, select **Scenario_Sim**.

## Step 3: Download a Scenario Set

1. In the left navigation bar, select **Resource Manager**.
2. Click **Scenarios** and download the required scenario set from the Apollo Studio cloud.
3. Click **Mode Settings** > **Environment Resources** > **Scenarios**, and select the scenario to run from the local scenario list.

## (Optional) Step 4: Select a Vehicle

In **ADS Resources**, select **Vehicle**.

## Step 5: Enable Modules

In **Modules**, enable the **Planning** and **Prediction** modules.

![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_alpha_doc/image_e21b23e.png)

## Step 6: Draw a Trajectory

1. In the **Vehicle Visualization** panel, click **Route Editing**.
2. Click the ![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_alpha_doc/image_4d04326.png) icon to add the initial point.
3. Click the ![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_alpha_doc/image_a559070.png) icon to add trajectory points, and click **Save Edit** when the drawing is complete.

![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_alpha_doc/image_804891a.png)

## Step 7: Run the Simulation

Click the play button in the bottom bar to run the simulation.

## Step 8: View Data

1. In the left navigation bar, click **Add Panel**.
2. Add the **PnC Monitor**, **Module Delay**, and **Console** panels to view module delay and console information.

   ![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_alpha_doc/image_ea6a228.png)

## Step 9: Debug and Modify Code

Debug the code according to your own needs.
