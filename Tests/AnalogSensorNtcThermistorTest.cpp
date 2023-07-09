///////////////////////////////////////////////////////////////////////////////////////////////////
/**
 *  @file   AnalogSensorNtcThermistorTest.cpp
 *  @author Manel González Farrera
 *  @date   February 2018 
 *  @brief  Implements AnalogSensorNtcThermistorTest
 */
///////////////////////////////////////////////////////////////////////////////////////////////////

#include <stdlib.h>
#include "AnalogSensorNtcThermistor.h"
#include "GpioAnalogRaspberryPi2BAds1115.h"


///////////////////////////////////////////////////////////////////////////////////////////////////
// MAIN
///////////////////////////////////////////////////////////////////////////////////////////////////

char Logger::logFileName_[] = "AnalogSensorNtcThermistorTest.logs";

int main(int argc, char *argv[]) {

    std::cout << "main creating instance of AnalogSensorNtcThermistor model 25C10KB3470" << std::endl;

    GpioAnalogRaspberryPi2BAds1115 * gpioAnalog = new GpioAnalogRaspberryPi2BAds1115("GpioAnalogRaspberryPi2BAds1115");
    gpioAnalog->initialize();

    for ( int id = 0; id < 4; id++)
    {
        float voltage = 0.0;
        Result result = gpioAnalog->getVoltage(id, voltage);
        if ( result != RESULT_OK )
        {
            std::cout << "ERROR main gpioAnalog getting voltage with result:" << result << std::endl;
        }
        else
        {
            std::cout << "main get gpioAnalog id:" << id << " voltage:" << voltage << std::endl;
        }
    }

    AnalogSensorNtcThermistor ntcThermistor("NtcThermistor", "25C10KB3470");
    ntcThermistor.setInterface("NtcThermitor25C10KB3470", "IGpio", static_cast<IGpio *>(gpioAnalog));

    ntcThermistor.initialize();

    ntcThermistor.start();

    float temperatureValue = 0.0f;
    char charTemp[10];
    
    Result result =  ntcThermistor.readValue( static_cast<unsigned char>(0), temperatureValue );
    sprintf(charTemp, "%0.1f", temperatureValue);
    std::cout << "main read value of temperature is channel ID 0 is " << charTemp << " degC" << std::endl;

    result =  ntcThermistor.readValue( static_cast<unsigned char>(1), temperatureValue );
    sprintf(charTemp, "%0.1f", temperatureValue);
    std::cout << "main read value of temperature is channel ID 1 is " << charTemp << " degC" << std::endl;

    return 0;
}
