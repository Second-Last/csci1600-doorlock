import { Text, View, KeyboardAvoidingView, Image, TextInput, TouchableOpacity } from "react-native";
import { StyleSheet } from "react-native";
import { useState, useEffect } from 'react';
import { pingLockServer } from "./api/api";
import { useRouter } from 'expo-router';
import { useFonts } from "expo-font";
import AsyncStorage from '@react-native-async-storage/async-storage';

export default function Index() {

  const router = useRouter()
  const [serverAddress, setServerAddress] = useState<string>("");
  const [serverPass, setServerPass] = useState<string>("...");

  const [fontsLoaded, fontError] = useFonts({
    "SpaceGrotesk-Regular": require("../assets/fonts/SpaceGrotesk-Regular.ttf"),
    "SpaceGrotesk-Bold": require("../assets/fonts/SpaceGrotesk-Bold.ttf")
  });

  const handlePingLockServer = async () => {
    if(!serverAddress || !serverPass){
      return;
    }
    
    const response = await pingLockServer({
      serverAddress: serverAddress,
      serverPass: serverPass  
    })

    if(response.status === 200){
      await AsyncStorage.setItem('serverAddress', serverAddress);
      await AsyncStorage.setItem('serverPass', serverPass);

      router.push({
        pathname: '/commands'
      });
    }
  }

  return (
    <View
      style={{
        flex: 1,
        justifyContent: "center",
        alignItems: "center",
        marginHorizontal: 15
      }}
      >
      <Text style={styles.pageTitle}>
        Connect to Remote Lock
      </Text>
      <Image
        source={ require('../assets/images/internet-icon.png')}
        style={styles.lockImage}
        />

      <View
        style={{
          marginVertical: 20,
          justifyContent: "center",
          alignItems: "center",
        }}
        >
        <TextInput
        placeholder="Server Address"
        onChangeText={setServerAddress}
        style={styles.connectInput}
        />
        <TextInput
          placeholder="Password"
          onChangeText={setServerPass}
          secureTextEntry={true}
          style={styles.connectInput}
          />
        <TouchableOpacity
          onPress={handlePingLockServer}
          style={styles.connectButton}
          >
          <Text style={styles.connectText}>Connect</Text>
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
  connectButton: {
    marginTop: 50,
    marginBottom: 20,
    borderWidth: 1,
    borderRadius: 17.5,
    padding: 20,
    paddingVertical: 12.5,
    width: 300,
    backgroundColor: '#C00404',
    borderColor: '#C00404'
  },
  connectText: {
    textAlign: 'center',
    color: 'white',
    fontWeight: 'bold',
    fontSize: 18,
    letterSpacing: 1,
    fontFamily: 'SpaceGrotesk-Bold',
  }
})