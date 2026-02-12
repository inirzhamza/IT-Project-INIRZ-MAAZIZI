#pragma once
#include <cstdint>

namespace lookback_mc {

/**
 * @brief Option type for floating-strike lookback options.
 */
enum class OptionType : int {
    Call = 1,
    Put  = 2
};

/**
 * @brief Monte-Carlo settings.
 */
struct MCSettings {
    int steps = 252;           ///< Number of time steps in [0,T]
    int paths = 100000;        ///< Number of Monte-Carlo paths
    std::uint32_t seed = 42;   ///< RNG seed
};

/**
 * @brief Parameters for Black-Scholes (risk-neutral GBM).
 */
struct BSParams {
    double S0 = 100.0;   ///< Spot
    double r  = 0.02;    ///< Risk-free rate
    double sigma = 0.2;  ///< Volatility
    std::int32_t valuation_yyyymmdd = 0; ///< Valuation date as YYYYMMDD (0 = use system date)
    double T = 1.0;      ///< Time to maturity (years)
};

/**
 * @brief Results: price + Greeks.
 */
struct Results {
    double price = 0.0;
    double delta = 0.0;
    double gamma = 0.0;
    double theta = 0.0;
    double rho   = 0.0;
    double vega  = 0.0;
    std::int32_t valuation_yyyymmdd = 0; ///< Valuation date used (YYYYMMDD)
    double std_error = 0.0; ///< MC standard error on price estimate
};

/**
 * @brief Price a floating-strike European lookback option by Monte-Carlo.
 *
 * Payoff at maturity:
 * - Call:  S(T) - min_{t in [0,T]} S(t)
 * - Put:   max_{t in [0,T]} S(t) - S(T)
 *
 * Simulation under risk-neutral GBM:
 * S_{t+dt} = S_t * exp((r - 0.5*sigma^2)dt + sigma*sqrt(dt)*Z)
 *
 * Greeks are computed by bump-and-revalue finite differences
 * with common random numbers for variance reduction.
 *
 * @param type Option type (Call/Put)
 * @param p    Black-Scholes parameters (S0, r, sigma, T)
 * @param mc   Monte-Carlo settings (steps, paths, seed)
 * @return Results struct
 */
Results price_lookback_mc(OptionType type, const BSParams& p, const MCSettings& mc);

} // namespace lookback_mc


