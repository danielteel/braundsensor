const textEncoder = new TextEncoder;
const Server = require('net').Server;

class PACKETSTATE {
    // Private Fields
    static get NAMELEN() { return 0; }
    static get NAME()    { return 1; }
    static get LEN1()    { return 2; }
    static get LEN2()    { return 3; }
    static get LEN3()    { return 4; }
    static get LEN4()    { return 5; }
    static get PAYLOAD() { return 6; }
    static get ERROR()   { return 7; }
}

class Device {
    static socketTimeoutTime = 30000;

    constructor(socket, onConnect, onDone, onPacket){
        this.onDone=onDone;
        this.onPacket=onPacket;
        this.onConnect=onConnect;

        this.socket=socket;
        this.socket.setTimeout(this.constructor.socketTimeoutTime);
    
        socket.on('data', this.onData);

        this.packetState=PACKETSTATE.NAMELEN;

        this.nameLength=0;
        this.nameWriteIndex=0;
        this.name=null;

        this.payloadLength=0;
        this.payloadWriteIndex=0;
        this.payload=null;

        socket.on('end', () => {
            this.deviceErrored('socket ended');
        });        
        socket.on('timeout', () => {
            this.deviceErrored('socket timeout');
        });
        socket.on('error', (err)=>{
            this.deviceErrored('socket error');
        });
    }

    deviceErrored = (reason) => {
        this.socket.destroy();
        this.socket=null;
        this.payload=null;
        this.packetState=PACKETSTATE.ERROR;
        this.onDone(this, reason);
    }

    sendPacket = (data) => {
        if (typeof data==='string') data=textEncoder.encode(data);
        if (data && data.length>0xFFFFFFF0){
            return false;
        }
  
        const header=new Uint8Array([0, 0, 0, 0]);
        (new DataView(header.buffer)).setUint32(0, data.length, true);
        this.socket.write(header);
        this.socket.write(data);
        return true;
    }

    onFullPacket = (data) => {
        if (this.onPacket){
            this.onPacket(this, data);
        }
    }

    onData = (buffer) => {    
        for (let i=0;i<buffer.length;i++){
            const byte=buffer[i];
            if (this.packetState===PACKETSTATE.NAMELEN){
                this.nameLength=byte;
                this.name="";
                this.packetState=PACKETSTATE.NAME;
            }else if (this.packetState===PACKETSTATE.NAME){
                this.name+=String.fromCharCode(byte);
                this.nameWriteIndex++;
                if (this.nameWriteIndex>=this.nameLength){
                    this.packetState=PACKETSTATE.LEN1;
                    this.onConnect(this);
                }
            }else if (this.packetState===PACKETSTATE.LEN1){
                this.payloadLength=byte;
                this.packetState=PACKETSTATE.LEN2;

            }else if (this.packetState===PACKETSTATE.LEN2){
                this.payloadLength|=byte<<8;
                this.packetState=PACKETSTATE.LEN3;

            }else if (this.packetState===PACKETSTATE.LEN3){
                this.payloadLength|=byte<<16;
                this.packetState=PACKETSTATE.LEN4;

            }else if (this.packetState===PACKETSTATE.LEN4){
                this.payloadLength|=byte<<24;
                this.packetState=PACKETSTATE.PAYLOAD;

                if (this.payloadLength>0xFFFFFFF0){
                    this.deviceErrored('incoming packet exceeded 0xFFFFFFF0 length');
                    return;
                }

                this.payload = Buffer.alloc(this.payloadLength);
                this.payloadWriteIndex=0;

            }else if (this.packetState===PACKETSTATE.PAYLOAD){
                const howFar = Math.min(this.payloadLength, buffer.length-i);
                buffer.copy(this.payload, this.payloadWriteIndex, i, howFar+i);
                this.payloadWriteIndex+=howFar;
                if (this.payloadWriteIndex>=this.payloadLength){
                    //Process complete packet here
                    this.onFullPacket(this.payload);
                    this.packetState=PACKETSTATE.LEN1;
                }
                i+=howFar-1;
            }else{
                this.deviceErrored('unknown packet state');
                return;
            }
        }
    }
}


function createDeviceServer(port, onConnect, onDisconnect, onPacket){
    const server = new Server();
    let devices = [];

    const onDeviceDone = (device, reason) => {
        const devicesOriginalLength=devices.length;
        devices=devices.filter( (v) => {
            return !(v===device);
        });
        if (devices.length!==devicesOriginalLength-1){
        }
        onDisconnect(device, reason);
    }

    server.on('connection', function(socket) {
        const newDevice=new Device(socket, onConnect, onDeviceDone, onPacket);
        devices.push(newDevice);
    });

    server.listen(port, function() {
        //console.log(`Device server listening on port ${port}`);
    });

    return server;
}

module.exports={createDeviceServer};