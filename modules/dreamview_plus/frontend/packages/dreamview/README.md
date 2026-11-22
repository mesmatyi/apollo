# `dreamview`

> Declaration file and proto type for dreamview data

## Usage
The generated JavaScript file (dreamview_proto_bundle.js) is a CommonJS module that can be used in a Node.js environment or in front-end bundlers that support CommonJS (such as webpack and Browserify).

```
const protobuf = require('dreamview');

// Now you can use the protobuf object to access all methods and types defined in your .proto files.
// For example, suppose you have a message type named `planning`:
let message = protobuf.planning.create({ /* your data */ });

// Serialize the message to binary format
let buffer = protobuf.planning.encode(message).finish();

// Deserialize from binary format
let decodedMessage = protobuf.planning.decode(buffer);
```
