# 1. Scenario Set Synchronization

## Why does a scenario set created on Studio not appear in Dreamview+?

### Symptom

The user created the following scenario set
![](images/scenario_studio_list.png)
but it does not appear in the resource manager of Dreamview+
![](images/scenario_dv_list.png)

### Analysis

When the scenario set was created, the scenario type "Online Evaluation" was selected. The Dreamview+ plugin only synchronizes scenario sets of the offline simulation type; scenario sets of the online evaluation type are not necessarily usable locally.

### Solution

Please recreate the scenario set and select "Offline Simulation" as the scenario type.
![](images/scenario_created.png)

## Why are the scenarios in a scenario set not loaded or selectable after the scenario set is downloaded?

### Symptom

When selecting a scenario in a scenario set, the selected state cannot be triggered. Checking **data/log/dreamview_plus.INFO** shows a log like the following
![](images/scenario_select_log.png)

### Analysis

The map that matches the scenario does not exist locally.

### Solution

- If it is a map downloaded by default by Apollo and it is missing, re-download the required map with the command **buildtool reinstall map**
- If it is a map created by the user, place the map in the **/apollo/modules/map/data** directory inside the container

# 2. Scenario Simulation

## Why are there no obstacles after enabling scenario simulation and starting a scenario?

### Symptom

When scenario simulation starts, the vehicle starts, but there are no obstacles.

### Analysis

- The scenario contains no obstacles
- sim_obstacle is not installed
- The sim_obstacle process exited for some reason

### Solution

- The scenario contains no obstacles

Step 1: Open the front-end console in Dreamview+, then refresh the page after opening it
![](images/console_start.png)

Step 2: Select the socket named websocket

![](images/console_message.png)

Step 3: Click the scenario that has no obstacles. A message with the action CHANGE_SCENARIO appears in the front-end console. Click the message and expand it. The first value in value is the scenario set ID, and the second value is the scenario ID.
Here our scenario set ID is 6513c9a72026b08081b3784e and the scenario ID is 5bdd8355195cc4004461bc03

![](images/console_change_scenario.png)

Step 4: Enter the scenario set directory and print the content of the scenario

`cd	 ~/.apollo/resources/scenario_sets/6513c9a72026b08081b3784e/scenarios`
`cat 5bdd8355195cc4004461bc03.json`

Step 5: Check whether the agent field has content to determine whether the scenario has obstacles. If it has content, the scenario has obstacles; otherwise it does not.

![](images/scenario_attribute.png)

- sim_obstacle is not installed

Check whether sim_obstacle exists in the container with the command **ls /opt/apollo/neo/bin/sim_obstacle**. If it does not exist, install it with the command **buildtool reinstall --legacy sim-obstacle**

- The sim_obstacle process exited for some reason

If the scenario has obstacles and sim_obstacle exists, check whether the sim_obstacle process exists with the command **ps -ef | grep sim_obstacle**.
If it does not exist, the sim_obstacle process exited abnormally. Print the error messages with the command **tail -500f data/log/sim_obstacle.INFO** and contact us promptly, and we will investigate.

# 3. Plugins

## Why did the plugin synchronization fail?

### Symptom

The plugin has been downloaded from Studio, but the resource manager entry does not appear when Dreamview+ is opened
![](images/resource_manager_entry.png)

### Analysis

The Dreamview+ plugin may not be installed, or the plugin may be an old version.

### Solution

Step 1: Log in to the **Apollo Studio** workbench
![](images/work_station.png)
Step 2: In the account information, click **My Services**
![](images/work_station_service.png)
Step 3: Select the **Simulation** tab and click **Generate** under plugin installation
![](images/work_station_simulation.png)
Step 4: Click **One-Click Copy**, then run the command in your docker environment, and the plugin synchronization (Dreamview+ login) is complete
![](images/work_station_copy.png)
Restart and enter Dreamview+. If an avatar and the resource center button appear in the lower-left corner, the plugin has been synchronized successfully and you can start downloading resources.
