export interface PingLockServerRequest{
    serverAddress: string;
    serverPass: string;
}

export interface PushCommand{
    serverAddress: string;
    serverPass: string;
    motorCommand: MotorCommand;
}

export enum MotorCommand {
    Lock = "LOCK",
    Unlock = "UNLOCK"
}