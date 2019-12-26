1. Copy virtualgps to vendor/intel/
2. Apply the patch in patches directory.
3. host_gps_client is test program in Ubuntu.
	cd host_gps_client/build
	cmake ../
	make
   Usage
	binlyd@binlyd[17:26:03]:/work/haas/local_render_gps$ ./host_gps_client -h
	./host_gps_client
	            	-l, --lat lat
	            	-o, --long long
	            	-a, --alt alt
	            	-c, --count count
	            	-h, --help help
 	1. run
		Please input comand('quit' for quit):run
		The command is : run
 	2. stop
		Please input comand('quit' for quit):stop
		The command is : stop
 	3. quit
		Please input comand('quit' for quit):quit
		The command is : quit
