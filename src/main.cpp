#include "model.hpp"
#include "rules.hpp"
#include "types.hpp"
#include <algorithm>
#include <cmath>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <numeric>
#include <set>
#include <sstream>
#include <stdexcept>

using namespace f1;
namespace fs = std::filesystem;
namespace {
std::vector<std::string> split(const std::string& s) {
    std::vector<std::string> v; std::string x; std::istringstream in(s);
    while (std::getline(in,x,'\t')) v.push_back(x);
    if (!s.empty() && s.back()=='\t') v.emplace_back();
    return v;
}
template<class F> void read(const fs::path& p,size_t columns,F parse,bool optional=false) {
    std::ifstream in(p); if(!in) { if(optional)return; throw std::runtime_error("Missing input: "+p.string()); }
    std::string line; int n=0;
    while(std::getline(in,line)) { ++n; if(line.empty()||line[0]=='#')continue;
        auto v=split(line); if(v[0]=="season")continue;
        if(v.size()<columns)throw std::runtime_error(p.string()+":"+std::to_string(n)+" too few columns");
        try{parse(v);}catch(const std::exception& e){throw std::runtime_error(p.string()+":"+std::to_string(n)+" "+e.what());}
    }
}
Data load(const fs::path& dir) {
    Data d;
    read(dir/"events.tsv",7,[&](auto& v){d.events.push_back({std::stoi(v[0]),std::stoi(v[1]),v[2],v[3],v[4],v[5],v[6]});});
    auto rows=[&](auto& target,const char* file){read(dir/file,10,[&](auto& v){target.push_back(Result{std::stoi(v[0]),std::stoi(v[1]),v[2],v[3],v[4],std::stoi(v[5]),std::stoi(v[6]),std::stoi(v[7]),v[8],std::stod(v[9])});});};
    rows(d.results,"results.tsv"); rows(d.sprints,"sprints.tsv");
    read(dir/"qualifying.tsv",6,[&](auto& v){d.qualifying.push_back({std::stoi(v[0]),std::stoi(v[1]),v[2],v[3],v[4],std::stoi(v[5])});});
    read(dir/"standings.tsv",8,[&](auto& v){d.standings.push_back({std::stoi(v[0]),std::stoi(v[1]),v[2],v[3],v[4],std::stoi(v[5]),std::stod(v[6]),std::stoi(v[7])});},true);
    read(dir/"grid.tsv",5,[&](auto& v){d.grids.push_back({std::stoi(v[0]),std::stoi(v[1]),v[2],std::stoi(v[3]),v[4]});},true);
    return d;
}
std::time_t utc(const std::string& text) {
    std::tm t{}; std::istringstream in(text); in>>std::get_time(&t,"%Y-%m-%dT%H:%M:%S");
    if(in.fail()||text.size()!=20||text.back()!='Z')throw std::runtime_error("Expected UTC timestamp YYYY-MM-DDTHH:MM:SSZ: "+text);
    return timegm(&t);
}
std::string stamp(std::time_t t) { char b[32]; std::strftime(b,sizeof(b),"%Y-%m-%dT%H:%M:%SZ",std::gmtime(&t));return b; }
int audit(const Data& d,int season,const std::string& cutoff) {
    auto c=utc(cutoff); int through=0,events=0; std::map<std::string,double> points;
    for(const auto& e:d.events)if(e.season==season) {
        ++events; bool expected=utc(e.race_start)+4*3600<=c;
        std::set<std::string> drivers; std::set<int> positions; int count=0;
        for(const auto& r:d.results)if(r.season==season&&r.round==e.round) {
            if(!drivers.insert(r.driver_id).second||!positions.insert(r.position).second)throw std::runtime_error("Duplicate result at round "+std::to_string(e.round));
            ++count; if(expected)points[r.driver_id]+=r.points;
        }
        if(expected) {
            if(count<15)throw std::runtime_error("Stale/incomplete feed: concluded GP round "+std::to_string(e.round)+" has "+std::to_string(count)+" results");
            through=std::max(through,e.round);
        } else if(count>0&&utc(e.race_start)>c)throw std::runtime_error("Input includes a future race result; choose a historical backtest instead");
        if(!e.sprint_start.empty()&&utc(e.sprint_start)+2*3600<=c) {
            int ns=0;for(const auto& r:d.sprints)if(r.season==season&&r.round==e.round){points[r.driver_id]+=r.points;++ns;}
            if(ns<15)throw std::runtime_error("Missing concluded sprint round "+std::to_string(e.round));
        }
    }
    if(!events||!through)throw std::runtime_error("No usable completed season history");
    bool checked=false;
    for(const auto& s:d.standings)if(s.season==season&&s.round==through) {
        checked=true;
        if(std::abs(s.points-points[s.driver_id])>1e-6)throw std::runtime_error("GP+sprint points disagree with standings for "+s.driver_id);
        points.erase(s.driver_id);
    }
    if(!checked)throw std::runtime_error("No matching standings snapshot for GP round "+std::to_string(through));
    for(const auto& p:points)if(p.second!=0)throw std::runtime_error("Standings omitted a scoring driver: "+p.first);
    return through;
}
double top5(const DriverForecast& d) { return std::accumulate(d.rank_probabilities.begin(),d.rank_probabilities.begin()+std::min<size_t>(5,d.rank_probabilities.size()),0.0); }
double title(const DriverForecast& d) {return d.rank_probabilities.empty()?0:d.rank_probabilities[0];}
std::string html(const std::string& s) {std::string o;for(char c:s){if(c=='&')o+="&amp;";else if(c=='<')o+="&lt;";else if(c=='>')o+="&gt;";else if(c=='\"')o+="&quot;";else o+=c;}return o;}
std::string csv(const std::string& s) {std::string o="\"";for(char c:s){if(c=='\"')o+='\"';o+=c;}return o+'\"';}
void check(const Forecast& f) {
    const size_t n=f.championship.size(); double membership=0;
    for(size_t rank=0;rank<n;++rank) {
        double sum=0;for(const auto& d:f.championship){if(d.rank_probabilities.size()!=n)throw std::runtime_error("Bad rank vector");sum+=d.rank_probabilities[rank];}
        if(std::abs(sum-1)>1e-8)throw std::runtime_error("Rank column is not normalized");
    }
    for(const auto& d:f.championship){if(std::abs(std::accumulate(d.rank_probabilities.begin(),d.rank_probabilities.end(),0.0)-1)>1e-8)throw std::runtime_error("Driver ranks not normalized");membership+=top5(d);}
    if(std::abs(membership-std::min<size_t>(5,n))>1e-8)throw std::runtime_error("Top-five mass is not five");
    for(const auto& race:f.races) {
        for(const auto& d:race.drivers)
            if(std::abs(std::accumulate(d.rank_probabilities.begin(),d.rank_probabilities.end(),0.0)-1)>1e-8)
                throw std::runtime_error("Race finish probabilities not normalized");
        for(size_t rank=0;rank<race.drivers.size();++rank) {
            double sum=0;for(const auto& d:race.drivers)sum+=d.rank_probabilities.at(rank);
            if(std::abs(sum-1)>1e-8)throw std::runtime_error("Race finish column not normalized");
        }
    }
}
void write(const Forecast& f,const fs::path& out) {
    check(f);fs::create_directories(out);
    std::ofstream c(out/"championship.csv"),r(out/"races.csv"),h(out/"report.html"),t(out/"summary.txt");
    if(!c||!r||!h||!t)throw std::runtime_error("Cannot write report directory");
    c<<std::fixed<<std::setprecision(8)<<"driver,name,team,current_points,expected_points,q05,q50,q95,p_title,p_top5,mcse_top5";
    for(size_t k=0;k<f.championship.size();++k)c<<",p_rank_"<<k+1;
    c<<"\n";
    auto drivers=f.championship;std::sort(drivers.begin(),drivers.end(),[](auto& a,auto& b){return top5(a)==top5(b)?a.mean_points>b.mean_points:top5(a)>top5(b);});
    t<<"F1 Drivers' Championship forecast\nCutoff "<<f.cutoff<<"; completed GPs "<<f.through_round<<"; simulations "<<f.simulations<<"; seed "<<f.seed<<"\n";
    t<<"Full-distance races; no weather/telemetry forecast. Probabilities are model estimates, not demonstrated calibration.\n\n";
    h<<"<!doctype html><html lang=en><meta charset=utf-8><meta name=viewport content='width=device-width'><title>F1 season forecast</title><style>body{font:16px system-ui;background:#111820;color:#eef2f4;max-width:1150px;margin:35px auto;padding:0 18px}h1{color:#ff6060}table{border-collapse:collapse;width:100%;font-variant-numeric:tabular-nums}th,td{padding:8px;text-align:right;border-bottom:1px solid #34404c}th:first-child,td:first-child{text-align:left}.scroll{overflow:auto}p,li{line-height:1.5;color:#cbd6df}details{margin:18px 0}a{color:#8dc7ff}</style><h1>Drivers’ Championship forecast</h1><p>Cutoff <b>"<<f.cutoff<<"</b> · "<<f.simulations<<" simulations · seed "<<f.seed<<" · "<<f.through_round<<" completed GPs.</p><p>Top five means the final Drivers’ Championship. This is an experimental recent-form Monte Carlo baseline. Full-distance events are assumed. Uncertainty intervals include the model’s sampled pace, incidents and parameter variation; MC error measures sampling precision separately.</p><div class=scroll><table><tr><th>Driver</th><th>Current points</th><th>Expected final</th><th>90% interval</th><th>Champion</th><th>Top five</th><th>MC SE</th></tr>";
    for(const auto& d:drivers) {
        double p=top5(d),se=std::sqrt(p*(1-p)/f.simulations);
        c<<csv(d.id)<<','<<csv(d.name)<<','<<csv(d.team)<<','<<d.current_points<<','<<d.mean_points<<','<<d.q05<<','<<d.q50<<','<<d.q95<<','<<title(d)<<','<<p<<','<<se;
        for(double x:d.rank_probabilities)c<<','<<x;
        c<<'\n';
        t<<std::left<<std::setw(23)<<d.name<<std::right<<std::fixed<<std::setprecision(1)<<" top5 "<<std::setw(5)<<100*p<<"%  title "<<std::setw(5)<<100*title(d)<<"%  points "<<d.mean_points<<" ["<<d.q05<<", "<<d.q95<<"]\n";
        h<<std::fixed<<std::setprecision(1)<<"<tr><td>"<<html(d.name)<<"</td><td>"<<d.current_points<<"</td><td>"<<d.mean_points<<"</td><td>"<<d.q05<<"–"<<d.q95<<"</td><td>"<<100*title(d)<<"%</td><td style='background:rgba(225,65,65,"<<p*.45<<")'>"<<100*p<<"%</td><td>"<<100*se<<" pp</td></tr>";
    }
    h<<"</table></div><h2>Remaining events</h2>";
    r<<std::fixed<<std::setprecision(8)<<"round,event,session,observed_grid,driver,p_win,p_top5,expected_points,p_dnf";
    size_t maxn=0;for(const auto& e:f.races)maxn=std::max(maxn,e.drivers.size());for(size_t k=0;k<maxn;++k)r<<",p_finish_"<<k+1;r<<'\n';
    for(const auto& e:f.races) {
        auto rows=e.drivers;std::sort(rows.begin(),rows.end(),[](auto& a,auto& b){return title(a)>title(b);});
        h<<"<details><summary>"<<e.round<<" · "<<html(e.name)<<" ("<<html(e.session)<<") · "<<(e.observed_grid?"observed grid":"simulated grid")<<"</summary><table><tr><th>Driver</th><th>Win</th><th>Top five</th><th>Points</th><th>DNF</th></tr>";
        for(const auto& d:rows){r<<e.round<<','<<csv(e.name)<<','<<e.session<<','<<e.observed_grid<<','<<csv(d.id)<<','<<title(d)<<','<<top5(d)<<','<<d.mean_points<<','<<d.p_dnf;for(double x:d.rank_probabilities)r<<','<<x;for(size_t k=d.rank_probabilities.size();k<maxn;++k)r<<',';r<<'\n';
            h<<"<tr><td>"<<html(d.name)<<"</td><td>"<<100*title(d)<<"%</td><td>"<<100*top5(d)<<"%</td><td>"<<d.mean_points<<"</td><td>"<<100*d.p_dnf<<"%</td></tr>";}
        h<<"</table></details>";
    }
    h<<"<h2>Assumptions and checks</h2><ul>";t<<"\nNotes:\n";
    for(const auto& note:f.notes){h<<"<li>"<<html(note)<<"</li>";t<<"- "<<note<<'\n';}
    h<<"</ul><p>Sources, timing and rules: <a href='../../docs/SOURCE_REVIEW.md'>source review</a>. See <a href='../../docs/MODEL.md'>model assumptions</a> and <a href='../backtest.csv'>chronological backtest</a>. Full rank distributions: <a href='championship.csv'>championship.csv</a>, <a href='races.csv'>races.csv</a>. Zero estimates are Monte Carlo zeros, not mathematical impossibility; with zero hits, the approximate 95% upper sampling bound is 3/N.</p></html>";
    t.close();std::ifstream summary(out/"summary.txt");std::cout<<summary.rdbuf();
}
double rps(const std::vector<double>& p,int rank) {double c=0,s=0;for(size_t k=0;k+1<p.size();++k){c+=p[k];double y=int(k+1)>=rank?1:0;s+=(c-y)*(c-y);}return p.size()>1?s/(p.size()-1):0;}
double mean_rank(const DriverForecast& d) {
    double value=0;for(size_t i=0;i<d.rank_probabilities.size();++i)value+=(i+1)*d.rank_probabilities[i];return value;
}
std::vector<std::pair<std::string,Config>> ablations() {
    Config base,c;std::vector<std::pair<std::string,Config>> v{{"form",base}};
    c=base;c.baseline="standings";v.push_back({"standings",c});
    c=base;c.track_scale=0;v.push_back({"no_track",c});
    c=base;c.qualifying_weight=0;c.grid_effect=0;v.push_back({"no_qualifying",c});
    c=base;c.half_life=std::numeric_limits<double>::infinity();v.push_back({"no_recency",c});
    c=base;c.independent_team_pace=true;v.push_back({"independent_team_pace",c});
    return v;
}
void overnight(const Data& all,int season,int sims,unsigned seed,const fs::path& dir) {
    fs::create_directories(dir);std::ofstream scores(dir/"scores.csv"),rows(dir/"drivers.csv");
    scores<<"season,through_round,cutoff,next_gp,variant,seed,simulations,boundary_n,contested_n,boundary_brier,contested_brier,boundary_rank_rps,boundary_rank_mae,boundary_points_mae,boundary_pinball,next_race_top5_brier,next_race_rank_rps,unseen_future_drivers,unpredicted_fastest_lap_points\n";
    rows<<"season,through_round,variant,seed,driver,current_points,contested,p_top5,mcse_top5,actual_top5,expected_rank,actual_rank,expected_points,actual_points,q05,q50,q95\n";
    std::map<std::string,Standing> actual;
    int final_round=0,standing_round=0;
    for(const auto& e:all.events)if(e.season==season)final_round=std::max(final_round,e.round);
    for(const auto& s:all.standings)if(s.season==season){actual[s.driver_id]=s;standing_round=std::max(standing_round,s.round);}
    if(!final_round||standing_round!=final_round)throw std::runtime_error("Overnight comparison requires a completed season with final standings");
    for(int fold:{6,8,10,12,14,16,18,20}) {
        const Event* next=nullptr;for(const auto& e:all.events)if(e.season==season&&e.round==fold+1)next=&e;
        if(!next)continue;
        auto first=utc(next->race_start);
        if(!next->qualifying_start.empty())first=std::min(first,utc(next->qualifying_start));
        if(!next->sprint_start.empty())first=std::min(first,utc(next->sprint_start));
        const auto cutoff=stamp(first-2*86400);Data past=all;
        auto trim=[&](auto& r){r.erase(std::remove_if(r.begin(),r.end(),[&](auto& x){return x.season==season&&x.round>fold;}),r.end());};
        trim(past.results);trim(past.sprints);trim(past.qualifying);past.standings.clear();past.grids.clear();
        std::set<std::string> boundary,contested;
        double omitted_bonus=0;
        if(season<=2024)for(const auto& r:all.results)if(r.season==season&&r.round>fold&&r.points>0)
            omitted_bonus+=std::max(0.,r.points-points_for_position(r.position));
        for(unsigned repeat=0;repeat<2;++repeat) {
            const unsigned paired_seed=seed+season*100+fold+repeat*1000003;
            Config standing_config;standing_config.baseline="standings";
            const auto reference=forecast(past,season,fold,cutoff,sims,paired_seed,standing_config);
            if(repeat==0) {
                auto ordered=reference.championship;
                // Selection uses current points only; forecast mean rank breaks points ties.
                std::sort(ordered.begin(),ordered.end(),[](const auto& a,const auto& b){return a.current_points==b.current_points?mean_rank(a)<mean_rank(b):a.current_points>b.current_points;});
                for(size_t i=3;i<std::min<size_t>(8,ordered.size());++i) {
                    boundary.insert(ordered[i].id);
                    if(top5(ordered[i])>=.05&&top5(ordered[i])<=.95)contested.insert(ordered[i].id);
                }
            }
            for(const auto& variant:ablations()) {
                const auto f=variant.first=="standings"?reference:forecast(past,season,fold,cutoff,sims,paired_seed,variant.second);check(f);
                double brier=0,cbrier=0,rank_score=0,rank_error=0,point_error=0,pinball=0,race_top=0,race_rank=0;
                int n=0,nc=0,nr=0,unseen=0;std::set<std::string> known;
                for(const auto& d:f.championship) {
                    known.insert(d.id);if(!boundary.count(d.id)||!actual.count(d.id))continue;
                    const auto& truth=actual.at(d.id);double p=top5(d),y=truth.position<=5?1.:0.;
                    brier+=(p-y)*(p-y);rank_score+=rps(d.rank_probabilities,truth.position);
                    rank_error+=std::abs(mean_rank(d)-truth.position);point_error+=std::abs(d.mean_points-truth.points);++n;
                    if(contested.count(d.id)){cbrier+=(p-y)*(p-y);++nc;}
                    for(const auto& q:std::vector<std::pair<double,double>>{{.05,d.q05},{.5,d.q50},{.95,d.q95}}) {
                        const double error=truth.points-q.second;pinball+=(q.first-(error<0?1.:0.))*error/3.;
                    }
                    rows<<std::fixed<<std::setprecision(8)<<season<<','<<fold<<','<<variant.first<<','<<paired_seed<<','<<d.id<<','<<d.current_points<<','<<contested.count(d.id)<<','<<p<<','<<std::sqrt(p*(1-p)/sims)<<','<<y<<','<<mean_rank(d)<<','<<truth.position<<','<<d.mean_points<<','<<truth.points<<','<<d.q05<<','<<d.q50<<','<<d.q95<<'\n';
                }
                for(const auto& a:actual)if(!known.count(a.first))++unseen;
                std::map<std::string,int> race_truth;for(const auto& r:all.results)if(r.season==season&&r.round==fold+1)race_truth[r.driver_id]=r.position;
                for(const auto& race:f.races)if(race.round==fold+1&&race.session=="race")for(const auto& d:race.drivers)if(race_truth.count(d.id)) {
                    double y=race_truth[d.id]<=5?1.:0.;race_top+=(top5(d)-y)*(top5(d)-y);race_rank+=rps(d.rank_probabilities,race_truth[d.id]);++nr;
                }
                scores<<std::fixed<<std::setprecision(8)<<season<<','<<fold<<','<<cutoff<<','<<next->race_start<<','<<variant.first<<','<<paired_seed<<','<<sims<<','<<n<<','<<nc<<','<<brier/n<<',';
                if(nc)scores<<cbrier/nc;else scores<<"NA";
                scores<<','<<rank_score/n<<','<<rank_error/n<<','<<point_error/n<<','<<pinball/n<<','<<race_top/nr<<','<<race_rank/nr<<','<<unseen<<','<<omitted_bonus<<'\n';
                std::cout<<season<<" r"<<fold<<' '<<variant.first<<" seed "<<paired_seed<<" boundary Brier "<<brier/n<<" contested ";if(nc)std::cout<<cbrier/nc;else std::cout<<"NA";std::cout<<std::endl;
            }
        }
    }
}
void current_ablations(const Data& d,int season,int through,const std::string& cutoff,int sims,unsigned seed,const fs::path& path) {
    fs::create_directories(path.parent_path());std::ofstream out(path);
    out<<"variant,seed,simulations,driver,p_top5,mcse_top5,p_title,expected_rank,expected_points\n";
    for(unsigned repeat=0;repeat<2;++repeat)for(const auto& v:ablations()) {
        const auto paired_seed=seed+repeat*1000003;auto f=forecast(d,season,through,cutoff,sims,paired_seed,v.second);check(f);
        for(const auto& row:f.championship){double p=top5(row);out<<std::fixed<<std::setprecision(8)<<v.first<<','<<paired_seed<<','<<sims<<','<<row.id<<','<<p<<','<<std::sqrt(p*(1-p)/sims)<<','<<title(row)<<','<<mean_rank(row)<<','<<row.mean_points<<'\n';}
    }
}
// Three small, pre-weekend folds: fit past rounds only. No hyperparameter search.
void backtest(const Data& all,int sims,unsigned seed,const fs::path& path) {
    fs::create_directories(path.parent_path());std::ofstream out(path);
    out<<"season,through_round,model,simulations,next_race_winner_logloss,next_race_winner_brier,next_race_top5_brier,championship_top5_brier,championship_rank_rps,drivers_scored,unseen_future_drivers\n";
    for(int season:{2025})for(int fold:{8,14,18}) {
        const Event* next=nullptr;for(const auto& e:all.events)if(e.season==season&&e.round==fold+1)next=&e;
        if(!next)continue;
        std::time_t first=utc(next->race_start);
        if(!next->qualifying_start.empty())first=std::min(first,utc(next->qualifying_start));
        if(!next->sprint_start.empty())first=std::min(first,utc(next->sprint_start));
        std::string cutoff=stamp(first-2*86400);Data past=all;
        auto erase=[&](auto& rows){rows.erase(std::remove_if(rows.begin(),rows.end(),[&](auto& r){return r.season==season&&r.round>fold;}),rows.end());};
        erase(past.results);erase(past.sprints);erase(past.qualifying);past.standings.clear();past.grids.clear();
        std::map<std::string,int> actual_rank;for(const auto& s:all.standings)if(s.season==season)actual_rank[s.driver_id]=s.position;
        if(actual_rank.empty())throw std::runtime_error("Backtest needs final standings for "+std::to_string(season));
        for(const std::string mode:{"form","standings","uniform"}) {
            Config config;config.baseline=mode;auto f=forecast(past,season,fold,cutoff,sims,seed+season*100+fold,config);check(f);
            const RaceForecast* race=nullptr;for(const auto& e:f.races)if(e.round==fold+1&&e.session=="race")race=&e;
            if(!race)throw std::runtime_error("Backtest next race missing");
            std::map<std::string,int> observed;for(const auto& r:all.results)if(r.season==season&&r.round==fold+1)observed[r.driver_id]=r.position;
            double loss=0,brier=0,top=0,champ=0,rank=0;int nr=0,nc=0,unseen=0;bool winner_found=false;
            std::set<std::string> known;for(const auto& d:f.championship)known.insert(d.id);
            for(const auto& a:actual_rank)if(!known.count(a.first))++unseen;
            for(const auto& d:race->drivers)if(observed.count(d.id)) {
                double p=(title(d)*sims+.5)/(sims+.5*race->drivers.size()),y=observed[d.id]==1?1:0;
                if(y){loss=-std::log(p);winner_found=true;}brier+=(p-y)*(p-y);double z=observed[d.id]<=5?1:0;top+=(top5(d)-z)*(top5(d)-z);++nr;
            }
            for(const auto& d:f.championship)if(actual_rank.count(d.id)){double y=actual_rank[d.id]<=5?1:0;champ+=(top5(d)-y)*(top5(d)-y);rank+=rps(d.rank_probabilities,actual_rank[d.id]);++nc;}
            if(!winner_found||!nr||!nc)throw std::runtime_error("Backtest outcome coverage missing");
            out<<std::fixed<<std::setprecision(8)<<season<<','<<fold<<','<<mode<<','<<sims<<','<<loss<<','<<brier<<','<<top/nr<<','<<champ/nc<<','<<rank/nc<<','<<nc<<','<<unseen<<'\n';
            std::cout<<season<<" through "<<fold<<" "<<mode<<": winner logloss "<<loss<<", season top5 Brier "<<champ/nc<<'\n';
        }
    }
}
void sensitivity(const Data& d,int season,int through,const std::string& cutoff,int sims,unsigned seed,const fs::path& path) {
    fs::create_directories(path.parent_path());std::ofstream out(path);out<<"scenario,driver,p_top5,p_title,expected_points\n";
    std::vector<std::pair<std::string,Config>> scenarios;Config base;scenarios.push_back({"base",base});
    for(double x:{.75,1.25}){auto c=base;c.noise_scale=x;scenarios.push_back({"noise_"+std::to_string(x),c});}
    for(double x:{.5,1.5}){auto c=base;c.reliability_scale=x;scenarios.push_back({"retirement_"+std::to_string(x),c});}
    for(double x:{3.,10.}){auto c=base;c.half_life=x;scenarios.push_back({"half_life_"+std::to_string(x),c});}
    auto c=base;c.track_scale=0;scenarios.push_back({"no_track_adjustment",c});c=base;c.parameter_scale=0;scenarios.push_back({"no_parameter_uncertainty",c});
    for(const auto& scenario:scenarios){auto f=forecast(d,season,through,cutoff,sims,seed,scenario.second);check(f);for(const auto& x:f.championship)out<<std::setprecision(9)<<scenario.first<<','<<x.id<<','<<top5(x)<<','<<title(x)<<','<<x.mean_points<<'\n';}
    std::cout<<"Wrote "<<path<<" ("<<scenarios.size()<<" scenarios; same seed, paths may diverge)\n";
}
}
int main(int argc,char** argv) {
    try {
        std::string command=argc>1?argv[1]:"help",cutoff="2026-10-25T01:50:00Z";fs::path data="data",out="reports/current";int season=2026,sims=20000;unsigned seed=20261025;
        for(int i=2;i<argc;++i){std::string a=argv[i];if(i+1>=argc)throw std::runtime_error("Missing value for "+a);std::string v=argv[++i];if(a=="--data")data=v;else if(a=="--out")out=v;else if(a=="--season")season=std::stoi(v);else if(a=="--cutoff")cutoff=v;else if(a=="--sims")sims=std::stoi(v);else if(a=="--seed")seed=unsigned(std::stoul(v));else throw std::runtime_error("Unknown argument "+a);}
        if(command=="help"||command=="--help") {std::cout<<"make\n./f1predict forecast [--data data] [--season 2026] [--cutoff 2026-10-25T01:50:00Z] [--sims 20000] [--seed 20261025] [--out reports/current]\n./f1predict backtest --sims 2000 --out reports/backtest.csv\n./f1predict sensitivity --sims 5000 --out reports/sensitivity.csv\n./f1predict overnight --season 2025 --sims 5000 --out reports/overnight/2025\n./f1predict ablate --sims 20000 --out reports/overnight/current.csv\nInput refresh (network): ./scripts/fetch.sh\n";return 0;}
        if(sims<1||sims>1000000)throw std::runtime_error("--sims must be 1..1000000");
        auto d=load(data);
        if(command=="backtest"){if(out=="reports/current")out="reports/backtest.csv";backtest(d,sims,seed,out);}
        else if(command=="overnight") {if(out=="reports/current")out="reports/overnight/"+std::to_string(season);overnight(d,season,sims,seed,out);}
        else if(command=="forecast"||command=="sensitivity"||command=="ablate") {
            std::ifstream input_cutoff(data/"CUTOFF.txt");std::string recorded;std::getline(input_cutoff,recorded);
            if(recorded!=cutoff)throw std::runtime_error("Cutoff differs from data/CUTOFF.txt; regenerate inputs for the requested cutoff first");
            int through=audit(d,season,cutoff);
            if(command=="forecast")write(forecast(d,season,through,cutoff,sims,seed),out);
            else if(command=="ablate"){if(out=="reports/current")out="reports/overnight/current.csv";current_ablations(d,season,through,cutoff,sims,seed,out);}
            else{if(out=="reports/current")out="reports/sensitivity.csv";sensitivity(d,season,through,cutoff,sims,seed,out);}
        }
        else throw std::runtime_error("Unknown command: "+command);
        return 0;
    }catch(const std::exception& e){std::cerr<<"Error: "<<e.what()<<'\n';return 1;}
}
