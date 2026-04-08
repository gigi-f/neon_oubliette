# T.3 — Negotiation Mechanics Specification

## Core Simulation Principles
Negotiation in Project Neon Oubliette is not a mini-game; it is an interaction between two complex simulation states. The success of a trade depends on the target agent's biological needs, their political standing, their personality, and the environmental context.

## 1. Information Utility Logic
Rumors and Intel collected via Phase N act as a secondary currency.

### Value Calculation
```cpp
float calculateInformationUtility(InformationRecord record, entt::entity target_agent) {
    float base_value = 0.0f;
    switch(record.type) {
        case InformationType::RUMOR: base_value = 50.0f; break;
        case InformationType::PROPAGANDA: base_value = 100.0f; break;
        case InformationType::INTELLIGENCE: base_value = 250.0f; break;
        case InformationType::PRICE_TIP: base_value = 150.0f; break;
        case InformationType::GOSSIP: base_value = 25.0f; break;
    }

    float value = base_value * record.veracity;
    value *= std::pow(0.9f, (float)record.hops);

    // Faction Relevance
    if (m_registry.all_of<Layer4PoliticalComponent>(target_agent)) {
        auto target_faction = m_registry.get<Layer4PoliticalComponent>(target_agent).primary_faction;
        
        // Internal Intel: Source matches Target's faction
        if (record.source_faction_id == target_faction) {
            value *= 1.5f;
        }

        // Rival Intel: Source is a rival of Target's faction
        auto* tension = m_registry.ctx().find<GlobalFactionTensionComponent>();
        if (tension) {
            float relation = tension->relations[{record.source_faction_id, target_faction}];
            if (relation < -50.0f) {
                value *= 2.0f; // Very valuable actionable intel
            }
        }
    }
    return value;
}
```

## 2. Dynamic NPC Needs-Based Counter-Offers
When a trade is close to acceptance (deal_ratio between 0.7 and 0.99), the NPC identifies a specific item in the player's inventory that would bridge the value gap and satisfies an urgent need.

### Trigger Condition
- `deal_ratio >= 0.7 && deal_ratio < 1.0`
- `npc_patience > 0.2`

### Logic
1. Calculate `value_needed = (requested_utility * npc_greed_margin) - offered_utility`.
2. Scan `Player::InventoryComponent` for an item `X` whose `utility_to_npc(X)` is closest to `value_needed`.
3. Set `npc_feedback` and notify player of the counter-offer.

### Contextual Feedback (Metaphorical Strings)
- **Hungry (hunger < 50)**: "I'll do it if you throw in that [ItemName]. My stomach is growling."
- **Thirsty (thirst < 50)**: "I'm parched. Add that [ItemName] and you have a deal."
- **Injured (health < 50)**: "I need those meds. Include the [ItemName] and it's yours."
- **Greedy (Personality::AGGRESSIVE)**: "I want more. Give me the [ItemName] too."
- **Default**: "Almost there. Give me the [ItemName] and we can call it even."

## 3. Pressure Mechanics
The `BarterState::PRESSURE` command allows the player to lower the `npc_greed_margin` without adding items.

### Success Probability
- **Base Chance**: 40%
- **Reputation Tier Bonus**:
  - `EXCOMMUNICATED`: 0% (Immediate Hostility/Termination)
  - `HOSTILE`: -30%
  - `SUSPICIOUS`: -15%
  - `NEUTRAL`: +0%
  - `FAVORED`: +10%
  - `FRIENDLY`: +25%
  - `ALLY`: +40%
- **Personality Multipliers**:
  - `SUBSERVIENT`: Success Chance x 1.5
  - `AGGRESSIVE`: Success Chance x 0.5
  - `PARANOID`: Success Chance x 0.8

### Outcomes
- **Success**:
  - `current_leverage += 0.15`
  - `npc_patience -= 0.1`
  - `npc_feedback`: "You drive a hard bargain. Fine."
- **Failure**:
  - `npc_patience -= 0.3`
  - `npc_feedback`: "You're pushing your luck. Offer something real or get out."

## 4. Visual Feedback Layers
The UI MUST reflect the mounting tension.

### Patience Metaphor
- `> 0.7`: '☺' (Green #00FF00) - Calm and open.
- `> 0.3`: '⚄' (Yellow #FFFF00) - Impatient, ready to leave.
- `<= 0.3`: '⚠' (Red #FF0000) - Hostile, about to terminate trade.

### Action Ticks
Every trade `REQUEST` or `PRESSURE` tick should cause a small patience decay (0.05) to simulate the time taken in the city.
