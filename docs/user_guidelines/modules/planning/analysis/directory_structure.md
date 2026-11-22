# Directory Structure

In the new version, planning is divided into layers, mainly two layers: the base package and plugins. The base package is the basic dependency layer for planning development, and plugins are a collection of function packages that users can customize and extend.

![image (5).png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Doc_CN_9_0/image%20%285%29_1dcf270.png)

The important directories and functions in planning are introduced below.

![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Doc_CN_9_0/image_0c73905.png)

The `planning_base` directory contains all of planning's basic algorithm dependencies and the planning parent class interfaces.

![image (1).png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Doc_CN_9_0/image%20%281%29_747f770.png)

The `scenarios` directory contains the existing scenario plugins, and each subdirectory is an independent plugin package.

![image (2).png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Doc_CN_9_0/image%20%282%29_c26e9f3.png)

The `tasks` directory contains all task plugins.

![image (3).png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Doc_CN_9_0/image%20%283%29_46d5e9e.png)

The `traffic_rules` directory contains all traffic rule plugins.

![image (4).png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Doc_CN_9_0/image%20%284%29_d841a55.png)

The `pnc_map` directory contains the pnc_map plugin. The pnc_map plugin generates the corresponding reference lines according to different map types and external commands, for use by subsequent scenario and task processing.

![image (6).png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Doc_CN_9_0/image%20%286%29_1c44e75.png)
