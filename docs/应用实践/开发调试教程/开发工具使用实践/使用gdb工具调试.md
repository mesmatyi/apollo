### How to Debug with gdb?

![](images/gdb_mind.png)

#### GDB Overview

GDB, the GNU Project debugger, allows you to see what is going on "inside" another program while it executes, or what another program was doing at the moment it crashed.

GDB can do four main kinds of things (plus other things in support of these) to help you catch bugs in the act:

- Start your program, specifying anything that might affect its behavior
- Make your program stop on specified conditions
- Examine what has happened when your program has stopped
- Change things in your program, so you can experiment with correcting the effects of one bug and go on to learn about another

These programs may be executed on the same machine as GDB (native), on another machine (remote), or on a simulator. GDB can run on most popular UNIX and Microsoft Windows variants, as well as on macOS.

#### Download the gdb Tool

The gdb debugging tool is not built into the apollo container, so users need to download it themselves

```shell
sudo apt install gdb
```

#### Compile Apollo Code with Debug Information

When compiling code with the buildtool tool, you can add debug information to the build output with the --dbg parameter

```shell
buildtool build --dbg 
```

#### Start GDB

##### gdb program

In Apollo, a module is usually started via mainboard plus a dag file, so we can use mainboard as the program and pass the dag file as an argument

```shell
gdb --args mainboard -d modules/my_component/dag/my_component.dag
```

![](images/gdb_mainboard.png)

The output "Reading symbols from mainboard...done." indicates that the program has been loaded

#### Debug the Program

##### Set a Breakpoint

The command to set a breakpoint is break, abbreviated b

- Set a breakpoint at the entry of the MyComponent::Init() function
  
  ```shell
  (gdb) b MyComponent::Init
  ```

![](images/gdb_b_init.png)

Note: The command line will prompt "Function "MyComponent::Init" not defined". This is because the program we want to debug is a dynamic library that has not been loaded yet. The next line, "Make breakpoint pending on future shared library load? (y or [n])", asks whether to set the breakpoint once the shared library is loaded later; enter "y" at this point

##### View Breakpoint Information

The command to view breakpoint information is info break, abbreviated i b

```shell
(gdb) info break
```

In the previous step we set a breakpoint at the MyComponent::Init function. With info break, we can see the information about the breakpoints that were set

![](images/gdb_info_command.png)

##### Run the Program

After the breakpoints are set, run again and the breakpoint information will be included in the debugging session

```shell
(gdb) r
```

![](images/gdb_r_command.png)

The program runs to the breakpoint (MyComponent::Init)

##### Execute Single Statements

The commands for running the program one statement at a time are next and step, abbreviated n and s

next: execute the program one step at a time, stepping over function calls

step: execute the program one step at a time, stepping into function calls

```shell
(gdb) n
```

```shell
(gdb) s
```

![](images/gdb_n_command.png)

##### View the Function Call Stack

The command to view the function call stack is backtrace, abbreviated bt

```shell
(gdb) bt
```

![](images/gdb_bt_command.png)

##### Continue Running Until the End

```shell
(gdb) c
```

![](images/gdb_third_c_command.png)

At this point we have done some simple debugging of our program. If you want more debugging options, refer to the [official gdb documentation](https://www.sourceware.org/gdb/documentation/)
