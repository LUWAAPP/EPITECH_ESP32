#include "data_manager.h"
#include "network/network_manager.h"
#include "utils/runtime_config.h"
#include "utils/task_manager.h"

#include <time.h>

// Initialize static member
DataManager* DataManager::instance = nullptr;

DataManager::DataManager() : 
    dataDoc(2048),
    scheduler(nullptr),
    hal(nullptr),
    humidity(0),
    temperature(0) {
    
    instance = this;
}

void DataManager::init(HAL& halInstance, Scheduler& taskRunner) {
    hal = &halInstance;
    scheduler = &taskRunner;
    setupTasks();
}

void DataManager::setupTasks() {
    scheduler->addTask(soundSampleTask);
    scheduler->addTask(pmSampleTask);
    scheduler->addTask(htSampleTask);
    scheduler->addTask(co2SampleTask);
    scheduler->addTask(sendDataFrameTask);
    
    soundSampleTask.enable();
    pmSampleTask.enable();
    htSampleTask.enable();
    co2SampleTask.enable();
    sendDataFrameTask.enable();
}

void DataManager::loop() {
    // Let the main loop execute the scheduler
    // scheduler->execute();
}

// Static task callbacks
void DataManager::soundSampleCallback() {
    if (instance && instance->hal) {
        if (instance->hal->isSensorInitialized(SENSOR_SOUND)) {
            unsigned int peakToPeak;
            if (instance->hal->getSoundSensor().readWithRetry(peakToPeak, 3, 200)) {
                instance->soundSamples.push_back(peakToPeak);
                LOG_D("Sound level: %u", peakToPeak);
            } else {
                LOG_W("Sound sensor read failed after retries");
            }
        } else {
            LOG_V("Sound sensor not initialized, skipping reading");
        }
    }
}

void DataManager::pmSampleCallback() {
    if (instance && instance->hal) {
        uint16_t pm25;
        if (instance->hal->isSensorInitialized(SENSOR_PM)) {
            if (instance->hal->getPMSensor().readWithRetry(pm25, 3, 200)) {
                instance->pm25Samples.push_back(pm25);
                LOG_D("PM2.5: %u µg/m³", pm25);
            } else {
                LOG_W("PM sensor read failed after retries");
            }
        } else {
            LOG_V("PM sensor not initialized, skipping reading");
        }
    }
}

void DataManager::co2SampleCallback() {
    if (instance && instance->hal) {
        if (instance->hal->isSensorInitialized(SENSOR_CO2)) {
            int co2;
            int8_t temp;
            if (instance->hal->getCO2Sensor().readWithRetry(co2, temp, 5, 500)) {
                instance->co2Samples.push_back(co2);
                LOG_D("CO2: %d ppm, Temperature: %d°C", co2, temp);
            } else {
                LOG_W("CO2 sensor read failed after retries");
            }
        } else {
            LOG_V("CO2 sensor not initialized, skipping reading");
        }
    }
}

void DataManager::htSampleCallback() {
    if (instance && instance->hal) {
        if (instance->hal->isSensorInitialized(SENSOR_TEMPERATURE)) {
            float temp, humidity;
            if (instance->hal->getTemperatureSensor().readWithRetry(temp, humidity, 3, 200)) {
                // Keep the precision the sensor gives: truncating to an integer
                // loses 0.1 C and 0.1 %RH on every frame.
                instance->temperature = temp;
                instance->humidity = humidity;
                LOG_D("Temperature: %.1f°C, Humidity: %.1f%%", temp, humidity);
            } else {
                LOG_W("Temperature sensor read failed after retries");
            }
        } else {
            LOG_V("Temperature sensor not initialized, skipping reading");
        }
    }
}

void DataManager::sendDataFrameCallback() {
    if (instance) {
        instance->createDataFrame();
    }
}

unsigned short int DataManager::getSoundSamplesAverage() {
    if (soundSamples.empty()) return 0;
    
    unsigned short int average = std::accumulate(soundSamples.begin(), soundSamples.end(), 0.0) / soundSamples.size();
    soundSamples.clear();
    return average;
}

unsigned short int DataManager::getPmSamplesAverage() {
    if (pm25Samples.empty()) return 0;
    
    unsigned short int average = std::accumulate(pm25Samples.begin(), pm25Samples.end(), 0.0) / pm25Samples.size();
    pm25Samples.clear();
    return average;
}

unsigned short int DataManager::getCo2SamplesAverage() {
    if (co2Samples.empty()) return 0;
    
    unsigned short int average = std::accumulate(co2Samples.begin(), co2Samples.end(), 0.0) / co2Samples.size();
    co2Samples.clear();
    return average;
}

void DataManager::createDataFrame() {
    unsigned short int avgSound = getSoundSamplesAverage();
    unsigned short int avgCO2 = getCo2SamplesAverage();
    unsigned short int avgPM25 = getPmSamplesAverage();

    // Metrology fix (S6+7 audit): sound, PM and CO2 are windowed averages and
    // humidity/temperature now keep their decimal precision instead of being
    // truncated to integers. A frame that stays identical for three rounds is
    // flagged as stale rather than published as fresh data.
    // Measurement time, not delivery time. Zero until NTP answers.
    long epoch = (long)time(nullptr);
    dataDoc["t"] = epoch > 1600000000L ? epoch : 0;

    // Frozen-value detection: identical quantised readings for three frames in a
    // row are suspicious (stuck driver, unplugged sensor returning last value).
    unsigned short qSound = avgSound;
    unsigned short qCo2 = avgCO2;
    unsigned short qPm25 = avgPM25;
    unsigned short qHum10 = (unsigned short)(humidity * 10.0f);
    unsigned short qTemp10 = (unsigned short)(temperature * 10.0f);

    bool frozen = haveLastFrame &&
                  qSound == lastSound && qCo2 == lastCo2 && qPm25 == lastPm25 &&
                  qHum10 == lastHum10 && qTemp10 == lastTemp10;
    frozenFrames = frozen ? frozenFrames + 1 : 0;

    lastSound = qSound;
    lastCo2 = qCo2;
    lastPm25 = qPm25;
    lastHum10 = qHum10;
    lastTemp10 = qTemp10;
    haveLastFrame = true;

    dataDoc["fields"][0]["snd"] = avgSound;
    dataDoc["fields"][1]["co2"] = avgCO2;
    dataDoc["fields"][2]["pm25"] = avgPM25;
    dataDoc["fields"][3]["hum"] = round(humidity * 10.0f) / 10.0f;
    dataDoc["fields"][4]["temp"] = round(temperature * 10.0f) / 10.0f;

    if (frozenFrames >= 2) {
        dataDoc["stale"] = true;
        LOG_W("values unchanged for %u frames; flagging stale", frozenFrames + 1);
    }

    String dataString;
    serializeJson(dataDoc, dataString);

    // RuntimeConfig reads the geohash from NVS, so cmd/geohash can move the node
    // without reflashing; the default comes from secrets.h.
    String teleTopic = RuntimeConfig::getInstance().getTeleTopic();
    if (NetworkManager::getInstance().publishMessage(teleTopic.c_str(), dataString.c_str())) {
        LOG_I("frame sent to %s: %s", teleTopic.c_str(), dataString.c_str());
    } else {
        LOG_E("frame dropped (no broker) %s", dataString.c_str());
    }

    if (soundSamples.empty() && pm25Samples.empty() && co2Samples.empty()) {
        LOG_W("frame built with no samples in any window");
    }
} 