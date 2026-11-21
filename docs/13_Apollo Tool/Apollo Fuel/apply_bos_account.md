# Baidu Cloud Object Storage (BOS) Registration and Basic Usage Guide
- [Baidu Cloud Object Storage (BOS) Registration and Basic Usage Guide](#百度云对象存储bos注册与基本使用向导)
  - [Overview](#概述)
  - [Prerequisites](#前提条件)
  - [Main steps](#主要步骤)
    - [1. Log in and activate BOS](#1登录开通bos)
    - [2. Create a bucket](#2创建bucket)
    - [3. Use the BOS client](#3使用bos客户端)
## Overview
This user manual is intended to help users register for and purchase the Baidu Cloud Object Storage (BOS) service, and use the client to log in to BOS and upload and download data.
## Prerequisites
Register a Baidu account for later use. [Login and registration site](https://passport.baidu.com/)

## Main steps
### 1. Log in and activate BOS
Open the [BOS home page](https://console.bce.baidu.com/) and log in with your Baidu account. The interface after a successful login is shown below:

![mian](images/login_main.png)

 Click Object Storage BOS; the interface is shown below:

![bos](images/login_bos.png)

Click the Activate Now button; the following interface pops up:

![bos_main](images/login_bos_main.png)

Click the Top Up button in the Finance section to top up; the interface that pops up is shown below:

![bos_recharge](images/login_bos_recharge.png)

Topping up is a pay-as-you-go model that charges by the capacity of standard storage used. Compared with the developer kit cloud service it is more cost-effective, and the balance can be withdrawn at any time.

### 2. Create a bucket
In the figure before the previous one, click New Bucket; the interface that pops up is shown below:

![create](images/bucket_create.png)

Note that in the Region field you should select your own region, set the read/write permission to private, and for the CDN billing method choose pay-by-traffic. This is a postpaid model: you pay for however much traffic you use, which is cost-effective. The interface after successful creation is shown below:

![main](images/bucket_main.png)

The permission setting is modified as shown below:

![authority](images/bucket_authority.png)

How to view the AK and SK after successful creation:

![ak](images/bucket_AK.png)

Click Access Key in the red box; the following interface pops up:

![sk](images/bucket_SK.png)

Click the Secret Key display button and enter the verification code to view the SK.

### 3. Use the BOS client
Download the desktop client software from the [BOS desktop client](https://cloud.baidu.com/doc/BOS/s/lk4tnbkrm) page; there are Windows and Mac versions. The following takes Windows as an example. The interface after successful installation is shown below when opened:

![ak_sk](images/bos_client_ak_sk.png)

After filling in your own AK and SK, click the right arrow, as shown below:

![security](images/bos_client_security.png)

Enter your security code in the settings and click Log in to see the bucket you created, as shown below:

![bucket](images/bos_client_bucket.png)

You can then upload and download single files or entire folders of data.