# Feature Evaluation: I.5 — Player Wanted Level
**Evaluated by:** Simulation Critic / Systemic Analyst

## Ratings
- **NOVELTY:** 7/10 — While common in games, a per-faction, systemic terminal-based wanted system is a rare find. It moves away from "police vs player" into "factions vs player".
- **EXCITEMENT:** 8/10 — Adds immediate stakes to every interaction and theft. The visual feedback of faction-coded glyphs will heighten the tension.
- **SYSTEMIC DEPTH:** 9/10 — Connects existing Crime (I.1-I.4), Factions (G, 5.4), and Guard Response (I.6) systems. It utilizes FOV, Information Propagation, and the Economic layer.

## Analysis & Recommendations
1. **Graduated Response:** Ensure the system is not binary. 
   - Levels 1-2: Passive suspicion (guards trail the player).
   - Level 3: Active questioning/search.
   - Level 4-5: Pursuit and lethal force.
2. **Notoriety vs. Wanted Level:** Notoriety should be a long-term decay value (global), while Wanted Level is short-term (faction-local). High notoriety should increase the rate of Wanted Level gain.
3. **Causal Connectivity:** If the player is wanted by the 'Corporate' faction, 'Syndicate' members might actually be more friendly or offer 'safe houses'. This leverages the social graph (Phase G).
4. **Visual Feedback:** The HUD should use specific glyphs for each faction (e.g., a corporate logo vs. a syndicate skull) to immediately show who is looking for the player.

## Alignment with Roadmap
This feature is a critical bridge to Phase I.6 (Guard Response System) and provides a reason for players to utilize the Sewer layer (Phase M) and the Information layer (Phase N).
