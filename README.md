This is a replacement to the EBU/BBC Live Interoperability Toolkit.

The current repository contains two tools:
- ```ws2tcp```: a minimalist WebSocket to TCP bridge. Written with node.js.
- ```subtitle-live-ingester```: a C++ application that ingest EBU-TT-Live (WDR flavour) and outputs data for the Live Subtitle Inserter.

A sample EBU-TT-Live in available in the ```sample/``` folder.

```ws2tcp``` runs by default on port 9998 (Websocket) and outputs on TCP port 9999. To test:
- ```npm install ws```
- ```node ws2tcp```
- ```ncat -l 9999``` -> connect to tcp first (on port 9999 by default)
- https://websocketking.com/ -> then connect to WebSocket (on port 9998 by default)

```subtitle-live-ingester``` is responsible for:
 - extracting text from EBU-TT-Live 
 - add timings
 - segment
 - serialize to WebVTT and TTML dialects
 - feed the Subtitle Live Inserter input

