#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WebSocketsClient.h>
#include <ArduinoJson.h>

#include "score_event.h"
#include "supabase_client.h"
#include "secrets.h"

namespace {

WebSocketsClient webSocket;
uint32_t lastHeartbeat = 0;
bool initialSyncDone = false;

void sendJoinMessage() {
    JsonDocument doc;
    doc["topic"] = "realtime:public:game_scores";
    doc["event"] = "phx_join";
    
    JsonObject payload = doc["payload"].to<JsonObject>();
    JsonObject config = payload["config"].to<JsonObject>();
    JsonArray postgres_changes = config["postgres_changes"].to<JsonArray>();
    
    JsonObject filter = postgres_changes.add<JsonObject>();
    filter["event"] = "*";
    filter["schema"] = "public";
    filter["table"] = "game_scores";
    
    doc["ref"] = "1";
    
    String msg;
    serializeJson(doc, msg);
    webSocket.sendTXT(msg);
    Serial.printf("[Supabase WS] Sent join topic message\n");
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
    if (err) return;
    
    const char* event = doc["event"];
    if (event && strcmp(event, "postgres_changes") == 0) {
        JsonObject payload = doc["payload"];
        JsonObject data = payload["data"];
        JsonObject record = data["record"];
        if (record.isNull()) {
            record = data["new"];
        }
        
        if (!record.isNull()) {
            int toasters = record["toasters_score"] | 999;
            int poppers = record["poppers_score"] | 999;
            
            if (toasters != 999 && poppers != 999) {
                postScoreEventFromWeb(SCORE_TARGET_HOME, SCORE_ACTION_SET, toasters);
                postScoreEventFromWeb(SCORE_TARGET_AWAY, SCORE_ACTION_SET, poppers);
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
        case WStype_TEXT:
            parseIncomingMessage((char*)payload);
            break;
        default:
            break;
    }
}

void initialSync() {
    if (WiFi.status() != WL_CONNECTED) return;
    
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
            Serial.printf("[Supabase] Initial boot sync success. Toasters: %d, Poppers: %d\n", toasters, poppers);
            postScoreEventFromWeb(SCORE_TARGET_HOME, SCORE_ACTION_SET, toasters);
            postScoreEventFromWeb(SCORE_TARGET_AWAY, SCORE_ACTION_SET, poppers);
            initialSyncDone = true;
        }
    } else {
        Serial.printf("[Supabase] Boot sync HTTP status: %d\n", httpCode);
    }
    http.end();
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
