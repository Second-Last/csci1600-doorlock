import { MotorCommand, PingLockServerRequest, PushCommandRequest } from "./models"
import CryptoJS from 'crypto-js';

export async function pingLockServer(params: PingLockServerRequest){
    const nonce = Date.now().toString();

    return await fetch(`http://${params.serverAddress}/status`, {
        method: 'GET',
        headers: {
            'X-Nonce' : nonce,
            'X-Signature' : CryptoJS.HmacSHA256(nonce, params.serverPass).toString()
        }
    })       
}

export async function pushMotorCommand(params: PushCommandRequest){
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