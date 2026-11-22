### Problem Description:

Compilation fails with: An error occurred during the fetch of repository'six'

![](images/repository_six_error.png)

### Cause:

Network problem. Accessing files.pythonhosted.org can result in download failures, because this website is not reliably accessible.

### Solution:
Simply retry.
If "Fetching@six;fetching" lasts more than 20s, retry. You may need to retry many times, but only one success is needed. Afterwards, do not delete the container and rerun unless necessary.
In addition, we have added the following snippet to the WORKSPACE file:

```shell
http_archive(    name = "six",
    sha256 = "5f58270f0c9fd489690bbf4bc98300e991cd456ce74124042014d5ebc6eaf093",
    urls = ["https://apollo-system.cdn.bcebos.com/archive/9.0/futures-3.3.0.tar.gz"],
)http_archive(    name = "enum34",
    strip_prefix = "enum34-1.1.6",
    sha256 = "8a87c334b113b18b2020d4ffafb0ec0557e103c6c347132140b64167457db558",
    urls = ["https://apollo-system.cdn.bcebos.com/archive/9.0/enum34-1.1.6.tar.gz"],
)http_archive(    name = "futures",
    strip_prefix = "futures-3.3.0",
    sha256 = "a5181a85f08c870672d9546f860575ab364d319ea6a3a2ec8cd732a030d505b6",
    urls = ["https://apollo-system.cdn.bcebos.com/archive/9.0/futures-3.3.0.tar.gz"],
)
```
