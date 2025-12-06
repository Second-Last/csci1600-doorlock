export interface GetLockStatusRequest{
    serverAddress: string;
}

export interface PingLockServerRequest{
    serverAddress: string;
    serverPass: string;
}

export interface PushCommandRequest{
    serverAddress: string;
    serverPass: string;
    motorCommand: MotorCommand;
}

export enum MotorCommand {
    Lock = "LOCK",
    Unlock = "UNLOCK"
}