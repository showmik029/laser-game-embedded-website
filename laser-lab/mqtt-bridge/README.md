# MQTT Bridge

This service subscribes to MQTT messages and updates the `players` table in Supabase.

## 1) Install dependencies

```powershell
cd F:\Laser_Lab_Website\laser-lab\mqtt-bridge
npm install
```

## 2) Configure env

Create `.env` in this folder and copy values from `.env.example`.

Required:
- `MQTT_URL`
- `MQTT_TOPIC`
- `SUPABASE_URL`
- `SUPABASE_SERVICE_ROLE_KEY`

## 3) Run

```powershell
npm start
```

## 4) Test publish

In another terminal:

```powershell
mosquitto_pub -h 127.0.0.1 -p 1883 -t laserlabs -m "{\"player_name\":\"Player A\",\"game_mode\":\"classic\",\"score_delta\":10,\"point_delta\":2,\"time_speed\":1.84,\"ammo_used_delta\":1}"
```

If your Pico sends compact keys, those are also supported:

```json
{"p":"Player A","gm":"time_trial","sd":10,"pt":2,"ts":1.84,"au":1}
```

You can also send friendly mode names:

```json
{"p":"Player A","gm":"time_trial","sd":10,"pt":2,"ts":1.84,"au":1}
{"p":"Player A","gm":"photon_panic","sd":10,"pt":2,"ts":1.84,"au":1}
```

Current stats mapped to DB:
- `player_name`
- `game_mode` from MQTT payload: `classic`, `time_trial`, `photon_panic`
- `score` (+`score_delta`)
- `point` (+`point_delta`)
- `time_speed` (latest value)
- `ammo_used` (+`ammo_used_delta`)
