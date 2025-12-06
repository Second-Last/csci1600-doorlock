import { MotorCommand, PingLockServerRequest, PushCommand } from "./models"
import CryptoJS from 'crypto-js';

export async function pingLockServer(params: PingLockServerRequest){
    return await fetch(`${params.serverAddress}/api/ping`, {
        method: 'POST',
        body: JSON.stringify({
            serverAddress: params.serverAddress,
            serverPass: params.serverPass
        }),
        headers: {
            'Content-type': 'application/json; charset=UTF-8'
        }
    })       
}

export async function pushMotorCommand(params: PushCommand){
    const path = (params.motorCommand === MotorCommand.Lock) ? "lock" : "unlock";
    const nonce = Date.now().toString();
    
    return await fetch(`http://${params.serverAddress}/${path}`, {
        method: 'POST',
        headers: {
            // 'Content-type': 'application/json; charset=UTF-8',
            'X-Nonce' : nonce,
            'X-Signature' : CryptoJS.HmacSHA256(nonce, params.serverPass).toString()
        }
    })
}