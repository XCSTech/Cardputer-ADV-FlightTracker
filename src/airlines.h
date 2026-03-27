#pragma once

#include <vector>
#include <LittleFS.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>

// OpenFlights airlines.dat — CSV (no header):
// id,name,alias,iata,icao,callsign,country,active

#define AIRLINES_URL "https://raw.githubusercontent.com/jpatokal/openflights/master/data/airlines.dat"
#define AIRLINES_CACHE "/airlines.csv"

struct Airline {
    char icao[4];   // 3-letter ICAO designator
    char name[24];  // Short display name
};

static std::vector<Airline> airlines;

// Parse a possibly-quoted CSV field, advance pos past the delimiter
static inline String parseCSVField(const String& line, int& pos) {
    if (pos >= (int)line.length()) return "";
    String result;
    if (line[pos] == '"') {
        pos++;
        while (pos < (int)line.length()) {
            if (line[pos] == '"') {
                pos++;
                if (pos < (int)line.length() && line[pos] == '"') {
                    result += '"';
                    pos++;
                } else break;
            } else {
                result += line[pos++];
            }
        }
        if (pos < (int)line.length() && line[pos] == ',') pos++;
    } else {
        int start = pos;
        while (pos < (int)line.length() && line[pos] != ',') pos++;
        result = line.substring(start, pos);
        if (pos < (int)line.length()) pos++;
    }
    return result;
}

// Load from cached LittleFS file (icao,name per line)
inline bool loadAirlinesFromFile() {
    airlines.clear();
    File f = LittleFS.open(AIRLINES_CACHE, "r");
    if (!f) return false;

    while (f.available()) {
        String line = f.readStringUntil('\n');
        line.trim();
        if (line.length() == 0) continue;
        int comma = line.indexOf(',');
        if (comma < 0) continue;

        Airline a;
        strncpy(a.icao, line.substring(0, comma).c_str(), sizeof(a.icao) - 1);
        a.icao[sizeof(a.icao) - 1] = '\0';
        strncpy(a.name, line.substring(comma + 1).c_str(), sizeof(a.name) - 1);
        a.name[sizeof(a.name) - 1] = '\0';
        airlines.push_back(a);
    }
    f.close();
    return airlines.size() > 0;
}

// Fetch from OpenFlights API — stream-parse, write cache, build vector in one pass
inline bool fetchAirlines() {
    airlines.clear();
    airlines.reserve(1500);

    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient http;
    http.setTimeout(15000);

    if (!http.begin(client, AIRLINES_URL)) return false;
    int code = http.GET();
    if (code != 200) { http.end(); return false; }

    File cache = LittleFS.open(AIRLINES_CACHE, "w");

    int total = http.getSize();
    int consumed = 0;
    WiFiClient* stream = http.getStreamPtr();

    while ((total == -1 || consumed < total) &&
           (stream->available() || stream->connected())) {

        if (!stream->available()) { delay(1); continue; }

        char buf[256];
        int len = 0;
        while (len < (int)sizeof(buf) - 1) {
            int c = stream->read();
            if (c < 0) break;
            consumed++;
            if (c == '\n') break;
            buf[len++] = (char)c;
        }
        buf[len] = '\0';
        if (len == 0) continue;

        String line(buf);
        int pos = 0;
        parseCSVField(line, pos);                        // 0: id
        String name = parseCSVField(line, pos);          // 1: name
        parseCSVField(line, pos);                        // 2: alias
        parseCSVField(line, pos);                        // 3: iata
        String icao = parseCSVField(line, pos);          // 4: icao
        parseCSVField(line, pos);                        // 5: callsign
        parseCSVField(line, pos);                        // 6: country
        String active = parseCSVField(line, pos);        // 7: active

        if (icao.length() != 3 || icao == "\\N" || icao == "-") continue;
        if (active != "Y") continue;

        Airline a;
        strncpy(a.icao, icao.c_str(), sizeof(a.icao) - 1);
        a.icao[sizeof(a.icao) - 1] = '\0';
        strncpy(a.name, name.c_str(), sizeof(a.name) - 1);
        a.name[sizeof(a.name) - 1] = '\0';
        airlines.push_back(a);

        if (cache) {
            cache.print(a.icao);
            cache.print(',');
            cache.println(a.name);
        }
    }
    if (cache) cache.close();
    http.end();
    return airlines.size() > 0;
}

inline int getAirlineCount() { return (int)airlines.size(); }

inline const char* findAirlineName(const char* icao_prefix) {
    for (auto& a : airlines) {
        if (strcmp(a.icao, icao_prefix) == 0) return a.name;
    }
    return nullptr;
}

// Extract 3-letter ICAO airline code from callsign (e.g. "UAL1234" -> "UAL")
inline void extractCarrierCode(const String& callsign, char* out, int maxlen) {
    int j = 0;
    for (int i = 0; i < (int)callsign.length() && j < maxlen - 1; i++) {
        char c = callsign[i];
        if (isAlpha(c)) out[j++] = c;
        else break;
    }
    out[j] = '\0';
}
