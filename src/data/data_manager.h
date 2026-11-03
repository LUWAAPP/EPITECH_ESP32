#pragma once

#include <Arduino.h>
#include <vector>
#include <numeric>
#include <ArduinoJson.h>

// Use the full TaskScheduler.h to get proper implementations
#include <TaskScheduler.h>

#include "hal/hal.h"

// Remove the global taskRunner declaration
// Scheduler taskRunner;

class DataManager {
public:
    static DataManager& getInstance() {
        static DataManager instance;
        return instance;
    }
    
    void init(HAL& hal, Scheduler& scheduler);
    void setupTasks();
    void loop();
    
private:
    DataManager();
    DataManager(const DataManager&) = delete;
    DataManager& operator=(const DataManager&) = delete;
    
    // Task callbacks
    static void soundSampleCallback();
    static void pmSampleCallback();
    static void co2SampleCallback();
    static void htSampleCallback();
    static void sendDataFrameCallback();
    
    // Methods to calculate averages
    unsigned short int getSoundSamplesAverage();
    unsigned short int getPmSamplesAverage();
    unsigned short int getCo2SamplesAverage();
    void createDataFrame();
    
    // Data storage
    std::vector<unsigned int> soundSamples;
    std::vector<unsigned int> pm25Samples;
    std::vector<unsigned int> co2Samples;
    unsigned short int humidity;
    unsigned short int temperature;
    
    // JSON document for MQTT
    DynamicJsonDocument dataDoc;
    
    // Task scheduler
    Scheduler* scheduler;
    
    // Define proper task classes that derive from Task
    class SoundSampleTask : public Task {
        public:
            SoundSampleTask() : Task(SOUND_SAMPLE_TIME, TASK_FOREVER) {}
            bool Callback() override { soundSampleCallback(); return true; }
    } soundSampleTask;
    
    class PMSampleTask : public Task {
        public:
            PMSampleTask() : Task(PM_SAMPLE_TIME, TASK_FOREVER) {}
            bool Callback() override { pmSampleCallback(); return true; }
    } pmSampleTask;
    
    class CO2SampleTask : public Task {
        public:
            CO2SampleTask() : Task(CO2_SAMPLE_TIME, TASK_FOREVER) {}
            bool Callback() override { co2SampleCallback(); return true; }
    } co2SampleTask;
    
    class HTSampleTask : public Task {
        public:
            HTSampleTask() : Task(HT_SAMPLE_TIME, TASK_FOREVER) {}
            bool Callback() override { htSampleCallback(); return true; }
    } htSampleTask;
    
    class SendDataFrameTask : public Task {
        public:
            SendDataFrameTask() : Task(SEND_DATA_TIME, TASK_FOREVER) {}
            bool Callback() override { sendDataFrameCallback(); return true; }
    } sendDataFrameTask;
    
    // References to other subsystems
    HAL* hal;
    
    // Static instance for callbacks
    static DataManager* instance;
    
    // Task timing constants
    static const int SOUND_SAMPLE_TIME = 120;
    static const int PM_SAMPLE_TIME = 400;
    static const int CO2_SAMPLE_TIME = 2000;
    static const int HT_SAMPLE_TIME = 15000;
    static const int SEND_DATA_TIME = 15000;
}; 