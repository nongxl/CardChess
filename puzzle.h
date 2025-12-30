#pragma once
#include "common.h"
#include <vector>
#include <String.h>

// 谜题结构体
class Puzzle {
private:
  String fen;              // 初始局面FEN
  Color sideToMove;        // 轮到哪一方走棋
  std::vector<Move> mainLine; // 正解走法序列
  
public:
  Puzzle() : sideToMove(Color::WHITE) {}
  Puzzle(const String& fen, Color sideToMove, const std::vector<Move>& mainLine)
    : fen(fen), sideToMove(sideToMove), mainLine(mainLine) {}
  
  // 获取FEN字符串
  String getFEN() const { return fen; }
  
  // 获取走棋方
  Color getSideToMove() const { return sideToMove; }
  
  // 获取正解走法序列
  const std::vector<Move>& getMainLine() const { return mainLine; }
  
  // 从文件加载谜题
  static std::vector<Puzzle> loadPuzzles(const String& filePath);
  
  // 复制赋值运算符
  Puzzle& operator=(const Puzzle& other) {
    if (this != &other) {
      fen = other.fen;
      sideToMove = other.sideToMove;
      mainLine = other.mainLine;
    }
    return *this;
  }
};


