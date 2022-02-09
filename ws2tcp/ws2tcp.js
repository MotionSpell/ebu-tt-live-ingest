var logger = require('util');
var net = require('net');

var _TO_TCP = '--> ';
var _FROM_TCP = '<-- ';
var _TARGET = '<-> ';

// WebSocket Pipe library entry point
var WebSocketPipe = function (wsServer, debug, tcpHost, tcpPort) {
    this.debug = debug || false;
    this.wsPath = wsServer.path || '/';
    this.tcpHost = tcpHost || 'localhost';
    this.tcpPort = tcpPort;
    log(this.debug, this.wsPath, _TARGET, this.tcpHost + ':' + this.tcpPort);
    
    // start handling new connections
    var self = this;
    wsServer.on('connection', function (webSocket) {
        log(self.debug, self.wsPath, 'new ', webSocket.protocol);
        var target = _newTcpClient(self, webSocket);
        webSocket.on('message', function (msg) {
            log(self.debug, self.wsPath, _TO_TCP, msg.toString('hex', 0, 32) + ' ' + msg.toString('ascii', 0, 32));
            // log(self.debug, self.wsPath, _FROM_TCP, msg.toString('ascii'));
            if (webSocket.protocol === 'base64') {
                target.write(new Buffer(msg, 'base64'));
            } else {
                target.write(msg, 'binary');
            }
        });
        webSocket.on('close', function (code, reason) {
            log(self.debug, self.wsPath, _TARGET, "ws disconnected: " + code + '[' + reason + ']');
            target.end();
        });
        webSocket.on('error', function (a) {
            log(self.debug, self.wsPath, _TARGET, "ws error: " + a);
            target.end();
        });
    });
};

function _newTcpClient(self, webSocket) {
    var tcpClient = new net.Socket();
    tcpClient.setKeepAlive(true);
    tcpClient.connect(self.tcpPort, self.tcpHost, function () {
        log(self.debug, self.wsPath, _TARGET, 'connected');
    });
    tcpClient.on('data', function (data) {
        log(self.debug, self.wsPath, _FROM_TCP, data.toString('hex', 0, 32) + ' ' + data.toString('ascii', 0, 32));
        // log(self.debug, self.wsPath, _FROM_TCP, data.toString('ascii'));
        try {
            if (webSocket.protocol === 'base64') {
                webSocket.send(new Buffer(data).toString('base64'));
            } else {
                webSocket.send(data, { binary: true });
            }
        } catch (e) {
            log(self.debug, self.wsPath, _TARGET, "ws exception, closing tcp session");
            tcpClient.end();
        }
    });
    tcpClient.on('end', function () {
        log(self.debug, self.wsPath, _TARGET, "tcp disconnected");
        webSocket.close();
    });
    tcpClient.on('error', function () {
        log(self.debug, self.wsPath, _TARGET, "tcp connection error");
        tcpClient.end();
        webSocket.close();
    });
    return tcpClient;
};

// debug request/response
function log(debug, path, prefix, value) {
    if (debug) {
        try {
            console.log('ws2tcp[' + path + '] ' + prefix + value.replace(/[^A-Za-z 0-9 \.,\?""!@#\$%\^&\*\(\)-_=\+;:<>\/\\\|\}\{\[\]`~]*/g, '') + '\n');
        } catch (e) { console.warn(e) }
    }
};

module.exports = WebSocketPipe;

// create websocket server
var WebSocketServer = require('ws').Server;
var server = new WebSocketServer({ port: 9998, path: '/ebu-tt/publish' });

// create TCP server
var debug = true;
var tcpServerHost = '127.0.0.1';
var tcpServerPort = 9999;
var wspipe = new WebSocketPipe(server, debug, tcpServerHost, tcpServerPort);
