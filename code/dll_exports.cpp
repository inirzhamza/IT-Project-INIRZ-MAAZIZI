#include "lookback_mc.h"

#if defined(_WIN32)
  #define DLL_EXPORT extern "C" __declspec(dllexport)
#else
  #define DLL_EXPORT extern "C"
#endif

// Returns 0 if OK, non-zero error code otherwise



/**
 * @brief DLL entry point callable from Excel/VBA.
 *
 * @param optionType 1=Call, 2=Put
 * @param S0 Spot
 * @param r  Rate (cont.)
 * @param sigma Vol
 * @param T  Time to maturity (years)
 * @param steps MC steps
 * @param paths MC paths
 * @param seed RNG seed
 * @param out_price etc pointers to doubles (must be non-null)
 * @return 0 if OK, non-zero error code
 */
DLL_EXPORT long LookbackMC(
    long optionType,
    double S0, double r, double sigma, double T,
    long steps, long paths, long seed,
    long valuation_yyyymmdd,
    double& out_price,
    double& out_delta,
    double& out_gamma,
    double& out_theta,
    double& out_rho,
    double& out_vega,
    double& out_std_error
) {
    try {
        lookback_mc::OptionType type =
            (optionType == 2) ? lookback_mc::OptionType::Put : lookback_mc::OptionType::Call;

        lookback_mc::BSParams p{};
        p.S0 = S0;
        p.r = r;
        p.sigma = sigma;
        p.T = T;
        p.valuation_yyyymmdd = static_cast<std::int32_t>(valuation_yyyymmdd);

        lookback_mc::MCSettings mc{};
        mc.steps = static_cast<int>(steps);
        mc.paths = static_cast<int>(paths);
        mc.seed  = static_cast<std::uint32_t>(seed);

        lookback_mc::Results res = lookback_mc::price_lookback_mc(type, p, mc);

        out_price = res.price;
        out_delta = res.delta;
        out_gamma = res.gamma;
        out_theta = res.theta;
        out_rho   = res.rho;
        out_vega  = res.vega;
        out_std_error = res.std_error;

        return 0;
    } catch (...) {
        return 1;
    }
} 
