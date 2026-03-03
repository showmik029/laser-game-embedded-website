"use client";

import { useEffect, useMemo, useState } from "react";
import { createSupabaseBrowserClient } from "@/lib/supabase-browser";
import type { Player } from "@/lib/player-types";

type Props = {
  initialPlayers: Player[];
  initialError: string | null;
};

export default function ScoreboardRealtime({
  initialPlayers,
  initialError,
}: Props) {
  const [players, setPlayers] = useState<Player[]>(initialPlayers);
  const [error, setError] = useState<string | null>(initialError);
  const supabase = useMemo(() => createSupabaseBrowserClient(), []);

  useEffect(() => {
    let isMounted = true;

    async function refreshPlayers() {
      const { data, error: fetchError } = await supabase
        .from("players")
        .select("id, player_name, score, accuracy, updated_at")
        .order("score", { ascending: false });

      if (!isMounted) {
        return;
      }

      if (fetchError) {
        setError(fetchError.message);
        return;
      }

      setPlayers((data as Player[]) ?? []);
      setError(null);
    }

    const channel = supabase
      .channel("players-live")
      .on(
        "postgres_changes",
        { event: "*", schema: "public", table: "players" },
        () => {
          void refreshPlayers();
        }
      )
      .subscribe();

    return () => {
      isMounted = false;
      void supabase.removeChannel(channel);
    };
  }, [supabase]);

  return (
    <div
      style={{
        border: "1px solid #ccc",
        borderRadius: "12px",
        padding: "16px",
        maxWidth: "620px",
      }}
    >
      <h2 style={{ fontSize: "24px", marginBottom: "12px" }}>Scores + Accuracy</h2>
      {players.map((player, index) => (
        <p key={player.id}>
          {index + 1}. {player.player_name} - Score: {player.score} | Accuracy:{" "}
          {player.accuracy}%
        </p>
      ))}
      {players.length === 0 && !error ? <p>No players yet.</p> : null}
      {error ? <p style={{ color: "crimson" }}>Data error: {error}</p> : null}
    </div>
  );
}
