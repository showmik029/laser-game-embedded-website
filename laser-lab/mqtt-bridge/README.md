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
mosquitto_pub -h 127.0.0.1 -p 1883 -t laserlabs -m "{\"player_name\":\"Player A\",\"score_delta\":10,\"accuracy_delta\":0.5}"
```

If your Pico sends compact keys, those are also supported:

```json
{"p":"Player A","sd":10,"ad":0.5}
```
