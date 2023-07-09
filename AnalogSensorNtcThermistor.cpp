///////////////////////////////////////////////////////////////////////////////////////////////////
/**
 *  @file   AnalogSensorNtcThermistor.cpp
 *  @author Manel González Farrera
 *  @date   January 2017 
 *  @brief  Implements AnalogSensorNtcThermistor class
 */
///////////////////////////////////////////////////////////////////////////////////////////////////

#include "AnalogSensorNtcThermistor.h"
#include "ConstantsServices.h"
#include <wiringPi.h>
#include <ads1115.h>
#include <cmath>
//#include <stdio.h>
//#include <stdlib.h>
//#include <unistd.h>


///////////////////////////////////////////////////////////////////////////////////////////////////
// PUBLIC METHODS
///////////////////////////////////////////////////////////////////////////////////////////////////

AnalogSensorNtcThermistor::AnalogSensorNtcThermistor(const char* instanceName, const char* model)
:   IComponent(instanceName),
    model_(model)
{
    // Enable errors log channel
    logChannels_ = Logger::ERRORS;
}

Result AnalogSensorNtcThermistor::initialize()
{
    // Read constants 
    std::string constantsFileName = "NtcThermistor";
    constantsFileName += model_;
    ConstantsServices constsServices(constantsFileName.c_str());

    if ( constsServices.readConstant("B", beta_) != RESULT_OK )
    {
        LOGGING(ERRORS, "ERROR reading constant B, to use default hardcoaded value %d", beta_);
    }    
    else LOGGING(INFO, "INFO: read value of constant B is %d", beta_);

    if ( constsServices.readConstant("T0", temperature0_) != RESULT_OK )
    {
        LOGGING(ERRORS, "ERROR reading constant T0, to use default hardcoaded value %d", temperature0_);
    }    
    else LOGGING(INFO, "INFO: read value of constant T0 is %d", temperature0_);

    if ( constsServices.readConstant("R0", resistance0_) != RESULT_OK )
    {
        LOGGING(ERRORS, "ERROR reading constant R0, to use default hardcoaded value %d", resistance0_);
    }    
    else LOGGING(INFO, "INFO: read value of constant R0 is %d", resistance0_);

    if ( constsServices.readConstant("RPU", resistancePullUp_) != RESULT_OK )
    {
        LOGGING(ERRORS, "ERROR reading constant RPU, to use default hardcoaded value %d", resistancePullUp_);
    }    
    else LOGGING(INFO, "INFO: read value of constant RPU is %d", resistancePullUp_);

    return RESULT_OK;
}

void AnalogSensorNtcThermistor::setInterface(const char* instanceName, const char* interfaceName, void* interface)
{
    if ( std::string(interfaceName) == "IGpio" )
    {
        gpioAnalog_ = static_cast<IGpio *>(interface);
    }
}
 
Result AnalogSensorNtcThermistor::readValue(unsigned char id, float& value) const
{
    assert( gpioAnalog_ != nullptr );

    float voltage = 0.0;
    Result result = gpioAnalog_->getVoltage(id, voltage);
    if ( result != RESULT_OK )
    {
        LOGGING(ERRORS, "ERROR getting voltage of gpioAnalog_ with result %d", result);
        return result;
    }    

    // To convert voltage to temperature:
    //
    //           Vout
    // Rntc = ---------- * Rpullup
    //        Vcc - Vout
    //
    float vcc = 3.3;
    if (voltage > vcc) vcc = voltage;
    float resistanceNtc = static_cast<float>( voltage * resistancePullUp_ / ( vcc - voltage ) );
        
    // To convert NTC resistance to temperature:
    //
    //               1      1         Rntc    -1
    // Tntc[K] = [ ----- + --- * Ln ( ---- ) ]
    //             T0[K]    B          R0
    //
    float temperatureNtc = static_cast<float>( 1.0 / ( 1.0/(temperature0_+273.0) + 1.0/beta_*log(resistanceNtc/resistance0_) ) );
    
    // Convert value from degress Kelvin to Celsius
    value = static_cast<float>(temperatureNtc - 273.0);    

    if ( previousValues_.find(id) == previousValues_.end() )
    {
    	previousValues_[id] = value;
        LOGGING(INFO, "got voltage %.2f volts, calculated resistanceNtc %.0f Ohms, calculated temperatureNtc is %.1f degC", voltage, resistanceNtc, value);
    }
    else
    {
    	if ( value != previousValues_[id] )
    	{
    		LOGGING(INFO, "got voltage %.2f volts, calculated resistanceNtc %.0f Ohms, calculated temperatureNtc is %.1f degC", voltage, resistanceNtc, value);
    		previousValues_[id] = value;
    	}
    	else
    	{
    		LOGGING(VERBOSE, "got voltage %.2f volts, calculated resistanceNtc %.0f Ohms, calculated temperatureNtc is %.1f degC", voltage, resistanceNtc, value);
    	}
    }

    return result;
}

