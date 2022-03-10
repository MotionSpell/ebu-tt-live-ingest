const logger = require('util');
const net = require('net');
const WebSocketServer = require('ws').Server;

const _TO_TCP = '--> ';
const _FROM_TCP = '<-- ';
const _TARGET = '<-> ';

// debug request/response
function logMessage(path, prefix, value) {
    try {
        console.debug('ws2tcp[' + path + '] ' + prefix + value.replace(/[^A-Za-z 0-9 \.,\?""!@#\$%\^&\*\(\)-_=\+;:<>\/\\\|\}\{\[\]`~]*/g, '') + '\n');
    } catch (e) { console.warn(e) }
};

class TcpClient {

    constructor(host, port, log) {
        
        this.tcpClient = new net.Socket();
        this.tcpClient.setKeepAlive(true);

        this.tcpClient.on('end', () => {
            console.error("tcp connection closed. exit process.");
            this.destroy();
        });

        this.tcpClient.on('error', (err) => {
            switch (err.code){
                case 'ECONNREFUSED':
                    console.error(`failed to connect to tcp socket on ${err.address}:${err.port}`);
                    this.destroy(1);
                    break;
                default:
                    console.error(err);
                    this.destroy(1);
            }
        });
        
        this.tcpClient.connect(port, host, () => {
            console.log(_TARGET + 'connected');
        });
        
    }

    destroy = (errcode=0) => {
        this.tcpClient.end();
        this.tcpClient = null;
        process.exit([errcode]);
    }

    write = (chunk, encoding, cb) => {
        this.tcpClient.write(chunk, encoding, cb);
    }

    on = (name, callback) => {
        this.tcpClient.on(name, callback);
    }

    removeListener = (name, callback) => {
        this.tcpClient.removeListener(name, callback);
    }

}

class WebSocketPipe {

    constructor (session, target){
        this.session = session;
        this.target = target;
    }

    onTcpData = data => {
        console.log('received tcp data from target: ', data.toString('hex', 0, 32) + ' ' + data.toString('ascii', 0, 32));
        try {
            if (this.session.protocol === 'base64') {
                this.session.send(new Buffer(data).toString('base64'));
            } else {
                this.session.send(data, { binary: true });
            }
        } catch (e) {
            console.log("websocket exception, closing session");
            self.session.close();
        }
    };

    onTcpConnectionClosed = () => {
        console.log(_TARGET + "tcp disconnected, closing websocket session");
        this.session.close();
    };

    onWsMessage = msg => {
        console.log(_TO_TCP + msg.toString('hex', 0, 32) + ' ' + msg.toString('ascii', 0, 32));
        if (this.session.protocol === 'base64') {
            this.target.write(new Buffer(msg, 'base64'));
        } else {
            this.target.write(msg, 'binary');
        }
    };

    onWsError = (err) => {
        console.error(err)
    }

    onWsClose = (code, reason) => {
        console.log(_TARGET + "ws disconnected: " + code + '[' + reason + ']');
        this.destroy()
    };

    init = () => {
        this.session.on('close', this.onWsClose);
        this.session.on('error', this.onWsError);
        this.session.on('message', this.onWsMessage);
        this.target.on('data', this.onTcpData);
    }

    destroy = () => {
        this.session.removeListener('close', this.onWsClose);
        this.session.removeListener('error', this.onWsError);
        this.session.removeListener('message', this.onWsMessage);
        this.target.removeListener('data', this.onTcpData);
    };
}


const debug = true;

const tcpServerHost = '127.0.0.1';
const tcpServerPort = 9999;
let target = new TcpClient(tcpServerHost, tcpServerPort);

const wsPath = '/ebu-tt/publish'
let session = null;

const server = new WebSocketServer({ port: 9998, path: wsPath });
server.on('connection', function (webSocket) {

    if ( session == null ){
        console.log('new ' + webSocket.protocol);
        webSocket.on('close', () => {
            session = null
        });
        session = new WebSocketPipe(webSocket, target);
        session.init();
    } else {
        console.warn('a websocket session already exists');
        webSocket.close();
    }

});

module.exports = WebSocketPipe;