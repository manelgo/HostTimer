#ifndef _I2C_RASPBERRYPI2B_H
#define _I2C_RASPBERRYPI2B_H

/////////////////////////////////////////////////////////////////////////////
/**
 *  @file   I2cRaspberryPi2B.h
 *  @author Manel González Farrera
 *  @date   December 2016
 *  @brief  Definition of I2cRaspberryPi2B
 *
 *  Function to be documented.
 */
/////////////////////////////////////////////////////////////////////////////

#include "LenamDevs_types.h"
#include "IComponent.h"
#include "IGpio.h"


////////////////////////////////////////////////////////////////////////////////////////////////////
// GLOBAL CONSTANTS
///////////////////////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////////////////////////
// CLASS DECLARATION
///////////////////////////////////////////////////////////////////////////////////////////////////

class I2cRaspberryPi2B : public IComponent, IAnalogSensor
{
  public:

    /*
     * Class constructor 
     */
    I2cRaspberryPi2B(const char* instanceName);

    /*
     * Class destructor 
     */
    ~I2cRaspberryPi2B() {}

    Result initialize();

    Result start() {}

    void shutdown() {} 
    
    Result readValue(unsigned char id, float& level) const;

  private:

    static const unsigned char MAX_NUM_I2C_PORTS = 2; 
};

#endif // _I2C_RASPBERRYPI2B_
