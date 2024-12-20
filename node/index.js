const {createDeviceServer} = require('./deviceServer');
const textDecoder=new TextDecoder;

const fs = require("fs");
let logData=[];

setInterval(()=>{
    try {
        // reading a JSON file synchronously
        fs.writeFile("./collected/data-"+(new Date()).getTime()+".json", JSON.stringify(logData), ()=>{});
        logData=[];
    } catch (error) {
        // logging the error
        console.error(error);
    
        throw error;
    }
}, 30000);

function onConnect(device){
    console.log('Device connected', device.name);
}

function onDisconnect(device, reason){
    console.log('Device', device.name, 'disconnected for reason:', reason);
}

function onData(device, data){
    const str=textDecoder.decode(data);
    const [time, temp, humidity, pressure, pressureAltitude] = str.split(",");
    console.log("Time:",Math.round((new Date()).getTime()/1000), "Temp", Number(temp), "Humidity:", Number(humidity), "Pressure:", Number(pressure), "PA:",Math.floor(Number(pressureAltitude*3.281)));
    logData.push({tm: Number(time), t: Number(temp), h: Number(humidity), p: Number(pressure)});
}

createDeviceServer(4004, onConnect, onDisconnect, onData);