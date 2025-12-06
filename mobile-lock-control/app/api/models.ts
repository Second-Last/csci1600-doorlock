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

export enum LockStatus {
    Lock = "LOCK",
    Unlock = "UNLOCK",
    BusyWait = "BUSY_WAIT",
    BusyMove = "BUSY_MOVE",
    Bad = "BAD"
}