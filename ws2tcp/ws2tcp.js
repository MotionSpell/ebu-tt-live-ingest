const logger = require('util');
const net = require('net');
const WebSocketServer = require('ws').Server;

const formatPayload = data => {
    try {
        return (
            data.toString('hex', 0, 32) + ' ' + data.toString('ascii', 0, 32)
            ).replace(/[^A-Za-z 0-9 \.,\?""!@#\$%\^&\*\(\)-_=\+;:<>\/\\\|\}\{\[\]`~]*/g, '') + '\n'
    } catch (e) { 
        return data
    }
};

const log = (level, str) => {
    console[level]("[ws-server]", str);
}

class TcpClient {

    constructor(host, port) {
        
        this.tcpClient = new net.Socket();
        this.tcpClient.setKeepAlive(true);

        this.tcpClient.on('end', () => {
            log("error", "tcp connection closed. exit process.");
            this.destroy();
        });

        this.tcpClient.on('error', (err) => {
            switch (err.code){
                case 'ECONNREFUSED':
                    log("error", `failed to connect to tcp socket on ${err.address}:${err.port}`);
                    this.destroy(1);
                    break;
                default:
                    console.error(err);
                    this.destroy(1);
            }
        });
        
        this.tcpClient.connect(port, host, () => {
            log("info", `connected to ${host}:${port}`);
        });
        
    }

    destroy = (errcode=0) => {
        this.tcpClient.destroy();
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

    constructor (wsSession, tcpTarget, verbose=true){
        this.wsSession = wsSession;
        this.tcpTarget = tcpTarget;
        this.verbose = verbose;
    }

    onTcpData = data => {
        if (this.verbose){
            log("info", `tcp --> ${formatPayload(data)}`);
        }
        try {
            if (this.wsSession.protocol === 'base64') {
                this.wsSession.send(new Buffer(data).toString('base64'));
            } else {
                this.wsSession.send(data, { binary: true });
            }
        } catch (e) {
            log("warn", `closing websocket session: ${e}`);
            self.session.close();
        }
    };

    onTcpConnectionEnd = () => {
        log("error", "tcp disconnected, closing websocket session");
        this.wsSession.close();
    };

    onWsMessage = msg => {
        if (this.verbose){
            log("info", `ws --> ${formatPayload(msg)}`);
        }
        if (this.wsSession.protocol === 'base64') {
            this.tcpTarget.write(new Buffer(msg, 'base64'));
        } else {
            this.tcpTarget.write(msg, 'binary');
        }
    };

    onWsError = (err) => {
        log("error", `ws disconnected: ${err}`);
    }

    onWsClose = (code, reason) => {
        log("info", `ws closing: ${code} [${reason}]`);
        this.destroy();
    };

    init = () => {
        this.tcpTarget.on('data', this.onTcpData);
        this.tcpTarget.on('end', this.onTcpConnectionEnd);
        this.wsSession.on('close', this.onWsClose);
        this.wsSession.on('error', this.onWsError);
        this.wsSession.on('message', this.onWsMessage);
    }

    destroy = () => {
        this.tcpTarget.removeListener('data', this.onTcpData);
        this.tcpTarget.removeListener('end', this.onTcpConnectionEnd);
        this.wsSession.removeListener('close', this.onWsClose);
        this.wsSession.removeListener('error', this.onWsError);
        this.wsSession.removeListener('message', this.onWsMessage);
    };
}


const tcpServerHost = '127.0.0.1';
const tcpServerPort = 9999;
let target = new TcpClient(tcpServerHost, tcpServerPort);

const wsPath = '/'+ (process.env.SUBSTANCE_EBUTT_SEQUENCE_ID || 'ebu-tt') +'/publish';
let session = null;
let p = 9998;
try {
    let n = parseInt(process.env.SUBSTANCE_WS_PORT);
    if (n >= 0 && n < 65535){
        p = n;
    } else {
        throw Error();
    }
} catch (err){
    console.error(`Invalid port number ${n} specified with SUBSTANCE_WS_PORT - using default port 9998`);
}

const server = new WebSocketServer({ port: p, path: wsPath });

server.on('connection', function (webSocket) {

    if ( session == null ){
        log("info", `new ${webSocket.protocol}`);
        webSocket.on('close', () => {
            session = null
        });
        session = new WebSocketPipe(webSocket, target, verbose=true);
        session.init();
    } else {
        log("warn", "a websocket session already exists");
        webSocket.close();
    }

});

module.exports = WebSocketPipe;