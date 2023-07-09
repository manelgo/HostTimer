///////////////////////////////////////////////////////////////////////////////////////////////////
/**
 *  @file   GpioRaspberryPi2B.cpp
 *  @author Manel González Farrera
 *  @date   Novembre 2015
 *  @brief  Implements GpioRaspberryPi2B class
 */
///////////////////////////////////////////////////////////////////////////////////////////////////

#include "GpioRaspberryPi2B.h"
#include "CommonGlobalsWebTimer.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>


///////////////////////////////////////////////////////////////////////////////////////////////////
// PUBLIC METHODS
///////////////////////////////////////////////////////////////////////////////////////////////////

GpioRaspberryPi2B::GpioRaspberryPi2B(const char* instanceName) : IComponent(instanceName)
{
    logChannels_ = Logger::VERBOSE;
}

Result GpioRaspberryPi2B::initialize()
{
    // Export GPIO's
    
    // PRODUCTIZED CONSTANTS
    { 
        // Number of GPIO's
        numGpios_ = MAX_NUM_GPIOS;
 
        // Designation of GPIO pin numbers
        //                 Pin Numbers = {11, 13, 15, 19, 22, 21, 24, 23,   32, 31, 33, 36, 35, 38, 37, 40}
        //gpioIdNumber_[MAX_NUM_GPIOS] = {17, 27, 22, 10, 25, 09, 08, 11,   12, 06, 13, 16, 19, 20, 26, 21}
        //                    Function = {RY, RY, RY, RY, RY, RY, RY, RY,   IO, IO, IO, IO, IO, IO, IO, IO}
        //
        // Output Relays
        gpioIdNumber_[ 0] = 17; // Pin Number 11
        gpioIdNumber_[ 1] = 27; // Pin Number 13
        gpioIdNumber_[ 2] = 22; // Pin Number 15
        gpioIdNumber_[ 3] = 10; // Pin Number 19
        gpioIdNumber_[ 4] = 25; // Pin Number 22
        gpioIdNumber_[ 5] =  9; // Pin Number 21
        gpioIdNumber_[ 6] =  8; // Pin Number 24
        gpioIdNumber_[ 7] = 11; // Pin Number 23
        //
        // Digital Input/Output's
        gpioIdNumber_[ 8] = 12; // Pin Number 32
        gpioIdNumber_[ 9] =  6; // Pin Number 31
        gpioIdNumber_[10] = 13; // Pin Number 33
        gpioIdNumber_[11] = 16; // Pin Number 36
        gpioIdNumber_[12] = 19; // Pin Number 35
        gpioIdNumber_[13] = 20; // Pin Number 38
        gpioIdNumber_[14] = 26; // Pin Number 37
        gpioIdNumber_[15] = 21; // Pin Number 40
    }

    // Export GPIO's to file system
    for (unsigned int i=0; i<numGpios_; i++)
    {
        if ( !isGpioExported(gpioIdNumber_[i]) )
        {
            if ( exportGpio(gpioIdNumber_[i]) != RESULT_OK )
            {
                LOGGING(ERRORS, "ERROR exporting GPIO number %d", gpioIdNumber_[i]);
                return RESULT_ERROR;
            } 
        }
    }

    return RESULT_OK;
}

Result GpioRaspberryPi2B::setMode(uint8_t gpioId, GpioMode_T mode)
{
    char auxBuffer[50];
    std::string setDirectionCommand, setActiveLowCommand = "default_active_low";
    std::string setDirection, getDirection, setActiveLow = "0", getActiveLow;

    switch (mode)
    {
      case INPUT_DIGITAL_INVERTED:
        sprintf(auxBuffer, "echo 1 > /sys/class/gpio/gpio%d/active_low", gpioIdNumber_[gpioId]);
        setActiveLowCommand = auxBuffer;
        setActiveLow = '1';
      case INPUT_DIGITAL:
        sprintf(auxBuffer, "echo in > /sys/class/gpio/gpio%d/direction", gpioIdNumber_[gpioId]);
        setDirectionCommand = auxBuffer;
        setDirection = "in";
        break;
      case OUTPUT_DIGITAL_INVERTED:
        sprintf(auxBuffer, "echo 1 > /sys/class/gpio/gpio%d/active_low", gpioIdNumber_[gpioId]);
        setActiveLowCommand = auxBuffer;
        setActiveLow = '1';
      case OUTPUT_DIGITAL:
        sprintf(auxBuffer, "echo out > /sys/class/gpio/gpio%d/direction", gpioIdNumber_[gpioId]);
        setDirectionCommand = auxBuffer;
        setDirection = "out";
        break;
      default:
        LOGGING(ERRORS, "ERROR GPIO mode %d unkwown", mode);
        return RESULT_ERROR;
    }
    if ( setActiveLowCommand != "default_active_low" )
    {
        LOGGING(VERBOSE, "executing linux command '%s'...", setActiveLowCommand.c_str());
        system(setActiveLowCommand.c_str());
        usleep(10000);
    }
    LOGGING(VERBOSE, "executing linux command '%s'...", setDirectionCommand.c_str());
    system(setDirectionCommand.c_str());
    usleep(10000);
    
    // Check active low is set correctly
    char activeLowFile[50];
    sprintf(activeLowFile, "/sys/class/gpio/gpio%d/active_low", gpioIdNumber_[gpioId]);
    std::ifstream activeLowFilePtr(activeLowFile);
    if ( !activeLowFilePtr.is_open() )
    {
        LOGGING(ERRORS, "ERROR opening file %s", activeLowFile);
        return RESULT_ERROR;
    }
    activeLowFilePtr >> getActiveLow;
    activeLowFilePtr.close();
    if (getActiveLow != setActiveLow)
    {
        LOGGING(ERRORS, "unable to set GPIO# %d active_low to %s, setting got is %s", gpioIdNumber_[gpioId], setActiveLow.c_str(), getActiveLow.c_str());
        return RESULT_ERROR;
    }

    // Check direction is set correctly
    char directionFile[50];
    sprintf(directionFile, "/sys/class/gpio/gpio%d/direction", gpioIdNumber_[gpioId]);
    std::ifstream directionFilePtr(directionFile);
    directionFilePtr >> getDirection;
    directionFilePtr.close();
    if (getDirection != setDirection)
    {
        LOGGING(ERRORS, "unable to set %s mode in GPIO# %d", setDirection.c_str(), gpioIdNumber_[gpioId]);
        return RESULT_ERROR;
    }

    // Set GPIO to init level to low
    if ( mode == OUTPUT_DIGITAL || mode == OUTPUT_DIGITAL_INVERTED )
    {
        if ( setLevel(gpioId, 0) != RESULT_OK )
        {
            LOGGING(ERRORS, "ERROR setting GPIO# %d to init level 0", gpioIdNumber_[gpioId]);
	    return RESULT_ERROR;
        }
    }

    return RESULT_OK;
}

Result GpioRaspberryPi2B::getLevel(uint8_t gpioId, signed int& level) const
{
    char valueFileName[50];
    sprintf(valueFileName, "/sys/class/gpio/gpio%d/value", gpioIdNumber_[gpioId]);
    std::ifstream valueFilePtr(valueFileName);
    valueFilePtr >> level;
    valueFilePtr.close();

    return RESULT_OK;
}

Result GpioRaspberryPi2B::setLevel(uint8_t gpioId, signed int level)
{
    char setLevelCommand[50];
    std::string setLevel, getLevel;

    // Check level value is supported
    switch (level)
    {
      case 0:
        setLevel = "0";
        break;
      case 1:
        setLevel = "1";
        break;
      default:
        LOGGING(ERRORS, "ERROR level %d is not supported", level);
        return RESULT_ERROR;
    }

    // Set level
    sprintf(setLevelCommand, "echo %d > /sys/class/gpio/gpio%d/value", level, gpioIdNumber_[gpioId]);  
    LOGGING(VERBOSE, "executing linux command '%s'...", setLevelCommand);
    system(setLevelCommand);
    usleep(10000);

    // Check level is set correctly
    char valueFileName[50];
    sprintf(valueFileName, "/sys/class/gpio/gpio%d/value", gpioIdNumber_[gpioId]);
    std::ifstream valueFilePtr(valueFileName);
    valueFilePtr >> getLevel;
    valueFilePtr.close();
    if (getLevel != setLevel)
    {
        LOGGING(ERRORS, "unable to set %s level in GPIO# %d", setLevel.c_str(), gpioIdNumber_[gpioId]);
        return RESULT_ERROR;
    }

    return RESULT_OK;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// PUBLIC METHODS
///////////////////////////////////////////////////////////////////////////////////////////////////

bool GpioRaspberryPi2B::isGpioExported(uint8_t gpioId)
{
    char directionFileName[50];

    sprintf(directionFileName, "/sys/class/gpio/gpio%d/direction", gpioId);

    std::ifstream directionFilePtr(directionFileName);
    
    if ( directionFilePtr.is_open() )
    {
        directionFilePtr.close();
        return true;
    }
    else return false;
}

Result GpioRaspberryPi2B::exportGpio(uint8_t gpioId)
{
    char linuxCommand[50];

    sprintf(linuxCommand, "echo %d > /sys/class/gpio/export", gpioId);
    LOGGING(VERBOSE, "executing linux command '%s'...", linuxCommand);
    system(linuxCommand);
    usleep(200000);

    if ( isGpioExported(gpioId) ) return RESULT_OK;
    else                          return RESULT_ERROR;
}


