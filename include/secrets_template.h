#pragma once

#include <stddef.h>

// Template secrets for public builds.
// Copy this file to include/secrets.h and replace all dummy values.

typedef struct {
  const char* ssid;
  const char* password;
} WifiCredential;

static const WifiCredential wifi_credentials[] = {
  {"YOUR_WIFI_1", "YOUR_PASSWORD_1"},
  {"YOUR_WIFI_2", "YOUR_PASSWORD_2"},
  {"YOUR_WIFI_3", "YOUR_PASSWORD_3"}
};

static constexpr size_t wifi_credentials_count =
    sizeof(wifi_credentials) / sizeof(wifi_credentials[0]);

// Future placeholders (unused by this project today).
static const char* mqtt_user_id = "DUMMY_MQTT_USER";
static const char* mqtt_password = "DUMMY_MQTT_PASSWORD";
static const char* mqtt_sub_topics[] = {
  "dummy/sub/topic"
};
static constexpr size_t mqtt_sub_topics_count =
    sizeof(mqtt_sub_topics) / sizeof(mqtt_sub_topics[0]);

static const char* mqtt_pub_topics[] = {
  "dummy/pub/topic"
};
static constexpr size_t mqtt_pub_topics_count =
    sizeof(mqtt_pub_topics) / sizeof(mqtt_pub_topics[0]);

static const char* smtp_server = "smtp.example.com";
static const char* smtp_user = "dummy@example.com";
static const char* smtp_password = "DUMMY_SMTP_PASSWORD";
