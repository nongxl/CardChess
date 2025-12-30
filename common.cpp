#include "common.h"

// 全局开关：控制是否启用串口输出
#define ENABLE_SERIAL_OUTPUT true

// Serial输出包装函数，避免Serial Monitor未连接时阻塞
void serialPrintln(const String& str) {
  if (ENABLE_SERIAL_OUTPUT) {
    Serial.println(str);
  }
}

void serialPrintln(const char* str) {
  if (ENABLE_SERIAL_OUTPUT) {
    Serial.println(str);
  }
}

void serialPrintf(const char* format, ...) {
  if (ENABLE_SERIAL_OUTPUT) {
    char buffer[256];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    Serial.print(buffer);
  }
}

void serialPrint(const String& str) {
  if (ENABLE_SERIAL_OUTPUT) {
    Serial.print(str);
  }
}

void serialPrint(const char* str) {
  if (ENABLE_SERIAL_OUTPUT) {
    Serial.print(str);
  }
}

// 全局棋盘实例
ChessBoard chessBoard;

ChessBoard::ChessBoard() {
  initBoard();
}

void ChessBoard::initBoard() {
  // 清空棋盘
  for (int y = 0; y < 8; y++) {
    for (int x = 0; x < 8; x++) {
      board[x][y] = Piece(NONE, WHITE);
    }
  }
  
  // 设置白方棋子
  board[0][0] = Piece(ROOK, WHITE);
  board[1][0] = Piece(KNIGHT, WHITE);
  board[2][0] = Piece(BISHOP, WHITE);
  board[3][0] = Piece(QUEEN, WHITE);
  board[4][0] = Piece(KING, WHITE);
  board[5][0] = Piece(BISHOP, WHITE);
  board[6][0] = Piece(KNIGHT, WHITE);
  board[7][0] = Piece(ROOK, WHITE);
  
  for (int x = 0; x < 8; x++) {
    board[x][1] = Piece(PAWN, WHITE);
  }
  
  // 设置黑方棋子
  board[0][7] = Piece(ROOK, BLACK);
  board[1][7] = Piece(KNIGHT, BLACK);
  board[2][7] = Piece(BISHOP, BLACK);
  board[3][7] = Piece(QUEEN, BLACK);
  board[4][7] = Piece(KING, BLACK);
  board[5][7] = Piece(BISHOP, BLACK);
  board[6][7] = Piece(KNIGHT, BLACK);
  board[7][7] = Piece(ROOK, BLACK);
  
  for (int x = 0; x < 8; x++) {
    board[x][6] = Piece(PAWN, BLACK);
  }
  
  currentPlayer = WHITE;
  selectedPiece = Position(-1, -1);
  validMoves.clear();
  whiteKingInCheck = false;
  blackKingInCheck = false;
  
  // 初始化王车易位状态
  whiteKingMoved = false;
  blackKingMoved = false;
  whiteRookMoved[0] = false; // a1
  whiteRookMoved[1] = false; // h1
  blackRookMoved[0] = false; // a8
  blackRookMoved[1] = false; // h8
  
  // 初始化吃过路兵状态
  enPassantTarget = Position(-1, -1);
  lastMoveFrom = Position(-1, -1);
  lastMoveTo = Position(-1, -1);
  
  // 初始化游戏状态
  currentState = NormalPlay;
  
  // 初始化升变相关状态
  promotionPawnPos = Position(-1, -1);
  promotionColor = WHITE;
  selectedPromotionPiece = QUEEN;
}

void ChessBoard::resetBoard() {
  initBoard();
}

bool ChessBoard::isOnBoard(int x, int y) const {
  return x >= 0 && x < 8 && y >= 0 && y < 8;
}

const Piece& ChessBoard::getPiece(int x, int y) const {
  static Piece emptyPiece(NONE, WHITE);
  if (isOnBoard(x, y)) {
    return board[x][y];
  }
  return emptyPiece;
}

const Piece& ChessBoard::getPiece(const Position& pos) const {
  return getPiece(pos.x, pos.y);
}

void ChessBoard::setPiece(int x, int y, const Piece& piece) {
  if (isOnBoard(x, y)) {
    board[x][y] = piece;
  }
}

void ChessBoard::setPiece(const Position& pos, const Piece& piece) {
  setPiece(pos.x, pos.y, piece);
}

bool ChessBoard::isPathClear(const Position& from, const Position& to) const {
  int dx = to.x - from.x;
  int dy = to.y - from.y;
  
  // 横向移动
  if (dy == 0) {
    int step = dx > 0 ? 1 : -1;
    for (int x = from.x + step; x != to.x; x += step) {
      if (!board[x][from.y].isEmpty()) {
        return false;
      }
    }
  }
  // 纵向移动
  else if (dx == 0) {
    int step = dy > 0 ? 1 : -1;
    for (int y = from.y + step; y != to.y; y += step) {
      if (!board[from.x][y].isEmpty()) {
        return false;
      }
    }
  }
  // 斜向移动
  else if (abs(dx) == abs(dy)) {
    int stepX = dx > 0 ? 1 : -1;
    int stepY = dy > 0 ? 1 : -1;
    int x = from.x + stepX;
    int y = from.y + stepY;
    while (x != to.x && y != to.y) {
      if (!board[x][y].isEmpty()) {
        return false;
      }
      x += stepX;
      y += stepY;
    }
  }
  
  return true;
}

bool ChessBoard::isMoveValid(const Position& from, const Position& to) const {
  if (!from.isValid() || !to.isValid()) {
    return false;
  }
  
  const Piece& fromPiece = getPiece(from);
  const Piece& toPiece = getPiece(to);
  
  if (fromPiece.isEmpty()) {
    return false;
  }
  
  if (fromPiece.color == toPiece.color && !toPiece.isEmpty()) {
    return false;
  }
  
  // 检查是否是王车易位
  if (fromPiece.type == KING) {
    int dx = abs(to.x - from.x);
    int dy = abs(to.y - from.y);
    
    // 王车易位（横向移动2格）
    if (dx == 2 && dy == 0) {
      if (fromPiece.color == WHITE) {
        // 白王易位
        if (from.y != 0 || to.y != 0) return false;
        
        // 短易位（e1→g1）
        if (to.x == 6) {
          return !whiteKingMoved && !whiteRookMoved[1] && 
                 isPathClear(from, to) && !isKingInCheck(WHITE) &&
                 !simulateMoveAndCheckCheck(from, Position(5, 0), WHITE);
        }
        // 长易位（e1→c1）
        else if (to.x == 2) {
          return !whiteKingMoved && !whiteRookMoved[0] && 
                 isPathClear(from, to) && !isKingInCheck(WHITE) &&
                 !simulateMoveAndCheckCheck(from, Position(3, 0), WHITE);
        }
      } else {
        // 黑王易位
        if (from.y != 7 || to.y != 7) return false;
        
        // 短易位（e8→g8）
        if (to.x == 6) {
          return !blackKingMoved && !blackRookMoved[1] && 
                 isPathClear(from, to) && !isKingInCheck(BLACK) &&
                 !simulateMoveAndCheckCheck(from, Position(5, 7), BLACK);
        }
        // 长易位（e8→c8）
        else if (to.x == 2) {
          return !blackKingMoved && !blackRookMoved[0] && 
                 isPathClear(from, to) && !isKingInCheck(BLACK) &&
                 !simulateMoveAndCheckCheck(from, Position(3, 7), BLACK);
        }
      }
    }
  }
  
  int dx = abs(to.x - from.x);
  int dy = abs(to.y - from.y);
  
  switch (fromPiece.type) {
    case PAWN: {
      int direction = (fromPiece.color == WHITE) ? 1 : -1;
      
      // 前进一格
      if (dx == 0 && dy == 1 && toPiece.isEmpty() && (to.y - from.y) == direction) {
        return true;
      }
      
      // 前进两格（初始位置）
      if (dx == 0 && dy == 2 && toPiece.isEmpty() && 
          ((fromPiece.color == WHITE && from.y == 1) || (fromPiece.color == BLACK && from.y == 6)) &&
          (to.y - from.y) == 2 * direction) {
        return true;
      }
      
      // 吃子
      if (dx == 1 && dy == 1 && !toPiece.isEmpty() && (to.y - from.y) == direction) {
        return true;
      }
      
      // 吃过路兵
      if (dx == 1 && dy == 1 && toPiece.isEmpty() && (to.y - from.y) == direction) {
        // 检查是否有可以吃过路兵的目标格
        Position capturedPawnPos(to.x, from.y);
        if (enPassantTarget.isValid() && enPassantTarget == to && getPiece(capturedPawnPos).type == PAWN && getPiece(capturedPawnPos).color != fromPiece.color) {
          return true;
        }
      }
      
      return false;
    }
    
    case KNIGHT: {
      return (dx == 2 && dy == 1) || (dx == 1 && dy == 2);
    }
    
    case BISHOP: {
      return dx == dy && isPathClear(from, to);
    }
    
    case ROOK: {
      return (dx == 0 || dy == 0) && isPathClear(from, to);
    }
    
    case QUEEN: {
      return (dx == 0 || dy == 0 || dx == dy) && isPathClear(from, to);
    }
    
    case KING: {
      // 普通移动
      if (dx <= 1 && dy <= 1) {
        return true;
      }
      // 王车易位
      if (dx == 2 && dy == 0) {
        Color kingColor = getPiece(from).color;
        if (kingColor == WHITE) {
          // 白王易位
          if (whiteKingMoved) return false;
          if (to.x == 6) { // 短易位
            if (whiteRookMoved[1]) return false;
            // 检查路径是否被阻挡
            if (!getPiece(6, 0).isEmpty() || !getPiece(5, 0).isEmpty()) return false;
            // 检查王是否经过被攻击的格子
            if (isKingInCheck(WHITE)) return false;
            if (wouldPutKingInCheck(from, Position(5, 0))) return false;
            return true;
          } else if (to.x == 2) { // 长易位
            if (whiteRookMoved[0]) return false;
            // 检查路径是否被阻挡
            if (!getPiece(1, 0).isEmpty() || !getPiece(2, 0).isEmpty() || !getPiece(3, 0).isEmpty()) return false;
            // 检查王是否经过被攻击的格子
            if (isKingInCheck(WHITE)) return false;
            if (wouldPutKingInCheck(from, Position(3, 0))) return false;
            return true;
          }
        } else {
          // 黑王易位
          if (blackKingMoved) return false;
          if (to.x == 6) { // 短易位
            if (blackRookMoved[1]) return false;
            // 检查路径是否被阻挡
            if (!getPiece(6, 7).isEmpty() || !getPiece(5, 7).isEmpty()) return false;
            // 检查王是否经过被攻击的格子
            if (isKingInCheck(BLACK)) return false;
            if (wouldPutKingInCheck(from, Position(5, 7))) return false;
            return true;
          } else if (to.x == 2) { // 长易位
            if (blackRookMoved[0]) return false;
            // 检查路径是否被阻挡
            if (!getPiece(1, 7).isEmpty() || !getPiece(2, 7).isEmpty() || !getPiece(3, 7).isEmpty()) return false;
            // 检查王是否经过被攻击的格子
            if (isKingInCheck(BLACK)) return false;
            if (wouldPutKingInCheck(from, Position(3, 7))) return false;
            return true;
          }
        }
      }
      return false;
    }
    
    default:
      return false;
  }
}

bool ChessBoard::wouldPutKingInCheck(const Position& from, const Position& to) const {
  Color kingColor = getPiece(from).color;
  return simulateMoveAndCheckCheck(from, to, kingColor);
}

bool ChessBoard::simulateMoveAndCheckCheck(const Position& from, const Position& to, Color kingColor) const {
  // 保存原始状态
  Piece originalFromPiece = getPiece(from);
  Piece originalToPiece = getPiece(to);
  
  // 模拟移动
  const_cast<ChessBoard*>(this)->setPiece(to, originalFromPiece);
  const_cast<ChessBoard*>(this)->setPiece(from, Piece(NONE, WHITE));
  
  // 检查是否被将军
  bool inCheck = isKingInCheck(kingColor);
  
  // 恢复原始状态
  const_cast<ChessBoard*>(this)->setPiece(from, originalFromPiece);
  const_cast<ChessBoard*>(this)->setPiece(to, originalToPiece);
  
  return inCheck;
}

bool ChessBoard::isKingInCheck(Color color) const {
  // 找到王的位置
  Position kingPos(-1, -1);
  for (int y = 0; y < 8; y++) {
    for (int x = 0; x < 8; x++) {
      const Piece& piece = getPiece(x, y);
      if (piece.type == KING && piece.color == color) {
        kingPos = Position(x, y);
        break;
      }
    }
    if (kingPos.isValid()) {
      break;
    }
  }
  
  if (!kingPos.isValid()) {
    return false;
  }
  
  // 检查对方棋子是否可以攻击王
  Color opponentColor = (color == WHITE) ? BLACK : WHITE;
  for (int y = 0; y < 8; y++) {
    for (int x = 0; x < 8; x++) {
      const Piece& piece = getPiece(x, y);
      if (piece.type != NONE && piece.color == opponentColor) {
        if (isMoveValid(Position(x, y), kingPos)) {
          return true;
        }
      }
    }
  }
  
  return false;
}

bool ChessBoard::canCaptureKing(Color attackerColor) const {
  // 找到对方王的位置
  Color kingColor = (attackerColor == WHITE) ? BLACK : WHITE;
  Position kingPos(-1, -1);
  for (int y = 0; y < 8; y++) {
    for (int x = 0; x < 8; x++) {
      const Piece& piece = getPiece(x, y);
      if (piece.type == KING && piece.color == kingColor) {
        kingPos = Position(x, y);
        break;
      }
    }
    if (kingPos.isValid()) {
      break;
    }
  }
  
  if (!kingPos.isValid()) {
    return false;
  }
  
  // 检查攻击者是否可以吃掉王
  for (int y = 0; y < 8; y++) {
    for (int x = 0; x < 8; x++) {
      const Piece& piece = getPiece(x, y);
      if (piece.type != NONE && piece.color == attackerColor) {
        if (isMoveValid(Position(x, y), kingPos)) {
          return true;
        }
      }
    }
  }
  
  return false;
}

void ChessBoard::generateValidMoves(const Position& pos) {
  validMoves.clear();
  
  if (!pos.isValid()) {
    return;
  }
  
  const Piece& piece = getPiece(pos);
  if (piece.isEmpty() || piece.color != currentPlayer) {
    return;
  }
  
  // 遍历所有可能的位置
  for (int y = 0; y < 8; y++) {
    for (int x = 0; x < 8; x++) {
      Position to(x, y);
      if (isMoveValid(pos, to) && !wouldPutKingInCheck(pos, to)) {
        validMoves.push_back(to);
      }
    }
  }
}

bool ChessBoard::selectPiece(const Position& pos) {
  // 在升变状态下，不允许选择棋子
  if (currentState == PromotionSelecting) {
    return false;
  }
  
  if (!pos.isValid()) {
    return false;
  }
  
  const Piece& piece = getPiece(pos);
  if (piece.isEmpty() || piece.color != currentPlayer) {
    return false;
  }
  
  selectedPiece = pos;
  generateValidMoves(pos);
  return true;
}

void ChessBoard::deselectPiece() {
  selectedPiece = Position(-1, -1);
  validMoves.clear();
}

bool ChessBoard::movePiece(const Position& from, const Position& to) {
  if (!from.isValid() || !to.isValid()) {
    return false;
  }
  
  Piece fromPiece = getPiece(from);
  Piece toPiece = getPiece(to);
  
  if (fromPiece.isEmpty() || fromPiece.color != currentPlayer) {
    return false;
  }
  
  if (!isMoveValid(from, to)) {
    return false;
  }
  
  if (wouldPutKingInCheck(from, to)) {
    return false;
  }
  
  // 保存上一手棋信息
  lastMoveFrom = from;
  lastMoveTo = to;
  
  // 保存目标位置的棋子（用于PGN生成和撤销）
  lastCapturedPiece = getPiece(to);
  Piece targetPiece = lastCapturedPiece;
  
  // 保存当前吃过路兵目标格（用于吃过路兵验证）
  Position originalEnPassantTarget = enPassantTarget;
  
  // 保存所有必要的状态用于撤销
  wasWhiteKingInCheck = whiteKingInCheck;
  wasBlackKingInCheck = blackKingInCheck;
  wasWhiteKingMoved = whiteKingMoved;
  wasBlackKingMoved = blackKingMoved;
  wasWhiteRookMoved[0] = whiteRookMoved[0];
  wasWhiteRookMoved[1] = whiteRookMoved[1];
  wasBlackRookMoved[0] = blackRookMoved[0];
  wasBlackRookMoved[1] = blackRookMoved[1];
  wasEnPassantTarget = enPassantTarget;
  wasCurrentPlayer = currentPlayer;
  
  // 执行移动
        setPiece(to, fromPiece);
        setPiece(from, Piece(NONE, fromPiece.color));
  
  // 初始化吃过路兵目标格
  enPassantTarget = Position(-1, -1);
  
  // 处理王车易位
  if (fromPiece.type == KING) {
    int dx = abs(to.x - from.x);
    Serial.printf("King move detected: from (%d,%d) to (%d,%d), dx=%d\n", from.x, from.y, to.x, to.y, dx);
    if (dx == 2 && fromPiece.color == WHITE) {
      // 白王易位
      if (to.x == 6) { // 短易位：王从e1到g1，车从h1到f1
        setPiece(Position(5, 0), getPiece(Position(7, 0)));
        setPiece(Position(7, 0), Piece(NONE, fromPiece.color));
        whiteRookMoved[1] = true;
        Serial.println("White castled short");
      } else if (to.x == 2) { // 长易位：王从e1到c1，车从a1到d1
        setPiece(Position(3, 0), getPiece(Position(0, 0)));
        setPiece(Position(0, 0), Piece(NONE, fromPiece.color));
        whiteRookMoved[0] = true;
        Serial.println("White castled long");
      }
      whiteKingMoved = true;
    } else if (dx == 2 && fromPiece.color == BLACK) {
      Serial.println("Black castling detected");
      // 黑王易位
      if (to.x == 6) { // 短易位：王从e8到g8，车从h8到f8
              setPiece(Position(5, 7), getPiece(Position(7, 7)));
              setPiece(Position(7, 7), Piece(NONE, BLACK));
              blackRookMoved[1] = true;
              Serial.println("Black castled short");
            } else if (to.x == 2) { // 长易位：王从e8到c8，车从a8到d8
              setPiece(Position(3, 7), getPiece(Position(0, 7)));
              setPiece(Position(0, 7), Piece(NONE, BLACK));
              blackRookMoved[0] = true;
              Serial.println("Black castled long");
            }
      blackKingMoved = true;
    }
  }
  
  // 处理吃过路兵
  if (fromPiece.type == PAWN) {
    int dx = abs(to.x - from.x);
    int dy = abs(to.y - from.y);
    
    // 兵前进两格，设置吃过路兵目标格
    if (dx == 0 && dy == 2) {
      enPassantTarget = Position(from.x, from.y + (fromPiece.color == WHITE ? 1 : -1));
    }
    
    // 吃过路兵
    if (dx == 1 && dy == 1 && toPiece.isEmpty()) {
      // 检查是否符合吃过路兵条件
        int direction = (fromPiece.color == WHITE) ? 1 : -1;
        Position capturedPawnPos(to.x, from.y);
        // 使用移动前的吃过路兵目标格进行检查
        if (originalEnPassantTarget.isValid() && originalEnPassantTarget == to && getPiece(capturedPawnPos).type == PAWN && getPiece(capturedPawnPos).color != fromPiece.color) {
          // 移除被吃的兵
          setPiece(capturedPawnPos, Piece(NONE, fromPiece.color));
          serialPrintf("En passant capture: removed pawn at (%d,%d)\n", capturedPawnPos.x, capturedPawnPos.y);
        }
    }
    
    // 兵升变
    if ((fromPiece.color == WHITE && to.y == 7) || (fromPiece.color == BLACK && to.y == 0)) {
      // 触发升变状态
      enterPromotionState(to, fromPiece.color);
      // 不切换玩家，留在升变状态
      return true;
    }
  }
  
  // 更新王车易位状态
  if (fromPiece.type == ROOK) {
    if (fromPiece.color == WHITE) {
      if (from.x == 0 && from.y == 0) whiteRookMoved[0] = true;
      if (from.x == 7 && from.y == 0) whiteRookMoved[1] = true;
    } else {
      if (from.x == 0 && from.y == 7) blackRookMoved[0] = true;
      if (from.x == 7 && from.y == 7) blackRookMoved[1] = true;
    }
  }
  
  // 检查对方是否被将军
  Color opponentColor = (currentPlayer == WHITE) ? BLACK : WHITE;
  if (isKingInCheck(opponentColor)) {
    if (opponentColor == WHITE) {
      whiteKingInCheck = true;
    } else {
      blackKingInCheck = true;
    }
  } else {
    if (opponentColor == WHITE) {
      whiteKingInCheck = false;
    } else {
      blackKingInCheck = false;
    }
  }
  
  // 切换玩家
  switchPlayer();
  
  // 取消选择
  deselectPiece();
  
  // 输出FEN到终端
  Serial.println("FEN: " + toFEN());
  
  // 输出PGN记谱法
  Serial.println("PGN: " + toPGN(from, to, fromPiece, targetPiece));
  
  return true;
}

Color ChessBoard::getCurrentPlayer() const {
  return currentPlayer;
}

Position ChessBoard::getSelectedPiece() const {
  return selectedPiece;
}

const std::vector<Position>& ChessBoard::getValidMoves() const {
  return validMoves;
}

bool ChessBoard::hasValidMoves() const {
  // 检查当前玩家是否有任何合法移动
  for (int y = 0; y < 8; y++) {
    for (int x = 0; x < 8; x++) {
      Position from(x, y);
      const Piece& piece = getPiece(from);
      if (piece.type != NONE && piece.color == currentPlayer) {
        for (int ty = 0; ty < 8; ty++) {
          for (int tx = 0; tx < 8; tx++) {
            Position to(tx, ty);
            if (isMoveValid(from, to) && !wouldPutKingInCheck(from, to)) {
              return true;
            }
          }
        }
      }
    }
  }
  return false;
}

bool ChessBoard::isCheckmate(Color color) const {
  if (!isKingInCheck(color)) {
    return false;
  }
  
  // 检查是否有任何合法移动可以解除将军
  for (int y = 0; y < 8; y++) {
    for (int x = 0; x < 8; x++) {
      Position from(x, y);
      const Piece& piece = getPiece(from);
      if (piece.type != NONE && piece.color == color) {
        for (int ty = 0; ty < 8; ty++) {
          for (int tx = 0; tx < 8; tx++) {
            Position to(tx, ty);
            if (isMoveValid(from, to) && !wouldPutKingInCheck(from, to)) {
              return false;
            }
          }
        }
      }
    }
  }
  
  return true;
}

bool ChessBoard::isInCheck(Color color) const {
  return isKingInCheck(color);
}

void ChessBoard::switchPlayer() {
  currentPlayer = (currentPlayer == WHITE) ? BLACK : WHITE;
}

String ChessBoard::toFEN() const {
  String fen;
  
  // 棋盘布局
  for (int y = 7; y >= 0; y--) {
    int emptyCount = 0;
    for (int x = 0; x < 8; x++) {
      const Piece& piece = getPiece(x, y);
      if (piece.isEmpty()) {
        emptyCount++;
      } else {
        if (emptyCount > 0) {
          fen += String(emptyCount);
          emptyCount = 0;
        }
        char c;
        switch (piece.type) {
          case PAWN: c = 'p'; break;
          case KNIGHT: c = 'n'; break;
          case BISHOP: c = 'b'; break;
          case ROOK: c = 'r'; break;
          case QUEEN: c = 'q'; break;
          case KING: c = 'k'; break;
          default: c = ' ';
        }
        if (piece.color == WHITE) {
          c = toupper(c);
        }
        fen += c;
      }
    }
    if (emptyCount > 0) {
      fen += String(emptyCount);
    }
    if (y > 0) {
      fen += '/';
    }
  }
  
  // 当前玩家
  fen += ' ';
  fen += (currentPlayer == WHITE) ? 'w' : 'b';
  
  // 易位权（暂不实现）
  fen += ' ';
  fen += '-';
  
  // 吃过路兵（暂不实现）
  fen += ' ';
  fen += '-';
  
  // 半回合（暂不实现）
  fen += ' ';
  fen += '0';
  
  // 全回合（暂不实现）
  fen += ' ';
  fen += '1';
  
  return fen;
}

String ChessBoard::toPGN(const Position& from, const Position& to, const Piece& piece, const Piece& targetPiece) const {
  String pgn;
  
  // 处理王车易位
  Serial.printf("toPGN: from (%d,%d) to (%d,%d), piece type=%d, dx=%d\n", from.x, from.y, to.x, to.y, piece.type, abs(to.x - from.x));
  if (piece.type == KING && abs(to.x - from.x) == 2) {
    if (to.x > from.x) {
      return "O-O"; // 短易位
    } else {
      return "O-O-O"; // 长易位
    }
  }
  
  // 处理兵升变
  if (piece.type == PAWN && ((piece.color == WHITE && to.y == 7) || (piece.color == BLACK && to.y == 0))) {
    // 默认升变为后
    return positionToPGN(to) + "=Q";
  }
  
  // 处理棋子类型
  switch (piece.type) {
    case KNIGHT: pgn += "N"; break;
    case BISHOP: pgn += "B"; break;
    case ROOK: pgn += "R"; break;
    case QUEEN: pgn += "Q"; break;
    case KING: pgn += "K"; break;
    case PAWN: pgn += "P"; break; // 兵显示类型
    default: break;
  }
  
  // 处理吃子
  bool isEnPassant = false;
  if (piece.type == PAWN && abs(to.x - from.x) == 1 && abs(to.y - from.y) == 1 && targetPiece.isEmpty()) {
    // 吃过路兵
    isEnPassant = true;
    pgn += char('a' + from.x);
    pgn += "x";
  } else if (targetPiece.type != NONE && targetPiece.color != piece.color) {
    // 普通吃子
    if (piece.type == PAWN) {
      // 兵吃子显示来源列
      pgn += char('a' + from.x);
    }
    pgn += "x";
  }
  
  // 处理目标位置
  pgn += positionToPGN(to);
  
  // 处理吃过路兵标记
  if (isEnPassant) {
    pgn += " e.p.";
  }
  
  return pgn;
}

String ChessBoard::positionToPGN(const Position& pos) const {
  String pgn;
  pgn += char('a' + pos.x); // 列：a-h
  pgn += char('1' + pos.y); // 行：1-8
  return pgn;
}

Move ChessBoard::parsePGN(const String& pgn) const {
  String cleanPGN = pgn;
  // 移除可能的注释和空格
  cleanPGN.trim();
  
  // 处理王车易位
  if (cleanPGN.equals("O-O")) {
    // 短易位
    if (currentPlayer == WHITE) {
      // 白方短易位
      return Move(Position(4, 0), Position(6, 0));
    } else {
      // 黑方短易位
      return Move(Position(4, 7), Position(6, 7));
    }
  } else if (cleanPGN.equals("O-O-O")) {
    // 长易位
    if (currentPlayer == WHITE) {
      // 白方长易位
      return Move(Position(4, 0), Position(2, 0));
    } else {
      // 黑方长易位
      return Move(Position(4, 7), Position(2, 7));
    }
  }
  
  // 处理升变
  int equalsIndex = cleanPGN.indexOf('=');
  bool isPromotion = equalsIndex != -1;
  String promotionPGN;
  if (isPromotion) {
    promotionPGN = cleanPGN.substring(0, equalsIndex);
  } else {
    promotionPGN = cleanPGN;
  }
  
  // 解析目标位置（最后两个字符）
  int targetCol = promotionPGN[promotionPGN.length() - 2] - 'a';
  int targetRow = promotionPGN[promotionPGN.length() - 1] - '1';
  Position target(targetCol, targetRow);
  
  // 解析棋子类型
  PieceType pieceType = PAWN;
  if (promotionPGN.length() > 2 && (promotionPGN[0] >= 'A' && promotionPGN[0] <= 'Z')) {
    switch (promotionPGN[0]) {
      case 'N': pieceType = KNIGHT; break;
      case 'B': pieceType = BISHOP; break;
      case 'R': pieceType = ROOK; break;
      case 'Q': pieceType = QUEEN; break;
      case 'K': pieceType = KING; break;
      default: pieceType = PAWN; break;
    }
  }
  
  // 解析吃子和来源信息
  bool isCapture = promotionPGN.indexOf('x') != -1;
  int captureIndex = promotionPGN.indexOf('x');
  
  // 解析来源信息
  int sourceCol = -1;
  int sourceRow = -1;
  
  // 检查是否有来源列或行
  if (promotionPGN.length() > 2) {
    int pieceIndex = (pieceType != PAWN) ? 1 : 0;
    int infoStart = pieceIndex;
    int infoEnd = isCapture ? captureIndex : promotionPGN.length() - 2;
    
    if (infoEnd > infoStart) {
      String info = promotionPGN.substring(infoStart, infoEnd);
      
      // 解析来源列
      if (info.length() > 0 && info[0] >= 'a' && info[0] <= 'h') {
        sourceCol = info[0] - 'a';
      }
      
      // 解析来源行
      if (info.length() > 0 && info[info.length() - 1] >= '1' && info[info.length() - 1] <= '8') {
        sourceRow = info[info.length() - 1] - '1';
      }
    }
  }
  
  // 查找匹配的棋子
  Position source(-1, -1);
  // 对于黑方，从高行号开始查找，确保选择正确的棋子
  if (currentPlayer == BLACK) {
    for (int y = 7; y >= 0; y--) {
      for (int x = 0; x < 8; x++) {
        const Piece& piece = board[x][y];
        if (piece.type == pieceType && piece.color == currentPlayer) {
          // 检查是否匹配来源信息
          if ((sourceCol == -1 || x == sourceCol) && (sourceRow == -1 || y == sourceRow)) {
            // 检查是否可以移动到目标位置
            if (isMoveValid(Position(x, y), target) && !wouldPutKingInCheck(Position(x, y), target)) {
              source = Position(x, y);
              break;
            }
          }
        }
      }
      if (source.isValid()) break;
    }
  } else {
    // 白方保持正常顺序
    for (int y = 0; y < 8; y++) {
      for (int x = 0; x < 8; x++) {
        const Piece& piece = board[x][y];
        if (piece.type == pieceType && piece.color == currentPlayer) {
          // 检查是否匹配来源信息
          if ((sourceCol == -1 || x == sourceCol) && (sourceRow == -1 || y == sourceRow)) {
            // 检查是否可以移动到目标位置
            if (isMoveValid(Position(x, y), target) && !wouldPutKingInCheck(Position(x, y), target)) {
              source = Position(x, y);
              break;
            }
          }
        }
      }
      if (source.isValid()) break;
    }
  }
  
  return Move(source, target);
}

GameState ChessBoard::getCurrentState() const {
  return currentState;
}

void ChessBoard::enterPromotionState(const Position& pos, Color color) {
  currentState = PromotionSelecting;
  promotionPawnPos = pos;
  promotionColor = color;
  selectedPromotionPiece = QUEEN; // 默认选择皇后
}

void ChessBoard::confirmPromotion() {
  if (currentState != PromotionSelecting) return;
  
  // 将待升变的兵替换为选中的棋子
  setPiece(promotionPawnPos, Piece(selectedPromotionPiece, promotionColor));
  
  // 退出升变状态，切换到正常游戏状态
  currentState = NormalPlay;
  
  // 切换玩家
  switchPlayer();
}

void ChessBoard::navigatePromotionSelection(int direction) {
  if (currentState != PromotionSelecting) return;
  
  // 升变可选棋子：ROOK, KNIGHT, QUEEN, BISHOP
  const PieceType promotionOptions[] = {ROOK, KNIGHT, QUEEN, BISHOP};
  const int numOptions = sizeof(promotionOptions) / sizeof(promotionOptions[0]);
  
  // 找到当前选中的棋子在选项中的索引
  int currentIndex = 0;
  for (int i = 0; i < numOptions; i++) {
    if (promotionOptions[i] == selectedPromotionPiece) {
      currentIndex = i;
      break;
    }
  }
  
  // 根据方向计算新的索引
  currentIndex += direction;
  
  // 循环选择
  if (currentIndex < 0) {
    currentIndex = numOptions - 1;
  } else if (currentIndex >= numOptions) {
    currentIndex = 0;
  }
  
  // 更新选中的棋子
  selectedPromotionPiece = promotionOptions[currentIndex];
}

Position ChessBoard::getPromotionPawnPos() const {
  return promotionPawnPos;
}

Color ChessBoard::getPromotionColor() const {
  return promotionColor;
}

PieceType ChessBoard::getSelectedPromotionPiece() const {
  return selectedPromotionPiece;
}

bool ChessBoard::fromFEN(const String& fen) {
  // 简单实现，仅解析棋盘布局和当前玩家
  int index = 0;
  int x = 0;
  int y = 7;
  
  while (index < fen.length() && fen[index] != ' ') {
    char c = fen[index];
    if (c == '/') {
      x = 0;
      y--;
      index++;
    } else if (isdigit(c)) {
      int count = c - '0';
      for (int i = 0; i < count; i++) {
        setPiece(x, y, Piece(NONE, WHITE));
        x++;
      }
      index++;
    } else {
      PieceType type;
      Color color = isupper(c) ? WHITE : BLACK;
      c = tolower(c);
      
      switch (c) {
        case 'p': type = PAWN; break;
        case 'n': type = KNIGHT; break;
        case 'b': type = BISHOP; break;
        case 'r': type = ROOK; break;
        case 'q': type = QUEEN; break;
        case 'k': type = KING; break;
        default: return false;
      }
      
      setPiece(x, y, Piece(type, color));
      x++;
      index++;
    }
  }
  
  // 解析当前玩家
  while (index < fen.length() && fen[index] == ' ') {
    index++;
  }
  if (index < fen.length()) {
    currentPlayer = (fen[index] == 'w') ? WHITE : BLACK;
  }
  
  deselectPiece();
  return true;
}

// 验证移动是否合法（公共方法，用于测试）
bool ChessBoard::validateMove(const Position& from, const Position& to) const {
  return isMoveValid(from, to) && !wouldPutKingInCheck(from, to);
}

// 撤销上一步移动
void ChessBoard::undoMove() {
  if (!lastMoveFrom.isValid() || !lastMoveTo.isValid()) {
    return; // 没有可撤销的移动
  }
  
  Piece movedPiece = getPiece(lastMoveTo);
  
  // 将棋子放回起始位置
  setPiece(lastMoveFrom, movedPiece);
  
  // 将被吃掉的棋子放回目标位置
  setPiece(lastMoveTo, lastCapturedPiece);
  
  // 恢复状态
  whiteKingInCheck = wasWhiteKingInCheck;
  blackKingInCheck = wasBlackKingInCheck;
  whiteKingMoved = wasWhiteKingMoved;
  blackKingMoved = wasBlackKingMoved;
  whiteRookMoved[0] = wasWhiteRookMoved[0];
  whiteRookMoved[1] = wasWhiteRookMoved[1];
  blackRookMoved[0] = wasBlackRookMoved[0];
  blackRookMoved[1] = wasBlackRookMoved[1];
  enPassantTarget = wasEnPassantTarget;
  currentPlayer = wasCurrentPlayer;
  
  // 特殊处理王车易位的撤销
  if (movedPiece.type == KING) {
    int dx = abs(lastMoveTo.x - lastMoveFrom.x);
    if (dx == 2) {
      if (movedPiece.color == WHITE) {
        if (lastMoveTo.x == 6) { // 撤销白王短易位
          setPiece(Position(7, 0), getPiece(Position(5, 0)));
          setPiece(Position(5, 0), Piece(NONE, WHITE));
        } else if (lastMoveTo.x == 2) { // 撤销白王长易位
          setPiece(Position(0, 0), getPiece(Position(3, 0)));
          setPiece(Position(3, 0), Piece(NONE, WHITE));
        }
      } else {
        if (lastMoveTo.x == 6) { // 撤销黑王短易位
          setPiece(Position(7, 7), getPiece(Position(5, 7)));
          setPiece(Position(5, 7), Piece(NONE, BLACK));
        } else if (lastMoveTo.x == 2) { // 撤销黑王长易位
          setPiece(Position(0, 7), getPiece(Position(3, 7)));
          setPiece(Position(3, 7), Piece(NONE, BLACK));
        }
      }
    }
  }
  
  // 特殊处理吃过路兵的撤销
  if (movedPiece.type == PAWN) {
    int dx = abs(lastMoveTo.x - lastMoveFrom.x);
    int dy = abs(lastMoveTo.y - lastMoveFrom.y);
    
    // 检查是否是吃过路兵
    if (dx == 1 && dy == 1 && lastCapturedPiece.isEmpty()) {
      // 恢复被吃的兵
      int direction = (movedPiece.color == WHITE) ? 1 : -1;
      Position capturedPawnPos(lastMoveTo.x, lastMoveFrom.y);
      setPiece(capturedPawnPos, Piece(PAWN, (movedPiece.color == WHITE) ? BLACK : WHITE));
    }
  }
  
  // 取消选择
  deselectPiece();
}
