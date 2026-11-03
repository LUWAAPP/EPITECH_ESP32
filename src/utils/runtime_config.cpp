#include "runtime_config.h"
#include "config_manager.h"
#include "secrets.h"
#include <Preferences.h>

const char* RuntimeConfig::NVS_DEVICE = "device";
const char* RuntimeConfig::NVS_PARAMS = "params";
const char* RuntimeConfig::KEY_GEOHASH = "geohash";
const char* RuntimeConfig::KEY_BOOT_COUNT = "boot_count";
const char* RuntimeConfig::KEY_PARAMS_JSON = "p";

void RuntimeConfig::begin() {
    incrementBootCount();
}

String RuntimeConfig::getGeohash() const {
    Preferences prefs;
    if (!prefs.begin(NVS_DEVICE, true)) return String(GEOHASH_DEFAULT);
    String g;
    if (prefs.isKey(KEY_GEOHASH)) g = prefs.getString(KEY_GEOHASH, "");
    prefs.end();
    if (g.length() > 0) return g;
    return String(GEOHASH_DEFAULT);
}

void RuntimeConfig::setGeohash(const char* geohash) {
    if (!geohash) return;
    Preferences prefs;
    if (!prefs.begin(NVS_DEVICE, false)) return;
    prefs.putString(KEY_GEOHASH, geohash);
    prefs.end();
}

String RuntimeConfig::getTeleTopic() const {
    String g = getGeohash();
    const char* sid = Config().getSensorId();
    return String("tele/") + g + "/" + sid;
}

bool RuntimeConfig::getParameter(const char* key, float* out) const {
    if (!out) return false;
    Preferences prefs;
    if (!prefs.begin(NVS_PARAMS, false)) return false;
    if (!prefs.isKey(KEY_PARAMS_JSON)) { prefs.end(); return false; }
    String json = prefs.getString(KEY_PARAMS_JSON, "{}");
    prefs.end();
    DynamicJsonDocument doc(512);
    if (deserializeJson(doc, json) != DeserializationError::Ok) return false;
    JsonObject o = doc.as<JsonObject>();
    if (!o.containsKey(key)) return false;
    *out = o[key].as<float>();
    return true;
}

void RuntimeConfig::setParameter(const char* key, float value) {
    Preferences prefs;
    if (!prefs.begin(NVS_PARAMS, false)) return;
    String json = prefs.isKey(KEY_PARAMS_JSON) ? prefs.getString(KEY_PARAMS_JSON, "{}") : String("{}");
    DynamicJsonDocument doc(512);
    deserializeJson(doc, json);
    doc[key] = value;
    String out;
    serializeJson(doc.as<JsonObject>(), out);
    prefs.putString(KEY_PARAMS_JSON, out);
    prefs.end();
}

void RuntimeConfig::setParametersFromJson(JsonObject params) {
    Preferences prefs;
    if (!prefs.begin(NVS_PARAMS, false)) return;
    String json = prefs.isKey(KEY_PARAMS_JSON) ? prefs.getString(KEY_PARAMS_JSON, "{}") : String("{}");
    DynamicJsonDocument doc(512);
    deserializeJson(doc, json);
    JsonObject o = doc.as<JsonObject>();
    for (JsonPair kv : params) {
        if (kv.value().is<float>()) o[kv.key()] = kv.value().as<float>();
        else if (kv.value().is<double>()) o[kv.key()] = (float)kv.value().as<double>();
        else if (kv.value().is<int>()) o[kv.key()] = (float)kv.value().as<int>();
    }
    String out;
    serializeJson(o, out);
    prefs.putString(KEY_PARAMS_JSON, out);
    prefs.end();
}

void RuntimeConfig::getParametersJson(JsonObject out) const {
    Preferences prefs;
    if (!prefs.begin(NVS_PARAMS, false)) return;
    if (!prefs.isKey(KEY_PARAMS_JSON)) { prefs.end(); return; }
    String json = prefs.getString(KEY_PARAMS_JSON, "{}");
    prefs.end();
    DynamicJsonDocument doc(512);
    if (deserializeJson(doc, json) != DeserializationError::Ok) return;
    for (JsonPair kv : doc.as<JsonObject>())
        out[kv.key()] = kv.value().as<float>();
}

int RuntimeConfig::getBootCount() const {
    Preferences prefs;
    if (!prefs.begin(NVS_DEVICE, true)) return 0;
    int c = prefs.getInt(KEY_BOOT_COUNT, 0);
    prefs.end();
    return c;
}

void RuntimeConfig::incrementBootCount() {
    Preferences prefs;
    if (!prefs.begin(NVS_DEVICE, false)) return;
    int c = prefs.getInt(KEY_BOOT_COUNT, 0);
    prefs.putInt(KEY_BOOT_COUNT, c + 1);
    prefs.end();
}
