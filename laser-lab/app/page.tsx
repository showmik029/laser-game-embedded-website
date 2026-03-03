import Link from "next/link";
import { getPlayers } from "@/lib/players";

export const dynamic = "force-dynamic";

export default async function Home() {
  const { players, error } = await getPlayers();
  const previewPlayers = players.slice(0, 3);

  return (
    <main style={{ padding: "100px", fontFamily: "Arial, sans-serif" }}>
      <section style={{ marginBottom: "40px" }}>
        <h1 style={{ fontSize: "48px", marginBottom: "12px" }}>Laser Labs</h1>
        <p style={{ fontSize: "18px", maxWidth: "600px" }}>
          Track scores and accuracy in real time.
        </p>
      </section>

      <section>
        <h2 style={{ fontSize: "28px", marginBottom: "12px" }}>
          Score + Accuracy
        </h2>
        <Link
          href="/scoreboard"
          style={{
            display: "block",
            border: "1px solid #ccc",
            borderRadius: "12px",
            padding: "16px",
            textDecoration: "none",
            color: "inherit",
            maxWidth: "520px",
          }}
        >
          <p style={{ marginBottom: "8px", fontWeight: 600 }}>
            Click to open full scoreboard
          </p>
          {previewPlayers.map((player) => (
            <p key={player.id}>
              {player.player_name} - Score: {player.score} | Accuracy:{" "}
              {player.accuracy}%
            </p>
          ))}
          {previewPlayers.length === 0 && !error ? <p>No players yet.</p> : null}
          {error ? <p style={{ color: "crimson" }}>Data error: {error}</p> : null}
        </Link>
      </section>
    </main>
  );
}
