#pragma once
#include <Arduino.h>
#include <math.h>

struct Flight {
    String icao24;
    String callsign;
    String origin_country;
    float longitude;
    float latitude;
    float altitude;      // meters (barometric)
    float velocity;      // m/s ground speed
    float heading;       // degrees clockwise from north
    bool on_ground;
    float distance_km;   // computed from observer position

    // Route info (fetched separately)
    String origin_icao;      // e.g. "KJFK"
    String destination_icao; // e.g. "EGLL"
    float origin_lat, origin_lon;
    float dest_lat, dest_lon;
    bool route_fetched;
};

// Haversine distance between two lat/lon points in km
inline float haversine_km(float lat1, float lon1, float lat2, float lon2) {
    constexpr float R = 6371.0f;
    float dLat = radians(lat2 - lat1);
    float dLon = radians(lon2 - lon1);
    float a = sinf(dLat / 2) * sinf(dLat / 2) +
              cosf(radians(lat1)) * cosf(radians(lat2)) *
              sinf(dLon / 2) * sinf(dLon / 2);
    float c = 2.0f * atan2f(sqrtf(a), sqrtf(1.0f - a));
    return R * c;
}

inline const char* heading_to_compass(float deg) {
    if (deg < 0) return "?";
    int idx = (int)((deg + 22.5f) / 45.0f) % 8;
    static const char* dirs[] = {"N", "NE", "E", "SE", "S", "SW", "W", "NW"};
    return dirs[idx];
}

inline float ms_to_knots(float ms) { return ms * 1.94384f; }
inline float m_to_feet(float m)    { return m * 3.28084f; }

// Interpolate a point along a great circle path (fraction 0..1)
inline void interpolate_great_circle(float lat1, float lon1, float lat2, float lon2,
                                     float frac, float& out_lat, float& out_lon) {
    float rlat1 = radians(lat1), rlon1 = radians(lon1);
    float rlat2 = radians(lat2), rlon2 = radians(lon2);
    float d = 2.0f * asinf(sqrtf(
        powf(sinf((rlat2 - rlat1) / 2), 2) +
        cosf(rlat1) * cosf(rlat2) * powf(sinf((rlon2 - rlon1) / 2), 2)));
    if (d < 0.0001f) {
        out_lat = lat1; out_lon = lon1; return;
    }
    float A = sinf((1.0f - frac) * d) / sinf(d);
    float B = sinf(frac * d) / sinf(d);
    float x = A * cosf(rlat1) * cosf(rlon1) + B * cosf(rlat2) * cosf(rlon2);
    float y = A * cosf(rlat1) * sinf(rlon1) + B * cosf(rlat2) * sinf(rlon2);
    float z = A * sinf(rlat1) + B * sinf(rlat2);
    out_lat = degrees(atan2f(z, sqrtf(x * x + y * y)));
    out_lon = degrees(atan2f(y, x));
}
