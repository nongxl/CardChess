#include <M5Cardputer.h>
#include "common.h"
#include "draw_helper.h"
#include "puzzle.h"
#include <FS.h>
#include <SD.h>
#include <SPI.h>

// SD卡引脚配置
#define SD_CS_PIN 12
#define SD_MISO_PIN 39
#define SD_MOSI_PIN 14
#define SD_SCK_PIN 40

// SD卡相关常量
#define CHESS_SAVE_DIR "/chess"
#define CHESS_SAVE_FILE "/chess/board.fen"

// SD卡状态
bool sdInitialized = false;

// 键盘键值定义
#define KEY_TAB 0x2b

// 日志函数
void logLine(const String& line) {
    Serial.println(line);
}


// AI相关声明
extern Move chooseAIMove(Color side, const ChessBoard& board);

// 全局画布
M5Canvas *canvas;

// 光标位置（棋盘坐标）
int cursorX = 0;
int cursorY = 0;

// 游戏状态
bool isGameStarted = false;
bool isWhitePlayer = true;
int selectedOption = 0; // 0: white, 1: black, 2: random, 3: load

// 谜题模式相关
bool isPuzzleMode = false;
std::vector<Puzzle> puzzles;
int currentPuzzleIndex = 0;
Puzzle currentPuzzle;
int currentMoveIndex = 0;

// AI走棋记录
Position aiLastMoveFrom = Position(-1, -1); // 记录AI上一步走棋的起始位置
Position aiLastMoveTo = Position(-1, -1);   // 记录AI上一步走棋的目标位置

// 按键处理相关
unsigned long lastKeyPressTime = 0;
const unsigned long DEBOUNCE_DELAY = 100;

// SD卡初始化
bool initializeSDCard() {
    int sclk = SD_SCK_PIN;
    int miso = SD_MISO_PIN;
    int mosi = SD_MOSI_PIN;
    int cs = SD_CS_PIN;

    serialPrintf("[SD] Using fixed pins -> SCLK:%d MISO:%d MOSI:%d CS:%d\n", sclk, miso, mosi, cs);

    // 初始化SPI
    SPI.end();
    SPI.begin(sclk, miso, mosi, cs);
    pinMode(cs, OUTPUT);
    digitalWrite(cs, HIGH);
    delay(100);

    // 尝试不同的SPI配置
    const struct {
        SPIClass* spi;
        uint32_t freq;
        const char* name;
    } spiConfigs[] = {
        {&SPI, 10000000, "SPI-10MHz"},
        {&SPI, 4000000, "SPI-4MHz"}
    };

    bool initialized = false;
    for (const auto& config : spiConfigs) {
        serialPrintf("[SD] Trying %s @ %u Hz...\n", config.name, config.freq);
        
        // 尝试初始化SD卡
        if (SD.begin(cs, *config.spi, config.freq)) {
            serialPrintf("[SD] OK via %s @ %u Hz\n", config.name, config.freq);
            initialized = true;
            break;
        }
        
        serialPrintf("[SD] Failed with %s @ %u Hz\n", config.name, config.freq);
        delay(100);
    }

    if (!initialized) {
        serialPrintln("[SD] All initialization attempts failed.");
        logLine("[SD] Initialization failed - check card");
        return false;
    }

    // 检查卡类型
    uint8_t cardType = SD.cardType();
    const char* cardTypeStr[] = {"MMC", "SD", "SDHC", "UNKNOWN"};
    const char* typeStr = (cardType <= 3) ? cardTypeStr[cardType] : "INVALID";
    
    serialPrintf("[SD] Card type: %s\n", typeStr);
    logLine(String("[SD] Card type: ") + typeStr);

    // 确保保存目录存在
    if (!SD.exists(CHESS_SAVE_DIR)) {
        serialPrintf("[SD] Creating directory: %s\n", CHESS_SAVE_DIR);
        if (SD.mkdir(CHESS_SAVE_DIR)) {
            serialPrintf("[SD] Directory created successfully\n");
            logLine(String("[SD] Created directory: ") + CHESS_SAVE_DIR);
        } else {
            serialPrintf("[SD] Failed to create directory: %s\n", CHESS_SAVE_DIR);
            logLine(String("[SD] Failed to create directory: ") + CHESS_SAVE_DIR);
            return false;
        }
    }

    sdInitialized = true;
    return true;
}

// 保存棋盘状态到SD卡
bool saveBoardState() {
    if (!sdInitialized) {
        return false;
    }

    String fen = chessBoard.toFEN();
    // 附加玩家颜色信息
    fen += ";isWhitePlayer:" + String(isWhitePlayer ? 1 : 0);
    File file = SD.open(CHESS_SAVE_FILE, FILE_WRITE);
    if (!file) {
        serialPrintf("[SD] Failed to open file for writing: %s\n", CHESS_SAVE_FILE);
        return false;
    }

    if (file.print(fen)) {
        serialPrintf("[SD] Board state saved to %s\n", CHESS_SAVE_FILE);
        file.close();
        return true;
    } else {
        serialPrintf("[SD] Failed to write to file: %s\n", CHESS_SAVE_FILE);
        file.close();
        return false;
    }
}

// 从SD卡加载棋盘状态
bool loadBoardState() {
    if (!sdInitialized) {
        return false;
    }

    if (!SD.exists(CHESS_SAVE_FILE)) {
        serialPrintf("[SD] Save file not found: %s\n", CHESS_SAVE_FILE);
        return false;
    }

    File file = SD.open(CHESS_SAVE_FILE, FILE_READ);
    if (!file) {
        serialPrintf("[SD] Failed to open file for reading: %s\n", CHESS_SAVE_FILE);
        return false;
    }

    String fen = file.readString();
    file.close();

    // 解析玩家颜色信息
    int semicolonIndex = fen.indexOf(';');
    String boardFen = fen;
    if (semicolonIndex != -1) {
        boardFen = fen.substring(0, semicolonIndex);
        String colorInfo = fen.substring(semicolonIndex + 1);
        if (colorInfo.startsWith("isWhitePlayer:")) {
            String colorValue = colorInfo.substring(14);
            isWhitePlayer = (colorValue == "1");
            serialPrintf("[SD] Player color loaded: %s\n", isWhitePlayer ? "White" : "Black");
        }
    }

    if (chessBoard.fromFEN(boardFen)) {
        serialPrintf("[SD] Board state loaded from %s\n", CHESS_SAVE_FILE);
        return true;
    } else {
        serialPrintf("[SD] Failed to parse FEN string: %s\n", boardFen.c_str());
        return false;
    }
}

// 显示确认对话框
bool showConfirmDialog(const String& message) {
    // 绘制背景
    canvas->fillRect(40, 40, 160, 60, canvas->color565(0, 0, 0));
    canvas->drawRect(40, 40, 160, 60, COLOR_WHITE);
    
    // 绘制消息
    canvas->setTextSize(1);
    canvas->setTextColor(COLOR_WHITE);
    canvas->setTextDatum(TC_DATUM);
    canvas->drawString(message, 120, 50);
    
    // 绘制选项
    canvas->setTextSize(1);
    canvas->drawString("OK (Y)", 80, 80);
    canvas->drawString("CANCEL (N)", 140, 80);
    
    canvas->pushSprite(0, 0);
    
    // 等待用户输入
    while (true) {
        M5Cardputer.update();
        // 直接检测按键状态，不依赖isChange()
        if (M5Cardputer.Keyboard.isKeyPressed('y') || M5Cardputer.Keyboard.isKeyPressed('Y')) {
            delay(200); // 防抖动
            return true;
        } else if (M5Cardputer.Keyboard.isKeyPressed('n') || M5Cardputer.Keyboard.isKeyPressed('N')) {
            delay(200); // 防抖动
            return false;
        }
        delay(50);
    }
}


// 显示开始界面
void showStartScreen() {
    canvas->fillScreen(COLOR_BLACK);          // 清空屏幕为黑色背景
    canvas->setTextSize(1.8f);                // 设置文本大小为1.8
    canvas->setTextColor(COLOR_WHITE);        // 设置默认文本颜色为白色
    canvas->setTextDatum(TC_DATUM);           // 设置文本对齐方式为顶部居中
    
    // 选择提示 - 居中显示在屏幕顶部（120为屏幕中心x坐标）
    canvas->drawString("Choose to start:", 120, 6);
    
    // 选项布局常量定义 - 统一控制所有选项的位置和间距
    const int OPTION_SPACING = 18;            // 各选项之间的垂直间距（像素）
    const int OPTION_TEXT_X = 120;            // 所有选项文本的x坐标（居中对齐）
    const int FIXED_ICON_X = 156;              // 图标固定x坐标（所有图标都显示在这个位置）
    const int TEXT_VERTICAL_ALIGN = 15;       // 文本和图标的垂直对齐偏移量
    const int BASE_Y_POS = 30;                 // 第一个选项的基础y坐标
    
    // 白色选项 - 第1个选项
    canvas->setTextColor(selectedOption == 0 ? COLOR_SELECTED : COLOR_WHITE);
    canvas->drawString("White", OPTION_TEXT_X, BASE_Y_POS);
    // 白色棋子图标使用固定x坐标
    int whitePawnY = BASE_Y_POS + TEXT_VERTICAL_ALIGN - PIECE_HEIGHT/2 - 5;
    canvas->pushImage(FIXED_ICON_X, whitePawnY, PIECE_WIDTH, PIECE_HEIGHT, (uint16_t*)whitePawnData, COLOR_BLACK);
    
    // 黑色选项 - 第2个选项
    canvas->setTextColor(selectedOption == 1 ? COLOR_SELECTED : COLOR_WHITE);
    canvas->drawString("Black", OPTION_TEXT_X, BASE_Y_POS + OPTION_SPACING);
    // 黑色棋子图标使用固定x坐标
    int blackPawnY = BASE_Y_POS + OPTION_SPACING + TEXT_VERTICAL_ALIGN - PIECE_HEIGHT/2 - 5;
    canvas->pushImage(FIXED_ICON_X, blackPawnY, PIECE_WIDTH, PIECE_HEIGHT, (uint16_t*)blackPawnData, COLOR_WHITE);
    
    // 随机选项 - 第3个选项
    canvas->setTextColor(selectedOption == 2 ? COLOR_SELECTED : COLOR_WHITE);
    canvas->drawString("Random", OPTION_TEXT_X, BASE_Y_POS + OPTION_SPACING * 2);
    // 随机选项的两个棋子图标使用固定x坐标位置
    //int randomY = BASE_Y_POS + OPTION_SPACING * 2 + TEXT_VERTICAL_ALIGN - PIECE_HEIGHT/4 - 3;
    // 计算两个棋子的x坐标，使它们在固定图标位置居中
    //int randomPawnX1 = FIXED_ICON_X - PIECE_WIDTH/4;
    //int randomPawnX2 = FIXED_ICON_X + PIECE_WIDTH/4;
    // 绘制左侧白色棋子（缩小一半）
    //canvas->pushImage(randomPawnX1, randomY, PIECE_WIDTH/2, PIECE_HEIGHT/2, (uint16_t*)whitePawnData, COLOR_BLACK);
    // 绘制右侧黑色棋子（缩小一半，与白色棋子相邻）
    //canvas->pushImage(randomPawnX2, randomY, PIECE_WIDTH/2, PIECE_HEIGHT/2, (uint16_t*)blackPawnData, COLOR_WHITE);
    
    // 读取存档选项 - 第4个选项
    canvas->setTextColor(selectedOption == 3 ? COLOR_SELECTED : COLOR_WHITE);
    canvas->drawString("Load", OPTION_TEXT_X, BASE_Y_POS + OPTION_SPACING * 3);
    
    // 谜题模式选项 - 第5个选项
    canvas->setTextColor(selectedOption == 4 ? COLOR_SELECTED : COLOR_WHITE);
    canvas->drawString("Puzzle", OPTION_TEXT_X, BASE_Y_POS + OPTION_SPACING * 4);
    
    // 开始提示 - 居中显示在所有选项下方（使用5倍间距）
    canvas->setTextColor(COLOR_WHITE);
    canvas->drawString(";.select|Space play", 120, BASE_Y_POS + OPTION_SPACING * 5);
    
    canvas->pushSprite(0, 0);                 // 将绘制内容显示到屏幕
}

// 显示谜题选择界面


// 绘制游戏界面
void drawGameScreen() {
    canvas->fillScreen(COLOR_BLACK);
    
    // 动态设置棋盘朝向：
    // 普通模式：玩家所执颜色在下
    // puzzle模式：白方在下（白方视角）
    bool isWhiteBottom = isPuzzleMode ? true : isWhitePlayer;
    
    // 绘制棋盘
    drawBoard(canvas, chessBoard, isWhiteBottom);
    
    // 绘制AI上一步走棋的起始位置和目标位置
    if ((aiLastMoveFrom.isValid() || aiLastMoveTo.isValid()) && chessBoard.getCurrentState() != PromotionSelecting) {
        // 绘制起始位置（背景高亮）
        if (aiLastMoveFrom.isValid()) {
            int screenX, screenY;
            boardToScreen(aiLastMoveFrom, screenX, screenY, isWhiteBottom);
            
            // 使用背景高亮，类似于升变状态选棋子的样式
            canvas->fillRect(screenX, screenY, SQUARE_SIZE, SQUARE_SIZE, COLOR_SELECTED);
            
            // 如果该位置现在有棋子，重新绘制棋子
            const Piece& piece = chessBoard.getPiece(aiLastMoveFrom);
            if (!piece.isEmpty()) {
                drawPiece(canvas, piece, screenX, screenY);
            }
        }
        
        // 绘制目标位置（背景高亮）
        if (aiLastMoveTo.isValid()) {
            int screenX, screenY;
            boardToScreen(aiLastMoveTo, screenX, screenY, isWhiteBottom);
            
            // 使用背景高亮，类似于升变状态选棋子的样式
            canvas->fillRect(screenX, screenY, SQUARE_SIZE, SQUARE_SIZE, COLOR_SELECTED);
            
            // 如果该位置现在有棋子，重新绘制棋子
            const Piece& piece = chessBoard.getPiece(aiLastMoveTo);
            if (!piece.isEmpty()) {
                drawPiece(canvas, piece, screenX, screenY);
            }
        }
    }
    
    // 绘制选中的棋子和合法移动
    Position selected = chessBoard.getSelectedPiece();
    if (selected.isValid() && chessBoard.getCurrentState() != PromotionSelecting) {
        drawSelectedPiece(canvas, selected, isWhiteBottom);
        drawValidMoves(canvas, chessBoard.getValidMoves(), isWhiteBottom);
    }
    
    // 检查是否处于升变状态
    if (chessBoard.getCurrentState() == PromotionSelecting) {
        // 绘制半透明遮罩
        canvas->fillRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, canvas->color565(0, 0, 0)); // 移除透明度参数
        
        // 获取升变兵的位置和颜色
        Position pawnPos = chessBoard.getPromotionPawnPos();
        Color color = chessBoard.getPromotionColor();
        PieceType selectedPiece = chessBoard.getSelectedPromotionPiece();
        
        // 计算升变兵在屏幕上的位置
        int pawnScreenX, pawnScreenY;
        boardToScreen(pawnPos, pawnScreenX, pawnScreenY, isWhiteBottom);
        
        // 定义可选的升变棋子类型
        std::vector<PieceType> promotionOptions = {ROOK, KNIGHT, QUEEN, BISHOP};
        
        // 计算选择网格的起始位置（以升变兵位置为中心）
        int gridWidth = 4 * SQUARE_SIZE;
        int startX = pawnScreenX - (gridWidth / 2) + (SQUARE_SIZE / 2);
        // 根据升变兵的颜色决定网格位置：白兵升变时网格在下方，黑兵升变时网格在上方
        int startY = (color == Color::WHITE) ? pawnScreenY + SQUARE_SIZE + 10 : pawnScreenY - SQUARE_SIZE - 10;
        
        // 绘制选择网格
        for (int i = 0; i < promotionOptions.size(); i++) {
            PieceType pieceType = promotionOptions[i];
            int x = startX + i * SQUARE_SIZE;
            int y = startY;
            
            // 绘制格子背景
            canvas->fillRect(x, y, SQUARE_SIZE, SQUARE_SIZE, (pieceType == selectedPiece) ? COLOR_SELECTED : COLOR_LIGHT_SQUARE);
            
            // 绘制多层边框增强光标可见性
            if (pieceType == selectedPiece) {
                // 两层黑色边框（外层）- 增加立体感
                canvas->drawRect(x - 2, y - 2, SQUARE_SIZE + 4, SQUARE_SIZE + 4, COLOR_BORDER);
                canvas->drawRect(x - 1, y - 1, SQUARE_SIZE + 2, SQUARE_SIZE + 2, COLOR_BORDER);
                // 一层选中色边框（内层）- 突出当前选中格子
                canvas->drawRect(x, y, SQUARE_SIZE, SQUARE_SIZE, COLOR_SELECTED);
            } else {
                canvas->drawRect(x, y, SQUARE_SIZE, SQUARE_SIZE, COLOR_BORDER);
            }
            
            // 绘制棋子
            Piece piece(pieceType, color);
            drawPiece(canvas, piece, x, y);
        }
    } else {
        // 绘制光标
        int screenX, screenY;
        boardToScreen(Position(cursorX, cursorY), screenX, screenY, isWhiteBottom);
        
        // 绘制多层边框增强光标可见性
        // 两层黑色边框（外层）- 增加立体感
        canvas->drawRect(screenX - 2, screenY - 2, SQUARE_SIZE + 4, SQUARE_SIZE + 4, COLOR_BLACK);
        canvas->drawRect(screenX - 1, screenY - 1, SQUARE_SIZE + 2, SQUARE_SIZE + 2, COLOR_BLACK);
        // 一层选中色边框（内层）- 突出当前选中格子
        canvas->drawRect(screenX, screenY, SQUARE_SIZE, SQUARE_SIZE, COLOR_SELECTED);
    }
    
    // 绘制回合信息
    drawTurnInfo(canvas, chessBoard.getCurrentPlayer());
    
    // 绘制光标位置的棋子信息
    drawPieceInfo(canvas);
    
    // 绘制将军信息
    if (chessBoard.isInCheck(Color::WHITE)) {
        drawCheckInfo(canvas, true, Color::WHITE);
    } else if (chessBoard.isInCheck(Color::BLACK)) {
        drawCheckInfo(canvas, true, Color::BLACK);
    }
    
    // 在屏幕左上角添加操作提示
    if (isPuzzleMode) {
        canvas->setTextColor(COLOR_WHITE, COLOR_BLACK);
        canvas->setTextSize(1);
        canvas->drawString("ESC:reset", 28, 7);
        canvas->drawString("TAB:tip", 22, 19);
    }
    
    canvas->pushSprite(0, 0);
}

// 处理按键输入
void handleKeyInput() {
    if (M5Cardputer.Keyboard.isChange()) {
        // 防抖动
        if (millis() - lastKeyPressTime < DEBOUNCE_DELAY) {
            return;
        }
        lastKeyPressTime = millis();
        
        if (!isGameStarted) {
            if (isPuzzleMode) {
                // 谜题选择界面的按键处理
                if (M5Cardputer.Keyboard.isKeyPressed(' ')) {
                    // 开始选择的谜题
                    const Puzzle& selectedPuzzle = puzzles[currentPuzzleIndex];
                    // 加载谜题初始局面
                    chessBoard.fromFEN(selectedPuzzle.getFEN());
                    // 根据谜题的当前走棋方设置玩家颜色
                    isWhitePlayer = (selectedPuzzle.getSideToMove() == Color::WHITE);
                    isGameStarted = true;
                    currentMoveIndex = 0;
                    // 重绘游戏界面
                    drawGameScreen();
                    return;
                } else if (M5Cardputer.Keyboard.isKeyPressed(';')) {
                    // 上箭头 - 选择上一个谜题
                    currentPuzzleIndex = (currentPuzzleIndex - 1 + puzzles.size()) % puzzles.size();
                    currentPuzzle = puzzles[currentPuzzleIndex];
                    chessBoard.fromFEN(currentPuzzle.getFEN());
                    currentMoveIndex = 0;
                    drawGameScreen();
                } else if (M5Cardputer.Keyboard.isKeyPressed('.')) {
                    // 下箭头 - 选择下一个谜题
                    currentPuzzleIndex = (currentPuzzleIndex + 1) % puzzles.size();
                    currentPuzzle = puzzles[currentPuzzleIndex];
                    chessBoard.fromFEN(currentPuzzle.getFEN());
                    currentMoveIndex = 0;
                    drawGameScreen();
                } else if (M5Cardputer.Keyboard.isKeyPressed('B')) {
                    // B键返回主菜单
                    isPuzzleMode = false;
                    showStartScreen();
                }
            } else {
                // 开始界面的按键处理
                if (M5Cardputer.Keyboard.isKeyPressed(' ')) {
                    // 根据选中的选项执行相应操作
                    if (selectedOption == 0) {
                        // 选择白色
                        isWhitePlayer = true;
                    } else if (selectedOption == 1) {
                        // 选择黑色
                        isWhitePlayer = false;
                    } else if (selectedOption == 2) {
                        // 随机选择颜色
                        isWhitePlayer = random(0, 2);
                    } else if (selectedOption == 3) {
                        // 读取存档 - 首先初始化SD卡
                        if (!initializeSDCard()) {
                            // SD卡初始化失败，显示错误信息
                            canvas->fillScreen(COLOR_BLACK);
                            canvas->setTextColor(COLOR_WHITE);
                            canvas->setTextSize(1);
                            canvas->setTextDatum(CC_DATUM);
                            canvas->drawString("SDCard not found", 120, 60);
                            canvas->drawString("Press any key to return", 120, 80);
                            canvas->pushSprite(0, 0);
                            
                            // 首先等待所有按键释放，清除当前按键状态
                            bool anyKeyPressed = true;
                            while (anyKeyPressed) {
                                M5Cardputer.update();
                                anyKeyPressed = false;
                                
                                // 检查字母键
                                for (int i = 0; i < 26; i++) {
                                    if (M5Cardputer.Keyboard.isKeyPressed('a' + i) || M5Cardputer.Keyboard.isKeyPressed('A' + i)) {
                                        anyKeyPressed = true;
                                        break;
                                    }
                                }
                                if (anyKeyPressed) { delay(50); continue; }
                                
                                // 检查数字键
                                for (int i = 0; i < 10; i++) {
                                    if (M5Cardputer.Keyboard.isKeyPressed('0' + i)) {
                                        anyKeyPressed = true;
                                        break;
                                    }
                                }
                                if (anyKeyPressed) { delay(50); continue; }
                                
                                // 检查特殊键
                                if (M5Cardputer.Keyboard.isKeyPressed(' ') || 
                                    M5Cardputer.Keyboard.isKeyPressed(KEY_BACKSPACE) ||
                                    M5Cardputer.Keyboard.isKeyPressed(KEY_ENTER) ||
                                    M5Cardputer.Keyboard.isKeyPressed(';') ||
                                    M5Cardputer.Keyboard.isKeyPressed('.') ||
                                    M5Cardputer.Keyboard.isKeyPressed(',') ||
                                    M5Cardputer.Keyboard.isKeyPressed('/') ||
                                    M5Cardputer.Keyboard.isKeyPressed('`')) {
                                    anyKeyPressed = true;
                                }
                                
                                if (anyKeyPressed) {
                                    delay(50);
                                }
                            }
                            
                            // 然后等待用户按下任意键
                            while (true) {
                                M5Cardputer.update();
                                if (M5Cardputer.Keyboard.isKeyPressed(' ') || 
                                    M5Cardputer.Keyboard.isKeyPressed('\n') || 
                                    M5Cardputer.Keyboard.isKeyPressed(KEY_BACKSPACE) ||
                                    M5Cardputer.Keyboard.isKeyPressed(KEY_ENTER) ||
                                    M5Cardputer.Keyboard.isKeyPressed(';') ||
                                    M5Cardputer.Keyboard.isKeyPressed('.') ||
                                    M5Cardputer.Keyboard.isKeyPressed(',') ||
                                    M5Cardputer.Keyboard.isKeyPressed('/') ||
                                    M5Cardputer.Keyboard.isKeyPressed('`')) {
                                    showStartScreen();
                                    return;
                                }
                                delay(50);
                            }
                        }
                        
                        // SD卡初始化成功，检查存档是否存在
                        if (SD.exists(CHESS_SAVE_FILE)) {
                            if (loadBoardState()) {
                                isGameStarted = true;
                                // 加载后重设AI走棋记录
                                aiLastMoveFrom = Position(-1, -1);
                                aiLastMoveTo = Position(-1, -1);
                                // 重绘游戏界面
                                drawGameScreen();
                                // 如果现在是AI的回合，AI需要走棋
                                if (chessBoard.getCurrentPlayer() != (isWhitePlayer ? Color::WHITE : Color::BLACK)) {
                                    delay(500); // 短暂延迟
                                    Color aiColor = chessBoard.getCurrentPlayer();
                                    Move aiMove = chooseAIMove(aiColor, chessBoard);
                                    if (aiMove.from.isValid() && aiMove.to.isValid()) {
                                        // 记录AI走棋的起始位置和目标位置
                                        aiLastMoveFrom = aiMove.from;
                                        aiLastMoveTo = aiMove.to;
                                        chessBoard.movePiece(aiMove.from, aiMove.to);
                                        // 保存AI移动后的棋盘状态
                                        saveBoardState();
                                        // 重绘游戏界面
                                        drawGameScreen();
                                    }
                                }
                                return;
                            } else {
                                // 加载失败，显示错误信息
                                canvas->fillScreen(COLOR_BLACK);
                                canvas->setTextColor(COLOR_WHITE);
                                canvas->setTextSize(1);
                                canvas->setTextDatum(CC_DATUM);
                                canvas->drawString("Failed to load game", 120, 60);
                                canvas->drawString("Press any key to return", 120, 80);
                                canvas->pushSprite(0, 0);
                                
                                // 首先等待所有按键释放，清除当前按键状态
                                bool anyKeyPressed = true;
                                while (anyKeyPressed) {
                                    M5Cardputer.update();
                                    anyKeyPressed = false;
                                    
                                    // 检查字母键
                                    for (int i = 0; i < 26; i++) {
                                        if (M5Cardputer.Keyboard.isKeyPressed('a' + i) || M5Cardputer.Keyboard.isKeyPressed('A' + i)) {
                                            anyKeyPressed = true;
                                            break;
                                        }
                                    }
                                    if (anyKeyPressed) { delay(50); continue; }
                                    
                                    // 检查数字键
                                    for (int i = 0; i < 10; i++) {
                                        if (M5Cardputer.Keyboard.isKeyPressed('0' + i)) {
                                            anyKeyPressed = true;
                                            break;
                                        }
                                    }
                                    if (anyKeyPressed) { delay(50); continue; }
                                    
                                    // 检查特殊键
                                    if (M5Cardputer.Keyboard.isKeyPressed(' ') || 
                                        M5Cardputer.Keyboard.isKeyPressed(KEY_BACKSPACE) ||
                                        M5Cardputer.Keyboard.isKeyPressed(KEY_ENTER) ||
                                        M5Cardputer.Keyboard.isKeyPressed(';') ||
                                        M5Cardputer.Keyboard.isKeyPressed('.') ||
                                        M5Cardputer.Keyboard.isKeyPressed(',') ||
                                        M5Cardputer.Keyboard.isKeyPressed('/') ||
                                        M5Cardputer.Keyboard.isKeyPressed('`')) {
                                        anyKeyPressed = true;
                                    }
                                    
                                    if (anyKeyPressed) {
                                        delay(50);
                                    }
                                }
                                
                                // 然后等待用户按下任意键
                                while (true) {
                                    M5Cardputer.update();
                                    
                                    // 检查字母键
                                    for (int i = 0; i < 26; i++) {
                                        if (M5Cardputer.Keyboard.isKeyPressed('a' + i) || M5Cardputer.Keyboard.isKeyPressed('A' + i)) {
                                            showStartScreen();
                                            return;
                                        }
                                    }
                                    
                                    // 检查数字键
                                    for (int i = 0; i < 10; i++) {
                                        if (M5Cardputer.Keyboard.isKeyPressed('0' + i)) {
                                            showStartScreen();
                                            return;
                                        }
                                    }
                                    
                                    // 检查特殊键
                                    if (M5Cardputer.Keyboard.isKeyPressed(' ') || 
                                        M5Cardputer.Keyboard.isKeyPressed('\n') || 
                                        M5Cardputer.Keyboard.isKeyPressed(KEY_BACKSPACE) ||
                                        M5Cardputer.Keyboard.isKeyPressed(KEY_ENTER) ||
                                        M5Cardputer.Keyboard.isKeyPressed(';') ||
                                        M5Cardputer.Keyboard.isKeyPressed('.') ||
                                        M5Cardputer.Keyboard.isKeyPressed(',') ||
                                        M5Cardputer.Keyboard.isKeyPressed('/') ||
                                        M5Cardputer.Keyboard.isKeyPressed('`')) {
                                        showStartScreen();
                                        return;
                                    }
                                    
                                    delay(50);
                                }
                            }
                        } else {
                            // 没有存档文件，显示错误信息
                            canvas->fillScreen(COLOR_BLACK);
                            canvas->setTextColor(COLOR_WHITE);
                            canvas->setTextSize(1);
                            canvas->setTextDatum(CC_DATUM);
                            canvas->drawString("No saved game", 120, 60);
                            canvas->drawString("Press any key to return", 120, 80);
                            canvas->pushSprite(0, 0);
                            
                            // 首先等待所有按键释放，清除当前按键状态
                            bool anyKeyPressed = true;
                            while (anyKeyPressed) {
                                M5Cardputer.update();
                                anyKeyPressed = false;
                                
                                // 检查字母键
                                for (int i = 0; i < 26; i++) {
                                    if (M5Cardputer.Keyboard.isKeyPressed('a' + i) || M5Cardputer.Keyboard.isKeyPressed('A' + i)) {
                                        anyKeyPressed = true;
                                        break;
                                    }
                                }
                                if (anyKeyPressed) { delay(50); continue; }
                                
                                // 检查数字键
                                for (int i = 0; i < 10; i++) {
                                    if (M5Cardputer.Keyboard.isKeyPressed('0' + i)) {
                                        anyKeyPressed = true;
                                        break;
                                    }
                                }
                                if (anyKeyPressed) { delay(50); continue; }
                                
                                // 检查特殊键
                                if (M5Cardputer.Keyboard.isKeyPressed(' ') || 
                                    M5Cardputer.Keyboard.isKeyPressed(KEY_BACKSPACE) ||
                                    M5Cardputer.Keyboard.isKeyPressed(KEY_ENTER) ||
                                    M5Cardputer.Keyboard.isKeyPressed(';') ||
                                    M5Cardputer.Keyboard.isKeyPressed('.') ||
                                    M5Cardputer.Keyboard.isKeyPressed(',') ||
                                    M5Cardputer.Keyboard.isKeyPressed('/') ||
                                    M5Cardputer.Keyboard.isKeyPressed('`')) {
                                    anyKeyPressed = true;
                                }
                                
                                if (anyKeyPressed) {
                                    delay(50);
                                }
                            }
                            
                            // 然后等待用户按下任意键
                            while (true) {
                                M5Cardputer.update();
                                
                                // 检查字母键
                                for (int i = 0; i < 26; i++) {
                                    if (M5Cardputer.Keyboard.isKeyPressed('a' + i) || M5Cardputer.Keyboard.isKeyPressed('A' + i)) {
                                        showStartScreen();
                                        return;
                                    }
                                }
                                
                                // 检查数字键
                                for (int i = 0; i < 10; i++) {
                                    if (M5Cardputer.Keyboard.isKeyPressed('0' + i)) {
                                        showStartScreen();
                                        return;
                                    }
                                }
                                
                                // 检查特殊键
                                if (M5Cardputer.Keyboard.isKeyPressed(' ') || 
                                    M5Cardputer.Keyboard.isKeyPressed('\n') || 
                                    M5Cardputer.Keyboard.isKeyPressed(KEY_BACKSPACE) ||
                                    M5Cardputer.Keyboard.isKeyPressed(KEY_ENTER) ||
                                    M5Cardputer.Keyboard.isKeyPressed(';') ||
                                    M5Cardputer.Keyboard.isKeyPressed('.') ||
                                    M5Cardputer.Keyboard.isKeyPressed(',') ||
                                    M5Cardputer.Keyboard.isKeyPressed('/') ||
                                    M5Cardputer.Keyboard.isKeyPressed('`')) {
                                    showStartScreen();
                                    return;
                                }
                                
                                delay(50);
                            }
                        }
                    } else if (selectedOption == 4) {
                        // 谜题模式
                        isPuzzleMode = true;
                        // 加载谜题
                        puzzles = Puzzle::loadPuzzles("");
                        if (puzzles.empty()) {
                            // 谜题加载失败，返回主菜单
                            isPuzzleMode = false;
                            showStartScreen();
                            return;
                        }
                        // 随机选择一个谜题
                        currentPuzzleIndex = random(0, puzzles.size());
                        currentPuzzle = puzzles[currentPuzzleIndex];
                        
                        // 开始新游戏
                        isGameStarted = true;
                        // 从FEN加载棋盘
                        chessBoard.fromFEN(currentPuzzle.getFEN());
                        // 根据谜题的当前走棋方设置玩家颜色
                        isWhitePlayer = (currentPuzzle.getSideToMove() == Color::WHITE);
                        // 重置光标位置
                        if (!isWhitePlayer) {
                            cursorX = 0;
                            cursorY = 7;
                        } else {
                            cursorX = 0;
                            cursorY = 0;
                        }
                        // 重置AI走棋记录
                        aiLastMoveFrom = Position(-1, -1);
                        aiLastMoveTo = Position(-1, -1);
                        // 重置当前走法索引
                        currentMoveIndex = 0;
                        
                        // 重绘游戏界面
                        drawGameScreen();
                        return;
                    }
                    
                    // 开始新游戏
                    isGameStarted = true;
                    // 初始化棋盘
                    chessBoard.initBoard();
                    // 重置AI走棋记录
                    aiLastMoveFrom = Position(-1, -1);
                    aiLastMoveTo = Position(-1, -1);
                    // 设置光标初始位置
                    if (!isWhitePlayer) {
                        cursorX = 0;
                        cursorY = 7;
                    } else {
                        cursorX = 0;
                        cursorY = 0;
                    }
                    
                    // 重绘游戏界面
                    drawGameScreen();
                    
                    // 如果玩家选择黑方，AI（白方）需要先走棋
                    if (!isWhitePlayer) {
                        delay(500); // 短暂延迟
                        Color aiColor = Color::WHITE;
                        Move aiMove = chooseAIMove(aiColor, chessBoard);
                        if (aiMove.from.isValid() && aiMove.to.isValid()) {
                            // 记录AI走棋的起始位置和目标位置
                            aiLastMoveFrom = aiMove.from;
                            aiLastMoveTo = aiMove.to;
                            chessBoard.movePiece(aiMove.from, aiMove.to);
                            // 保存AI移动后的棋盘状态
                            saveBoardState();
                            // 重绘游戏界面
                            drawGameScreen();
                        }
                    }
                } else if (M5Cardputer.Keyboard.isKeyPressed(';')) {
                    // 上箭头 - 选择上一个选项
                    selectedOption = (selectedOption - 1 + 5) % 5;
                    showStartScreen();
                } else if (M5Cardputer.Keyboard.isKeyPressed('.')) {
                    // 下箭头 - 选择下一个选项
                    selectedOption = (selectedOption + 1) % 5;
                    showStartScreen();
                }
            }
        } else {
                // 游戏界面的按键处理
                
                // 检查是否处于升变状态
                if (chessBoard.getCurrentState() == PromotionSelecting) {
                    // 升变状态下的按键处理
                    if (M5Cardputer.Keyboard.isKeyPressed(',')) {
                        // 左箭头 - 向左选择
                        chessBoard.navigatePromotionSelection(-1);
                    } else if (M5Cardputer.Keyboard.isKeyPressed('/')) {
                        // 右箭头 - 向右选择
                        chessBoard.navigatePromotionSelection(1);
                    } else if (M5Cardputer.Keyboard.isKeyPressed(' ')) {
                        // 空格键 - 确认选择
                        chessBoard.confirmPromotion();
                    }
                } else {
                    // 正常游戏状态下的按键处理
                    if (M5Cardputer.Keyboard.isKeyPressed('`')) {
                        // ESC键处理
                        if (isPuzzleMode) {
                            // 谜题模式：直接重置为当前谜题的初始状态
                            const Puzzle& selectedPuzzle = puzzles[currentPuzzleIndex];
                            // 加载谜题初始局面
                            chessBoard.fromFEN(selectedPuzzle.getFEN());
                            // 重置当前走法索引
                            currentMoveIndex = 0;
                            // 重置AI走棋记录
                            aiLastMoveFrom = Position(-1, -1);
                            aiLastMoveTo = Position(-1, -1);
                            // 重置光标位置
                            // 谜题模式下白棋永远在下方，所以cursorX和cursorY的计算基于白棋视角
                            bool isWhitePerspective = isPuzzleMode ? true : isWhitePlayer;
                            if (!isWhitePerspective) {
                                cursorX = 0;
                                cursorY = 7;
                            } else {
                                cursorX = 0;
                                cursorY = 0;
                            }
                        } else {
                            // 正常游戏模式：显示重置棋盘确认对话框
                            if (showConfirmDialog("Reset board?")) {
                                // 重置棋盘
                                chessBoard.initBoard();
                                // 重置AI走棋记录
                                aiLastMoveFrom = Position(-1, -1);
                                aiLastMoveTo = Position(-1, -1);
                                // 重置光标位置
                                if (!isWhitePlayer) {
                                    cursorX = 0;
                                    cursorY = 7;
                                } else {
                                    cursorX = 0;
                                    cursorY = 0;
                                }
                                // 保存重置后的棋盘状态
                                saveBoardState();
                            }
                        }
                    } else if (M5Cardputer.Keyboard.isKeyPressed(';')) {
                        // 上
                        // 根据棋盘朝向调整光标移动方向
                        bool isWhiteBottom = isPuzzleMode ? true : isWhitePlayer;
                        if (isWhiteBottom) {
                            cursorY = (cursorY + 1) % 8;
                        } else {
                            cursorY = (cursorY - 1 + 8) % 8;
                        }
                    } else if (M5Cardputer.Keyboard.isKeyPressed('.')) {
                        // 下
                        // 根据棋盘朝向调整光标移动方向
                        bool isWhiteBottom = isPuzzleMode ? true : isWhitePlayer;
                        if (isWhiteBottom) {
                            cursorY = (cursorY - 1 + 8) % 8;
                        } else {
                            cursorY = (cursorY + 1) % 8;
                        }
                    } else if (M5Cardputer.Keyboard.isKeyPressed(',')) {
                        // 左
                        // 根据棋盘朝向调整光标移动方向
                        bool isWhiteBottom = isPuzzleMode ? true : isWhitePlayer;
                        if (isWhiteBottom) {
                            cursorX = (cursorX - 1 + 8) % 8;
                        } else {
                            cursorX = (cursorX + 1) % 8;
                        }
                    } else if (M5Cardputer.Keyboard.isKeyPressed('/')) {
                        // 右
                        // 根据棋盘朝向调整光标移动方向
                        bool isWhiteBottom = isPuzzleMode ? true : isWhitePlayer;
                        if (isWhiteBottom) {
                            cursorX = (cursorX + 1) % 8;
                        } else {
                            cursorX = (cursorX - 1 + 8) % 8;
                        }
                    } else if (M5Cardputer.Keyboard.isKeyPressed(KEY_TAB)) {
                        // TAB键 - 显示提示
                        if (isPuzzleMode) {
                            const Puzzle& localPuzzle = puzzles[currentPuzzleIndex];
                            const std::vector<Move>& mainLine = localPuzzle.getMainLine();
                            
                            // 检查是否还有下一步移动
                            if (currentMoveIndex < mainLine.size()) {
                                // 获取正确的下一步移动
                                const Move& correctMove = mainLine[currentMoveIndex];
                                
                                // 高亮显示正确的移动
                                std::vector<Position> tipMoves;
                                tipMoves.push_back(correctMove.to);
                                
                                // 保存当前选中状态
                                Position savedSelected = chessBoard.getSelectedPiece();
                                chessBoard.deselectPiece();
                                
                                // 重新绘制游戏界面
                                drawGameScreen();
                                
                                // 高亮显示正确的目标位置
                                int screenX, screenY;
                                // 使用与drawGameScreen相同的逻辑来确定棋盘朝向
                                bool isWhiteBottom = isPuzzleMode ? true : isWhitePlayer;
                                boardToScreen(correctMove.from, screenX, screenY, isWhiteBottom);
                                canvas->fillRect(screenX, screenY, SQUARE_SIZE, SQUARE_SIZE, COLOR_VALID_MOVE);
                                // 重新绘制起始位置的棋子
                                const Piece& piece = chessBoard.getPiece(correctMove.from);
                                if (!piece.isEmpty()) {
                                    drawPiece(canvas, piece, screenX, screenY);
                                }
                                
                                // 高亮显示目标位置
                                boardToScreen(correctMove.to, screenX, screenY, isWhiteBottom);
                                canvas->fillRect(screenX, screenY, SQUARE_SIZE, SQUARE_SIZE, COLOR_VALID_MOVE);
                                
                                // 如果目标位置有棋子，重新绘制
                                const Piece& targetPiece = chessBoard.getPiece(correctMove.to);
                                if (!targetPiece.isEmpty()) {
                                    drawPiece(canvas, targetPiece, screenX, screenY);
                                }
                                
                                canvas->pushSprite(0, 0);
                                
                                // 等待玩家按下任何键
                                while (true) {
                                    M5Cardputer.update();
                                    M5Cardputer.Keyboard.updateKeyList();
                                    if (M5Cardputer.Keyboard.isPressed() > 0) {
                                        break;
                                    }
                                }
                                
                                // 恢复选中状态
                                if (savedSelected.isValid()) {
                                    chessBoard.selectPiece(savedSelected);
                                }
                                
                                // 重新绘制游戏界面
                                drawGameScreen();
                            }
                        }
                    } else if (M5Cardputer.Keyboard.isKeyPressed(' ')) {
                            // 选择/落子
                            Position currentPos(cursorX, cursorY);
                            
                            if (chessBoard.getSelectedPiece().isValid()) {
                                // 尝试移动棋子
                            Position fromPos = chessBoard.getSelectedPiece();
                            if (chessBoard.movePiece(fromPos, currentPos)) {
                                // 移动成功，switchPlayer is already called inside movePiece
                                // 取消选中（虽然movePiece也会调用deselectPiece，但确保双重保险）
                                chessBoard.deselectPiece();
                                
                                // 谜题模式特殊处理
                                if (isPuzzleMode) {
                                    const Puzzle& localPuzzle = puzzles[currentPuzzleIndex];
                                    const std::vector<Move>& mainLine = localPuzzle.getMainLine();
                                    
                                    // 检查当前走法是否与正解序列匹配
                                    Move playerMove(fromPos, currentPos);
                                    if (currentMoveIndex < mainLine.size() && playerMove == mainLine[currentMoveIndex]) {
                                        // 走法正确，更新走法索引
                                        currentMoveIndex++;
                                        
                                        // 检查是否完成谜题
                                        if (currentMoveIndex >= mainLine.size()) {
                                            // 谜题完成，显示完成信息
                                            canvas->fillScreen(COLOR_BLACK);
                                            canvas->setTextSize(2);
                                            canvas->setTextColor(COLOR_WHITE);
                                            canvas->setTextDatum(CC_DATUM);
                                            canvas->drawString("Congratulations!", 120, 40);
                                            canvas->drawString("Puzzle Completed!", 120, 65);
                                            
                                            // 显示选项
                                            canvas->setTextSize(1);
                                            canvas->drawString("R:Retry", 80, 100);
                                            canvas->drawString("N:Next Puzzle", 140, 100);
                                            canvas->drawString("M:Main Menu", 100, 120);
                                            canvas->pushSprite(0, 0);
                                            
                                            // 等待用户输入
                                            while (true) {
                                                M5Cardputer.update();
                                                if (M5Cardputer.Keyboard.isKeyPressed('r') || M5Cardputer.Keyboard.isKeyPressed('R')) {
                                                    // 重试当前谜题
                                                    chessBoard.fromFEN(currentPuzzle.getFEN());
                                                    currentMoveIndex = 0;
                                                    drawGameScreen();
                                                    return;
                                                } else if (M5Cardputer.Keyboard.isKeyPressed('n') || M5Cardputer.Keyboard.isKeyPressed('N')) {
                                                    // 下一个谜题
                                                    currentPuzzleIndex = (currentPuzzleIndex + 1) % puzzles.size();
                                                    currentPuzzle = puzzles[currentPuzzleIndex];
                                                    chessBoard.fromFEN(currentPuzzle.getFEN());
                                                    // 谜题模式下白棋永远在下方，所以isWhitePlayer始终为true
                                                    isWhitePlayer = true;
                                                    currentMoveIndex = 0;
                                                    // 重置光标位置
                                                    if (!isWhitePlayer) {
                                                        cursorX = 0;
                                                        cursorY = 7;
                                                    } else {
                                                        cursorX = 0;
                                                        cursorY = 0;
                                                    }
                                                    drawGameScreen();
                                                    return;
                                                } else if (M5Cardputer.Keyboard.isKeyPressed('m') || M5Cardputer.Keyboard.isKeyPressed('M')) {
                                                    // 回到主菜单
                                                    isGameStarted = false;
                                                    isPuzzleMode = false;
                                                    currentMoveIndex = 0;
                                                    showStartScreen();
                                                    return;
                                                }
                                                delay(50);
                                            }
                                        }
                                        
                                        // 如果还有更多走法，检查是否轮到谜题的下一个走法
                                        if (currentMoveIndex < mainLine.size()) {
                                            // 自动执行对手的走法（如果有的话）
                                            if (currentMoveIndex % 2 != 0) {
                                                // 延迟一下，让玩家看清楚
                                                delay(500);
                                                
                                                Move nextMove = mainLine[currentMoveIndex];
                                                chessBoard.movePiece(nextMove.from, nextMove.to);
                                                aiLastMoveFrom = nextMove.from;
                                                aiLastMoveTo = nextMove.to;
                                                currentMoveIndex++;
                                            }
                                        }
                                    } else {
                                    // 走法错误，撤销当前移动并恢复棋盘状态
                                    chessBoard.undoMove();
                                    
                                    // 显示错误信息
                                    canvas->setTextColor(COLOR_WHITE, COLOR_BLACK);
                                    canvas->setTextSize(1);
                                    const char* wrongMoveText = "Wrong Move!";
                                    int textWidth = canvas->textWidth(wrongMoveText);
                                    int textX = canvas->width() - textWidth - 10;
                                    int textY = 1;
                                    canvas->drawString(wrongMoveText, textX, textY);
                                    canvas->pushSprite(0, 0);
                                    
                                    // 两阶段按键处理：
                                    // 1. 首先等待所有按键释放，清除当前按键状态
                                    bool anyKeyPressed = true;
                                    while (anyKeyPressed) {
                                        M5Cardputer.update();
                                        anyKeyPressed = false;
                                        
                                        // 检查字母键
                                        for (int i = 0; i < 26; i++) {
                                            if (M5Cardputer.Keyboard.isKeyPressed('a' + i) || M5Cardputer.Keyboard.isKeyPressed('A' + i)) {
                                                anyKeyPressed = true;
                                                break;
                                            }
                                        }
                                        if (anyKeyPressed) { delay(50); continue; }
                                        
                                        // 检查数字键
                                        for (int i = 0; i < 10; i++) {
                                            if (M5Cardputer.Keyboard.isKeyPressed('0' + i)) {
                                                anyKeyPressed = true;
                                                break;
                                            }
                                        }
                                        if (anyKeyPressed) { delay(50); continue; }
                                        
                                        // 检查特殊键
                                        if (M5Cardputer.Keyboard.isKeyPressed(' ') || 
                                            M5Cardputer.Keyboard.isKeyPressed(KEY_BACKSPACE) ||
                                            M5Cardputer.Keyboard.isKeyPressed(KEY_ENTER) ||
                                            M5Cardputer.Keyboard.isKeyPressed(';') ||
                                            M5Cardputer.Keyboard.isKeyPressed('.') ||
                                            M5Cardputer.Keyboard.isKeyPressed(',') ||
                                            M5Cardputer.Keyboard.isKeyPressed('/') ||
                                            M5Cardputer.Keyboard.isKeyPressed('`')) {
                                            anyKeyPressed = true;
                                        }
                                        
                                        if (anyKeyPressed) delay(50);
                                    }
                                    
                                    // 2. 然后等待用户按下新的按键
                                    bool keyPressed = false;
                                    while (!keyPressed) {
                                        M5Cardputer.update(); // 更新输入状态
                                        // 检查任意按键是否被按下
                                        for (int i = 0; i < 26; i++) {
                                            if (M5Cardputer.Keyboard.isKeyPressed('a' + i) || M5Cardputer.Keyboard.isKeyPressed('A' + i)) {
                                                keyPressed = true;
                                                break;
                                            }
                                        }
                                        if (keyPressed) break;
                                        
                                        // 检查数字键
                                        for (int i = 0; i < 10; i++) {
                                            if (M5Cardputer.Keyboard.isKeyPressed('0' + i)) {
                                                keyPressed = true;
                                                break;
                                            }
                                        }
                                        if (keyPressed) break;
                                        
                                        // 检查特殊键
                                        if (M5Cardputer.Keyboard.isKeyPressed(' ') || 
                                            M5Cardputer.Keyboard.isKeyPressed(KEY_BACKSPACE) ||
                                            M5Cardputer.Keyboard.isKeyPressed(KEY_ENTER) ||
                                            M5Cardputer.Keyboard.isKeyPressed(';') ||
                                            M5Cardputer.Keyboard.isKeyPressed('.') ||
                                            M5Cardputer.Keyboard.isKeyPressed(',') ||
                                            M5Cardputer.Keyboard.isKeyPressed('/') ||
                                            M5Cardputer.Keyboard.isKeyPressed('`')) {
                                            keyPressed = true;
                                        }
                                        
                                        delay(50);
                                    }
                                    
                                    // 消除错误信息，重绘游戏界面
                                    drawGameScreen();
                                    }
                                } else {
                                    // 正常游戏模式
                                    // 保存玩家移动后的棋盘状态
                                    saveBoardState();
                                      
                                    // 立即重绘游戏界面，显示玩家的落子效果
                                    drawGameScreen();
                                    
                                    // 检查是否轮到AI走棋
                                    Color aiColor = isWhitePlayer ? Color::BLACK : Color::WHITE;
                                    if (chessBoard.getCurrentPlayer() == aiColor) {
                                        // AI走棋
                                        Move aiMove = chooseAIMove(aiColor, chessBoard);
                                        if (aiMove.from.isValid() && aiMove.to.isValid()) {
                                            // 记录AI走棋的起始位置和目标位置
                                            aiLastMoveFrom = aiMove.from;
                                            aiLastMoveTo = aiMove.to;
                                            chessBoard.movePiece(aiMove.from, aiMove.to);
                                            // 保存AI移动后的棋盘状态
                                            saveBoardState();
                                            
                                            // AI走棋后重绘游戏界面
                                            drawGameScreen();
                                        }
                                    }
                                }
                            } else {
                                    // 移动失败，尝试选择新的棋子
                                    chessBoard.selectPiece(currentPos);
                                }
                            } else {
                                // 选择棋子
                                chessBoard.selectPiece(currentPos);
                            }
                        }
                }
            
            // 重绘游戏界面
            drawGameScreen();
        }
    }
}

void setup() {
    // 初始化M5Cardputer
    M5Cardputer.begin();
    // 初始化串口
    Serial.begin(115200);
    delay(500);
    M5Cardputer.Display.init();
    M5Cardputer.Display.setRotation(1);
    M5Cardputer.Keyboard.begin();
    
    // 创建画布
    canvas = new M5Canvas(&M5Cardputer.Display);
    canvas->createSprite(M5Cardputer.Display.width(), M5Cardputer.Display.height());
    canvas->setTextDatum(TC_DATUM);
    
    // 显示开始界面
    showStartScreen();
    
    // 测试串口输出
    Serial.println("Chess app started!");
}

void loop() {
    // 更新M5Cardputer
    M5Cardputer.update();
    
    // 处理按键输入
    handleKeyInput();
}