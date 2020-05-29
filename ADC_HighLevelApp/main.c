//
/* Copyright (c) Microsoft Corporation. All rights reserved.
   Licensed under the MIT License. */

// This sample C application for Azure Sphere demonstrates how to use ADC (Analog to Digital
// Conversion).
// The sample opens an ADC controller which is connected to a potentiometer. Adjusting the
// potentiometer will change the displayed values.
//
// It uses the API for the following Azure Sphere application libraries:
// - ADC (Analog to Digital Conversion)
// - log (messages shown in Visual Studio's Device Output window during debugging)
// - eventloop (system invokes handlers for timer events)


#include <errno.h>
#include <signal.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

//SAK added for 
//// OLED support
//// modified to add bpm support
////created Authors:
////Peter Fenn(Avnet Engineering& Technology)
////Brian Willess(Avnet Engineering& Technology)

 #include "oled.h"


// applibs_versions.h defines the API struct versions to use for applibs APIs.
#include "applibs_versions.h"
#include <applibs/adc.h>
#include <applibs/log.h>

//SAK 1-26-2020     Added LED Support
#include <applibs/gpio.h>

// By default, this sample's CMake build targets hardware that follows the MT3620
// Reference Development Board (RDB) specification, such as the MT3620 Dev Kit from
// Seeed Studios.
//
// To target different hardware, you'll need to update the CMake build. The necessary
// steps to do this vary depending on if you are building in Visual Studio, in Visual
// Studio Code or via the command line.
//
// See https://github.com/Azure/azure-sphere-samples/tree/master/Hardware for more details.
//
// This #include imports the sample_hardware abstraction from that hardware definition.
#include <hw/sample_hardware.h>

#include "eventloop_timer_utilities.h"

/// <summary>
/// Exit codes for this application. These are used for the
/// application exit code.  They they must all be between zero and 255,
/// where zero is reserved for successful termination.
/// </summary>
typedef enum {
    ExitCode_Success = 0,

    ExitCode_TermHandler_SigTerm = 1,

    ExitCode_AdcTimerHandler_Consume = 2,
    ExitCode_AdcTimerHandler_Poll = 3,

    ExitCode_Init_EventLoop = 4,
    ExitCode_Init_AdcOpen = 5,
    ExitCode_Init_GetBitCount = 6,
    ExitCode_Init_UnexpectedBitCount = 7,
    ExitCode_Init_SetRefVoltage = 8,
    ExitCode_Init_AdcPollTimer = 9,

    ExitCode_Main_EventLoopFail = 10,
    ExitCode_Init_GPIO_OpenAsOutput = 11

} ExitCode;

// File descriptors - initialized to invalid value
static int adcControllerFd = -1;

static EventLoop *eventLoop = NULL;
static EventLoopTimer *adcPollTimer = NULL;

static int fd; //SAK 1-26-2020     Added LED Support

// The size of a sample in bits
static int sampleBitCount = -1;

// The maximum voltage
static float sampleMaxVoltage = 2.5f;

// Termination state
static volatile sig_atomic_t exitCode = ExitCode_Success;

static void TerminationHandler(int signalNumber);
static void AdcPollingEventHandler(EventLoopTimer *timer);
static ExitCode InitPeripheralsAndHandlers(void);
static void CloseFdAndPrintError(int fd, const char *fdName);
static void ClosePeripheralsAndHandlers(void);

////SAK 4-29-2020 -- Added from ported code PulseSensor_timer.c

// FUNCTION PROTOTYPES (porte from code PulseSensor_timer.c)
//void getPulse(int sig_num); 

void getPulse(uint32_t sig_num);
void initPulseSensorVariables(void);

// defined in i2c.c
int initI2c(void);

// Had to create a function to get microseconds from system.
//The ported code has this as a built in function in the gcc C compiler
unsigned int micros(void);

//Jitter options defines
//#define OPT_R 10        // min uS allowed lag btw alarm and callback
//#define OPT_U 2000      // sample time uS between alarms
#define OPT_R 10  // min uS allowed lag btw alarm and callback
#define OPT_U 50// sample time uS between alarms
#define OPT_O_ELAPSED 0 // output option uS elapsed time between alarms
#define OPT_O_JITTER 1  // output option uS jitter (elapsed time - sample time)
#define OPT_O 1         // defaoult output option
#define OPT_C 10000     // number of samples to run (testing)
#define OPT_N 1         // number of Pulse Sensors (only 1 supported)

// VARIABLES USED TO DETERMINE SAMPLE JITTER & TIME OUT
volatile unsigned int eventCounter, thisTime, lastTime, elapsedTime, jitter;
//volatile  int eventCounter, thisTime, lastTime, elapsedTime, jitter;

volatile int sampleFlag = 0;
volatile int sumJitter, firstTime, secondTime, duration;
unsigned int timeOutStart, dataRequestStart, m;

// VARIABLES USED TO DETERMINE BPM
volatile int Signal;
//volatile uint32_t Signal;

volatile unsigned int sampleCounter;
volatile int threshSetting, lastBeatTime, fadeLevel;
volatile int thresh = 550;
volatile int P = 512;                               // set P default
volatile int T = 512;                               // set T default
volatile int firstBeat = 1;                      // set these to avoid noise
volatile int secondBeat = 0;                    // when we get the heartbeat back
volatile int QS = 0;
volatile int rate[10];

extern int BPM = 0;
int OldBpm = 0;

volatile int IBI = 600;                  // 600ms per beat = 100 Beats Per Minute (BPM)
volatile int Pulse = 0;
volatile int amp = 100;                  // beat amplitude 1/10 of input range.
// LED CONTROL
volatile int fadeLevel = 0;
// FILE STUFF
char filename[100];
struct tm* timenow;


/// <summary>
///     Signal handler for termination requests. This handler must be async-signal-safe.
/// </summary>
static void TerminationHandler(int signalNumber)
{
    // Don't use Log_Debug here, as it is not guaranteed to be async-signal-safe.
    exitCode = ExitCode_TermHandler_SigTerm;
}

/// <summary>
///     Handle polling timer event: takes a single reading from ADC channelId,
///     every second, outputting the result.
/// </summary>
static void AdcPollingEventHandler(EventLoopTimer *timer)
{
    if (ConsumeEventLoopTimerEvent(timer) != 0) {
        exitCode = ExitCode_AdcTimerHandler_Consume;
        return;
    }



    uint32_t value;
    int result = ADC_Poll(adcControllerFd, SAMPLE_POTENTIOMETER_ADC_CHANNEL, &value);
    if (result == -1) {
        Log_Debug("ADC_Poll failed with error: %s (%d)\n", strerror(errno), errno);
        exitCode = ExitCode_AdcTimerHandler_Poll;
        return;
    }

    //float voltage = ((float)value * sampleMaxVoltage) / (float)((1 << sampleBitCount) - 1);
    //Log_Debug("The out sample value is %.3f V\n", voltage);
    
    //SAK 4-29-2020 - added call
    //Log_Debug("The out sample value is %i \n", value);
    
    
    getPulse(value);


}

/// <summary>
///     Set up SIGTERM termination handler, initialize peripherals, and set up event handlers.
/// </summary>
/// <returns>ExitCode_Success if all resources were allocated successfully; otherwise another
/// ExitCode value which indicates the specific failure.</returns>
static ExitCode InitPeripheralsAndHandlers(void)
{
    struct sigaction action;
    memset(&action, 0, sizeof(struct sigaction));
    action.sa_handler = TerminationHandler;
    sigaction(SIGTERM, &action, NULL);

    //// Start OLED
    ////sak oled no need to do this it will happen in i2c.c in initI2c()
    //if (oled_init())
    //{
    //    Log_Debug("OLED not found!\n");
    //}
    //else
    //{
    //    Log_Debug("OLED found!\n");
    //}

    if (initI2c() == -1) {
        return -1;
    }

    // Draw AVNET logo
    //oled_draw_logo();
    //sak oled oled_i2c_bus_status(0);
    ////// OLED
    oled_state = 7;
    update_oled();

    // put a 0 on the display
    oled_state = 8;
    update_oled();

    //SAK 1-26-2020     Added LED Support
    // Change this GPIO number and the number in app_manifest.json if required by your hardware.
    //Red LED Is GPIO 8
    fd = GPIO_OpenAsOutput(8, GPIO_OutputMode_PushPull, GPIO_Value_High);
    if (fd < 0) {
        Log_Debug(
            "Error opening GPIO: %s (%d). Check that app_manifest.json includes the GPIO used.\n",
            strerror(errno), errno);
        return ExitCode_Init_GPIO_OpenAsOutput;
    }

    eventLoop = EventLoop_Create();
    if (eventLoop == NULL) {
        Log_Debug("Could not create event loop.\n");
        return ExitCode_Init_EventLoop;
    }

    adcControllerFd = ADC_Open(SAMPLE_POTENTIOMETER_ADC_CONTROLLER);
    if (adcControllerFd < 0) {
        Log_Debug("ADC_Open failed with error: %s (%d)\n", strerror(errno), errno);
        return ExitCode_Init_AdcOpen;
    }

    sampleBitCount = ADC_GetSampleBitCount(adcControllerFd, SAMPLE_POTENTIOMETER_ADC_CHANNEL);
    if (sampleBitCount == -1) {
        Log_Debug("ADC_GetSampleBitCount failed with error : %s (%d)\n", strerror(errno), errno);
        return ExitCode_Init_GetBitCount;
    }
    if (sampleBitCount == 0) {
        Log_Debug("ADC_GetSampleBitCount returned sample size of 0 bits.\n");
        return ExitCode_Init_UnexpectedBitCount;
    }

    int result = ADC_SetReferenceVoltage(adcControllerFd, SAMPLE_POTENTIOMETER_ADC_CHANNEL,
                                         sampleMaxVoltage);
    if (result < 0) {
        Log_Debug("ADC_SetReferenceVoltage failed with error : %s (%d)\n", strerror(errno), errno);
        return ExitCode_Init_SetRefVoltage;
    }

    //struct timespec adcCheckPeriod = {.tv_sec = 1, .tv_nsec = 0};
    // sak 5-21-2020 need to check on microseconds not seconds
    ///  1 microsecond is 1000 nanoseconds
    ///  1 nanosecond is 0.001 microseconds
    struct timespec adcCheckPeriod = { .tv_sec = 0, .tv_nsec = 600000};


    adcPollTimer =
        CreateEventLoopPeriodicTimer(eventLoop, &AdcPollingEventHandler, &adcCheckPeriod);
    if (adcPollTimer == NULL) {
        return ExitCode_Init_AdcPollTimer;
    }

    return ExitCode_Success;
}

/// <summary>
///     Closes a file descriptor and prints an error on failure.
/// </summary>
/// <param name="fd">File descriptor to close</param>
/// <param name="fdName">File descriptor name to use in error message</param>
static void CloseFdAndPrintError(int fd, const char *fdName)
{
    if (fd >= 0) {
        int result = close(fd);
        if (result != 0) {
            Log_Debug("ERROR: Could not close fd %s: %s (%d).\n", fdName, strerror(errno), errno);
        }
    }
}

/// <summary>
///     Close peripherals and handlers.
/// </summary>
static void ClosePeripheralsAndHandlers(void)
{
    DisposeEventLoopTimer(adcPollTimer);
    EventLoop_Close(eventLoop);

    Log_Debug("Closing file descriptors.\n");
    CloseFdAndPrintError(adcControllerFd, "ADC");
}

/// <summary>
///     Main entry point for this application.
/// </summary>
int main(int argc, char *argv[])
{
    Log_Debug("ADC application starting.\n");
    exitCode = InitPeripheralsAndHandlers();
    initPulseSensorVariables();  // initilaize Pulse Sensor beat finder

    Log_Debug("Ready to run with %d latency at %duS sample rate\n", OPT_R, OPT_U);

    // Use event loop to wait for events and trigger handlers, until an error or SIGTERM happens
    while (exitCode == ExitCode_Success) {
        /* EventLoop_Run(eventLoop, -1, true);
            RUN eventloop 
            -1 = the loop will keep running until interrupted
            True = to break the loop after the first event is processed.
        */
        EventLoop_Run_Result result = EventLoop_Run(eventLoop, -1, true);

        // Continue if interrupted by signal, e.g. due to breakpoint being set.
        if (result == EventLoop_Run_Failed && errno != EINTR) {
            exitCode = ExitCode_Main_EventLoopFail;
        }

        /*signal(SIGINT, sigHandler);*/
        //int settings = 0;
        // command line settings
        //settings = initOpts(argc, argv);
        time_t now = time(NULL);
        timenow = gmtime(&now);

        //strftime(filename, sizeof(filename),
        //    "/home/pi/Documents/PulseSensor/PULSE_DATA_%Y-%m-%d_%H:%M:%S.dat", timenow);
        //data = fopen(filename, "w+");
        //fprintf(data, "#Running with %d latency at %duS sample rate\n", OPT_R, OPT_U);
        //fprintf(data, "#sampleCount\tSignal\tBPM\tIBI\tjitter\n");

        //printf("Ready to run with %d latency at %duS sample rate\n", OPT_R, OPT_U);

        //wiringPiSetup(); //use the wiringPi pin numbers
        ////piHiPri(99);
        //mcp3004Setup(BASE, SPI_CHAN);    // setup the mcp3004 library
        //pinMode(BLINK_LED, OUTPUT); 
        
        ////SAK 1-26-2020     Added LED Support
        // Red LED OFF
        //digitalWrite(BLINK_LED, LOW);
        GPIO_SetValue(fd, GPIO_Value_High);
 
        //startTimer(OPT_R, OPT_U);   // start sampling

        //while (1)
        //{
            if (sampleFlag) {
                sampleFlag = 0;
                timeOutStart = micros();

                ////SAK 1-26-2020     Added LED Support
                // Red LED OFF/ON
                //digitalWrite(BLINK_LED, Pulse);
                //GPIO_SetValue(fd, Pulse);


                // PRINT DATA TO TERMINAL
                //printf("%lu\t%d\t%d\t%d\t%d\n",
                //    sampleCounter, Signal, BPM, IBI, jitter,
                //    );

                Log_Debug("%d\t%d\t%d\t%d\t%d\t%d\n",
                    sampleCounter, Signal, IBI, BPM, jitter, duration
                    );

                /// If BPM changes then
                //// Display BPM on OLED
                /*if ((BPM != OldBpm))*/

                if ((BPM != 0) && OldBpm != BPM)
                {

                    ////SAK 1-26-2020     Added LED Support
                    // Red LED OFF/ON
                    //digitalWrite(BLINK_LED, Pulse);
                    GPIO_SetValue(fd, GPIO_Value_Low);// ON
                    oled_state = 8;
                    update_oled();
                    OldBpm = BPM;

                    //Sleep for a blink
                    struct timespec sleepTime = { 0, 600000 };
                    nanosleep(&sleepTime, NULL);

                    //turn OFF the led
                    GPIO_SetValue(fd, GPIO_Value_High); //OFF
                }



                //// PRINT DATA TO FILE
                //fprintf(data, "%d\t%d\t%d\t%d\t%d\t%d\n",
                //    sampleCounter, Signal, IBI, BPM, jitter, duration
                //    );
            }
        //}
    }

    ClosePeripheralsAndHandlers();
    Log_Debug("Application exiting.\n");
    return exitCode;
}


// ADDED Functions for calculating samplecounter, Signal, BPM, IBI, jitter
//SAK 4-29-2020
void initPulseSensorVariables(void) {
    for (int i = 0; i < 10; ++i) {
        rate[i] = 0;
    }
    QS = 0;
    BPM = 0;
    IBI = 600;                  // 600ms per beat = 100 Beats Per Minute (BPM)
    Pulse = 0;
    sampleCounter = 0;
    lastBeatTime = 0;
    P = 512;                    // peak at 1/2 the input range of 0..1023
    T = 512;                    // trough at 1/2 the input range.
    threshSetting = 550;        // used to seed and reset the thresh variable
    thresh = 550;               // threshold a little above the trough
    amp = 100;                  // beat amplitude 1/10 of input range.
    firstBeat = 1;           // looking for the first beat
    secondBeat = 0;         // not yet looking for the second beat in a row
    lastTime = micros();
    timeOutStart = lastTime;
}

//SAK //SAK 4-29-2020 -- created fumction
//unsigned int micros(void)
//{
//    //Get the System time in micro seconds
//
//    
//    unsigned int sysTimeMS = time(NULL);
//    sysTimeMS = sysTimeMS * 600000;
//    //sysTimeMS = sysTimeMS;
//    return sysTimeMS;
//}

#include <sys/time.h>

/**
 * Returns the current time in microseconds.
 */
unsigned int micros(void) {
    struct timeval currentTime;
    gettimeofday(&currentTime, NULL);
    return currentTime.tv_sec * (int)1e6 + currentTime.tv_usec;
}

void getPulse(uint32_t sig_num) {
//void getPulse() {
    //if (sig_num == SIGALRM)
    //{
        thisTime = micros();
        //Signal = analogRead(BASE);
        //Signal = sig_num/2;
        Signal = sig_num/2;
        elapsedTime = thisTime - lastTime;

        //Log_Debug("%d\t%d\t%d\t%d\t%d\t%d\n",
        //    elapsedTime, thisTime, lastTime, BPM, jitter, duration);

        lastTime = thisTime;
        jitter = elapsedTime - OPT_U;
        sumJitter += jitter;
        sampleFlag = 1;
        sampleCounter += 2;         // keep track of the time in mS with this variable
        int N = sampleCounter - lastBeatTime;      // monitor the time since the last beat to avoid noise

      // FADE LED HERE, IF WE COULD FADE...

        //  find the peak and trough of the pulse wave
        if (Signal < thresh && N >(IBI / 5) * 3) { // avoid dichrotic noise by waiting 3/5 of last IBI
            if (Signal < T) {                        // T is the trough
                T = Signal;                            // keep track of lowest point in pulse wave
            }
        }

        if (Signal > thresh && Signal > P) {       // thresh condition helps avoid noise
            P = Signal;                              // P is the peak
        }                                          // keep track of highest point in pulse wave

        //  NOW IT'S TIME TO LOOK FOR THE HEART BEAT
        // signal surges up in value every time there is a pulse
        if (N > 250) {                             // avoid high frequency noise
            if ((Signal > thresh) && (Pulse == 0) && (N > ((IBI / 5) * 3))) {
                Pulse = 1;                             // set the Pulse flag when we think there is a pulse
                IBI = sampleCounter - lastBeatTime;    // measure time between beats in mS
                lastBeatTime = sampleCounter;          // keep track of time for next pulse

                if (secondBeat) {                      // if this is the second beat, if secondBeat == TRUE
                    secondBeat = 0;                      // clear secondBeat flag
                    for (int i = 0; i <= 9; i++) {       // seed the running total to get a realisitic BPM at startup
                        rate[i] = IBI;
                    }
                }

                if (firstBeat) {                       // if it's the first time we found a beat, if firstBeat == TRUE
                    firstBeat = 0;                       // clear firstBeat flag
                    secondBeat = 1;                      // set the second beat flag
                    // IBI value is unreliable so discard it
                    return;
                }


                // keep a running total of the last 10 IBI values
                int runningTotal = 0;                  // clear the runningTotal variable

                for (int i = 0; i <= 8; i++) {          // shift data in the rate array
                    rate[i] = rate[i + 1];                // and drop the oldest IBI value
                    runningTotal += rate[i];              // add up the 9 oldest IBI values
                }

                rate[9] = IBI;                          // add the latest IBI to the rate array
                runningTotal += rate[9];                // add the latest IBI to runningTotal
                runningTotal /= 10;                     // average the last 10 IBI values
                BPM = 60000 / runningTotal;             // how many beats can fit into a minute? that's BPM!
                QS = 1;                              // set Quantified Self flag (we detected a beat)
                //fadeLevel = MAX_FADE_LEVEL;             // If we're fading, re-light that LED.
            }
        }

        if (Signal < thresh && Pulse == 1) {  // when the values are going down, the beat is over
            Pulse = 0;                         // reset the Pulse flag so we can do it again
            amp = P - T;                           // get amplitude of the pulse wave
            thresh = amp / 2 + T;                  // set thresh at 50% of the amplitude
            P = thresh;                            // reset these for next time
            T = thresh;
        }

        if (N > 2500) {                          // if 2.5 seconds go by without a beat
            thresh = threshSetting;                // set thresh default
            P = 512;                               // set P default
            T = 512;                               // set T default
            lastBeatTime = sampleCounter;          // bring the lastBeatTime up to date
            firstBeat = 1;                      // set these to avoid noise
            secondBeat = 0;                    // when we get the heartbeat back
            QS = 0;
            BPM = 0;
            IBI = 600;                  // 600ms per beat = 100 Beats Per Minute (BPM)
            Pulse = 0;
            amp = 100;                  // beat amplitude 1/10 of input range.

        }


        duration = micros() - thisTime;


       

    //}

}