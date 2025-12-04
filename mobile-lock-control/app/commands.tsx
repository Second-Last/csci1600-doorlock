import { Text, View, KeyboardAvoidingView, TextInput, TouchableOpacity } from "react-native";
import { StyleSheet } from 'react-native';
import { useLocalSearchParams } from "expo-router";
import { pushMotorCommand } from "./api/api";
import { MotorCommand } from "./api/models";
import { useRouter } from 'expo-router';


export default function PushCommand() {

    // Locked + isMoving = Currently moving to be in the locked state
    // Unlocked + isMoving = Currently moving to be in the unlocked state
    // Locked + !isMoving = Currently held in the locked state
    // Unlocked + !isMoving = Currently held in the unlocked state
    // const [isMoving, setIsMoving] = useState<boolean>(false);
    // const [isLocked, setIsLocked] = useState<boolean>(false);
    // const [isUnlocked, setIsUnlocked] = useState<boolean>(false);
    const router = useRouter();
    const params = useLocalSearchParams();
    const { serverAddress, serverPass } = params;

    const handlePushCommand = async (motorCommand: MotorCommand) => {        
        const response = await pushMotorCommand({
            serverAddress: serverAddress,
            serverPass: serverPass,
            motorCommand: motorCommand
        })

        if(response.status === 200){
            console.log("Command received");
            // setIsMoving(true);
        }
    }

    const returnHome = () => {
        router.push('/');
    }

    return (
        <View
        style={{
            flex: 1,
            justifyContent: "center",
            alignItems: "center"
        }}
        >
            <Text>
                Configure Remote Lock
            </Text>
            <TouchableOpacity
                onPress={() => handlePushCommand(MotorCommand.Lock)}
            >
                <Text>
                    Lock
                </Text>
            </TouchableOpacity>
            <TouchableOpacity
                onPress={() => handlePushCommand(MotorCommand.Unlock)}
            >
                <Text>
                    Unlock
                </Text>
            </TouchableOpacity>
            <TouchableOpacity
                onPress={returnHome}
            >
                <Text>
                    Disconnect
                </Text>
            </TouchableOpacity>
        </View>
    );
}

const styles = StyleSheet.create({
      connectInput: {
        borderColor: '#C00404',
        borderWidth: 1.5,
        borderRadius: 10,
        padding: 20,
        paddingVertical: 15,
        fontSize: 18,
        fontFamily: 'SpaceGrotesk-Regular',
        margin: 20,
        marginBottom: 0,
        width: 300
    },
})