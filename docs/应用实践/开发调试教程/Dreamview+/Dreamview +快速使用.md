Dreamview+ is a web application built into Apollo that provides developers with a visual output interface for each autonomous driving module, such as the planned path, localization information and chassis status. While the autonomous vehicle is running, Dreamview+ lets you monitor the vehicle hardware and the status of each autonomous driving module in real time, start and stop the vehicle and individual modules through the human-machine interface, and locate problems with debugging tools such as the PnC monitor, giving developers more comprehensive control over the autonomous driving process. In this experiment, you will use Dreamview+ to play and analyze an autonomous driving offline data package.

## Experience Link

https://apollo.baidu.com/community/course/1

## Experiment Objectives

- Master the startup process and principles of Apollo;
- Master how to use Apollo's autonomous driving debugging tools to analyze and locate autonomous driving problems.

## Experiment Steps

### Step 1: Start Dreamview+

1. In the terminal, run the Dreamview+ startup command. After it succeeds, click the Dreamview+ button in the menu bar to enter the Dreamview+ interface.
   
   ```bash
   aem bootstrap start --plus
   ```
   
   ![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Beta_Doc/image_cb5f4fe.png)
   
   The following screen indicates that Dreamview+ has started successfully.
   
   ![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Beta_Doc/image_8455c10.png)
   
   Click **Personal Center** > **Settings** > **Global Settings** in the lower left corner to choose the interface language.
   
   ![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Beta_Doc/image_ce0ce76.png)

### Step 2: Play an Autonomous Driving Offline Data Package

1. Click the left menu bar, select the PnC mode, and select Record as the operation.
   
   ![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Beta_Doc/image_a601a39.png)

2. In Environment Resources > Data Packages, select demo3.5 (if there is no data package, you can go to the Resource Manager to download it). After selecting, click the play button and you will see that the offline data package demo_3.5 is being played.
   
   ![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Beta_Doc/image_5e8a3a5.png)

### Step 3: Run the PNC Monitor

The PNC Monitor component is on the right side of the interface. PNC Monitor displays the data curves related to the Planning and Control modules.

![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Beta_Doc/image_72ffb35.png)

### Step 4: Run the cyber_monitor Real-time Channel Monitor

Open a new terminal emulator, then enter and run cyber_monitor.

```bash
cyber_monitor
```

![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Beta_Doc/image_7920d63.png)

Use the left and right arrow keys to enter and exit a Channel, and press the esc key to exit cyber_monitor completely.

This concludes the experiment.
