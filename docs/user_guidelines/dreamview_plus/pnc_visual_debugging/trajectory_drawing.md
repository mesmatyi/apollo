# Trajectory Drawing

## Overview

By drawing a series of points on the map to represent the path between a start point and an end point, you can conveniently feed data into the planning algorithm and visualize the output of the path planning.

## Step 1: Install and Start Dreamview+

See [Play a Data Package](Apollo_alpha_doc/安装说明/软件包方式/快速上手/播放数据包.md).

## Step 2: Select a Simulation Mode

In **Operations**, select **Sim_Control** or **Scenario_Sim**. This section uses **Sim_Control** as an example.

## Step 3: Select a Map and Vehicle

1. In **Environment Resources**, select a map. This section uses **San Mateo** as an example.

2. In **ADS Resources**, select a vehicle. This section uses **Mkz Example** as an example.

   > Note: Selecting a vehicle is optional.

## Step 4: Start Trajectory Drawing

1. In the **Vehicle Visualization** panel, click **Route Editing** to enable the route editing function.

   ![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_alpha_doc/image_f5da9de.png)

2. Click the initial position icon at the upper left, then click on the map to set the initial position.

   ![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_alpha_doc/image_904b988.png)

   > Note: Press and hold the right mouse button to drag and move the map.

3. Click the trajectory point icon at the upper left, then click on the map to add waypoints.

   ![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_alpha_doc/image_17fbcca.png)

   > Note: Press, hold, and drag to change the direction of the initial position and trajectory points.

4. Click **Save Edit** to complete the trajectory drawing.

## Step 5: Enable Modules and View the Planning Results

Enable the Planning and Prediction modules, and click the play button in the bottom operation bar to view the planning results.

![5d25fa56f2b6054864a44150c234e487.jpg](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_alpha_doc/5d25fa56f2b6054864a44150c234e487_5d25fa5.jpg)
