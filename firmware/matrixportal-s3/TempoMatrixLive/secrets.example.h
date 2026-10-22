#pragma once

// Copy to secrets.h in this same folder and set your own values.
// secrets.h is gitignored; never commit real Wi-Fi passwords or private certificates.
#define TEMPO_WIFI_SSID ""
#define TEMPO_WIFI_PASSWORD ""
#define TEMPO_API_BASE_URL ""

// "nba", "nfl", or "cfb". NBA uses today's America/New_York date once NTP syncs.
// For football, choose a real season/year/week manually; auto week discovery is future work.
#define TEMPO_LEAGUE "nba"
#define TEMPO_NBA_DATE ""        // optional override: YYYYMMDD
#define TEMPO_FOOTBALL_SEASON 2026
#define TEMPO_FOOTBALL_SEASONTYPE 2
#define TEMPO_FOOTBALL_WEEK 1

// Required only for HTTPS. Paste the issuing root CA PEM here (not a leaf
// certificate); never use setInsecure() for live score requests.
// This is public certificate data, not a secret, but keep deployment values local.
#define TEMPO_ROOT_CA ""
