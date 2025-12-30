#pragma once
#include <Arduino.h>
#include <vector>

// 棋子类型枚举
enum PieceType {
  NONE,
  PAWN,
  KNIGHT,
  BISHOP,
  ROOK,
  QUEEN,
  KING
};

// 颜色枚举
enum Color {
  WHITE,
  BLACK
};

// 游戏状态枚举
enum GameState {
  NormalPlay,
  PromotionSelecting
};

// 位置结构体
struct Position {
  int x; // 0-7 (a-h)
  int y; // 0-7 (1-8)
  
  Position() : x(-1), y(-1) {}
  Position(int x, int y) : x(x), y(y) {}
  
  bool isValid() const { return x >= 0 && x < 8 && y >= 0 && y < 8; }
  bool operator==(const Position& other) const { return x == other.x && y == other.y; }
  bool operator!=(const Position& other) const { return !(*this == other); }
};

// 棋子结构体
struct Piece {
  PieceType type;
  Color color;
  
  Piece() : type(NONE), color(Color::WHITE) {}
  Piece(PieceType type, Color color) : type(type), color(color) {}
  
  bool isEmpty() const { return type == NONE; }
};

// 走法结构体
struct Move {
  Position from;
  Position to;
  
  Move() : from(Position(-1, -1)), to(Position(-1, -1)) {}
  Move(Position from, Position to) : from(from), to(to) {}
  
  bool isValid() const { return from.isValid() && to.isValid(); }
  bool operator==(const Move& other) const { return from == other.from && to == other.to; }
  bool operator!=(const Move& other) const { return !(*this == other); }
};

// 前向声明


// 棋盘类
class ChessBoard {
private:
  Piece board[8][8];
  Color currentPlayer;
  Position selectedPiece;
  std::vector<Position> validMoves;
  bool whiteKingInCheck;
  bool blackKingInCheck;
  
  // 王车易位相关状态
  bool whiteKingMoved;
  bool blackKingMoved;
  bool whiteRookMoved[2]; // 0: a1, 1: h1
  bool blackRookMoved[2]; // 0: a8, 1: h8
  
  // 吃过路兵相关状态
  Position enPassantTarget;
  Position lastMoveFrom;
  Position lastMoveTo;
  
  // 用于撤销移动的状态
  Piece lastCapturedPiece;
  bool wasWhiteKingInCheck;
  bool wasBlackKingInCheck;
  bool wasWhiteKingMoved;
  bool wasBlackKingMoved;
  bool wasWhiteRookMoved[2];
  bool wasBlackRookMoved[2];
  Position wasEnPassantTarget;
  Color wasCurrentPlayer;
  
  // 游戏状态
  GameState currentState;
  
  // 升变相关状态
  Position promotionPawnPos;
  Color promotionColor;
  PieceType selectedPromotionPiece;
  
  // 检查位置是否在棋盘内
  bool isOnBoard(int x, int y) const;
  
  // 检查路径是否被阻挡（用于车、象、后）
  bool isPathClear(const Position& from, const Position& to) const;
  
  // 检查移动是否合法（不考虑将军）
  bool isMoveValid(const Position& from, const Position& to) const;
  
  // 检查是否会导致自己的王被将军
  bool wouldPutKingInCheck(const Position& from, const Position& to) const;
  
  // 生成所有合法移动
  void generateValidMoves(const Position& pos);
  
  // 检查王是否被将军
  bool isKingInCheck(Color color) const;
  
  // 检查是否可以吃掉对方的王（用于将军判断）
  bool canCaptureKing(Color attackerColor) const;
  
  // 模拟移动并检查是否会被将军
  bool simulateMoveAndCheckCheck(const Position& from, const Position& to, Color kingColor) const;
  

  
public:
  ChessBoard();
  
  // 初始化棋盘
  void initBoard();
  
  // 重置棋盘
  void resetBoard();
  
  // 获取棋子
  const Piece& getPiece(int x, int y) const;
  const Piece& getPiece(const Position& pos) const;
  
  // 设置棋子
  void setPiece(int x, int y, const Piece& piece);
  void setPiece(const Position& pos, const Piece& piece);
  
  // 移动棋子
  bool movePiece(const Position& from, const Position& to);
  
  // 选择棋子
  bool selectPiece(const Position& pos);
  
  // 取消选择
  void deselectPiece();
  
  // 获取当前玩家
  Color getCurrentPlayer() const;
  
  // 获取选中的棋子位置
  Position getSelectedPiece() const;
  
  // 获取合法移动列表
  const std::vector<Position>& getValidMoves() const;
  
  // 检查是否有合法移动
  bool hasValidMoves() const;
  
  // 检查是否被将死
  bool isCheckmate(Color color) const;
  
  // 检查是否被将军
  bool isInCheck(Color color) const;
  
  // 切换玩家
  void switchPlayer();
  
  // 生成FEN字符串
  String toFEN() const;
  
  // 从FEN加载棋盘
  bool fromFEN(const String& fen);
  
  // 生成PGN记谱法
  String toPGN(const Position& from, const Position& to, const Piece& piece, const Piece& targetPiece) const;
  
  // 位置转PGN字符串
  String positionToPGN(const Position& pos) const;
  
  // 从PGN字符串解析移动
  Move parsePGN(const String& pgn) const;
  
  // 获取当前游戏状态
  GameState getCurrentState() const;
  
  // 进入升变状态
  void enterPromotionState(const Position& pos, Color color);
  
  // 确认升变
  void confirmPromotion();
  
  // 导航升变选择
  void navigatePromotionSelection(int direction);
  
  // 获取升变相关状态
  Position getPromotionPawnPos() const;
  Color getPromotionColor() const;
  PieceType getSelectedPromotionPiece() const;
  
  // 验证移动是否合法（公共方法，用于测试）
  bool validateMove(const Position& from, const Position& to) const;
  
  // 撤销上一步移动
  void undoMove();
};

// 全局棋盘实例
extern ChessBoard chessBoard;

// Serial输出包装函数，避免Serial Monitor未连接时阻塞
void serialPrintln(const String& str);
void serialPrintln(const char* str);
void serialPrintf(const char* format, ...);
void serialPrint(const String& str);
void serialPrint(const char* str);
