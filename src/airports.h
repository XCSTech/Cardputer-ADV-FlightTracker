#pragma once

struct Airport {
    const char* icao;
    const char* city;
    float lat;
    float lon;
};

static const Airport AIRPORTS[] = {
    // ── North America ───────────────────────────────────────────────────
    {"KATL", "Atlanta",        33.6367, -84.4281},
    {"KBOS", "Boston",         42.3656, -71.0096},
    {"KBWI", "Baltimore",      39.1754, -76.6683},
    {"KCLT", "Charlotte",      35.2140, -80.9431},
    {"KCVG", "Cincinnati",     39.0488, -84.6678},
    {"KDAL", "Dallas Love",    32.8471, -96.8518},
    {"KDEN", "Denver",         39.8561,-104.6737},
    {"KDFW", "Dallas/FW",      32.8968, -97.0380},
    {"KDTW", "Detroit",        42.2124, -83.3534},
    {"KEWR", "Newark",         40.6925, -74.1687},
    {"KFLL", "Ft Lauderdale",  26.0726, -80.1527},
    {"KHOU", "Houston Hobby",  29.6454, -95.2789},
    {"KIAH", "Houston IAH",    29.9844, -95.3414},
    {"KIND", "Indianapolis",   39.7173, -86.2944},
    {"KJFK", "New York JFK",   40.6413, -73.7781},
    {"KLAS", "Las Vegas",      36.0840,-115.1537},
    {"KLAX", "Los Angeles",    33.9425,-118.4081},
    {"KLGA", "New York LGA",   40.7769, -73.8740},
    {"KMCI", "Kansas City",    39.2976, -94.7139},
    {"KMCO", "Orlando",        28.4294, -81.3089},
    {"KMDW", "Chicago Midway", 41.7868, -87.7522},
    {"KMEM", "Memphis",        35.0424, -89.9767},
    {"KMIA", "Miami",          25.7959, -80.2870},
    {"KMSP", "Minneapolis",    44.8848, -93.2223},
    {"KOAK", "Oakland",        37.7213,-122.2208},
    {"KORD", "Chicago O'Hare", 41.9742, -87.9073},
    {"KPDX", "Portland",       45.5898,-122.5951},
    {"KPHL", "Philadelphia",   39.8744, -75.2424},
    {"KPHX", "Phoenix",        33.4373,-112.0078},
    {"KPIT", "Pittsburgh",     40.4915, -80.2329},
    {"KRDU", "Raleigh",        35.8776, -78.7875},
    {"KSAN", "San Diego",      32.7336,-117.1897},
    {"KSAT", "San Antonio",    29.5337, -98.4698},
    {"KSEA", "Seattle",        47.4502,-122.3088},
    {"KSFO", "San Francisco",  37.6213,-122.3790},
    {"KSLC", "Salt Lake City", 40.7884,-111.9778},
    {"KSMF", "Sacramento",     38.6954,-121.5908},
    {"KSTL", "St. Louis",      38.7487, -90.3700},
    {"KTPA", "Tampa",          27.9755, -82.5332},
    {"PANC", "Anchorage",      61.1744,-149.9964},
    {"PHNL", "Honolulu",       21.3187,-157.9224},
    {"CYUL", "Montreal",       45.4706, -73.7408},
    {"CYYZ", "Toronto",        43.6777, -79.6248},
    {"CYVR", "Vancouver",      49.1947,-123.1839},
    {"CYOW", "Ottawa",         45.3225, -75.6692},
    {"CYWG", "Winnipeg",       49.9100, -97.2399},
    {"MMMX", "Mexico City",    19.4363, -99.0721},
    {"MMUN", "Cancun",         21.0365, -86.8771},

    // ── Europe ──────────────────────────────────────────────────────────
    {"EGLL", "London LHR",     51.4700,  -0.4543},
    {"EGKK", "London Gatwick", 51.1537,  -0.1821},
    {"EGLC", "London City",    51.5048,   0.0495},
    {"EGSS", "London Stansted",51.8860,   0.2389},
    {"LFPG", "Paris CDG",      49.0097,   2.5479},
    {"LFPO", "Paris Orly",     48.7233,   2.3794},
    {"EDDF", "Frankfurt",      50.0379,   8.5622},
    {"EDDM", "Munich",         48.3538,  11.7861},
    {"EDDB", "Berlin",         52.3667,  13.5033},
    {"EHAM", "Amsterdam",      52.3105,   4.7683},
    {"LEMD", "Madrid",         40.4936,  -3.5668},
    {"LEBL", "Barcelona",      41.2971,   2.0785},
    {"LIRF", "Rome FCO",       41.8003,  12.2389},
    {"LIMC", "Milan MXP",      45.6306,   8.7231},
    {"LSZH", "Zurich",         47.4647,   8.5492},
    {"LOWW", "Vienna",         48.1103,  16.5697},
    {"EKCH", "Copenhagen",     55.6180,  12.6508},
    {"ESSA", "Stockholm",      59.6519,  17.9186},
    {"ENGM", "Oslo",           60.1939,  11.1004},
    {"EFHK", "Helsinki",       60.3172,  24.9633},
    {"EPWA", "Warsaw",         52.1657,  20.9671},
    {"LKPR", "Prague",         50.1008,  14.2600},
    {"EIDW", "Dublin",         53.4213,  -6.2701},
    {"LPPT", "Lisbon",         38.7813,  -9.1359},
    {"LHBP", "Budapest",       47.4369,  19.2556},
    {"LGAV", "Athens",         37.9364,  23.9445},
    {"LTFM", "Istanbul",       41.2753,  28.7519},
    {"UUEE", "Moscow SVO",     55.9726,  37.4146},
    {"UUDD", "Moscow DME",     55.4088,  37.9063},

    // ── Middle East ─────────────────────────────────────────────────────
    {"OMDB", "Dubai",          25.2528,  55.3644},
    {"OMAA", "Abu Dhabi",      24.4330,  54.6511},
    {"OEJN", "Jeddah",         21.6706,  39.1505},
    {"OERK", "Riyadh",         24.9578,  46.6989},
    {"OTHH", "Doha",           25.2731,  51.6081},
    {"LLBG", "Tel Aviv",       32.0114,  34.8867},
    {"OIIE", "Tehran",         35.4161,  51.1522},

    // ── Asia ────────────────────────────────────────────────────────────
    {"VHHH", "Hong Kong",      22.3080, 113.9185},
    {"ZSSS", "Shanghai SHA",   31.1434, 121.8052},
    {"ZSPD", "Shanghai PVG",   31.1443, 121.8083},
    {"ZBAA", "Beijing",        40.0799, 116.6031},
    {"ZGSZ", "Shenzhen",       22.6393, 113.8107},
    {"ZGGG", "Guangzhou",      23.3924, 113.2988},
    {"RJTT", "Tokyo HND",     35.5523, 139.7798},
    {"RJAA", "Tokyo NRT",     35.7647, 140.3864},
    {"RJBB", "Osaka KIX",     34.4347, 135.2440},
    {"RKSI", "Seoul ICN",     37.4691, 126.4505},
    {"VTBS", "Bangkok",        13.6900, 100.7501},
    {"WSSS", "Singapore",       1.3502, 103.9944},
    {"WMKK", "Kuala Lumpur",    2.7456, 101.7099},
    {"WIII", "Jakarta",        -6.1256, 106.6559},
    {"VABB", "Mumbai",         19.0887,  72.8679},
    {"VIDP", "Delhi",          28.5562,  77.1000},
    {"RPLL", "Manila",         14.5086, 121.0198},
    {"VTSP", "Phuket",          8.1132,  98.3169},

    // ── Oceania ─────────────────────────────────────────────────────────
    {"YSSY", "Sydney",        -33.9461, 151.1772},
    {"YMML", "Melbourne",     -37.6733, 144.8433},
    {"YBBN", "Brisbane",      -27.3842, 153.1175},
    {"NZAA", "Auckland",      -37.0082, 174.7850},

    // ── South America ───────────────────────────────────────────────────
    {"SBGR", "Sao Paulo GRU", -23.4356, -46.4731},
    {"SBGL", "Rio de Janeiro",-22.8100, -43.2506},
    {"SCEL", "Santiago",       -33.3930, -70.7858},
    {"SKBO", "Bogota",          4.7016, -74.1469},
    {"SEQM", "Quito",          -0.1292, -78.3575},
    {"SPJC", "Lima",           -12.0219, -77.1143},
    {"SAEZ", "Buenos Aires",   -34.8222, -58.5358},

    // ── Africa ──────────────────────────────────────────────────────────
    {"FAOR", "Johannesburg",   -26.1392,  28.2460},
    {"HECA", "Cairo",           30.1219,  31.4056},
    {"GMMN", "Casablanca",      33.3675,  -7.5898},
    {"DNMM", "Lagos",            6.5774,   3.3213},
    {"HKJK", "Nairobi",         -1.3192,  36.9278},
    {"HAAB", "Addis Ababa",      8.9779,  38.7994},
    {"FACT", "Cape Town",       -33.9649,  18.6017},
};

static const int AIRPORT_COUNT = sizeof(AIRPORTS) / sizeof(AIRPORTS[0]);

inline const Airport* findAirport(const char* icao) {
    for (int i = 0; i < AIRPORT_COUNT; i++) {
        if (strcmp(AIRPORTS[i].icao, icao) == 0) return &AIRPORTS[i];
    }
    return nullptr;
}

// Search airports by partial city name (case-insensitive)
inline int searchAirports(const char* query, const Airport** results, int max_results) {
    int count = 0;
    int qlen = strlen(query);
    for (int i = 0; i < AIRPORT_COUNT && count < max_results; i++) {
        const char* city = AIRPORTS[i].city;
        // Case-insensitive substring search
        for (int j = 0; city[j] && count < max_results; j++) {
            bool match = true;
            for (int k = 0; k < qlen && match; k++) {
                if (tolower(city[j + k]) != tolower(query[k])) match = false;
            }
            if (match) { results[count++] = &AIRPORTS[i]; break; }
        }
    }
    return count;
}
