# Agent-Based Modeling (ABM) Research
## Behavioral Simulation for SocioSim and EcoSim Layers

---

## 1. ABM Fundamentals for Oubliette

Agent-Based Modeling involves autonomous agents interacting within an environment, producing emergent collective behavior—exactly what Oubliette's SocioSim (factions, rumors, crime) and EcoSim (markets, employment) require.

### Key Requirements:
- **Heterogeneity**: Agents have distinct attributes (skills, wealth, psychology)
- **Autonomy**: Agents make decisions based on local information
- **Local Interactions**: No global controller (emergent order from bottom-up)
- **Adaptation**: Agents learn/change strategies over time

---

## 2. Major ABM Frameworks

### 2.1 NetLogo
**Website**: https://ccl.northwestern.edu/netlogo/  
**GitHub**: https://github.com/NetLogo/NetLogo

**Characteristics**:
- Domain-specific language (Logo dialect)
- Visual agent representation
- Extensive model library (Social Science, Biology, Economics)
- 2D grid-based environments primarily

**Strengths**:
- Rapid prototyping of behavioral rules
- Excellent for validating simulation logic before implementation
- Built-in plotting and data export
- Large academic community (urban modeling, epidemiology)

**Trade-offs**:
- Performance ceiling (~10,000 agents)
- Hard to integrate with game engines
- 2D focus limits 3D city representation

**Relevant Models**:
- **Virus model**: https://ccl.northwestern.edu/netlogo/models/Virus  
  *Disease spread on networks, quarantine behavior*
  
- **Wealth Distribution**: https://ccl.northwestern.edu/netlogo/models/WealthDistribution  
  *Gini coefficient emergence from exchange rules*
  
- **Artificial Anasazi**: http://www.openabm.org/model/2220  
  *Historical settlement patterns, resource depletion*

**Usage for Oubliette**:
Prototype rumor propagation and economic exchange algorithms in NetLogo, then port optimized C++ implementation.

---

### 2.2 MASON (Multi-Agent Simulator Of Neighborhoods)
**Website**: https://cs.gmu.edu/~eclab/projects/mason/  
**GitHub**: https://github.com/eclab/mason

**Characteristics**:
- Java-based (portable, garbage collected)
- 2D and 3D continuous/discrete environments
- Reproducibility focus (identical seeds = identical results)
- Visualization separated from model (headless possible)

**Strengths**:
- Academic rigor (citable publications)
- Checkpoint/save system for long simulations
- Hexagonal grid support (better for city planning than squares)
- Environment portfolios (import GIS data)

**Trade-offs**:
- Java memory overhead
- GUI is utilitarian (not game-ready)
- Slower than native C++ for >100k agents

**Relevant Features**:
- **Social Networks**: Built-in graph structures for relationship modeling
- **Sparse Grids**: Efficient for large cities with empty spaces
- **GeoMason**: GIS extension for real-world city imports (OpenStreetMap)

---

### 2.3 Repast Symphony
**Website**: https://repast.github.io/  
**GitHub**: https://github.com/Repast/repast4py

**Characteristics**:
- Java (Repast Simphony) and Python (Repast4Py) versions
- Point-and-click model construction
- Built-in regression testing
- Automatic parameter sweeping

**Strengths**:
- Python version (Repast4Py) allows numpy/scipy integration
- Scheduled actions (agents act at specific times)
- Context-Projection architecture separates agent logic from space

**Trade-offs**:
- Python GIL limits true parallelism (though Repast4Py has MPI support)
- Heavier weight than custom solutions

**Usage for Oubliette**:
Repast4Py could serve as reference implementation for Python-based tooling/data science integration.

---

## 3. Economic Simulation Specifics

### 3.1 Econophysics Approaches

**Wealth Distribution Models**:
- **Yard-Sale Model**: Random exchange with conservation
- **Bouchaud-Mezard**: Network effects in wealth concentration
- **Angle Distribution**: Empirical income distributions

**Implementation for Oubliette**:
Use Bouchaud-Mezard model for black market networks, Yard-Sale for legitimate commerce.

### 3.2 Market Mechanisms

**Double Auction** (Continuous trading):
- Matching buy/sell orders continuously
- Used by NASDAQ, appropriate for Oubliette markets

**Call Auction** (Periodic clearing):
- Batch process orders at intervals (every game hour)
- Better performance for less liquid markets (specialized goods)

**Implementation Reference**:
- **Gode and Sunder (1993)**: "Allocative Efficiency of Markets with Zero-Intelligence Traders"  
  *Shows markets emerge even from simple agents*

---

## 4. Social Network Analysis Integration

### 4.1 Graph Libraries

**NetworkX** (Python):
- https://networkx.org/
- Comprehensive graph algorithms (centrality, community detection)
- Use for offline analysis of Oubliette's social structure

**Boost.Graph** (C++):
- https://www.boost.org/doc/libs/release/libs/graph/
- Adjacency lists, BGL (Boost Graph Library)
- Runtime graph analysis (who influences whom)

**LEMON** (C++):
- http://lemon.cs.elte.hu/trac/lemon
- Efficient graph algorithms
- Better performance than Boost for large graphs

### 4.2 Rumor Propagation Models

**Independent Cascade Model**:
- Each node has one chance to activate neighbors
- Good for sudden news/events

**Linear Threshold Model**:
- Node activates when weighted sum of neighbors exceeds threshold
- Good for gradual opinion shifts

**SIR Model Adaptation**:
- Susceptible-Infected-Recovered (from epidemiology)
- "Infected" = believes rumor, "Recovered" = skeptical/rejects

**Implementation**:
Use Linear Threshold for ideology spread, Independent Cascade for specific event rumors (death, scandal).

---

## 5. Behavioral Economics Integration

### 5.1 Prospect Theory (Kahneman & Tversky)
Agents value gains/losses asymmetrically:
- Loss aversion (losses hurt 2x more than equivalent gains feel good)
- Reference dependence (evaluation relative to current state, not absolute)

**Oubliette Application**:
Poor agents take desperate risks (high probability of small gain preferred over low probability of large gain), while wealthy agents avoid risk.

### 5.2 Bounded Rationality
Agents don't optimize globally:
- **Satisficing**: Choose first option that meets threshold
- **Heuristics**: Availability bias (recent events seem more likely)

**Implementation**:
Limit agent information to local neighborhood (agents don't know cheapest food city-wide, only in their district).

---

## 6. Implementation Strategy for Oubliette

### Phase 1: Behavioral Prototypes (Python/Mesa)
- Validate economic exchange rules
- Tune rumor propagation speeds
- Test Schelling-style segregation

### Phase 2: Core Engine (C++/EnTT)
- Port validated algorithms to high-performance ECS
- Implement spatial partitioning (districts as cells)
- Graph structure for social networks (Boost.Graph)

### Phase 3: Scale Testing (FLAME GPU)
- If needed, port to GPU for 100k+ agents
- Otherwise, stick to CPU with LOD abstraction

---

## 7. References

1. **Epstein, J. M., & Axtell, R.** (1996). "Growing Artificial Societies: Social Science from the Bottom Up". *MIT Press*.
   - Sugarscape model, foundational ABM text

2. **Schelling, T. C.** (1971). "Dynamic Models of Segregation". *Journal of Mathematical Sociology*.
   - Micromotives and macrobehavior, segregation dynamics

3. **Macy, M. W., & Willer, R.** (2002). "From Factors to Actors: Computational Sociology and Agent-Based Modeling". *Annual Review of Sociology*.
   - Sociological foundations of ABM

4. **Tesfatsion, L., & Judd, K. L.** (Eds.). (2006). "Handbook of Computational Economics, Vol. 2: Agent-Based Computational Economics". *Elsevier*.
   - Economic simulation methods
