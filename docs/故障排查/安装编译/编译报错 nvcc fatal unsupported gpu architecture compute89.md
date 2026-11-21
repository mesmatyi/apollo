### Problem Description:

Compilation fails with: nvcc fatal : Unsupported gpu architecture 'compute_89'

![](images/build_unsupported_compute_89.png)

### Cause:

This usually happens when the user's graphics card is a 40-series model and the project of Apollo 9.0 apollo-core is deployed. On the x86 architecture, Apollo is divided into the following versions:
apollo-core: the open-source version of Apollo, corresponding to the application-core repository on GitHub. Version 9.0 only supports 30-series and lower graphics cards, and does not support 40-series cards.
apollo-universe: the general campus edition of Apollo on the x86 architecture. The campus edition supports 40-series and lower graphics cards. If you are using a 40-series graphics card, please use the project of the general campus edition. For the deployment method and graphics card driver requirements, refer to the installation documentation of the campus edition.

### Solution:

If you must use 9.0, you can use the Apollo general campus edition. If you can use a newer version, please use Apollo 10.0, which supports 40-series graphics cards by default.
