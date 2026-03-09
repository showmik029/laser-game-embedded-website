import { createSupabaseServerClient } from "./supabase";
import type { Player } from "./player-types";

export type { Player } from "./player-types";

export async function getPlayers() {
  try {
    const supabase = createSupabaseServerClient();
    const { data, error } = await supabase
      .from("players")
      .select(
        "id, player_name, game_mode, score, point, time_speed, ammo_used, updated_at"
      )
      .order("score", { ascending: false })
      .order("updated_at", { ascending: false });

    if (error) {
      throw new Error(error.message);
    }

    return { players: (data as Player[]) ?? [], error: null as string | null };
  } catch (err) {
    const message =
      err instanceof Error ? err.message : "Failed to load players.";
    return { players: [] as Player[], error: message };
  }
}
