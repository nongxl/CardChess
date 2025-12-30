#include "puzzle_parser.h"

// 解析PGN移动序列并生成Move列表
std::vector<Move> PuzzleParser::parsePGNMoves(const String& fen, const String& pgnMoves, Color startingColor) {
  std::vector<Move> moves;
  
  // 创建临时棋盘
  ChessBoard board;
  board.fromFEN(fen);
  
  // 设置起始玩家
  if (board.getCurrentPlayer() != startingColor) {
    board.switchPlayer();
  }
  
  // 分割PGN移动序列
  std::vector<String> pgnMoveList = splitPGNMoves(pgnMoves);
  
  // 解析每个移动
  for (const String& pgnMove : pgnMoveList) {
    if (pgnMove.isEmpty()) continue;
    
    // 解析PGN移动
    Move move = board.parsePGN(pgnMove);
    
    // 检查移动是否有效
    if (move.from.isValid() && move.to.isValid()) {
      moves.push_back(move);
      
      // 执行移动，以便下一个移动可以基于当前位置解析
      board.movePiece(move.from, move.to);
    }
  }
  
  return moves;
}

// 从FEN和PGN创建PuzzleData
PuzzleData PuzzleParser::createPuzzleData(const String& fen, Color sideToMove, const String& pgnMoves) {
  // 直接使用PGN构造函数
  return PuzzleData(fen, sideToMove, pgnMoves);
}

// 分割PGN移动序列为单独的移动
std::vector<String> PuzzleParser::splitPGNMoves(const String& pgnMoves) {
  std::vector<String> moves;
  String currentMove;
  
  // 移除可能的回合数（如 "1. e4 e5 2. Nf3 Nc6" 中的 "1." 和 "2."）
  for (int i = 0; i < pgnMoves.length(); i++) {
    char c = pgnMoves[i];
    
    // 跳过回合数
    if (c >= '0' && c <= '9' && (i == 0 || pgnMoves[i-1] == ' ')) {
      while (i < pgnMoves.length() && pgnMoves[i] != ' ') {
        i++;
      }
      continue;
    }
    
    // 遇到空格或换行符时，保存当前移动
    if (c == ' ' || c == '\n' || c == '\t') {
      if (!currentMove.isEmpty()) {
        moves.push_back(currentMove);
        currentMove = "";
      }
    } else {
      currentMove += c;
    }
  }
  
  // 保存最后一个移动
  if (!currentMove.isEmpty()) {
    moves.push_back(currentMove);
  }
  
  return moves;
}