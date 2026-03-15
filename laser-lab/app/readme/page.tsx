import Link from "next/link";

export default function ReadmePage() {
  return (
    <main style={{ padding: "40px", fontFamily: "Arial, sans-serif" }}>
      <h1 className="ll-title" style={{ fontSize: "42px", marginBottom: "16px" }}>
        Project Readme
      </h1>
      <p style={{ color: "var(--muted)", lineHeight: 1.6, maxWidth: "900px" }}>
        Laser Labs is an embedded IoT laser-tag system with live game modes,
        score tracking, and Supabase-backed leaderboard updates via MQTT bridge.
      </p>
      <p style={{ color: "var(--muted)", lineHeight: 1.6, maxWidth: "900px" }}>
        Main stack: Next.js website, Supabase database, Mosquitto MQTT broker,
        and a local Node.js bridge that writes MQTT events into the database.
      </p>
      <div style={{ marginTop: "18px", display: "flex", gap: "10px", flexWrap: "wrap" }}>
        <Link
          href="/"
          style={{
            display: "inline-block",
            padding: "10px 18px",
            borderRadius: "999px",
            border: "1px solid var(--border)",
            background: "rgba(255,255,255,0.08)",
            color: "var(--foreground)",
            textDecoration: "none",
            fontWeight: 600,
          }}
        >
          Home
        </Link>
        <Link
          href="/scoreboard"
          style={{
            display: "inline-block",
            padding: "10px 18px",
            borderRadius: "999px",
            border: "1px solid var(--border)",
            background: "rgba(255,255,255,0.08)",
            color: "var(--foreground)",
            textDecoration: "none",
            fontWeight: 600,
          }}
        >
          Scoreboard
        </Link>
      </div>
    </main>
  );
}
