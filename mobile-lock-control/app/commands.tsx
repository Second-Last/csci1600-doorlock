import { Text, View, KeyboardAvoidingView, TextInput, TouchableOpacity } from "react-native";
import { useLocalSearchParams } from "expo-router";
import { pushMotorCommand } from "./api/api";
import { MotorCommand } from "./api/models";


export default function PushCommand() {

    // Locked + isMoving = Currently moving to be in the locked state
    // Unlocked + isMoving = Currently moving to be in the unlocked state
    // Locked + !isMoving = Currently held in the locked state
    // Unlocked + !isMoving = Currently held in the unlocked state
    // const [isMoving, setIsMoving] = useState<boolean>(false);
    // const [isLocked, setIsLocked] = useState<boolean>(false);
    // const [isUnlocked, setIsUnlocked] = useState<boolean>(false);
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

    return (
        <View
        style={{
            flex: 1,
            justifyContent: "center",
            alignItems: "center"
        }}
        >
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
        </View>
    );
}
