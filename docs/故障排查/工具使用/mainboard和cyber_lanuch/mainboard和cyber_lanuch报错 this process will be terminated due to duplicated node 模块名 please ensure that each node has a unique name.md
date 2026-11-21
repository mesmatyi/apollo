### Problem Description:

Starting a module with mainboard or cyber_lanuch fails with: this process will be terminated due to duplicated node[module name], please ensure that each node has a unique name.

![](images/mainboard_error.png)

![](images/cyber_lanuch_error.png)

### Cause:

The module has already been started, and starting it again fails.

### Solution:

Use ps to check which modules are currently running, use kill to stop the running module, and then start it again.

![](images/ps_mainboard.png)
