//*************************************************
//* daqviki
//* saves continuous data to file
//* 5th April 2022 - rev 1 - shabaz
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
#include <math.h>
#include <time.h>
#include "daqhats_utils.h"
#include <algorithm>
#include <string>
#include "log_file.h"
#include <wiringPi.h>
#include "i2cfunc.h"
#include <linux/reboot.h>
#include <sys/reboot.h>
#include <sys/types.h>
#include <dirent.h>
#include <syslog.h>



// ****************** defines ******************
#define MYPATH "/home/pi/development/daqproj/daqviki/"
#define FILEPREFIX "log"
#define SCANRATE 51200.0
#define B1_PIN 24
#define B2_PIN 23
#define B1_STATUS digitalRead(B1_PIN)
#define B2_STATUS digitalRead(B2_PIN)
#define REDLED_PIN 4
#define REDLED_ON digitalWrite(REDLED_PIN, 1)
#define REDLED_OFF digitalWrite(REDLED_PIN, 0)
#define I2C_BUS 1
#define LCD_RESET_PIN 17
#define LCD_RES_HIGH digitalWrite(LCD_RESET_PIN, 1)
#define LCD_RES_LOW digitalWrite(LCD_RESET_PIN, 0)
#define LCD_ADDR 0x3e
#define LCD_LINE_1 lcd_setpos(0,0)
#define LCD_LINE_2 lcd_setpos(1,0)
#define MPAGE_STARTLOG 0
#define MPAGE_STOPLOG 1
#define MPAGE_SAMPRATE 2
#define MPAGE_IEPE 3
#define MPAGE_SHUTDOWN 4
#define MPAGE_TOT 5
#define MSRPAGE_TOT 5
#define MIEPEPAGE_TOT 2

// ****************** globals *****************
uint8_t g_chan_mask;
int i2c_handle = -1;
const char lcd_init_arr[]={ 0x38, 0x39, 0x14, 0x79, 0x50, 0x6c, 0x0c, 0x01 };
const char mpagetext[][17]= {
    "1=Start Log", "1=Stop Log", "1=SampRate", "1=IEPE", "1=Shutdown"
};
const char msrtext[][17]= {
    "1=51.2k", "1=25.6k", "1=12.8k", "1=6.4k", "1=3.2k"
};
const double ratearr[] = {
    51200.0, 25600.0, 12800.0, 6400.0, 3200.0
};
const char miepetext[][17]= {
    "1=IEPE Off", "1=IEPE ON"
};

char fname_nopath[32];

char mpage=0;
char oldmpage=-1;
char msrpage=0;
char oldmsrpage=0;
char logstate=0;
double rate = SCANRATE;
uint8_t iepe_enable = 0;
int dummy = 0; 

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
void init_io(void);
void release_resources(void);
void lcd_init(void);
void lcd_clear(void);
void lcd_setpos(char ln, char idx);
void lcd_print(char* str);
void lcd_print_line(char* str); // print line and erase remnant previous text
void handle_menu(char button, int* resp); // show menu and act on any button press
int do_scan(void);
void do_getsamprate(void);
void do_getiepe(void);
void do_shutdown();
void create_fname();

// ************ main function *****************
int main(int argc, char **argv)
{
    char bval = 0; // button value
    int maction = -1; // menu action
    int forever=1;
    
    openlog ("daqviki_log", LOG_CONS | LOG_PID | LOG_NDELAY, LOG_LOCAL1);
    syslog (LOG_NOTICE, "Program started by user %d", getuid());

    srand(time(NULL));
    init_io();
    REDLED_ON;
    lcd_init();

    lcd_clear();
    LCD_LINE_1;
    handle_menu(bval, &maction);

    // handle command line parameters
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
    strcpy(csv_filename, "file.csv");
    //printf("filename is %s\n", csv_filename);

    printf("\nPress correct menu item to start scan\n");

    while(forever) {
        while(maction<0) {
            // wait for a menu action to be selected
            bval = 0;
            usleep(50000);
            if (B1_STATUS) bval = 1;
            if (B2_STATUS) bval = 2;
            handle_menu(bval, &maction);
        }
        printf("maction is %d\n", maction);

        if (maction == MPAGE_STARTLOG) {
            // start log is requested.
            create_fname();
            sprintf(csv_filename, "%s%s", MYPATH, fname_nopath);
            printf("filename is %s\n", csv_filename);
            // once log is started, the only option will be to stop the log
            mpage = MPAGE_STOPLOG;
            handle_menu(0, &maction); // display the stop log menu
            do_scan(); // start the scan and logging
            // ok user requested the scan and logging to stop
            // revert to the start log menu page
            LCD_LINE_2;
            lcd_print_line(" "); // clear the second line
            oldmpage = MPAGE_STOPLOG;
            mpage = MPAGE_STARTLOG;
            handle_menu(0, &maction); // display the start log menu
            usleep(1000000); // needed because the debounce is basic
        }
        if (maction == MPAGE_SAMPRATE) {
            do_getsamprate();
        }
        if (maction == MPAGE_IEPE) {
            do_getiepe();
        }
        if (maction == MPAGE_SHUTDOWN) {
            do_shutdown();
        }

        maction = -1;
    }
    return(0);
}

// ***************** do_scan **************************
int
do_scan(void)
{
    int result = RESULT_SUCCESS;
    uint8_t address = 0;
    char c;
    int retval = 0;
    char display_header[512];
    int i;
    char channel_string[512];
    char options_str[512];
    double vrms[2];
    char vbuf[32];

    log_file_ptr = open_log_file(csv_filename);

    syslog (LOG_INFO, "do_scan open log file %s", csv_filename);

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
    syslog (LOG_INFO, "Selected MCC 172 at address %d", address);

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

        syslog (LOG_INFO, "ADC clock set to %f", scan_rate);

        // Wait for the ADCs to synchronize.
        do {
            result = mcc172_a_in_clock_config_read(address, &clock_source,
                &actual_scan_rate, &synced);
            STOP_ON_ERROR(result);
            usleep(5000);
        } while (synced == 0);
        syslog (LOG_INFO, "ADCs are synced");
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

    // Configure and start the scan.
    // Since the continuous option is being used, the samples_per_channel
    // parameter is ignored if the value is less than the default internal
    // buffer size (10000 * num_channels in this case). If a larger internal
    // buffer size is desired, set the value of this parameter accordingly.
    syslog (LOG_INFO, "Starting scan..");
    if (dummy==0) {
        result = mcc172_a_in_scan_start(address, channel_mask, samples_per_channel,
            options);
        STOP_ON_ERROR(result);
    }
    syslog (LOG_INFO, "Initializing the log file..");
    printf("initializing the log file\n");
    retval = init_log_file(log_file_ptr, channel_mask, 2 /*MAX_CHANNELS*/);
    if (retval < 0) {
        printf("error initializing the log file!\n");
        exit(1);
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
    syslog (LOG_INFO, "Continuous running started");
    // Continuously update the display value until enter key is pressed
    do
    {
        // Since the read_request_size is set to -1 (READ_ALL_AVAILABLE), this
        // function returns immediately with whatever samples are available (up
        // to user_buffer_size) and the timeout parameter is ignored.
        if (dummy==0) {
            result = mcc172_a_in_scan_read(address, &read_status, read_request_size,
                timeout, read_buf, user_buffer_size, &samples_read_per_channel);
            if (result==RESULT_SUCCESS) {
                //
            } else {
                syslog (LOG_NOTICE, "scan_read error!");
            }
            STOP_ON_ERROR(result);
            retval = write_log_file(log_file_ptr, read_buf, samples_read_per_channel, num_channels);
            if (retval < 0) {
                printf("error writing to the log file!\n");
                syslog (LOG_NOTICE, "error writing to log file!");
                exit(1);
            }
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
            syslog (LOG_NOTICE, "hardware overrun!");
            break;
        }
        else if (read_status & STATUS_BUFFER_OVERRUN)
        {
            printf("\n\nBuffer overrun\n");
            syslog (LOG_NOTICE, "buffer overrun!");
            break;
        }

        total_samples_read += samples_read_per_channel;

        if (samples_read_per_channel > 0) {
            // Display the samples read and total samples
            printf("\r%12.0d    %10.0d  ", samples_read_per_channel,
                total_samples_read);

            // Calculate and display RMS voltage of the input data
            for (i = 0; i < num_channels; i++) {
                vrms[i] = calc_rms(read_buf, i, num_channels,
                        samples_read_per_channel);
                printf("%10.4f", vrms[i]);
            }
            fflush(stdout);
            LCD_LINE_2;
            sprintf(vbuf, "%8.3f%8.3f", vrms[0], vrms[1]);
            lcd_print_line(vbuf);
        }

        usleep(100000);
    } while ((result == RESULT_SUCCESS) &&
           ((read_status & STATUS_RUNNING) == STATUS_RUNNING) &&
           /*!enter_press() &&*/ (B1_STATUS==0));

    printf("\n");

stop:
    syslog (LOG_INFO, "stopping do_scan");
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

    syslog (LOG_INFO, "exiting do_scan");
    return 0;
}


// **************** other functions *****************
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

void init_io(void)
{
    wiringPiSetupGpio();
    pinMode(B1_PIN, INPUT); 
    pinMode(B2_PIN, INPUT);
    pinMode(REDLED_PIN, OUTPUT);
    i2c_handle=i2c_open(I2C_BUS, LCD_ADDR);
    pinMode(LCD_RESET_PIN, OUTPUT); 
}

void
release_resources(void)
{
    if (i2c_handle!=-1)
        i2c_close(i2c_handle);
}

void
lcd_init(void)
{
    LCD_RES_HIGH;
    delay_ms(50); // 50msec delay
    LCD_RES_LOW;
    delay_ms(50);
    LCD_RES_HIGH;
    delay_ms(50);
    int i;
    char data[2];
    data[0]=0;
    for (i=0; i<8; i++)
    {
        data[1]=lcd_init_arr[i];
        i2c_write(i2c_handle, data, 2);
    }
    delay_ms(2);
}

void
lcd_clear(void)
{
    char data[2];
    data[0]=0;
    data[1]=1;
    i2c_write(i2c_handle, data, 2);
    delay_ms(2);
}

void
lcd_setpos(char ln, char idx)
{
    char data[2];
    data[0]=0;
    if (ln==0)
    {
        data[1]=0x80+idx;  // 0x00 | 0x80
    }
    else
    {
        data[1]=0xc0+idx;  // 0x40 | 0x80
    }
    i2c_write(i2c_handle, data, 2);
}

void
lcd_print(char* str)
{
    char data[2];
    data[0]=0x40;
    while(*str != '\0')
    {
        data[1]=*str++;
        i2c_write(i2c_handle, data, 2);
    }
}

void
lcd_print_line(char* str)
{
    char n = 0;
    char data[2];
    data[0]=0x40;
    while(*str != '\0')
    {
        data[1]=*str++;
        i2c_write(i2c_handle, data, 2);
        n++;
    }
    // erase rest of the line
    data[1]=' ';
    while(n<16) {
        i2c_write(i2c_handle, data, 2);
        n++;
    }
}

void
handle_menu(char button, int* resp)
{
    char pressed=0;
    char lbuf[64];

    *resp = -1;
    if ((button==0) && (mpage != oldmpage)) {
        // no button pressed, just update LCD if necessary
        LCD_LINE_1;
        lcd_print_line((char*)mpagetext[mpage]);
        oldmpage = mpage;
        LCD_LINE_2;
        if (mpage == MPAGE_STARTLOG) {
            lcd_print_line("GO    NEXT");
        } else if (mpage == MPAGE_STOPLOG) {
            LCD_LINE_1;
            sprintf(lbuf, "1=Stop %s", fname_nopath);
            if (strlen(lbuf)>16) {
                lbuf[16]='\0';
            }
            lcd_print_line(lbuf); // display filename
        } else {
            lcd_print_line("OK    NEXT");
        }
    }
    // handle request for next menu page (button 2)
    if (button==2) {
        pressed=1;
        // Next menu page requested
        mpage++;
        if (mpage>=MPAGE_TOT) {
            mpage = 0;
        }
        // check if any menu pages are not appropriate
        if (logstate==0) {
            // can't stop log if we have not started
            if (mpage==MPAGE_STOPLOG) mpage++;
        }
        if (logstate==1) {
            // can't start log if we have already started
            if (mpage==MPAGE_STARTLOG) mpage++;
            // can't change sample rate if we are already logging
            if (mpage==MPAGE_SAMPRATE) mpage++;
        }
        if (mpage>=MPAGE_TOT) {
            mpage = 0;
        }
        oldmpage = mpage;
        LCD_LINE_1;
        lcd_print_line((char*)mpagetext[mpage]);
        LCD_LINE_2;
        if (mpage == MPAGE_STARTLOG) {
            lcd_print_line("GO    NEXT");
        } else {
            lcd_print_line("OK    NEXT");
        }
    }
    // handle request for menu action (button 1)
    if (button==1) {
        pressed=1;
        *resp = mpage;
    }

    while(pressed) {
        usleep(50000);
        if ((B1_STATUS==0) && (B2_STATUS==0)) {
            pressed=0;
        }
    }

}

void
do_getsamprate(void)
{
    int sel=-1;
    char button=0;
    char pressed=0;

    LCD_LINE_1;
    lcd_print_line((char*)msrtext[msrpage]);

    LCD_LINE_2;
    lcd_print_line("OK    NEXT");

    while(sel<0)
    {
        while (button==0)
        {
            if (B1_STATUS==1) button=1;
            if (B2_STATUS==1) button=2;
        }

        // handle request for next menu page (button 2)
        if (button==2) {
            pressed=1;
            // Next menu page requested
            msrpage++;
            if (msrpage>=MSRPAGE_TOT) {
                msrpage = 0;
            }
            oldmsrpage = msrpage;
            LCD_LINE_1;
            lcd_print_line((char*)msrtext[msrpage]);
        }
        // handle request for menu action (button 1)
        if (button==1) {
            pressed=1;
            sel = msrpage;
            rate=ratearr[sel];
            printf("Selected sample rate of %f\n", rate);
            mpage=0;
        }
        // debounce
        button=0;
        while(pressed) {
            usleep(50000);
            if ((B1_STATUS==0) && (B2_STATUS==0)) {
                pressed=0;
            }
        }
    }
}

void do_getiepe(void)
{
    int sel=-1;
    char button=0;
    char pressed=0;
    char p=0;

    LCD_LINE_1;
    lcd_print_line((char*)miepetext[p]);

    LCD_LINE_2;
    lcd_print_line("OK    NEXT");

    while(sel<0)
    {
        while (button==0)
        {
            if (B1_STATUS==1) button=1;
            if (B2_STATUS==1) button=2;
        }

        // handle request for next menu page (button 2)
        if (button==2) {
            pressed=1;
            // Next menu page requested
            p++;
            if (p>=MIEPEPAGE_TOT) {
                p = 0;
            }
            LCD_LINE_1;
            lcd_print_line((char*)miepetext[p]);
        }
        // handle request for menu action (button 1)
        if (button==1) {
            pressed=1;
            sel = p;
            if (sel==0) {
                printf("Selected IEPE off\n");
                iepe_enable = 0;
            } else if (sel==1) {
                printf("Selected IEPE on\n");
                iepe_enable = 1;
            }
            mpage=0;
        }
        // debounce
        button=0;
        while(pressed) {
            usleep(50000);
            if ((B1_STATUS==0) && (B2_STATUS==0)) {
                pressed=0;
            }
        }
    }
}

void do_shutdown(void)
{
    printf("shutting down..\n");
    usleep(500000);
    system("sudo poweroff");
}

void create_fname(void)
{
    int file_count = 0;
    DIR * dirp;
    struct dirent * entry;

    dirp = opendir(MYPATH); /* There should be error handling after this */
    while ((entry = readdir(dirp)) != NULL) {
        if (entry->d_type == DT_REG) { /* If the entry is a regular file */
            file_count++;
        }
    }
    closedir(dirp);
    sprintf(fname_nopath, "%s%d.csv", FILEPREFIX, file_count);
    printf("fname is '%s'\n", fname_nopath);
}
