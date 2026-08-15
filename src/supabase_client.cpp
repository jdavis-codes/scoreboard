#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WebSocketsClient.h>
#include <ArduinoJson.h>

#include "score_event.h"
#include "game_mode_event.h"
#include "supabase_client.h"
#include "secrets.h"

namespace {

WebSocketsClient webSocket;
uint32_t lastHeartbeat = 0;
bool initialSyncDone = false;

void sendJoinMessage() {
    // Join game_scores table changes
    {
        JsonDocument doc;
        doc["topic"] = "realtime:public:game_scores";
        doc["event"] = "phx_join";
        
        JsonObject payload = doc["payload"].to<JsonObject>();
        JsonObject config = payload["config"].to<JsonObject>();
        JsonObject broadcast = config["broadcast"].to<JsonObject>();
        broadcast["ack"] = false;
        broadcast["self"] = false;
        JsonObject presence = config["presence"].to<JsonObject>();
        presence["key"] = "";
        config["private"] = false;
        JsonArray postgres_changes = config["postgres_changes"].to<JsonArray>();
        
        JsonObject filter = postgres_changes.add<JsonObject>();
        filter["event"] = "*";
        filter["schema"] = "public";
        filter["table"] = "game_scores";
        
        doc["ref"] = "1";
        
        String msg;
        serializeJson(doc, msg);
        webSocket.sendTXT(msg);
    }
    
    // Join game_modes table changes
    {
        JsonDocument doc;
        doc["topic"] = "realtime:public:game_modes";
        doc["event"] = "phx_join";
        
        JsonObject payload = doc["payload"].to<JsonObject>();
        JsonObject config = payload["config"].to<JsonObject>();
        JsonObject broadcast = config["broadcast"].to<JsonObject>();
        broadcast["ack"] = false;
        broadcast["self"] = false;
        JsonObject presence = config["presence"].to<JsonObject>();
        presence["key"] = "";
        config["private"] = false;
        JsonArray postgres_changes = config["postgres_changes"].to<JsonArray>();
        
        JsonObject filter = postgres_changes.add<JsonObject>();
        filter["event"] = "*";
        filter["schema"] = "public";
        filter["table"] = "game_modes";
        
        doc["ref"] = "2";
        
        String msg;
        serializeJson(doc, msg);
        webSocket.sendTXT(msg);
    }
    
    Serial.printf("[Supabase WS] Sent join topic messages for game_scores and game_modes\n");
}

void sendHeartbeat() {
    JsonDocument doc;
    doc["topic"] = "phoenix";
    doc["event"] = "heartbeat";
    doc["payload"] = JsonObject();
    doc["ref"] = "hb";
    
    String msg;
    serializeJson(doc, msg);
    webSocket.sendTXT(msg);
}

void parseIncomingMessage(char* jsonStr) {
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, jsonStr);
    if (err) {
        Serial.printf("[Supabase WS] JSON parse error: %s\n", err.c_str());
        return;
    }
    
    const char* topic = doc["topic"];
    const char* eventName = doc["event"];

    if (eventName && strcmp(eventName, "phx_reply") == 0) {
        const char* status = doc["payload"]["status"] | "";
        Serial.printf("[Supabase WS] phx_reply topic=%s status=%s\n",
                      topic ? topic : "",
                      status);

        if (strcmp(status, "ok") != 0) {
            JsonObject response = doc["payload"]["response"];
            String responseStr;
            serializeJson(response, responseStr);
            Serial.printf("[Supabase WS] phx_reply error response=%s\n", responseStr.c_str());
        }
        return;
    }

    if (eventName && strcmp(eventName, "system") == 0) {
        String payloadStr;
        serializeJson(doc["payload"], payloadStr);
        Serial.printf("[Supabase WS] system topic=%s payload=%s\n",
                      topic ? topic : "",
                      payloadStr.c_str());
        return;
    }

    if (eventName && strcmp(eventName, "postgres_changes") != 0) {
        Serial.printf("[Supabase WS] Ignored event=%s topic=%s\n", eventName, topic ? topic : "");
        return;
    }

    if (eventName && strcmp(eventName, "postgres_changes") == 0) {
        JsonObject payload = doc["payload"];
        JsonObject data = payload["data"];
        JsonObject record = data["record"];
        if (record.isNull()) {
            record = data["new"];
        }
        
        if (!record.isNull()) {
            if (topic && strcmp(topic, "realtime:public:game_scores") == 0) {
                int toasters = record["toasters_score"] | 0;
                int poppers = record["poppers_score"] | 0;
                Serial.printf("[Supabase WS] game_scores change: toasters=%d poppers=%d\n", toasters, poppers);
                postScoreEventFromWeb(SCORE_TARGET_HOME, SCORE_ACTION_SET, toasters);
                postScoreEventFromWeb(SCORE_TARGET_AWAY, SCORE_ACTION_SET, poppers);
            } else if (topic && strcmp(topic, "realtime:public:game_modes") == 0) {
                const char* mode = record["current_mode"] | "NORMAL";
                const char* activeEvent = record["active_event"]; // can be null
                const char* triggeredAt = record["event_triggered_at"]; // can be null
                Serial.printf("[Supabase WS] game_modes change: mode=%s event=%s triggered_at=%s\n",
                              mode,
                              activeEvent ? activeEvent : "",
                              triggeredAt ? triggeredAt : "");
                postGameModeEvent(mode, activeEvent, triggeredAt);
            }
        }
    }
}

void webSocketEvent(WStype_t type, uint8_t * payload, size_t length) {
    switch (type) {
        case WStype_DISCONNECTED:
            Serial.println("[Supabase WS] Disconnected!");
            break;
        case WStype_CONNECTED:
            Serial.println("[Supabase WS] Connected!");
            sendJoinMessage();
            break;
        case WStype_ERROR:
            Serial.println("[Supabase WS] Error event");
            break;
        case WStype_PING:
            Serial.println("[Supabase WS] Ping");
            break;
        case WStype_PONG:
            Serial.println("[Supabase WS] Pong");
            break;
        case WStype_TEXT:
            Serial.printf("[Supabase WS] Incoming text len=%u\n", static_cast<unsigned>(length));
            parseIncomingMessage((char*)payload);
            break;
        default:
            Serial.printf("[Supabase WS] Event type=%d len=%u\n", static_cast<int>(type), static_cast<unsigned>(length));
            break;
    }
}

void initialSync() {
    if (WiFi.status() != WL_CONNECTED) return;
    
    // Get scores
    {
        HTTPClient http;
        String url = "https://" + String(SB_URL) + "/rest/v1/game_scores?id=eq.1";
        http.begin(url);
        http.addHeader("apikey", SB_ANON_KEY);
        http.addHeader("Authorization", "Bearer " + String(SB_ANON_KEY));
        
        int httpCode = http.GET();
        if (httpCode == 200) {
            String payload = http.getString();
            JsonDocument doc;
            DeserializationError err = deserializeJson(doc, payload);
            if (!err && doc.is<JsonArray>() && doc.size() > 0) {
                JsonObject obj = doc[0];
                int toasters = obj["toasters_score"] | 0;
                int poppers = obj["poppers_score"] | 0;
                Serial.printf("[Supabase] Initial boot score sync success. Toasters: %d, Poppers: %d\n", toasters, poppers);
                postScoreEventFromWeb(SCORE_TARGET_HOME, SCORE_ACTION_SET, toasters);
                postScoreEventFromWeb(SCORE_TARGET_AWAY, SCORE_ACTION_SET, poppers);
            } else if (err) {
                Serial.printf("[Supabase] Failed parsing initial score payload: %s\n", err.c_str());
            }
        } else {
            Serial.printf("[Supabase] Boot score sync HTTP status: %d\n", httpCode);
        }
        http.end();
    }

    // Get modes
    {
        HTTPClient http;
        String url = "https://" + String(SB_URL) + "/rest/v1/game_modes?id=eq.1";
        http.begin(url);
        http.addHeader("apikey", SB_ANON_KEY);
        http.addHeader("Authorization", "Bearer " + String(SB_ANON_KEY));
        
        int httpCode = http.GET();
        if (httpCode == 200) {
            String payload = http.getString();
            JsonDocument doc;
            DeserializationError err = deserializeJson(doc, payload);
            if (!err && doc.is<JsonArray>() && doc.size() > 0) {
                JsonObject obj = doc[0];
                const char* mode = obj["current_mode"] | "NORMAL";
                const char* event = obj["active_event"];
                const char* triggeredAt = obj["event_triggered_at"];
                Serial.printf("[Supabase] Initial boot mode sync success. Mode: %s, Event: %s\n", mode, event ? event : "none");
                postGameModeEvent(mode, event, triggeredAt);
                initialSyncDone = true;
            } else if (err) {
                Serial.printf("[Supabase] Failed parsing initial mode payload: %s\n", err.c_str());
            }
        } else {
            Serial.printf("[Supabase] Boot mode sync HTTP status: %d\n", httpCode);
        }
        http.end();
    }
}

void transmitScore(int homeScore, int awayScore) {
    if (WiFi.status() != WL_CONNECTED) return;
    
    HTTPClient http;
    String url = "https://" + String(SB_URL) + "/rest/v1/rpc/update_game_score";
    http.begin(url);
    http.addHeader("apikey", SB_ANON_KEY);
    http.addHeader("Authorization", "Bearer " + String(SB_ANON_KEY));
    http.addHeader("Content-Type", "application/json");
    
    JsonDocument doc;
    doc["p_toasters"] = homeScore;
    doc["p_poppers"] = awayScore;
    doc["p_password"] = SB_PASSWORD;
    
    String body;
    serializeJson(doc, body);
    
    int httpCode = http.POST(body);
    if (httpCode == 200 || httpCode == 204) {
        Serial.printf("[Supabase] Transmitted score successfully. Home: %d, Away: %d\n", homeScore, awayScore);
    } else {
        Serial.printf("[Supabase] Score transmission failed, status: %d\n", httpCode);
    }
    http.end();
}

void supabaseTask(void* pvParameters) {
    initSupabaseQueue();
    
    // Set reconnect interval to 5 sec
    webSocket.setReconnectInterval(5000);
    webSocket.beginSSL(SB_URL, 443, "/realtime/v1/websocket?apikey=" SB_ANON_KEY "&vsn=1.0.0");
    webSocket.onEvent(webSocketEvent);
    
    while (1) {
        if (WiFi.status() == WL_CONNECTED) {
            if (!initialSyncDone) {
                initialSync();
            }
            webSocket.loop();
            
            // Check heartbeat
            if (millis() - lastHeartbeat >= 30000) {
                lastHeartbeat = millis();
                sendHeartbeat();
                Serial.println("[Supabase WS] Heartbeat sent");
            }
        } else {
            static unsigned long lastWifiLog = 0;
            if (millis() - lastWifiLog > 5000) {
                lastWifiLog = millis();
                Serial.printf("[Supabase] WiFi disconnected. status=%d\n", WiFi.status());
            }
        }
        
        // Check transmit queue
        ScoreUpdate update;
        if (xQueueReceive(supabaseQueue, &update, 0) == pdTRUE) {
            transmitScore(update.home, update.away);
        }
        
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

} // namespace

void setupSupabase() {
    xTaskCreate(&supabaseTask, "supabaseTask", 8192, NULL, 5, NULL);
}
