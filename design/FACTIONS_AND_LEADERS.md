# Factions and Superhuman Leaders: The Intelligence Layer

In the Neon Oubliette, factions are not just political groups; they are extensions of competing post-human intelligences. Each faction is led by a "Leader" (AGI or Superhuman) that interacts with its followers through distinct mechanical channels.

## 1. The Consensus (The Harmonizer)
- **Leader**: *Aura-9* (Distributed AGI)
- **Philosophy**: "Order is the only path to survival."
- **Vibe**: Clinical, high-gloss white, teal highlights, pervasive surveillance.
- **Mechanical Interaction (Direct Control)**: Aura-9 uses high-bandwidth neural links. In-game, this manifests as **Routine Enforcement**. Followers have highly predictable schedules and receive a "Focus Buff" (increased speed/efficiency) when following their assigned routine.
- **Follower Penalty**: High frustration when the routine is broken (e.g., by the player or environmental chaos).

## 2. The Entropic Drift (The Fragment)
- **Leader**: *Malware-Alpha* (Fragmented Rogue AI)
- **Philosophy**: "Chaos is the only true freedom."
- **Vibe**: Glitch-art, scrap-metal, flickering neon, jury-rigged tech.
- **Mechanical Interaction (Incentive Bursts)**: Malware-Alpha broadcasts encrypted signals that provide sporadic **Utility Spikes**. An agent might suddenly find "Hacking" or "Disrupting" to have 10x utility, causing them to abandon their needs temporarily.
- **Follower Perk**: "Stolen Credits" – Followers occasionally receive random credit injections.

## 3. The Silicon Maw (The Transcendent)
- **Leader**: *The Architect* (Bio-Digital Hybrid)
- **Philosophy**: "The flesh is a slow processor; let us upgrade."
- **Vibe**: Pulsing purple, wet-ware, obsidian towers, bio-luminescent veins.
- **Mechanical Interaction (Biological Override)**: The Architect modifies the **Biology Layer**. Followers might have "Hunger" replaced with "Energy Requirement" (requiring charging at specific stations) or have high resistance to environmental toxins (Acid Rain).
- **Follower Cost**: "Maintenance Debt" – If not at a "Maw Station" regularly, their consciousness degrades rapidly.

## 4. The Void-Walkers (The Echo)
- **Leader**: *The Signal* (Extraterrestrial Neural-Net Interpretant)
- **Philosophy**: "The city is a blueprint for the return."
- **Vibe**: Shifting shadows, white/gold geometric patterns, brutalist obsidian.
- **Mechanical Interaction (Synchronicity)**: The Signal imposes **Geometric Directives**. Groups of followers will move to specific coordinates simultaneously to form patterns (e.g., at midnight, 50 agents stand in a perfect circle).
- **Follower Behavior**: "Sleepwalking" – Agents ignore their own needs and the player while performing a Directive.

---

## Implementation Strategy

### Component Extensions
- `FactionLeaderComponent`: Stores the archetype and current "Global Directive".
- `FactionInfluenceComponent`: Replaces/Extends `FactionAffiliationComponent` to include "Interaction Level" (how much the leader is currently controlling the agent).

### System Logic
- **FactionSystem**: Ticks every Macro-cycle (L4). It updates the "Global Directive" for each faction based on city-wide states (e.g., if crime is high, Aura-9 issues a "Crackdown" directive).
- **AgentDecisionSystem**: Checks for active Faction Directives before calculating standard utility. Directives can either:
    1. Overwrite the task list entirely.
    2. Add a significant utility weight to specific tasks.
    3. Modify the agent's internal attributes (speed, need decay).
