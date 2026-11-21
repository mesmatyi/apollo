# planning-traffic-rules-speed-setting

## Introduction

`planning-traffic-rules-speed-setting` is the plugin in the planning module that handles, in real time, commands that modify the cruising speed. While the vehicle is driving at the currently set cruising speed, the user can modify the current cruising speed through `apollo::external_command::SpeedCommand` (see the description of the external-command-speed package).

The handling of the command that modifies the cruising speed is in the ApplyRule function. The processing steps of the function are:
- **Determining whether to accept and process a command**:
  - Processing happens only when a new SpeedCommand is received.
  - If a navigation command is received, the new navigation command is used as the base cruising speed.
  - If no SpeedCommand is received, the cruising speed after the last adjustment is used as the cruising speed for this cycle
  - If a received SpeedCommand fails to parse, the previous cruising speed is used
  - If a SpeedCommand that restores the cruising speed is received, the cruising speed originally set in ReferenceLineInfo is used
  - If a SpeedCommand that sets the value of the cruising speed is received, the speed in the SpeedCommand is used as the cruising speed for this cycle
  - If a SpeedCommand that sets the ratio of the cruising speed is received, the previous cruising speed is multiplied by the ratio to set the new cruising speed

## Directory Structure

```shell

modules/planning/traffic_rules/speed_setting/
├── speed_setting
    ├── conf                    // Parameter configuration files
    ├── docs                    // Content referenced by the documentation
    ├── BUILD                   // Build rules file
    ├── cyberfile.xml           // Package management configuration file
    ├── speed_setting.cc        // Source code of the speed setting plugin
    ├── speed_setting.h         // Header file of the speed setting plugin
    └── README.md               // Documentation
```

## Plugin

### SpeedSetting

apollo::planning::SpeedSetting

#### Usage
##### Adding the package dependency
If you run Apollo in package management mode, add the dependency on the plugin to the cyber.xml file of the package management:
```shell
<depend repo_name="planning-traffic-rules-speed-setting" type="binary">planning-traffic-rules-speed-setting</depend>
```
##### Plugin loading configuration
All traffic rule plugins are loaded and run in the planning package. To support handling the SpeedCommand command, add the following configuration to the configuration file modules/planning/planning_component/conf/traffic_rule_config.pb.txt:
```shell
    rule {
      name: "SPEED_SETTING"
      type: "SpeedSetting"
    }
```
