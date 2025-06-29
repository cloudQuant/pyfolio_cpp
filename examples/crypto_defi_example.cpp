#include <pyfolio/crypto/defi_analytics.h>
#include <iostream>
#include <iomanip>

using namespace pyfolio::crypto;

/**
 * @brief Create sample cryptocurrency portfolio with DeFi positions
 */
CryptoPortfolio create_sample_crypto_portfolio() {
    CryptoPortfolio portfolio;
    
    // Add wallet addresses
    portfolio.add_wallet("0x742d35Cc7619C615C17C2BED35B40C8D5bB2A1F", ChainId::Ethereum);
    portfolio.add_wallet("0x8a8eAFb1cf62BfBeb1741769DAE1a9dd47996192", ChainId::Polygon);
    portfolio.add_wallet("0x5aAeb6053F3E94C9b9A09f33669435E7Ef1BeAed", ChainId::BinanceSmartChain);
    
    // Create token information
    TokenInfo eth("ETH", "Ethereum", "0x0000000000000000000000000000000000000000", ChainId::Ethereum);
    eth.price_usd = 2000.0;
    
    TokenInfo usdc("USDC", "USD Coin", "0xA0b86a33E6e131C93C8C5c1a7B5F5c7a7f8f8f8f", ChainId::Ethereum);
    usdc.price_usd = 1.0;
    
    TokenInfo wbtc("WBTC", "Wrapped Bitcoin", "0x2260FAC5E5542a773Aa44fBCfeDf7C193bc2C599", ChainId::Ethereum);
    wbtc.price_usd = 45000.0;
    
    TokenInfo matic("MATIC", "Polygon", "0x7D1AfA7B718fb893dB30A3aBc0Cfc608AaCfeBB0", ChainId::Polygon);
    matic.price_usd = 0.8;
    
    // Add basic holdings
    CryptoHolding eth_holding(eth, 5.0, "0x742d35Cc7619C615C17C2BED35B40C8D5bB2A1F");
    eth_holding.value_usd = eth_holding.balance * eth.price_usd;
    portfolio.add_holding(eth_holding);
    
    CryptoHolding usdc_holding(usdc, 10000.0, "0x742d35Cc7619C615C17C2BED35B40C8D5bB2A1F");
    usdc_holding.value_usd = usdc_holding.balance * usdc.price_usd;
    portfolio.add_holding(usdc_holding);
    
    // Add liquidity position (ETH/USDC on Uniswap V3)
    LiquidityPosition eth_usdc_lp;
    eth_usdc_lp.pool_address = "0x88e6A0c2dDD26FEEb64F039a2c41296FcB3f5640";
    eth_usdc_lp.protocol = Protocol::Uniswap_V3;
    eth_usdc_lp.chain_id = ChainId::Ethereum;
    eth_usdc_lp.tokens = {eth, usdc};
    eth_usdc_lp.token_balances = {2.5, 5000.0};
    eth_usdc_lp.token_weights = {0.5, 0.5};
    eth_usdc_lp.total_value_usd = 10000.0;
    eth_usdc_lp.lp_token_balance = 1000.0;
    eth_usdc_lp.share_of_pool = 0.001; // 0.1% of pool
    eth_usdc_lp.fees_earned_24h = 15.0;
    eth_usdc_lp.fees_earned_7d = 105.0;
    eth_usdc_lp.fees_earned_30d = 450.0;
    eth_usdc_lp.current_apy = 12.5;
    eth_usdc_lp.entry_value_usd = 9500.0;
    eth_usdc_lp.entry_token_prices = {1900.0, 1.0};
    portfolio.add_liquidity_position(eth_usdc_lp);
    
    // Add lending position (Aave)
    LendingPosition aave_position;
    aave_position.market_address = "0x7Fc66500c84A76Ad7e9c93437bFc5Ac33E2DDaE9";
    aave_position.protocol = Protocol::Aave;
    aave_position.chain_id = ChainId::Ethereum;
    aave_position.collateral_token = wbtc;
    aave_position.debt_token = usdc;
    aave_position.collateral_amount = 0.5;
    aave_position.debt_amount = 15000.0;
    aave_position.collateral_value_usd = 22500.0;
    aave_position.debt_value_usd = 15000.0;
    aave_position.collateral_ratio = 1.5;
    aave_position.liquidation_threshold = 0.8;
    aave_position.liquidation_price = 36000.0;
    aave_position.health_factor = 1.8;
    aave_position.supply_apy = 2.5;
    aave_position.borrow_apy = 4.2;
    aave_position.net_apy = -1.7;
    aave_position.rewards_apy = 3.8;
    portfolio.add_lending_position(aave_position);
    
    // Add yield farming position (Curve)
    YieldFarmPosition curve_farm;
    curve_farm.farm_address = "0xbFcF63294aD7105dEa65aA58F8AE5BE2D9d0952A";
    curve_farm.protocol = Protocol::Curve;
    curve_farm.chain_id = ChainId::Ethereum;
    curve_farm.staked_tokens = {usdc, TokenInfo("DAI", "Dai Stablecoin", "0x6B175474E89094C44Da98b954EedeAC495271d0F", ChainId::Ethereum)};
    curve_farm.staked_amounts = {5000.0, 5000.0};
    curve_farm.total_staked_value_usd = 10000.0;
    
    TokenInfo crv("CRV", "Curve DAO Token", "0xD533a949740bb3306d119CC777fa900bA034cd52", ChainId::Ethereum);
    curve_farm.reward_tokens = {crv};
    curve_farm.pending_rewards = {150.0};
    curve_farm.claimed_rewards = {50.0};
    curve_farm.total_rewards_value_usd = 200.0;
    curve_farm.current_apy = 8.5;
    curve_farm.effective_apy = 9.2;
    curve_farm.smart_contract_risk_score = 15.0;
    curve_farm.impermanent_loss_risk = 2.0; // Low for stablecoin pool
    curve_farm.liquidity_risk_score = 10.0;
    portfolio.add_yield_farm_position(curve_farm);
    
    return portfolio;
}

/**
 * @brief Demonstrate basic cryptocurrency portfolio analysis
 */
void demonstrate_crypto_portfolio_analysis() {
    std::cout << "=== Cryptocurrency Portfolio Analysis ===\n\n";
    
    auto portfolio = create_sample_crypto_portfolio();
    
    std::cout << "Portfolio Overview:\n";
    std::cout << "Total Value: $" << std::fixed << std::setprecision(2) 
              << portfolio.get_total_value_usd() << "\n\n";
    
    // Basic holdings
    std::cout << "Spot Holdings:\n";
    std::cout << "Symbol   Balance        Value USD    Chain\n";
    std::cout << "----------------------------------------\n";
    auto all_holdings = portfolio.get_all_holdings();
    for (const auto& holding : all_holdings) {
        std::cout << std::setw(8) << holding.token.symbol
                  << std::setw(12) << std::setprecision(4) << holding.balance
                  << std::setw(12) << std::setprecision(2) << holding.value_usd
                  << std::setw(10) << static_cast<int>(holding.token.chain_id)
                  << "\n";
    }
    
    // Liquidity positions
    std::cout << "\nLiquidity Positions:\n";
    std::cout << "Protocol    Pool Value    APY      24h Fees   IL Risk\n";
    std::cout << "---------------------------------------------------\n";
    for (const auto& position : portfolio.get_liquidity_positions()) {
        std::cout << std::setw(11) << "Uniswap_V3"
                  << std::setw(12) << std::setprecision(0) << position.total_value_usd
                  << std::setw(8) << std::setprecision(1) << position.current_apy << "%"
                  << std::setw(10) << std::setprecision(2) << position.fees_earned_24h
                  << std::setw(8) << std::setprecision(1) << position.impermanent_loss_pct << "%"
                  << "\n";
    }
    
    // Lending positions
    std::cout << "\nLending Positions:\n";
    std::cout << "Protocol  Collateral    Debt         Health   Liq. Risk\n";
    std::cout << "-----------------------------------------------------\n";
    for (const auto& position : portfolio.get_lending_positions()) {
        std::cout << std::setw(8) << "Aave"
                  << std::setw(12) << std::setprecision(0) << position.collateral_value_usd
                  << std::setw(12) << std::setprecision(0) << position.debt_value_usd
                  << std::setw(10) << std::setprecision(2) << position.health_factor
                  << std::setw(10) << "Medium"
                  << "\n";
    }
    
    // Yield farming positions
    std::cout << "\nYield Farming Positions:\n";
    std::cout << "Protocol  Staked Value   APY     Risk Score  Rewards\n";
    std::cout << "---------------------------------------------------\n";
    for (const auto& position : portfolio.get_yield_farm_positions()) {
        std::cout << std::setw(8) << "Curve"
                  << std::setw(12) << std::setprecision(0) << position.total_staked_value_usd
                  << std::setw(8) << std::setprecision(1) << position.current_apy << "%"
                  << std::setw(12) << std::setprecision(0) << position.smart_contract_risk_score
                  << std::setw(8) << std::setprecision(0) << position.total_rewards_value_usd
                  << "\n";
    }
    
    std::cout << "\n";
}

/**
 * @brief Demonstrate impermanent loss analysis
 */
void demonstrate_impermanent_loss_analysis() {
    std::cout << "=== Impermanent Loss Analysis ===\n\n";
    
    auto portfolio = create_sample_crypto_portfolio();
    DeFiAnalyzer analyzer;
    
    auto lp_positions = portfolio.get_liquidity_positions();
    if (!lp_positions.empty()) {
        const auto& position = lp_positions[0];
        
        // Current token prices (ETH has increased from $1900 to $2000)
        std::vector<double> current_prices = {2000.0, 1.0}; // ETH, USDC
        
        auto il_result = analyzer.calculate_impermanent_loss(position, current_prices);
        if (il_result.is_ok()) {
            const auto& analysis = il_result.value();
            
            std::cout << "ETH/USDC Liquidity Position Analysis:\n";
            std::cout << "Entry Price: ETH $1,900, USDC $1.00\n";
            std::cout << "Current Price: ETH $2,000, USDC $1.00\n";
            std::cout << "Price Change: ETH +5.26%\n\n";
            
            std::cout << "Impermanent Loss: " << std::setprecision(2) 
                      << analysis.current_impermanent_loss_pct << "%\n";
            std::cout << "HODL Value: $" << std::setprecision(2) << analysis.hodl_value_usd << "\n";
            std::cout << "LP Position Value: $" << analysis.current_lp_value_usd << "\n";
            std::cout << "Fees Earned: $" << analysis.total_fees_earned_usd << "\n";
            std::cout << "Net vs HODL: " << std::showpos << analysis.net_performance_vs_hodl_pct 
                      << "%" << std::noshowpos << "\n";
            
            if (analysis.fees_vs_hodl_breakeven_days > 0) {
                std::cout << "Fees Breakeven: " << std::setprecision(1) 
                          << analysis.fees_vs_hodl_breakeven_days << " days\n";
            }
        }
    }
    
    std::cout << "\n";
}

/**
 * @brief Demonstrate liquidation risk analysis
 */
void demonstrate_liquidation_risk_analysis() {
    std::cout << "=== Liquidation Risk Analysis ===\n\n";
    
    auto portfolio = create_sample_crypto_portfolio();
    DeFiAnalyzer analyzer;
    
    auto lending_positions = portfolio.get_lending_positions();
    if (!lending_positions.empty()) {
        const auto& position = lending_positions[0];
        
        auto risk_result = analyzer.calculate_liquidation_risk(position);
        if (risk_result.is_ok()) {
            double risk_score = risk_result.value();
            
            std::cout << "WBTC/USDC Lending Position (Aave):\n";
            std::cout << "Collateral: " << position.collateral_amount << " WBTC ($" 
                      << std::setprecision(0) << position.collateral_value_usd << ")\n";
            std::cout << "Debt: " << position.debt_amount << " USDC ($" 
                      << position.debt_value_usd << ")\n";
            std::cout << "Collateral Ratio: " << std::setprecision(1) 
                      << (position.collateral_ratio * 100) << "%\n";
            std::cout << "Health Factor: " << std::setprecision(2) << position.health_factor << "\n";
            std::cout << "Liquidation Price: $" << std::setprecision(0) 
                      << position.liquidation_price << "\n";
            std::cout << "Current WBTC Price: $" << position.collateral_token.price_usd << "\n";
            
            std::string risk_level;
            if (risk_score < 20) risk_level = "Low";
            else if (risk_score < 50) risk_level = "Medium";
            else if (risk_score < 80) risk_level = "High";
            else risk_level = "Critical";
            
            std::cout << "Liquidation Risk: " << risk_level << " (" 
                      << std::setprecision(1) << risk_score << "/100)\n";
            
            double price_buffer = ((position.collateral_token.price_usd - position.liquidation_price) / 
                                 position.collateral_token.price_usd) * 100.0;
            std::cout << "Price Buffer: " << std::setprecision(1) << price_buffer << "%\n";
        }
    }
    
    std::cout << "\n";
}

/**
 * @brief Demonstrate yield optimization
 */
void demonstrate_yield_optimization() {
    std::cout << "=== Yield Optimization Analysis ===\n\n";
    
    auto portfolio = create_sample_crypto_portfolio();
    YieldOptimizer optimizer;
    
    // Add sample yield opportunities
    YieldOpportunity aave_usdc;
    aave_usdc.protocol = Protocol::Aave;
    aave_usdc.chain_id = ChainId::Ethereum;
    aave_usdc.required_tokens = {TokenInfo("USDC", "USD Coin", "0xA0b86a33E6e131C93C8C5c1a7B5F5c7a7f8f8f8f", ChainId::Ethereum)};
    aave_usdc.required_amounts = {10000.0};
    aave_usdc.total_required_value_usd = 10000.0;
    aave_usdc.base_apy = 3.5;
    aave_usdc.rewards_apy = 2.1;
    aave_usdc.total_apy = 5.6;
    aave_usdc.smart_contract_risk_score = 20.0;
    aave_usdc.liquidity_risk_score = 5.0;
    aave_usdc.overall_risk_score = 25.0;
    aave_usdc.total_value_locked_usd = 8500000000.0; // $8.5B TVL
    aave_usdc.strategy_description = "Supply USDC to Aave v3, earn interest + AAVE rewards";
    aave_usdc.minimum_investment_usd = 100.0;
    aave_usdc.gas_cost_estimate_usd = 25.0;
    optimizer.add_opportunity(aave_usdc);
    
    YieldOpportunity curve_3pool;
    curve_3pool.protocol = Protocol::Curve;
    curve_3pool.chain_id = ChainId::Ethereum;
    curve_3pool.required_tokens = {
        TokenInfo("USDC", "USD Coin", "0xA0b86a33E6e131C93C8C5c1a7B5F5c7a7f8f8f8f", ChainId::Ethereum),
        TokenInfo("USDT", "Tether USD", "0xdAC17F958D2ee523a2206206994597C13D831ec7", ChainId::Ethereum),
        TokenInfo("DAI", "Dai Stablecoin", "0x6B175474E89094C44Da98b954EedeAC495271d0F", ChainId::Ethereum)
    };
    curve_3pool.required_amounts = {3333.0, 3333.0, 3334.0};
    curve_3pool.total_required_value_usd = 10000.0;
    curve_3pool.base_apy = 2.8;
    curve_3pool.rewards_apy = 4.7;
    curve_3pool.total_apy = 7.5;
    curve_3pool.smart_contract_risk_score = 15.0;
    curve_3pool.impermanent_loss_risk_score = 3.0; // Low for stablecoin pool
    curve_3pool.overall_risk_score = 18.0;
    curve_3pool.total_value_locked_usd = 1200000000.0; // $1.2B TVL
    curve_3pool.strategy_description = "Provide liquidity to Curve 3Pool, earn fees + CRV rewards";
    curve_3pool.minimum_investment_usd = 500.0;
    curve_3pool.gas_cost_estimate_usd = 45.0;
    optimizer.add_opportunity(curve_3pool);
    
    YieldOpportunity compound_eth;
    compound_eth.protocol = Protocol::Compound;
    compound_eth.chain_id = ChainId::Ethereum;
    compound_eth.required_tokens = {TokenInfo("ETH", "Ethereum", "0x0000000000000000000000000000000000000000", ChainId::Ethereum)};
    compound_eth.required_amounts = {5.0};
    compound_eth.total_required_value_usd = 10000.0;
    compound_eth.base_apy = 1.8;
    compound_eth.rewards_apy = 3.2;
    compound_eth.total_apy = 5.0;
    compound_eth.smart_contract_risk_score = 25.0;
    compound_eth.overall_risk_score = 30.0;
    compound_eth.total_value_locked_usd = 4200000000.0; // $4.2B TVL
    compound_eth.strategy_description = "Supply ETH to Compound, earn interest + COMP rewards";
    compound_eth.minimum_investment_usd = 1000.0;
    compound_eth.gas_cost_estimate_usd = 35.0;
    optimizer.add_opportunity(compound_eth);
    
    // Find optimal strategies
    auto strategies_result = optimizer.find_optimal_strategies(portfolio, 5.0, 35.0); // 5% target APY, max 35 risk score
    if (strategies_result.is_ok()) {
        const auto& strategies = strategies_result.value();
        
        std::cout << "Optimal Yield Strategies (Target: 5%+ APY, Risk < 35):\n\n";
        std::cout << "Protocol   APY     Risk  TVL        Strategy\n";
        std::cout << "--------------------------------------------------------\n";
        
        for (const auto& strategy : strategies) {
            std::cout << std::setw(10) << (strategy.protocol == Protocol::Curve ? "Curve" :
                                          strategy.protocol == Protocol::Aave ? "Aave" : "Compound")
                      << std::setw(7) << std::setprecision(1) << strategy.total_apy << "%"
                      << std::setw(7) << std::setprecision(0) << strategy.overall_risk_score
                      << std::setw(10) << std::setprecision(1) << (strategy.total_value_locked_usd / 1e9) << "B"
                      << "  " << strategy.strategy_description.substr(0, 40) << "\n";
        }
        
        if (!strategies.empty()) {
            std::cout << "\nTop Recommendation: " << (strategies[0].protocol == Protocol::Curve ? "Curve 3Pool" :
                                                     strategies[0].protocol == Protocol::Aave ? "Aave USDC" : "Compound ETH") << "\n";
            std::cout << "Expected APY: " << std::setprecision(1) << strategies[0].total_apy << "%\n";
            std::cout << "Risk Score: " << strategies[0].overall_risk_score << "/100\n";
            std::cout << "Risk-Adjusted Return: " << std::setprecision(2) 
                      << (strategies[0].total_apy / strategies[0].overall_risk_score) << "\n";
        }
    }
    
    std::cout << "\n";
}

/**
 * @brief Demonstrate gas optimization
 */
void demonstrate_gas_optimization() {
    std::cout << "=== Gas Optimization Analysis ===\n\n";
    
    GasOptimizer gas_optimizer;
    
    std::vector<ChainId> chains = {
        ChainId::Ethereum,
        ChainId::Polygon,
        ChainId::BinanceSmartChain,
        ChainId::Arbitrum,
        ChainId::Optimism
    };
    
    std::cout << "Gas Price Comparison:\n";
    std::cout << "Chain             Gas Price    Tx Cost (Swap)  Cost Ratio\n";
    std::cout << "--------------------------------------------------------\n";
    
    double transaction_value = 5000.0; // $5000 transaction
    double swap_gas_limit = 150000;   // Typical DEX swap
    
    for (ChainId chain : chains) {
        auto gas_price_result = gas_optimizer.estimate_optimal_gas_price(chain);
        if (gas_price_result.is_ok()) {
            double gas_price = gas_price_result.value();
            
            auto cost_result = gas_optimizer.calculate_transaction_cost_usd(
                chain, swap_gas_limit, gas_price);
            if (cost_result.is_ok()) {
                double cost_usd = cost_result.value();
                double cost_ratio = (cost_usd / transaction_value) * 100.0;
                
                std::string chain_name;
                switch (chain) {
                    case ChainId::Ethereum: chain_name = "Ethereum"; break;
                    case ChainId::Polygon: chain_name = "Polygon"; break;
                    case ChainId::BinanceSmartChain: chain_name = "BSC"; break;
                    case ChainId::Arbitrum: chain_name = "Arbitrum"; break;
                    case ChainId::Optimism: chain_name = "Optimism"; break;
                    default: chain_name = "Unknown"; break;
                }
                
                std::cout << std::setw(16) << chain_name
                          << std::setw(12) << std::setprecision(1) << gas_price << " gwei"
                          << std::setw(12) << std::setprecision(2) << cost_usd
                          << std::setw(10) << std::setprecision(3) << cost_ratio << "%"
                          << "\n";
            }
        }
    }
    
    // Find optimal chain
    auto optimal_chain_result = gas_optimizer.find_optimal_chain_for_transaction(chains, transaction_value);
    if (optimal_chain_result.is_ok()) {
        ChainId optimal = optimal_chain_result.value();
        std::string optimal_name;
        switch (optimal) {
            case ChainId::Ethereum: optimal_name = "Ethereum"; break;
            case ChainId::Polygon: optimal_name = "Polygon"; break;
            case ChainId::BinanceSmartChain: optimal_name = "BSC"; break;
            case ChainId::Arbitrum: optimal_name = "Arbitrum"; break;
            case ChainId::Optimism: optimal_name = "Optimism"; break;
        }
        
        std::cout << "\nOptimal Chain for $" << std::setprecision(0) << transaction_value 
                  << " transaction: " << optimal_name << "\n";
    }
    
    std::cout << "\n";
}

/**
 * @brief Demonstrate MEV analysis
 */
void demonstrate_mev_analysis() {
    std::cout << "=== MEV (Maximal Extractable Value) Analysis ===\n\n";
    
    MEVAnalyzer mev_analyzer;
    
    // Analyze arbitrage opportunities
    TokenInfo usdc("USDC", "USD Coin", "0xA0b86a33E6e131C93C8C5c1a7B5F5c7a7f8f8f8f", ChainId::Ethereum);
    std::vector<Protocol> dex_protocols = {
        Protocol::Uniswap_V2,
        Protocol::SushiSwap,
        Protocol::Curve,
        Protocol::Balancer
    };
    
    auto arbitrage_result = mev_analyzer.detect_arbitrage_opportunities(dex_protocols, usdc, 10.0);
    if (arbitrage_result.is_ok()) {
        const auto& opportunities = arbitrage_result.value();
        
        std::cout << "Arbitrage Opportunities:\n";
        std::cout << "Type        Profit    Gas Cost  Net Profit  Success %\n";
        std::cout << "---------------------------------------------------\n";
        
        for (const auto& opportunity : opportunities) {
            std::cout << std::setw(11) << opportunity.opportunity_type
                      << std::setw(9) << std::setprecision(0) << opportunity.profit_potential_usd
                      << std::setw(10) << opportunity.gas_cost_usd
                      << std::setw(11) << opportunity.net_profit_usd
                      << std::setw(9) << std::setprecision(0) << (opportunity.success_probability * 100) << "%"
                      << "\n";
        }
        
        if (!opportunities.empty()) {
            double total_potential = 0.0;
            for (const auto& opp : opportunities) {
                total_potential += opp.net_profit_usd;
            }
            std::cout << "\nTotal MEV Potential: $" << std::setprecision(2) << total_potential << "\n";
        }
    }
    
    // Analyze sandwich attack potential
    auto sandwich_result = mev_analyzer.analyze_sandwich_opportunity("0x123abc...", 100000.0);
    if (sandwich_result.is_ok()) {
        const auto& sandwich = sandwich_result.value();
        
        std::cout << "\nSandwich Attack Analysis:\n";
        std::cout << "Target Trade Size: $100,000\n";
        std::cout << "Estimated Profit: $" << std::setprecision(2) << sandwich.profit_potential_usd << "\n";
        std::cout << "Gas Cost: $" << sandwich.gas_cost_usd << "\n";
        std::cout << "Net Profit: $" << sandwich.net_profit_usd << "\n";
        std::cout << "Success Probability: " << std::setprecision(0) 
                  << (sandwich.success_probability * 100) << "%\n";
    }
    
    std::cout << "\n";
}

/**
 * @brief Demonstrate protocol exposure analysis
 */
void demonstrate_protocol_exposure_analysis() {
    std::cout << "=== Protocol Exposure Analysis ===\n\n";
    
    auto portfolio = create_sample_crypto_portfolio();
    DeFiAnalyzer analyzer;
    
    auto exposure_result = analyzer.calculate_protocol_exposure(portfolio);
    if (exposure_result.is_ok()) {
        const auto& exposures = exposure_result.value();
        
        std::cout << "DeFi Protocol Exposure:\n";
        std::cout << "Protocol      Allocation    Risk Level\n";
        std::cout << "-------------------------------------\n";
        
        for (const auto& [protocol, allocation] : exposures) {
            std::string protocol_name;
            std::string risk_level;
            
            switch (protocol) {
                case Protocol::Uniswap_V3:
                    protocol_name = "Uniswap V3";
                    risk_level = "Medium";
                    break;
                case Protocol::Aave:
                    protocol_name = "Aave";
                    risk_level = "Low";
                    break;
                case Protocol::Curve:
                    protocol_name = "Curve";
                    risk_level = "Low";
                    break;
                default:
                    protocol_name = "Other";
                    risk_level = "Unknown";
                    break;
            }
            
            std::cout << std::setw(13) << protocol_name
                      << std::setw(11) << std::setprecision(1) << (allocation * 100) << "%"
                      << std::setw(12) << risk_level
                      << "\n";
        }
    }
    
    // Calculate portfolio yield
    auto yield_result = analyzer.calculate_portfolio_yield(portfolio);
    if (yield_result.is_ok()) {
        std::cout << "\nPortfolio-wide DeFi Yield: " << std::setprecision(2) 
                  << yield_result.value() << "% APY\n";
    }
    
    std::cout << "\n";
}

/**
 * @brief Main demonstration function
 */
int main() {
    std::cout << "PyFolio C++ Cryptocurrency & DeFi Portfolio Analysis\n";
    std::cout << "====================================================\n\n";
    
    try {
        demonstrate_crypto_portfolio_analysis();
        demonstrate_impermanent_loss_analysis();
        demonstrate_liquidation_risk_analysis();
        demonstrate_yield_optimization();
        demonstrate_gas_optimization();
        demonstrate_mev_analysis();
        demonstrate_protocol_exposure_analysis();
        
        std::cout << "All DeFi analytics demonstrations completed successfully!\n\n";
        std::cout << "Key Features Demonstrated:\n";
        std::cout << "✓ Multi-chain portfolio tracking (Ethereum, Polygon, BSC, L2s)\n";
        std::cout << "✓ DeFi protocol integration (Uniswap, Aave, Curve, Compound)\n";
        std::cout << "✓ Impermanent loss calculation and analysis\n";
        std::cout << "✓ Liquidation risk assessment for lending positions\n";
        std::cout << "✓ Yield farming strategy optimization\n";
        std::cout << "✓ Gas cost optimization across chains\n";
        std::cout << "✓ MEV opportunity detection (arbitrage, sandwich attacks)\n";
        std::cout << "✓ Protocol exposure and risk analysis\n";
        std::cout << "✓ Real-time yield and performance tracking\n";
        
    } catch (const std::exception& e) {
        std::cerr << "Error during demonstration: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}