#ifndef _GPIORASPBERRYPI2BADS1115_H
#define _GPIORASPBERRYPI2BADS1115_H

/////////////////////////////////////////////////////////////////////////////
/**
 *  @file   GpioAnalogRaspberryPi2BAds1115.h
 *  @author Manel González Farrera
 *  @date   July 2018
 *  @brief  Definition of GpioAnalogRaspberryPi2BAds1115
 *
 *  Function to be documented.
 */
/////////////////////////////////////////////////////////////////////////////

#include <stdint.h>
#include <array>
#include "LenamDevs_types.h"
#include "IComponent.h"
#include "IGpio.h"
#include <wiringPi.h>
#include <ads1115.h>


////////////////////////////////////////////////////////////////////////////////////////////////////
// GLOBAL CONSTANTS
///////////////////////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////////////////////////
// CLASS DECLARATION
///////////////////////////////////////////////////////////////////////////////////////////////////

class GpioAnalogRaspberryPi2BAds1115 : public IComponent, public IGpio
{
  public:

    /*
     * Class constructor 
     */
    GpioAnalogRaspberryPi2BAds1115(const char* instanceName) : IComponent(instanceName)
    {
        // Enable errors log channel
        logChannels_ = Logger::ERRORS;
    }

    /*
     * Class destructor 
     */
    ~GpioAnalogRaspberryPi2BAds1115() {}

    Result initialize() { ads1115Setup(100,0x48); return RESULT_OK; }

    Result start() { return RESULT_OK; }

    void shutdown() {} 
    
    Result setMode(unsigned char gpioId, GpioMode_T mode) { return RESULT_UNIMPLEMENTED; }

    Result getLevel(unsigned char id, signed int& level) const { return RESULT_UNIMPLEMENTED; }
    
    Result setLevel(unsigned char id, signed int level) { return RESULT_UNIMPLEMENTED; }

    Result getVoltage(unsigned char id, float &voltage) override
    {
        assert( id < NUMBER_CHANNELS_ );

        int16_t adcRead = (int16_t)analogRead(100 + (int16_t)id); 
        voltage = adcRead * (4.096 / 32768);

        if ( voltage != previousValues_[id] )
        {
            LOGGING(INFO, "id:%d adcRead:%d voltage:%1f", id, adcRead, voltage);
            previousValues_[id] = voltage;
        }
        else
        {
            LOGGING(VERBOSE, "id:%d adcRead:%d voltage:%1f", id, adcRead, voltage);
        }

        return RESULT_OK;
    }

  private:

    static const unsigned char NUMBER_CHANNELS_ = 4;

    std::array<float, NUMBER_CHANNELS_> previousValues_ { { 0.0, 0.0, 0.0, 0.0 } };
};

#endif // _GPIORASPBERRYPI2BADS1115_H
