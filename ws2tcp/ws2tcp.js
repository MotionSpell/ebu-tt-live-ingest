const net = require('net');
const fs = require('fs');
const WebSocketServer = require('ws').Server;

const useSslWss = process.env.SUBSTANCE_SSL_CERT && process.env.SUBSTANCE_SSL_KEY;

if (useSslWss){
    if (!fs.existsSync(process.env.SUBSTANCE_SSL_CERT)){
        console.warn("SSL/TLS certificate file not found:", process.env.SUBSTANCE_SSL_CERT);
    }
    if (!fs.existsSync(process.env.SUBSTANCE_SSL_KEY)){
        console.warn("SSL/TLS private key file not found:", process.env.SUBSTANCE_SSL_KEY);
    }
}

const server = useSslWss ? require('https').createServer({
    cert: fs.readFileSync(process.env.SUBSTANCE_SSL_CERT),
    key: fs.readFileSync(process.env.SUBSTANCE_SSL_KEY)
}) : null;

const padded = (v, n=2, f="0") => {
    return String(v).padStart(n, f);
};

const log = (level, str) => {
    const elapsed = process.uptime().toFixed(1);
    const t = new Date();
    console[level](`[${t.getUTCFullYear()}/${padded(t.getUTCMonth())}/${padded(t.getUTCDay())} ${padded(t.getUTCHours())}:${padded(t.getUTCMinutes())}:${padded(t.getUTCSeconds())}][${elapsed}][ws-server][${level}]`, str);
};

class TcpClient {

    constructor(host, port) {
        
        this.tcpClient = new net.Socket();
        this.tcpClient.setKeepAlive(true);

        this.tcpClient.on('end', () => {
            log("error", "connection to ingester closed. exit process.");
            this.destroy();
        });

        this.tcpClient.on('error', (err) => {
            switch (err.code){
                case 'ECONNREFUSED':
                    log("error", `failed to connect to ingester at ${err.address}:${err.port}`);
                    this.destroy(1);
                    break;
                default:
                    console.error(err);
                    this.destroy(1);
            }
        });
        
        this.tcpClient.connect(port, host, () => {
            log("info", `connected to ingester at ${host}:${port}`);
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

    constructor (wsSession, tcpTarget, verbose=false){
        this.wsSession = wsSession;
        this.tcpTarget = tcpTarget;
        this.verbose = verbose;
        this.startTime = new Date()
    }

    onTcpData = data => {
        log("info", `forwarding tcp data back to client: ${data.length} bytes`);
        if (this.verbose){
            log("debug", data.toString())
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
        log("error", "lost connection to ingester, closing websocket session");
        this.wsSession.close();
    };

    onWsMessage = msg => {
        log("info", `websocket message received - ${msg.length} bytes.`);
        if (this.verbose){
            log("debug", msg.toString())
        }
        if (this.wsSession.protocol === 'base64') {
            this.tcpTarget.write(new Buffer(msg, 'base64'));
        } else {
            this.tcpTarget.write(msg, 'binary');
        }
    };

    onWsError = (err) => {
        log("error", `websocket error - ${err}`);
    }

    onWsClose = (code, reason) => {
        log("info", `websocket connection closed - code=${code}, reason=[${reason}]`);
        this.destroy();
    };

    init = () => {
        this.tcpTarget.on('data', this.onTcpData);
        this.tcpTarget.on('end', this.onTcpConnectionEnd);
        this.wsSession.on('close', this.onWsClose);
        this.wsSession.on('error', this.onWsError);
        this.wsSession.on('message', this.onWsMessage);
        log("info", "websocket server is ready, awaiting client connection.")
    }

    destroy = () => {
        this.tcpTarget.removeListener('data', this.onTcpData);
        this.tcpTarget.removeListener('end', this.onTcpConnectionEnd);
        this.wsSession.removeListener('close', this.onWsClose);
        this.wsSession.removeListener('error', this.onWsError);
        this.wsSession.removeListener('message', this.onWsMessage);
        log("info", "websocket server destroyed.")
    };
}

const tcpServerHost = '127.0.0.1';
const tcpServerPort = 9999;
let target = new TcpClient(tcpServerHost, tcpServerPort);

const wsPath = '/'+ (process.env.SUBSTANCE_EBUTT_SEQUENCE_ID || 'ebu-tt') +'/publish';
let session = null;

let p = parseInt(process.env.SUBSTANCE_WS_PORT);
if (!(p >= 0 && p <= 65535)) {
    log('error', `invalid port number ${p} specified with SUBSTANCE_WS_PORT - using default port 9998`);
    p = 9998;
}

const wssCfg = { path: wsPath };

if (server){
    wssCfg["server"] = server;
} else {
    wssCfg["port"] = p;
}
const wss = new WebSocketServer(wssCfg);

wss.on('connection', function (webSocket) {
    if ( session == null ){
        webSocket.on('close', () => {
            session = null
            log("info", `ready to accept new client connections.`);
        });
        session = new WebSocketPipe(webSocket, target, verbose=false);
        session.init();
        log("info", `new websocket session initialized`);
    } else {
        log("warn", "a websocket session already exists. Attempt to open a new session rejected.");
        webSocket.close();
    }
});

if (server){
    server.listen(p);
}

module.exports = WebSocketPipe;