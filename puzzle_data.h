#ifndef PUZZLE_DATA_H
#define PUZZLE_DATA_H

#include "common.h"
#include <vector>
#include <string>

// 谜题数据结构体
struct PuzzleData {
  String fen;
  Color sideToMove;
  std::vector<Move> mainLine;
  
  // 构造函数声明
  PuzzleData(const String& fen, const String& pgnMoves);
  PuzzleData(const String& fen, Color sideToMove, const String& pgnMoves);
  PuzzleData(const String& fen, Color sideToMove, const std::vector<Move>& mainLine);
  
  // 辅助函数声明
  static Color getSideToMoveFromFEN(const String& fen);
};

// 所有谜题数据
const std::vector<PuzzleData> PUZZLES_DATA = {
  // 谜题1：经典转向（黑先）- 使用PGN格式
  PuzzleData("3r2k1/p4ppp/1q6/8/8/2R1P3/P3QPPP/6K1 b - - 0 1", 
             "Qb2 Rc8 Qb1+ Qf1 Qxf1+ Kxf1 Rxc8"),
  
  // 谜题2：进入兵残局（白先）- 使用PGN格式
  PuzzleData("8/1p2kp1p/p3pn2/2r5/8/P1N5/1PP3PP/5RK1 w - - 0 1", 
             "c4 bxc4 c3 a5 a4 Kd5 g5 hxg5 hxg5 e5 g6 Ke6 g7 Kf7 Ke4 Kxg7 Kxe5 Kf7 Kd5"),
  
  // 谜题3：占线和刺入（白先）- 使用PGN格式
  PuzzleData("1n1q1rk1/1Nb2ppb1/pp4p1/3p4/3Pn3/BP1BPN2/P3QPPP/2R3K1 w - - 0 1", 
             "Qc2 Qd7 Qc7 Ba8 Nc8 Bf6 Qxb8 Bc6 Bxa6")
};

#endif // PUZZLE_DATA_H