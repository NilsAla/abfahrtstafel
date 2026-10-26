#include "departure_board.h"

#include <string.h>

#include <algorithm>
#include <map>

namespace abf {

namespace {

void replaceAll(std::string &s, const std::string &from, const std::string &to) {
  for (size_t pos = s.find(from); pos != std::string::npos; pos = s.find(from, pos + to.size())) {
    s.replace(pos, from.size(), to);
  }
}

bool endsWith(const std::string &s, const std::string &suffix) {
  return s.size() >= suffix.size() && s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
}

std::string collapseSpaces(const std::string &s) {
  std::string out;
  for (char c : s) {
    if (c == ' ' && (out.empty() || out.back() == ' ')) continue;
    out.push_back(c);
  }
  while (!out.empty() && (out.back() == ' ' || out.back() == ',')) out.pop_back();
  return out;
}

}  // namespace

std::string shortenHeadsign(const std::string &headsign) {
  std::string s;
  int depth = 0;
  for (char c : headsign) {
    if (c == '(') {
      ++depth;
      s.push_back(' ');
    } else if (c == ')') {
      if (depth > 0) --depth;
    } else if (depth == 0) {
      s.push_back(c);
    }
  }
  replaceAll(s, "Hauptbahnhof", "Hbf");
  s = collapseSpaces(s);
  replaceAll(s, " ,", ",");
  replaceAll(s, ", Hbf", " Hbf");
  for (const char *suffix : {", Bahnhof", " Bahnhof", ", Bf", " Bf"}) {
    if (endsWith(s, suffix) && s.size() > strlen(suffix)) {
      s.erase(s.size() - strlen(suffix));
      break;
    }
  }
  s = collapseSpaces(s);
  return s.empty() ? headsign : s;
}

bool matchesSelection(const Departure &d, const StopConfig &cfg) {
  if (cfg.filter == StopFilter::All) return (cfg.modes & d.mode) != 0;
  for (const RouteDirection &r : cfg.select) {
    if (r.routeId == d.routeId && r.direction == d.direction) return true;
  }
  return false;
}

std::vector<BoardRow> prepareDepartures(const std::vector<Departure> &in, const StopConfig &cfg, int64_t now,
                                        size_t maxRows) {
  const int64_t reachable = now + static_cast<int64_t>(cfg.walkMinutes) * 60;
  std::vector<BoardRow> rows;
  for (const Departure &d : in) {
    if (!matchesSelection(d, cfg)) continue;
    BoardRow r;
    r.cancelled = d.cancelled;
    r.when = d.cancelled ? d.scheduled : d.expected;
    if (r.when < reachable) continue;
    r.line = d.line;
    r.to = shortenHeadsign(d.headsign);
    r.track = d.track;
    r.mode = d.mode;
    r.color = d.color;
    r.textColor = d.textColor;
    r.scheduled = d.scheduled;
    r.realtime = d.realtime;
    r.delayMin = d.realtime && !d.cancelled ? static_cast<int32_t>((d.expected - d.scheduled) / 60) : 0;
    r.minutes = static_cast<int32_t>((r.when - now) / 60);

    // Dublette aus zweitem Datenfeed: gleiche Linie, gleiches Ziel, gleiche Planzeit
    auto dup = std::find_if(rows.begin(), rows.end(), [&](const BoardRow &o) {
      return o.line == r.line && o.to == r.to && o.scheduled == r.scheduled;
    });
    if (dup != rows.end()) {
      const bool better = (r.realtime && !dup->realtime) || (dup->track.empty() && !r.track.empty());
      if (better) *dup = r;
      continue;
    }
    rows.push_back(std::move(r));
  }
  std::stable_sort(rows.begin(), rows.end(), [](const BoardRow &a, const BoardRow &b) {
    if (a.when != b.when) return a.when < b.when;
    return a.line < b.line;
  });
  if (rows.size() > maxRows) rows.resize(maxRows);
  return rows;
}

std::vector<DirectionGroup> groupDirections(const std::vector<Departure> &in) {
  struct Acc {
    DirectionOption opt;
    std::map<std::string, uint32_t> targets;
  };
  std::map<std::pair<std::string, uint8_t>, Acc> byRoute;
  for (const Departure &d : in) {
    Acc &a = byRoute[{d.routeId, d.direction}];
    a.opt.routeId = d.routeId;
    a.opt.direction = d.direction;
    a.opt.line = d.line;
    a.opt.mode = d.mode;
    ++a.opt.count;
    ++a.targets[shortenHeadsign(d.headsign)];
  }

  std::map<std::string, DirectionGroup> byTarget;
  for (auto &entry : byRoute) {
    Acc &a = entry.second;
    auto best = std::max_element(a.targets.begin(), a.targets.end(),
                                 [](const auto &x, const auto &y) { return x.second < y.second; });
    a.opt.to = best->first;
    DirectionGroup &g = byTarget[a.opt.to];
    g.to = a.opt.to;
    g.count += a.opt.count;
    g.options.push_back(a.opt);
  }

  std::vector<DirectionGroup> groups;
  for (auto &entry : byTarget) {
    DirectionGroup &g = entry.second;
    std::sort(g.options.begin(), g.options.end(), [](const DirectionOption &a, const DirectionOption &b) {
      if (a.mode != b.mode) return a.mode < b.mode;  // Bahn vor Bus
      return a.line < b.line;
    });
    groups.push_back(std::move(g));
  }
  std::stable_sort(groups.begin(), groups.end(), [](const DirectionGroup &a, const DirectionGroup &b) {
    const uint8_t ma = a.options.front().mode;
    const uint8_t mb = b.options.front().mode;
    if (ma != mb) return ma < mb;
    return a.count > b.count;
  });
  return groups;
}

}  // namespace abf
