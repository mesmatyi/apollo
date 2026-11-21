## Experiment Content

Apollo Cyber is the first high-performance open-source runtime framework designed specifically for autonomous driving. It mainly addresses the high concurrency, low latency, high throughput and task scheduling needs of autonomous driving systems, and also provides multiple communication mechanisms and user-level coroutines; when resources are limited, it schedules and processes tasks according to their priorities. This experiment describes how to configure and schedule task priorities.

## Experience Link

https://apollo.baidu.com/community/course/41#

## Experiment Objectives

Learn how Apollo sets Cyber's scheduling policy and configures task priorities.

## Experiment Procedure

The directory structure involved in this experiment is as follows:

```bash
/apollo
|-- cyber
|    | -- conf
|         | -- example_sched_choreography.conf       -----------task configuration file
|    | -- examples
|         | -- common_component_example
|              | -- common.dag                       -----------the dag file to start
|              | -- common.launch                    -----------the launch file to start
|-- data
|   |-- log
|       |-- mainboard.INFO                           -----------log file
|-- modules
|-- scripts
```

1. Modify `/apollo/cyber/conf/example_sched_choreography.conf`; task priorities can be set by changing the configuration. Four tasks, common0, common1, common2 and common3, are set here together with their priorities. In tasks, the task entries are configured: name is the name of the task and prio is the priority of the task; a larger value means a higher priority. The priority order of the four tasks, from high to low, is common3, common2, common1, common0.

   ```bash
   scheduler_conf {
    policy: "choreography"
    process_level_cpuset: "0"  # all threads in the process are on the cpuset

    choreography_conf {
        choreography_processor_num: 4
        choreography_affinity: "range"
        choreography_cpuset: "0"
        choreography_processor_policy: "SCHED_FIFO" # policy: SCHED_OTHER,SCHED_RR,SCHED_FIFO
        choreography_processor_prio: 10

        pool_processor_num: 4
        pool_affinity: "range"
        pool_cpuset: "0"
        pool_processor_policy: "SCHED_OTHER"
        pool_processor_prio: 0

        tasks: [
            {
                name: "common0"
                processor: 0
                prio: 1
            },
            {
                name: "common1"
                processor: 0
                prio: 2
            },
            {
                name: "common2"
                processor: 0
                prio: 3
            },
            {
                name: "common3"
                processor: 0
                prio: 4
            }
        ]
    }
   }
   ```

2. Under the path `/apollo/cyber/examples/common_component_example`, create common0.dag, common1.dag, common2.dag and common3.dag, and modify common.launch.

   The command is:

   ```bash
   touch common0.dag common1.dag common2.dag common3.dag
   ```

   The contents of the four files are as follows:

   ```bash
    # Define all coms in DAG streaming.
    module_config {
    module_library : "cyber/examples/common_component_example/libcommon_component_example.so"
    components {
        class_name : "CommonComponentSample"
        config {
            name : "common0"
            readers {
                channel: "/apollo/prediction"
            }
            readers {
                channel: "/apollo/test"
            }
        }
      }
    }
   ```

   ```bash
    # Define all coms in DAG streaming.
    module_config {
    module_library : "cyber/examples/common_component_example/libcommon_component_example.so"
    components {
        class_name : "CommonComponentSample"
        config {
            name : "common1"
            readers {
                channel: "/apollo/prediction"
            }
            readers {
                channel: "/apollo/test"
            }
        }
      }
    }
   ```

   ```bash
   # Define all coms in DAG streaming.
    module_config {
    module_library : "cyber/examples/common_component_example/libcommon_component_example.so"
    components {
        class_name : "CommonComponentSample"
        config {
            name : "common2"
            readers {
                channel: "/apollo/prediction"
            }
            readers {
                channel: "/apollo/test"
            }
        }
      }
    }
   ```

   ```bash
   # Define all coms in DAG streaming.
    module_config {
    module_library : "cyber/examples/common_component_example/libcommon_component_example.so"
    components {
        class_name : "CommonComponentSample"
        config {
            name : "common3"
            readers {
                channel: "/apollo/prediction"
            }
            readers {
                channel: "/apollo/test"
            }
        }
      }
    }
   ```

   The common.launch configuration is as follows:

   ```bash
   <cyber>
    <module>
        <name>common</name>
        <dag_conf>/apollo/cyber/examples/common_component_example/common0.dag</dag_conf>
        <dag_conf>/apollo/cyber/examples/common_component_example/common1.dag</dag_conf>
        <dag_conf>/apollo/cyber/examples/common_component_example/common2.dag</dag_conf>
        <dag_conf>/apollo/cyber/examples/common_component_example/common3.dag</dag_conf>
        <process_name>example_sched_choreography</process_name>
    </module>
   </cyber>
   ```

3. Run:

   ```bash
   channel_prediction_writer &
   channel_test_writer &
   ```

   Send messages to the channels /apollo/prediction and /apollo/test.

4. Start

   ```bash
   cyber_launch start /apollo/cyber/examples/common_component_example/common.launch
   ```

   This consumes the messages in the channels.

   ![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Beta_Doc/image_7874cef.png)

5. Open a new Terminal, go to the `/apollo/data/log` directory, and view the mainboard.INFO log. You can see that the tasks are executed in the order common3, common2, common1, common0, as expected.

   ```bash
   cd /apollo/data/log
   ll
   ```

   ![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Beta_Doc/image_08e9f94.png)

   ![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Beta_Doc/image_4632854.png)

   ![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Beta_Doc/image_37aa354.png)

   This concludes the experiment.
