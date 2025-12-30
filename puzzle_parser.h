#ifndef PUZZLE_PARSER_H
#define PUZZLE_PARSER_H

#include "common.h"
#include "puzzle_data.h"
#include <vector>
#include <string>

class PuzzleParser {
public:
  // 解析PGN移动序列并生成Move列表
  static std::vector<Move> parsePGNMoves(const String& fen, const String& pgnMoves, Color startingColor);
  
  // 从FEN和PGN创建PuzzleData
  static PuzzleData createPuzzleData(const String& fen, Color sideToMove, const String& pgnMoves);
  
private:
  // 分割PGN移动序列为单独的移动
  static std::vector<String> splitPGNMoves(const String& pgnMoves);
};

#endif // PUZZLE_PARSER_H