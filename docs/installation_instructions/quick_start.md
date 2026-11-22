# Quick Start

Prerequisites

Before reading and using this document, make sure you have followed the [Essential Software Installation Guide](../installation_instructions/essential_software_installation_guide_cn.md) document to complete the installation of the Apollo Environment Manager and the container environment

## Create a project, install Apollo packages, and play a record with Dreamview+

This scenario describes how to use Dreamview to play a record, laying the foundation for developers to become familiar with the Apollo platform. You can use Dreamview to play the record packages provided by Apollo
to further observe and learn about Apollo autonomous driving.

### Step 1: Enter the Apollo Docker environment

> The sample project used in this document can be obtained from the [github repository](https://github.com/ApolloAuto/application-core)

#### 1. Clone the sample project

```shell
git clone https://github.com/ApolloAuto/application-core.git application-core
cd application-core
```

#### 2. Start the Apollo environment container

```shell
aem start
```

#### 3. Enter the Apollo environment container

```shell
aem enter
```

### Step 2: Add and install dependencies

#### 1. Install the dependency packages

> What this operation actually does is build the `core` package in the project, but `core` itself has no code to build, so this operation only installs the dependency packages declared in `core/cyberfile.xml`

```shell
buildtool build -p core
```

### Step 3: Install the Dreamview+ plugin

Please follow the steps below to install the Dreamview+ plugin.

#### 1. Log in to the Apollo Studio workbench

Log in to the [Apollo Studio workbench](https://apollo.baidu.com/workspace).

#### 2. Get the plugin installation link

1. In the account information, click **My Services**

   ![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Studio/image_54b0f1e.png)

2. Select the **Simulation** tab.

3. In **Plugin Installation**, click **Generate**.

   ![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Studio/image_8624845.png)

4. Select the Apollo version and click **OK**.

   ![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Studio/image_75ee359.png)

#### 3. Run the plugin installation command in the docker environment

1. Enter the docker environment.

   ```bash
   # Pull and start the docker container
   aem start

   # Enter the container
   aem enter

   # Download and install dependency packages: this installs all the dependency packages in cyberfile.xml under the core directory
   buildtool build
   ```

2. In the docker environment, run the plugin installation command copied in the previous step.

   ![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Studio/image_76b7133.png)

### Step 4: Start and open Dreamview+

#### 1. Start Dreamview+

Enter the docker environment through package management, and run the following command in the docker environment to start Dreamview+:

```bash
aem bootstrap start --plus
```

> Note:
>
> - If you want to stop Dreamview+, enter `aem bootstrap stop --plus`,
> - If you want to restart Dreamview+, enter `aem bootstrap restart --plus`.

#### 2. Open Dreamview+

After a successful start, enter the URL `localhost:8888` in the browser to open the Dreamview+ interface.

### Step 5: Play a record

- Play a record in Dreamview+

  After starting Dreamview+, enter the Dreamview+ interface. You can choose the default mode or another mode to play a record. This section takes the default mode as an example.

  ![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_alpha_doc/image_7deb2d2.png)

  1.  Select **Default Mode**.
  2.  Check **Accept the User Agreement and Privacy Policy**, and click **Enter this Mode** to enter the **Mode Settings** page.
  3.  Download the record.

      In **Resource Manager** > **Records**, first download the required record.

      ![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Doc_CN_9_0/image_0b8f187.png)

  4.  On the **Mode Settings** page, set the record playback parameters.

      - In **Operations**, select **Record**.
      - In **Environment Resources**, click **Records**, and select the specific record you want to play.
      - In **Environment Resources**, click **HDMap**, and select **Sunnyvale Big Loop**.

  5.  Click the play button in the bottom area.

      ![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Doc_CN_9_0/image_45acc2d.png)

      You can see the record playback in **Vehicle Visualization**.

- Play a record through the command line

  1.  Enter the docker environment,
  2.  In Dreamview+, in **Resource Manager** > **Records**, first download the required record. Enter the following command to play the record:

      ```bash
      cyber_recorder play -f ~/.apollo/resources/records/record_name -l
      ```

      > Note: If you want to play the record in a loop, add -l; if you do not want to loop, there is no need to add -l.
