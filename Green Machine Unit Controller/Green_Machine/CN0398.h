#ifndef _CN0398_H_
#define _CN0398_H_
#include "Arduino.h"
#include "HardwareSerial.h"
#include "AD7124.h"
#include "Pinout.h"

#define YES 1
#define NO 0

//////////////////// Configuration ////////////////////

#define DISPLAY_REFRESH 1000 //ms
#define USE_PH_CALIBRATION YES
#define USE_LOAD_FACTOR NO

#define USE_MANUFACTURER_MOISTURE_EQ

#define TEMPERATURE_SENSOR_PRESENT
#define MOISTURE_SENSOR_PRESENT
#define PH_SENSOR_PRESENT

// #define USE_LINEAR_TEMP_EQ

///////////////////////////////////////////////////////

// Calibration stuff skipped

#define RTD_CHANNEL 0
#define PH_CHANNEL 1
#define MOISTURE_CHANNEL 2

#define ZERO_POINT_TOLERANCE (0.003)
#define PH_ISO (7)
#define AVOGADRO (8.314)
#define FARADAY_CONSTANT (96485.0)
#define KELVIN_OFFSET (273.1)

#define R0 100.0

class CN0398{
    public:
        CN0398();

        float read_rtd();
        float read_ph(float temperature = 25.0);
        float read_moisture();

        enum {
            P1 = 0,
            P2 = 1,
            P3 = 2,
            P4 = 3
        };

        int32_t read_channel(uint8_t ch);
        float data_to_voltage(uint32_t data, uint8_t gain = 1, float VREF = 2.5);
        float data_to_voltage_bipolar(uint32_t data, uint8_t gain = 1, float VREF = 2.5);

        void enable_channel(int channel);
        void disable_channel(int channel);
        void enable_current_source0(int current_source_channel);
        void enable_current_source1(int current_source_channel);
        void set_digital_output(uint8_t p, bool state);
        void start_single_conversion();

        void reset();
        void setup();
        void init();

        void set_data(void);
        void display_data(void);

        AD7124 ad7124;

        bool use_nernst = false;
        const uint16_t SENSOR_SETTLING_TIME = 400; /*in ms*/
        float offset_voltage;
        float calibration_ph[2][2];
        uint8_t solution0,solution1;

        // Default calibration
        const float default_offset_voltage = 0;
        float default_calibration_ph[2][2] = {{4, 0.169534}, {10,  -0.134135}};
};

#endif
