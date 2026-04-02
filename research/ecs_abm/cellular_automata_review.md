# Cellular Automata (CA) Research
## Spatial Simulation for Disease, Infrastructure, and Urban Dynamics

---

## 1. CA Fundamentals for Oubliette

Cellular Automata consist of:
- **Grid**: Discrete cells (2D/3D) representing space
- **States**: Finite set per cell (e.g., Empty, Infected, Powered)
- **Neighborhood**: Adjacent cells affecting updates (Moore vs Von Neumann)
- **Rules**: Function determining next state based on neighborhood

**Why CA for Oubliette**:
- **Parallelizable**: Update all cells simultaneously (GPU-friendly)
- **Emergent Complexity**: Simple local rules create global patterns (traffic jams, disease waves)
- **Deterministic**: Perfect for inspect rewind mechanics
- **Compositional**: Can layer multiple CA (power grid + disease + fire)

---

## 2. Classic CA Models with Oubliette Applications

### 2.1 Conway's Game of Life (1970)
**Rules** (2D Moore neighborhood):
- Birth: Dead cell with exactly 3 live neighbors → Live
- Survival: Live cell with 2-3 live neighbors → Live
- Death: All other cases → Dead

**Oubliette Application**:
Abstract model for **Urban Blight**:
- "Live" = Occupied/Healthy building
- "Death" = Abandonment
- Oscillators/Glitters = Gentrification cycles

### 2.2 Forest Fire Model (Drossel & Schwabl, 1992)
**States**: Empty → Tree → Burning → Empty  
**Parameters**: p = tree growth, f = lightning/fire start

**Oubliette Application**:
**Infrastructure Failure Cascades**:
- Tree = Operational node
- Burning = Failed node
- Fire spreads to neighbors (cascading blackout)

### 2.3 SIR/SEIR Epidemiological Models
**States**: Susceptible → Exposed → Infected → Recovered/Dead

**CA Extension**:
- Moore neighborhood infection
- Probability-based transmission
- Time-delayed state transitions

**Oubliette Application**:
**BioSim Disease Layer**:
- Cells represent individuals or spatial zones
- Agents move between cells
- Infection probability based on cell density + agent immunity

---

## 3. Implementation Technologies

### 3.1 GPGPU CA (CUDA/OpenCL)
**Libraries**:
- **CUDA**: Nvidia-specific, best performance
- **OpenCL**: Cross-platform, portable
- **ArrayFire**: High-level GPU array operations

**Example** (Python/PyTorch for prototyping):
