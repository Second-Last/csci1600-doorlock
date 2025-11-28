import { Text, View, KeyboardAvoidingView, TextInput, TouchableOpacity } from "react-native";
import { useState, useEffect } from 'react';
import { pingLockServer } from "./api/api";
import { useRouter } from 'expo-router';

export default function Index() {

  const router = useRouter()
  const [serverAddress, setServerAddress] = useState<string>("");
  const [serverPass, setServerPass] = useState<string>("...");

  const handlePingLockServer = async () => {
    if(!serverAddress || !serverPass){
      return;
    }
    
    const response = await pingLockServer({
      serverAddress: serverAddress,
      serverPass: serverPass  
    })

    if(response.status === 200){
      router.push({
        pathname: '/commands', 
        params: {
          serverAddress: serverAddress,
          serverPass: serverPass
        }
      });
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
      <Text>
        Connect to Remote Lock
      </Text>
      <TextInput
        placeholder="Server Address"
        onChangeText={setServerAddress}
        />
      <TextInput
        placeholder="Password"
        onChangeText={setServerPass}
        secureTextEntry={true}
        />
      <TouchableOpacity
        onPress={handlePingLockServer}
        >
        <Text>Connect</Text>
      </TouchableOpacity>
    </View>
  );
}
