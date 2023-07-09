#ifndef _ANALOGSENSOR_NTCTHERMISTOR_H
#define _ANALOGSENSOR_NTCTHERMISTOR_H

/////////////////////////////////////////////////////////////////////////////
/**
 *  @file   AnalogSensorNtcThermistor.h
 *  @author Manel González Farrera
 *  @date   January 2017
 *  @brief  Definition of AnalogSensorNtcThermistor
 *
 *  Function to be documented.
 */
/////////////////////////////////////////////////////////////////////////////

#include <stdint.h>
#include <map>
#include "LenamDevs_types.h"
#include "IComponent.h"
#include "IGpio.h"
#include "IAnalogSensor.h"


////////////////////////////////////////////////////////////////////////////////////////////////////
// GLOBAL CONSTANTS
///////////////////////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////////////////////////
// CLASS DECLARATION
///////////////////////////////////////////////////////////////////////////////////////////////////

class AnalogSensorNtcThermistor : public IComponent, public IAnalogSensor
{
  public:

    /*
     * Class constructor 
     */
    AnalogSensorNtcThermistor(const char* instanceName, const char* model);

    /*
     * Class destructor 
     */
    ~AnalogSensorNtcThermistor() {}

    Result initialize();

    Result start() { return RESULT_OK; }

    void shutdown() {} 
   
    void setInterface(const char* instanceName, const char* interfaceName, void* interface);
 
    Result readValue(unsigned char id, float& value) const;

  private:

    //std::ifstream constantsFilePtr_;

    std::string model_;

    int32_t beta_             = 3470;
    int32_t temperature0_     = 25;
    int32_t resistance0_      = 10000;
    int32_t resistancePullUp_ = 10000;

    IGpio* gpioAnalog_        = nullptr;

    mutable std::map<unsigned char, float> previousValues_;
};

#endif // _ANALOGSENSOR_NTCTHERMISTOR_H
