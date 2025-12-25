#pragma once
#include <M5Cardputer.h>
#include "common.h"
#include "icon_bmp.h"

// 外部变量声明
extern int cursorX;
extern int cursorY;
extern ChessBoard chessBoard;

// 棋盘绘制相关常量
const int BOARD_PADDING = 2;

// 屏幕分辨率
const int SCREEN_WIDTH = 240;
const int SCREEN_HEIGHT = 135;

// 棋盘位置计算
const int BOARD_WIDTH = BOARD_SIZE * SQUARE_SIZE;
const int BOARD_HEIGHT = BOARD_SIZE * SQUARE_SIZE;
const int BOARD_X = (SCREEN_WIDTH - BOARD_WIDTH) / 2;
const int BOARD_Y = (SCREEN_HEIGHT - BOARD_HEIGHT) / 2;

// 颜色定义
const unsigned short COLOR_BLACK = 0x0000;
const unsigned short COLOR_WHITE = 0xFFFF;
const unsigned short COLOR_LIGHT_SQUARE = LIGHT_SQUARE_COLOR;
const unsigned short COLOR_DARK_SQUARE = DARK_SQUARE_COLOR;
const unsigned short COLOR_SELECTED = SELECTED_SQUARE_COLOR;
const unsigned short COLOR_VALID_MOVE = VALID_MOVE_COLOR;
const unsigned short COLOR_BORDER = BORDER_COLOR;

// 绘制棋盘
void drawBoard(M5Canvas *canvas, const ChessBoard& board, bool isWhiteBottom);

// 绘制棋子
void drawPiece(M5Canvas *canvas, const Piece& piece, int x, int y);

// 绘制选中的棋子
void drawSelectedPiece(M5Canvas *canvas, const Position& pos);

// 绘制合法移动
void drawValidMoves(M5Canvas *canvas, const std::vector<Position>& validMoves);

// 绘制回合信息
void drawTurnInfo(M5Canvas *canvas, Color currentPlayer);

// 绘制光标位置的棋子信息
void drawPieceInfo(M5Canvas *canvas);

// 绘制将军信息
void drawCheckInfo(M5Canvas *canvas, bool isInCheck, Color color);

// 坐标转换函数
Position screenToBoard(int screenX, int screenY, bool isWhiteBottom);

// 棋盘坐标转屏幕坐标
void boardToScreen(const Position& pos, int& screenX, int& screenY, bool isWhiteBottom);

// 实现函数
void drawBoard(M5Canvas *canvas, const ChessBoard& board, bool isWhiteBottom) {
  // 绘制棋盘边框
  canvas->drawRect(BOARD_X - BOARD_PADDING, BOARD_Y - BOARD_PADDING, 
                   BOARD_WIDTH + 2 * BOARD_PADDING, BOARD_HEIGHT + 2 * BOARD_PADDING, COLOR_BORDER);
  
  // 绘制棋盘格子
  for (int y = 0; y < BOARD_SIZE; y++) {
    for (int x = 0; x < BOARD_SIZE; x++) {
      int screenX, screenY;
      boardToScreen(Position(x, y), screenX, screenY, isWhiteBottom);
      
      // 计算格子颜色：a1是浅色格子，h8是深色格子
      // 直接使用棋盘坐标计算，不受棋盘旋转影响
      bool isLightSquare = ((x + y) % 2 == 0);
      unsigned short color = isLightSquare ? COLOR_LIGHT_SQUARE : COLOR_DARK_SQUARE;
      
      canvas->fillRect(screenX, screenY, SQUARE_SIZE, SQUARE_SIZE, color);
      
      // 绘制棋子
      const Piece& piece = board.getPiece(x, y);
      if (!piece.isEmpty()) {
        drawPiece(canvas, piece, screenX, screenY);
      }
    }
  }
}

void drawPiece(M5Canvas *canvas, const Piece& piece, int x, int y) {
  if (piece.isEmpty()) {
    return;
  }
  
  // 棋子图标居中绘制
  int pieceX = x + (SQUARE_SIZE - PIECE_WIDTH) / 2;
  int pieceY = y + (SQUARE_SIZE - PIECE_HEIGHT) / 2;
  
  const unsigned short* bmpData = nullptr;
  
  switch (piece.type) {
    case PAWN:
      bmpData = (piece.color == Color::WHITE) ? whitePawnData : blackPawnData;
      break;
    case KNIGHT:
      bmpData = (piece.color == Color::WHITE) ? whiteKnightData : blackKnightData;
      break;
    case BISHOP:
      bmpData = (piece.color == Color::WHITE) ? whiteBishopData : blackBishopData;
      break;
    case ROOK:
      bmpData = (piece.color == Color::WHITE) ? whiteRookData : blackRookData;
      break;
    case QUEEN:
      bmpData = (piece.color == Color::WHITE) ? whiteQueenData : blackQueenData;
      break;
    case KING:
      bmpData = (piece.color == Color::WHITE) ? whiteKingData : blackKingData;
      break;
    default:
      return;
  }
  
  if (bmpData != nullptr) {
    if (piece.color == Color::BLACK) {
      // 黑棋：黑色填充棋子形状，背景透明
      canvas->pushImage(pieceX, pieceY, PIECE_WIDTH, PIECE_HEIGHT, (uint16_t*)bmpData, COLOR_WHITE);
    } else {
      // 白棋：白色填充棋子形状，黑色边框，背景透明
      canvas->pushImage(pieceX, pieceY, PIECE_WIDTH, PIECE_HEIGHT, (uint16_t*)bmpData, COLOR_BLACK);
    }
  }
}

void drawSelectedPiece(M5Canvas *canvas, const Position& pos, bool isWhiteBottom) {
  if (!pos.isValid()) {
    return;
  }
  
  int screenX, screenY;
  boardToScreen(pos, screenX, screenY, isWhiteBottom);
  
  // 增强选中格子的可见性 - 多层边框设计
  // 两层黑色边框（外层）- 增加立体感和对比度
  canvas->drawRect(screenX - 2, screenY - 2, SQUARE_SIZE + 4, SQUARE_SIZE + 4, COLOR_BLACK);
  canvas->drawRect(screenX - 1, screenY - 1, SQUARE_SIZE + 2, SQUARE_SIZE + 2, COLOR_BLACK);
  // 一层选中色边框（内层）- 突出当前选中格子
  canvas->drawRect(screenX, screenY, SQUARE_SIZE, SQUARE_SIZE, COLOR_SELECTED);
}

void drawValidMoves(M5Canvas *canvas, const std::vector<Position>& validMoves, bool isWhiteBottom) {
  for (const Position& pos : validMoves) {
    if (pos.isValid()) {
      int screenX, screenY;
      boardToScreen(pos, screenX, screenY, isWhiteBottom);
      
      // 在格子中心绘制一个小圆点表示合法移动
      int centerX = screenX + SQUARE_SIZE / 2;
      int centerY = screenY + SQUARE_SIZE / 2;
      int radius = 3;
      
      canvas->fillCircle(centerX, centerY, radius, COLOR_VALID_MOVE);
    }
  }
}

void drawTurnInfo(M5Canvas *canvas, Color currentPlayer) {
  const char* playerText = (currentPlayer == Color::WHITE) ? "White" : "Black";
  int textWidth = canvas->textWidth(playerText);
  // 使用canvas的实际宽度计算位置，确保在正确的右上角
  int textX = canvas->width() - textWidth - 5;  // Add 5px padding to right edge
  int textY = 10;  // Move to top right corner with more spacing
  
  // 确保文本背景为黑色，避免闪烁
  canvas->setTextColor(COLOR_WHITE, COLOR_BLACK);
  canvas->setTextSize(1);
  canvas->drawString(playerText, textX, textY);
}

// 绘制光标位置的棋子信息
void drawPieceInfo(M5Canvas *canvas) {
  // 获取光标位置的棋子
  Position cursorPos(cursorX, cursorY);
  const Piece& piece = chessBoard.getPiece(cursorPos);
  
  if (!piece.isEmpty()) {
    // 将棋子类型转换为字母代号
    char pieceLetter;
    switch (piece.type) {
      case PAWN: pieceLetter = 'P'; break;
      case KNIGHT: pieceLetter = 'N'; break;
      case BISHOP: pieceLetter = 'B'; break;
      case ROOK: pieceLetter = 'R'; break;
      case QUEEN: pieceLetter = 'Q'; break;
      case KING: pieceLetter = 'K'; break;
      default: pieceLetter = ' ';
    }
       
    // 创建显示文本
    char pieceText[2] = {pieceLetter, '\0'};
    
    // 计算文本位置（在屏幕右上角，玩家信息下方）
    int textWidth = canvas->textWidth(pieceText);
    int textX = canvas->width() - textWidth - 40;
    int textY = 20;  // 在玩家信息下方
    
    // 绘制棋子字母代号
    canvas->setTextColor(COLOR_WHITE, COLOR_BLACK);
    canvas->setTextSize(1);
    canvas->drawString(pieceText, textX, textY);
  }
}

void drawCheckInfo(M5Canvas *canvas, bool isInCheck, Color color) {
  if (isInCheck) {
    const char* checkText = (color == Color::WHITE) ? "White is in check!" : "Black is in check!";
    int textWidth = canvas->textWidth(checkText);
    int textX = canvas->width() - textWidth - 10;
    int textY = 1;
    
    canvas->setTextColor(COLOR_WHITE, COLOR_BLACK);
    canvas->setTextSize(1);
    canvas->drawString(checkText, textX, textY);
  }
}

Position screenToBoard(int screenX, int screenY, bool isWhiteBottom) {
  int boardX = screenX - BOARD_X;
  int boardY = screenY - BOARD_Y;
  
  if (boardX < 0 || boardX >= BOARD_WIDTH || boardY < 0 || boardY >= BOARD_HEIGHT) {
    return Position(-1, -1);
  }
  
  int x = boardX / SQUARE_SIZE;
  int y = boardY / SQUARE_SIZE;
  
  if (isWhiteBottom) {
    y = BOARD_SIZE - 1 - y;
  } else {
    // 执黑方时，x轴也需要反转
    x = BOARD_SIZE - 1 - x;
    y = BOARD_SIZE - 1 - y;
  }
  
  return Position(x, y);
}

void boardToScreen(const Position& pos, int& screenX, int& screenY, bool isWhiteBottom) {
  int x = pos.x;
  int y = pos.y;
  
  if (isWhiteBottom) {
    // 白方在下，棋盘正常显示（a1在左下角，h8在右上角）
    screenX = BOARD_X + x * SQUARE_SIZE;
    screenY = BOARD_Y + (BOARD_SIZE - 1 - y) * SQUARE_SIZE;
  } else {
    // 黑方在下，棋盘旋转180度（a1在右上角，h8在左下角）
    screenX = BOARD_X + (BOARD_SIZE - 1 - x) * SQUARE_SIZE;
    screenY = BOARD_Y + y * SQUARE_SIZE;
  }
}
