#include <Arduino.h>

#define CLOCKRATE 80000000 /* Hz */
#define TIMERDIVIDER 4

#define OVER_SAMPLE_RATIO (16)
#define CYCLES (20)
#define NSAMPLES (OVER_SAMPLE_RATIO * CYCLES) // 321

#define VOLTAGE_ADC_PIN (34)
#define CURRENT_ADC_PIN (35)

volatile int sampleCount = NSAMPLES;
volatile int voltageSamples[NSAMPLES];
volatile int currentSamples[NSAMPLES];

hw_timer_t *My_timer = NULL;

struct ElectricalMeasurements {
  double vrms;
  double irms;
  double realPower;
  double apparentPower;
  double powerFactor;
};

struct ElectricalMeasurements measurements;

void setupMeasurement();
struct ElectricalMeasurements makeMeasurement();

double amountMeasurements;
double sumVrms;
double sumIrms;
double sumRealPower;
double sumApparentPower;
double sumPowerFactor;

int countRmsMeasurements = 0;
double sumVoltageToSend = 0.0;
double sumCurrentToSend = 0.0;
double sumRealPowerToSend = 0.0;

const float voltageCalibration = 282.5;
const float currentCalibration = 30.5;

void setup() {
  Serial.begin(115200);
  setupMeasurement();
}

void loop() {
  measurements = makeMeasurement();
  if (countRmsMeasurements < 100) {
    sumVoltageToSend += measurements.vrms;
    sumCurrentToSend += measurements.irms;
    sumRealPowerToSend += measurements.realPower;
    sumApparentPower += measurements.vrms * measurements.irms;

    countRmsMeasurements++;
    
    return;
  }
  Serial.print("Vrms: ");
  Serial.print(sumVoltageToSend / 100.0, 3);
  Serial.print(" (V); Irms: ");
  Serial.print(sumCurrentToSend / 100.0, 3);
  Serial.print(" (I); Prms: ");
  Serial.print(sumRealPowerToSend / 100.0, 3);
  Serial.print(" ");
  float powerFactorConverted = (sumRealPowerToSend / 100.0) / (sumApparentPower / 100.0);
  Serial.println(powerFactorConverted);

  sumVoltageToSend = 0;
  sumCurrentToSend = 0;
  sumRealPowerToSend = 0;
  sumApparentPower = 0;

  countRmsMeasurements = 0;
}

void IRAM_ATTR onTimer() {
  if ((sampleCount >= 0) && (sampleCount < (NSAMPLES))) {
    voltageSamples[sampleCount] = analogRead(VOLTAGE_ADC_PIN);
    currentSamples[sampleCount] = analogRead(CURRENT_ADC_PIN);

    sampleCount++;
  }
}

void setupMeasurement() {
  My_timer = timerBegin(
    1,
    TIMERDIVIDER,
    true
  );

  timerAttachInterrupt(My_timer, &onTimer, true);

  float measureRatePerInterval = 1.0 / ( 60.0 * OVER_SAMPLE_RATIO);

  int amountTimeBetweenInterruption = (int)( measureRatePerInterval * CLOCKRATE / TIMERDIVIDER + 0.5);

  timerAlarmWrite(My_timer, amountTimeBetweenInterruption, true);

  timerAlarmEnable(My_timer);
}

void readAnalogSamples() {
  int waitDelay = 17 * CYCLES;
  sampleCount = 0;

  delay(waitDelay); // 340 ms
  if (sampleCount != NSAMPLES) {
    Serial.print("ADC processing is not working.");
  }

  timerWrite(My_timer, 0);
}

struct ElectricalMeasurements measureRms(int* voltageSamples, int* currentSamples, int nsamples) {
  struct ElectricalMeasurements eletricMeasurements;

  int offsetVoltage = 1851;
  int offsetCurrent = 1848;

  float sumVoltage = 0;
  float sumCurrent = 0;
  float sumInstantaneousPower = 0;
  for (int i = 0; i < nsamples; i++) {
    int y_voltageNoOffset = voltageSamples[i] - offsetVoltage;
    int y_currentNoOffset = currentSamples[i] - offsetCurrent;

    float y_voltage = ((float)y_voltageNoOffset) * voltageCalibration * (3.3/4096.0);
    float y_current = ((float)y_currentNoOffset) * currentCalibration * (3.3/4096.0);

    float y_instantaneousPower = y_voltage * y_current;
  
    sumVoltage += y_voltage * y_voltage;
    sumCurrent += y_current * y_current;
    sumInstantaneousPower += y_instantaneousPower;
  }

  float ym_voltage = sumVoltage / (float) nsamples;
  float ym_current = sumCurrent / (float) nsamples;
  float ym_realPower = sumInstantaneousPower / (float) nsamples;

  float vrmsSquared = sqrt(ym_voltage);
  float irmsSquared = sqrt(ym_current);

  float Vrms = vrmsSquared;
  float Irms = irmsSquared;
  float realPower = ym_realPower;

  eletricMeasurements.vrms = Vrms;
  eletricMeasurements.irms = Irms;
  eletricMeasurements.realPower = realPower;

  return eletricMeasurements;
}

struct ElectricalMeasurements makeMeasurement() {
  struct ElectricalMeasurements eletricMeasurements;

  readAnalogSamples();
  if (sampleCount == NSAMPLES) {
    eletricMeasurements = measureRms((int*) voltageSamples, (int*) currentSamples, NSAMPLES);
  }

  return eletricMeasurements;
}