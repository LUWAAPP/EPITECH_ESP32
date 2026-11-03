#include "ota_manager.h"
#include <esp_ota_ops.h>
#include <esp_err.h>

void OTAManager::init(const uint8_t* publicKey, size_t keyLen, 
                     const char* firmwareUrl, const char* signatureUrl,
                     const char* statusTopic,
                     OtaStatusCallback onStatus) {
    this->publicKey = publicKey;
    this->keyLen = keyLen;
    this->firmwareUrl = firmwareUrl;
    this->signatureUrl = signatureUrl;
    this->statusTopic = statusTopic;
    this->onStatus = onStatus;
}

void OTAManager::reportOtaStatus(const char* commandStatus, int progress) {
    if (onStatus) {
        onStatus(commandStatus, progress);
    } else if (statusTopic) {
        NetworkManager::getInstance().publishMessage(statusTopic, commandStatus);
    }
}

bool OTAManager::performUpdate() {
    LOG_I("Checking for updates...");
    NetworkManager& networkManager = NetworkManager::getInstance();
    
    // RAII wrapper for mbedtls contexts
    class MbedContexts {
    public:
        // These are mbedtls context objects that need proper initialization/cleanup
        mbedtls_pk_context pkContext;     // Public key context
        mbedtls_md_context_t mdContext;   // Message digest context
        
        // Constructor - automatically called when object is created
        MbedContexts() {
            mbedtls_pk_init(&pkContext);  // Initialize public key context
            mbedtls_md_init(&mdContext);  // Initialize message digest context
        }
        
        // Destructor - automatically called when object goes out of scope
        ~MbedContexts() {
            mbedtls_md_free(&mdContext);  // Clean up message digest context
            mbedtls_pk_free(&pkContext);  // Clean up public key context
        }
    } contexts;

    // Download signature
    HTTPClient sigHttp;
    sigHttp.begin(signatureUrl);
    sigHttp.setTimeout(30000);
    int httpCode = sigHttp.GET();
    if (httpCode != HTTP_CODE_OK) {
        LOG_E("Failed to download signature");
        networkManager.publishMessage(statusTopic, "Failed to download signature");
        return false;
    }

    size_t signatureSize = sigHttp.getSize();
    std::unique_ptr<uint8_t[]> signature(new uint8_t[signatureSize]);
    if (!signature) {
        LOG_E("Failed to allocate signature memory");
        networkManager.publishMessage(statusTopic, "Memory allocation failed");
        return false;
    }

    sigHttp.getStreamPtr()->readBytes(signature.get(), signatureSize);
    sigHttp.end();

    // Setup crypto contexts
    if (mbedtls_pk_parse_public_key(&contexts.pkContext, publicKey, keyLen) != 0) {
        LOG_E("Failed to parse public key");
        networkManager.publishMessage(statusTopic, "Failed to parse public key");
        return false;
    }

    if (mbedtls_md_setup(&contexts.mdContext, mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), 0) != 0) {
        LOG_E("Failed to setup hash context");
        networkManager.publishMessage(statusTopic, "Hash setup failed");
        return false;
    }

    // Download and verify firmware
    HTTPClient firmwareHttp;
    firmwareHttp.begin(firmwareUrl);
    firmwareHttp.setTimeout(60000);
    httpCode = firmwareHttp.GET();
    if (httpCode != HTTP_CODE_OK) {
        LOG_E("Failed to start firmware download");
        networkManager.publishMessage(statusTopic, "Firmware download failed");
        return false;
    }

    WiFiClient* client = firmwareHttp.getStreamPtr();
    client->setTimeout(60000);
    const size_t bufSize = 4096;
    std::unique_ptr<uint8_t[]> buf(new uint8_t[bufSize]);
    if (!buf) {
        LOG_E("Failed to allocate buffer");
        networkManager.publishMessage(statusTopic, "Memory allocation failed");
        return false;
    }

    int totalBytes = firmwareHttp.getSize();
    int remainingBytes = totalBytes;
    int lastProgress = 0;
    int maxRetries = 3;
    unsigned long lastReadTime = 0;
    unsigned long progressReportInterval = 5000;
    unsigned long lastProgressReport = 0;

    LOG_I("Starting firmware download (%d bytes)", totalBytes);
    networkManager.publishMessage(statusTopic, String("Downloading firmware: 0%").c_str());

    mbedtls_md_starts(&contexts.mdContext);

    // Hash the firmware
    while (remainingBytes > 0) {
        unsigned long now = millis();
        int progress = ((totalBytes - remainingBytes) * 100) / totalBytes;
        if (progress >= lastProgress + 10 || now - lastProgressReport > progressReportInterval) {
            lastProgress = progress;
            lastProgressReport = now;
            LOG_I("Download progress: %d%%", progress);
            networkManager.publishMessage(statusTopic, String("Downloading firmware: " + String(progress) + "%").c_str());
            delay(10);
        }

        size_t bytesToRead = remainingBytes > bufSize ? bufSize : remainingBytes;
        size_t bytesRead = 0;
        int retries = 0;
        
        while (bytesRead == 0 && retries < maxRetries) {
            yield();
            bytesRead = client->readBytes(buf.get(), bytesToRead);
            lastReadTime = millis();
            
            if (bytesRead == 0) {
                retries++;
                LOG_W("Read retry %d/%d", retries, maxRetries);
                delay(100 * retries);
            }
        }
        
        if (bytesRead == 0) {
            LOG_E("Read timeout after %d retries", maxRetries);
            networkManager.publishMessage(statusTopic, "Firmware read timeout after retries");
            return false;
        }

        mbedtls_md_update(&contexts.mdContext, buf.get(), bytesRead);
        remainingBytes -= bytesRead;
    }

    LOG_I("Download complete: 100%%");
    networkManager.publishMessage(statusTopic, "Download complete");
    
    uint8_t hash[32];
    mbedtls_md_finish(&contexts.mdContext, hash);

    // Verify signature
    if (mbedtls_pk_verify(&contexts.pkContext, MBEDTLS_MD_SHA256, hash, sizeof(hash), 
                          signature.get(), signatureSize) != 0) {
        LOG_E("Signature verification failed");
        networkManager.publishMessage(statusTopic, "Signature verification failed");
        return false;
    }

    firmwareHttp.end();
    LOG_I("Signature verified successfully");
    networkManager.publishMessage(statusTopic, "Signature verified");

    // Perform update
    firmwareHttp.begin(firmwareUrl);
    firmwareHttp.setTimeout(120000);
    client = firmwareHttp.getStreamPtr();
    client->setTimeout(120000);
    
    LOG_I("Starting firmware installation...");
    networkManager.publishMessage(statusTopic, "Installing firmware");
    
    t_httpUpdate_return ret = httpUpdate.update(*client, firmwareUrl);

    switch (ret) {
        case HTTP_UPDATE_FAILED:
            LOG_E("HTTP_UPDATE_FAILED Error (%d): %s\n", 
                         httpUpdate.getLastError(),
                         httpUpdate.getLastErrorString().c_str());
            networkManager.publishMessage(statusTopic, "OTA update failed");
            return false;

        case HTTP_UPDATE_NO_UPDATES:
            LOG_E("HTTP_UPDATE_NO_UPDATES");
            networkManager.publishMessage(statusTopic, "No updates available");
            return false;

        case HTTP_UPDATE_OK:
            LOG_I("HTTP_UPDATE_OK");
            networkManager.publishMessage(statusTopic, "OTA update successful");
            return true;
    }
    
    return false;
}

bool OTAManager::verifySignature(const uint8_t* hash, size_t hashLen, 
                                const uint8_t* signature, size_t sigLen) {
    mbedtls_pk_context pk;
    mbedtls_pk_init(&pk);
    
    int ret = mbedtls_pk_parse_public_key(&pk, publicKey, keyLen);
    if (ret != 0) {
        mbedtls_pk_free(&pk);
        return false;
    }
    
    ret = mbedtls_pk_verify(&pk, MBEDTLS_MD_SHA256, hash, hashLen, signature, sigLen);
    mbedtls_pk_free(&pk);
    
    return ret == 0;
}

bool OTAManager::performUpdateFromMqtt(const char* url, const char* version,
                                      const char* checksum, size_t size) {
    (void)version;
    (void)checksum;
    (void)size;
    if (!url || !url[0]) return false;
    reportOtaStatus("in_progress", 0);

    class MbedContexts {
    public:
        mbedtls_pk_context pkContext;
        mbedtls_md_context_t mdContext;
        MbedContexts() {
            mbedtls_pk_init(&pkContext);
            mbedtls_md_init(&mdContext);
        }
        ~MbedContexts() {
            mbedtls_md_free(&mdContext);
            mbedtls_pk_free(&pkContext);
        }
    } contexts;

    HTTPClient sigHttp;
    sigHttp.begin(signatureUrl);
    sigHttp.setTimeout(30000);
    int httpCode = sigHttp.GET();
    if (httpCode != HTTP_CODE_OK) {
        reportOtaStatus("failed", 0);
        return false;
    }
    size_t signatureSize = sigHttp.getSize();
    std::unique_ptr<uint8_t[]> signature(new uint8_t[signatureSize]);
    if (!signature) {
        reportOtaStatus("failed", 0);
        return false;
    }
    sigHttp.getStreamPtr()->readBytes(signature.get(), signatureSize);
    sigHttp.end();

    if (mbedtls_pk_parse_public_key(&contexts.pkContext, publicKey, keyLen) != 0 ||
        mbedtls_md_setup(&contexts.mdContext, mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), 0) != 0) {
        reportOtaStatus("failed", 0);
        return false;
    }

    HTTPClient firmwareHttp;
    firmwareHttp.begin(url);
    firmwareHttp.setTimeout(60000);
    httpCode = firmwareHttp.GET();
    if (httpCode != HTTP_CODE_OK) {
        reportOtaStatus("failed", 0);
        return false;
    }
    WiFiClient* client = firmwareHttp.getStreamPtr();
    client->setTimeout(60000);
    const size_t bufSize = 4096;
    std::unique_ptr<uint8_t[]> buf(new uint8_t[bufSize]);
    if (!buf) {
        reportOtaStatus("failed", 0);
        return false;
    }
    int totalBytes = firmwareHttp.getSize();
    int remainingBytes = totalBytes;
    int lastProgress = 0;
    const int maxRetries = 3;

    mbedtls_md_starts(&contexts.mdContext);
    while (remainingBytes > 0) {
        int progress = totalBytes > 0 ? ((totalBytes - remainingBytes) * 100) / totalBytes : 0;
        if (progress >= lastProgress + 10) {
            lastProgress = progress;
            reportOtaStatus("in_progress", progress);
            delay(10);
        }
        size_t bytesToRead = (remainingBytes > bufSize) ? bufSize : remainingBytes;
        size_t bytesRead = 0;
        int retries = 0;
        while (bytesRead == 0 && retries < maxRetries) {
            yield();
            bytesRead = client->readBytes(buf.get(), bytesToRead);
            if (bytesRead == 0) { retries++; delay(100 * retries); }
        }
        if (bytesRead == 0) {
            reportOtaStatus("failed", 0);
            return false;
        }
        mbedtls_md_update(&contexts.mdContext, buf.get(), bytesRead);
        remainingBytes -= bytesRead;
    }
    reportOtaStatus("in_progress", 100);
    uint8_t hash[32];
    mbedtls_md_finish(&contexts.mdContext, hash);
    if (mbedtls_pk_verify(&contexts.pkContext, MBEDTLS_MD_SHA256, hash, sizeof(hash),
                          signature.get(), signatureSize) != 0) {
        reportOtaStatus("failed", 0);
        return false;
    }
    firmwareHttp.end();

    firmwareHttp.begin(url);
    firmwareHttp.setTimeout(120000);
    client = firmwareHttp.getStreamPtr();
    client->setTimeout(120000);
    t_httpUpdate_return ret = httpUpdate.update(*client, url);
    firmwareHttp.end();
    if (ret == HTTP_UPDATE_OK) {
        reportOtaStatus("completed", 100);
        return true;
    }
    reportOtaStatus("failed", 0);
    return false;
}

bool OTAManager::performRollback() {
    esp_err_t err = esp_ota_mark_app_invalid_rollback_and_reboot();
    return (err == ESP_OK);
} 