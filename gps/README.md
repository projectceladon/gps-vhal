1. Copy virtualgps to vendor/intel/
2. Apply the patch in the patches directory.
3. The Android test application is https://github.com/barbeau/gpstest. The application can be download from Google Play(GPSTest - Apps on Google Play). Copy one to gpstest directory. 
4. host\_gps\_client is a test program in Ubuntu.
```shell
cd host_gps_client/build
cmake ../
make
```
5. Usage:
		binlyd@binlyd[17:26:03]:/work/haas/local_render_gps$ ./host_gps_client -h
		./host_gps_client
		        -l, --lat lat
		        -o, --long long
		        -a, --alt alt
		        -c, --count count
		        -h, --help help
		        1. run
		                Please input command('quit' for quit):run
		                The command is : run
		        2. stop
		                Please input command('quit' for quit):stop
		                The command is : stop
		        3. quit
		                Please input command('quit' for quit):quit
		                The command is : quit
6. Contact me: bing.deng@intel.com
