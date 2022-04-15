/*************************************
 * dspgen - DSP Signal Generator
 * rev 1 - march 2022 - shabaz
 *************************************/

// includes
 #include <stdio.h>
 #include <string.h>
 #include <stdint.h>
 #include <stdlib.h>
 #include <argp.h>
 #include <wiringPi.h>
 #include <i2cfunc.h>

// defines
#define NUMARGS 0
#define DSP_ADDR 0x34
#define EE_ADDR 0x50
#define TWOPOW23 8388608.0
#define I2CBUS 1
#define WPGPIO 4
#define BLOCKSIZE 32

// consts
const char SIN_ADDR[] = {0x00, 0x00,    0x00, 0x01,    0x00, 0x02};
const char AMP_ADDR[] = {0x00, 0x03};

// structs
static struct argp_option options[] =
{
    {"freq", 'f', "FHERTZ", OPTION_ARG_OPTIONAL, "set frequency (Hz)"},
    {"amp", 'a', "AMPLITUDE", OPTION_ARG_OPTIONAL, "set amplitude"},
    {"prog", 'p', "PROG", OPTION_ARG_OPTIONAL, "program board"},
    {0}
};

struct arguments
{
    char *fhertz;
    char *amp;
    char *binfile;
};

// global variables
static char args_doc[] = "";
static char doc[] = "dspgen - a program to generate a signal";
int dsp_handle; // I2C handle for DSP chip

// parse_opt
static error_t
parse_opt (int key, char *arg, struct argp_state *state)
{
    struct arguments *arguments = state->input;
    switch(key)
    {
        case 'f':
            arguments->fhertz = arg;
            break;
        case 'a':
            arguments->amp = arg;
            break;
        case 'p':
            arguments->binfile = arg;
            break;
        case ARGP_KEY_ARG:
            if (state->arg_num >= NUMARGS)
	        {
	            argp_usage(state);
	        }
            //arguments->args[state->arg_num] = arg;
            break;
        case ARGP_KEY_END:
            if (state->arg_num < NUMARGS)
	        {
	            argp_usage (state);
	        }
            break;
        default:
            return ARGP_ERR_UNKNOWN;
    }
    return(0);
}

static struct argp argp = {options, parse_opt, args_doc, doc};

// 5.23 format used by DSP
// parameters: v is the decimal input, bytes is a 4-byte array
void
double_to_5_23_format(double v, char* bytes) {
    int i;
    int decscaled;
    char tb;

    decscaled = (int)( v * TWOPOW23 );
    memcpy((void*)bytes, (const void*)&decscaled, 4);
    bytes[3] &= 0x0f; // we only want 28 bits, not 32
    // flip the bytes ordering
    tb = bytes[0];
    bytes[0]=bytes[3];
    bytes[3]=tb;
    tb=bytes[1];
    bytes[1]=bytes[2];
    bytes[2]=tb;
}

void
set_freq(int f) {
    char buf[6];

    // sin_lookupAlg19401mask
    memcpy((void*)buf, (const void*)&SIN_ADDR[0], 2);
    buf[2] = 0x00;
    buf[3] = 0x00;
    buf[4] = 0x00;
    buf[5] = 0xff;
    printf("writing to address 0x%02x%02x\n", buf[0], buf[1]);
    i2c_write(dsp_handle, buf, 6);

    // sin_lookupAlg19401increment
    memcpy((void*)buf, (const void*)&SIN_ADDR[1<<1], 2);
    double_to_5_23_format( ((double)f)/24000.0, &buf[2] );
    printf("writing to address 0x%02x%02x\n", buf[0], buf[1]);
    i2c_write(dsp_handle, buf, 6);
    
    // sin_lookupAlg19401ison
    memcpy((void*)buf, (const void*)&SIN_ADDR[2<<1], 2);
    buf[2] = 0x00;
    buf[3] = 0x80;
    buf[4] = 0x00;
    buf[5] = 0x00;
    printf("writing to address 0x%02x%02x\n", buf[0], buf[1]);
    i2c_write(dsp_handle, buf, 6);

}

void
set_amp(double a) {
    char buf[6];

    // Gain1940AlgNS1
    memcpy((void*)buf, (const void*)&AMP_ADDR[0], 2);
    double_to_5_23_format( a, &buf[2] );
    printf("writing to address 0x%02x%02x\n", buf[0], buf[1]);
    i2c_write(dsp_handle, buf, 6);
}

int
ee_prog(char* fname) {
    FILE *fptr = NULL;
    char fbuf[BLOCKSIZE+2];
    size_t bytesRead = 0;
    unsigned int addr = 0x0000;
    int ee_handle;


    if ((fptr = fopen(fname, "rb")) == NULL) {
        printf("file '%s' not found!\n", fname);
        return(1);
    }

    printf("setting WP low\n");
    wiringPiSetupGpio();
    pinMode(WPGPIO, OUTPUT);
    digitalWrite(WPGPIO, 0);

    ee_handle = i2c_open(I2CBUS, EE_ADDR);

    memset(&fbuf[2], 0xff, BLOCKSIZE);
    while ((bytesRead = fread(&fbuf[2], 1, BLOCKSIZE, fptr)) > 0)
    {
        printf("writing block to addr 0x%04x\n", addr);
        fbuf[0]=(char)((addr>>8) & 0x00ff);
        fbuf[1]=(char)(addr & 0x00ff);
        i2c_write(ee_handle, fbuf, BLOCKSIZE+2);
        delay_ms(200);
        memset(fbuf, 0xff, BLOCKSIZE);
        addr = addr + BLOCKSIZE;
    }
    i2c_close(ee_handle);
    pinMode(WPGPIO, INPUT);
    return(0);
}

// ************* main program **********************
 int
 main(int argc, char **argv)
 {
    struct arguments arguments;
    int fhertz = 0;
    double amp = 0;
    char binfile[100];
    char buf[24];
    char do_freq=0;
    char do_amp=0;

    arguments.fhertz="#";
    arguments.amp="#";
    arguments.binfile="#";

    argp_parse (&argp, argc, argv, 0, 0, &arguments);


    if ((arguments.binfile) && (arguments.binfile[0]!='#')) {
        strcpy(binfile, arguments.binfile);
        if (ee_prog(binfile) == 0) {
            printf("done writing to EEPROM, exiting\n");
            exit(0);
        } else {
            printf("error, exiting\n");
            exit(1);
        }
    }    

    if ((arguments.fhertz) && (arguments.fhertz[0]!='#')) {
        sscanf(arguments.fhertz, "%d", &fhertz);
        printf("Setting frequency to %d Hz\n", fhertz);
        do_freq=1;
    }

    if ((arguments.amp) && (arguments.amp[0]!='#')) {
        sscanf(arguments.amp, "%lf", &amp);
        printf("Setting amplitude to %f\n", amp);
        do_amp=1;
    }

    dsp_handle = i2c_open(I2CBUS, DSP_ADDR);

    if (do_freq) set_freq(fhertz);
    if (do_amp) set_amp(amp);

    i2c_close(dsp_handle);

    return(0);
 }

