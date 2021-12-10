//#include "pch.h"
#include <iostream>
#include "StockfishLib.h"
#include "Bitboard.h"
#include "Position.h"
#include "search.h"
#include "uci.h"
#include "thread.h"
#include "pawn.h"
#include "psqt.h"


Position _pos;
StateListPtr _states;
std::deque<Move> _moveHistory;

int main()
{
    return 0;
}

void cpp_init_stockfish(double skill, double time)
{
    UCI::init(Options, skill, time);
    PSQT::init();
    Bitboards::init();
    Position::init();
    Bitbases::init(); //unable to figure out
    Search::init(); //unable to figure out
    Pawns::init(); //unable to figure out
    Threads.set(Options["Threads"]);
    Search::clear(); // After threads are up

    UCI::init(_pos, _states);
    std::cout << "created stockfish instance" << "\n";
}
