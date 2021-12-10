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
#include <cassert>
#include <iostream>
#include <sstream>
#include <string>
#include <iterator>

#include "evaluate.h"
#include "movegen.h"
#include "Position.h"
#include "search.h"
#include "thread.h"
#include "timem.h"
#include "tt.h"
#include "uci.h"
#include "syzygy/tbprobe.h"

using namespace std;

extern vector<string> setup_bench(const Position&, istream&);

namespace {

  // FEN string of the initial position, normal chess
  const char* StartFEN = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

}


/// UCI::value() converts a Value to a string suitable for use with the UCI
/// protocol specification:
///
/// cp <x>    The score from the engine's point of view in centipawns.
/// mate <y>  Mate in y moves, not plies. If the engine is getting mated
///           use negative values for y.

string UCI::value(Value v) {

  assert(-VALUE_INFINITE < v && v < VALUE_INFINITE);

  stringstream ss;

  if (abs(v) < VALUE_MATE - MAX_PLY)
      ss << "cp " << v * 100 / PawnValueEg;
  else
      ss << "mate " << (v > 0 ? VALUE_MATE - v + 1 : -VALUE_MATE - v) / 2;

  return ss.str();
}


/// UCI::square() converts a Square to a string in algebraic notation (g1, a7, etc.)

std::string UCI::square(Square s) {
  return std::string{ char('a' + file_of(s)), char('1' + rank_of(s)) };
}


/// UCI::move() converts a Move to a string in coordinate notation (g1f3, a7a8q).
/// The only special case is castling, where we print in the e1g1 notation in
/// normal chess mode, and in e1h1 notation in chess960 mode. Internally all
/// castling moves are always encoded as 'king captures rook'.

string UCI::move(Move m, bool chess960) {

  Square from = from_sq(m);
  Square to = to_sq(m);

  if (m == MOVE_NONE)
      return "(none)";

  if (m == MOVE_NULL)
      return "0000";

  if (type_of(m) == CASTLING && !chess960)
      to = make_square(to > from ? FILE_G : FILE_C, rank_of(from));

  string move = UCI::square(from) + UCI::square(to);

  if (type_of(m) == PROMOTION)
      move += " pnbrqk"[promotion_type(m)];

  return move;
}


/// UCI::to_move() converts a string representing a move in coordinate notation
/// (g1f3, a7a8q) to the corresponding legal Move, if any.

Move UCI::to_move(const Position& pos, string& str) {

  if (str.length() == 5) // Junior could send promotion piece in uppercase
      str[4] = char(tolower(str[4]));

  for (const auto& m : MoveList<LEGAL>(pos))
      if (str == UCI::move(m, pos.is_chess960()))
          return m;

  return MOVE_NONE;
}

void after_move(Position& pos, Move m, std::deque<Move> &moveHistory) {
    Threads.do_move(m, pos);
    moveHistory.push_back(m);
    cout << pos << endl;
}

void UCI::init_move(int from, int to, Position &pos, std::deque<Move> &moveHistory) {
    StateInfo *tmp = new StateInfo;
    Move m = make_move(Square(from), Square(to));
    
    pos.do_move(m, *tmp);
    after_move(pos, m, moveHistory);
}

void UCI::castle_move(int castleSide, Position &pos, std::deque<Move> &moveHistory) {
    Color colors = pos.side_to_move();
    CastlingSide cs = CastlingSide(castleSide);
    const Position *const_pos = const_cast<const Position*>(&pos);
    Move castle = make_castling_move(colors, cs, *const_pos);
    
    StateInfo *tmp = new StateInfo;
    pos.do_move(castle, *tmp);
    after_move(pos, castle, moveHistory);
}

void UCI::enpassant_move(int from, Position &pos, std::deque<Move> &moveHistory) {
    const Position *const_pos = const_cast<const Position*>(&pos);
    Move ep = make_enpassant_move(Square(from), *const_pos);
    
    StateInfo *tmp = new StateInfo;
    pos.do_move(ep, *tmp);
    after_move(pos, ep, moveHistory);
}

void UCI::promotion_move(int from, int to, Position &pos, std::deque<Move> &moveHistory) {
    Move promo = make_promotion_move(Square(from), Square(to));
    
    StateInfo *tmp = new StateInfo;
    pos.do_move(promo, *tmp);
    after_move(pos, promo, moveHistory);
}

//only for think
void threads_setup(Position &pos, StateListPtr &states) {
    Threads.setup(pos, states);
}

//stronger AI than search_best
Move UCI::think(Position& pos, deque<Move> &moveHistory) {

  Search::LimitsType limits;
  string token;
  bool ponderMode = false;
  limits.startTime = now();
    
  std::future<Move> ftr = Threads.main()->pMove.get_future();
  Threads.think(pos, limits, ponderMode);
  Move m = ftr.get();

  Threads.main()->pMove = std::promise<Move>();
    
  if (m != MOVE_NONE) {
    if (type_of(m) == CASTLING)
    {
        Square from = from_sq(m);
        Square to = to_sq(m);
        to = make_square(to > from ? FILE_G : FILE_C, rank_of(from));
        
        Move cm = make_move(from, to);
        return cm;
    }
    
  }
  return m;
}

void UCI::init(Position &pos, StateListPtr &states) {
    StateListPtr s(new std::deque<StateInfo>(1));
    states = move(s);
    auto uiThread = std::make_shared<Thread>(0);

    pos.set(StartFEN, Options["UCI_Chess960"], &states->back(), uiThread.get());
    threads_setup(pos, states);
    cout << pos << endl;
}

void UCI::init(Position &pos, StateListPtr &states, const char* customFEN) {
    StateListPtr s(new std::deque<StateInfo>(1));
    states = move(s);
    auto uiThread = std::make_shared<Thread>(0);

    pos.set(customFEN, Options["UCI_Chess960"], &states->back(), uiThread.get());
    threads_setup(pos, states);
    cout << pos << endl;
}

void UCI::undo_move(Position& pos, deque<Move> &moveHistory) {
    Move m = moveHistory.back();
    Threads.undo_move(m);
    pos.undo_move(m, RELEASE);
    moveHistory.pop_back();
    
    cout << pos << endl;
}

void UCI::new_game(Position &pos, StateListPtr &states, std::deque<Move> &moveHistory) {
    Search::clear();
    pos.release();
    StateListPtr s(new std::deque<StateInfo>(1));
    states = move(s);
    auto uiThread = std::make_shared<Thread>(0);

    pos.set(StartFEN, Options["UCI_Chess960"], &states->back(), uiThread.get());
    moveHistory.clear();
    
    threads_setup(pos, states);
}

void UCI::release_resources(Position &pos) {
    pos.release();
    Threads.set(0);
}

bool UCI::is_game_draw(Position &pos) { 
    bool repeat = pos.three_fold_repetition();
    int fivetyMove = pos.rule50_count();
    
    if (!repeat && fivetyMove < 50) {
        return false;
    } else {
        return true;
    }
}

int UCI::fivety_move_rule_count(Position &pos) {
    return pos.sInfo()->rule50;
}

bool UCI::all_possible_moves_match(Position &pos, const int* from, const int* to, const int count) {
    int mCount = 0;
    int move50 = pos.sInfo()->rule50;
    if (move50 >= 100) {
        return true;
    }
    
    for (auto& m : MoveList<LEGAL>(pos)) {
        mCount++;
        bool match = false;
        int const *movesFrom = from;
        int const *movesTo = to;
        Square f = from_sq(m);
        Square t = to_sq(m);
        if (type_of(m) == CASTLING)
        {
            t = make_square(t > f ? FILE_G : FILE_C, rank_of(f));
        }
        
        for (int i = 0; i < count; i++) {
            cout << "mcount " << mCount << " count " << count << endl;
            cout << "moves from " << *movesFrom << " f " << f << endl;
            cout << "moves to" << *movesTo << " t " << t << endl;
            if (*movesFrom == f && *movesTo == t) {
                cout << "matched" << endl;
                match = true;
                break;
            }
            
            movesFrom++;
            movesTo++;
        }
        
        if (match == false)
            return false;
    }
    
    if (mCount != count) {
        return false;
    }
    assert(mCount == count);
    return true;
}

bool UCI::all_possible_moves_match_inverse(Position &pos, const int* from, const int* to, const int count) {
    int move50 = pos.sInfo()->rule50;
    if (move50 >= 100) {
        return true;
    }

    int const *movesFrom = from;
    int const *movesTo = to;
    vector<int> mFrom;
    vector<int> mTo;
    for (auto& m : MoveList<LEGAL>(pos)) {
        Square f = from_sq(m);
        Square t = to_sq(m);
        if (type_of(m) == CASTLING)
        {
            t = make_square(t > f ? FILE_G : FILE_C, rank_of(f));
        }
        
        mFrom.push_back(f);
        mTo.push_back(t);
    }
    for (int i = 0; i < count; i++) {
        bool match = false;
        
        for (int j = 0; j < mFrom.size(); j++) {
            int tempMFrom = mFrom[j];
            int tempMTo = mTo[j];
            cout << "mFrom.size() " << mFrom.size() << " count" << count << endl;
            cout << " f " << tempMFrom << "moves from " << *movesFrom << endl;
            cout << " t " << tempMTo << "moves to " << *movesTo << endl;
            if (*movesFrom == tempMFrom && *movesTo == tempMTo) {
                cout << "matched" << endl;
                match = true;
                break;
            }
        }

        movesFrom++;
        movesTo++;
        if (match == false)
            return false;
    }

    if (mFrom.size() != count) {
        return false;
    }
    assert(mFrom.size() == count);
    return true;
}

void UCI::pos_possible_moves(const Position &pos, int &size, int *mPointer) {
    int count = 0;
    for (auto& m : MoveList<LEGAL>(pos)) {
        count++;
        *mPointer = m;
        
        mPointer++;
    }
    
    for (int i = 0; i < count; i++) {
        mPointer--;
    }
    size = count;
}

void UCI::set_position(Position& pos, StateListPtr& states, string fen) {
  StateListPtr s(new std::deque<StateInfo>(1));
  states = move(s);
  pos.set(fen, Options["UCI_Chess960"], &states->back(), Threads.main());
  threads_setup(pos, states);
    
    cout << pos << endl;
}
