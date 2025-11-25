import { Text, View, KeyboardAvoidingView, TextInput, TouchableOpacity } from "react-native";
import { useState, useEffect } from 'react';

export default function Index() {

  const [serverAddr, setServerAddr] = useState<string>();
  const [serverPass, setServerPass] = useState<string>();

  return (
    // <View
    //   style={{
    //     flex: 1,
    //     justifyContent: "center",
    //     alignItems: "center",
    //   }}
    // >
    //   <Text>Edit app/index.tsx to edit this screen.</Text>
    // </View>
    <View
      style={{
        flex: 1,
        justifyContent: "center",
        alignItems: "center"
      }}
      >
      <TextInput
        placeholder="Server Address"
        onChangeText={setServerAddr}
        />
      <TextInput
        placeholder="Password"
        onChangeText={setServerPass}
        secureTextEntry={true}
        />
      <TouchableOpacity>
        <Text>Connect</Text>
      </TouchableOpacity>
    </View>
  );
}
