/*
  Stockfish, a UCI chess playing engine derived from Glaurung 2.1
  Copyright (C) 2004-2008 Tord Romstad (Glaurung author)
  Copyright (C) 2008-2015 Marco Costalba, Joona Kiiski, Tord Romstad
  Copyright (C) 2015-2019 Marco Costalba, Joona Kiiski, Gary Linscott, Tord Romstad

  Stockfish is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Stockfish is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <stdio.h>

#include <algorithm>
#include <cassert>
#include <ostream>

#include "misc.h"
#include "search.h"
#include "thread.h"
#include "tt.h"
#include "uci.h"
#include "syzygy/tbprobe.h"

using std::string;

UCI::OptionsMap Options; // Global object

namespace UCI {

/// 'On change' actions, triggered by an option's value change
void on_clear_hash(const Option&) { Search::clear(); }
void on_hash_size(const Option& o) { TT.resize(o); }
void on_logger(const Option& o) { start_logger(o); }
void on_threads(const Option& o) { Threads.set(o); }
void on_tb_path(const Option& o) { Tablebases::init(o); }


/// Our case insensitive less() function as required by UCI protocol
bool CaseInsensitiveLess::operator() (const string& s1, const string& s2) const {

  return std::lexicographical_compare(s1.begin(), s1.end(), s2.begin(), s2.end(),
         [](char c1, char c2) { return tolower(c1) < tolower(c2); });
}


/// init() initializes the UCI options to their hard-coded default values
// skill | think | elo
// 17 | 1000 | draw chess.com 2700 (w&b*1) [2500 elo]
// 18 | 1000 | 2700 lose (w*1) win (w&b*1)
// 19 | 1000 | still testing (lose chess.com 2900), win 2700 (w&b*1)
// 20 | 1000 | (won chess.com 2900) (w*1,b*2), won android app 2135 (w*1)

// 0 | 20 | lose android app 1435 (b&w * 1), won android app 1235, 1352 (w * 1), lose sunfish (d=3, b&w*1) [elo 1350]
// 2 | 20 | chess.com 1500 lose(w*1)
// 5 | 20 | won android app 1435 (w * 1), draw (b * 1), won sunfish (d=3, b&w*1) [elo 1500]
// 10 | 20, 5 | won android app 1435 (b&w * 1), won 1527 (b(2), w * 1), lose 1680 (w * 1), win (b*1), chess.com 1700 draw(w&b*1) [lose on thinking time 5, (b*1)] [1650 elo]
// 15 | 20 | lose android app 1835 (w&b*1), won (b*1), chess.com 2000 won(b*1), lose(w*1)  [1750 elo]
// 19 | 20 | won android app 1835 (w*1), lose (b*1), losing on 1915 (w*1) [1850 elo] not included
// 20 | 20 | won android app 1835 (w&b *1), 1680 (w *1), lose on 1915 (w * 1, b draw), 1985 (b&w * 1), win chess.com 2000 (w*1), win 2200 (w&b*1), lose 2300 (b*1) [1900 elo]
// 0,5,15 | 20 | lose android app 1680 (w * 1)

// 10 | 100 | lose android app 2135 (b*1)
// 15 | 100 | lose android app 2135 (b*1), won 1985 (b*1), won 2200 chess.com (b&w*1), lose 2300 (b*1) [2200 elo]
// 17 | 100 | lose chess.com 2500 (b*1), 2400 draw (b*1), 2300 draw(b*1) won(b*1)[2400 elo]
// 18 | 100 | won chess.com 2400(b*1), 2500 draw (b*1) won (b*1), 2600 lose (b*1) [2500 elo] not included
// 20 | 100 | won android app 1915 (w*1), 1985 (b&w*1), 2135 (b&w*1), won chess.com 2500 (b&w * 1), 2600 lose(b*1), 2700 won(w*1) draw(b*1) lose(b*1) [elo 2600]

// skill(think)
// w)0(20) vs b)2(20) 0-3 | b)0(2) vs w)2(20) 0-3
// w)20(20) vs b)15(100) 1-2 | b)20(20) vs w)15(100) 0-3
// w)20(20) vs b)18(100) 0-2 | b)20(20) vs w)18(100) 0-1
// w)17(100) vs b)15(100) 3-0
// w)15(100) vs b)17(100) 0-2
// w)17(100) vs b)20(100) 0-1
// w)20(20) vs b)17(100) 0-1 | w)17(100) vs b)20(20) 1-0
// w)15(1000) vs b)20(100) 2-0, draw*1 | w)20(100) vs b)15(1000) 3-0
// w)17(1000) vs b)20(100) 1-0 | w)20(100) vs b)17(1000) 1-2
// w)17(3000) vs b)20(1000) 0-1 | b)17(3000) vs w)20(1000) 1-0
// w)19(3000) vs b)20(3000) 0-1
// w)20(1000) vs b)20(3000) 0-1, draw*2 | b)20(1000) vs w)20(3000) draw*1


/**
 skill | time | elo
 0 | 20 | 1350
 5 | 20 | 1500
 10 | 20 | 1650
 15 | 20 | 1750
 20 | 20 1900
 17 | 100 | 2150
 20 | 100 | 2300
 17 | 1000 | 2500
 20 | 1000 | 2700
 20 | 3000 | ?
 
 */
void init(OptionsMap& o, double skill, double time) {

  // at most 2^32 clusters.
  constexpr int MaxHashMB = Is64Bit ? 131072 : 2048;

  o["Debug Log File"]        << Option("", on_logger);
  o["Contempt"]              << Option(24, -100, 100);
  o["Analysis Contempt"]     << Option("Both var Off var White var Black var Both", "Both");
  o["Threads"]               << Option(1, 1, 512, on_threads);
  o["Hash"]                  << Option(16, 1, MaxHashMB, on_hash_size);
  o["Clear Hash"]            << Option(on_clear_hash);
  o["Ponder"]                << Option(false);
  o["MultiPV"]               << Option(1, 1, 500);
  o["Skill Level"]           << Option(skill, 0, 20);
  o["Move Overhead"]         << Option(30, 0, 5000);
  o["Minimum Thinking Time"] << Option(time, 0, 5000);
  o["Slow Mover"]            << Option(84, 10, 1000);
  o["nodestime"]             << Option(0, 0, 10000);
  o["UCI_Chess960"]          << Option(false);
  o["UCI_AnalyseMode"]       << Option(false);
  o["SyzygyPath"]            << Option("<empty>", on_tb_path);
  o["SyzygyProbeDepth"]      << Option(1, 1, 100);
  o["Syzygy50MoveRule"]      << Option(true);
  o["SyzygyProbeLimit"]      << Option(7, 0, 7);
}

/// operator<<() is used to print all the options default values in chronological
/// insertion order (the idx field) and in the format defined by the UCI protocol.

std::ostream& operator<<(std::ostream& os, const OptionsMap& om) {

  for (size_t idx = 0; idx < om.size(); ++idx)
      for (const auto& it : om)
          if (it.second.idx == idx)
          {
              const Option& o = it.second;
              os << "\noption name " << it.first << " type " << o.type;

              if (o.type == "string" || o.type == "check" || o.type == "combo")
                  os << " default " << o.defaultValue;

              if (o.type == "spin")
                  os << " default " << int(stof(o.defaultValue))
                     << " min "     << o.min
                     << " max "     << o.max;

              break;
          }

  return os;
}


/// Option class constructors and conversion operators

Option::Option(const char* v, OnChange f) : type("string"), min(0), max(0), on_change(f)
{ defaultValue = currentValue = v; }

Option::Option(bool v, OnChange f) : type("check"), min(0), max(0), on_change(f)
{ defaultValue = currentValue = (v ? "true" : "false"); }

Option::Option(OnChange f) : type("button"), min(0), max(0), on_change(f)
{}

Option::Option(double v, int minv, int maxv, OnChange f) : type("spin"), min(minv), max(maxv), on_change(f)
{ defaultValue = currentValue = std::to_string(v); }

Option::Option(const char* v, const char* cur, OnChange f) : type("combo"), min(0), max(0), on_change(f)
{ defaultValue = v; currentValue = cur; }

Option::operator double() const {
  assert(type == "check" || type == "spin");
  return (type == "spin" ? stof(currentValue) : currentValue == "true");
}

Option::operator std::string() const {
  assert(type == "string");
  return currentValue;
}

bool Option::operator==(const char* s) const {
  assert(type == "combo");
  return    !CaseInsensitiveLess()(currentValue, s)
         && !CaseInsensitiveLess()(s, currentValue);
}


/// operator<<() inits options and assigns idx in the correct printing order

void Option::operator<<(const Option& o) {

  static size_t insert_order = 0;

  *this = o;
  idx = insert_order++;
}


/// operator=() updates currentValue and triggers on_change() action. It's up to
/// the GUI to check for option's limits, but we could receive the new value from
/// the user by console window, so let's check the bounds anyway.

Option& Option::operator=(const string& v) {

  assert(!type.empty());

  if (   (type != "button" && v.empty())
      || (type == "check" && v != "true" && v != "false")
      || (type == "spin" && (stof(v) < min || stof(v) > max)))
      return *this;

  if (type != "button")
      currentValue = v;

  if (on_change)
      on_change(*this);

  return *this;
}

} // namespace UCI
