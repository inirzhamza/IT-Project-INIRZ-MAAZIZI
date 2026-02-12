#include "lookback_mc.h"
#include <random>
#include <vector>
#include <cmath>
#include <algorithm>
#include <chrono>
#include <ctime>

namespace lookback_mc {

namespace {

// Get system date as YYYYMMDD
std::int32_t today_yyyymmdd() {
    using clock = std::chrono::system_clock;
    std::time_t t = clock::to_time_t(clock::now());
    std::tm tm{};
#if defined(_WIN32)
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    const int y = tm.tm_year + 1900;
    const int m = tm.tm_mon + 1;
    const int d = tm.tm_mday;
    return static_cast<std::int32_t>(y * 10000 + m * 100 + d);
}

// Simulate payoff for one path given the normal draws Z[0..steps-1]
double simulate_payoff(OptionType type, const BSParams& p, int steps, const std::vector<double>& Z) {
    const double dt = p.T / static_cast<double>(steps);
    const double drift = (p.r - 0.5 * p.sigma * p.sigma) * dt;
    const double vol = p.sigma * std::sqrt(dt);

    double S = p.S0;
    double s_min = S;
    double s_max = S;

    for (int i = 0; i < steps; ++i) {
        S = S * std::exp(drift + vol * Z[i]);
        s_min = std::min(s_min, S);
        s_max = std::max(s_max, S);
    }

    if (type == OptionType::Call) {
        return std::max(0.0, S - s_min);
    } else {
        return std::max(0.0, s_max - S);
    }
}

// Price only, with std error, using common Z per path
std::pair<double,double> price_only(OptionType type, const BSParams& p, const MCSettings& mc,
                                    const std::vector<std::vector<double>>& normals) {
    const double disc = std::exp(-p.r * p.T);
    double sum = 0.0;
    double sum2 = 0.0;

    for (int i = 0; i < mc.paths; ++i) {
        double payoff = simulate_payoff(type, p, mc.steps, normals[i]);
        double x = disc * payoff;
        sum += x;
        sum2 += x * x;
    }

    const double mean = sum / mc.paths;
    const double var = std::max(0.0, (sum2 / mc.paths) - mean * mean);
    const double se = std::sqrt(var / mc.paths);
    return {mean, se};
}

} // namespace

Results price_lookback_mc(OptionType type, const BSParams& p_in, const MCSettings& mc_in) {
    Results out{};

    // Basic validation
    BSParams p = p_in;
    MCSettings mc = mc_in;

    if (mc.steps < 1) mc.steps = 1;
    if (mc.paths < 1000) mc.paths = 1000;
    if (p.T <= 0.0) p.T = 1e-6;
    if (p.sigma <= 0.0) p.sigma = 1e-8;
    if (p.S0 <= 0.0) p.S0 = 1e-8;

    // Valuation date (informational: model uses T in years)
    if (p.valuation_yyyymmdd == 0) {
        p.valuation_yyyymmdd = today_yyyymmdd();
    }

    // Generate common random numbers: normals[path][step]
    std::mt19937 rng(mc.seed);
    std::normal_distribution<double> nd(0.0, 1.0);

    std::vector<std::vector<double>> normals(static_cast<size_t>(mc.paths),
                                             std::vector<double>(static_cast<size_t>(mc.steps)));

    for (int i = 0; i < mc.paths; ++i) {
        for (int j = 0; j < mc.steps; ++j) {
            normals[i][j] = nd(rng);
        }
    }

    // Base price
    auto [P0, se] = price_only(type, p, mc, normals);
    out.price = P0;
    out.std_error = se;
    out.valuation_yyyymmdd = p.valuation_yyyymmdd;

    // Bumps
    const double hS = std::max(1e-4 * p.S0, 1e-3);
    const double hr = 1e-4;            // 1bp
    const double hs = 1e-4;            // 1e-4 abs vol
    const double hT = 1.0/365.0;       // 1 day in years

    // Delta via central differences
    BSParams pSplus = p;  pSplus.S0 = p.S0 + hS;
    BSParams pSminus = p; pSminus.S0 = std::max(1e-8, p.S0 - hS);

    const double Pplus = price_only(type, pSplus, mc, normals).first;
    const double Pminus = price_only(type, pSminus, mc, normals).first;

    out.delta = (Pplus - Pminus) / (pSplus.S0 - pSminus.S0);

    // Gamma
    const double hGamma = (p.S0 > hS) ? hS : (0.5 * p.S0);
    BSParams pGp = p; pGp.S0 = p.S0 + hGamma;
    BSParams pGm = p; pGm.S0 = std::max(1e-8, p.S0 - hGamma);
    const double PGp = price_only(type, pGp, mc, normals).first;
    const double PGm = price_only(type, pGm, mc, normals).first;

    if (std::abs((pGp.S0 - p.S0) - (p.S0 - pGm.S0)) < 1e-12) {
        out.gamma = (PGp - 2.0*P0 + PGm) / (hGamma*hGamma);
    } else {
        // Unequal step second derivative fallback
        const double h1 = p.S0 - pGm.S0;
        const double h2 = pGp.S0 - p.S0;
        if (h1 > 0.0 && h2 > 0.0) {
            out.gamma = 2.0 * ( (PGp - P0)/h2 - (P0 - PGm)/h1 ) / (h1 + h2);
        } else {
            out.gamma = 0.0;
        }
    }

    // Rho
    BSParams prp = p; prp.r = p.r + hr;
    BSParams prm = p; prm.r = p.r - hr;
    const double Prp = price_only(type, prp, mc, normals).first;
    const double Prm = price_only(type, prm, mc, normals).first;
    out.rho = (Prp - Prm) / (2.0 * hr);

    // Vega
    BSParams pvp = p; pvp.sigma = p.sigma + hs;
    BSParams pvm = p; pvm.sigma = std::max(1e-8, p.sigma - hs);
    const double Pvp = price_only(type, pvp, mc, normals).first;
    const double Pvm = price_only(type, pvm, mc, normals).first;
    out.vega = (Pvp - Pvm) / (pvp.sigma - pvm.sigma);

    // Theta: -dP/dT approx (time to maturity)
    if (p.T > hT) {
        BSParams pTm = p; pTm.T = p.T - hT;
        const double PTm = price_only(type, pTm, mc, normals).first;
        out.theta = (PTm - P0) / hT;
    } else {
        out.theta = 0.0;
    }

    return out;
}

} // namespace lookback_mc
