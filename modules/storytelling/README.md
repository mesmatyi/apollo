# Storytelling

## Introduction
Storytelling is a global, high-level scenario manager used to coordinate actions across modules. To drive an autonomous vehicle safely on urban roads, complex planning scenarios are needed to ensure safe driving. These complex scenarios may involve different modules to ensure proper maneuvers. To avoid a sequence-based approach to such scenarios, a new, independent scenario manager, the "Storytelling" module, was created. This module creates Stories for complex scenarios, and these Stories trigger actions in multiple modules. According to some predefined rules, the module creates one or more Stories and publishes them to the "/apollo/storytelling" channel. The main advantage of this module is fine-tuning the driving experience and isolating complex scenarios into Stories, which can be subscribed to by other modules (such as planning and control).

## Module

### Storytelling

#### Input

| Channel name                               | Type                                            | <div style="width: 300pt">Description</div>                                           |
| ---------------------------------------- | ----------------------------------------------- | ------------------------------------------------------------------------------ |
| `/apollo/planning`   | `apollo::planning::ADCTrajectory`   | Planned trajectory output by planning |     

#### Output

| Channel name                               | Type                                            | <div style="width: 300pt">Description</div>                                           |
| ---------------------------------------- | ----------------------------------------------- | ------------------------------------------------------------------------------ |
| `/apollo/storytelling`   | `apollo::storytelling::Stories`   | Stories published by Storytelling | 

#### Configuration

| File path                                                                     | Type/Structure                                       | <div style="width: 300pt">Description</div> |
| ---------------------------------------------------------------------------- | ----------------------------------------------- | ------------------------------------ |
| `modules/storytelling/conf/storytelling_conf.pb.txt`                 | `apollo::storytelling::StorytellingConfig`              |Configuration file of Storytelling       |

#### Usage

##### Starting with mainboard

```shell
mainboard -d modules/storytelling/dag/storytelling.dag
```

##### Starting with cyber_launch

```shell
cyber_launch start modules/storytelling/launch/storytelling.launch
