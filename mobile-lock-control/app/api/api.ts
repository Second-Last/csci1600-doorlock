import { PingLockServerRequest, PushCommand } from "./models"

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
    return await fetch(`${params.serverAddress}/api/command/push`, {
        method: 'POST',
        body: JSON.stringify({
            serverAddress: params.serverAddress,
            serverPass: params.serverPass,
            motorCommand: params.motorCommand.toString()
        }),
        headers: {
            'Content-type': 'application/json; charset=UTF-8'
        }
    })
}
