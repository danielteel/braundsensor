#include <Arduino.h>
#include <Esp.h>
#include "net.h"
#include "utils.h"


Net::Net(String deviceName, String address, uint16_t port){
    this->deviceName=deviceName;
    this->hostAddress=address;
    this->port=port;
}

Net::~Net(){
    Client.stop();

    if (packetPayload){
        free(packetPayload);
        packetPayload=nullptr;
    }
}


void Net::errorOccured(String errorText){
    Client.stop();
    
    if (packetPayload){
        free(packetPayload);
        packetPayload=nullptr;
    }

    this->netStatus=NETSTATUS::NOTHING;
    this->recvState=RECVSTATE::LEN1;

    wasConnected=false;
    if (onDisconnected) onDisconnected();

    Serial.print("Net error occurred: ");
    Serial.println(errorText);
}

void Net::attemptToConnect(){
    if (!this->Client.connected()){
        if (wasConnected){
            wasConnected=false;
            if (onDisconnected) onDisconnected();
        }
        
        this->netStatus=NETSTATUS::NOTHING;
        this->recvState=RECVSTATE::LEN1;

        if (isTimeToExecute(this->lastConnectAttempt, connectAttemptInterval)){
            Serial.println("Attempting to connect to server...");
            if (this->Client.connect(this->hostAddress.c_str(), this->port)){
                this->Client.write((uint8_t)this->deviceName.length());
                this->Client.write(this->deviceName.c_str());
                this->netStatus=NETSTATUS::READY;
                if (onConnected) onConnected();
                wasConnected=true;
            }
        }
    }
}

void Net::setPacketReceivedCallback(void (*callback)(uint8_t*, uint32_t)){
    packetReceived=callback;
}

void Net::setOnConnected(void (*callback)(void)){
    onConnected=callback;
}
void Net::setOnDisconnected(void (*callback)(void)){
    onDisconnected=callback;
}

bool Net::sendString(String str){
    return sendPacket((uint8_t*)str.c_str(), str.length());
}

bool Net::sendBinary(uint8_t* data, uint32_t dataLength){
    return sendPacket(data, dataLength);
}

bool Net::ready(){
    return (netStatus==NETSTATUS::READY) && Client.connected();
}

bool Net::sendPacket(uint8_t* data, uint32_t dataLength){
    this->Client.write((uint8_t*)&dataLength, 4);
    this->Client.write(data, dataLength);
    return true;
}

void Net::byteReceived(uint8_t data){
    switch (recvState){
        case RECVSTATE::LEN1:
            packetLength=data;
            recvState=RECVSTATE::LEN2;
            break;
        case RECVSTATE::LEN2:
            packetLength|=(data<<8);
            recvState=RECVSTATE::LEN3;
            break;
        case RECVSTATE::LEN3:
            packetLength|=(data<<16);
            recvState=RECVSTATE::LEN4;
            break;
        case RECVSTATE::LEN4:
            packetLength|=(data<<24);
            payloadRecvdCount=0;
            if (packetPayload){
                free(packetPayload);
                packetPayload=nullptr;
            }
            if (packetLength==0){
                recvState=RECVSTATE::LEN1;
            }else{
                recvState=RECVSTATE::PAYLOAD;
                packetPayload=(uint8_t*)malloc(packetLength);
                if (!packetPayload){
                    errorOccured("Failed to allocate packet payload space");
                }
            }
            break;
        case RECVSTATE::PAYLOAD:
            packetPayload[payloadRecvdCount]=data;
            payloadRecvdCount++;
            if (payloadRecvdCount>=packetLength){
                recvState=RECVSTATE::LEN1;

                if (packetReceived) packetReceived(packetPayload, packetLength);
                
                if (packetPayload){
                    free(packetPayload);
                    packetPayload=nullptr;
                }
            }
            break;
        default:
            errorOccured("Unknown recvState");
            break;

    }
}

void Net::processIncoming(){
    while (this->Client.connected() && Client.available()>0){
        this->byteReceived(this->Client.read());
    }
}

bool Net::loop(){
    this->attemptToConnect();
    this->processIncoming();
    return ready();
}