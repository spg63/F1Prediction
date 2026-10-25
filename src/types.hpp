#pragma once
#include <string>
#include <vector>

namespace f1 {
struct Event {
    int season=0, round=0;
    std::string circuit_id, name, race_start, qualifying_start, sprint_start;
};
struct Result {
    int season=0, round=0;
    std::string driver_id, name, team_id;
    int position=0, grid=0, laps=0;
    std::string status;
    double points=0;
};
struct Qualifying {
    int season=0, round=0;
    std::string driver_id, name, team_id;
    int position=0;
};
struct Standing {
    int season=0, round=0;
    std::string driver_id, name, team_id;
    int position=0;
    double points=0;
    int wins=0;
};
struct Grid {
    int season=0, round=0;
    std::string driver_id;
    int position=0;
    std::string published_at; // conservative known-at timestamp (retrieval if not published)
};
struct Data {
    std::vector<Event> events;
    std::vector<Result> results, sprints;
    std::vector<Qualifying> qualifying;
    std::vector<Standing> standings;
    std::vector<Grid> grids;
};
struct Config {
    double half_life=6, noise_scale=1, reliability_scale=1, parameter_scale=1;
    double grid_effect=.25, track_scale=.15;
    std::string baseline="form"; // form, standings, uniform
};
struct DriverForecast {
    std::string id, name, team;
    double current_points=0, mean_points=0, q05=0, q50=0, q95=0, p_dnf=0;
    std::vector<double> rank_probabilities;
};
struct RaceForecast {
    int round=0;
    std::string name, session;
    bool observed_grid=false;
    std::vector<DriverForecast> drivers;
};
struct Forecast {
    int season=0, through_round=0, simulations=0;
    unsigned seed=0;
    std::string cutoff;
    std::vector<DriverForecast> championship;
    std::vector<RaceForecast> races;
    std::vector<std::string> notes;
    int unresolved_ties=0;
};
}
