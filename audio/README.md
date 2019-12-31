# The feature of virtual audio:
1. Audio playback/record via socket.
2. Socket server is in android. the client can be connected to server by requirement.

# Compile:
## Copy virtualaudio directory to vendor/intel/.
## Apply the patches in patches directory to CIC source code.
## Compile host\_audio\_client.
```shell
cd host\_audio\_client/build/
cmake ..
make
		../bin/linux/host_audio_out_client, ../bin/linux/host_audio_in_client are available.
```
## Compile packages/apps/SoundRecorder/
```shell
make SoundRecorder -j36
		out/target/product/cic_cloud/system/app/SoundRecorder/SoundRecorder.apk is available.
```
# Usage:
## Push media.
```shell
adb root
adb push yrzr.wav /sdcard/
```
## Playback:
1. Play music in Android with default music application.
2. run ./host\_audio\_out\_client in the directory which has work\_dir.
```shell
		Please input comand('quit' for quit):run
```
3. Pause/stop music in the music application.
4. Input stop in ./host\_audio\_out\_client:
```shell
		Please input comand('quit' for quit):stop
		audio_out_sock0.pcm is available.
```
5. Play audio\_out\_sock0.pcm by command: ffplay -f s16le -ar 48k -ac 2 audio\_out\_sock0.pcm

## Record:
1. yrzr\_8000\_mono.pcm should in the directory which has work\_dir.
2. Install SoundRecorder.apk.
2. Recard in Android with SoundRecorder.apk.
3. run ./host\_audio\_out\_client in the directory which has work\_dir.
```shell
		./host_audio_in_client
		Please input comand('quit' for quit):run
```
4. Stop recording in the SoundRecorder application.
5. Input stop in ./host\_audio\_in\_client:
```shell
		Please input comand('quit' for quit):stop
```
6. 3gpp file is in /sdcard.
```shell
		e.g: recording1002289895543849854.3gpp
```
7. Pull 3gpp file:
```shell
		adb pull /sdcard/recording1002289895543849854.3gpp ./
```
8. Check the 3gpp on host.
```shell
		vlc recording1002289895543849854.3gpp
		Note: No sound is heard if the data is not input to Android by ./host_audio_in_client.
```
# Contact me: bing.deng@intel.com
