#pragma once

struct Airline {
    const char* icao;   // 3-letter ICAO designator
    const char* name;   // Short display name
};

static const Airline AIRLINES[] = {
    // ── US / Canada ─────────────────────────────────────────────────────
    {"AAL", "American"},
    {"DAL", "Delta"},
    {"UAL", "United"},
    {"SWA", "Southwest"},
    {"JBU", "JetBlue"},
    {"NKS", "Spirit"},
    {"FFT", "Frontier"},
    {"ASA", "Alaska"},
    {"HAL", "Hawaiian"},
    {"SKW", "SkyWest"},
    {"RPA", "Republic"},
    {"ENY", "Envoy Air"},
    {"PDT", "Piedmont"},
    {"PSA", "PSA Airlines"},
    {"MES", "Mesa"},
    {"ACA", "Air Canada"},
    {"WJA", "WestJet"},

    // ── Europe ──────────────────────────────────────────────────────────
    {"BAW", "British Airways"},
    {"DLH", "Lufthansa"},
    {"AFR", "Air France"},
    {"KLM", "KLM"},
    {"EZY", "easyJet"},
    {"RYR", "Ryanair"},
    {"VLG", "Vueling"},
    {"IBE", "Iberia"},
    {"SAS", "SAS"},
    {"FIN", "Finnair"},
    {"AZA", "ITA Airways"},
    {"SWR", "Swiss"},
    {"AUA", "Austrian"},
    {"TAP", "TAP Portugal"},
    {"THA", "THAI"},
    {"LOT", "LOT Polish"},
    {"CSA", "Czech Airlines"},
    {"AEE", "Aegean"},
    {"THY", "Turkish"},
    {"AFL", "Aeroflot"},
    {"WZZ", "Wizz Air"},
    {"EWG", "Eurowings"},
    {"BEL", "Brussels"},
    {"NAX", "Norwegian"},
    {"ICE", "Icelandair"},
    {"EIN", "Aer Lingus"},

    // ── Middle East / Africa ────────────────────────────────────────────
    {"UAE", "Emirates"},
    {"ETD", "Etihad"},
    {"QTR", "Qatar"},
    {"SVA", "Saudia"},
    {"MEA", "MEA"},
    {"ELY", "El Al"},
    {"MSR", "EgyptAir"},
    {"ETH", "Ethiopian"},
    {"SAA", "South African"},
    {"RAM", "Royal Air Maroc"},
    {"KQA", "Kenya Airways"},

    // ── Asia-Pacific ────────────────────────────────────────────────────
    {"CPA", "Cathay Pacific"},
    {"SIA", "Singapore"},
    {"MAS", "Malaysia"},
    {"ANA", "ANA"},
    {"JAL", "JAL"},
    {"KAL", "Korean Air"},
    {"AAR", "Asiana"},
    {"CCA", "Air China"},
    {"CES", "China Eastern"},
    {"CSN", "China Southern"},
    {"HDA", "Hainan"},
    {"GIA", "Garuda"},
    {"PAL", "Philippine"},
    {"VNM", "Vietnam"},
    {"AIQ", "AirAsia"},
    {"EVA", "EVA Air"},
    {"CAL", "China Airlines"},

    // ── Oceania ─────────────────────────────────────────────────────────
    {"QFA", "Qantas"},
    {"ANZ", "Air New Zealand"},
    {"VOZ", "Virgin Aus"},
    {"JST", "Jetstar"},

    // ── Latin America ───────────────────────────────────────────────────
    {"TAM", "LATAM Brasil"},
    {"LAN", "LATAM Chile"},
    {"AVA", "Avianca"},
    {"CMP", "Copa"},
    {"AMX", "Aeromexico"},
    {"VIV", "VivaAerobus"},
    {"GLO", "GOL"},

    // ── Cargo ───────────────────────────────────────────────────────────
    {"FDX", "FedEx"},
    {"UPS", "UPS"},
    {"GTI", "Atlas Air"},
    {"ABX", "ABX Air"},
    {"CLX", "Cargolux"},
};

static const int AIRLINE_COUNT = sizeof(AIRLINES) / sizeof(AIRLINES[0]);

inline const char* findAirlineName(const char* icao_prefix) {
    for (int i = 0; i < AIRLINE_COUNT; i++) {
        if (strcmp(AIRLINES[i].icao, icao_prefix) == 0) return AIRLINES[i].name;
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
