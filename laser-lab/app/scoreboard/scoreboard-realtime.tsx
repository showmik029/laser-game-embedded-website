"use client";

import { useEffect, useMemo, useState } from "react";
import { createSupabaseBrowserClient } from "@/lib/supabase-browser";
import type { Player } from "@/lib/player-types";

type Props = {
  initialPlayers: Player[];
  initialError: string | null;
};

type Tab = "classic" | "time_trial" | "photon_panic" | "best";

const modeTabs: { key: Tab; label: string }[] = [
  { key: "classic", label: "Classic" },
  { key: "time_trial", label: "Time Trial" },
  { key: "photon_panic", label: "Photon Panic" },
  { key: "best", label: "Best Scores" },
];

const modeKeys: Array<"classic" | "time_trial" | "photon_panic"> = [
  "classic",
  "time_trial",
  "photon_panic",
];

function normalizeMode(
  mode: string | null | undefined
): "classic" | "time_trial" | "photon_panic" {
  if (mode === "classic" || mode === "mode_1") return "classic";
  if (mode === "time_trial" || mode === "mode_2") return "time_trial";
  if (mode === "photon_panic" || mode === "mode_3") return "photon_panic";
  return "classic";
}

function modeLabel(mode: "classic" | "time_trial" | "photon_panic") {
  if (mode === "classic") return "Classic";
  if (mode === "time_trial") return "Time Trial";
  return "Photon Panic";
}

function compareByScoreThenTime(a: Player, b: Player) {
  const scoreDiff = Number(b.score) - Number(a.score);
  if (scoreDiff !== 0) {
    return scoreDiff;
  }
  return new Date(b.updated_at).getTime() - new Date(a.updated_at).getTime();
}

function compareByLatestThenScore(a: Player, b: Player) {
  const timeDiff =
    new Date(b.updated_at).getTime() - new Date(a.updated_at).getTime();
  if (timeDiff !== 0) {
    return timeDiff;
  }
  return Number(b.score) - Number(a.score);
}

function formatTime(timestamp: string) {
  return new Date(timestamp).toLocaleString();
}

function getRankTheme(rank: number) {
  if (rank === 1) {
    return {
      cardBackground:
        "linear-gradient(145deg, rgba(120,90,20,0.9), rgba(70,45,5,0.82))",
      cardBorder: "1px solid rgba(255,215,0,0.65)",
      cardShadow: "0 10px 20px rgba(255,215,0,0.2)",
      badgeBackground: "rgba(255,215,0,0.26)",
      badgeBorder: "1px solid rgba(255,215,0,0.72)",
      badgeColor: "#fff8dc",
    };
  }

  if (rank === 2) {
    return {
      cardBackground:
        "linear-gradient(145deg, rgba(110,118,134,0.9), rgba(59,65,76,0.82))",
      cardBorder: "1px solid rgba(192,192,192,0.65)",
      cardShadow: "0 10px 20px rgba(192,192,192,0.18)",
      badgeBackground: "rgba(192,192,192,0.26)",
      badgeBorder: "1px solid rgba(192,192,192,0.72)",
      badgeColor: "#f8fafc",
    };
  }

  if (rank === 3) {
    return {
      cardBackground:
        "linear-gradient(145deg, rgba(111,62,38,0.92), rgba(74,41,24,0.84))",
      cardBorder: "1px solid rgba(205,127,50,0.7)",
      cardShadow: "0 10px 20px rgba(205,127,50,0.2)",
      badgeBackground: "rgba(205,127,50,0.28)",
      badgeBorder: "1px solid rgba(205,127,50,0.74)",
      badgeColor: "#fff7ed",
    };
  }

  return {
    cardBackground:
      "linear-gradient(145deg, rgba(17,24,39,0.9), rgba(30,41,59,0.78))",
    cardBorder: "1px solid rgba(148,163,184,0.38)",
    cardShadow: "0 8px 18px rgba(2,6,23,0.34)",
    badgeBackground: "rgba(14,165,233,0.22)",
    badgeBorder: "1px solid rgba(56,189,248,0.55)",
    badgeColor: "#e0f2fe",
  };
}

export default function ScoreboardRealtime({
  initialPlayers,
  initialError,
}: Props) {
  const [players, setPlayers] = useState<Player[]>(initialPlayers);
  const [error, setError] = useState<string | null>(initialError);
  const [activeTab, setActiveTab] = useState<Tab>("classic");
  const supabase = useMemo(() => createSupabaseBrowserClient(), []);

  useEffect(() => {
    let isMounted = true;

    async function refreshPlayers() {
      const { data, error: fetchError } = await supabase
        .from("players")
        .select(
          "id, player_name, game_mode, score, point, time_speed, ammo_used, updated_at"
        )
        .order("score", { ascending: false })
        .order("updated_at", { ascending: false });

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

  const modePlayers =
    activeTab === "best"
      ? []
      : players
          .filter((p) => normalizeMode(p.game_mode) === activeTab)
          .sort(compareByLatestThenScore)
          .slice(0, 10);

  const bestByMode = modeKeys.map((mode) => ({
    mode,
    rows: players
      .filter((p) => normalizeMode(p.game_mode) === mode)
      .sort(compareByScoreThenTime)
      .slice(0, 10),
  }));

  function renderPlayerCard(
    player: Player,
    rank: number,
    showMode = false,
    highlightTop3 = false
  ) {
    const theme = highlightTop3 ? getRankTheme(rank) : getRankTheme(0);

    return (
      <article
        key={`${player.id}-${rank}`}
        style={{
          background: theme.cardBackground,
          border: theme.cardBorder,
          borderRadius: "14px",
          padding: "14px",
          boxShadow: theme.cardShadow,
          minHeight: "138px",
        }}
      >
        <div
          style={{
            display: "flex",
            justifyContent: "space-between",
            alignItems: "center",
            marginBottom: "10px",
          }}
        >
          <strong style={{ fontSize: "17px", color: "#f8fafc" }}>
            {player.player_name}
          </strong>
          <span
            style={{
              fontSize: "12px",
              color: theme.badgeColor,
              background: theme.badgeBackground,
              border: theme.badgeBorder,
              borderRadius: "999px",
              padding: "2px 9px",
            }}
          >
            #{rank}
          </span>
        </div>

        {showMode ? (
          <p style={{ margin: "0 0 8px", color: "#cbd5e1", fontSize: "13px" }}>
            Mode: {modeLabel(normalizeMode(player.game_mode))}
          </p>
        ) : null}

        <p style={{ margin: "0 0 6px", color: "#dbeafe" }}>
          Score: <strong>{player.score}</strong>
        </p>
        <p style={{ margin: "0 0 6px", color: "#cbd5e1", fontSize: "14px" }}>
          Point: {player.point} | Speed: {player.time_speed}
        </p>
        <p style={{ margin: "0", color: "#94a3b8", fontSize: "13px" }}>
          Ammo: {player.ammo_used} | {formatTime(player.updated_at)}
        </p>
      </article>
    );
  }

  return (
    <div
      style={{
        width: "min(1100px, 100%)",
        margin: "0 auto",
        border: "1px solid rgba(148,163,184,0.35)",
        borderRadius: "18px",
        padding: "18px",
        background:
          "linear-gradient(180deg, rgba(15,23,42,0.9), rgba(2,6,23,0.76))",
        boxShadow: "0 18px 35px rgba(2,6,23,0.35)",
      }}
    >
      <div
        style={{
          display: "flex",
          flexWrap: "wrap",
          gap: "8px",
          marginBottom: "16px",
          justifyContent: "center",
        }}
      >
        {modeTabs.map((tab) => (
          <button
            key={tab.key}
            type="button"
            onClick={() => setActiveTab(tab.key)}
            style={{
              border: "1px solid rgba(125,211,252,0.5)",
              borderRadius: "999px",
              padding: "8px 14px",
              background:
                activeTab === tab.key
                  ? "linear-gradient(90deg, rgba(14,165,233,0.3), rgba(147,51,234,0.3))"
                  : "rgba(15,23,42,0.7)",
              color: "#f8fafc",
              cursor: "pointer",
              fontWeight: 600,
            }}
          >
            {tab.label}
          </button>
        ))}
      </div>

      {activeTab !== "best" ? (
        <div
          style={{
            display: "grid",
            gridTemplateColumns: "repeat(auto-fit, minmax(220px, 1fr))",
            gap: "12px",
          }}
        >
          {modePlayers.map((player, index) =>
            renderPlayerCard(player, index + 1, false, false)
          )}
        </div>
      ) : (
        bestByMode.map((group) => (
          <div key={group.mode} style={{ marginBottom: "16px" }}>
            <h3
              style={{
                fontSize: "24px",
                marginBottom: "10px",
                color: "#e2e8f0",
                fontWeight: 800,
              }}
            >
              Best {modeLabel(group.mode)}
            </h3>
            {group.rows.length === 0 ? (
              <p style={{ color: "#94a3b8" }}>
                No entries for {modeLabel(group.mode)} yet.
              </p>
            ) : (
              <div
                style={{
                  display: "grid",
                  gridTemplateColumns: "repeat(auto-fit, minmax(220px, 1fr))",
                  gap: "12px",
                }}
              >
                {group.rows.map((player, index) =>
                  renderPlayerCard(player, index + 1, true, true)
                )}
              </div>
            )}
          </div>
        ))
      )}

      {activeTab !== "best" && modePlayers.length === 0 && !error ? (
        <p style={{ color: "#94a3b8" }}>No entries for this tab yet.</p>
      ) : null}
      {error ? <p style={{ color: "#fb7185" }}>Data error: {error}</p> : null}
    </div>
  );
}
