### Problem Description:

Running aem init fails with: outside from Apollo container env

![](images/aem_init.png)

### Cause:

Apollo 8.0 requires running aem init inside the container to initialize the workspace.

Note: aem init is the way to initialize the workspace in 8.0. For version 9.0, it is recommended to clone the GitHub project by following the tutorial, and there is no need to initialize it yourself to install Apollo.

### Related Documents:

- [Step 4: Install the example project](https://apollo.baidu.com/docs/apollo/latest/md_docs_2_xE5_xAE_x89_xE8_xA3_x85_xE6_x8C_x87_xE5_x8D_x97_2_xE5_x8C_x85_xE7_xAE_xA1_xE7_x90_x86_410bb1324792103828eeacd86377c551.html)
