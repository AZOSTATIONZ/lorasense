// HTTP Digest Authentication with role-based access control
// Roles: technician (full access) and user (dashboard + control only)

#pragma once
#include <Arduino.h>
#include <WebServer.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include "MD5Builder.h"

enum AuthRole : uint8_t {
  ROLE_NONE       = 0,
  ROLE_USER       = 1,
  ROLE_TECHNICIAN = 2,
};

const char *AUTH_REALM = "LoRaSense";
const char *AUTH_FILE  = "/config/auth.json";

struct AuthCredential {
  char username[32];
  char password[32];
  AuthRole role;
  bool active;
};

const int MAX_CREDENTIALS = 4;

struct AuthManager {
  AuthCredential credentials[MAX_CREDENTIALS];
  int count;
  bool enabled;
  uint8_t failedAttempts;
  uint32_t lockoutUntil;

  AuthManager() : count(0), enabled(false), failedAttempts(0), lockoutUntil(0) {}

  void begin() {
    if (!LittleFS.exists(AUTH_FILE)) {
      enabled = false;
      count = 0;
      return;
    }
    File f = LittleFS.open(AUTH_FILE, "r");
    if (!f) { enabled = false; return; }

    JsonDocument doc;
    if (deserializeJson(doc, f) != DeserializationError::Ok) {
      f.close();
      enabled = false;
      return;
    }
    f.close();

    enabled = doc["enabled"] | false;
    count = 0;
    JsonArray arr = doc["users"].as<JsonArray>();
    for (JsonObject u : arr) {
      if (count >= MAX_CREDENTIALS) break;
      AuthCredential &c = credentials[count];
      strlcpy(c.username, u["username"] | "", sizeof(c.username));
      strlcpy(c.password, u["password"] | "", sizeof(c.password));
      c.role = (AuthRole)(u["role"] | 1);
      c.active = u["active"] | true;
      if (c.username[0] && c.password[0]) count++;
    }
  }

  bool save() {
    LittleFS.mkdir("/config");
    File f = LittleFS.open(AUTH_FILE, "w");
    if (!f) return false;

    JsonDocument doc;
    doc["_v"] = 1;
    doc["enabled"] = enabled;
    JsonArray arr = doc["users"].to<JsonArray>();
    for (int i = 0; i < count; i++) {
      AuthCredential &c = credentials[i];
      JsonObject u = arr.add<JsonObject>();
      u["username"] = c.username;
      u["password"] = c.password;
      u["role"]     = (int)c.role;
      u["active"]   = c.active;
    }
    serializeJson(doc, f);
    f.close();
    return true;
  }

  void setDefaults(const char *techUser, const char *techPass,
                   const char *userUser, const char *userPass) {
    count = 0;
    // Technician account
    AuthCredential &t = credentials[count++];
    strlcpy(t.username, techUser, sizeof(t.username));
    strlcpy(t.password, techPass, sizeof(t.password));
    t.role = ROLE_TECHNICIAN;
    t.active = true;
    // User account
    AuthCredential &u = credentials[count++];
    strlcpy(u.username, userUser, sizeof(u.username));
    strlcpy(u.password, userPass, sizeof(u.password));
    u.role = ROLE_USER;
    u.active = true;

    enabled = true;
    save();
  }

  // Check if a request is authenticated for the required role
  // Returns true if access is granted (auth disabled, or credentials valid with sufficient role)
  bool check(WebServer &server, AuthRole required) {
    if (!enabled) return true;
    if (count == 0) return true;

    // Rate limiting: 5 failures → 60s lockout
    if (failedAttempts >= 5 && millis() < lockoutUntil) {
      server.send(429, "application/json", "{\"error\":\"too many attempts, try again later\"}");
      return false;
    }
    if (failedAttempts >= 5 && millis() >= lockoutUntil) {
      failedAttempts = 0;
    }

    // Try each credential
    for (int i = 0; i < count; i++) {
      AuthCredential &c = credentials[i];
      if (!c.active) continue;
      if (c.role < required) continue;
      if (server.authenticate(c.username, c.password)) {
        failedAttempts = 0;
        return true;
      }
    }

    // Also check lower-role credentials (they authenticate but we deny)
    for (int i = 0; i < count; i++) {
      AuthCredential &c = credentials[i];
      if (!c.active) continue;
      if (c.role >= required) continue;
      if (server.authenticate(c.username, c.password)) {
        failedAttempts = 0;
        server.send(403, "application/json", "{\"error\":\"insufficient permissions\"}");
        return false;
      }
    }

    failedAttempts++;
    if (failedAttempts >= 5) {
      lockoutUntil = millis() + 60000;
    }
    server.requestAuthentication(DIGEST_AUTH, AUTH_REALM, "Authentication required");
    return false;
  }

  AuthRole identifyRole(WebServer &server) {
    if (!enabled || count == 0) return ROLE_TECHNICIAN;
    for (int i = 0; i < count; i++) {
      AuthCredential &c = credentials[i];
      if (!c.active) continue;
      if (server.authenticate(c.username, c.password)) return c.role;
    }
    return ROLE_NONE;
  }
};
