#pragma once

// ── Location (default: center of continental US) ────────────────────────
#define DEFAULT_LAT 39.8283
#define DEFAULT_LON -98.5795

// ── Search radii in degrees (~1 deg ≈ 111 km) ──────────────────────────
#define RADIUS_NEAR     3.6    // "Near Me" ~250 miles / 402 km
#define RADIUS_CITY     3.0    // "By City" around selected airport
#define RADIUS_WIDE    15.0    // "By Carrier" / "Search" / "Long Flights"

// ── OpenSky Network API ─────────────────────────────────────────────────
#define OPENSKY_STATES_URL "https://opensky-network.org/api/states/all"
#define OPENSKY_ROUTES_URL "https://opensky-network.org/api/routes"

// ── Display ─────────────────────────────────────────────────────────────
#define SCREEN_W 240
#define SCREEN_H 135
#define FLIGHTS_PER_PAGE 5

// ── Limits ──────────────────────────────────────────────────────────────
#define MAX_FLIGHTS      200   // cap flights stored in memory
#define MAX_LIST_FLIGHTS  15   // max shown in filtered list

// ── Map refresh ─────────────────────────────────────────────────────────
#define MAP_REFRESH_MS  5000

// ── NVS ─────────────────────────────────────────────────────────────────
#define NVS_NAMESPACE "flighttrk"
