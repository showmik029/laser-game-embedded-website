import Link from "next/link";
import { getPlayers } from "@/lib/players";
import ScoreboardRealtime from "./scoreboard-realtime";

export const dynamic = "force-dynamic";

export default async function ScoreboardPage() {
  const { players, error } = await getPlayers();

  return (
    <main style={{ padding: "100px", fontFamily: "Arial, sans-serif" }}>
      <h1 style={{ fontSize: "40px", marginBottom: "20px" }}>
        Full Scoreboard
      </h1>

      <ScoreboardRealtime initialPlayers={players} initialError={error} />

      <div style={{ marginTop: "24px" }}>
        <Link href="/">Back to Home</Link>
      </div>
    </main>
  );
}
