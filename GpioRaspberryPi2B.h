#ifndef _GPIORASPBERRYPI2B_H
#define _GPIORASPBERRYPI2B_H

/////////////////////////////////////////////////////////////////////////////
/**
 *  @file   GpioRaspberryPi2B.h
 *  @author Manel González Farrera
 *  @date   November 2015
 *  @brief  Definition of GpioRaspberryPi2B
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

//extern const unsigned int NUM_CHANNELS;
const char GPIO_EXPORT_CHECK_FILE_NAME[20] = "gpioExport.check";

////////////////////////////////////////////////////////////////////////////////////////////////////
// CLASS DECLARATION
///////////////////////////////////////////////////////////////////////////////////////////////////

class GpioRaspberryPi2B : public IComponent, IGpio
{
  public:

    /*
     * Class constructor 
     */
    GpioRaspberryPi2B(const char* instanceName);

    /*
     * Class destructor 
     */
    ~GpioRaspberryPi2B() {}

    Result initialize();

    Result start() { return RESULT_OK; }

    void shutdown() {} 
    
    Result setMode(uint8_t gpioId, GpioMode_T mode);

    Result getLevel(uint8_t id, signed int& level) const;
    
    Result setLevel(uint8_t id, signed int level);

    Result getVoltage(uint8_t id, float &voltage) { return RESULT_UNIMPLEMENTED; }

  private:

    static const uint8_t MAX_NUM_GPIOS = 16u;

    /*
     * Number of operational GPIO's 
     */
    uint8_t numGpios_;

    /*
     * Correspondence between GPIO Id and Number 
     */
    uint8_t gpioIdNumber_[MAX_NUM_GPIOS];

    bool isGpioExported(uint8_t gpioId);

    Result exportGpio(uint8_t gpioId);
};

#endif // _GPIORASPBERRYPI2B_H
