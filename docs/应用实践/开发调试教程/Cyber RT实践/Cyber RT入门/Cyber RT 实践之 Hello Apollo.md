## Practice Content

Understand and modify the global configuration parameters.

This experiment uses Dreamview+ to run Scenario_Sim and simulate a traffic light scenario. By modifying software parameters, you will set the stopping distance at which the ego vehicle avoids the red light ahead and then passes once the light turns green.

- Goal: By adjusting the Traffic Rules configuration parameters, make the ego vehicle stop accurately at a position 2 m to 2.5 m from the stop line, without crossing it, when it detects a red light ahead. After the traffic light turns green, the ego vehicle should pass through the intersection smoothly.

- Method: Find the Traffic Rules configuration files in planning 2.0 and adjust the configuration parameters to meet the requirements of the traffic light scenario.

- Result: In the Apollo Dreamview+ simulation environment, observe the ego vehicle's behavior in the traffic light scenario and verify that it stops at a suitable position at the red light and passes through the intersection after the light turns green.

## Experience Link

https://apollo.baidu.com/community/course/19

## Experiment Objectives

- Become familiar with the workflow of the decision function in the Planning module and master how to debug traffic rule decisions;

- By running the Planning module, observe and analyze in detail the stopping and avoidance strategy of the ego vehicle, and its effect, when it detects a red light ahead.

## Prerequisites

For this experiment, please create a scenario set in the Apollo Studio workbench simulation. The scenario set must contain the following system scenario:

- Scenario name: `xh-contest-交通灯路口` (traffic light intersection; the literal name of the system scenario)

## Experiment Procedure

1. In the terminal, run the DreamView+ startup command. After it succeeds, click the dreamview+ button in the menu bar to enter the dreamview+ interface.
   
   ```bash
   aem bootstrap start --plus
   ```
   
   ![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Beta_Doc/image_1563a05.png)
   
   The following screen indicates that dreamview+ has started successfully.
   
   ![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Beta_Doc/image_023df8d.png)

2. Click **Personal Center** > **Settings** > **General Settings** in the lower left corner to choose the interface language.
   
   ![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Beta_Doc/image_0987230.png)

3. Open the **Mode Settings** panel in the left navigation bar, select **PnC** for **Mode** and **Scenario_Sim** for **Operations** to enter scenario simulation.

![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Beta_Doc/image_cdbd0fe.png)

4. Open the **Resource Manager** panel in the left navigation bar and download the corresponding scenario set.

![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Beta_Doc/image_8f326a6.png)

> Note: This experiment uses the demo scenario set as an example.

5. Go back to the **Mode Settings** panel, select **Brake on the sidewalk to avoid collision** from demo in **Enviroment Resources** as the simulation scenario, and select the **Mkz Example** vehicle as the vehicle parameters in **ADS Resources**.

![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Beta_Doc/image_3cd8dd4.png)

6. In the **Modules** menu bar, click the planning and prediction buttons to start the corresponding modules, and click the start-simulation button in the lower left corner.

![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Beta_Doc/image_9fa69ab.png)

7. Observe the simulation. The red line is the path found by the routing module on the map, and the light blue trajectory is the local path planned in real time by the planning module. By default, the distance between the stop wall and the traffic light stop line is 1.0 m.

![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Beta_Doc/image_f706c5b.png)

According to the experiment requirements, we need to stop within 2-2.5 m of the stop line and pass after the light turns green. By default, the experiment requirements are therefore not met. Next, we will modify the code with the online editor to implement this.

## Parameter Adjustment Practice

### Synchronize Traffic_light Configuration Parameters

**Synchronize Local Configuration Parameters**

1. Open a new terminal window and enter the configuration parameter synchronization command. The system will automatically copy the Traffic_light configuration parameters of the planning module into the default directory of the profile:
   
   ```bash
   buildtool profile config init --package planning-traffic-rules-traffic-light --profile=default
   ```
   
   Use the configuration in the default directory:
   
   ```bash
    # Use the configuration named default
    aem profile use default
   ```
   
   View the profile plugin directory structure:
   
   ```bash
   tree profiles/default/modules/planning/traffic_rules/
   ```
   
   Directory structure:
   
   ```bash
   profiles/default/modules/planning/traffic_rules/
   `-- traffic_light
    `-- conf
        `-- default_conf.pb.txt
   ```

### Adjust the Stopping Distance in the Traffic Light Scenario

1. Open the online editor:
   
   ![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Beta_Doc/image_90fdbbb.png)
   
   ![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Beta_Doc/image_e889136.png)

2. In the apollo_workspace working directory, find the configuration file `profiles/default/modules/planning/traffic_rules/traffic_light/conf/default_conf.pb.txt` and adjust the traffic light stop configuration parameter to achieve the desired stopping effect.

```bash
 # Original
 stop_distance: 1.0
 # Modified
 stop_distance: 2.2
```

3. After saving the changes, go back to dreamview+ and restart the Planning module in Modules (so that the system reloads the Planning parameters). Select the traffic light scenario again and observe how the stopping distance of the ego vehicle at a red light changes before and after the adjustment.
   
   ![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Beta_Doc/image_a0b0d74.png)
