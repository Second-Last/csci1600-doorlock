import { Text, View, Image, TouchableOpacity } from "react-native";
import { StyleSheet } from 'react-native';
import { pingLockServer, pushMotorCommand } from "./api/api";
import { MotorCommand, LockStatus } from "./api/models";
import { useRouter, useFocusEffect } from 'expo-router';
import { useFonts } from "expo-font";
import { useState, useEffect } from "react";
import AsyncStorage from "@react-native-async-storage/async-storage";

export default function PushCommand() {

    const [lockStatus, setLockStatus] = useState<string>();
    const [lockStatusMsg, setLockStatusMsg] = useState<string>();
    
    const router = useRouter();
    
    const [fontsLoaded, fontError] = useFonts({
        "SpaceGrotesk-Regular": require("../assets/fonts/SpaceGrotesk-Regular.ttf"),
        "SpaceGrotesk-Bold": require("../assets/fonts/SpaceGrotesk-Bold.ttf")
    });
    
    const updateLockStatusMsg = (newStatus : string) => {
        setLockStatus(newStatus)

        switch (newStatus){
            case LockStatus.Lock:
                setLockStatusMsg("Locked");
                break;
            case LockStatus.Unlock:
                setLockStatusMsg("Unlocked");
                break;
            case LockStatus.BusyMove:
                setLockStatusMsg("Busy");
                break;
            case LockStatus.BusyWait:
                setLockStatusMsg("Busy");
                break;
            default:
                setLockStatusMsg("Bad");
                break;
        }
    }

    const handlePushCommand = async (motorCommand: MotorCommand) => {  
        const serverAddress = await AsyncStorage.getItem('serverAddress');
        const serverPass = await AsyncStorage.getItem('serverPass');

        if(!serverAddress || !serverPass){
            return;
        }
        
        const response = await pushMotorCommand({
            serverAddress: serverAddress,
            serverPass: serverPass,
            motorCommand: motorCommand
        })

        if(response.status === 200){
            console.log("Command received");
            const text = (await response.text()).trim()
            
            updateLockStatusMsg(text)
            
        }
    }

    const updateLockStatus = async () => {
        const serverAddress = await AsyncStorage.getItem('serverAddress');
        const serverPass = await AsyncStorage.getItem('serverPass');

        if(!serverAddress || !serverPass){
            return;
        }

        const response = await pingLockServer({
            serverAddress: serverAddress,
            serverPass: serverPass
        });

        if (response.status === 200){
            const text = (await response.text()).trim();
            updateLockStatusMsg(text);
        }
    }

    useFocusEffect(() => {
         const updateStatusInterval = setInterval(() => {
            updateLockStatus()
        }, 2500)

        return () => {
            clearInterval(updateStatusInterval)
        }
    });

    return (
        <View
            style={{
                flex: 1,
                justifyContent: "center",
                alignItems: "center"
            }}
            >
            <Text style={styles.pageTitle}>
                Configure Remote Lock
            </Text>
            
            <Image
                source={(lockStatus === LockStatus.Lock)  ? require('../assets/images/lock-icon.png') : require('../assets/images/unlock-icon.jpg')}
                style={styles.lockImage}
            />

            <View
                style={{
                    flexDirection: 'row',
                    marginVertical: 20
                }}
                >
                <Text style={{
                    color: 'black',
                    fontFamily: 'SpaceGrotesk-Bold',
                    fontSize: 22
                }}>
                    Status:
                </Text>

                <Text style={{
                    fontFamily: 'SpaceGrotesk-Regular',
                    fontSize: 22,
                    color: '#ffc72c'
                }}>
                    {` ${lockStatusMsg}`}

                </Text>
            </View>
            
            <View
                style={{
                    marginVertical: 20,
                    alignItems: 'center',
                    justifyContent: 'center'
                }}
                >
                
                <View
                    style={{
                        flexDirection: 'row',
                    }}
                >
                    <TouchableOpacity
                        onPress={() => handlePushCommand(MotorCommand.Lock)}
                        style={styles.commandButton}
                    >
                        <Text style={styles.commandText}>
                            Lock
                        </Text>
                    </TouchableOpacity>
                    <TouchableOpacity
                        onPress={() => handlePushCommand(MotorCommand.Unlock)}
                        style={styles.commandButton}
                    >
                        <Text style={styles.commandText}>
                            Unlock
                        </Text>
                    </TouchableOpacity>
                </View>
                 
                <TouchableOpacity
                    onPress={() => { router.push("/") }}
                    style={styles.disconnectButton}>
                    <Text
                        style={styles.disconnectText}>
                        Disconnect
                    </Text>
                </TouchableOpacity>
            </View>
           
        </View>
    );
}


const styles = StyleSheet.create({
  pageTitle: {
    fontSize: 36,
    fontFamily: 'SpaceGrotesk-Bold',
    paddingVertical: 24,
    textAlign: 'center'
  },
    lockImage: {
    width: 125,
    height: 125,
    resizeMode: 'contain'
  },
  commandButton: {
    margin: 10,
    backgroundColor: 'black',
    borderWidth: 1,
    borderRadius: 12.5,
    padding: 20,
    paddingVertical: 12.5,
    width: 100,
    borderColor: 'black'
  },
  commandText: {
    textAlign: 'center',
    color: 'white',
    fontWeight: 'bold',
    fontSize: 14,
    fontFamily: 'SpaceGrotesk-Regular'
  },
  disconnectButton: {
    marginTop: 20,
    marginBottom: 20,
    borderWidth: 1,
    borderRadius: 17.5,
    padding: 20,
    paddingVertical: 12.5,
    width: 300,
    backgroundColor: '#C00404',
    borderColor: '#C00404'
  },
  disconnectText: {
    textAlign: 'center',
    color: 'white',
    fontWeight: 'bold',
    fontSize: 18,
    letterSpacing: 1,
    fontFamily: 'SpaceGrotesk-Bold',
  }
})