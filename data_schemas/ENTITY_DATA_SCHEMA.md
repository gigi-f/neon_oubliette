# Entity Data Schema Specification

## Core Philosophy
All simulation data is componentized and layer-isolated. No god objects.

## Base Entity Structure

class NeonEntity:
    entity_id: UUID
    position: (x, y, z)
    layer_components: Dict[LayerID, LayerComponent]
    privacy_masks: Dict[LayerID, ConcealmentLevel]
    
    def get_layer(self, layer_id: LayerID) -> Optional[LayerComponent]:
        return self.layer_components.get(layer_id)

## Layer 0: PhysicsComponent
- material_type: MaterialEnum (CONCRETE, FLESH, STEEL)
- structural_integrity: float 0.0-1.0
- temperature_celsius: float
- is_liquid/gas/plasma: bool

## Layer 1: BiologyComponent
- species: SpeciesEnum (HUMAN, RAT, SYNTHETIC)
- vital_signs: VitalSigns
- consciousness_level: float 0.0-1.0
- pain_level: int 0-10
- organs: Dict[OrganType, OrganState]
- blood_chemistry: Dict[ChemicalCompound, Concentration]
- pathogen_loads: Dict[PathogenID, InfectionState]
- implants: List[Implant]

## Layer 2: CognitiveComponent
- agent_type: AgentType (NPC, PLAYER, ANIMAL)
- beliefs: KnowledgeBase
- desires: List[Goal]
- intentions: List[Plan]
- pleasure/arousal/dominance: float -1.0 to 1.0
- relationship_web: Dict[EntityID, Relationship]
- reputation_scores: Dict[FactionID, float]

## Layer 3: EconomicComponent
- cash_on_hand: int
- digital_credits: Dict[BankID, Account]
- inventory: List[ItemInstance]
- debts: List[DebtObligation]
- credit_score: int 300-850

class ItemInstance:
    item_type: ItemType
    instance_id: UUID  # Every item unique!
    provenance: ProvenanceChain
    condition: float 0.0-1.0

## Layer 4: PoliticalComponent
- primary_faction: FactionID
- faction_loyalty: float 0.0-1.0
- rank: RankEnum
- soft_power/hard_power: float
- criminal_record: List[Conviction]

## Privacy Masks

class PrivacyMask:
    layer_id: LayerID
    concealment_level: ConcealmentLevel (NONE, CASUAL, ACTIVE, MILITARY)
    falsified_data: Dict[str, Any]
    encryption_strength: int
    
    def attempt_reveal(self, tools: ToolSet, skill: int) -> RevealResult:
        pass

## Inspect Report

class InspectReport:
    target_id: UUID
    inspection_time: Timestamp
    tools_used: List[ToolType]
    layer_data: Dict[LayerID, LayerView]
    confidence_levels: Dict[LayerID, float]
    derived_insights: List[Insight]
