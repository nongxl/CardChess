#include "puzzle.h"
#include "puzzle_data.h"
#include "common.h"

// PuzzleData 辅助函数：从FEN字符串中提取当前玩家
Color PuzzleData::getSideToMoveFromFEN(const String& fen) {
  int index = 0;
  // 跳过棋盘布局部分
  while (index < fen.length() && fen[index] != ' ') {
    index++;
  }
  // 跳过空格
  while (index < fen.length() && fen[index] == ' ') {
    index++;
  }
  // 解析当前玩家
  return (index < fen.length() && fen[index] == 'w') ? WHITE : BLACK;
}

// PuzzleData 构造函数：使用PGN字符串（自动从FEN中提取当前玩家）
PuzzleData::PuzzleData(const String& fen, const String& pgnMoves) {
  this->fen = fen;
  this->sideToMove = getSideToMoveFromFEN(fen);
  
  // 创建临时棋盘来解析PGN移动
  ChessBoard tempBoard;
  tempBoard.fromFEN(fen);
  
  // 解析PGN移动
  String cleanPGN = pgnMoves;
  cleanPGN.trim();
  
  int currentIndex = 0;
  while (currentIndex < cleanPGN.length()) {
    // 跳过空格
    while (currentIndex < cleanPGN.length() && (cleanPGN[currentIndex] == ' ' || cleanPGN[currentIndex] == '\t' || cleanPGN[currentIndex] == '\n')) {
      currentIndex++;
    }
    
    if (currentIndex >= cleanPGN.length()) break;
    
    // 解析单个移动
    int moveStart = currentIndex;
    while (currentIndex < cleanPGN.length() && cleanPGN[currentIndex] != ' ' && cleanPGN[currentIndex] != '\t' && cleanPGN[currentIndex] != '\n') {
      currentIndex++;
    }
    
    String pgnMove = cleanPGN.substring(moveStart, currentIndex);
    Move move = tempBoard.parsePGN(pgnMove);
    
    if (move.from.isValid() && move.to.isValid()) {
      mainLine.push_back(move);
      // 执行移动以便下一个移动可以基于当前位置解析
      tempBoard.movePiece(move.from, move.to);
    }
  }
}

// PuzzleData 构造函数：使用PGN字符串（兼容旧版，允许手动指定当前玩家）
PuzzleData::PuzzleData(const String& fen, Color sideToMove, const String& pgnMoves) {
  this->fen = fen;
  this->sideToMove = sideToMove;
  
  // 创建临时棋盘来解析PGN移动
  ChessBoard tempBoard;
  tempBoard.fromFEN(fen);
  
  // 设置当前玩家
  if (tempBoard.getCurrentPlayer() != sideToMove) {
    tempBoard.switchPlayer();
  }
  
  // 解析PGN移动
  String cleanPGN = pgnMoves;
  cleanPGN.trim();
  
  int currentIndex = 0;
  while (currentIndex < cleanPGN.length()) {
    // 跳过空格
    while (currentIndex < cleanPGN.length() && (cleanPGN[currentIndex] == ' ' || cleanPGN[currentIndex] == '\t' || cleanPGN[currentIndex] == '\n')) {
      currentIndex++;
    }
    
    if (currentIndex >= cleanPGN.length()) break;
    
    // 解析单个移动
    int moveStart = currentIndex;
    while (currentIndex < cleanPGN.length() && cleanPGN[currentIndex] != ' ' && cleanPGN[currentIndex] != '\t' && cleanPGN[currentIndex] != '\n') {
      currentIndex++;
    }
    
    String pgnMove = cleanPGN.substring(moveStart, currentIndex);
    Move move = tempBoard.parsePGN(pgnMove);
    
    if (move.from.isValid() && move.to.isValid()) {
      mainLine.push_back(move);
      // 执行移动以便下一个移动可以基于当前位置解析
      tempBoard.movePiece(move.from, move.to);
    }
  }
}

// PuzzleData 构造函数：使用预定义的Move列表
PuzzleData::PuzzleData(const String& fen, Color sideToMove, const std::vector<Move>& mainLine) {
  this->fen = fen;
  this->sideToMove = sideToMove;
  this->mainLine = mainLine;
}

// 从谜题数据加载谜题
std::vector<Puzzle> Puzzle::loadPuzzles(const String& filePath) {
  std::vector<Puzzle> puzzles;
  
  // 从puzzle_data.h加载所有谜题
  for (const auto& puzzleData : PUZZLES_DATA) {
    Puzzle puzzle(puzzleData.fen, puzzleData.sideToMove, puzzleData.mainLine);
    puzzles.push_back(puzzle);
  }
  
  return puzzles;
}
