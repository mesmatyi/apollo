## Step 1: Start Dreamview+

You can start Dreamview+ through package management or from source. Choose one method and start Dreamview+ with the corresponding commands.

### 1. Start Dreamview+

#### Method 1: Package Management

Enter the docker environment through package management, and run the following command in the docker environment to start Dreamview+:

```bash
aem bootstrap start --plus
```

> Note:
> 
> - If you want to stop Dreamview+, enter `aem bootstrap stop --plus`,
> - If you want to restart Dreamview+, enter `aem bootstrap restart --plus`.

#### Method 2: Source Code

Enter the docker environment from source, and run the following command in the docker environment to start Dreamview+:

```bash
bash scripts/bootstrap.sh start_plus
```

> Note:
> 
> - If you want to stop Dreamview+, enter `bash scripts/bootstrap.sh stop_plus`,
> - If you want to restart Dreamview+, enter `bash scripts/bootstrap.sh restart_plus`.

### 2. Open Dreamview+

After it starts successfully, enter `localhost:8888` in the browser to open the Dreamview+ interface.

The following screen indicates that Dreamview+ has started successfully.

![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Beta_Doc/image_8455c10.png)

Click **Personal Center** > **Settings** > **Global Settings** in the lower left corner to choose the interface language.

![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Beta_Doc/image_ce0ce76.png)

## Step 2: Use SimControl to Simulate an Autonomous Driving Scenario

1. In the **Mode** settings, select **PNC Mode**, start the **Planning** module, select the **Sim_Control** operation mode, select **Sunnyvale Big Loop** for the HD map, and select **MKZ Example** for the vehicle.
   
   ![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Beta_Doc/image_fa3ada7.png)

2. Click the **Routing Editing** function of the **Vehicle Visualization** panel to enter the vehicle route setting interface.
   
   ![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Beta_Doc/image_669f410.png)

3. Set the start point and the waypoints (the last waypoint is the end point), then click Save Editing after you finish.
   
   ![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Beta_Doc/image_4ac1090.png)

4. After returning to the main interface, click the start button in the lower left corner and you will see the vehicle start running in the simulation environment.
   
   ![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Beta_Doc/image_4d87528.png)
