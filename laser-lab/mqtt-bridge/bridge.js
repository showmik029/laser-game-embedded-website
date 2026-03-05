require("dotenv").config();
const mqtt = require("mqtt");
const { createClient } = require("@supabase/supabase-js");

const {
  MQTT_URL,
  MQTT_TOPIC,
  MQTT_USER,
  MQTT_PASS,
  SUPABASE_URL,
  SUPABASE_SERVICE_ROLE_KEY,
} = process.env;

if (!MQTT_URL || !MQTT_TOPIC) {
  throw new Error("Missing MQTT_URL or MQTT_TOPIC in mqtt-bridge/.env");
}
if (!SUPABASE_URL || !SUPABASE_SERVICE_ROLE_KEY) {
  throw new Error(
    "Missing SUPABASE_URL or SUPABASE_SERVICE_ROLE_KEY in mqtt-bridge/.env"
  );
}

const supabase = createClient(SUPABASE_URL, SUPABASE_SERVICE_ROLE_KEY);

const mqttClient = mqtt.connect(MQTT_URL, {
  username: MQTT_USER || undefined,
  password: MQTT_PASS || undefined,
  reconnectPeriod: 2000,
});

mqttClient.on("connect", () => {
  console.log("[mqtt] connected");
  mqttClient.subscribe(MQTT_TOPIC, { qos: 1 }, (err) => {
    if (err) {
      console.error("[mqtt] subscribe failed:", err.message);
      return;
    }
    console.log(`[mqtt] subscribed to ${MQTT_TOPIC}`);
  });
});

mqttClient.on("error", (err) => {
  console.error("[mqtt] error:", err.message);
});

function parseEvent(rawPayload) {
  const msg = JSON.parse(rawPayload.toString());

  // Supports both full payload keys and compact keys from embedded firmware.
  const player_name = msg.player_name || msg.p || msg.player || "Unknown";
  const score_delta = Number(msg.score_delta ?? msg.sd ?? 0);
  const accuracy_delta = Number(msg.accuracy_delta ?? msg.ad ?? 0);

  return { player_name, score_delta, accuracy_delta, raw: msg };
}

async function upsertPlayerScore(player_name, score_delta, accuracy_delta) {
  const { data: existing, error: selectError } = await supabase
    .from("players")
    .select("id, score, accuracy")
    .eq("player_name", player_name)
    .maybeSingle();

  if (selectError) {
    throw new Error(`select failed: ${selectError.message}`);
  }

  if (!existing) {
    const { error: insertError } = await supabase.from("players").insert({
      player_name,
      score: score_delta,
      accuracy: accuracy_delta,
    });
    if (insertError) {
      throw new Error(`insert failed: ${insertError.message}`);
    }
    return { action: "inserted", score: score_delta, accuracy: accuracy_delta };
  }

  const nextScore = Number(existing.score) + score_delta;
  const nextAccuracy = Number(existing.accuracy) + accuracy_delta;

  const { error: updateError } = await supabase
    .from("players")
    .update({
      score: nextScore,
      accuracy: nextAccuracy,
      updated_at: new Date().toISOString(),
    })
    .eq("id", existing.id);

  if (updateError) {
    throw new Error(`update failed: ${updateError.message}`);
  }

  return { action: "updated", score: nextScore, accuracy: nextAccuracy };
}

mqttClient.on("message", async (topic, payload) => {
  try {
    const event = parseEvent(payload);
    const result = await upsertPlayerScore(
      event.player_name,
      event.score_delta,
      event.accuracy_delta
    );

    console.log(
      `[bridge] ${result.action} ${event.player_name} | ` +
        `score_delta=${event.score_delta} accuracy_delta=${event.accuracy_delta} | topic=${topic}`
    );
  } catch (err) {
    console.error("[bridge] message handling error:", err.message);
  }
});

console.log("[bridge] running");
