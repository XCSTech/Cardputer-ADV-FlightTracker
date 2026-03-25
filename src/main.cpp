#include <M5Cardputer.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <algorithm>
#include <vector>
#include <map>

#include "config.h"
#include "flight.h"
#include "airports.h"
#include "airlines.h"
#include "map_data.h"

// ── Colors ──────────────────────────────────────────────────────────────
#define C_BG       TFT_BLACK
#define C_HEADER   0x1A3A
#define C_TEXT     TFT_WHITE
#define C_DIM      TFT_DARKGREY
#define C_ACCENT   0x07FF
#define C_GROUND   0xFDA0
#define C_AIRBORNE 0x07E0
#define C_ROUTE    0xF81F
#define C_LAND     0x2945
#define C_OCEAN    0x0011
#define C_PLANE    TFT_YELLOW
#define C_ORIGIN   0x07E0
#define C_DEST     0xF800
#define C_SEL      0x2104

// ── Views ───────────────────────────────────────────────────────────────
enum AppView {
    VIEW_MENU,       // main search menu
    VIEW_CITY_PICK,  // city/airport selector
    VIEW_CARRIER_PICK,// carrier selector
    VIEW_LIST,       // flight list
    VIEW_DETAIL,     // flight detail
    VIEW_MAP         // route map
};
static AppView current_view = VIEW_MENU;

// ── Search mode that produced the current list ──────────────────────────
enum SearchMode { SEARCH_NEAR, SEARCH_LONG, SEARCH_CITY, SEARCH_CARRIER, SEARCH_TEXT };
static SearchMode search_mode = SEARCH_NEAR;

// ── Global state ────────────────────────────────────────────────────────
static float observer_lat = DEFAULT_LAT;
static float observer_lon = DEFAULT_LON;

static std::vector<Flight> flights;     // current filtered list
static std::vector<Flight> all_flights; // all fetched (for carrier/search filtering)
static bool wifi_connected = false;

// List nav
static int cursor = 0;
static int scroll_offset = 0;
static int selected_flight = -1;
static bool show_routes = false;

// Menu nav
static int menu_cursor = 0;

// City picker
static int city_cursor = 0;
static int city_scroll = 0;
static String city_filter = "";

// Carrier picker
struct CarrierInfo { char code[4]; const char* name; int count; };
static std::vector<CarrierInfo> carriers;
static int carrier_cursor = 0;
static int carrier_scroll = 0;
static String carrier_filter = "";

// Track which page of routes we last fetched
static int last_route_page = -1;

// Track where map was opened from (list vs detail)
static bool map_from_list = false;

// Map refresh
static unsigned long map_last_refresh = 0;

static Preferences prefs;

// ── Forward declarations ────────────────────────────────────────────────
// WiFi
void wifiScanAndConnect();
String keyboardInput(const char* prompt, bool is_password = false);
bool connectWiFi(const String& ssid, const String& pass);
void loadSavedWiFi();

// Data
bool fetchFlightsInArea(float lat, float lon, float radius);
void parseFlights(const String& json, float sort_lat, float sort_lon);
void fetchRoute(Flight& f);
void fetchVisibleRoutes();
bool refreshSelectedFlight();
void buildCarrierList();
void filterByCarrier(const char* code);
void filterByText(const String& query);

// Input handlers
void handleMenuInput(Keyboard_Class::KeysState& keys);
void handleCityPickInput(Keyboard_Class::KeysState& keys);
void handleCarrierPickInput(Keyboard_Class::KeysState& keys);
void handleListInput(Keyboard_Class::KeysState& keys);
void handleDetailInput(Keyboard_Class::KeysState& keys);
void handleMapInput(Keyboard_Class::KeysState& keys);

// Drawing
void drawHeader(const char* title, const char* right = nullptr);
void drawStatusBar(const char* msg);
void drawCentered(const char* text, int y, uint16_t color = C_TEXT);
void drawMenu();
void drawCityPicker();
void drawCarrierPicker();
void drawFlightList();
void drawFlightDetail();
void drawFlightMap();
void drawMiniMap(const Flight& f);
void drawAllFlightsMap();

// ════════════════════════════════════════════════════════════════════════
//  SETUP & LOOP
// ════════════════════════════════════════════════════════════════════════

void setup() {
    auto cfg = M5.config();
    M5Cardputer.begin(cfg);

    M5Cardputer.Display.setRotation(1);
    M5Cardputer.Display.fillScreen(C_BG);
    M5Cardputer.Display.setTextSize(1);
    M5Cardputer.Display.setTextColor(C_TEXT, C_BG);

    drawCentered("Flight Tracker", 30, C_ACCENT);
    drawCentered("v2.0", 45, C_DIM);
    delay(600);

    prefs.begin(NVS_NAMESPACE, false);
    loadSavedWiFi();

    if (!wifi_connected) {
        wifiScanAndConnect();
    }

    if (wifi_connected) {
        current_view = VIEW_MENU;
        drawMenu();
    }
}

void loop() {
    M5Cardputer.update();

    // Map auto-refresh (runs even without key presses)
    if (current_view == VIEW_MAP &&
        millis() - map_last_refresh >= MAP_REFRESH_MS) {
        if (map_from_list) {
            // Refresh all flights in the list by re-fetching area
            // (too expensive per-flight, just redraw with current data)
            drawAllFlightsMap();
        } else if (selected_flight >= 0) {
            if (refreshSelectedFlight()) {
                drawFlightMap();
            }
        }
        map_last_refresh = millis();
    }

    if (!M5Cardputer.Keyboard.isChange() || !M5Cardputer.Keyboard.isPressed()) {
        delay(50);
        return;
    }

    Keyboard_Class::KeysState keys = M5Cardputer.Keyboard.keysState();

    switch (current_view) {
    case VIEW_MENU:         handleMenuInput(keys); break;
    case VIEW_CITY_PICK:    handleCityPickInput(keys); break;
    case VIEW_CARRIER_PICK: handleCarrierPickInput(keys); break;
    case VIEW_LIST:         handleListInput(keys); break;
    case VIEW_DETAIL:       handleDetailInput(keys); break;
    case VIEW_MAP:          handleMapInput(keys); break;
    }
    delay(50);
}

// ════════════════════════════════════════════════════════════════════════
//  WIFI
// ════════════════════════════════════════════════════════════════════════

void loadSavedWiFi() {
    String ssid = prefs.getString("ssid", "");
    String pass = prefs.getString("pass", "");
    if (ssid.length() == 0) return;

    M5Cardputer.Display.fillScreen(C_BG);
    drawCentered("Saved WiFi...", 45, C_DIM);
    drawCentered(ssid.c_str(), 60, C_ACCENT);

    wifi_connected = connectWiFi(ssid, pass);
    if (wifi_connected) {
        drawCentered("Connected!", 80, C_AIRBORNE);
        delay(400);
    }
}

void wifiScanAndConnect() {
    M5Cardputer.Display.fillScreen(C_BG);
    drawHeader("WiFi Setup");
    drawCentered("Scanning...", 55, C_DIM);

    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    delay(200);
    int n = WiFi.scanNetworks();

    if (n <= 0) {
        drawCentered("No networks found", 55, TFT_RED);
        delay(1500);
        return;
    }

    struct NetInfo { String ssid; int rssi; bool open; };
    std::vector<NetInfo> nets;
    for (int i = 0; i < n; i++) {
        String s = WiFi.SSID(i);
        if (s.length() == 0) continue;
        bool dup = false;
        for (auto& ni : nets) if (ni.ssid == s) { dup = true; break; }
        if (dup) continue;
        nets.push_back({s, WiFi.RSSI(i), WiFi.encryptionType(i) == WIFI_AUTH_OPEN});
    }
    WiFi.scanDelete();
    std::sort(nets.begin(), nets.end(),
              [](const NetInfo& a, const NetInfo& b) { return a.rssi > b.rssi; });

    int sel = 0, scr = 0;
    const int vis = 6;

    auto drawNetMenu = [&]() {
        M5Cardputer.Display.fillScreen(C_BG);
        drawHeader("Select WiFi", String(String(nets.size()) + " found").c_str());
        int y = 18;
        int end = min(scr + vis, (int)nets.size());
        for (int i = scr; i < end; i++) {
            bool hl = (i == sel);
            if (hl) M5Cardputer.Display.fillRect(0, y - 1, SCREEN_W, 15, C_SEL);
            M5Cardputer.Display.setTextColor(hl ? C_ACCENT : C_TEXT, hl ? C_SEL : C_BG);
            M5Cardputer.Display.setCursor(4, y);
            int bars = (nets[i].rssi > -50) ? 4 : (nets[i].rssi > -65) ? 3 :
                       (nets[i].rssi > -75) ? 2 : 1;
            for (int b = 0; b < bars; b++) M5Cardputer.Display.print("|");
            for (int b = bars; b < 4; b++) M5Cardputer.Display.print(" ");
            M5Cardputer.Display.print(" ");
            M5Cardputer.Display.print(nets[i].ssid.substring(0, 22));
            if (nets[i].open) {
                M5Cardputer.Display.setTextColor(C_AIRBORNE, hl ? C_SEL : C_BG);
                M5Cardputer.Display.print(" [open]");
            }
            y += 16;
        }
        drawStatusBar(";.=scroll Enter=select");
    };

    drawNetMenu();

    while (true) {
        M5Cardputer.update();
        if (!M5Cardputer.Keyboard.isChange() || !M5Cardputer.Keyboard.isPressed()) {
            delay(50); continue;
        }
        Keyboard_Class::KeysState k = M5Cardputer.Keyboard.keysState();
        bool changed = false;
        for (auto key : k.word) {
            if ((key == ';' || key == ',') && sel > 0)
                { sel--; if (sel < scr) scr = sel; changed = true; }
            if ((key == '.' || key == '/') && sel < (int)nets.size() - 1)
                { sel++; if (sel >= scr + vis) scr = sel - vis + 1; changed = true; }
        }
        if (k.enter) {
            String ssid = nets[sel].ssid;
            String pass = "";
            if (!nets[sel].open) pass = keyboardInput("Password:", true);
            M5Cardputer.Display.fillScreen(C_BG);
            drawCentered("Connecting...", 50, C_DIM);
            drawCentered(ssid.c_str(), 65, C_ACCENT);
            if (connectWiFi(ssid, pass)) {
                wifi_connected = true;
                prefs.putString("ssid", ssid);
                prefs.putString("pass", pass);
                drawCentered("Connected!", 85, C_AIRBORNE);
                delay(500);
                return;
            }
            drawCentered("Failed!", 85, TFT_RED);
            delay(1000);
            drawNetMenu();
        }
        if (changed) drawNetMenu();
        delay(50);
    }
}

bool connectWiFi(const String& ssid, const String& pass) {
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid.c_str(), pass.c_str());
    for (int i = 0; i < 40 && WiFi.status() != WL_CONNECTED; i++) delay(500);
    return WiFi.status() == WL_CONNECTED;
}

String keyboardInput(const char* prompt, bool is_password) {
    String input = "";
    M5Cardputer.Display.fillScreen(C_BG);
    M5Cardputer.Display.setTextColor(C_ACCENT, C_BG);
    M5Cardputer.Display.setCursor(4, 10);
    M5Cardputer.Display.print(prompt);
    M5Cardputer.Display.setTextColor(C_TEXT, C_BG);
    M5Cardputer.Display.setCursor(4, 30);
    M5Cardputer.Display.print("> ");

    while (true) {
        M5Cardputer.update();
        if (M5Cardputer.Keyboard.isChange() && M5Cardputer.Keyboard.isPressed()) {
            Keyboard_Class::KeysState k = M5Cardputer.Keyboard.keysState();
            if (k.enter) return input;
            if (k.del && input.length() > 0) input.remove(input.length() - 1);
            for (auto key : k.word) input += key;

            M5Cardputer.Display.fillRect(0, 28, SCREEN_W, 16, C_BG);
            M5Cardputer.Display.setCursor(4, 30);
            M5Cardputer.Display.setTextColor(C_TEXT, C_BG);
            M5Cardputer.Display.print("> ");
            if (is_password) {
                for (size_t i = 0; i < input.length(); i++) M5Cardputer.Display.print('*');
            } else {
                M5Cardputer.Display.print(input);
            }
        }
        delay(50);
    }
}

// ════════════════════════════════════════════════════════════════════════
//  DATA FETCHING
// ════════════════════════════════════════════════════════════════════════

bool fetchFlightsInArea(float lat, float lon, float radius) {
    if (WiFi.status() != WL_CONNECTED) return false;

    HTTPClient http;
    String url = String(OPENSKY_STATES_URL) +
                 "?lamin=" + String(lat - radius, 4) +
                 "&lamax=" + String(lat + radius, 4) +
                 "&lomin=" + String(lon - radius, 4) +
                 "&lomax=" + String(lon + radius, 4);

    http.begin(url);
    http.setTimeout(15000);
    int code = http.GET();
    if (code != 200) { http.end(); return false; }

    String payload = http.getString();
    http.end();
    parseFlights(payload, lat, lon);
    return true;
}

void parseFlights(const String& json, float sort_lat, float sort_lon) {
    all_flights.clear();

    JsonDocument doc;
    if (deserializeJson(doc, json)) return;
    JsonArray states = doc["states"].as<JsonArray>();
    if (states.isNull()) return;

    int count = 0;
    for (JsonArray s : states) {
        if (count >= MAX_FLIGHTS) break;
        Flight f;
        f.icao24 = s[0].as<String>();
        f.callsign = s[1].as<String>();
        f.callsign.trim();
        if (f.callsign.length() == 0) f.callsign = "------";
        f.origin_country = s[2].as<String>();
        if (s[5].isNull() || s[6].isNull()) continue;
        f.longitude  = s[5].as<float>();
        f.latitude   = s[6].as<float>();
        f.altitude   = s[7].isNull() ? 0 : s[7].as<float>();
        f.on_ground  = s[8].as<bool>();
        f.velocity   = s[9].isNull() ? 0 : s[9].as<float>();
        f.heading    = s[10].isNull() ? -1 : s[10].as<float>();
        f.distance_km = haversine_km(sort_lat, sort_lon, f.latitude, f.longitude);
        f.route_fetched = false;
        f.origin_lat = f.origin_lon = f.dest_lat = f.dest_lon = 0;
        all_flights.push_back(f);
        count++;
    }

    std::sort(all_flights.begin(), all_flights.end(),
              [](const Flight& a, const Flight& b) { return a.distance_km < b.distance_km; });
}

void fetchRoute(Flight& f) {
    if (f.route_fetched) return;
    f.route_fetched = true;
    if (WiFi.status() != WL_CONNECTED || f.callsign == "------") return;

    HTTPClient http;
    http.begin(String(OPENSKY_ROUTES_URL) + "?callsign=" + f.callsign);
    http.setTimeout(8000);
    int code = http.GET();
    if (code == 200) {
        String payload = http.getString();
        JsonDocument doc;
        if (!deserializeJson(doc, payload)) {
            JsonArray route = doc["route"].as<JsonArray>();
            if (!route.isNull() && route.size() >= 2) {
                f.origin_icao = route[0].as<String>();
                f.destination_icao = route[route.size() - 1].as<String>();
                const Airport* o = findAirport(f.origin_icao.c_str());
                if (o) { f.origin_lat = o->lat; f.origin_lon = o->lon; }
                const Airport* d = findAirport(f.destination_icao.c_str());
                if (d) { f.dest_lat = d->lat; f.dest_lon = d->lon; }
            }
        }
    }
    http.end();
}

// Fetch routes only for the currently visible page of flights
void fetchVisibleRoutes() {
    int page = scroll_offset / FLIGHTS_PER_PAGE;
    if (page == last_route_page) return;  // already fetched this page
    last_route_page = page;

    int start = scroll_offset;
    int end = min(start + FLIGHTS_PER_PAGE, (int)flights.size());
    bool need_any = false;
    for (int i = start; i < end; i++) {
        if (!flights[i].route_fetched) { need_any = true; break; }
    }
    if (!need_any) return;

    M5Cardputer.Display.fillScreen(C_BG);
    int total = end - start;
    int done = 0;
    for (int i = start; i < end; i++) {
        if (!flights[i].route_fetched) {
            char buf[32];
            snprintf(buf, sizeof(buf), "Routes: %d/%d", done + 1, total);
            M5Cardputer.Display.fillRect(0, 55, SCREEN_W, 25, C_BG);
            drawCentered(buf, 58, C_DIM);
            int barW = (done + 1) * 180 / total;
            M5Cardputer.Display.drawRect(30, 72, 180, 6, C_DIM);
            M5Cardputer.Display.fillRect(30, 72, barW, 6, C_ACCENT);
            fetchRoute(flights[i]);
        }
        done++;
    }
}

bool refreshSelectedFlight() {
    if (selected_flight < 0 || selected_flight >= (int)flights.size()) return false;
    Flight& f = flights[selected_flight];
    if (WiFi.status() != WL_CONNECTED) return false;

    HTTPClient http;
    String url = String(OPENSKY_STATES_URL) + "?icao24=" + f.icao24;
    http.begin(url);
    http.setTimeout(8000);
    int code = http.GET();
    if (code != 200) { http.end(); return false; }

    String payload = http.getString();
    http.end();

    JsonDocument doc;
    if (deserializeJson(doc, payload)) return false;
    JsonArray states = doc["states"].as<JsonArray>();
    if (states.isNull() || states.size() == 0) return false;

    JsonArray s = states[0];
    if (!s[5].isNull()) f.longitude = s[5].as<float>();
    if (!s[6].isNull()) f.latitude  = s[6].as<float>();
    if (!s[7].isNull()) f.altitude  = s[7].as<float>();
    f.on_ground = s[8].as<bool>();
    if (!s[9].isNull())  f.velocity = s[9].as<float>();
    if (!s[10].isNull()) f.heading  = s[10].as<float>();
    f.distance_km = haversine_km(observer_lat, observer_lon, f.latitude, f.longitude);

    return true;
}

void buildCarrierList() {
    // Count active flights per carrier
    std::map<String, int> counts;
    for (auto& f : all_flights) {
        char code[4];
        extractCarrierCode(f.callsign, code, sizeof(code));
        if (strlen(code) >= 2) counts[String(code)]++;
    }

    carriers.clear();
    // Include ALL known airlines from database
    for (int i = 0; i < AIRLINE_COUNT; i++) {
        CarrierInfo ci;
        strncpy(ci.code, AIRLINES[i].icao, 3);
        ci.code[3] = '\0';
        ci.name = AIRLINES[i].name;
        auto it = counts.find(String(ci.code));
        ci.count = (it != counts.end()) ? it->second : 0;
        carriers.push_back(ci);
    }
    // Also add any active carriers not in our database
    for (auto& kv : counts) {
        bool found = false;
        for (int i = 0; i < AIRLINE_COUNT; i++) {
            if (kv.first == AIRLINES[i].icao) { found = true; break; }
        }
        if (!found) {
            CarrierInfo ci;
            strncpy(ci.code, kv.first.c_str(), 3);
            ci.code[3] = '\0';
            ci.name = nullptr;
            ci.count = kv.second;
            carriers.push_back(ci);
        }
    }
    // Sort: active flights first (desc), then alphabetical
    std::sort(carriers.begin(), carriers.end(),
              [](const CarrierInfo& a, const CarrierInfo& b) {
                  if (a.count != b.count) return a.count > b.count;
                  return strcmp(a.code, b.code) < 0;
              });
}

void filterByCarrier(const char* code) {
    flights.clear();
    for (auto& f : all_flights) {
        char fc[4];
        extractCarrierCode(f.callsign, fc, sizeof(fc));
        if (strcmp(fc, code) == 0) {
            flights.push_back(f);
            if ((int)flights.size() >= MAX_LIST_FLIGHTS) break;
        }
    }
}

void filterByText(const String& query) {
    flights.clear();
    String q = query;
    q.toLowerCase();
    for (auto& f : all_flights) {
        String cs = f.callsign; cs.toLowerCase();
        String co = f.origin_country; co.toLowerCase();

        // Carrier name
        char code[4];
        extractCarrierCode(f.callsign, code, sizeof(code));
        const char* aname = findAirlineName(code);
        String an = aname ? String(aname) : ""; an.toLowerCase();

        // Origin/destination airport codes and city names
        String oi = f.origin_icao; oi.toLowerCase();
        String di = f.destination_icao; di.toLowerCase();
        String ocity = "", dcity = "";
        if (f.origin_icao.length() > 0) {
            const Airport* oa = findAirport(f.origin_icao.c_str());
            if (oa) { ocity = String(oa->city); ocity.toLowerCase(); }
        }
        if (f.destination_icao.length() > 0) {
            const Airport* da = findAirport(f.destination_icao.c_str());
            if (da) { dcity = String(da->city); dcity.toLowerCase(); }
        }

        if (cs.indexOf(q) >= 0 || co.indexOf(q) >= 0 || an.indexOf(q) >= 0 ||
            oi.indexOf(q) >= 0 || di.indexOf(q) >= 0 ||
            ocity.indexOf(q) >= 0 || dcity.indexOf(q) >= 0) {
            flights.push_back(f);
            if ((int)flights.size() >= MAX_LIST_FLIGHTS) break;
        }
    }
}

// ════════════════════════════════════════════════════════════════════════
//  INPUT HANDLERS
// ════════════════════════════════════════════════════════════════════════

static const char* MENU_ITEMS[] = {
    "Near Me",
    "Long Flights",
    "By City",
    "By Carrier",
    "Search"
};
static const int MENU_COUNT = 5;

void doSearch(SearchMode mode);

void handleMenuInput(Keyboard_Class::KeysState& keys) {
    bool redraw = false;
    for (auto key : keys.word) {
        if ((key == ';' || key == ',') && menu_cursor > 0)
            { menu_cursor--; redraw = true; }
        if ((key == '.' || key == '/') && menu_cursor < MENU_COUNT - 1)
            { menu_cursor++; redraw = true; }
        if (key == 'w' || key == 'W') {
            wifiScanAndConnect();
            redraw = true;
        }
    }
    if (keys.enter) {
        doSearch((SearchMode)menu_cursor);
        return;
    }
    if (redraw) drawMenu();
}

void doSearch(SearchMode mode) {
    search_mode = mode;
    M5Cardputer.Display.fillScreen(C_BG);
    drawCentered("Fetching...", 45, C_DIM);

    switch (mode) {
    case SEARCH_NEAR:
        if (!fetchFlightsInArea(observer_lat, observer_lon, RADIUS_NEAR)) {
            drawCentered("Fetch failed!", 65, TFT_RED);
            delay(1000);
            drawMenu();
            return;
        }
        flights.clear();
        for (int i = 0; i < min((int)all_flights.size(), MAX_LIST_FLIGHTS); i++)
            flights.push_back(all_flights[i]);
        fetchVisibleRoutes();
        break;

    case SEARCH_LONG:
        // Fetch large area, sort by altitude (proxy for long-haul)
        if (!fetchFlightsInArea(observer_lat, observer_lon, RADIUS_WIDE)) {
            drawCentered("Fetch failed!", 65, TFT_RED);
            delay(1000);
            drawMenu();
            return;
        }
        // Sort by altitude descending (high altitude = long haul)
        std::sort(all_flights.begin(), all_flights.end(),
                  [](const Flight& a, const Flight& b) { return a.altitude > b.altitude; });
        flights.clear();
        for (int i = 0; i < min((int)all_flights.size(), MAX_LIST_FLIGHTS); i++)
            flights.push_back(all_flights[i]);
        fetchVisibleRoutes();
        break;

    case SEARCH_CITY:
        city_cursor = 0;
        city_scroll = 0;
        city_filter = "";
        current_view = VIEW_CITY_PICK;
        drawCityPicker();
        return;

    case SEARCH_CARRIER:
        // Build carrier list from known airlines + any cached flights
        buildCarrierList();
        carrier_cursor = 0;
        carrier_scroll = 0;
        carrier_filter = "";
        current_view = VIEW_CARRIER_PICK;
        drawCarrierPicker();
        return;

    case SEARCH_TEXT: {
        String query = keyboardInput("Search flights:");
        if (query.length() == 0) { drawMenu(); return; }
        M5Cardputer.Display.fillScreen(C_BG);
        drawCentered("Fetching...", 45, C_DIM);
        if (!fetchFlightsInArea(observer_lat, observer_lon, RADIUS_WIDE)) {
            drawCentered("Fetch failed!", 65, TFT_RED);
            delay(1000);
            drawMenu();
            return;
        }
        // Fetch routes for all flights so we can search by city/airport
        drawCentered("Loading routes...", 60, C_DIM);
        int total = min((int)all_flights.size(), 30); // cap route fetches
        for (int i = 0; i < total; i++) {
            if (!all_flights[i].route_fetched) {
                char buf[32];
                snprintf(buf, sizeof(buf), "Routes: %d/%d", i + 1, total);
                M5Cardputer.Display.fillRect(0, 70, SCREEN_W, 20, C_BG);
                drawCentered(buf, 75, C_DIM);
                fetchRoute(all_flights[i]);
            }
        }
        filterByText(query);
        break;
    }
    }

    cursor = 0;
    scroll_offset = 0;
    selected_flight = -1;
    show_routes = false;
    last_route_page = -1;
    current_view = VIEW_LIST;
    fetchVisibleRoutes();
    drawFlightList();
}

void handleCityPickInput(Keyboard_Class::KeysState& keys) {
    bool redraw = false;
    int total = AIRPORT_COUNT;

    // Filter changes total
    const Airport* filtered[AIRPORT_COUNT];
    if (city_filter.length() > 0) {
        total = searchAirports(city_filter.c_str(), filtered, AIRPORT_COUNT);
    }

    for (auto key : keys.word) {
        if ((key == ';' || key == ',') && city_cursor > 0) {
            city_cursor--;
            if (city_cursor < city_scroll) city_scroll = city_cursor;
            redraw = true;
        } else if ((key == '.' || key == '/') && city_cursor < total - 1) {
            city_cursor++;
            if (city_cursor >= city_scroll + 5) city_scroll = city_cursor - 4;
            redraw = true;
        } else if (key == '`') {
            current_view = VIEW_MENU;
            drawMenu();
            return;
        } else if (isAlpha(key) || isDigit(key) || key == ' ') {
            city_filter += key;
            city_cursor = 0;
            city_scroll = 0;
            redraw = true;
        }
    }

    if (keys.del) {
        if (city_filter.length() > 0) {
            city_filter.remove(city_filter.length() - 1);
            city_cursor = 0;
            city_scroll = 0;
            redraw = true;
        } else {
            current_view = VIEW_MENU;
            drawMenu();
            return;
        }
    }

    if (keys.enter && total > 0) {
        const Airport* apt;
        if (city_filter.length() > 0) {
            apt = filtered[city_cursor];
        } else {
            apt = &AIRPORTS[city_cursor];
        }

        M5Cardputer.Display.fillScreen(C_BG);
        char msg[40];
        snprintf(msg, sizeof(msg), "Flights near %s...", apt->city);
        drawCentered(msg, 45, C_DIM);

        if (!fetchFlightsInArea(apt->lat, apt->lon, RADIUS_CITY)) {
            drawCentered("Fetch failed!", 65, TFT_RED);
            delay(1000);
            drawCityPicker();
            return;
        }
        flights.clear();
        for (int i = 0; i < min((int)all_flights.size(), MAX_LIST_FLIGHTS); i++)
            flights.push_back(all_flights[i]);
        fetchVisibleRoutes();

        cursor = 0; scroll_offset = 0; selected_flight = -1;
        show_routes = false; last_route_page = -1;
        search_mode = SEARCH_CITY;
        current_view = VIEW_LIST;
        fetchVisibleRoutes();
        drawFlightList();
        return;
    }

    if (redraw) drawCityPicker();
}

// Build a filtered carrier list based on carrier_filter
static std::vector<int> getFilteredCarrierIndices() {
    std::vector<int> result;
    if (carrier_filter.length() == 0) {
        for (int i = 0; i < (int)carriers.size(); i++) result.push_back(i);
        return result;
    }
    String q = carrier_filter;
    q.toLowerCase();
    for (int i = 0; i < (int)carriers.size(); i++) {
        String code_s = String(carriers[i].code); code_s.toLowerCase();
        String name_s = carriers[i].name ? String(carriers[i].name) : ""; name_s.toLowerCase();
        if (code_s.indexOf(q) >= 0 || name_s.indexOf(q) >= 0) {
            result.push_back(i);
        }
    }
    return result;
}

void handleCarrierPickInput(Keyboard_Class::KeysState& keys) {
    bool redraw = false;
    auto filtered = getFilteredCarrierIndices();
    int total = (int)filtered.size();

    for (auto key : keys.word) {
        if ((key == ';' || key == ',') && carrier_cursor > 0) {
            carrier_cursor--;
            if (carrier_cursor < carrier_scroll) carrier_scroll = carrier_cursor;
            redraw = true;
        } else if ((key == '.' || key == '/') && carrier_cursor < total - 1) {
            carrier_cursor++;
            if (carrier_cursor >= carrier_scroll + 5) carrier_scroll = carrier_cursor - 4;
            redraw = true;
        } else if (key == '`') {
            current_view = VIEW_MENU;
            drawMenu();
            return;
        } else if (isAlpha(key) || isDigit(key) || key == ' ') {
            carrier_filter += key;
            carrier_cursor = 0;
            carrier_scroll = 0;
            redraw = true;
        }
    }
    if (keys.del) {
        if (carrier_filter.length() > 0) {
            carrier_filter.remove(carrier_filter.length() - 1);
            carrier_cursor = 0;
            carrier_scroll = 0;
            redraw = true;
        } else {
            current_view = VIEW_MENU;
            drawMenu();
            return;
        }
    }
    if (keys.enter && total > 0) {
        int real_idx = filtered[carrier_cursor];
        M5Cardputer.Display.fillScreen(C_BG);
        char msg[40];
        snprintf(msg, sizeof(msg), "Fetching %s flights...",
                 carriers[real_idx].name ? carriers[real_idx].name : carriers[real_idx].code);
        drawCentered(msg, 45, C_DIM);

        // Fetch flights in wide area, then filter to this carrier
        if (!fetchFlightsInArea(observer_lat, observer_lon, RADIUS_WIDE)) {
            drawCentered("Fetch failed!", 65, TFT_RED);
            delay(1000);
            drawCarrierPicker();
            return;
        }
        filterByCarrier(carriers[real_idx].code);

        cursor = 0; scroll_offset = 0; selected_flight = -1;
        show_routes = false; last_route_page = -1;
        search_mode = SEARCH_CARRIER;
        current_view = VIEW_LIST;
        fetchVisibleRoutes();
        drawFlightList();
        return;
    }
    if (redraw) drawCarrierPicker();
}

void handleListInput(Keyboard_Class::KeysState& keys) {
    bool redraw = false;
    int total = (int)flights.size();

    for (auto key : keys.word) {
        if ((key == ';' || key == ',') && cursor > 0) {
            if (cursor == scroll_offset && scroll_offset > 0) {
                // At top of page: page up
                scroll_offset = max(0, scroll_offset - FLIGHTS_PER_PAGE);
                cursor = min(scroll_offset + FLIGHTS_PER_PAGE - 1, total - 1);
            } else {
                cursor--;
            }
            redraw = true;
        }
        else if ((key == '.' || key == '/') && cursor < total - 1) {
            if (cursor == scroll_offset + FLIGHTS_PER_PAGE - 1) {
                // At bottom of page: page down
                scroll_offset = min(scroll_offset + FLIGHTS_PER_PAGE, total - 1);
                cursor = scroll_offset;
            } else {
                cursor++;
            }
            redraw = true;
        }
        else if (key == 'd' || key == 'D') {
            // Page down
            if (scroll_offset + FLIGHTS_PER_PAGE < total) {
                scroll_offset += FLIGHTS_PER_PAGE;
                cursor = scroll_offset;
            }
            redraw = true;
        }
        else if (key == 'u' || key == 'U') {
            // Page up
            if (scroll_offset > 0) {
                scroll_offset = max(0, scroll_offset - FLIGHTS_PER_PAGE);
                cursor = scroll_offset;
            }
            redraw = true;
        }
        else if (key == 'r' || key == 'R') {
            doSearch(search_mode);
            return;
        }
        else if (key == 'm' || key == 'M') {
            map_from_list = true;
            map_last_refresh = millis();
            current_view = VIEW_MAP;
            drawAllFlightsMap();
            return;
        }
        else if (key == 't' || key == 'T') {
            show_routes = !show_routes;
            redraw = true;
        }
        else if (key == '`') {
            current_view = VIEW_MENU;
            drawMenu();
            return;
        }
    }

    if (keys.del) {
        current_view = VIEW_MENU;
        drawMenu();
        return;
    }

    if (keys.enter && !flights.empty()) {
        selected_flight = cursor;
        if (!flights[selected_flight].route_fetched) {
            M5Cardputer.Display.fillScreen(C_BG);
            drawCentered("Loading route...", 55, C_DIM);
            fetchRoute(flights[selected_flight]);
        }
        current_view = VIEW_DETAIL;
        drawFlightDetail();
        return;
    }

    if (redraw) {
        // Fetch routes for new visible page if needed
        fetchVisibleRoutes();
        drawFlightList();
    }
}

void handleDetailInput(Keyboard_Class::KeysState& keys) {
    for (auto key : keys.word) {
        if (key == 'm' || key == 'M') {
            map_from_list = false;
            map_last_refresh = millis();
            current_view = VIEW_MAP;
            drawFlightMap();
            return;
        }
        if (key == '`' || key == ',') {
            current_view = VIEW_LIST;
            drawFlightList();
            return;
        }
    }
    if (keys.del || keys.enter) {
        current_view = VIEW_LIST;
        drawFlightList();
        return;
    }
}

void handleMapInput(Keyboard_Class::KeysState& keys) {
    auto goBack = [&]() {
        if (map_from_list) {
            current_view = VIEW_LIST;
            drawFlightList();
        } else {
            current_view = VIEW_DETAIL;
            drawFlightDetail();
        }
    };

    if (keys.del || keys.enter) {
        goBack();
        return;
    }
    for (auto key : keys.word) {
        (void)key;
        goBack();
        return;
    }
}

// ════════════════════════════════════════════════════════════════════════
//  DRAWING HELPERS
// ════════════════════════════════════════════════════════════════════════

void drawCentered(const char* text, int y, uint16_t color) {
    M5Cardputer.Display.setTextColor(color, C_BG);
    int w = M5Cardputer.Display.textWidth(text);
    M5Cardputer.Display.setCursor((SCREEN_W - w) / 2, y);
    M5Cardputer.Display.print(text);
}

void drawHeader(const char* title, const char* right) {
    M5Cardputer.Display.fillRect(0, 0, SCREEN_W, 14, C_HEADER);
    M5Cardputer.Display.setTextColor(C_TEXT, C_HEADER);
    M5Cardputer.Display.setCursor(4, 3);
    M5Cardputer.Display.print(title);
    if (right) {
        int w = M5Cardputer.Display.textWidth(right);
        M5Cardputer.Display.setCursor(SCREEN_W - w - 4, 3);
        M5Cardputer.Display.print(right);
    }
}

void drawStatusBar(const char* msg) {
    M5Cardputer.Display.fillRect(0, SCREEN_H - 12, SCREEN_W, 12, C_HEADER);
    M5Cardputer.Display.setTextColor(C_TEXT, C_HEADER);
    M5Cardputer.Display.setCursor(4, SCREEN_H - 10);
    M5Cardputer.Display.print(msg);
}

// ════════════════════════════════════════════════════════════════════════
//  MAIN MENU
// ════════════════════════════════════════════════════════════════════════

void drawMenu() {
    M5Cardputer.Display.fillScreen(C_BG);
    char info[32];
    int batt = M5Cardputer.Power.getBatteryLevel();
    int rssi = WiFi.RSSI();
    int bars = (rssi > -50) ? 4 : (rssi > -65) ? 3 : (rssi > -75) ? 2 : (rssi > -85) ? 1 : 0;
    char sig[5] = "";
    for (int i = 0; i < bars; i++) sig[i] = '|';
    sig[bars] = '\0';
    snprintf(info, sizeof(info), "%s %d%%", sig, batt);
    drawHeader("Flight Tracker", info);

    int y = 22;
    const char* descs[] = {
        "250mi radius",
        "Long flights",
        "Select an airport",
        "Filter by airline",
        ""
    };

    for (int i = 0; i < MENU_COUNT; i++) {
        bool hl = (i == menu_cursor);
        if (hl) M5Cardputer.Display.fillRect(0, y - 2, SCREEN_W, 19, C_SEL);

        M5Cardputer.Display.setTextColor(hl ? C_ACCENT : C_TEXT, hl ? C_SEL : C_BG);
        M5Cardputer.Display.setCursor(8, y);
        M5Cardputer.Display.print(hl ? "> " : "  ");
        M5Cardputer.Display.print(MENU_ITEMS[i]);

        M5Cardputer.Display.setTextColor(C_DIM, hl ? C_SEL : C_BG);
        M5Cardputer.Display.setCursor(120, y);
        M5Cardputer.Display.print(descs[i]);

        y += 20;
    }

    drawStatusBar(";.=nav Enter=select W=wifi");
}

// ════════════════════════════════════════════════════════════════════════
//  CITY PICKER
// ════════════════════════════════════════════════════════════════════════

void drawCityPicker() {
    M5Cardputer.Display.fillScreen(C_BG);

    const Airport* list[AIRPORT_COUNT];
    int total;
    if (city_filter.length() > 0) {
        total = searchAirports(city_filter.c_str(), list, AIRPORT_COUNT);
    } else {
        total = AIRPORT_COUNT;
        for (int i = 0; i < total; i++) list[i] = &AIRPORTS[i];
    }

    char hdr[32];
    snprintf(hdr, sizeof(hdr), "Select City (%d)", total);
    drawHeader(hdr);

    // Search field
    M5Cardputer.Display.setTextColor(C_ACCENT, C_BG);
    M5Cardputer.Display.setCursor(4, 17);
    M5Cardputer.Display.print("Filter: ");
    M5Cardputer.Display.setTextColor(C_TEXT, C_BG);
    M5Cardputer.Display.print(city_filter.length() > 0 ? city_filter.c_str() : "_");

    int y = 32;
    int vis = 5;
    int end = min(city_scroll + vis, total);
    for (int i = city_scroll; i < end; i++) {
        bool hl = (i == city_cursor);
        if (hl) M5Cardputer.Display.fillRect(0, y - 1, SCREEN_W, 16, C_SEL);
        M5Cardputer.Display.setTextColor(hl ? C_ACCENT : C_TEXT, hl ? C_SEL : C_BG);
        M5Cardputer.Display.setCursor(4, y);
        M5Cardputer.Display.printf("%-4s  %s", list[i]->icao, list[i]->city);
        y += 17;
    }

    drawStatusBar(";.=nav Type=filter Del=back");
}

// ════════════════════════════════════════════════════════════════════════
//  CARRIER PICKER
// ════════════════════════════════════════════════════════════════════════

void drawCarrierPicker() {
    M5Cardputer.Display.fillScreen(C_BG);

    auto filtered = getFilteredCarrierIndices();
    int total = (int)filtered.size();

    char hdr[32];
    snprintf(hdr, sizeof(hdr), "Airlines (%d)", total);
    drawHeader(hdr);

    // Search field
    M5Cardputer.Display.setTextColor(C_ACCENT, C_BG);
    M5Cardputer.Display.setCursor(4, 17);
    M5Cardputer.Display.print("Filter: ");
    M5Cardputer.Display.setTextColor(C_TEXT, C_BG);
    M5Cardputer.Display.print(carrier_filter.length() > 0 ? carrier_filter.c_str() : "_");

    int y = 32;
    int vis = 5;
    int end = min(carrier_scroll + vis, total);
    for (int i = carrier_scroll; i < end; i++) {
        int ri = filtered[i];  // real index into carriers[]
        bool hl = (i == carrier_cursor);
        if (hl) M5Cardputer.Display.fillRect(0, y - 1, SCREEN_W, 16, C_SEL);
        M5Cardputer.Display.setTextColor(hl ? C_ACCENT : C_TEXT, hl ? C_SEL : C_BG);
        M5Cardputer.Display.setCursor(4, y);
        M5Cardputer.Display.print(carriers[ri].code);
        M5Cardputer.Display.setCursor(35, y);
        if (carriers[ri].name) {
            M5Cardputer.Display.print(carriers[ri].name);
        } else {
            M5Cardputer.Display.print("(unknown)");
        }
        M5Cardputer.Display.setTextColor(C_DIM, hl ? C_SEL : C_BG);
        char cnt[8];
        snprintf(cnt, sizeof(cnt), "%d", carriers[ri].count);
        int w = M5Cardputer.Display.textWidth(cnt);
        M5Cardputer.Display.setCursor(SCREEN_W - w - 6, y);
        M5Cardputer.Display.print(cnt);
        y += 17;
    }

    drawStatusBar(";.=nav Type=filter Del=back");
}

// ════════════════════════════════════════════════════════════════════════
//  FLIGHT LIST
// ════════════════════════════════════════════════════════════════════════

void drawFlightList() {
    M5Cardputer.Display.fillScreen(C_BG);

    String right = String((int)flights.size()) + " flights";
    drawHeader(MENU_ITEMS[(int)search_mode], right.c_str());

    if (flights.empty()) {
        drawCentered("No flights found", 50, C_DIM);
        drawStatusBar("R=refresh Del=menu");
        return;
    }

    int y = 17;
    M5Cardputer.Display.setTextColor(C_ACCENT, C_BG);

    if (show_routes) {
        M5Cardputer.Display.setCursor(4, y);
        M5Cardputer.Display.print("AIRLINE");
        M5Cardputer.Display.setCursor(80, y);
        M5Cardputer.Display.print("FROM");
        M5Cardputer.Display.setCursor(130, y);
        M5Cardputer.Display.print("TO");
        M5Cardputer.Display.setCursor(185, y);
        M5Cardputer.Display.print("PROG");
    } else {
        M5Cardputer.Display.setCursor(4, y);
        M5Cardputer.Display.print("CALLSIGN");
        M5Cardputer.Display.setCursor(75, y);
        M5Cardputer.Display.print("ALT(ft)");
        M5Cardputer.Display.setCursor(135, y);
        M5Cardputer.Display.print("SPD(kt)");
        M5Cardputer.Display.setCursor(195, y);
        M5Cardputer.Display.print("DIST");
    }

    y += 11;
    M5Cardputer.Display.drawLine(0, y, SCREEN_W, y, C_DIM);
    y += 3;

    int end = min(scroll_offset + FLIGHTS_PER_PAGE, (int)flights.size());
    for (int i = scroll_offset; i < end; i++) {
        const Flight& f = flights[i];
        bool hl = (i == cursor);

        if (hl) M5Cardputer.Display.fillRect(0, y - 1, SCREEN_W, 17, C_SEL);
        uint16_t bg = hl ? C_SEL : C_BG;
        uint16_t fg = f.on_ground ? C_GROUND : (hl ? C_ACCENT : C_TEXT);
        M5Cardputer.Display.setTextColor(fg, bg);

        if (show_routes) {
            // Airline name (or callsign fallback)
            M5Cardputer.Display.setCursor(4, y);
            char code[4];
            extractCarrierCode(f.callsign, code, sizeof(code));
            const char* aname = findAirlineName(code);
            if (aname) {
                String sname = String(aname);
                M5Cardputer.Display.print(sname.substring(0, 10));
            } else {
                M5Cardputer.Display.print(f.callsign.substring(0, 8));
            }

            // Origin ICAO
            M5Cardputer.Display.setCursor(80, y);
            M5Cardputer.Display.print(f.route_fetched
                ? (f.origin_icao.length() > 0 ? f.origin_icao.substring(0, 4) : "----")
                : " ...");
            // Destination ICAO
            M5Cardputer.Display.setCursor(130, y);
            M5Cardputer.Display.print(f.route_fetched
                ? (f.destination_icao.length() > 0 ? f.destination_icao.substring(0, 4) : "----")
                : " ...");

            // % complete
            M5Cardputer.Display.setCursor(185, y);
            if (f.route_fetched && f.origin_lat != 0 && f.dest_lat != 0) {
                float total_d = haversine_km(f.origin_lat, f.origin_lon, f.dest_lat, f.dest_lon);
                float from_o = haversine_km(f.origin_lat, f.origin_lon, f.latitude, f.longitude);
                int pct = (total_d > 0) ? constrain((int)(from_o / total_d * 100), 0, 100) : 0;
                M5Cardputer.Display.printf("%3d%%", pct);
            } else if (f.on_ground) {
                M5Cardputer.Display.print("GND");
            } else {
                M5Cardputer.Display.print(" --");
            }
        } else {
            M5Cardputer.Display.setCursor(4, y);
            M5Cardputer.Display.print(f.callsign.substring(0, 8));

            M5Cardputer.Display.setCursor(75, y);
            if (f.on_ground) M5Cardputer.Display.print("  GND");
            else M5Cardputer.Display.printf("%6.0f", m_to_feet(f.altitude));
            M5Cardputer.Display.setCursor(135, y);
            M5Cardputer.Display.printf("%5.0f", ms_to_knots(f.velocity));

            M5Cardputer.Display.setCursor(195, y);
            if (f.distance_km < 100) M5Cardputer.Display.printf("%4.1fk", f.distance_km);
            else M5Cardputer.Display.printf("%4.0fk", f.distance_km);
        }

        y += 18;
    }

    char bar[64];
    snprintf(bar, sizeof(bar), ";.=nav T=%s R=redo M=map Enter=sel",
             show_routes ? "alt" : "route");
    drawStatusBar(bar);
}

// ════════════════════════════════════════════════════════════════════════
//  FLIGHT DETAIL
// ════════════════════════════════════════════════════════════════════════

void drawFlightDetail() {
    if (selected_flight < 0 || selected_flight >= (int)flights.size()) return;
    const Flight& f = flights[selected_flight];

    M5Cardputer.Display.fillScreen(C_BG);

    // Header: callsign + carrier name
    char code[4];
    extractCarrierCode(f.callsign, code, sizeof(code));
    const char* airline = findAirlineName(code);

    char title[40];
    if (airline) snprintf(title, sizeof(title), "%s (%s)", f.callsign.c_str(), airline);
    else snprintf(title, sizeof(title), "%s", f.callsign.c_str());
    drawHeader(title);

    int y = 18;
    int lh = 14;

    auto label = [&](const char* lbl) {
        M5Cardputer.Display.setTextColor(C_DIM, C_BG);
        M5Cardputer.Display.setCursor(4, y);
        M5Cardputer.Display.print(lbl);
    };
    auto val = [&](const char* v, uint16_t c = C_TEXT) {
        M5Cardputer.Display.setTextColor(c, C_BG);
        M5Cardputer.Display.setCursor(65, y);
        M5Cardputer.Display.print(v);
        y += lh;
    };

    // Origin
    label("From:");
    if (f.origin_icao.length() > 0) {
        const Airport* o = findAirport(f.origin_icao.c_str());
        char buf[40];
        if (o) snprintf(buf, sizeof(buf), "%s - %s", f.origin_icao.c_str(), o->city);
        else snprintf(buf, sizeof(buf), "%s", f.origin_icao.c_str());
        val(buf, C_ORIGIN);
    } else {
        val("Unknown", C_DIM);
    }

    // Destination
    label("To:");
    if (f.destination_icao.length() > 0) {
        const Airport* d = findAirport(f.destination_icao.c_str());
        char buf[40];
        if (d) snprintf(buf, sizeof(buf), "%s - %s", f.destination_icao.c_str(), d->city);
        else snprintf(buf, sizeof(buf), "%s", f.destination_icao.c_str());
        val(buf, C_DEST);
    } else {
        val("Unknown", C_DIM);
    }

    // Altitude
    char buf[48];
    label("Alt:");
    if (f.on_ground) val("On Ground", C_GROUND);
    else { snprintf(buf, sizeof(buf), "%.0f ft (%.0f m)", m_to_feet(f.altitude), f.altitude); val(buf, C_AIRBORNE); }

    // Speed
    label("Speed:");
    snprintf(buf, sizeof(buf), "%.0f kts (%.0f km/h)", ms_to_knots(f.velocity), f.velocity * 3.6f);
    val(buf);

    // Heading
    label("Hdg:");
    if (f.heading >= 0) snprintf(buf, sizeof(buf), "%.0f° %s", f.heading, heading_to_compass(f.heading));
    else snprintf(buf, sizeof(buf), "N/A");
    val(buf);

    // Distance
    label("Dist:");
    snprintf(buf, sizeof(buf), "%.1f km / %.1f mi", f.distance_km, f.distance_km * 0.621371f);
    val(buf, C_ACCENT);

    // Country
    label("Ctry:");
    val(f.origin_country.c_str());

    bool has_map = (f.origin_lat != 0 || f.origin_lon != 0) &&
                   (f.dest_lat != 0 || f.dest_lon != 0);
    drawStatusBar(has_map ? "M=map Del=back" : "Del=back");
}

// ════════════════════════════════════════════════════════════════════════
//  MAP VIEW
// ════════════════════════════════════════════════════════════════════════

void drawFlightMap() {
    if (selected_flight < 0 || selected_flight >= (int)flights.size()) return;
    const Flight& f = flights[selected_flight];

    if ((f.origin_lat == 0 && f.origin_lon == 0) ||
        (f.dest_lat == 0 && f.dest_lon == 0)) {
        M5Cardputer.Display.fillScreen(C_BG);
        drawCentered("No route data", 55, C_DIM);
        drawStatusBar("Any key=back");
        return;
    }

    drawMiniMap(f);
}

void drawMiniMap(const Flight& f) {
    M5Cardputer.Display.fillScreen(C_OCEAN);

    const int mx = 0, my = 14, mw = SCREEN_W, mh = SCREEN_H - 26;

    // Compute bounding box including origin, destination, and current position
    float min_lat = min({f.origin_lat, f.dest_lat, f.latitude});
    float max_lat = max({f.origin_lat, f.dest_lat, f.latitude});
    float min_lon = min({f.origin_lon, f.dest_lon, f.longitude});
    float max_lon = max({f.origin_lon, f.dest_lon, f.longitude});

    // Add 15% padding
    float pad_lat = (max_lat - min_lat) * 0.15f + 1.0f;  // minimum 1 degree padding
    float pad_lon = (max_lon - min_lon) * 0.15f + 1.0f;
    min_lat -= pad_lat;  max_lat += pad_lat;
    min_lon -= pad_lon;  max_lon += pad_lon;

    // Aspect ratio correction: screen is wider than tall
    float lat_span = max_lat - min_lat;
    float lon_span = max_lon - min_lon;
    float aspect = (float)mw / (float)mh;

    if (lon_span / lat_span < aspect) {
        // Too narrow, widen longitude
        float needed = lat_span * aspect;
        float mid = (min_lon + max_lon) / 2;
        min_lon = mid - needed / 2;
        max_lon = mid + needed / 2;
        lon_span = needed;
    } else {
        // Too short, heighten latitude
        float needed = lon_span / aspect;
        float mid = (min_lat + max_lat) / 2;
        min_lat = mid - needed / 2;
        max_lat = mid + needed / 2;
        lat_span = needed;
    }

    auto toX = [&](float lon) -> int {
        return mx + (int)(((lon - min_lon) / lon_span) * mw);
    };
    auto toY = [&](float lat) -> int {
        return my + mh - (int)(((lat - min_lat) / lat_span) * mh);
    };

    // Draw coastlines
    bool pen_down = false;
    int prev_x = 0, prev_y = 0;
    for (int i = 0; ; i += 2) {
        int16_t lat10 = COASTLINE[i];
        int16_t lon10 = COASTLINE[i + 1];
        if (lat10 == 0x7FFF) break;
        if (lat10 == 0x7FFE) { pen_down = false; continue; }

        float lat = lat10 / 10.0f, lon = lon10 / 10.0f;
        int sx = toX(lon), sy = toY(lat);
        if (pen_down && abs(sx - prev_x) < mw) {
            M5Cardputer.Display.drawLine(prev_x, prev_y, sx, sy, C_LAND);
        }
        pen_down = true;
        prev_x = sx; prev_y = sy;
    }

    // Route line (great circle segments)
    const int segs = 40;
    int px = 0, py = 0;
    for (int i = 0; i <= segs; i++) {
        float frac = (float)i / segs;
        float lat, lon;
        interpolate_great_circle(f.origin_lat, f.origin_lon,
                                 f.dest_lat, f.dest_lon, frac, lat, lon);
        int sx = toX(lon), sy = toY(lat);
        if (i > 0 && abs(sx - px) < mw / 2) {
            M5Cardputer.Display.drawLine(px, py, sx, sy, C_ROUTE);
        }
        px = sx; py = sy;
    }

    // Origin dot + label
    {
        int ox = toX(f.origin_lon), oy = toY(f.origin_lat);
        M5Cardputer.Display.fillCircle(ox, oy, 3, C_ORIGIN);
        M5Cardputer.Display.setTextColor(C_ORIGIN, C_OCEAN);
        M5Cardputer.Display.setCursor(ox + 5, oy - 4);
        M5Cardputer.Display.print(f.origin_icao.substring(0, 4));
    }

    // Destination dot + label
    {
        int dx = toX(f.dest_lon), dy = toY(f.dest_lat);
        M5Cardputer.Display.fillCircle(dx, dy, 3, C_DEST);
        M5Cardputer.Display.setTextColor(C_DEST, C_OCEAN);
        M5Cardputer.Display.setCursor(dx + 5, dy - 4);
        M5Cardputer.Display.print(f.destination_icao.substring(0, 4));
    }

    // Plane icon
    {
        int ax = toX(f.longitude), ay = toY(f.latitude);
        float rad = radians(f.heading >= 0 ? f.heading : 0);
        float sz = 5.0f;
        int nx = ax + (int)(sinf(rad) * sz * 1.5f);
        int ny = ay - (int)(cosf(rad) * sz * 1.5f);
        int lx = ax + (int)(sinf(rad - 2.3f) * sz);
        int ly = ay - (int)(cosf(rad - 2.3f) * sz);
        int rx = ax + (int)(sinf(rad + 2.3f) * sz);
        int ry = ay - (int)(cosf(rad + 2.3f) * sz);
        M5Cardputer.Display.fillTriangle(nx, ny, lx, ly, rx, ry, C_PLANE);
        M5Cardputer.Display.fillCircle(ax, ay, 1, C_PLANE);
    }

    // Header
    char title[48];
    snprintf(title, sizeof(title), "%s: %s > %s",
             f.callsign.c_str(), f.origin_icao.c_str(), f.destination_icao.c_str());
    drawHeader(title);

    // Status bar: speed + progress
    float total_dist = haversine_km(f.origin_lat, f.origin_lon, f.dest_lat, f.dest_lon);
    float from_orig = haversine_km(f.origin_lat, f.origin_lon, f.latitude, f.longitude);
    int pct = (total_dist > 0) ? constrain((int)(from_orig / total_dist * 100), 0, 100) : 0;

    char bar[64];
    snprintf(bar, sizeof(bar), "%.0fkt  %.0fft  %d%%  %.0fkm left",
             ms_to_knots(f.velocity), m_to_feet(f.altitude),
             pct, max(0.0f, total_dist - from_orig));
    drawStatusBar(bar);
}

// ════════════════════════════════════════════════════════════════════════
//  ALL-FLIGHTS MAP (from list view)
// ════════════════════════════════════════════════════════════════════════

void drawAllFlightsMap() {
    if (flights.empty()) {
        M5Cardputer.Display.fillScreen(C_BG);
        drawCentered("No flights to show", 55, C_DIM);
        drawStatusBar("Any key=back");
        return;
    }

    M5Cardputer.Display.fillScreen(C_OCEAN);

    const int mx = 0, my = 14, mw = SCREEN_W, mh = SCREEN_H - 26;

    // Compute bounding box from ALL flights (positions + route endpoints)
    float min_lat = 90, max_lat = -90, min_lon = 180, max_lon = -180;
    for (auto& f : flights) {
        if (f.latitude  < min_lat) min_lat = f.latitude;
        if (f.latitude  > max_lat) max_lat = f.latitude;
        if (f.longitude < min_lon) min_lon = f.longitude;
        if (f.longitude > max_lon) max_lon = f.longitude;
        // Include route endpoints if available
        if (f.route_fetched && (f.origin_lat != 0 || f.origin_lon != 0)) {
            if (f.origin_lat < min_lat) min_lat = f.origin_lat;
            if (f.origin_lat > max_lat) max_lat = f.origin_lat;
            if (f.origin_lon < min_lon) min_lon = f.origin_lon;
            if (f.origin_lon > max_lon) max_lon = f.origin_lon;
        }
        if (f.route_fetched && (f.dest_lat != 0 || f.dest_lon != 0)) {
            if (f.dest_lat < min_lat) min_lat = f.dest_lat;
            if (f.dest_lat > max_lat) max_lat = f.dest_lat;
            if (f.dest_lon < min_lon) min_lon = f.dest_lon;
            if (f.dest_lon > max_lon) max_lon = f.dest_lon;
        }
    }

    // Padding
    float pad_lat = (max_lat - min_lat) * 0.12f + 0.5f;
    float pad_lon = (max_lon - min_lon) * 0.12f + 0.5f;
    min_lat -= pad_lat;  max_lat += pad_lat;
    min_lon -= pad_lon;  max_lon += pad_lon;

    // Aspect ratio correction
    float lat_span = max_lat - min_lat;
    float lon_span = max_lon - min_lon;
    float aspect = (float)mw / (float)mh;

    if (lon_span / lat_span < aspect) {
        float needed = lat_span * aspect;
        float mid = (min_lon + max_lon) / 2;
        min_lon = mid - needed / 2;
        max_lon = mid + needed / 2;
        lon_span = needed;
    } else {
        float needed = lon_span / aspect;
        float mid = (min_lat + max_lat) / 2;
        min_lat = mid - needed / 2;
        max_lat = mid + needed / 2;
        lat_span = needed;
    }

    auto toX = [&](float lon) -> int {
        return mx + (int)(((lon - min_lon) / lon_span) * mw);
    };
    auto toY = [&](float lat) -> int {
        return my + mh - (int)(((lat - min_lat) / lat_span) * mh);
    };

    // Draw coastlines
    bool pen_down = false;
    int prev_x = 0, prev_y = 0;
    for (int i = 0; ; i += 2) {
        int16_t lat10 = COASTLINE[i];
        int16_t lon10 = COASTLINE[i + 1];
        if (lat10 == 0x7FFF) break;
        if (lat10 == 0x7FFE) { pen_down = false; continue; }
        float lat = lat10 / 10.0f, lon = lon10 / 10.0f;
        int sx = toX(lon), sy = toY(lat);
        if (pen_down && abs(sx - prev_x) < mw) {
            M5Cardputer.Display.drawLine(prev_x, prev_y, sx, sy, C_LAND);
        }
        pen_down = true;
        prev_x = sx; prev_y = sy;
    }

    // Draw route lines (dimmer) and plane icons for ALL flights
    for (auto& f : flights) {
        // Route line if we have endpoints
        if (f.route_fetched &&
            (f.origin_lat != 0 || f.origin_lon != 0) &&
            (f.dest_lat != 0 || f.dest_lon != 0)) {
            const int segs = 20;
            int px = 0, py = 0;
            for (int s = 0; s <= segs; s++) {
                float frac = (float)s / segs;
                float rlat, rlon;
                interpolate_great_circle(f.origin_lat, f.origin_lon,
                                         f.dest_lat, f.dest_lon, frac, rlat, rlon);
                int sx = toX(rlon), sy = toY(rlat);
                if (s > 0 && abs(sx - px) < mw / 2) {
                    M5Cardputer.Display.drawLine(px, py, sx, sy, C_DIM);
                }
                px = sx; py = sy;
            }

            // Origin/dest dots (small)
            int ox = toX(f.origin_lon), oy = toY(f.origin_lat);
            M5Cardputer.Display.fillCircle(ox, oy, 2, C_ORIGIN);
            int dx = toX(f.dest_lon), dy = toY(f.dest_lat);
            M5Cardputer.Display.fillCircle(dx, dy, 2, C_DEST);
        }

        // Plane icon
        int ax = toX(f.longitude), ay = toY(f.latitude);
        float rad = radians(f.heading >= 0 ? f.heading : 0);
        float sz = 3.5f;
        int nx = ax + (int)(sinf(rad) * sz * 1.5f);
        int ny = ay - (int)(cosf(rad) * sz * 1.5f);
        int lx = ax + (int)(sinf(rad - 2.3f) * sz);
        int ly = ay - (int)(cosf(rad - 2.3f) * sz);
        int rx = ax + (int)(sinf(rad + 2.3f) * sz);
        int ry = ay - (int)(cosf(rad + 2.3f) * sz);
        M5Cardputer.Display.fillTriangle(nx, ny, lx, ly, rx, ry, C_PLANE);
    }

    // Header
    char title[48];
    snprintf(title, sizeof(title), "%s - %d flights",
             MENU_ITEMS[(int)search_mode], (int)flights.size());
    drawHeader(title);

    drawStatusBar("Any key=back");
}
