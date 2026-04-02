# Economy & Shop System Design

## 1. Overview

The **Economy** module is the heart of the macro‑micro interaction in *Neon Oubliette*.  It models the flow of money, goods, and services across the city, and ties into the micro‑level shop infrastructure to provide realistic trading, supply‑demand, and price dynamics.

### Core Pillars

| Pillar | Purpose |
|--------|---------|
| **Determinism** | Prices and balances are reproducible from a seed. |
| **Scalable** | Supports millions of citizens, thousands of shops, and many micro‑worlds. |
| **Extensible** | Modders can inject custom currencies, taxation rules, or trade networks. |
| **Integration** | Works seamlessly with the `CommandBuffer` pipeline and the event bus. |

## 2. Economic Data Model

| Entity | Component | Meaning |
|--------|-----------|---------|
| **Citizen** | `BankAccount` | Holds current balance and transaction history. |
| **Shop** | `ShopComponent` | Inventory, pricing, owner, and economic state. |
| **Building** | `BuildingComponent` | Accumulates tax revenue, electricity cost, etc. |
| **MacroRegistry** | `WorldStats` | Aggregated totals: total wealth, tax revenue, unemployment. |
| **Resource** | `ResourceStockpile` | Quantities of goods like food, steel, etc. |

### Component Definitions

```cpp
struct BankAccount {
    uint64_t balance;         // In smallest currency unit (cents)
    std::deque<Transaction> history; // Limited-size history for audit
};

struct Transaction {
    enum class Type { Deposit, Withdrawal, Transfer, Sale, Purchase } type;
    uint64_t amount;
    uint64_t timestamp; // Game ticks
    uint64_t from; // Entity ID (0 for system)
    uint64_t to;   // Entity ID
};

struct ShopComponent {
    uint64_t owner_entity;
    std::unordered_map<std::string, ItemStock> inventory;
    PriceModel pricing;
    uint64_t gold_in_register; // cash on hand
    uint64_t tax_rate;         // percent
};

struct ItemStock {
    uint32_t quantity;
    uint64_t base_cost; // base cost before tax
};

struct PriceModel {
    enum class Strategy { Fixed, DemandDriven, TimeBased } strategy;
    std::unordered_map<std::string, uint64_t> fixed_prices; // for Fixed
    // For DemandDriven: price = base * (1 + demand_factor)
    double demand_factor;
};

struct ResourceStockpile {
    std::unordered_map<std::string, uint64_t> resources;
};
```

## 3. Macro‑Level Systems

| System | Phase | Description |
|--------|-------|-------------|
| `TaxationSystem` | `Macro` | Computes taxes from all buildings and citizens, transfers to `WorldStats`. |
| `PriceUpdateSystem` | `Macro` | Iterates over all shops, updating `ShopComponent::pricing` based on demand and supply. |
| `ResourceAllocationSystem` | `Macro` | Moves resources between `ResourceStockpile`s, applies consumption from buildings. |
| `BankingSystem` | `Macro` | Processes queued `Transaction`s, updates balances. |
| `EconomicEventSystem` | `Macro` | Generates macro events (e.g., recession, boom) that influence tax rates and demand. |

### Example: `TaxationSystem`

```cpp
void TaxationSystem::update(float dt) {
    for (auto [e, building] : registry.view<BuildingComponent>()) {
        uint64_t tax = building.power_consumption * building.tax_rate / 100;
        registry.emplace_or_replace<TaxRecord>(e, tax);
        registry.queue_command([=](entt::registry& r){
            r.modify<WorldStats>(world_stats_entity).tax_revenue += tax;
        });
    }
}
```

## 4. Micro‑Level Systems – Shops

| System | Phase | Responsibility |
|--------|-------|----------------|
| `ShopTransactionSystem` | `Micro` | Handles buy/sell actions initiated by citizens. |
| `ShopInventorySystem` | `Micro` | Restocks from resource stockpiles, removes sold items. |
| `ShopPricingSystem` | `Micro` | Adjusts shop prices based on local demand. |
| `ShopTaxSystem` | `Micro` | Applies local tax to shop sales. |

### `ShopTransactionSystem`

- Citizens request a purchase via an event `ShopPurchaseEvent{shop_id, item_id, quantity}`.
- The system resolves the event: checks inventory, balances, and posts a `Transaction` to the `BankingSystem`.
- If successful, the item is removed from the shop inventory and the citizen’s `BankAccount` is debited.

## 5. Resource Flow Mechanics

- **Production**: Certain buildings (e.g., factories) produce resources each tick, depositing into their `ResourceStockpile`.
- **Consumption**: Buildings consume resources from their stockpile or from the global `WorldStats` if out of stock.
- **Trade**: Shops act as exchange points: citizens can trade goods for money.  The `ShopTransactionSystem` also supports trade of goods for other goods (e.g., barter) via `TradeEvent`.

### Flow Diagram

```
[Citizen] ──┐
            ▼
        ShopComponent (inventory)
            ▲
            │
        ResourceStockpile (goods)
            ▲
            │
        BuildingComponent (production)
            ▲
            │
        WorldStats (global resource pool)
```

## 6. Event Bus Integration

| Event | Producer | Consumer |
|-------|----------|----------|
| `ShopPurchaseEvent` | UI / NPC system | `ShopTransactionSystem` |
| `TaxCollectedEvent` | `TaxationSystem` | `BankingSystem` |
| `ResourceDepletedEvent` | `ResourceAllocationSystem` | `ShopInventorySystem` |
| `MarketShockEvent` | `EconomicEventSystem` | all economic systems |

The event bus ensures that systems can remain loosely coupled.  For example, when a `MarketShockEvent` occurs, the `PriceUpdateSystem` will re‑evaluate all shop prices.

## 7. Performance & Determinism

- All monetary calculations use fixed‑point integers to avoid floating‑point drift.
- Transaction queues are processed in the `CommandBufferApply` phase, guaranteeing order.
- Random elements (e.g., demand fluctuations) use deterministic RNG derived from entity seeds.

## 8. Extensibility Hooks

- **Currency Plugins**: Implement `ICurrencyPlugin` to introduce new currencies with conversion rates.
- **Tax Plugins**: Hook into `TaxationSystem` to add progressive tax brackets or special levies.
- **Shop Types**: Mods can register new `ShopType` components (e.g., `BlackMarketComponent`) and provide custom pricing logic.

---

**Next** – Draft the `economy_and_shops.md` file (this is the file itself).  Once reviewed, we can add the component definitions to `entt_component_types.md` and register them in `component_registry.h`.
