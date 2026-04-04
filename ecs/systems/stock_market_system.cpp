#include "stock_market_system.h"
#include <cmath>
#include <algorithm>
#include <random>

namespace NeonOubliette {

StockMarketSystem::StockMarketSystem(entt::registry& registry, entt::dispatcher& dispatcher)
    : m_registry(registry), m_dispatcher(dispatcher)
{
    m_dispatcher.sink<StockPurchaseEvent>().connect<&StockMarketSystem::handleStockPurchase>(*this);
    m_dispatcher.sink<StockSaleEvent>().connect<&StockMarketSystem::handleStockSale>(*this);
}

void StockMarketSystem::initialize() {
    // 1. Assign stocks to the 4 main factions if they don't have them
    auto faction_view = m_registry.view<FactionComponent, NameComponent>();
    for (auto entity : faction_view) {
        auto& faction = faction_view.get<FactionComponent>(entity);
        auto& name = faction_view.get<NameComponent>(entity);
        
        if (!m_registry.all_of<StockComponent>(entity)) {
            StockComponent stock;
            stock.company_name = name.name;
            stock.ticker = faction.faction_id.substr(0, 4); // Simple ticker gen
            
            // Faction specific settings
            if (faction.faction_id == "GOVERNMENT") {
                stock.ticker = "AURA";
                stock.current_price = 150.0;
                stock.volatility = 0.02f; // Stable
            } else if (faction.faction_id == "REBEL") {
                stock.ticker = "ENTP";
                stock.current_price = 45.0;
                stock.volatility = 0.15f; // Highly volatile
            } else if (faction.faction_id == "MAW") {
                stock.ticker = "MAW";
                stock.current_price = 85.0;
                stock.volatility = 0.08f;
            } else if (faction.faction_id == "VOID") {
                stock.ticker = "SIGN";
                stock.current_price = 120.0;
                stock.volatility = 0.05f;
            }
            
            stock.previous_price = stock.current_price;
            stock.opening_price = stock.current_price;
            stock.total_shares = 10000000; // 10 Million
            stock.outstanding_shares = 5000000;
            
            m_registry.emplace<StockComponent>(entity, stock);
        }
    }
}

void StockMarketSystem::update(double delta_time) {
    // L3 update (every 10 turns)
    updateStockPrices();
    processAgentTrades();
}

void StockMarketSystem::updateStockPrices() {
    auto stock_view = m_registry.view<StockComponent, FactionComponent>();
    
    std::random_device rd;
    std::mt19937 gen(rd());
    
    for (auto entity : stock_view) {
        auto& stock = stock_view.get<StockComponent>(entity);
        auto& faction = stock_view.get<FactionComponent>(entity);
        
        stock.previous_price = stock.current_price;
        
        // Price formula: (Influence boost) + (Random Walk) + (Momentum)
        // Influence [0.0 to 100.0 or so] -> scale to [0.9 to 1.1]
        // Note: faction.influence is currently a float in FactionComponent
        float influence_factor = 1.0f + (faction.influence - 1.0f) / 10.0f; // Simplified: 1.0 influence is baseline
        
        std::normal_distribution<double> dist(0, stock.volatility * stock.current_price);
        double random_walk = dist(gen);
        
        stock.current_price *= influence_factor;
        stock.current_price += random_walk;
        
        // Add momentum decay
        stock.current_price += stock.momentum;
        stock.momentum *= 0.8f; // Dampen momentum
        
        // Minimum price
        if (stock.current_price < 1.0) stock.current_price = 1.0;
        
        // Update momentum based on change
        stock.momentum += (float)(stock.current_price - stock.previous_price) * 0.1f;
        
        // Clamp momentum
        stock.momentum = std::clamp(stock.momentum, -10.0f, 10.0f);
    }
}

void StockMarketSystem::processAgentTrades() {
    auto agent_view = m_registry.view<Layer3EconomicComponent, Layer2CognitiveComponent>();
    auto stock_view = m_registry.view<StockComponent>();
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> chance_dist(0.0f, 1.0f);

    for (auto agent_ent : agent_view) {
        auto& econ = agent_view.get<Layer3EconomicComponent>(agent_ent);
        // auto& cog = agent_view.get<Layer2CognitiveComponent>(agent_ent); // Could use for risk tolerance

        // Agents only trade if they have some cash or assets
        if (econ.cash_on_hand < 100 && econ.portfolio.empty()) continue;

        // Trade frequency: 5% chance per economic tick
        if (chance_dist(gen) > 0.05f) continue;

        for (auto stock_ent : stock_view) {
            auto& stock = stock_view.get<StockComponent>(stock_ent);
            
            // BUY Logic
            if (econ.cash_on_hand > stock.current_price * 2) {
                // Buy if price is lower than previous (dip buying) or just random FOMO
                bool buy_signal = (stock.current_price < stock.previous_price) || (chance_dist(gen) < 0.1f);
                
                if (buy_signal) {
                    uint64_t shares_to_buy = (uint64_t)(econ.cash_on_hand * 0.1 / stock.current_price);
                    if (shares_to_buy > 0) {
                        m_dispatcher.enqueue<StockPurchaseEvent>({agent_ent, stock.ticker, shares_to_buy, stock.current_price});
                    }
                }
            }

            // SELL Logic
            if (econ.portfolio.contains(stock.ticker)) {
                uint64_t shares_held = econ.portfolio.at(stock.ticker);
                if (shares_held > 0) {
                    // Sell if price is higher than opening (profit taking) or if they are low on cash
                    bool sell_signal = (stock.current_price > stock.opening_price * 1.1) || (econ.cash_on_hand < 50) || (chance_dist(gen) < 0.05f);
                    
                    if (sell_signal) {
                        uint64_t shares_to_sell = (uint64_t)(shares_held * 0.5); // Sell half
                        if (shares_to_sell == 0) shares_to_sell = shares_held;
                        
                        m_dispatcher.enqueue<StockSaleEvent>({agent_ent, stock.ticker, shares_to_sell, stock.current_price});
                    }
                }
            }
        }
    }
}

void StockMarketSystem::handleStockPurchase(const StockPurchaseEvent& event) {
    if (!m_registry.all_of<Layer3EconomicComponent>(event.agent)) return;
    
    auto& econ = m_registry.get<Layer3EconomicComponent>(event.agent);
    double total_cost = (double)event.shares * event.price_per_share;
    
    if (econ.cash_on_hand >= (int)total_cost) {
        econ.cash_on_hand -= (int)total_cost;
        econ.portfolio[event.ticker] += event.shares;
        
        // Log trade
        m_dispatcher.enqueue<LogEvent>({
            "AGENT purchased " + std::to_string(event.shares) + " " + event.ticker + " @ " + std::to_string(event.price_per_share),
            LogSeverity::DEBUG,
            "StockMarketSystem"
        });
    }
}

void StockMarketSystem::handleStockSale(const StockSaleEvent& event) {
    if (!m_registry.all_of<Layer3EconomicComponent>(event.agent)) return;
    
    auto& econ = m_registry.get<Layer3EconomicComponent>(event.agent);
    
    if (econ.portfolio.contains(event.ticker) && econ.portfolio[event.ticker] >= event.shares) {
        econ.portfolio[event.ticker] -= event.shares;
        if (econ.portfolio[event.ticker] == 0) {
            econ.portfolio.erase(event.ticker);
        }
        
        double proceeds = (double)event.shares * event.price_per_share;
        econ.cash_on_hand += (int)proceeds;

        // Log trade
        m_dispatcher.enqueue<LogEvent>({
            "AGENT sold " + std::to_string(event.shares) + " " + event.ticker + " @ " + std::to_string(event.price_per_share),
            LogSeverity::DEBUG,
            "StockMarketSystem"
        });
    }
}

void StockMarketSystem::processMacroTrades(entt::registry& registry, std::vector<MacroAgentRecord>& records) {
    auto stock_view = registry.view<StockComponent>();
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> chance_dist(0.0f, 1.0f);

    for (auto& record : records) {
        // Trade frequency: 1% chance per tick for macro agents (slower than L3 for performance)
        if (chance_dist(gen) > 0.01f) continue;

        for (auto stock_ent : stock_view) {
            auto& stock = stock_view.get<StockComponent>(stock_ent);
            
            // BUY Logic
            if (record.cash_on_hand > stock.current_price * 2) {
                bool buy_signal = (stock.current_price < stock.previous_price) || (chance_dist(gen) < 0.1f);
                if (buy_signal) {
                    uint64_t shares_to_buy = (uint64_t)(record.cash_on_hand * 0.1 / stock.current_price);
                    if (shares_to_buy > 0) {
                        record.cash_on_hand -= (int)(shares_to_buy * stock.current_price);
                        record.portfolio[stock.ticker] += shares_to_buy;
                    }
                }
            }

            // SELL Logic
            if (record.portfolio.contains(stock.ticker)) {
                uint64_t shares_held = record.portfolio.at(stock.ticker);
                if (shares_held > 0) {
                    bool sell_signal = (stock.current_price > stock.opening_price * 1.1) || (record.cash_on_hand < 50) || (chance_dist(gen) < 0.05f);
                    if (sell_signal) {
                        uint64_t shares_to_sell = (uint64_t)(shares_held * 0.5);
                        if (shares_to_sell == 0) shares_to_sell = shares_held;
                        
                        record.portfolio[stock.ticker] -= shares_to_sell;
                        if (record.portfolio[stock.ticker] == 0) record.portfolio.erase(stock.ticker);
                        
                        record.cash_on_hand += (int)(shares_to_sell * stock.current_price);
                    }
                }
            }
        }
    }
}

} // namespace NeonOubliette
