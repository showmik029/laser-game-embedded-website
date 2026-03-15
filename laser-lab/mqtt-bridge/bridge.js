/* eslint-disable @typescript-eslint/no-require-imports */
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
  const rawMode = String(msg.game_mode ?? msg.mode ?? msg.gm ?? "classic")
    .toLowerCase()
    .trim();

  // MQTT payload and DB now use friendly mode names.
  let game_mode = "classic";
  if (["time_trial", "time-trial", "time trial"].includes(rawMode)) {
    game_mode = "time_trial";
  } else if (
    ["photon_panic", "photon-panic", "photon panic"].includes(rawMode)
  ) {
    game_mode = "photon_panic";
  } else if (rawMode === "classic") {
    game_mode = "classic";
  }
  const score = Number(msg.score ?? msg.score_delta ?? msg.sd ?? 0);
  const point = Number(msg.point ?? msg.point_delta ?? msg.pt ?? 0);
  const ammo_used = Number(msg.ammo_used ?? msg.ammo_used_delta ?? msg.au ?? 0);
  const time_speed = Number(msg.time_speed ?? msg.speed ?? msg.ts ?? 0);

  return {
    player_name,
    game_mode,
    score,
    point,
    ammo_used,
    time_speed,
  };
}

async function insertPlayerResult(player_name, game_mode, score, point, ammo_used, time_speed) {
  const { error: insertError } = await supabase.from("players").insert({
    player_name,
    game_mode,
    score,
    point,
    time_speed,
    ammo_used,
  });

  if (insertError) {
    throw new Error(`insert failed: ${insertError.message}`);
  }

  return { action: "inserted", score, point, ammo_used, time_speed };
}

mqttClient.on("message", async (topic, payload) => {
  try {
    const event = parseEvent(payload);
    const result = await insertPlayerResult(
      event.player_name,
      event.game_mode,
      event.score,
      event.point,
      event.ammo_used,
      event.time_speed
    );

    console.log(
      `[bridge] ${result.action} ${event.player_name} | ` +
        `mode=${event.game_mode} score=${event.score} point=${event.point} ` +
        `ammo_used=${event.ammo_used} time_speed=${event.time_speed} | topic=${topic}`
    );
  } catch (err) {
    console.error("[bridge] message handling error:", err.message);
  }
});

console.log("[bridge] running");
