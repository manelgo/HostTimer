///////////////////////////////////////////////////////////////////////////////////////////////////
/**
 *  @file   HostTimer.cpp
 *  @author Manel Gonzalez Farrera
 *  @date   September 2015
 *  @brief  Implements Host Timer
 */
///////////////////////////////////////////////////////////////////////////////////////////////////

#include "HostTimer.h"
//#include "GpioRaspberryPi2B.h"
#include <ctime>
#include <unistd.h>
#include <stdlib.h>
#include <sstream>
#include <numeric>


///////////////////////////////////////////////////////////////////////////////////////////////////
// FORWARD DECLARATIONS
///////////////////////////////////////////////////////////////////////////////////////////////////


///////////////////////////////////////////////////////////////////////////////////////////////////
// PUBLIC METHODS
///////////////////////////////////////////////////////////////////////////////////////////////////

HostTimer::HostTimer(const char* instanceName) : IComponent(instanceName)
{
    logChannels_ = Logger::INFO;

    // Initialize digital GPIO's
    gpio_ = new GpioRaspberryPi2B("GpioRaspberryPi2B");
    ASSERT( gpio_ != nullptr );
    if ( gpio_->initialize() != RESULT_OK )
    {
        LOGMSG(ERRORS, "ERROR initializing GPIOs");
        ASSERT(0);
    }

    // Initialize Analog Inputs
    gpioAnalog_ = new GpioAnalogRaspberryPi2BAds1115("GpioAnalogRaspberryPi2BAds1115");
    ASSERT( gpioAnalog_ != nullptr );
    if ( gpioAnalog_->initialize() != RESULT_OK )
    {
        LOGMSG(ERRORS, "ERROR initializing analog inputs");
        ASSERT(0);
    }

    // Map Analog Channels
    {
        analogIdNumber_[16] = 0u; 
        analogIdNumber_[17] = 1u; 
        analogIdNumber_[18] = 2u; 
        analogIdNumber_[19] = 3u; 
    }

    timerStatus_ = new TimerStatus("HostTimerStatus");

    // PROGRAM FILE UPDATE
    // Check if program update flag file exists and execute update
    if ( checkProgramUpdate(false) != RESULT_OK )
    {
        LOGMSG(ERRORS, "ERROR checking program update");
        ASSERT(0);
    }
}

HostTimer::~HostTimer()
{
    // Close program file
    fclose(programFilePtr_);

    delete timerStatus_;
}

Result HostTimer::initialize()
{
    // Read program set file (set prograFileName_ class attribute)
    if ( readProgramSetFile() != RESULT_OK )
    {
        LOGGING(ERRORS, "ERROR reading program set file %s", PROGRAM_SET_FILE_NAME);    
        ASSERT(0);
    }

    // Open program file
    LOGGING(INFO, "opening program file %s...", programFileName_.c_str());
    if ( (programFilePtr_ = fopen(programFileName_.c_str(), "r")) == NULL )
    {
        LOGGING(ERRORS, "ERROR opening program file %s, waiting for program update...", programFileName_.c_str());
        // Continous check whether program update flag exists
        std::ifstream filePtr;
        RETRY_ACTION(filePtr.open(PROGRAM_UPDATE_FLAG_FILE_NAME), !filePtr.is_open(),
                     ERRORS, "ERROR opening program update flag %s", PROGRAM_UPDATE_FLAG_FILE_NAME);
        if ( updateProgramFile() != RESULT_OK )
        {
            LOGMSG(ERRORS, "ERROR updating program file");
            ASSERT(0);
        }
    }
 
#ifdef GENERATE_EXAMPLE_OF_CHANNELS_FILE
    LOGGING(VERBOSE, "gRaspberryPi2BAds1115enerating example of channels file %s...", CHANNELS_FILE_NAME);
    if ( generateChannelsFile() != RESULT_OK )
    {
        LOGGING(ERRORS, "ERROR generating file %s", CHANNELS_FILE_NAME);
        return RESULT_ERROR;        
    }
#endif
    
    // Open and read channels file into array channels_
    {
        //std::ifstream channelsFilePtr;
        FILE * channelsFilePtr; 
        LOGGING(INFO, "reading file %s...", CHANNELS_FILE_NAME);
        if ( (channelsFilePtr = fopen(CHANNELS_FILE_NAME, "r")) == NULL )
        {
            LOGGING(ERRORS, "ERROR opening file %s ", CHANNELS_FILE_NAME);
            ASSERT(0);        
        }
        //for ( unsigned int i=0; !channelsFilePtr.eof(); i++ )
        for ( unsigned int i=0; i<=NUM_CHANNELS; i++ )
        {
            fread( &channels_[i], sizeof(Channel_T), 1, channelsFilePtr );         
            if ( !feof(channelsFilePtr) )
            {
                if ( i < NUM_CHANNELS )
                {
                    LOGGING(INFO, "channel id:%02d name:%s type:%d model:%s dutyCycle:%d", channels_[i].id, channels_[i].name, channels_[i].type, channels_[i].model, channels_[i].dutyCycle);
                }
                else // ( i == NUM_CHANNELS)
                {
                    LOGGING(ERRORS, "WARNING more than %d channels found in file %s are discarded", NUM_CHANNELS, CHANNELS_FILE_NAME);
                    break;
                }
            }
            else if ( i < NUM_CHANNELS - 1 )
            {
                LOGGING(ERRORS, "ERROR missing definition of channels in file %s; only %d channels defined", CHANNELS_FILE_NAME, i+1);
                ASSERT(0);                
            }
            else // ( i == NUM_CHANNELS - 1 )
            {
                LOGGING(VERBOSE, "total number of channels read is %d", i);
                break;
            }

            // Check type of channels
            if ( i < NUM_OUTPUT_RELAYS && channels_[i].type != OUTPUT_RELAY )
            {
                LOGGING(ERRORS, "ERROR channel %d must be of type output relay", i);
                ASSERT(0);                
            }
            if ( i >= NUM_OUTPUT_RELAYS && i < (NUM_OUTPUT_RELAYS + NUM_DIO_CHANNELS)
                 && channels_[i].type != INPUT_DIGITAL && channels_[i].type != OUTPUT_DIGITAL )
            {
                LOGGING(ERRORS, "ERROR channel %d must be of type either input digital or output digital", i);
                ASSERT(0);                                
            }
            if ( i >= (NUM_OUTPUT_RELAYS + NUM_DIO_CHANNELS) && i < (NUM_OUTPUT_RELAYS + NUM_DIO_CHANNELS + NUM_AIN_CHANNELS)
                 && channels_[i].type != INPUT_ANALOG && channels_[i].type != INPUT_NTC_THERMISTOR )
            {
                LOGGING(ERRORS, "ERROR channel %d must be of type either input analog or NTC thermistor", i);
                ASSERT(0);                                
            }
        }
        //channelsFilePtr.close();
        fclose(channelsFilePtr);
    }
    
    // Configure GPIO's and Analog Channels
    {
        // Clear NTC Thermistors Map
        ntcThermistors_.clear();

        for (unsigned int i=0; i<NUM_CHANNELS; i++ )
        {
            switch (channels_[i].type)
            {
            case INPUT_DIGITAL:
                if ( std::string(channels_[i].model) == "N.O." )
                {
                    if ( gpio_->setMode(i, IGpio::INPUT_DIGITAL_INVERTED) != RESULT_OK )
                    {
                        LOGGING(ERRORS, "ERROR setting mode input digital in GPIO id %d", i);
                        return RESULT_ERROR;
                    }
                }
                else if ( std::string(channels_[i].model) == "N.C." )
                {
                    if ( gpio_->setMode(i, IGpio::INPUT_DIGITAL) != RESULT_OK )
                    {
                        LOGGING(ERRORS, "ERROR setting mode input digital inverted in GPIO id %d", i);
                        return RESULT_ERROR;
                    }
                }
                else
                {
                    LOGGING(ERRORS, "ERROR: unexpected input digital model %s found", channels_[i].model);
                    return RESULT_ERROR;
                }
                break;
            case OUTPUT_DIGITAL:
                if ( std::string(channels_[i].model) == "N.O." )
                {
                    if ( gpio_->setMode(i, IGpio::OUTPUT_DIGITAL) != RESULT_OK)
                    {
                        LOGGING(ERRORS, "ERROR setting mode output digital in GPIO id %d", i);
                        return RESULT_ERROR;
                    }
                }
                else if ( std::string(channels_[i].model) == "N.C." )
                {
                    if ( gpio_->setMode(i, IGpio::OUTPUT_DIGITAL_INVERTED) != RESULT_OK)
                    {
                        LOGGING(ERRORS, "ERROR setting mode output digital inverted in GPIO id %d", i);
                        return RESULT_ERROR;
                    }
                }
                else
                {
                    LOGGING(ERRORS, "ERROR: unexpected output digital model %s found", channels_[i].model);
                    return RESULT_ERROR;
                }
                break;
            case OUTPUT_RELAY:
                if ( gpio_->setMode(i, IGpio::OUTPUT_DIGITAL) != RESULT_OK)
                {
                    LOGGING(ERRORS, "ERROR setting mode output digital in GPIO id %d", i);
                    return RESULT_ERROR;
                }
                break;
            case INPUT_ANALOG:
                break;
            case INPUT_NTC_THERMISTOR:
                if ( ntcThermistors_.find( channels_[i].model ) == ntcThermistors_.end() )
                {
                    LOGGING(VERBOSE, "contructing new AnalogSensorNtcThermistor model %s", channels_[i].model);
                    ntcThermistors_[channels_[i].model] = new AnalogSensorNtcThermistor("NtcThermistor", channels_[i].model);
                    std::string ntcThermistorName = std::string("NtcThermistor") + channels_[i].model;
                    LOGGING(VERBOSE, "setting interface IGpio for instance:%s", ntcThermistorName.c_str());
                    ntcThermistors_[channels_[i].model]->setInterface(ntcThermistorName.c_str(), "IGpio", static_cast<IGpio *>(gpioAnalog_));
                    LOGGING(VERBOSE, "initializing channel %d", i);
                    ntcThermistors_[channels_[i].model]->initialize();
                }
                break;
            }
            usleep(200000);
        }        
    }

#ifdef GENERATE_EXAMPLE_OF_GUARDS_FILE
    LOGGING(VERBOSE, "generating example of guards file %s...", GUARDS_FILE_NAME);
    if ( generateGuardsFile() != RESULT_OK )
    {
        LOGGING(ERRORS, "ERROR generating file %s", GUARDS_FILE_NAME);
        return RESULT_ERROR;        
    }
#endif

    // Open and read guards file into array guards_
    {
        FILE * guardsFilePtr;
        LOGGING(VERBOSE, "reading file %s...", GUARDS_FILE_NAME);
        if ( (guardsFilePtr = fopen(GUARDS_FILE_NAME, "r")) == NULL )
        {
            LOGGING(ERRORS, "WARNING opening file %s", GUARDS_FILE_NAME);
        }
        else
        {
            //for ( unsigned int i=0; !guardsFilePtr.eof(); i++ )
            for ( unsigned int i=0; i<=MAX_NUM_GUARDS; i++ )
            {
                fread( &guards_[i], sizeof(Guard_T), 1, guardsFilePtr );         

                // Check if end of file is reached and set flag in next record if needed
                if ( !feof(guardsFilePtr) ) 
                {
                    if ( i < MAX_NUM_GUARDS )
                    {
                        LOGGING(INFO, "guard %i is type:%d channelId:%02d, guardId:%d guardThreshold:%d, guardLevel:%.1f",
                                i+1, guards_[i].type, guards_[i].channelId, guards_[i].guardId, guards_[i].guardThreshold, guards_[i].guardLevel);
                    }
                    else
                    {
                        LOGGING(ERRORS, "WARNING more than %d guards found in file %s are discarded", MAX_NUM_GUARDS, GUARDS_FILE_NAME);
                        break;
                    }
                }
                else // end of file is reached
                {
                    guards_[i].type = END_OF_GUARDS;
                    {
                        if ( i > 0 ) LOGGING(VERBOSE, "total number of guards found is %i", i);
                        else         LOGMSG (ERRORS, "WARNING no guards defined");
                    }
                    break; 
                }
            }    
            fclose(guardsFilePtr);
        }
    }
    
    return RESULT_OK;
}

Result HostTimer::start()
{
    // Initialization of variables requiring persistence interloops.
    Byte_T manualTimeout, programSetpoints = 0x00;
    long prevWeekMinute;

    // Main execution loop
    while (1)
    {
        // PROGRAM FILE UPDATE
        //- Check if program update flag file exists and execute update
        if ( checkProgramUpdate(true) != RESULT_OK )
        {
            LOGMSG(ERRORS, "ERROR checking program update");
        }

        // NORMAL PROGRAM EXECUTION
        Byte_T dutyCyclesMask = 0xFF, conditionsMask = 0xFF, triggersMask = 0x00, relaySetpoints = 0x00;
        // Calculate week minute
        time_t now = time(0);
        tm * tmTime = localtime(&now);
        long weekMinute = (tmTime->tm_wday==0 ? 6 : tmTime->tm_wday - 1)*24*60 + tmTime->tm_hour*60 + tmTime->tm_min;
        if ( weekMinute != prevWeekMinute )
        {
            LOGGING(INFO, "weekMinute: address 0x%x = %d corresponding to weekDay:%d, hour:%d, minute:%d",
                          weekMinute, weekMinute, (tmTime->tm_wday==0 ? 6 : tmTime->tm_wday), tmTime->tm_hour, tmTime->tm_min);
            prevWeekMinute = weekMinute;
        }

        // Update week minute in status file
        std::string stringfo = "";
        if ( ( timerStatus_->updateItem(TimerStatus::WEEK_MINUTE, stringfo, weekMinute)) != RESULT_OK )
        {
            LOGGING(ERRORS, "ERROR updating week minute status item %d with value %d", TimerStatus::WEEK_MINUTE, weekMinute);
            ASSERT(0);
        }

	    // Compose duty cycles mask
		if ( composeDutyCyclesMask(dutyCyclesMask) != RESULT_OK )
		{
			LOGMSG(ERRORS, "ERROR composing duty cycles mask");
			return RESULT_ERROR;
		}

        // Read input/output channels
        if ( readInputOutputChannels() != RESULT_OK )
        {
            LOGMSG(ERRORS, "ERROR reading input/output channels");
            return RESULT_ERROR; 
        }
        if ( ( timerStatus_->updateItem(TimerStatus::INPUTS_OUTPUTS, ioChannelValues_)) != RESULT_OK )
        {
            LOGMSG(ERRORS, "ERROR updating status of inputs/outputs");
        }
 
        if ( ( programFileName_ == "MANUAL.prog" ) )
        {
            if ( manualModeOn_ )
            {
				if ( manualStartMinute_ == -1l )
				{
					LOGMSG(VERBOSE, "starting of manual program");

					// Set manual start minute the first loop
					manualStartMinute_ = weekMinute;

					// Read manual set point from Manual.prog file
					if ( (fseek(programFilePtr_, 0l, SEEK_SET)) != 0 )
					{
						LOGMSG(ERRORS, "ERROR seeking program file pointer in position 0l");
						ASSERT(0);
					}
					if ( (fread( &programSetpoints, sizeof(Byte_T), 1, programFilePtr_)) != 1 )
					{
						LOGMSG(ERRORS, "ERROR reading program set points in position 0l");
						ASSERT(0);
					}
					LOGBIN(VERBOSE, "program set points: 0x%02x", programSetpoints);

					// Read manual time out from Manual.prog file
					if ( (fread( &manualTimeout, sizeof(Byte_T), 1, programFilePtr_)) != 1 )
					{
						LOGMSG(ERRORS, "ERROR reading time out in next position");
						ASSERT(0);
					}
					LOGGING(VERBOSE, "manual timeout is %d minutes", manualTimeout);
				}
				else
				{
					// Check manual time out
					bool overrun = ( weekMinute < manualStartMinute_ );
					if
					(
						( !overrun && ( (weekMinute - manualStartMinute_) >= manualTimeout ) )
						||
						( overrun && ( ( ( 7*24*60 - 1) - manualStartMinute_ + weekMinute  ) >= manualTimeout ) )
					)
					{
						programSetpoints = 0x00;
						manualStartMinute_ = -1l;
						manualModeOn_ = false;

						LOGMSG(VERBOSE, "ending of manual program");
					}
				}

				// Calculate masked relay set points
				relaySetpoints = programSetpoints & dutyCyclesMask;
				LOGBIN(VERBOSE, "relays set points: 0x%02x", relaySetpoints);
            }
        }
        else // This is the regular program operation
        {

			// Read relay set points from program file
			if ( (fseek(programFilePtr_, weekMinute, SEEK_SET)) != 0 )
			{
				LOGGING(ERRORS, "ERROR seeking program file pointer in position %d", weekMinute);
				ASSERT(0);
			}
			if ( (fread( &programSetpoints, sizeof(Byte_T), 1, programFilePtr_)) != 1 )
			{
				LOGGING(ERRORS, "ERROR reading program set points in position %d", weekMinute);
				ASSERT(0);
			}
			LOGBIN(VERBOSE, "program set points: 0x%02x", programSetpoints);

			// Compose conditions mask
			LOGMSG(VERBOSE, "composing conditions mask...");
			if ( composeConditionsMask(conditionsMask) != RESULT_OK )
			{
				LOGMSG(ERRORS, "ERROR composing conditions mask");
				return RESULT_ERROR;
			}
			LOGBIN(VERBOSE, "conditions mask: 0x%02x", conditionsMask);

			// Compose triggers mask
			LOGMSG(VERBOSE, "composing triggers mask...");
			if ( composeTriggersMask(triggersMask) != RESULT_OK )
			{
				LOGMSG(ERRORS, "ERROR composing triggers mask");
				return RESULT_ERROR;
			}
			LOGBIN(VERBOSE, "triggers mask: 0x%02x", triggersMask);

			// Calculate masked relay set points
			relaySetpoints = ( triggersMask | ( conditionsMask & programSetpoints ) ) & dutyCyclesMask;
			LOGBIN(VERBOSE, "relays set points: 0x%02x", relaySetpoints);
        }     

        // Set relay set points
        LOGMSG(VERBOSE, "setting level to relay channels..."); 
        for (uint8_t i=0; i < NUM_OUTPUT_RELAYS; i++)
        {
            signed int level = static_cast<signed int>( ( (0x01 << i) & relaySetpoints ) >> i );
            if( gpio_->setLevel(i, level) != RESULT_OK )
            {
                LOGGING(ERRORS, "ERROR setting level %d in channel id %d", level, i);
                return RESULT_ERROR;
            }
        }
        // Update setpoints and masks in status file
        if ( ( timerStatus_->updateItem(TimerStatus::PROGRAM_SETPOINTS, stringfo, programSetpoints, this) ) != RESULT_OK )
        {
            LOGGING(ERRORS, "ERROR updating programSetpoints status item %d with value %d", TimerStatus::PROGRAM_SETPOINTS, programSetpoints);
            ASSERT(0);
        }
        if ( ( timerStatus_->updateItem(TimerStatus::DUTY_CYCLES_MASK, stringfo, dutyCyclesMask, this) ) != RESULT_OK )
        {
            LOGGING(ERRORS, "ERROR updating dutyCyclesMask status item %d with value %d", TimerStatus::DUTY_CYCLES_MASK, dutyCyclesMask);
            ASSERT(0);
        }
        if ( ( timerStatus_->updateItem(TimerStatus::TRIGGERS_MASK, stringfo, triggersMask, this) ) != RESULT_OK )
        {
            LOGGING(ERRORS, "ERROR updating triggersMask status item %d with value %d", TimerStatus::TRIGGERS_MASK, triggersMask);
            ASSERT(0);
        }
        if ( ( timerStatus_->updateItem(TimerStatus::CONDITIONS_MASK, stringfo, conditionsMask, this) ) != RESULT_OK )
        {
            LOGGING(ERRORS, "ERROR updating conditionsMask status item %d with value %d", TimerStatus::CONDITIONS_MASK, conditionsMask);
            ASSERT(0);
        }
        if ( ( timerStatus_->updateItem(TimerStatus::RELAY_SETPOINTS, stringfo, relaySetpoints, this) ) != RESULT_OK )
        {
            LOGGING(ERRORS, "ERROR updating relaySetpoints status item %d with value %d", TimerStatus::RELAY_SETPOINTS, relaySetpoints);
            ASSERT(0);
        }

        // Wait 1 second for next iteration
        usleep(1000000);
        LOGRESS(INFO, "waiting for next minute", ".");
    }
    
    return RESULT_OK;
}

void HostTimer::shutdown()
{
    LOGMSG(VERBOSE, "executing...");
    for (uint8_t i=0; i < NUM_OUTPUT_RELAYS; i++)
    {
        gpio_->setLevel(i, 0);
    }
}


///////////////////////////////////////////////////////////////////////////////////////////////////
// PRIVATE METHODS
///////////////////////////////////////////////////////////////////////////////////////////////////

Result HostTimer::readProgramSetFile()
{
    std::ifstream programSetFilePtr;
    std::string programName;
    LOGGING(VERBOSE, "reading file %s...", PROGRAM_SET_FILE_NAME);
    RETRY_ACTION(programSetFilePtr.open(PROGRAM_SET_FILE_NAME), !programSetFilePtr.is_open(), ERRORS, "ERROR opening program set file %s", PROGRAM_SET_FILE_NAME);    
    programSetFilePtr >> programName;
    programFileName_ = programName + ".prog";
    LOGGING(VERBOSE, "set program file name is %s ", programFileName_.c_str());
    programSetFilePtr.close();

    //Result result = updateStatusItem(PROGRAM_SET, programName);    
    if ( ( timerStatus_->updateItem(TimerStatus::PROGRAM_SET, programName) ) != RESULT_OK )
    {
        LOGGING(ERRORS, "ERROR updating program set status item %d with value %s", TimerStatus::PROGRAM_SET, programName.c_str());
        return RESULT_ERROR;
    }

    return RESULT_OK;
}

Result HostTimer::checkProgramUpdate(bool reinitialize)
{
	Result result = RESULT_OK;

    // Check if program update flag file exists and execute update
    std::ifstream filePtr(PROGRAM_UPDATE_FLAG_FILE_NAME);
    if ( filePtr.good() )
    {
        filePtr.close();
        LOGGING(INFO, "program update flag %s found", PROGRAM_UPDATE_FLAG_FILE_NAME);
        Result result = updateProgramFile();
        if ( result != RESULT_OK )
        {
            LOGGING(ERRORS, "ERROR updating program file with result %d", result);
            return result;
        }

        // Initialize HostTimer
        if (reinitialize)
        {
        	result = initialize();
            if ( result != RESULT_OK )
            {
                LOGGING(ERRORS, "ERROR initializing with result %d", result);
                return result;
            }

            // Initialize in case of manual program
            if ( programFileName_ == "MANUAL.prog" ) manualModeOn_ = true;
        }
    }
    
    return RESULT_OK;
}

Result HostTimer::updateProgramFile()
{
    char linuxCommand[50];

    // Close current program file if exists
    if ( programFilePtr_ != NULL )
    {
        LOGGING(INFO, "closing program file %s if exists...", programFileName_.c_str());
        fclose(programFilePtr_);
    }

    // Rename all _update files removing _update
    std::ifstream updateListFilePtr("Update.list");
    if ( !updateListFilePtr.is_open() )
    {
        LOGMSG(ERRORS, "ERROR opening Update.list file, aborting update");
        return RESULT_ERROR;
    }
    else
    {
        std::string fileToUpdate;
        updateListFilePtr >> fileToUpdate;
        while ( !updateListFilePtr.eof() )
        {
            std::string updatedFile = fileToUpdate.substr(0, fileToUpdate.find("_update"));
            sprintf(linuxCommand, "mv -f %s %s", fileToUpdate.c_str(), updatedFile.c_str());
            LOGGING(INFO, "executing Linux command '%s'...", linuxCommand);
            system(linuxCommand);
            updateListFilePtr >> fileToUpdate; 
        }
        updateListFilePtr.close();
        system("rm -f Update.list");
    }

    // Remove program update flag
    sprintf(linuxCommand, "rm -f %s", PROGRAM_UPDATE_FLAG_FILE_NAME);
    LOGGING(INFO, "executing Linux command '%s'...", linuxCommand);
    system(linuxCommand);

    return RESULT_OK;
}

#ifdef GENERATE_EXAMPLE_OF_CHANNELS_FILE
Result HostTimer::generateChannelsFile()
{
    FILE * channelsFilePtr;
    if ( (channelsFilePtr = fopen(CHANNELS_FILE_NAME, "w")) == NULL )
    {
        LOGGING(ERRORS, "ERROR opening file %s", CHANNELS_FILE_NAME);
        return RESULT_ERROR;
    }
    Channel_T channel;
    for (unsigned int i=0; i < 4; i++)
    {
        channel.id = i;
        sprintf(channel.name, "WaterValve%d\0", i + 1);
        channel.type = OUTPUT_RELAY;
        fwrite(&channel, sizeof(channel), 1, channelsFilePtr);
    }
    {  // Other output relay channels
        channel.id = 4; sprintf(channel.name, "CentralHeating\0"); channel.type = OUTPUT_RELAY;
        fwrite(&channel, sizeof(channel), 1, channelsFilePtr);
        channel.id = 5; sprintf(channel.name, "TerraceWaterValve\0"); channel.type = OUTPUT_RELAY;
        fwrite(&channel, sizeof(channel), 1, channelsFilePtr);
        channel.id = 6; sprintf(channel.name, "TerraceSprayValve\0"); channel.type = OUTPUT_RELAY;
        fwrite(&channel, sizeof(channel), 1, channelsFilePtr);
        channel.id = 7; sprintf(channel.name, "Alarm\0"); channel.type = OUTPUT_RELAY;
        fwrite(&channel, sizeof(channel), 1, channelsFilePtr);
    }
    {  // Other general channels
        channel.id = 8; sprintf(channel.name, "TemperatureInHouse\0"); channel.type = INPUT_ANALOG;
        fwrite(&channel, sizeof(channel), 1, channelsFilePtr);
        channel.id = 9; sprintf(channel.name, "HumidityInHouse\0"); channel.type = INPUT_ANALOG;
        fwrite(&channel, sizeof(channel), 1, channelsFilePtr);
        channel.id =10; sprintf(channel.name, "TemperatureExterior\0"); channel.type = INPUT_ANALOG;
        fwrite(&channel, sizeof(channel), 1, channelsFilePtr);
        channel.id =11; sprintf(channel.name, "RainSensor\0"); channel.type = INPUT_DIGITAL;
        fwrite(&channel, sizeof(channel), 1, channelsFilePtr);
        channel.id =12; sprintf(channel.name, "channel-12\0"); channel.type = NOT_CONNECTED;
        fwrite(&channel, sizeof(channel), 1, channelsFilePtr);
        channel.id =13; sprintf(channel.name, "channel-13\0"); channel.type = NOT_CONNECTED;
        fwrite(&channel, sizeof(channel), 1, channelsFilePtr);
        channel.id =14; sprintf(channel.name, "channel-14\0"); channel.type = NOT_CONNECTED;
        fwrite(&channel, sizeof(channel), 1, channelsFilePtr);
        channel.id =15; sprintf(channel.name, "channel-15\0"); channel.type = NOT_CONNECTED;
        fwrite(&channel, sizeof(channel), 1, channelsFilePtr);
    }
    fclose(channelsFilePtr);    

    return RESULT_OK;
}
#endif

#ifdef GENERATE_EXAMPLE_OF_GUARDS_FILE
Result HostTimer::generateGuardsFile()
{
    FILE * guardsFilePtr;
    if ( (guardsFilePtr = fopen(GUARDS_FILE_NAME, "w")) == NULL )
    {
        LOGGING(ERRORS, "ERROR opening file %s", GUARDS_FILE_NAME);
        return RESULT_ERROR;        
    }
    Guard_T guard;
    for (unsigned int i=0; i < 4; i++)
    {
        guard.type = CONDITION;
        guard.channelId = i;
        guard.guardId = 11;
        guard.guardThreshold = MAXIMUM;
        guard.guardLevel = 0.0;
        fwrite(&guard, sizeof(guard), 1, guardsFilePtr);
    }
    {   // Other trigger guards
        guard.type = TRIGGER; guard.channelId = 4; guard.guardId = 8; guard.guardThreshold = MINIMUM; guard.guardLevel = 5;
        fwrite(&guard, sizeof(guard), 1, guardsFilePtr);
        guard.type = TRIGGER; guard.channelId = 6; guard.guardId = 10; guard.guardThreshold = MAXIMUM; guard.guardLevel = 30;
        fwrite(&guard, sizeof(guard), 1, guardsFilePtr);
    } 
    fclose(guardsFilePtr);    

    return RESULT_OK;
}
#endif

Result HostTimer::readInputOutputChannels()
{
    for (auto channel : channels_)
    {
        Result result = RESULT_OK;
        signed int level = 0;
        float floatBuffer = 0.0;

        switch ( channel.type )
        {
        case INPUT_DIGITAL:
        case OUTPUT_DIGITAL:
            result = gpio_->getLevel(channel.id, level);
            if ( result != RESULT_OK )
            {
                LOGGING(ERRORS, "ERROR getting level of Gpio ID %d with result %d", channel.id, result);
                return result;
            }
            ioChannelValues_[channel.id] = static_cast<float>(level);
            break;
        case INPUT_ANALOG:
            if ( analogIdNumber_.find(channel.id) == analogIdNumber_.end() )
            {
                LOGGING(ERRORS, "ERROR: channel id %d not found in analogIdNumber_ map", channel.id);
                return RESULT_ERROR;
            }
            result = gpioAnalog_->getVoltage(analogIdNumber_[channel.id], floatBuffer);
            if ( result != RESULT_OK )
            {
                LOGGING(ERRORS, "ERROR gettig voltage of gpio analog id %d", analogIdNumber_[channel.id]);
                return result;
            }
            ioChannelValues_[channel.id] = floatBuffer;
            break;
        case INPUT_NTC_THERMISTOR:
            if ( analogIdNumber_.find(channel.id) == analogIdNumber_.end() )
            {
                LOGGING(ERRORS, "ERROR: channel id %d not found in analogIdNumber_ map", channel.id);
                return RESULT_ERROR;
            }
            if ( ntcThermistors_.find(channel.model) == ntcThermistors_.end() )
            {
                LOGGING(ERRORS, "ERROR: NTC model %s not found in ntcThermistors_ map", channel.model);
                return RESULT_ERROR;
            }
            result = ntcThermistors_[channel.model]->readValue(analogIdNumber_[channel.id], floatBuffer);
            if ( result != RESULT_OK )
            {
                LOGGING(ERRORS, "ERROR reading value of NTC Thermistor ID %d with result %d", analogIdNumber_[channel.id], result);
                return result;
            }
            ioChannelValues_[channel.id] =  floatBuffer;
            break;
        default: continue;
        }
        LOGGING(VERBOSE, "channel id:%d name:%s type:%d model:%s value:%.1f", channel.id, channel.name, channel.type, channel.model, ioChannelValues_[channel.id]);
    }

    return RESULT_OK;
}

Result HostTimer::composeDutyCyclesMask(Byte_T & dutyCyclesMask)
{
    dutyCyclesMask = 0xFF;

    time_t now = time(0);
    tm * tmTime = localtime(&now);

    for (unsigned int i=0; i < NUM_OUTPUT_RELAYS; i++ )
    {

        if ( tmTime->tm_sec >= channels_[i].dutyCycle )
	    {
	        dutyCyclesMask &= ( 0xFF ^ ( 0x01 << channels_[i].id ) );

            LOGGING(VERBOSE, "updating mask current second:%d dutyCycle:%d", tmTime->tm_sec, channels_[i].dutyCycle);
	    }
    }

    return RESULT_OK;
}

Result HostTimer::composeConditionsMask(Byte_T & conditionsMask)
{
    conditionsMask = 0xFF;
    
    for (unsigned int i=0; i < sizeof(guards_)/sizeof(Guard_T); i++ )
    {
        if ( guards_[i].type == END_OF_GUARDS) break;
        if ( guards_[i].type == CONDITION )
        {
        	float signalValue = ioChannelValues_[guards_[i].guardId];
            LOGGING(VERBOSE, "value of guardId:%02d is %.1f", guards_[i].guardId, signalValue);

            switch (guards_[i].guardThreshold)
            {
                case MAXIMUM:
                    if ( signalValue > guards_[i].guardLevel ) conditionsMask &= ( 0xFF ^ ( 0x01 << guards_[i].channelId ) );
                    break;
                case MINIMUM:
                    if ( signalValue < guards_[i].guardLevel ) conditionsMask &= ( 0xFF ^ ( 0x01 << guards_[i].channelId ) );
                    break;
                case HIGHER_THAN:
                    if ( signalValue <= guards_[i].guardLevel ) conditionsMask &= ( 0xFF ^ ( 0x01 << guards_[i].channelId ) );
                    break;
                case LOWER_THAN:
                    if ( signalValue >= guards_[i].guardLevel ) conditionsMask &= ( 0xFF ^ ( 0x01 << guards_[i].channelId ) );
                    break;
                case EQUAL_TO:
                    if ( signalValue != guards_[i].guardLevel ) conditionsMask &= ( 0xFF ^ ( 0x01 << guards_[i].channelId ) );
                    break;
                case UNEQUAL_TO:
                    if ( signalValue == guards_[i].guardLevel ) conditionsMask &= ( 0xFF ^ ( 0x01 << guards_[i].channelId ) );
                    break;
                default:
                    ASSERT(0);
                    break;
            }
        }
    }
    return RESULT_OK;
}

Result HostTimer::composeTriggersMask(Byte_T & triggersMask)
{
    triggersMask = 0x00;
    
    for (unsigned int i=0; i < sizeof(guards_)/sizeof(Guard_T); i++ )
    {
        if ( guards_[i].type == END_OF_GUARDS) break;
        if ( guards_[i].type == TRIGGER )
        {
        	float signalValue = ioChannelValues_[guards_[i].guardId];
            LOGGING(VERBOSE, "value of guardId:%02d is %.1f", guards_[i].guardId, signalValue);

            switch (guards_[i].guardThreshold)
            {
                case MAXIMUM:
                    if ( signalValue <= guards_[i].guardLevel ) triggersMask |= ( 0x01 << guards_[i].channelId );
                    break;
                case MINIMUM:
                    if ( signalValue >= guards_[i].guardLevel ) triggersMask |= ( 0x01 << guards_[i].channelId );
                    break;
                case HIGHER_THAN:
                    if ( signalValue > guards_[i].guardLevel ) triggersMask |= ( 0x01 << guards_[i].channelId );
                    break;
                case LOWER_THAN:
                    if ( signalValue < guards_[i].guardLevel ) triggersMask |= ( 0x01 << guards_[i].channelId );
                    break;
                case EQUAL_TO:
                    if ( signalValue == guards_[i].guardLevel ) triggersMask |= ( 0x01 << guards_[i].channelId );
                    break;
                case UNEQUAL_TO:
                    if ( signalValue != guards_[i].guardLevel ) triggersMask |= ( 0x01 << guards_[i].channelId );
                    break;
                default:
                    ASSERT(0);
                    break;
            }
        }
    }
    return RESULT_OK;
}

Result HostTimer::convertToStrByte(std::string & strByte, Byte_T byte)
{
    char buffer[10];
    
    int numChars = sprintf(buffer, "%d%d%d%d%d%d%d%d",
        (0x80&byte) == 0 ? 0 : 1,
        (0x40&byte) == 0 ? 0 : 1,
        (0x20&byte) == 0 ? 0 : 1,
        (0x10&byte) == 0 ? 0 : 1,
        (0x08&byte) == 0 ? 0 : 1,
        (0x04&byte) == 0 ? 0 : 1,
        (0x02&byte) == 0 ? 0 : 1,
        (0x01&byte) == 0 ? 0 : 1);

    if ( numChars != (int)8 )
    {
        LOGGING(ERRORS, "ERROR unexpected number of chars %d converted", numChars);
        return RESULT_ERROR;
    }
    else strByte = buffer;
    
    return RESULT_OK;                
}

Result HostTimer::TimerStatus::updateItem(Item_T item, std::string & stringfo, long longifo)
{
    // Update status item in file
    
    if ( !filePtr_.is_open() )
    {
        LOGMSG(ERRORS, "ERROR status file is not open");
        return RESULT_ERROR;
    }

    std::string itemHeader;
    std::stringstream stream;

    switch (item)
    {
        case PROGRAM_SET:
            itemHeader = "Program Set: ";
            if ( stringfo == prevProgramSet_ ) return RESULT_OK;
            else prevProgramSet_ = stringfo;
            break;
        case WEEK_MINUTE:
            itemHeader = "Week Minute: ";
            stream << longifo;
            stream >> stringfo;
            if ( stringfo == prevWeekMinute_ ) return RESULT_OK;
            else prevWeekMinute_ = stringfo;
            break;
        default:
            LOGGING(ERRORS, "ERROR unknown status item=%d", item);
            return RESULT_ERROR;
    }

    if ( ( itemHeader.length() + stringfo.length() ) >= ITEM_SIZE_IN_BYTES )
    {
        LOGGING(ERRORS, "ERROR item header %s lenght=%d + stringfo %s length=%d is bigger or equal than ITEM_SIZE_IN_BYTES=%d",
                        itemHeader.c_str(), itemHeader.length(), stringfo.c_str(), stringfo.length(), ITEM_SIZE_IN_BYTES);
        return RESULT_ERROR;
    }

    LOGGING(INFO, "updating %s %s", itemHeader.c_str(), stringfo.c_str() );

    filePtr_.seekp(std::ios_base::beg + item * ITEM_SIZE_IN_BYTES);
    filePtr_ << itemHeader;
    filePtr_.width( ITEM_SIZE_IN_BYTES -1 - itemHeader.length() ); filePtr_ << stringfo << std::endl;
    filePtr_.flush();

    return RESULT_OK;
}

Result HostTimer::TimerStatus::updateItem(Item_T item, std::string & stringfo, Byte_T byteInfo, HostTimer * hostTimer)
{
	assert( hostTimer != nullptr );

    // Update status item in file

    if ( !filePtr_.is_open() )
    {
        LOGMSG(ERRORS, "ERROR status file is not open");
        return RESULT_ERROR;
    }

    std::string itemHeader;

    switch (item)
    {
    case PROGRAM_SETPOINTS:
        itemHeader = "Program Setpoints [76543210]: ";
        if ( hostTimer->convertToStrByte(stringfo, static_cast<Byte_T>(byteInfo) ) != RESULT_OK );
        if ( byteInfo == prevProgramSetpoints_ ) return RESULT_OK;
        else prevProgramSetpoints_ = byteInfo;
        break;
    case DUTY_CYCLES_MASK:
        itemHeader = "Duty Cycles Mask            : ";
        if ( hostTimer->convertToStrByte(stringfo, static_cast<Byte_T>(byteInfo) ) != RESULT_OK );
        if ( byteInfo == prevDutyCyclesMask_ ) return RESULT_OK;
        else
        {
        	prevDutyCyclesMask_ = byteInfo;
        	if ( byteInfo & ( prevTriggersMask_ | ( prevConditionsMask_ & prevProgramSetpoints_ ) ) == ( prevTriggersMask_ | ( prevConditionsMask_ & prevProgramSetpoints_ ) ) ) return RESULT_OK;
        }
        break;
    case TRIGGERS_MASK:
        itemHeader = "Triggers Mask               : ";
        if ( hostTimer->convertToStrByte(stringfo, static_cast<Byte_T>(byteInfo) ) != RESULT_OK );
        if ( byteInfo == prevTriggersMask_ ) return RESULT_OK;
        else prevTriggersMask_ = byteInfo;
        break;
    case CONDITIONS_MASK:
        itemHeader = "Conditions Mask             : ";
        if ( hostTimer->convertToStrByte(stringfo, static_cast<Byte_T>(byteInfo) ) != RESULT_OK );
        if ( byteInfo == prevConditionsMask_ ) return RESULT_OK;
        else prevConditionsMask_ = byteInfo;
        break;
    case RELAY_SETPOINTS:
        itemHeader = "Relay Setpoints             : ";
        if ( hostTimer->convertToStrByte(stringfo, static_cast<Byte_T>(byteInfo) ) != RESULT_OK );
        if ( byteInfo == prevRelaySetpoints_ ) return RESULT_OK;
        else prevRelaySetpoints_ = byteInfo;
        break;
    default:
        LOGGING(ERRORS, "ERROR unknown status item=%d", item);
        return RESULT_ERROR;
    }

    if ( ( itemHeader.length() + stringfo.length() ) >= ITEM_SIZE_IN_BYTES )
    {
        LOGGING(ERRORS, "ERROR item header %s lenght=%d + stringfo %s length=%d is bigger or equal than ITEM_SIZE_IN_BYTES=%d",
                        itemHeader.c_str(), itemHeader.length(), stringfo.c_str(), stringfo.length(), ITEM_SIZE_IN_BYTES);
        return RESULT_ERROR;
    }

    LOGGING(INFO, "updating %s %s", itemHeader.c_str(), stringfo.c_str() );

    filePtr_.seekp(std::ios_base::beg + item * ITEM_SIZE_IN_BYTES);
    filePtr_ << itemHeader;
    filePtr_.width( ITEM_SIZE_IN_BYTES -1 - itemHeader.length() ); filePtr_ << stringfo << std::endl;
    filePtr_.flush();

    return RESULT_OK;
}

Result HostTimer::TimerStatus::updateItem(Item_T item, const std::map<uint8_t, float> & ioChannelValues)
{
    assert( item == INPUTS_OUTPUTS );

    std::string itemHeader = "Inputs/Outputs: ";
    char itemInfo[ITEM_SIZE_IN_BYTES] = "";

    for ( auto value : ioChannelValues )
    {
        if ( std::string(itemInfo) == "" )
        {
            snprintf(itemInfo, ITEM_SIZE_IN_BYTES, "%d:%.1f", value.first, value.second);
        }
        else
        {
            std::string buffer(itemInfo);
            snprintf(itemInfo, ITEM_SIZE_IN_BYTES, "%s|%d:%.1f", buffer.c_str(), value.first, value.second);
        }
    }

    float ioSum = std::accumulate(ioChannelValues.begin(), ioChannelValues.end(), 0.0,
    		                      [] (float value, const std::map<uint8_t, float>::value_type& element)
    		                          {
    		                              return abs(value) + abs( element.second );
    		                          } );

    //LOGGING(VERBOSE, "ioSum:%.1f prevIOSum_:%.1f variation:%.4f", prevIOSum_, ioSum, abs( ( ioSum - prevIOSum_)/prevIOSum_ ) );

    if ( abs( ( ioSum - prevIOSum_)/prevIOSum_ ) < 0.002) return RESULT_OK;
    else prevIOSum_ = ioSum;

    LOGGING(INFO, "updating %s %s", itemHeader.c_str(), itemInfo );

    filePtr_.seekp(std::ios_base::beg + item * ITEM_SIZE_IN_BYTES);
    filePtr_ << itemHeader;
    filePtr_.width( ITEM_SIZE_IN_BYTES -1 - itemHeader.length() ); filePtr_ << itemInfo << std::endl;
    filePtr_.flush();

    return RESULT_OK;
}


///////////////////////////////////////////////////////////////////////////////////////////////////
// PROTECTED METHODS
///////////////////////////////////////////////////////////////////////////////////////////////////


///////////////////////////////////////////////////////////////////////////////////////////////////
// MAIN
///////////////////////////////////////////////////////////////////////////////////////////////////

char Logger::logFileName_[] = "HostTimer.logs";

int main()
{
    // Wait for 1 second for HostKeeper to report first process PID before any logs
    usleep(1000000);

    Logger* logger = Logger::getInstance();
    logger->logging("main creating hostTimer instance...");
    HostTimer hostTimer("HostTimer");
    logger->logging("main initializing hostTimer...");
    if ( hostTimer.initialize() != RESULT_OK )
    {
        hostTimer.shutdown();
        assert(0);
    }
    logger->logging("main starting hostTimer...");
    if ( hostTimer.start() != RESULT_OK )
    {
        hostTimer.shutdown();
        assert(0);
    }
}
