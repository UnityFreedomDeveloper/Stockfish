// gcc -c -std=c++17 -fdeclspec Stockfish/benchmark.cpp Stockfish/Bitbase.cpp Stockfish/Bitboard.cpp Stockfish/endgame.cpp Stockfish/evaluate.cpp Stockfish/material.cpp Stockfish/misc.cpp Stockfish/movegen.cpp Stockfish/movepick.cpp Stockfish/pawn.cpp Stockfish/Position.cpp Stockfish/psqt.cpp Stockfish/search.cpp Stockfish/thread.cpp Stockfish/timem.cpp Stockfish/tt.cpp Stockfish/uci.cpp Stockfish/ucioption.cpp Stockfish/syzygy/tbprobe.cpp StockfishLib.cpp
// g++ -dynamiclib -fPIC -o StockfishLib.dylib benchmark.o Bitbase.o Bitboard.o endgame.o evaluate.o material.o misc.o movegen.o movepick.o pawn.o Position.o psqt.o search.o thread.o timem.o tt.o uci.o ucioption.o tbprobe.o StockfishLib.o

#pragma once
#include <string>

#ifdef STOCKFISHLIB_EXPORTS
#define STOCKFISHLIB_API __declspec(dllexport)
#else
#define STOCKFISHLIB_API __declspec(dllimport)
#endif

extern "C" STOCKFISHLIB_API void cpp_init_stockfish(double skill, double time);

extern "C" STOCKFISHLIB_API void cpp_init_custom_stockfish(double skill, double time, const char* customFEN);

extern "C" STOCKFISHLIB_API void cpp_set_position(std::string fen);

extern "C" STOCKFISHLIB_API void cpp_call_move(int from, int to);

extern "C" STOCKFISHLIB_API void cpp_castle_move(int castleSide);

extern "C" STOCKFISHLIB_API void cpp_enpassant_move(int from);

extern "C" STOCKFISHLIB_API void cpp_promotion_move(int from, int to);

extern "C" STOCKFISHLIB_API int cpp_search_move();

extern "C" STOCKFISHLIB_API void cpp_undo_move();

extern "C" STOCKFISHLIB_API void cpp_new_game();

extern "C" STOCKFISHLIB_API bool cpp_draw_check();

extern "C" STOCKFISHLIB_API void cpp_release_resource();