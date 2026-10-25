#pragma once
#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <numeric>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

namespace f1 {
inline double points_for_position(int position, bool sprint = false, bool classified = true,
                                 int season = 2026) {
    static const std::array<int, 10> gp{25,18,15,12,10,8,6,4,2,1};
    static const std::array<int, 8> sp{8,7,6,5,4,3,2,1};
    if (!classified || position < 1) return 0.;
    if (sprint && season == 2021) return position <= 3 ? 4 - position : 0.;
    if (sprint) return position <= 8 ? sp[position - 1] : 0.;
    return position <= 10 ? gp[position - 1] : 0.;
}

inline bool is_retirement(const std::string& status) {
    if (status.empty() || status == "Finished" || status == "Running" ||
        status == "Classified" || status[0] == '+') return false;
    return true;
}

inline bool incident_failure(std::string status) {
    std::transform(status.begin(), status.end(), status.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    for (const auto* word : {"collision", "accident", "spun", "damage", "disqual", "excluded", "injur"})
        if (status.find(word) != std::string::npos) return true;
    return false;
}

struct Classification { std::vector<int> order; std::vector<bool> classified; };

inline Classification classify_finishers(const std::vector<int>& pace_order,
                                          const std::vector<int>& laps,
                                          const std::vector<bool>& retired,
                                          int winner_laps) {
    if (laps.size() != retired.size() || pace_order.size() != laps.size())
        throw std::invalid_argument("Classification vectors must have equal length");
    std::vector<int> rank(laps.size());
    for (std::size_t i = 0; i < pace_order.size(); ++i) rank.at(pace_order[i]) = static_cast<int>(i);
    Classification result{pace_order, std::vector<bool>(laps.size())};
    std::stable_sort(result.order.begin(), result.order.end(), [&](int a, int b) {
        if (laps[a] != laps[b]) return laps[a] > laps[b];
        if (retired[a] != retired[b]) return !retired[a];
        return rank[a] < rank[b];
    });
    const int minimum = static_cast<int>(std::floor(.9 * winner_laps));
    for (std::size_t i = 0; i < laps.size(); ++i) result.classified[i] = laps[i] >= minimum;
    return result;
}

struct ChampionshipOrder {
    std::vector<int> order;
    std::vector<std::vector<int>> unresolved_groups;
};

inline ChampionshipOrder championship_order(const std::vector<double>& points,
                                            const std::vector<std::vector<int>>& gp_counts,
                                            const std::vector<std::vector<int>>& qualifying_counts,
                                            int season, std::mt19937_64& rng) {
    if (points.size() != gp_counts.size() ||
        (season >= 2026 && qualifying_counts.size() != points.size()))
        throw std::invalid_argument("Countback requires one full vector per driver");
    auto compare = [&](int a, int b) {
        if (points[a] != points[b]) return points[a] > points[b];
        if (gp_counts[a] != gp_counts[b]) return gp_counts[a] > gp_counts[b];
        if (season >= 2026 && qualifying_counts[a] != qualifying_counts[b])
            return qualifying_counts[a] > qualifying_counts[b];
        return false;
    };
    ChampionshipOrder result;
    result.order.resize(points.size());
    std::iota(result.order.begin(), result.order.end(), 0);
    std::stable_sort(result.order.begin(), result.order.end(), compare);
    for (std::size_t begin = 0; begin < result.order.size();) {
        std::size_t end = begin + 1;
        while (end < result.order.size() && !compare(result.order[begin], result.order[end]) &&
               !compare(result.order[end], result.order[begin])) ++end;
        if (end - begin > 1) {
            result.unresolved_groups.emplace_back(result.order.begin() + begin, result.order.begin() + end);
            std::shuffle(result.order.begin() + begin, result.order.begin() + end, rng);
        }
        begin = end;
    }
    return result;
}
} // namespace f1
