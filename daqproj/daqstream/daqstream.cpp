//*************************************************
//* daqstream
//* streams continuous data using WebSocket
//* 3rd April 2022 - rev 1 - shabaz
//* Arguments:
//* -r [n]    - sets the scan rate to n samp per sec
//* -i        - enables IEPE power supply
//* -d        - No ADC, uses dummy random values
//*************************************************

// ****************** includes *****************
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ws.h>
#include <math.h>
#include <time.h>
#include "daqhats_utils.h"
#include <algorithm>
#include <string>



// ****************** defines ******************
#define SCANRATE 51200.0
#define WS_PORT 8080
#define CHUNKSIZE 50

// ****************** globals *****************
int ws_fd = 0; // file descriptor/handle for websocket connection
char wsbuf[CHUNKSIZE * 8 * 2]; // double is 8 bytes, times 2 for two channels
uint32_t storednum=0; // number of sample pairs stored for transmission
int do_pause=0;


// ********* function prototypes **************
double calc_rms(double* data, uint8_t channel, uint8_t num_channels,
    uint32_t num_samples_per_channel);
void ws_onopen(int fd);
void ws_onclose(int fd);
void ws_onmessage(int fd, const unsigned char *msg, uint64_t size, int type);
void send_samples(double* data, uint8_t channel, uint8_t num_channels,
    uint32_t num_samples_per_channel);
char* getCmdOption(char ** begin, char ** end, const std::string & option);
bool cmdOptionExists(char** begin, char** end, const std::string& option);

// ************ main function *****************
int
main(int argc, char **argv)
{
    int result = RESULT_SUCCESS;
    uint8_t address = 0;
    char c;
    char display_header[512];
    int i;
    char channel_string[512];
    char options_str[512];
    double rate = SCANRATE;
    uint8_t iepe_enable = 0;
    int dummy = 0; 
    srand(time(NULL));

    char * ratestr = getCmdOption(argv, argv + argc, "-r");
    if (ratestr) {
        sscanf(ratestr, "%lf", &rate);
        printf("Requesting scan rate %f sps\n", rate);
    }

    if(cmdOptionExists(argv, argv+argc, "-i")) {
        iepe_enable = 1;
    }

    if(cmdOptionExists(argv, argv+argc, "-d")) {
        dummy = 1;
    }

    // Set the channel mask which is used by the library function
    // mcc172_a_in_scan_start to specify the channels to acquire.
    // The functions below, will parse the channel mask into a
    // character string for display purposes.
    uint8_t channel_mask = {CHAN0 | CHAN1};
    convert_chan_mask_to_string(channel_mask, channel_string);

    int max_channel_array_length;
    if (dummy) {
        max_channel_array_length = 2;
    } else {
        max_channel_array_length = mcc172_info()->NUM_AI_CHANNELS;
    }
    int channel_array[max_channel_array_length];
    uint8_t num_channels = convert_chan_mask_to_array(channel_mask,
        channel_array);

    // When doing a continuous scan, the timeout value will be ignored in the
    // call to mcc172_a_in_scan_read because we will be requesting that all
    // available samples (up to the default buffer size) be returned.
    double timeout = 5.0;
    double scan_rate = rate; //3200.0; //51200.0;
    double actual_scan_rate = 0.0;
    uint32_t options = OPTS_CONTINUOUS;
    uint16_t read_status = 0;
    uint32_t samples_read_per_channel = 0;
    uint8_t synced;
    uint8_t clock_source;
    uint32_t user_buffer_size = 2 * scan_rate * num_channels;
    uint32_t samples_per_channel = 2 * scan_rate;
    double read_buf[user_buffer_size];
    int total_samples_read = 0;
    int32_t read_request_size = READ_ALL_AVAILABLE;
    uint32_t j;

    // websocket
	struct ws_events evs;
	evs.onopen    = &ws_onopen;
	evs.onclose   = &ws_onclose;
	evs.onmessage = &ws_onmessage;
	ws_socket(&evs, WS_PORT, 1);


    // Select an MCC 172 HAT device to use.
    if (dummy==0) {
        if (select_hat_device(HAT_ID_MCC_172, &address)) {
            // Error getting device.
            return -1;
        }
    } else {
        //
    }

    printf ("\nSelected MCC 172 device at address %d\n", address);

    // Open a connection to the device.
    if (dummy==0) {
        result = mcc172_open(address);
        STOP_ON_ERROR(result);

        for (i = 0; i < num_channels; i++) {
            result = mcc172_iepe_config_write(address, channel_array[i],
                iepe_enable);
            STOP_ON_ERROR(result);
        }

        // Set the ADC clock to the desired rate.
        result = mcc172_a_in_clock_config_write(address, SOURCE_LOCAL, scan_rate);
        STOP_ON_ERROR(result);

        // Wait for the ADCs to synchronize.
        do {
            result = mcc172_a_in_clock_config_read(address, &clock_source,
                &actual_scan_rate, &synced);
            STOP_ON_ERROR(result);
            usleep(5000);
        } while (synced == 0);
    }

    convert_options_to_string(options, options_str);
    convert_chan_mask_to_string(channel_mask, channel_string) ;

    printf("\nMCC 172 daqstream\n");
    printf("    Channels: %s\n", channel_string);
    printf("    Requested scan rate: %-10.2f\n", scan_rate);
    printf("    Actual scan rate: %-10.2f\n", actual_scan_rate);
    printf("    Options: %s\n", options_str);
    if (iepe_enable) {
        printf("    IEPE supply enabled\n");
    }
    if (dummy) {
        printf("Dummy Mode (will use random values)\n");
    }

    printf("\nPress ENTER to start scan\n");
    scanf("%c", &c);

    // Configure and start the scan.
    // Since the continuous option is being used, the samples_per_channel
    // parameter is ignored if the value is less than the default internal
    // buffer size (10000 * num_channels in this case). If a larger internal
    // buffer size is desired, set the value of this parameter accordingly.
    if (dummy==0) {
        result = mcc172_a_in_scan_start(address, channel_mask, samples_per_channel,
            options);
        STOP_ON_ERROR(result);
    }

    printf("Starting scan ... Press ENTER to stop\n\n");

    // Create the header containing the column names.
    strcpy(display_header, "Samples Read    Scan Count    ");
    for (i = 0; i < num_channels; i++)
    {
        sprintf(channel_string, "Ch %d RMS  ", channel_array[i]);
        strcat(display_header, channel_string);
    }
    strcat(display_header, "\n");
    printf("%s", display_header);

    // Continuously update the display value until enter key is pressed
    do
    {
        // Since the read_request_size is set to -1 (READ_ALL_AVAILABLE), this
        // function returns immediately with whatever samples are available (up
        // to user_buffer_size) and the timeout parameter is ignored.
        if (dummy==0) {
            //if (do_pause) {
            //    while(do_pause) {
            //        usleep(10000);
            //    }
            //}
            result = mcc172_a_in_scan_read(address, &read_status, read_request_size,
                timeout, read_buf, user_buffer_size, &samples_read_per_channel);
            STOP_ON_ERROR(result);
        } else {
            for (j=0; j<user_buffer_size/2; j++) {
                read_buf[j] = (double)rand() / (double)RAND_MAX;
            }
            usleep(1000000);
            read_status = STATUS_RUNNING;
            result = RESULT_SUCCESS;
            samples_read_per_channel = user_buffer_size/10;
        }

        if (read_status & STATUS_HW_OVERRUN)
        {
            printf("\n\nHardware overrun\n");
            break;
        }
        else if (read_status & STATUS_BUFFER_OVERRUN)
        {
            printf("\n\nBuffer overrun\n");
            break;
        }

        total_samples_read += samples_read_per_channel;

        if (samples_read_per_channel > 0) {
            // Display the samples read and total samples
            printf("\r%12.0d    %10.0d  ", samples_read_per_channel,
                total_samples_read);

            // send the samples if the connection is open
            if (ws_fd != 0) {
                if (do_pause==0) {
                    send_samples(read_buf, i, num_channels,
                            samples_read_per_channel);
                }
            }

            // Calculate and display RMS voltage of the input data
            for (i = 0; i < num_channels; i++) {
                printf("%10.4f",
                    calc_rms(read_buf, i, num_channels,
                        samples_read_per_channel));
            }
            fflush(stdout);
        }

        usleep(100000);
    } while ((result == RESULT_SUCCESS) &&
           ((read_status & STATUS_RUNNING) == STATUS_RUNNING) &&
           !enter_press());

    printf("\n");

stop:
    if (dummy==0) {
        print_error(mcc172_a_in_scan_stop(address));
        print_error(mcc172_a_in_scan_cleanup(address));
    }

    // Turn off IEPE supply
    for (i = 0; i < num_channels; i++)
    {
        if (dummy==0) {
            result = mcc172_iepe_config_write(address, channel_array[i], 0);
            STOP_ON_ERROR(result);
        }
    }

    if (dummy==0) {
        print_error(mcc172_close(address));
    }

    if (ws_fd != 0) {
		ws_close_client(ws_fd);
        ws_fd = 0;
	}
    return 0;
}


// **************** functions *****************
double calc_rms(double* data, uint8_t channel, uint8_t num_channels,
    uint32_t num_samples_per_channel)
{
    double value;
    uint32_t i;
    uint32_t index;

    value = 0.0;
    for (i = 0; i < num_samples_per_channel; i++)
    {
        index = (i * num_channels) + channel;
        value += (data[index] * data[index]) / num_samples_per_channel;
    }

    return sqrt(value);
}

void ws_onopen(int fd)
{
	char *cli;
	cli = ws_getaddress(fd);
#ifndef DISABLE_VERBOSE
	printf("Connection opened, client: %d | addr: %s\n", fd, cli);
#endif
	ws_fd = fd;
	free(cli);
}

void ws_onclose(int fd)
{
	char *cli;
	cli = ws_getaddress(fd);
#ifndef DISABLE_VERBOSE
	printf("Connection closed, client: %d | addr: %s\n", fd, cli);
#endif
	free(cli);
}

void ws_onmessage(int fd, const unsigned char *msg, uint64_t size, int type)
{
	char *cli;
	cli = ws_getaddress(fd);
#ifndef DISABLE_VERBOSE
	printf("I receive a message: %s (size: %" PRId64 ", type: %d), from: %s/%d\n",
		msg, size, type, cli, fd);
#endif
	free(cli);
    if (strncmp((const char*)msg, "pause", size)==0) {
        do_pause=1;
        printf("pausing\n");
    } else if (strncmp((const char*)msg, "resume", size)==0) {
        printf("resuming\n");
        do_pause=0;
    }
    // can use ws_sendframe_txt or ws_sendframe_bin too
	ws_sendframe(fd, (char *)msg, size, true, type);
}

void send_samples(double* data, uint8_t channel, uint8_t num_channels,
    uint32_t num_samples_per_channel)
{
    uint32_t index = 0;

    uint32_t remaining;

    remaining = num_samples_per_channel;

    while(remaining>=(CHUNKSIZE-storednum)) {
        memcpy((void*)&wsbuf[storednum], (void*)&data[index], (CHUNKSIZE-storednum)*8*2);
        ws_sendframe_bin(ws_fd, (const char *)wsbuf, CHUNKSIZE*8*2, 0);
        index = index + ((CHUNKSIZE-storednum) * 2); // increase by number of double values read
        remaining = remaining - (CHUNKSIZE-storednum);
        storednum = 0;
    }
    if (remaining>0) {
        // we have few samples, send them anyway?
        memcpy((void*)&wsbuf[storednum], (void*)&data[index], remaining*8*2);
        ws_sendframe_bin(ws_fd, (const char *)wsbuf, remaining*8*2, 0);
        storednum = 0;
    }

}

char* getCmdOption(char ** begin, char ** end, const std::string & option)
{
    char ** itr = std::find(begin, end, option);
    if (itr != end && ++itr != end)
    {
        return *itr;
    }
    return 0;
}

bool cmdOptionExists(char** begin, char** end, const std::string& option)
{
    return std::find(begin, end, option) != end;
}

