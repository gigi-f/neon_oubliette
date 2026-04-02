# Political and Social Simulation Architecture

## 1. Political Factions and Power Dynamics

### 1.1 Faction Component
*   **Definition**: Entities representing organized groups (e.g., corporations, crime syndicates, political parties, citizen collectives).
*   **Key Fields**:
    *   `Name`, `Ideology`, `Resources` (financial, influence, military assets)
    *   `ReputationComponent` (public and internal perception)
    *   `RelationshipComponent` (affinity/rivalry with other factions)
    *   `PowerBaseComponent` (geographic control, economic dominance)

### 1.2 Influence and Public Opinion
*   **PublicOpinionComponent (Macro)**: City-wide sentiment towards various issues, factions, and policies. Influenced by `NewsEvents`, `PropagandaEvents`, `CrimeEvents`.
*   **InfluenceComponent (NPC/Faction)**: A metric of an entity's ability to sway decisions or actions. Can be spent and gained through various interactions.
*   **MediaNetworkSystem**: Simulates news outlets, their biases, and their impact on `PublicOpinionComponent`.

## 2. NPC Political Behavior and Beliefs

### 2.1 BeliefSystem
*   **BeliefComponent (NPC)**: An NPC's stance on key political, economic, and social issues. Influenced by `FactionAffiliationComponent`, `LifeExperienceComponent`.
*   **VoterComponent (NPC)**: Determines an NPC's likelihood to vote, preferred candidates, and susceptibility to campaign messaging.

### 2.2 Faction Affiliation
*   **FactionAffiliationComponent (NPC)**: Links an NPC to one or more factions, potentially with varying loyalty levels. Influences NPC actions and resource sharing.
*   **RecruitmentSystem**: Simulates how factions attract new members based on ideology, resources, and social influence.

## 3. Campaigns, Elections, and Legislation

### 3.1 Candidate Component
*   **Definition**: NPCs actively running for public office.
*   **Key Fields**:
    *   `PlatformComponent` (stances on issues)
    *   `FundingComponent` (donations received/spent)
    *   `EndorsementComponent` (support from factions/prominent NPCs)

### 3.2 CampaignSystem
*   Manages `CampaignEvent`s: rallies, debates, advertisements, lobbying efforts.
*   Consumes `FundingComponent` and `InfluenceComponent` to alter `PublicOpinionComponent` and `VoterComponent`s.
*   Generates `PromiseComponent`s (campaign promises) and `LieComponent`s (false claims), which can impact `ReputationComponent` if exposed.

### 3.3 ElectionSystem
*   Simulates the voting process based on `VoterComponent`s and `PublicOpinionComponent`.
*   Determines election outcomes and assigns `ElectedOfficeComponent` to winning candidates.

### 3.4 LegislativeSystem
*   **BillComponent**: Represents proposed laws with `EffectComponent` (how it impacts various city systems).
*   **VotingProcess**: Elected officials (NPCs with `ElectedOfficeComponent`) vote on `BillComponent`s, influenced by `FactionAffiliationComponent`, `InfluenceComponent`, and `BeliefComponent`.

## 4. Corruption, Black Market, and Backroom Deals

### 4.1 Bribery and Favor System
*   **BriberyComponent**: Represents an attempt to exchange `CurrencyComponent` for a specific action (e.g., favorable vote, overlooking a crime).
*   **FavorComponent**: Represents a non-monetary obligation between NPCs or Factions, to be repaid later.
*   **CorruptionSystem**: Processes `BriberyComponent` and `FavorComponent` transactions, potentially generating `CorruptionEvent`s if discovered.

### 4.2 Black Market Component
*   **Definition**: Underground network for illicit goods and services.
*   **Key Fields**:
    *   `IllicitGoodsComponent` (drugs, weapons, stolen items)
    *   `IllegalServicesComponent` (assassination, data breaches)
    *   `RiskComponent` (chance of detection/punishment)
*   **CrimeSystem**: Simulates criminal activities, enforcement, and their impact on `PublicOpinionComponent` and `FactionComponent` reputations.

### 4.3 Negotiation and Deals
*   **NegotiationSystem**: Models NPC and Faction interactions for agreements, compromises, and coercion.
*   **BackroomDealComponent**: Represents secret agreements between powerful entities, often involving `BriberyComponent` or `FavorComponent`, with hidden `EffectComponent`s on the city simulation.
*   **DiscoveryEvent**: Can expose `BackroomDealComponent`s, leading to `ScandalEvent`s, `ReputationComponent` damage, and political instability.
