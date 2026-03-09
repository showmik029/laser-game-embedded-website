export type Player = {
  id: string;
  player_name: string;
  game_mode: "classic" | "time_trial" | "photon_panic" | string;
  score: number;
  point: number;
  time_speed: number;
  ammo_used: number;
  updated_at: string;
};
