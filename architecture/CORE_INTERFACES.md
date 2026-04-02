# Core System Interfaces
## Architecture Layer Specifications

---

## 1. Event Bus Interface

The central nervous system connecting all simulation layers.

class IEventBus {
public:
    virtual ~IEventBus() = default;
    
    // Publish event to all subscribers
    template<typename EventType>
    void publish(const EventType& event);
    
    // Subscribe to specific event type
    template<typename EventType, typename Callback>
    void subscribe(Callback&& callback);
    
    // Process queued events (call once per simulation tick)
    virtual void dispatch() = 0;
};

**Requirements:**
- Thread-safe (for future multithreading)
- Deterministic order (same events -> same results)
- Zero-allocation during dispatch

---

## 2. Layer Component Interface

Base interface for all simulation layer components.

class ILayerComponent {
public:
    virtual ~ILayerComponent() = default;
    
    // Unique identifier for this layer
    virtual LayerID getLayerId() const = 0;
    
    // Update called at layer-specific frequency
    virtual void update(WorldState& world, float deltaTime) = 0;
    
    // Reset to initial state (for rewinding)
    virtual void reset() = 0;
};

---

## 3. Inspectable Interface

Implemented by all entities that can be inspected.

class IInspectable {
public:
    virtual ~IInspectable() = default;
    
    // Generate inspection report for this entity
    virtual InspectReport inspect(const InspectorContext& context) const = 0;
    
    // Get list of available inspection layers
    virtual std::vector<LayerID> getAvailableLayers() const = 0;
};

---

## 4. Simulation Clock Interface

Manages phase-locked simulation timing.

class ISimulationClock {
public:
    virtual ~ISimulationClock() = default;
    
    // Current simulation tick
    virtual uint64_t getCurrentTick() const = 0;
    
    // Check if layer should update this tick
    virtual bool shouldLayerUpdate(LayerID layer) const = 0;
    
    // Advance simulation by one tick
    virtual void tick() = 0;
    
    // Register layer with specific frequency
    virtual void registerLayer(LayerID layer, uint32_t frequency) = 0;
};

---

## 5. World State Interface

Central state management for the simulation.

class IWorldState {
public:
    virtual ~IWorldState() = default;
    
    // Entity management
    virtual EntityID createEntity() = 0;
    virtual void destroyEntity(EntityID id) = 0;
    
    // Query entities with specific components
    virtual QueryResult query() const = 0;
    
    // Spatial queries
    virtual std::vector<EntityID> queryRadius(Vector3 position, float radius) const = 0;
    
    // Save/State management for rewinding
    virtual StateSnapshot captureSnapshot() const = 0;
    virtual void restoreSnapshot(const StateSnapshot& snapshot) = 0;
};

---

## 6. Renderer Interface

Abstract interface for terminal rendering.

class IRenderer {
public:
    virtual ~IRenderer() = default;
    
    // Initialization
    virtual bool initialize() = 0;
    virtual void shutdown() = 0;
    
    // Drawing operations
    virtual void drawGlyph(int x, int y, char glyph, Color fg, Color bg) = 0;
    virtual void drawString(int x, int y, const std::string& text, Color fg) = 0;
    
    // Terminal capabilities
    virtual bool supportsTrueColor() const = 0;
    virtual TerminalSize getTerminalSize() const = 0;
};

---

## Technology Versions

- EnTT: v3.12.2 (latest stable)
- notcurses: v3.0.9 (true color, Sixel support)
- C++ Standard: C++20
- Build System: CMake 3.20+

---

## Implementation Strategy

### Phase 1: Foundation (Week 1)
1. Implement EventBus with EnTT signals
2. Implement SimulationClock with phase-locking
3. Implement basic WorldState with EnTT registry

### Phase 2: Rendering (Week 2-3)
1. Implement IRenderer using notcurses
2. Implement IInputHandler
3. Create basic UI framework

### Phase 3: BioSim Layer (Week 4-5)
1. Implement BiologyComponent
2. Create OrganState processing
3. Implement basic drug metabolism
