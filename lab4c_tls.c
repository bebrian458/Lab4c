// NAME:    Brian Be
// EMAIL:   bebrian458@gmail.com
// ID:  	204612203

#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <math.h>
#include <mraa/aio.h>   //temp sensor
#include <mraa/gpio.h>  //btn
#include <pthread.h>
#include <ctype.h>
#include <time.h>
#include <poll.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/types.h> // for socks
#include <netdb.h>
#include <openssl/ssl.h>

// Constants
const int B = 4275;               	// B value of the thermistor
const int R0 = 100000;            	// R0 = 100k
const int TIME_DISP_SIZE = 9;		// HH:MM:SS\0
const int SIZE_BUFFER = 1024;

// Flags and their default values
int opt_period = 1, opt_log = 0, cmd_off = 0, cmd_report = 1;
char opt_scale = 'F';

// Used to create/open logfile
FILE* logfile;

// Timer variables
time_t timer;
struct tm* converted_time_info;
char *time_disp;

// TCP - Socket
char *opt_id = "323227654";
char *hostname = "131.179.192.136";
int portno, sockfd;
struct sockaddr_in serv_addr;
struct hostent *server;

// TLS
SSL* ssl;


void check_period(char *optarg){
    if(!isdigit(*optarg) || opt_period < 0){
        fprintf(stderr, "Invalid option argument for period. Please use an integer greater than 0 or default value of 1 second\n");
        exit(1);
    }
}

void check_scale(){
    if(opt_scale != 'F' && opt_scale != 'C'){
        fprintf(stderr, "Invalid option argument for scale. Please use C for Celsius or F for Fahrenheit\n");
        exit(1);
    }
}

void check_logfile(){
    if(logfile == NULL){
        fprintf(stderr, "Error creating or opening\n");
        exit(1);
    }
}

void check_id(){

	// Check length of ID
	if(strlen(opt_id) != 9){
		fprintf(stderr, "Invalid ID number. Please enter a 9-digit number\n");
		exit(1);
	}

	// Check if each character is a valid digit
	int i;
	for(i = 0; i < 9; i++){
		if(!isdigit(opt_id[i])){
        	fprintf(stderr, "Not all ID characters are valid numbers. Please enter a 9-digit number\n");
        	exit(1);
        }
	}
}

void* check_btn(){

    // Initialize Grove - Button connect to D3
    mraa_gpio_context btn = mraa_gpio_init(3);       
    mraa_gpio_dir(btn, 1);

    while(1){

        // Constantly read time
	    time(&timer);
	    converted_time_info = localtime(&timer);
	    strftime(time_disp, TIME_DISP_SIZE, "%H:%M:%S", converted_time_info);

		// Constantly check for button or OFF command
        if(mraa_gpio_read(btn) || cmd_off){
        	if(opt_log)
        		fprintf(logfile, "%s SHUTDOWN\n", time_disp);
            //fprintf(stdout, "%s SHUTDOWN\n", time_disp);
            mraa_gpio_close(btn);
            exit(0);
        }
    }

    return NULL;
}

void* check_cmd(){

	// Make sure the temperature is read once, before polling for commands
	sleep(1);

	// Initialize poll for STDIN
	struct pollfd fds[1];
	fds[0].fd = sockfd;
	fds[0].events = 0;
	fds[0].events = POLLIN | POLLHUP | POLLERR;

	// Initialize command buffer and its index
	char *cmd_buffer = malloc(sizeof(char)*SIZE_BUFFER);
	memset(cmd_buffer, 0, SIZE_BUFFER);
	int cmd_index = 0;

	while(1){

		// Check poll status
		int poll_ret = poll(fds,1,0);
		if(poll_ret < 0){
			fprintf(stderr, "Error while polling\n");
			exit(1);
		}

		if(fds[0].revents & POLLIN){

			// Initialize input buffer and its index
			char input_buffer[SIZE_BUFFER];
			int input_index = 0;

			//ssize_t bytes_read = read(sockfd, input_buffer, SIZE_BUFFER);
			ssize_t bytes_read = SSL_read(ssl, input_buffer, SIZE_BUFFER);


			// Read the buffer one byte at a time
			while(bytes_read > 0 && input_index < bytes_read){

				// Check for EOF
				if(input_buffer[input_index] == -1){
					if(opt_log)
						fprintf(logfile, "%s\n", "EOF");
					cmd_off = 1;

					// TODO: check if this is the valid EOF handling
				}

				// Check for \r and \n of input_buffer to process cmd buffer
				if(input_buffer[input_index] == '\n'){

					// Process cmd_buffer
					if(strcmp(cmd_buffer, "OFF") == 0){
						if(opt_log){
						 	fprintf(logfile, "%s\n", cmd_buffer);
						 	fprintf(logfile, "%s SHUTDOWN\n", time_disp);
						}
						exit(0);
					}
					else if(strcmp(cmd_buffer, "STOP") == 0){
						if(opt_log)
						 	fprintf(logfile, "%s\n", cmd_buffer);
						cmd_report = 0;
					}
					else if(strcmp(cmd_buffer, "START") == 0){
						if(opt_log)
						 	fprintf(logfile, "%s\n", cmd_buffer);
						cmd_report = 1;
					}
					else if(strcmp(cmd_buffer, "SCALE=F") == 0){
						if(opt_log)
						 	fprintf(logfile, "%s\n", cmd_buffer);
						 opt_scale = 'F';
					}
					else if(strcmp(cmd_buffer, "SCALE=C") == 0){
						if(opt_log)
						 	fprintf(logfile, "%s\n", cmd_buffer);
						opt_scale = 'C';
					}
					else{

						// Parse the buffer into char matching and digit matching
						char char_buffer[8];
						memcpy(char_buffer, cmd_buffer, 7);
						char_buffer[7] = '\0';
						char digit_buffer[1024-7];
						memcpy(digit_buffer, &cmd_buffer[7], 1024-7);
						
						int digit_index = 0, isValidPeriod = 1;

						// Check if first 7 chars are PERIOD=
						if(strcmp(char_buffer, "PERIOD=") != 0){
							isValidPeriod = 0;
						}
						// Check if the rest of the chars are digits
						else{
							while(digit_buffer[digit_index] != '\0'){
								if(!isdigit(digit_buffer[digit_index])){
									isValidPeriod = 0;
									break;
								}
								digit_index++;
							}
						}

						// Process the valid period
						if(isValidPeriod){
							if(opt_log)
								fprintf(logfile, "%s\n", cmd_buffer);
							opt_period = atoi(digit_buffer);
						}
						// Otherwise print default case error
						else{
							//fprintf(stderr, "%s: not a valid command\n", cmd_buffer);
							if(opt_log)
								fprintf(logfile, "%s: not a valid command\n", cmd_buffer);
							exit(1);
						}
					}

					// Reset cmd_buffer and its index
					memset(cmd_buffer, 0, SIZE_BUFFER);
					cmd_index = 0;					
				}

				// Copy input_buffer into cmd_buffer
				else{
					cmd_buffer[cmd_index] = input_buffer[input_index];
					cmd_index++;
				}

				// Continue processing the next character in input_buffer
				input_index++;
			}
		}
	}
	return NULL;
}


int main(int argc, char *argv[]){
    
    // Default values
    int opt = 0;

    struct option longopts[] = {
        {"period",  required_argument,  NULL, 'p'},
        {"scale",   required_argument,  NULL, 's'},
        {"log",     required_argument,  NULL, 'l'},
        {"id",		required_argument,	NULL, 'i'},
        {"host",	required_argument, 	NULL, 'h'},
        {0,0,0,0}
    };

    while((opt = getopt_long(argc, argv, "p:s:l:", longopts, NULL)) != -1){
        switch(opt){
            case 'p':
                opt_period = atoi(optarg);
                check_period(optarg);
                break;
            case 's':
                opt_scale = *optarg;
                check_scale();
                break;
            case 'l':
                opt_log = 1;
                logfile =  fopen(optarg, "w");
                check_logfile();
                break;
            case 'i':
            	opt_id = optarg;
            	check_id();
            	break;
            case 'h':
            	hostname = optarg;
            	break;
            default:
                fprintf(stderr, "Invalid Arguments. Usage: ./lab4b --period=time_in_secs --scale=[FP] --log=filename\n");
                exit(1);
                break;
        }
    }

    /* TCP Server */
    // Initialize socket
    if(argv[optind] == NULL){
    	fprintf(stderr, "Not enough arguments. Please give a valid portnumber.\n");
    	exit(1);
    }
    portno = atoi(argv[optind]);

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0){
    	fprintf(stderr, "Error opening socket\n");
    	exit(1);
    }

    server = gethostbyname(hostname);
	if ( server == NULL ){
		fprintf(stderr, "Error: Invalid hostname.\n");
		exit(1);
	}

    memset((char*)&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    memmove(((char*)&serv_addr.sin_addr.s_addr), (char*)server->h_addr, server->h_length);
    serv_addr.sin_port = htons(portno);

    // Attempt to connect to server
    if(connect(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr))){
    	fprintf(stderr, "Error connecting to server\n");
    	exit(1);
    }

    /* TLS Server */
    // Initialize TLS
    SSL_library_init();
    SSL_load_error_strings();
    OpenSSL_add_all_algorithms();
    
    // Create an object as a framework to establish TLS/SSL enabled connections
    SSL_CTX* SSLClient = SSL_CTX_new(TLSv1_client_method());
    ssl = SSL_new(SSLClient);

    // Set sockfd as the input/output facility for TLS/SSL (encrypted) side of ssl
    if(SSL_set_fd(ssl, sockfd) == 0){
    	fprintf(stderr, "Error assigning sockfd as input/output for SSL\n");
    	exit(1);
    }

    // Initiate TLS/SSL handshake with an TLS/SSL server
    if(SSL_connect(ssl) != 1){
    	fprintf(stderr, "Error initiating the TLS/SSL handshake\n");
    	exit(1);
    }

    fprintf(stderr, "Established a connection to server. Sending reports...\n");

    // Create ID buffer
    char id_buffer[14] = "ID=";
    const char newline[2] = "\n";
    strcat(id_buffer, opt_id);
    strcat(id_buffer, newline);

    // First message sent to server should be ID buffer
    if (SSL_write(ssl, id_buffer, 14) < 0){
    	fprintf(stderr, "Error writing ID to server\n");
    	exit(1);
    }

    // Initialize Grove - Temperature Sensor connect to A0
    mraa_aio_context pinTempSensor = mraa_aio_init(0);

    // Allocate memory for time display
    time_disp = malloc(sizeof(char) * TIME_DISP_SIZE);
    if(time_disp == NULL){
    	fprintf(stderr, "Error allocating memory for time display\n");
    	exit(1);
    }

    // Set time to local time
    setenv("TZ", "PST8PDT", 1);
    tzset();

    // Read time first in case parent executes display before child thread updates time
    time(&timer);
    converted_time_info = localtime(&timer);
    strftime(time_disp, TIME_DISP_SIZE, "%H:%M:%S", converted_time_info);

    // Create thread to check button, avoiding problem of sleeping while button is pressed
    // Will also update time to timestamp SHUTDOWN message, even while ain thread is sleeping
    pthread_t btn_thread;
    if(pthread_create(&btn_thread, NULL, check_btn, NULL) < 0){
        fprintf(stderr, "Error creating thread for button\n");
        exit(1);
    }

    // Create thread to check for stdin commands even while main thread is sleeping
    pthread_t cmd_thread;
    if(pthread_create(&cmd_thread, NULL, check_cmd, NULL) < 0){
        fprintf(stderr, "Error creating thread for commands\n");
        exit(1);
    }

    // Main thread reads temperature
    while(1){

        // Read Temperature
        int a = mraa_aio_read(pinTempSensor);
        double R = 1023.0/((double)a)-1.0;
        R = R0*R;

        double celsius = 1.0/(log(R/R0)/B+1/298.15)-273.15; // convert to celsius temperature via datasheet
        double fahrenheit = celsius * 9/5 + 32;

        char server_report[64];

        // START and STOP commands toggle report generation
	    if(cmd_report){    

	    	// Generate report to stdout
	        if(opt_scale == 'C'){

	        	// Concatenate time and temp into server report
	        	if(snprintf(server_report, 64, "%s %.1f\n", time_disp, celsius) < 0){
	        		fprintf(stderr, "Error storing string into server_report\n");
	        		exit(1);
	        	}

	        	// Send report to server
	        	// if(write(sockfd, server_report, strlen(server_report)+1) < 0){
	        	if(SSL_write(ssl, server_report, strlen(server_report)+1) < 0){
	        		fprintf(stderr, "Error writing server_report to server\n");
	        		exit(1);
	        	}

	        	// Output to stdout
	            // fprintf(stdout, "%s %.1f\n", time_disp, celsius);
	        }
	        else{

	        	// Concatenate time and temp into server report
	        	if(snprintf(server_report, 64, "%s %.1f\n", time_disp, fahrenheit) < 0){
	        		fprintf(stderr, "Error storing string into server_report\n");
	        		exit(1);
	        	}

	        	// Send report to server
	        	// if(write(sockfd, server_report, strlen(server_report)+1) < 0){
	        	if(SSL_write(ssl, server_report, strlen(server_report)+1) < 0){
	        		fprintf(stderr, "Error writing server_report to server\n");
	        		exit(1);
	        	}

	        	// Output to stdout
	            // fprintf(stdout, "%s %.1f\n", time_disp, fahrenheit);
	        }

	        // Generate report to logfile 
	        if(opt_log){

	            if(opt_scale == 'C')
	                fprintf(logfile, "%s %.1f\n", time_disp, celsius);
	            else
	                fprintf(logfile, "%s %.1f\n", time_disp, fahrenheit);

	            // Flush to make sure it prints to logfile
	            fflush(logfile);
	        }
	    }

        sleep(opt_period);
    }

    // Close everything
    mraa_aio_close(pinTempSensor);
    close(sockfd);
    SSL_shutdown(ssl);

    return 0;
}