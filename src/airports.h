#pragma once

#include <vector>
#include <LittleFS.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>

// OpenFlights airports.dat — CSV (no header):
// id,name,city,country,iata,icao,lat,lon,altitude,timezone,dst,tz_name,type,source

#define AIRPORTS_URL "https://raw.githubusercontent.com/jpatokal/openflights/master/data/airports.dat"
#define AIRPORTS_CACHE "/airports.csv"
#define MAX_AIRPORTS_STORED 2000

struct Airport {
    char icao[5];
    char city[20];
    float lat;
    float lon;
};

static std::vector<Airport> airports;

// Forward-declare from airlines.h
static inline String parseCSVField(const String& line, int& pos);

// Load from cached LittleFS file (icao,city,lat,lon per line)
inline bool loadAirportsFromFile() {
    airports.clear();
    File f = LittleFS.open(AIRPORTS_CACHE, "r");
    if (!f) return false;

    while (f.available()) {
        String line = f.readStringUntil('\n');
        line.trim();
        if (line.length() == 0) continue;

        int c1 = line.indexOf(',');
        if (c1 < 0) continue;
        int c2 = line.indexOf(',', c1 + 1);
        if (c2 < 0) continue;
        int c3 = line.indexOf(',', c2 + 1);
        if (c3 < 0) continue;

        Airport a;
        strncpy(a.icao, line.substring(0, c1).c_str(), sizeof(a.icao) - 1);
        a.icao[sizeof(a.icao) - 1] = '\0';
        strncpy(a.city, line.substring(c1 + 1, c2).c_str(), sizeof(a.city) - 1);
        a.city[sizeof(a.city) - 1] = '\0';
        a.lat = line.substring(c2 + 1, c3).toFloat();
        a.lon = line.substring(c3 + 1).toFloat();
        airports.push_back(a);
    }
    f.close();
    return airports.size() > 0;
}

// Fetch from OpenFlights API — stream-parse, filter to commercial airports,
// write cache file, and build vector in one pass (no large intermediate file)
inline bool fetchAirports() {
    airports.clear();
    airports.reserve(MAX_AIRPORTS_STORED);

    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient http;
    http.setTimeout(15000);

    Serial.printf("[AP] Free heap before connect: %d\n", ESP.getFreeHeap());
    if (!http.begin(client, AIRPORTS_URL)) return false;
    int code = http.GET();
    if (code != 200) { http.end(); return false; }

    File cache = LittleFS.open(AIRPORTS_CACHE, "w");

    int total = http.getSize();
    int consumed = 0;
    WiFiClient* stream = http.getStreamPtr();
    // Read line buffer — built char-by-char to avoid large String allocations
    while ((int)airports.size() < MAX_AIRPORTS_STORED &&
           (total == -1 || consumed < total) &&
           (stream->available() || stream->connected())) {

        if (!stream->available()) { delay(1); continue; }

        // Read one line
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
        parseCSVField(line, pos);                        // 1: name
        String city = parseCSVField(line, pos);          // 2: city
        parseCSVField(line, pos);                        // 3: country
        String iata = parseCSVField(line, pos);          // 4: iata
        String icao = parseCSVField(line, pos);          // 5: icao
        String slat = parseCSVField(line, pos);          // 6: lat
        String slon = parseCSVField(line, pos);          // 7: lon

        // Only keep commercial airports (have both ICAO and IATA codes)
        if (icao.length() != 4 || icao == "\\N") continue;
        if (iata.length() != 3 || iata == "\\N" || iata == "-") continue;

        Airport a;
        strncpy(a.icao, icao.c_str(), sizeof(a.icao) - 1);
        a.icao[sizeof(a.icao) - 1] = '\0';
        strncpy(a.city, city.c_str(), sizeof(a.city) - 1);
        a.city[sizeof(a.city) - 1] = '\0';
        a.lat = slat.toFloat();
        a.lon = slon.toFloat();
        airports.push_back(a);

        // Write to cache simultaneously
        if (cache) {
            cache.print(a.icao);
            cache.print(',');
            cache.print(a.city);
            cache.print(',');
            cache.print(a.lat, 4);
            cache.print(',');
            cache.println(a.lon, 4);
        }
    }
    if (cache) cache.close();
    http.end();

    Serial.printf("[AP] Loaded %d airports, heap: %d\n", airports.size(), ESP.getFreeHeap());
    return airports.size() > 0;
}

inline int getAirportCount() { return (int)airports.size(); }

inline const Airport* getAirport(int index) {
    if (index < 0 || index >= (int)airports.size()) return nullptr;
    return &airports[index];
}

inline const Airport* findAirport(const char* icao) {
    for (auto& a : airports) {
        if (strcmp(a.icao, icao) == 0) return &a;
    }
    return nullptr;
}

// Search airports by partial city name (case-insensitive)
inline int searchAirports(const char* query, const Airport** results, int max_results) {
    int count = 0;
    int qlen = strlen(query);
    for (int i = 0; i < (int)airports.size() && count < max_results; i++) {
        const char* city = airports[i].city;
        for (int j = 0; city[j] && count < max_results; j++) {
            bool match = true;
            for (int k = 0; k < qlen && match; k++) {
                if (tolower(city[j + k]) != tolower(query[k])) match = false;
            }
            if (match) { results[count++] = &airports[i]; break; }
        }
    }
    return count;
}
