#pragma once

#include <Arduino.h>
#include <HTTPClient.h>
#include <HTTPUpdate.h>
#include <mbedtls/md.h>
#include <mbedtls/pk.h>
#include "network/network_manager.h"

typedef void (*OtaStatusCallback)(const char* commandStatus, int progress);

class OTAManager {
public:
    static OTAManager& getInstance() {
        static OTAManager instance;
        return instance;
    }
    
    void init(const uint8_t* publicKey, size_t keyLen, 
              const char* firmwareUrl, const char* signatureUrl,
              const char* statusTopic,
              OtaStatusCallback onStatus = nullptr);
    bool performUpdate();
    bool performUpdateFromMqtt(const char* url, const char* version,
                              const char* checksum, size_t size);
    bool performRollback();
    
private:
    OTAManager() : publicKey(nullptr), keyLen(0), firmwareUrl(nullptr),
                  signatureUrl(nullptr), statusTopic(nullptr), onStatus(nullptr) {}
    OTAManager(const OTAManager&) = delete;
    OTAManager& operator=(const OTAManager&) = delete;
    
    bool verifySignature(const uint8_t* hash, size_t hashLen, 
                         const uint8_t* signature, size_t sigLen);
    void reportOtaStatus(const char* commandStatus, int progress);

    const uint8_t* publicKey;
    size_t keyLen;
    const char* firmwareUrl;
    const char* signatureUrl;
    const char* statusTopic;
    OtaStatusCallback onStatus;
}; 