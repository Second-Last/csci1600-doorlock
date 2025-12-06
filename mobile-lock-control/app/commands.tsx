import { Text, View, KeyboardAvoidingView, Image, TouchableOpacity } from "react-native";
import { StyleSheet } from 'react-native';
import { useLocalSearchParams } from "expo-router";
import { pushMotorCommand, getLockStatus } from "./api/api";
import { MotorCommand } from "./api/models";
import { useRouter } from 'expo-router';
import { useFonts } from "expo-font";
import { useState, useEffect } from "react";
import AsyncStorage from "@react-native-async-storage/async-storage";

export default function PushCommand() {

    const [isLocked, setIsLocked] = useState<boolean | null>(null);
    const [isMoving, setIsMoving] = useState<boolean>(false);
    const [updateStatusInterval, setUpdateStatusInterval] = useState<number>();
    
    const router = useRouter();
    // const params = useLocalSearchParams();
    // const { serverAddress, serverPass } = params;
    
    const [fontsLoaded, fontError] = useFonts({
        "SpaceGrotesk-Regular": require("../assets/fonts/SpaceGrotesk-Regular.ttf"),
        "SpaceGrotesk-Bold": require("../assets/fonts/SpaceGrotesk-Bold.ttf")
    });
    

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
        }
    }

    const updateLockStatus = async () => {
        const serverAddress = await AsyncStorage.getItem('serverAddress');

        if(!serverAddress){
            return;
        }

        const response = await getLockStatus({
            serverAddress: serverAddress
        });

        if (response.status === 200){
            if ((await response.text()) == MotorCommand.Lock){
                setIsLocked(true);
            }
            else{
                setIsLocked(false);
            }
            setIsMoving(false);
        }
        else{
            setIsMoving(true);
        }
    }

    useEffect(() => {
        setUpdateStatusInterval(
            setInterval(updateLockStatus, 1000)
        );

        return () => {
            clearInterval(updateStatusInterval)
        }
    }, [])

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
                source={ isLocked ? require('../assets/images/lock-icon.png') : require('../assets/images/unlock-icon.jpg')}
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
                {isMoving 
                    ?<Text style={{
                        fontFamily: 'SpaceGrotesk-Regular',
                        fontSize: 22,
                        color: '#ffc72c'
                    }}>
                        Moving...
                    </Text>
                    : <Text style={[{
                        fontFamily: 'SpaceGrotesk-Regular',
                        fontSize: 22
                    }, 
                        isLocked && { color: '#C00404'}, 
                        !isLocked && { color: 'green'}
                    ]}>
                        {isLocked ? " Locked" : " Unlocked"} 
                    </Text>
                }
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