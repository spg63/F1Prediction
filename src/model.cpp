#include "model.hpp"
#include "rules.hpp"
#include <cstdint>
#include <cstdio>
#include <map>
#include <set>

namespace f1 {
namespace {
// UTC calendar arithmetic, independent of the machine's timezone.
std::int64_t seconds(const std::string& iso) {
    int year, month, day, hour, minute, second;
    if (std::sscanf(iso.c_str(), "%d-%d-%dT%d:%d:%d", &year, &month, &day, &hour, &minute, &second) != 6)
        throw std::invalid_argument("Expected an ISO UTC timestamp: " + iso);
    if (iso.back() != 'Z' && iso.find("+00:00") == std::string::npos)
        throw std::invalid_argument("Timestamp must be UTC: " + iso);
    year -= month <= 2;
    const int era = (year >= 0 ? year : year - 399) / 400;
    const unsigned yoe = static_cast<unsigned>(year - era * 400);
    const unsigned doy = (153 * (month + (month > 2 ? -3 : 9)) + 2) / 5 + day - 1;
    const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return (static_cast<std::int64_t>(era) * 146097 + doe - 719468) * 86400 + hour * 3600 + minute * 60 + second;
}
bool completed(const std::string& start, std::int64_t cutoff, int conservative_minutes) {
    return !start.empty() && seconds(start) + conservative_minutes * 60 <= cutoff;
}
double rank_score(int position, int field) {
    return field > 1 ? (field + 1. - 2. * position) / (field - 1.) : 0.;
}
std::string track_type(const std::string& circuit) {
    static const std::set<std::string> street{"monaco", "marina_bay", "baku", "jeddah", "las_vegas", "vegas"};
    static const std::set<std::string> fast{"monza", "spa", "silverstone", "suzuka", "losail", "red_bull_ring"};
    static const std::set<std::string> mixed{"bahrain", "albert_park", "shanghai", "miami", "imola", "villeneuve",
        "gilles_villeneuve", "catalunya", "barcelona", "hungaroring", "zandvoort", "americas", "austin",
        "rodriguez", "mexico", "interlagos", "yas_marina", "istanbul", "sepang", "hockenheimring", "nurburgring",
        "portimao", "ricard", "mugello", "sakhir"};
    if (street.count(circuit)) return "street";
    if (fast.count(circuit)) return "fast";
    return mixed.count(circuit) ? "mixed" : "unknown";
}
struct Mean {
    double weight = 0., sum = 0., square_weight = 0.;
    void add(double value, double w) { weight += w; sum += w * value; square_weight += w * w; }
    double shrunk(double prior_weight) const { return sum / (weight + prior_weight); }
    double effective() const { return square_weight > 0 ? weight * weight / square_weight : 0.; }
};
struct Driver {
    std::string id, name, team;
    int latest = -1;
    double points = 0., mean = 0., uncertainty = .12, incident_a = 1., incident_b = 24.;
};
struct Observation { std::string driver, team, track; double score, weight; };
double beta_draw(double a, double b, std::mt19937_64& rng) {
    std::gamma_distribution<double> first(a, 1.), second(b, 1.);
    const double x = first(rng), y = second(rng);
    return x / (x + y);
}
double quantile(std::vector<double> values, double q) {
    if (values.empty()) return 0.;
    const auto index = static_cast<std::size_t>(q * (values.size() - 1));
    std::nth_element(values.begin(), values.begin() + index, values.end());
    return values[index];
}
bool disqualified(const std::string& status) {
    return status.find("Disqual") != std::string::npos || status.find("Excluded") != std::string::npos;
}
} // namespace

Forecast forecast(const Data& data, int season, int through_round, const std::string& cutoff,
                  int simulations, unsigned seed, const Config& config) {
    if (simulations < 1 || config.half_life <= 0 || config.noise_scale <= 0 ||
        config.parameter_scale < 0 || config.reliability_scale < 0 || config.track_scale < 0 ||
        config.grid_effect < 0 || config.grid_effect > 1)
        throw std::invalid_argument("Invalid simulation count or model configuration");
    if (config.baseline != "form" && config.baseline != "standings" && config.baseline != "uniform")
        throw std::invalid_argument("Unknown baseline: " + config.baseline);
    const auto cutoff_seconds = seconds(cutoff);
    Forecast output;
    output.season = season; output.through_round = through_round; output.simulations = simulations;
    output.seed = seed; output.cutoff = cutoff;
    std::map<int, Event> events;
    for (const auto& event : data.events) if (event.season == season) events[event.round] = event;
    if (events.empty()) throw std::invalid_argument("No season calendar in data");
    auto race_known = [&](int round) {
        auto event = events.find(round);
        return round <= through_round && event != events.end() && completed(event->second.race_start, cutoff_seconds, 180);
    };
    auto quali_known = [&](int round) {
        auto event = events.find(round);
        return event != events.end() && completed(event->second.qualifying_start, cutoff_seconds, 90);
    };
    auto sprint_known = [&](int round) {
        auto event = events.find(round);
        return event != events.end() && completed(event->second.sprint_start, cutoff_seconds, 60);
    };
    std::map<int, std::vector<Result>> races, sprints;
    std::map<int, std::vector<Qualifying>> qualifying;
    std::map<std::string, Driver> drivers;
    auto remember = [&](const std::string& id, const std::string& name, const std::string& team, int stamp) {
        if (id.empty()) return;
        auto& driver = drivers[id]; driver.id = id;
        if (stamp >= driver.latest) { driver.name = name; driver.team = team; driver.latest = stamp; }
    };
    for (const auto& row : data.results) if (row.season == season && race_known(row.round)) {
        races[row.round].push_back(row); remember(row.driver_id, row.name, row.team_id, row.round * 4 + 3);
        drivers[row.driver_id].points += row.points;
    }
    for (const auto& row : data.sprints) if (row.season == season && sprint_known(row.round)) {
        sprints[row.round].push_back(row); remember(row.driver_id, row.name, row.team_id, row.round * 4);
        drivers[row.driver_id].points += row.points;
    }
    for (const auto& row : data.qualifying) if (row.season == season && quali_known(row.round)) {
        qualifying[row.round].push_back(row); remember(row.driver_id, row.name, row.team_id, row.round * 4 + 2);
    }
    // Standings snapshots are eligible only at/before the requested GP round.
    // At an exact matching snapshot they preserve penalties and observed points.
    std::map<std::string, Standing> standings;
    for (const auto& row : data.standings) if (row.season == season && row.round <= through_round && race_known(row.round)) {
        if (!standings.count(row.driver_id) || row.round > standings[row.driver_id].round) standings[row.driver_id] = row;
        remember(row.driver_id, row.name, row.team_id, row.round * 4 + 3);
    }
    bool used_standings = false;
    for (const auto& pair : standings) if (pair.second.round == through_round) {
        auto& driver = drivers[pair.first]; driver.points = pair.second.points; used_standings = true;
        for (const auto& sprint : sprints) if (sprint.first > through_round)
            for (const auto& row : sprint.second) if (row.driver_id == pair.first) driver.points += row.points;
    }
    if (drivers.empty()) throw std::invalid_argument("No as-of driver roster/results; refusing a synthetic forecast");
    std::set<std::string> active;
    int latest_roster_round = -1;
    for (const auto& race : races) if (race.first >= latest_roster_round) {
        latest_roster_round = race.first; active.clear();
        for (const auto& row : race.second) active.insert(row.driver_id);
    }
    for (const auto& session : qualifying) if (session.first > latest_roster_round) {
        latest_roster_round = session.first; active.clear();
        for (const auto& row : session.second) active.insert(row.driver_id);
    }
    if (active.empty()) for (const auto& row : standings) active.insert(row.first);
    if (active.empty()) throw std::invalid_argument("No identifiable as-of active entrants");
    const std::size_t count = drivers.size();
    std::vector<Driver> field;
    std::map<std::string, int> index;
    for (const auto& pair : drivers) { index[pair.first] = static_cast<int>(field.size()); field.push_back(pair.second); }
    std::vector<int> entrants;
    for (const auto& id : active) entrants.push_back(index.at(id));
    std::map<std::string, Mean> team_form, driver_residual, track_residual;
    std::map<std::string, std::pair<double,double>> mechanical;
    std::map<std::string, int> starts;
    std::vector<Observation> observations;
    std::size_t positions = count;
    for (const auto& race : races) {
        const double recency = std::pow(.5, std::max(0, through_round - race.first) / config.half_life);
        std::vector<Result> finishers;
        for (const auto& row : race.second) {
            positions = std::max(positions, static_cast<std::size_t>(std::max(0, row.position)));
            ++starts[row.driver_id];
            auto& hazard = mechanical[row.team_id];
            if (hazard.first == 0 && hazard.second == 0) hazard = {1.5, 28.5};
            auto& driver = field[index.at(row.driver_id)];
            const bool retired = is_retirement(row.status);
            if (retired && incident_failure(row.status)) driver.incident_a += recency;
            else driver.incident_b += recency;
            if (retired && !incident_failure(row.status)) hazard.first += recency;
            else hazard.second += recency;
            if (!retired && row.position > 0) finishers.push_back(row);
        }
        std::sort(finishers.begin(), finishers.end(), [](const Result& a, const Result& b) { return a.position < b.position; });
        for (std::size_t place = 0; place < finishers.size(); ++place) {
            const auto& row = finishers[place];
            const int grid = row.grid > 0 ? row.grid : static_cast<int>(race.second.size());
            const double score = rank_score(static_cast<int>(place + 1), static_cast<int>(finishers.size())) -
                                 config.grid_effect * rank_score(grid, static_cast<int>(race.second.size()));
            observations.push_back({row.driver_id, row.team_id, track_type(events.at(race.first).circuit_id), score, recency});
        }
    }
    for (const auto& session : qualifying) if (session.first <= through_round) {
        const double weight = .55 * std::pow(.5, std::max(0, through_round - session.first) / config.half_life);
        for (const auto& row : session.second) if (row.position > 0)
            observations.push_back({row.driver_id, row.team_id, track_type(events.at(session.first).circuit_id),
                (1. - config.grid_effect) * rank_score(row.position, static_cast<int>(session.second.size())), weight});
    }
    for (const auto& row : observations) team_form[row.team].add(row.score, row.weight);
    for (const auto& row : observations)
        driver_residual[row.driver].add(row.score - team_form[row.team].shrunk(2.), row.weight);
    for (auto& driver : field) {
        const auto& residual = driver_residual[driver.id];
        driver.mean = team_form[driver.team].shrunk(2.) + residual.shrunk(4.);
        driver.uncertainty = .025 + .18 / std::sqrt(2. + residual.effective());
    }
    for (const auto& row : observations)
        track_residual[row.team + "/" + row.track].add(row.score - team_form[row.team].shrunk(2.) -
                                                     driver_residual[row.driver].shrunk(4.), row.weight);
    if (config.baseline == "standings") {
        double mean = 0., variance = 0.;
        for (auto& driver : field) { driver.mean = std::log(1. + driver.points / std::max(1, starts[driver.id])); mean += driver.mean; }
        mean /= count;
        for (const auto& driver : field) variance += std::pow(driver.mean - mean, 2);
        const double scale = std::sqrt(variance / count);
        for (auto& driver : field) driver.mean = scale > 0 ? .45 * (driver.mean - mean) / scale : 0.;
    } else if (config.baseline == "uniform") for (auto& driver : field) driver.mean = 0.;
    std::vector<std::vector<int>> historical_gp(count, std::vector<int>(positions)), historical_quali = historical_gp;
    for (const auto& race : races) {
        int winner_laps = 0;
        for (const auto& row : race.second) winner_laps = std::max(winner_laps, row.laps);
        for (const auto& row : race.second) if (row.position > 0 && !disqualified(row.status) &&
              row.laps >= static_cast<int>(std::floor(.9 * winner_laps)))
            ++historical_gp[index.at(row.driver_id)][row.position - 1];
    }
    int missing_qualifying = 0, partial_qualifying = 0;
    for (const auto& race : races) if (!qualifying.count(race.first)) ++missing_qualifying;
    else {
        std::set<std::string> qualified;
        for (const auto& row : qualifying.at(race.first)) qualified.insert(row.driver_id);
        for (const auto& row : race.second) if (!qualified.count(row.driver_id)) { ++partial_qualifying; break; }
    }
    for (const auto& session : qualifying) for (const auto& row : session.second)
        if (row.position > 0 && static_cast<std::size_t>(row.position) <= positions)
            ++historical_quali[index.at(row.driver_id)][row.position - 1];
    struct Future { Event event; bool sprint; std::map<int,int> grid; bool known_qualifying = false; };
    std::vector<Future> future;
    for (const auto& event_pair : events) {
        const auto& event = event_pair.second;
        if (event.round <= through_round) continue;
        if (seconds(event.race_start) <= cutoff_seconds)
            throw std::invalid_argument("A GP has started after through_round; refuse to treat it as an unstarted future race");
        if (!event.sprint_start.empty() && !sprints.count(event.round)) {
            if (seconds(event.sprint_start) <= cutoff_seconds)
                throw std::invalid_argument("A scheduled sprint has started but its as-of results are missing");
            future.push_back({event, true, {}, false});
        }
        Future gp{event, false, {}, qualifying.count(event.round) != 0};
        if (gp.known_qualifying) for (const auto& row : qualifying.at(event.round))
            if (active.count(row.driver_id)) gp.grid[index.at(row.driver_id)] = row.position;
        // Published grid overrides qualifying order, so penalties are applied once.
        for (const auto& row : data.grids) if (row.season == season && row.round == event.round &&
              !row.published_at.empty() && seconds(row.published_at) <= cutoff_seconds && active.count(row.driver_id))
            gp.grid[index.at(row.driver_id)] = row.position;
        if (!gp.grid.empty()) {
            std::set<int> unique;
            for (const auto& grid : gp.grid) unique.insert(grid.second);
            if (gp.grid.size() != entrants.size() || unique.size() != entrants.size() || *unique.begin() != 1 ||
                *unique.rbegin() != static_cast<int>(entrants.size()))
                throw std::invalid_argument("Observed future grid is partial or has duplicate/missing positions");
        }
        future.push_back(gp);
    }
    std::sort(future.begin(), future.end(), [](const Future& a, const Future& b) {
        return (a.sprint ? a.event.sprint_start : a.event.race_start) < (b.sprint ? b.event.sprint_start : b.event.race_start);
    });
    output.championship.resize(count);
    std::vector<std::vector<double>> total_samples(count);
    for (std::size_t d = 0; d < count; ++d) {
        auto& result = output.championship[d]; const auto& driver = field[d];
        result.id = driver.id; result.name = driver.name; result.team = driver.team; result.current_points = driver.points;
        result.rank_probabilities.assign(count, 0.); total_samples[d].reserve(simulations);
    }
    for (const auto& session : future) {
        RaceForecast result; result.round = session.event.round; result.name = session.event.name;
        result.session = session.sprint ? "sprint" : "race"; result.observed_grid = !session.grid.empty();
        for (const int d : entrants) {
            DriverForecast row; row.id = field[d].id; row.name = field[d].name; row.team = field[d].team;
            row.rank_probabilities.assign(entrants.size(), 0.); result.drivers.push_back(row);
        }
        output.races.push_back(result);
    }
    std::mt19937_64 rng(seed);
    std::normal_distribution<double> normal(0., 1.);
    std::uniform_real_distribution<double> uniform(0., 1.);
    auto order_scores = [&](const std::vector<double>& scores) {
        auto order = entrants;
        // Continuous random shocks resolve latent performance ties, never driver names.
        std::sort(order.begin(), order.end(), [&](int a, int b) { return scores[a] > scores[b]; });
        return order;
    };
    for (int simulation = 0; simulation < simulations; ++simulation) {
        std::vector<double> points(count), persistent_driver(count), incident(count), season_mean(count);
        auto gp_counts = historical_gp, quali_counts = historical_quali;
        std::map<std::string,double> persistent_team, mechanical_probability, persistent_track;
        for (std::size_t d = 0; d < count; ++d) {
            const auto& driver = field[d]; points[d] = driver.points;
            if (!persistent_team.count(driver.team)) {
                const double team_sd = config.baseline == "form" ?
                    .025 + .18 / std::sqrt(2. + team_form[driver.team].effective()) : .08;
                persistent_team[driver.team] = normal(rng) * team_sd * config.parameter_scale;
                const auto hazard = mechanical.count(driver.team) ? mechanical.at(driver.team) : std::make_pair(1.5,28.5);
                mechanical_probability[driver.team] = beta_draw(hazard.first, hazard.second, rng);
            }
            persistent_driver[d] = normal(rng) * (config.baseline == "form" ? driver.uncertainty : .08) * config.parameter_scale;
            incident[d] = config.baseline == "form" ? beta_draw(driver.incident_a, driver.incident_b, rng) : .04;
            season_mean[d] = driver.mean + persistent_driver[d] + persistent_team[driver.team];
        }
        int last_round = -1;
        std::vector<double> weekend_driver(count);
        std::map<std::string,double> weekend_team;
        for (std::size_t session_index = 0; session_index < future.size(); ++session_index) {
            const auto& session = future[session_index];
            if (last_round != session.event.round) {
                last_round = session.event.round; weekend_team.clear();
                for (const int d : entrants) weekend_driver[d] = .08 * normal(rng) * config.noise_scale;
                for (const int d : entrants) if (!weekend_team.count(field[d].team))
                    weekend_team[field[d].team] = .08 * normal(rng) * config.noise_scale;
            }
            std::vector<double> pace(count), quali_score(count);
            const auto type = track_type(session.event.circuit_id);
            for (const int d : entrants) {
                const auto key = field[d].team + "/" + type;
                if (!persistent_track.count(key)) persistent_track[key] = .08 * normal(rng) * config.parameter_scale;
                const double track = config.baseline == "form" && type != "unknown" ? config.track_scale *
                    (track_residual[key].shrunk(8.) + persistent_track[key]) : 0.;
                pace[d] = season_mean[d] + track + weekend_driver[d] + weekend_team[field[d].team];
                quali_score[d] = pace[d] + .18 * normal(rng) * config.noise_scale;
            }
            const auto qualifying_order = order_scores(quali_score);
            std::vector<int> grid(count), laps(count);
            for (std::size_t place = 0; place < qualifying_order.size(); ++place) grid[qualifying_order[place]] = static_cast<int>(place + 1);
            if (!session.sprint && !session.grid.empty()) for (const auto& entry : session.grid) grid[entry.first] = entry.second;
            if (!session.sprint && !session.known_qualifying)
                for (std::size_t place = 0; place < qualifying_order.size(); ++place) ++quali_counts[qualifying_order[place]][place];
            for (const int d : entrants) pace[d] += (config.baseline == "form" ? config.grid_effect : 0.) *
                    rank_score(grid[d], static_cast<int>(entrants.size())) + .23 * normal(rng) * config.noise_scale;
            const auto pace_order = order_scores(pace);
            std::vector<bool> retired(count, false);
            const int distance = session.sprint ? 20 : 60; // normalized distance, not a lap-level forecast
            const double distance_scale = session.sprint ? .35 : 1.;
            const double incident_weekend = std::exp(.35 * normal(rng) - .5 * .35 * .35);
            std::map<std::string,double> reliability_weekend;
            for (const int d : entrants) if (!reliability_weekend.count(field[d].team))
                reliability_weekend[field[d].team] = std::exp(.45 * normal(rng) - .5 * .45 * .45);
            int finishers = 0;
            for (const int d : entrants) {
                const double mechanical_base = config.baseline == "form" ? mechanical_probability[field[d].team] : .05;
                const double pm = std::clamp(1. - std::pow(1. - mechanical_base, distance_scale), 0., .95);
                const double pi = 1. - std::pow(1. - incident[d], distance_scale);
                const bool mechanical_dnf = uniform(rng) < std::min(.95, pm * reliability_weekend[field[d].team] * config.reliability_scale);
                const bool incident_dnf = uniform(rng) < std::min(.95, pi * incident_weekend * config.reliability_scale);
                retired[d] = mechanical_dnf || incident_dnf;
                if (retired[d]) laps[d] = incident_dnf && uniform(rng) < .25 ? 0 : static_cast<int>(uniform(rng) * distance);
                else { laps[d] = distance; ++finishers; }
            }
            // The forecast is conditional on a full-distance race taking place.
            if (finishers == 0) { retired[pace_order.front()] = false; laps[pace_order.front()] = distance; }
            std::vector<int> active_laps, local_pace;
            std::vector<bool> active_retired;
            std::map<int,int> local;
            for (std::size_t i = 0; i < entrants.size(); ++i) {
                local[entrants[i]] = static_cast<int>(i); active_laps.push_back(laps[entrants[i]]); active_retired.push_back(retired[entrants[i]]);
            }
            for (const int d : pace_order) local_pace.push_back(local.at(d));
            const auto classification = classify_finishers(local_pace, active_laps, active_retired, distance);
            for (std::size_t place = 0; place < classification.order.size(); ++place) {
                const int local_driver = classification.order[place], d = entrants[local_driver];
                const bool classified = classification.classified[local_driver];
                const double earned = points_for_position(static_cast<int>(place + 1), session.sprint, classified, season);
                points[d] += earned;
                if (!session.sprint && classified) ++gp_counts[d][place];
                auto& result = output.races[session_index].drivers[local_driver];
                result.mean_points += earned / simulations;
                result.p_dnf += (retired[d] ? 1. : 0.) / simulations;
                result.rank_probabilities[place] += 1. / simulations;
            }
        }
        const auto final_order = championship_order(points, gp_counts, quali_counts, season, rng);
        if (!final_order.unresolved_groups.empty()) ++output.unresolved_ties;
        for (std::size_t rank = 0; rank < final_order.order.size(); ++rank) {
            const int d = final_order.order[rank];
            output.championship[d].rank_probabilities[rank] += 1. / simulations;
            output.championship[d].mean_points += points[d] / simulations;
            total_samples[d].push_back(points[d]);
        }
    }
    for (std::size_t d = 0; d < count; ++d) {
        output.championship[d].q05 = quantile(total_samples[d], .05);
        output.championship[d].q50 = quantile(total_samples[d], .5);
        output.championship[d].q95 = quantile(total_samples[d], .95);
    }
    output.notes.push_back("Current-season recency-weighted form baseline; driver/team components are confounded descriptive adjustments, not identified causal skill.");
    output.notes.push_back("Season-level parameter draws and team/weekend shocks represent heuristic model uncertainty; Monte Carlo standard errors measure simulation noise only.");
    output.notes.push_back("Future sessions assume full distance and unchanged latest known entrants. Cancellation, shortened races, future substitutions, weather forecasts, strategies and unannounced penalties are not modeled.");
    output.notes.push_back("Qualifying results require start+90min before cutoff; races start+180min and sprints start+60min. Published grid overrides qualifying once.");
    output.notes.push_back("Retirement timing uses normalized 60/20-lap distances; late retirees can classify at floor(90% of winner distance). Finishing-order ranks include unclassified starters at the back.");
    output.notes.push_back(season >= 2026 ? "2026 countback: GP places, then GP qualifying places; residual exact ties receive random simulation allocation, not an FIA determination." : "Pre-2026 countback: GP places; exact remaining ties fall to FIA discretion and receive random simulation allocation.");
    if (missing_qualifying) output.notes.push_back("WARNING: " + std::to_string(missing_qualifying) + " historical GP qualifying sessions missing; 2026 qualifying countback may be incomplete.");
    if (partial_qualifying) output.notes.push_back("WARNING: " + std::to_string(partial_qualifying) + " historical qualifying sources list fewer entrants than GP sources. Nonparticipation is possible; absent qualifying ranks are not imputed and countback coverage may be incomplete.");
    if (output.unresolved_ties) output.notes.push_back("Unresolved countback ties required random rank allocation in " + std::to_string(output.unresolved_ties) + " of " + std::to_string(simulations) + " simulations; these are not regulatory determinations.");
    if (!used_standings) output.notes.push_back("Current points summed from observed race/sprint points; separate championship penalties absent from result rows may be missing.");
    if (season == 2026) output.notes.push_back("2026 regulation reset: no constructor or driver pace estimates transferred from earlier seasons.");
    return output;
}
} // namespace f1
