#ifndef _HOST_TIMER_H
#define _HOST_TIMER_H

/////////////////////////////////////////////////////////////////////////////
/**
 *  @file   HostTimer.h
 *  @author Manel Gonzalez Farrera
 *  @date   September 2015
 *  @brief  Definition of Host Timer
 *
 *  Format definition of .prog file (result of parsing "program" element in Program.csv)
 *      Each minute represented by 1 byte (1 bit per output relay channel).
 *      Size of file is 7days (1week) x 24h x 60min x 1B = 10080 bytes.
 *      Address of 2nd hour is 60min = 60d = 0x3c.
 *      Address of 2nd day is 60min x 24h = 1440d = 0x5A0.
 *      Address of past-then-end minute is 10080d = 0x2760.
 *  Manual.prog is a special case: format definition is:
 *      Size of file is 2 bytes.
 *      First byte represents output relay setpoints.
 *      Second byte represents timeout in minutes: after this time out all relays are switch ot off permanently. 
 *
 *  Hardware dependent features:
 *      Total channels : 16.
 *      Channels 1 to 8 are output of type relay.
 *      Channels 8 to 16 are generals inputs or outputs. 
 * 
 *  Guards : Conditions and Trigers:
 *      These only apply to channel output 1 to 8 of type relay.
 *      One channel is only activated if ALL conditions are satisfied.
 *      One channel is actiaved if ANY trigger is satisfied.
 *
 *  Program update/reload strategy:
 *      - HostKeeper UPDATE STEP 1: Check that the flag Program.update does not exist; if it does wait.
 *      - (DEPRECATED) UPDATE STEP 2: The TSA saves the updated program in the HostTimer under the name <ProgramName>.prog_update. 
 *      - HostKeeper UPDATE STEP 2 : Check if Program.update.tar package is found in FTP folder: The package may contain any files under the name <FileName>_update;
 *                                   these may include new .prog files as well as new Program.set, Program.channels and Program.guards always saved as *_update.
 *      - HostKeeper UPDATE STEP 3 : Check that update tar file is valid, move _update files to run folder, create Update.list file.
 *      When saving is completed:
 *      - (DEPRECATED) UPDATE STEP 3: In case that a different program name is set, then TSA updates the Program.set file accordingly in the HostTimer.
 *      - (DEPREDATED) UPDATE STEP 4: TSA raises a program update flag as an empty file named Program.update.
 *      - HostKeeper UPDATE STEP 4: Updater raises a program update flag as an empty fine named Program.update.
 *      - Every minute, after relay set points are set, the HostTimer checks if a Program.update file exists. If so continue next steps:
 *      - HostTimer closes current <ProgramName>.prog file.  
 *      - (DEPRECATED) HostTimer moves old <ProgramName>.prog to <ProgramName>prog_old.
 *      - HostTimer renames all *_update files removing ending _update.
 *      - HostTimer reads the Program.set file.
 *      - HostTimer checks if new <ProgramName>.prog_update exists and moves to <ProgramName>.prog.
 *      - HostTimer opens new <ProgramName>.prog file.
 *      - HostTimer removes/deletes flag Program.update.:
 */
/////////////////////////////////////////////////////////////////////////////
 
#include <stdio.h>
#include <iostream>
#include <fstream>
#include <unistd.h>
#include <map>
#include "CommonGlobalsWebTimer.h"
#include "IComponent.h"
#include "GpioRaspberryPi2B.h"
#include "GpioAnalogRaspberryPi2BAds1115.h"
#include "AnalogSensorNtcThermistor.h"

//#define GENERATE_EXAMPLE_OF_CHANNELS_FILE
//#define GENERATE_EXAMPLE_OF_GUARDS_FILE

#define LOGBIN(CHANNEL, MESSAGE, VARIABLE) LOGGING(CHANNEL, MESSAGE " = %d%d%d%d%d%d%d%db", VARIABLE, \
                                                                        (0x80&VARIABLE) == 0 ? 0 : 1, \
                                                                        (0x40&VARIABLE) == 0 ? 0 : 1, \
                                                                        (0x20&VARIABLE) == 0 ? 0 : 1, \
                                                                        (0x10&VARIABLE) == 0 ? 0 : 1, \
                                                                        (0x08&VARIABLE) == 0 ? 0 : 1, \
                                                                        (0x04&VARIABLE) == 0 ? 0 : 1, \
                                                                        (0x02&VARIABLE) == 0 ? 0 : 1, \
                                                                        (0x01&VARIABLE) == 0 ? 0 : 1)

#define RETRY(ACTION_STATEMENT, CHANNEL, MESSAGE, ...)           \
    unsigned int i=0;                                            \
    while (ACTION_STATEMENT) {                                   \
        LOGGING(CHANNEL, MESSAGE " retry %d", __VA_ARGS__, i++); \
        usleep(60000000);                                        \
    }

#define RETRY_ACTION(ACTION, STATEMENT, CHANNEL, MESSAGE, ...)   \
    unsigned int i=0;                                            \
    ACTION;                                                      \
    while (STATEMENT) {                                          \
        LOGGING(CHANNEL, MESSAGE " retry %d", __VA_ARGS__, i++); \
        usleep(60000000);                                        \
        ACTION;                                                  \
    }


////////////////////////////////////////////////////////////////////////////////////////////////////
// GLOBAL CONSTANTS
///////////////////////////////////////////////////////////////////////////////////////////////////

//const char          PROGRAM_SET_FILE_NAME[20] = "Program.set";         // Moved to CommonGlobalsWebTimer.h 
//const char             CHANNELS_FILE_NAME[20] = "Program.channels";    // Moved to CommonGlobalsWebTimer.h
//const char               GUARDS_FILE_NAME[20] = "Program.guards";      // Moved to CommonGlobalsWebTimer.h
const char  PROGRAM_UPDATE_FLAG_FILE_NAME[20] = "Program.update";    
const char               STATUS_FILE_NAME[20] = "HostTimer.status";    
//const unsigned int               NUM_CHANNELS = 16u;                   // Moved to CommonGlobalsWebTimer.h
const unsigned int          NUM_OUTPUT_RELAYS =  8u;
const unsigned int           NUM_DIO_CHANNELS =  8u;
const unsigned int           NUM_AIN_CHANNELS =  4u;
const unsigned int             MAX_NUM_GUARDS = 16u;
//const unsigned int PROGRAM_FILE_SIZE_IN_BYTES = 10080u;                // Moved to CommonGlobalsWebTimer.h
const unsigned int              MAX_CHAR_SIZE = 30u;
//const unsigned int  STATUS_ITEM_SIZE_IN_BYTES = 30u;


////////////////////////////////////////////////////////////////////////////////////////////////////
// CLASS DECLARATION
///////////////////////////////////////////////////////////////////////////////////////////////////

class HostTimer : public IComponent
{
  public:

    ////////////////////u
    // Public Methods //
    ////////////////////
  
    /*
     * Class constructor 
     */
    HostTimer(const char* instanceName);

    /*
     * Class destructor 
     */
    ~HostTimer();
    
    Result initialize();

    Result start();

    void shutdown(); 

    ////////////////////////////
    // Public Data Structures //
    //////////////////////////// 

    enum ChannelType_T
    {
        INPUT_DIGITAL,
        INPUT_ANALOG,
        OUTPUT_RELAY,
        OUTPUT_DIGITAL,
        OUTPUT_ANALOG,
        INPUT_NTC_THERMISTOR,
        NOT_CONNECTED        
    };
    
    enum GuardType_T
    {
        CONDITION,
        TRIGGER,
        END_OF_GUARDS
    };
    
    struct Channel_T
    {
        uint8_t id;
        char name[MAX_CHAR_SIZE];
        ChannelType_T type;
        char model[MAX_CHAR_SIZE];
        uint8_t dutyCycle;
    };
    Channel_T channels_[NUM_CHANNELS];
    
    enum Threshold_T
    {
        MAXIMUM,
        MINIMUM,
        HIGHER_THAN,
        LOWER_THAN,
        EQUAL_TO,
        UNEQUAL_TO
    };

    struct Guard_T
    {
        GuardType_T type;
        uint8_t channelId;
        uint8_t guardId;
        Threshold_T guardThreshold;
        float guardLevel;
        // Pending to implement attribute "units" : char units[10];       
    };

    friend class TimerStatus;

  private:

    /*
     * Subclass TimerStatus used to update the timer status info the status file 
     */
    class TimerStatus : public Logs
    {
    public:
        
        enum Item_T
        {
            PROGRAM_SET,
            WEEK_MINUTE,
            PROGRAM_SETPOINTS,
            DUTY_CYCLES_MASK,
            TRIGGERS_MASK,
            CONDITIONS_MASK,
            RELAY_SETPOINTS,
            INPUTS_OUTPUTS
        };

        /**
         * Formats of TimerStatus:
         *
         * Program Set:                                      Developing
         * Week Minute:                                9330 | Mon 00:00
         * Relay SetPoints [76543210]:                         00000000
         * Inputs/Outputs:    8:27.5|9:35.8|10:0.0|11:0.0|12:2.4|13:1.5
         */

        static const unsigned int ITEM_SIZE_IN_BYTES = 64;

        TimerStatus(const char* instanceName) : Logs(instanceName)
        {
            // Open status file
            LOGGING(VERBOSE, "opening status file %s...", STATUS_FILE_NAME);
            RETRY_ACTION(filePtr_.open(STATUS_FILE_NAME), !filePtr_.is_open(), ERRORS, "ERROR opening status file %s", STATUS_FILE_NAME);
        }

        ~TimerStatus()
        {
            // Close status file
            filePtr_.close();
        }

        /**
         * Updates status item in file STATUS_FILE_NAME
         * @param item of status to update
         * @param stringfo of item to be updated
         * @param longifo of item to be updated
         * @result RESULT_OK if no errors
         */
        Result updateItem(Item_T item, std::string & stringfo, long longifo = 0);

        Result updateItem(Item_T item, std::string & stringfo, Byte_T longifo, HostTimer * htPtr);

        Result updateItem(Item_T item, const std::map<uint8_t, float> & ioChannelValues);

    private:
        std::ofstream filePtr_;

        std::string prevProgramSet_  = "";
        std::string  prevWeekMinute_ = "";
        Byte_T prevProgramSetpoints_ = 0x00;
        Byte_T prevDutyCyclesMask_   = 0x00;
        Byte_T prevTriggersMask_     = 0xFF;
        Byte_T prevConditionsMask_   = 0x00;
        Byte_T prevRelaySetpoints_   = 0x00;
        float  prevIOSum_            = 0.0;
    };

    bool manualModeOn_ = false;
    long manualStartMinute_ = -1l;
    
    TimerStatus * timerStatus_;

    std::map<uint8_t, float> ioChannelValues_;

    std::string gpioName_;    

    Guard_T guards_[MAX_NUM_GUARDS];
    
    std::string programFileName_;
    FILE* programFilePtr_ = NULL;

    GpioRaspberryPi2B * gpio_;

    std::map<uint8_t, uint8_t> analogIdNumber_;
    GpioAnalogRaspberryPi2BAds1115 * gpioAnalog_;
    AnalogSensorNtcThermistor * ntcThermistor_;
    std::map< std::string, AnalogSensorNtcThermistor *> ntcThermistors_;

    /**
     * Read program set file and sets value of programFileName_
     * @return Result RESULT_OK in case of correct execution
     */
    Result readProgramSetFile();

    /**
     * Check if program update flag file exists and trigger update if so
     * @param bool reinitialize if true if update needed
     * @return Result RESULT_OK in case of correct execution
     */
    Result checkProgramUpdate(bool reinitialize);

    /**
     * Update program file
     * (See definition above.)
     * @return Result RESULT_OK in case of correct execution
     */
    Result updateProgramFile();

    #ifdef GENERATE_EXAMPLE_OF_CHANNELS_FILE
    /**
     * Generate examples of channels and guards files for DEVELOPMENT purposes
     * @return Result RESULT_OK in case of correct execution
     */
    Result generateChannelsFile();
    #endif
    #ifdef GENERATE_EXAMPLE_OF_GUARDS_FILE
    Result generateGuardsFile();
    #endif

    /**
     * Read input/output channels
     * @return Result RESULT_OK in case of correct execution
     */
    Result readInputOutputChannels();
 
    /**
     * Composes duty cycles mask based on tm_sec and channels duty cycles
     * @param Byte_T& resulted duty cycles mask
     * @return Result RESULT_OK in case of correct execution
     */
    Result composeDutyCyclesMask(Byte_T & mask);

    /**
     * Composes conditions mask based on guards_ data and status of input/outputs
     * @param Byte_T& resulted conditions mask
     * @return Result RESULT_OK in case of correct execution
     */
    Result composeConditionsMask(Byte_T & mask);

    /**
     * Composes triggers mask based on guards_ data and status of input/outputs
     * @param Byte_T& resulted triggers mask
     * @return Result RESULT_OK in case of correct execution
     */
    Result composeTriggersMask(Byte_T & mask);

    /**
     * Converts
     * @param item of status to update
     * @param info of item to be updated
     * @result RESULT_OK if no errors
     */
    Result convertToStrByte(std::string & strByte, Byte_T byte);
};

#endif // _HOST_TIMER_H
