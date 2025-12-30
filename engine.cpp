#include "common.h"
#include <vector>
#include <algorithm> // std::max, std::min
#include <cstdlib>   // rand(), srand()
#include <ctime>     // time()

// ==========================================
// 1. 基础结构与配置
// ==========================================



// 简单的结构体，用来暂存走法和对应的分数
struct ScoredMove {
    Move move;
    int score;
    ScoredMove(Move m, int s) : move(m), score(s) {}
};

// ==========================================
// 位置价值表 (Piece-Square Tables)
// ==========================================

// 兵的位置表：加强了 e4, d4, e5, d5 中心格的分数，鼓励冲中心兵
static const int PAWN_TABLE[8][8] = {
    { 0,  0,  0,  0,  0,  0,  0,  0},
    {50, 50, 50, 50, 50, 50, 50, 50},
    {10, 10, 20, 30, 30, 20, 10, 10},
    { 5,  5, 10, 28, 28, 10,  5,  5}, // 略微提高了中心兵的价值 (25 -> 28)
    { 0,  0,  0, 25, 25,  0,  0,  0}, // 略微提高了中心兵的价值 (20 -> 25)
    { 5, -5,-10,  0,  0,-10, -5,  5},
    { 5, 10, 10,-25,-25, 10, 10,  5}, // 稍微惩罚阻挡中心兵
    { 0,  0,  0,  0,  0,  0,  0,  0}
};

// 马的位置表
static const int KNIGHT_TABLE[8][8] = {
    {-50,-40,-30,-30,-30,-30,-40,-50},
    {-40,-20,  0,  0,  0,  0,-20,-40},
    {-30,  0, 10, 15, 15, 10,  0,-30},
    {-30,  5, 15, 20, 20, 15,  5,-30},
    {-30,  0, 15, 20, 20, 15,  0,-30},
    {-30,  5, 10, 15, 15, 10,  5,-30},
    {-40,-20,  0,  5,  5,  0,-20,-40},
    {-50,-40,-30,-30,-30,-30,-40,-50}
};

// 基础子力值
int getPieceValue(PieceType type) {
    switch (type) {
        case QUEEN:  return 900;
        case ROOK:   return 500;
        case BISHOP: return 330;
        case KNIGHT: return 320;
        case PAWN:   return 100;
        case KING:   return 20000;
        default:     return 0;
    }
}

// 获取位置加分
int getPositionBonus(const Piece& piece, int x, int y) {
    int row = (piece.color == WHITE) ? (7 - y) : y;
    if (piece.type == PAWN) return PAWN_TABLE[row][x];
    if (piece.type == KNIGHT) return KNIGHT_TABLE[row][x];
    return 0;
}

// ==========================================
// 2. 局面评估与辅助函数
// ==========================================

// 评估函数
int evaluateBoard(const ChessBoard& board, Color side) {
    int score = 0;
    for (int y = 0; y < 8; y++) {
        for (int x = 0; x < 8; x++) {
            Position pos(x, y);
            const Piece& piece = board.getPiece(pos);
            if (!piece.isEmpty()) {
                int val = getPieceValue(piece.type) + getPositionBonus(piece, x, y);
                score += (piece.color == side) ? val : -val;
            }
        }
    }
    return score;
}

// 获取合法走法
std::vector<Move> getAllValidMoves(const ChessBoard& board, Color side) {
    std::vector<Move> moves;
    for (int y = 0; y < 8; y++) {
        for (int x = 0; x < 8; x++) {
            Position from(x, y);
            const Piece& piece = board.getPiece(from);
            if (!piece.isEmpty() && piece.color == side) {
                ChessBoard temp = board;
                if (temp.selectPiece(from)) {
                    const std::vector<Position>& valid = temp.getValidMoves();
                    for (const Position& to : valid) {
                        moves.push_back(Move(from, to));
                    }
                }
            }
        }
    }
    return moves;
}

// ==========================================
// 3. Minimax 核心算法
// ==========================================

int minimax(ChessBoard board, int depth, int alpha, int beta, bool isMaximizing, Color myColor) {
    if (depth == 0) return evaluateBoard(board, myColor);

    Color currentPlayer = isMaximizing ? myColor : (myColor == WHITE ? BLACK : WHITE);
    std::vector<Move> allMoves = getAllValidMoves(board, currentPlayer);

    if (allMoves.empty()) {
        if (board.isInCheck(currentPlayer)) return isMaximizing ? -99999 : 99999;
        return 0;
    }

    if (isMaximizing) {
        int maxEval = -1000000;
        for (const Move& move : allMoves) {
            ChessBoard tempBoard = board;
            tempBoard.movePiece(move.from, move.to);
            int eval = minimax(tempBoard, depth - 1, alpha, beta, false, myColor);
            maxEval = std::max(maxEval, eval);
            alpha = std::max(alpha, eval);
            if (beta <= alpha) break;
        }
        return maxEval;
    } else {
        int minEval = 1000000;
        for (const Move& move : allMoves) {
            ChessBoard tempBoard = board;
            tempBoard.movePiece(move.from, move.to);
            int eval = minimax(tempBoard, depth - 1, alpha, beta, true, myColor);
            minEval = std::min(minEval, eval);
            beta = std::min(beta, eval);
            if (beta <= alpha) break;
        }
        return minEval;
    }
}

// ==========================================
// 4. AI 入口函数 (已加入随机性逻辑)
// ==========================================

Move chooseAIMove(Color side, const ChessBoard& board) {
    // 静态变量确保只初始化一次随机种子
    static bool seeded = false;
    if (!seeded) {
        srand(time(NULL));
        seeded = true;
    }

    std::vector<Move> allMoves = getAllValidMoves(board, side);
    if (allMoves.empty()) return Move(Position(-1, -1), Position(-1, -1));

    // 搜索深度
    // 搜索深度设为3层，评估速度更快，同时也能保持一定的棋力。4耗时有点久，5会重启
    const int SEARCH_DEPTH = 3;
    
    // 存储所有走法及其评分
    std::vector<ScoredMove> moveScores;
    int maxScore = -1000000;

    // 1. 对每个第一步走法进行打分
    for (const Move& move : allMoves) {
        ChessBoard tempBoard = board;
        tempBoard.movePiece(move.from, move.to);
        
        // 计算分值
        int score = minimax(tempBoard, SEARCH_DEPTH - 1, -1000000, 1000000, false, side);
        
        moveScores.push_back(ScoredMove(move, score));
        if (score > maxScore) {
            maxScore = score;
        }
    }

    // 2. 筛选出“好棋” (Candidates)
    // 策略：如果一个走法的分数在 [最高分 - 容差] 范围内，就算作候选走法
    // 容差值 (Tolerance)：设为 15 分。
    // 兵的价值是100，15分大约是微小的位置差异，不足以送掉一个兵，但足以改变开局选择。
    int tolerance = 15; 
    std::vector<Move> bestCandidates;

    for (const auto& sm : moveScores) {
        if (sm.score >= maxScore - tolerance) {
            bestCandidates.push_back(sm.move);
        }
    }

    // 3. 从候选走法中随机选择一个
    if (!bestCandidates.empty()) {
        int randomIndex = rand() % bestCandidates.size();
        return bestCandidates[randomIndex];
    }

    // 兜底（理论上不会执行到这里）
    return allMoves[0];
}