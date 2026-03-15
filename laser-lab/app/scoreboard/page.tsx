import Link from "next/link";
import { getPlayers } from "@/lib/players";
import ScoreboardRealtime from "./scoreboard-realtime";

export const dynamic = "force-dynamic";

export default async function ScoreboardPage() {
  const { players, error } = await getPlayers();

  return (
    <main
      style={{
        padding: "80px 20px 40px",
        fontFamily: "Arial, sans-serif",
        maxWidth: "1200px",
        margin: "0 auto",
      }}
    >
      <h1
        className="ll-title"
        style={{
          fontSize: "56px",
          marginBottom: "20px",
          textAlign: "center",
          fontWeight: 400,
        }}
      >
        Full Scoreboard
      </h1>

      <ScoreboardRealtime initialPlayers={players} initialError={error} />

      <div style={{ marginTop: "24px", textAlign: "center" }}>
        <Link
          href="/"
          style={{
            display: "inline-block",
            padding: "10px 18px",
            borderRadius: "999px",
            border: "1px solid rgba(125,211,252,0.5)",
            background:
              "linear-gradient(90deg, rgba(14,165,233,0.24), rgba(147,51,234,0.24))",
            color: "#f8fafc",
            textDecoration: "none",
            fontWeight: 700,
          }}
        >
          Back Home
        </Link>
      </div>
    </main>
  );
}
