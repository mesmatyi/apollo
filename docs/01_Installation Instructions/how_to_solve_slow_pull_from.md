# Workaround for slow GitHub repository pulls from within China

## Step 1: Open the following two URLs in a browser and find the corresponding IP addresses:

1. http://github.com.ipaddress.com/
2. http://github.global.ssl.fastly.net.ipaddress.com/

Assume the corresponding IP addresses are 140.82.xx.xxx and 199.232.yy.yyy

## Step 2: Edit the hosts file

```
sudo vim /etc/hosts
```

Add the following two lines:

```
140.82.xx.xxx  github.com
199.232.yy.yyy github.global.ssl.fastly.net
```

## Step 3: Update the DNS cache (Mac users only)

Linux users can skip this step.

```
sudo dscacheutil -flushcache
```

All done!

Now you can try pulling the code from GitHub again. Is it a bit faster?

