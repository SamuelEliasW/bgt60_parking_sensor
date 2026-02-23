// BGT60 Parking Sensor
// ====================
// Making a stationary parking sensor for cars.
//
// 02.2026
// Created by ThatOneSchmu and Infineon

#include <SPI.h>
#include <Adafruit_NeoPixel.h>
#include <bgt60tr13c.hpp>

// NeoPixel configuration
#define LED_PIN    0        // Pin connected to NeoPixel strip
#define LED_COUNT  18       // Number of LEDs in strip
Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_GRB);

// Distance mapping constants
const float min_distance = 10.0;
const float max_distance = 100.0;

const float distance_safe = 40;
const float distance_danger = 15;


// const values
static const size_t no_of_chirps = 1;
static const size_t samples_per_chirp = 128;
static const size_t words = samples_per_chirp * no_of_chirps;
static const size_t ADC_DIV = 60;
static const size_t start_freq = 58000000;  // in kHz
static const size_t bandwidth  =  4500000;    // in kHz
static const float threshold = 3.0;

static float range_resolution;

/*
  Define the pins for the BGT60TR13C sensor->
  The Board used is the Infineon CY8CKIT-062S2-AI.
*/
#define RSPI_MOSI 41
#define RSPI_MISO 42
#define RSPI_SCLK 43
#define RSPI_CS   44
#define RXRES_L   40

#define CHIP_FREQ 100000000

#ifdef TARGET_APP_CY8CKIT_062S2_AI
/* 
 * The CY8CKIT-062S2-AI-Board uses the
 * class SPIClassPSOC to create a new SPI-Instance.
 * This way, we can wire the radar sensor directly
 * to the spi interface.
 */
static SPIClassPSOC spi_radar_interface = SPIClassPSOC(
  RSPI_MOSI, 
  RSPI_MISO, 
  RSPI_SCLK, 
  NC, 
  false
);
static SPIClass* spi_interface = &spi_radar_interface;
#else
/*
 * When a different Board is used, the default SPI-Class
 * is used.
 * Change, when necessary.
 */
static SPIClass* spi_interface = &SPI;
#endif

/**
 * @brief Finds the nearest peak in the signal based on a threshold function.
 * @param signal Pointer to the signal data.
 * @return returns nearest peak detected in cm
 */
float find_nearest_peak(float const * const signal)
{
  for(size_t i = 1; i < words/2 - 1; i++)
  {
    if (signal[i] > threshold)
      return calculate_range_from_index(i, range_resolution) * 100.0 / no_of_chirps;
  }
  return -1.0;  // No peak found
}

/**
 * @brief Controls NeoPixel strip based on distance measurement
 * @param distance Distance in cm (10-100cm range)
 */
void controlNeoPixels(float distance) {
  // Clear all LEDs first
  strip.clear();
  
  // Check if distance is valid
  if (distance < 0 || distance < min_distance || distance > max_distance) {
    // No valid distance detected - turn off all LEDs
    strip.show();
    return;
  }
  
  // Map distance to number of LEDs (closer = more LEDs)
  // At 10cm: all 16 LEDs, at 100cm: 1 LED
  int numLEDs = map(distance, min_distance, max_distance, LED_COUNT, 1);
  
  // Calculate color based on distance
  uint32_t color;
  if (distance <= distance_danger) {
    // Close distance: Red (warning)
    color = strip.Color(255, 0, 0);
  } else if (distance <= distance_safe) {
    // Medium distance: Yellow/Orange
    color = strip.Color(255, 165, 0);
  } else {
    // Far distance: Green (safe)
    color = strip.Color(0, 255, 0);
  }
  
  // Light up the calculated number of LEDs
  for (int i = 0; i < numLEDs; i++) {
    strip.setPixelColor(i, color);
  }
  
  // Update the strip
  strip.show();
  
  // Debug output
  Serial.print("Distance: ");
  Serial.print(distance);
  Serial.print(" cm, LEDs lit: ");
  Serial.println(numLEDs);
}

BGT60TR13C* sensor;
void setup() {
  Serial.begin(115200);
  Serial.println("> Serial Monitor enabled.");

  // Initialize NeoPixel strip
  strip.begin();
  strip.show();
  strip.setBrightness(50);
  Serial.println("> NeoPixel strip initialized.");
  

  sensor = new BGT60TR13C(words, NULL, RSPI_CS, RXRES_L, CHIP_FREQ, spi_interface);

  Serial.println("> Reset sensor->..");
  sensor->reset();

  sensor->set_adc_div(ADC_DIV);
  sensor->set_chirp_len(samples_per_chirp);

  size_t FSU = sensor->calculate_FSU(start_freq);
  size_t RTU = sensor->calculate_RTU(ADC_DIV, samples_per_chirp);
  size_t RSU = sensor->calculate_RSU(bandwidth, RTU);

  sensor->configure_chirp(FSU, RTU, RSU);

  sensor->set_vga_gain(1, 3);

  sensor->init_sensor();
  Serial.println("> Sensor initialised!");
  
  range_resolution = sensor->get_range_resolution();
  
  sensor->start_frame();
}

void loop() {
  sensor->read_distance();
  
  float* fft_measured_data = sensor->get_fft_data();

  float distance = find_nearest_peak(fft_measured_data);

  controlNeoPixels(distance);
  delay(100);
  
  sensor->reset_fifo();
  sensor->start_frame();
}
