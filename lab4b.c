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


void check_period(char *optarg){
    if(!isdigit(*optarg) || opt_period < 0){
        fprintf(stderr, "Invalid option argument for period. Please use an integer greater than 0 or default value of 1 second\n");
        fprintf(stderr, "%d\n", opt_period);
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
	fds[0].fd = 0;
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

			ssize_t bytes_read = read(0, input_buffer, SIZE_BUFFER);

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
            default:
                fprintf(stderr, "Invalid Arguments. Usage: ./lab4b --period=time_in_secs --scale=[FP] --log=filename\n");
                exit(1);
                break;
        }
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

        // START and STOP commands toggle report generation
	    if(cmd_report){    

	    	// Generate report to stdout
	        if(opt_scale == 'C'){  
	            fprintf(stdout, "%s %.1f\n", time_disp, celsius);
	        }
	        else
	            fprintf(stdout, "%s %.1f\n", time_disp, fahrenheit);

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

    mraa_aio_close(pinTempSensor);
    return 0;
}