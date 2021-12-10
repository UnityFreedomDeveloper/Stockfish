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

#ifndef uci_h
#define uci_h

#include <map>
#include <string>

#include "types.h"
#include "Position.h"

class Position;

namespace UCI {

class Option;

/// Custom comparator because UCI options should be case insensitive
struct CaseInsensitiveLess {
  bool operator() (const std::string&, const std::string&) const;
};

/// Our options container is actually a std::map
typedef std::map<std::string, Option, CaseInsensitiveLess> OptionsMap;

/// Option class implements an option as defined by UCI protocol
class Option {

  typedef void (*OnChange)(const Option&);

public:
  Option(OnChange = nullptr);
  Option(bool v, OnChange = nullptr);
  Option(const char* v, OnChange = nullptr);
  Option(double v, int minv, int maxv, OnChange = nullptr);
  Option(const char* v, const char* cur, OnChange = nullptr);

  Option& operator=(const std::string&);
  void operator<<(const Option&);
  operator double() const;
  operator std::string() const;
  bool operator==(const char*) const;

private:
  friend std::ostream& operator<<(std::ostream&, const OptionsMap&);

  std::string defaultValue, currentValue, type;
  int min, max;
  size_t idx;
  OnChange on_change;
};

void init(OptionsMap&, double skill, double time);
void init(Position &pos, StateListPtr &states);
void init(Position &pos, StateListPtr &states, const char* customFEN);

Move think(Position& pos, std::deque<Move> &moveHistory);
void undo_move(Position& pos, std::deque<Move> &moveHistory);
void new_game(Position &pos, StateListPtr &states, std::deque<Move> &moveHistory);
void set_position(Position& pos, StateListPtr& states, std::string fen);
void pos_possible_moves(const Position &pos, int &size, int *mPointer);
void release_resources(Position &pos);
std::string value(Value v);
std::string square(Square s);
std::string move(Move m, bool chess960);
std::string pv(const Position& pos, Depth depth, Value alpha, Value beta);
Move to_move(const Position& pos, std::string& str);
bool is_game_draw(Position &pos);

//call from swift
void init_move(int from, int to, Position &pos, std::deque<Move> &moveHistory);
void castle_move(int castleSide, Position &pos, std::deque<Move> &moveHistory);
void enpassant_move(int from, Position &pos, std::deque<Move> &moveHistory);
void promotion_move(int from, int to, Position &pos, std::deque<Move> &moveHistory);

//Testing purpose
int fivety_move_rule_count(Position &pos);
bool all_possible_moves_match(Position &pos, const int* from, const int* to, const int count);
bool all_possible_moves_match_inverse(Position &pos, const int* from, const int* to, const int count);
} // namespace UCI

extern UCI::OptionsMap Options;


#endif /* uci_h */
