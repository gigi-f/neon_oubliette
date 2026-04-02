# Inspect Mechanics Specification

## Core Philosophy
Inspection is not just viewing stats - it is ontological interrogation. The player extracts knowledge from the simulation with depth limited by:
1. Physical proximity (cannot inspect what you cannot sense)
2. Tool quality (cybernetic scanners vs naked eye)
3. Player knowledge (understanding what you are seeing)
4. Entity stealth (concealment, encryption, counter-surveillance)

## Inspect Modes

### Mode 0: Glance (Passive)
- Trigger: Adjacent to entity or looking at distant entity
- Resolution: Single character + color
- Information: Visual identification only

### Mode 1: Surface Scan (Active)
- Trigger: i key on adjacent entity
- Resolution: Name + obvious physical state + immediate economic value
- Cost: 1 turn

### Mode 2: Biological Audit (Deep)
- Trigger: I + direction on living target
- Tool Required: Medscanner
- Displays: Full organ status, blood chemistry, pathogens, derived insights

### Mode 3: Cognitive Profile (Behavioral)
- Trigger: Extended observation (3+ turns) + profile command
- Displays: Current emotional state, social connections, motivation prediction

### Mode 4: Financial Forensics (Economic)
- Trigger: Access to target funds or digital wallet
- Displays: Transaction history, debt obligations, income sources

### Mode 5: Structural Analysis (Political/Physical)
- Trigger: Administrative access or architectural scanners
- Displays: Building integrity, ownership chain, surveillance blind spots

## UI Layout

The interface uses a tabbed hierarchical display when inspecting:

+-------------------------------------------------------------+
| INSPECTING: Street Vendor                            [X]    |
+-------------------------------------------------------------+
| [Physical] [Bio*] [Mental] [Inventory] [Network] [History] |
+-------------------------------------------------------------+
|                                                             |
| [ASCII Portrait Area]                                       |
|       .---.                                                 |
|      / o o  |   Rep: Neutral                                |
|      |  >  |   Threat: Low                                  |
|      | === |                                                |
|       \`---\                                                |
|                                                             |
| HEALTH SUMMARY:                                             |
| Overall: 84% (fatigued)                                     |
| Visible: Scar tissue (neck), Neural port (temple)          |
|                                                             |
+-------------------------------------------------------------+
