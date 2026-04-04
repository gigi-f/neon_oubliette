#ifndef NEON_OUBLIETTE_ECS_SYSTEMS_STOCK_MARKET_SYSTEM_H
#define NEON_OUBLIETTE_ECS_SYSTEMS_STOCK_MARKET_SYSTEM_H

#include <entt/entt.hpp>
#include "../simulation_coordinator.h"
#include "../components/components.h"
#include "../components/simulation_layers.h"
#include "../components/lod_components.h"
#include "../event_declarations.h"

namespace NeonOubliette {

/**
 * @brief [NEW SYSTEM] StockMarketSystem handles the L3 economic simulation of financial assets.
 *        Factions have stocks that agents can buy and sell.
 */
class StockMarketSystem : public ISimulationSystem {
public:
    StockMarketSystem(entt::registry& registry, entt::dispatcher& dispatcher);

    void initialize() override;
    void update(double delta_time) override;
    SimulationLayer simulation_layer() const override { return SimulationLayer::L3_Economic; }

    void handleStockPurchase(const StockPurchaseEvent& event);
    void handleStockSale(const StockSaleEvent& event);

    /**
     * @brief Process trades for agents in cold storage.
     */
    static void processMacroTrades(entt::registry& registry, std::vector<MacroAgentRecord>& records);

private:
    void updateStockPrices();
    void processAgentTrades();

    entt::registry& m_registry;
    entt::dispatcher& m_dispatcher;
};

} // namespace NeonOubliette

#endif // NEON_OUBLIETTE_ECS_SYSTEMS_STOCK_MARKET_SYSTEM_H
