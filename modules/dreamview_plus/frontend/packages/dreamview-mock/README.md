# `dreamview-mock`

> `dreamview-mock` is a Node.js module for simulating the Dreamview WebSocket server. It provides a convenient way for developers to quickly add their own real business interfaces.

## Usage

In your project, you can start the mock server directly with the dreamview-mock command:
```bash
yarn lerna run dreamview-mock
```
The server starts automatically and listens on the specified port.

## Capacity
`dreamview-mock` provides two communication modes:

Publish-subscribe mode: after a client connects to the server, it can send a subscribe message to subscribe to a certain type of data. The server periodically sends this type of data to all subscribed clients. A client can also send an unsubscribe message to cancel the subscription.

Request-response mode: a client can send a request carrying a requestId, and the server returns the same requestId in its response. This simulates an HTTP-like request-response pattern.

# `dreamview-recorder`

> `dreamview-recorder` is a Node.js module for recording data from the Dreamview WebSocket server. It provides a convenient way for developers to quickly record their own real business interfaces.

## Usage

Command-line tool:
```bash
// Show help
yarn workspace @dreamview/dreamview-mock dreamview-record -H
```
