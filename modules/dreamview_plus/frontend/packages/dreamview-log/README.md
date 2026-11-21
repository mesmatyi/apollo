# `@dreamview/log`

> description

This module is a library for creating and managing logs. It is built on the open-source library loglevel and provides a Logger class for creating and managing a variety of loggers.

Each instance of the Logger class can have its log level set independently and can output log messages with timestamps and log levels. The class is a singleton per name: for any given name, Logger.getInstance(name) always returns the same Logger instance. The instance is created on first request and reused in subsequent requests.




## Usage

```
const logger = Logger.getInstance('myLogger');
const logger = Logger.getInstance(__filename);

// DEMONSTRATE API
logger.debug('This is a debug message');
logger.info('This is an info message');
logger.warn('This is a warning message');
logger.error('This is an error message');
```
