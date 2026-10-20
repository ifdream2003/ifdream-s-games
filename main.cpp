#include <graphics.h>
#include <iostream>
#include <vector>
#include <map>
#include <string>
#include <windows.h>
#include <stack>
#include "Button.h"
#pragma comment(lib, "MSIMG32.LIB")

using namespace std;

const int BOARDSIZE = 19;//棋盘大小
int board[BOARDSIZE][BOARDSIZE] = { 0 }; // 0空，1黑，2白
int currentPlayer = 1;   // 1黑，2白
const int TOP_N = 5;//计算分支数量
const int dep = 3;//递归深度
IMAGE white, black, menu, back, buffer(808, 608);
// 落子记录
stack<pair<int, int>> moveHistory = {};

Button undoButton = { 708, 608 - 50, 90, 30, L"悔棋", RGB(255,255,255),RGB(0,0,0)};
// 函数声明
vector<string> getChessLines(int x, int y, int tempboard[BOARDSIZE][BOARDSIZE] = board);
vector<string> getLines(int x, int y, int tempboard[BOARDSIZE][BOARDSIZE] = board);
bool isValid(int x, int y, int tempboard[BOARDSIZE][BOARDSIZE] = board);
void PVCWindow();
void PVPWindow();

inline void putimage_alpha(int x, int y, IMAGE* img, IMAGE* dest = NULL) {
    HDC hdcDest = dest ? GetImageHDC(dest) : GetImageHDC(NULL);
    HDC hdcSrc = GetImageHDC(img);

    int w = img->getwidth();
    int h = img->getheight();

    AlphaBlend(
        hdcDest, x, y, w, h,
        hdcSrc, 0, 0, w, h,
        { AC_SRC_OVER, 0, 255, AC_SRC_ALPHA }
    );
}

//记录一步棋
void recordMove(pair<int, int> location) {
    std::cout << "Recording move at: (" << location.first << ", " << location.second << ")\n";
    moveHistory.push(location);
}

// 清空所有历史记录
void clearHistory() {
    std::cout << "Clearing move history.\n";
    moveHistory = {};
}

// 检查是否可以悔棋
bool canUndo() {
    return !moveHistory.empty();
}

// 悔棋功能
void undoMove(int state) {
    // 首先检查是否有历史记录可以撤销
    if (!canUndo()) {
        std::cout << "Cannot undo. No moves have been made.\n";
        return; // 什么都不做，直接返回
    }
    pair<int, int> locate;
    if (state == 1) {
        if (moveHistory.size() < 4) {//小于4，说明玩家回退的是第一步
            for (int i = 0; i < 3; i++) {
                locate = moveHistory.top();
                board[locate.second][locate.first] = 0;
                moveHistory.pop();
            }
        }
        else {//大于4的处理方式
            for (int i = 0; i < 4; i++) {
                locate = moveHistory.top();
                board[locate.second][locate.first] = 0;
                moveHistory.pop();
            }
        }
    }
    if (state == 2) {
        if (moveHistory.size() < 2) {//小于2，说明玩家回退的是第一步
            locate = moveHistory.top();
            board[locate.second][locate.first] = 0;
            moveHistory.pop();
        }
        else {//大于2的处理方式
            for (int i = 0; i < 2; i++) {
                locate = moveHistory.top();
                board[locate.second][locate.first] = 0;
                moveHistory.pop();
            }
        }
    }
}

// 主菜单
int MainMenu() {
    putimage(0, 0, &menu);

    //需要一个“连六棋”大字标题
    settextcolor(RGB(0, 0, 0));
    setbkmode(TRANSPARENT);
    settextstyle(80, 0, L"隶书");
    outtextxy(248, 100, L"连 六 棋");

    Button btnPVP = { 300, 250, 200, 50, L"双人对战", RGB(208,208,208),RGB(0,0,0)};
    Button btnPVC = { 300, 320, 200, 50, L"人机对战", RGB(208,208,208),RGB(0,0,0) };
    Button btnExit = { 300, 390, 200, 50, L"退出游戏",RGB(208,208,208),RGB(0,0,0) };
    btnPVP.setFont(L"隶书");
    btnPVC.setFont(L"隶书");
    btnExit.setFont(L"隶书");
    btnPVP. setFontSize(40);
    btnPVC. setFontSize(40);
    btnExit.setFontSize(40);

    btnPVP.DrawButton();
    btnPVC.DrawButton();
    btnExit.DrawButton();

    MOUSEMSG m;
    while (true) {
        m = GetMouseMsg();
        //检测点击
        if (m.uMsg == WM_LBUTTONDOWN) {
            if (btnPVP.IsMouseOnButton(m)) {
                return 1;
            }
            else if (btnPVC.IsMouseOnButton(m)) {
                return 2;
            }
            else if (btnExit.IsMouseOnButton(m)) {
                return 3;
            }
        }
    }
}

// 加载图片
void LoadImages() {
    loadimage(&white, L"img/2.png");
    loadimage(&black, L"img/1.png");
    loadimage(&menu, L"img/menu.png");
    loadimage(&back, L"img/back.png");
}

//打印棋盘
void DrawBoard() {
    // 绘制背景到缓冲区
    putimage_alpha(0, 0, &back, &buffer);

    // 绘制棋子到缓冲区
    for (int i = 0; i < 19; i++) {
        for (int j = 0; j < 19; j++) {
            int x = j * 32;
            int y = i * 32;
            if (board[i][j] == 1) {
                putimage_alpha(x, y, &black, &buffer);
            }
            else if (board[i][j] == 2) {
                putimage_alpha(x, y, &white, &buffer);
            }
        }
    }
    // 将缓冲区绘制到屏幕
    putimage(0, 0, &buffer);
    undoButton.setFont(L"隶书");
    undoButton.setFontSize(20);
    undoButton.DrawButton();
}

// 判断落子是否合法函数
bool isValid(int x, int y, int tempboard[BOARDSIZE][BOARDSIZE]) {
    if (x < 0 || x >= BOARDSIZE || y < 0 || y >= BOARDSIZE) {
        return false;
    }
    if (tempboard[y][x] != 0) {
        return false;
    }
    return true;
}

// 放置棋子函数
bool setchess(int x, int y, int chess, int tempboard[BOARDSIZE][BOARDSIZE] = board) {
    if (!isValid(x, y, tempboard)) {
        cout << "Invalid move! (" << x << "," << y << ")" << endl;
        return false;
    }
    tempboard[y][x] = chess;
    return true;
}

// 获取某个位置的四条线
vector<string> getLines(int x, int y, int tempboard[BOARDSIZE][BOARDSIZE]) {
    vector<string> lines(4, "");
    // 水平
    for (int col = 0; col < BOARDSIZE; col++) {
        lines[0] += to_string(tempboard[y][col]);
    }
    // 垂直
    for (int row = 0; row < BOARDSIZE; row++) {
        lines[1] += to_string(tempboard[row][x]);
    }
    // 左斜
    int startX = x, startY = y;
    while (startX > 0 && startY > 0) {
        startX--;
        startY--;
    }
    while (startX < BOARDSIZE && startY < BOARDSIZE) {
        lines[2] += to_string(tempboard[startY][startX]);
        startX++;
        startY++;
    }
    // 右斜
    startX = x, startY = y;
    while (startX < BOARDSIZE - 1 && startY > 0) {
        startX++;
        startY--;
    }
    while (startX >= 0 && startY < BOARDSIZE) {
        lines[3] += to_string(tempboard[startY][startX]);
        startX--;
        startY++;
    }
    return lines;
}

// 判断胜负函数
int checkWin(int x, int y, int tempboard[BOARDSIZE][BOARDSIZE] = board) {
    int wincount = 0;
    wincount++;
    string pattern1 = "111111";
    string pattern2 = "222222";
    int player = tempboard[y][x];
    vector<string> lines = getLines(x, y, tempboard);
    for (auto& line : lines) {
        if (line.find(pattern1) != string::npos) {
            return 1; // 黑棋胜利
        }
        else if (line.find(pattern2) != string::npos) {
            return 2; // 白棋胜利
        }
    }
    for (int i = 0; i < BOARDSIZE; i++) {
        for (int j = 0; j < BOARDSIZE; j++) {
            if (tempboard[i][j] == 1 || tempboard[i][j] == 2)wincount++;
        }
    }
    if (wincount == 362) {
        return 3;//和棋
    }
    return 0;
}

// 复制棋盘函数
void copyBoard(int source[BOARDSIZE][BOARDSIZE], int dest[BOARDSIZE][BOARDSIZE]) {
    for (int i = 0; i < BOARDSIZE; i++) {
        for (int j = 0; j < BOARDSIZE; j++) {
            dest[i][j] = source[i][j];
        }
    }
}

// 提取包含空位的完整棋形
string getPattern(const string& line) {
    if (line.empty()) return ""; // 空字符串防护，避免越界
    string res(1, line[0]);
    char base = res[0];          // 基准字符，AI是2、玩家是1）
    char opponent = (base == '2') ? '1' : '2'; // 对方棋子

    if (line.size() == 1) return res; // 单字符情况直接返回
    // 情况1：第二个字符与基准相同（如22...、11...）
    if (line[1] == base) {
        int i = 1;
        // 连续添加相同基准字符
        for (; i < line.size() && line[i] == base; i++) {
            res += line[i];
        }
        // 遇到对方棋子，截断返回
        if (i < line.size() && line[i] == opponent) {
            return res;
        }
        // 遇到空位，继续处理
        else if (i < line.size() && line[i] == '0') {
            // 连续添加空位
            for (; i < line.size() && line[i] == '0'; i++) {
                res += line[i];
            }
            // 空位后仍为基准字符，继续添加
            if (i < line.size() && line[i] == base) {
                for (; i < line.size() && line[i] == base; i++) {
                    res += line[i];
                }
            }
            return res;
        }
    }
    // 情况2：第二个字符与基准不同（如21...、20...、12...、10...）
    else {
        int i = 1;
        // 遇到空位
        if (i < line.size() && line[i] == '0') {
            // 连续添加空位
            for (; i < line.size() && line[i] == '0'; i++) {
                res += line[i];
            }
            // 空位后仍为基准字符，继续添加
            if (i < line.size() && line[i] == base) {
                for (; i < line.size() && line[i] == base; i++) {
                    res += line[i];
                }
            }
            return res;
        }
        // 遇到对方棋子
        else if (i < line.size() && line[i] == opponent) {
            // 连续添加对方棋子
            for (; i < line.size() && line[i] == opponent; i++) {
                res += line[i];
            }
        }
    }
    // 处理完所有字符，返回完整结果
    return res;
}

//ai落子评估函数
multimap<int, pair<int, int>> aiMove(int tempboard[][BOARDSIZE], int step) {

    vector<pair<string, int>> bestMove = {
        // AI的强进攻棋形
        {"222222",200000}, // AI六连
        {"22222", 90000},  // AI五连
        {"202222", 40000},
        {"222022", 40000},
        {"220222", 40000},
        {"222202", 40000},
        {"2222", 30000},
        {"20222", 20000},
        {"22202", 20000},
        {"22022", 20000},
        {"222002", 10000},
        {"220022", 10000},
        {"200222", 10000},

        // AI的弱进攻棋形
        {"222", 500},     // AI活三
        {"22", 100},      // AI活二

		//ai防守棋形
        {"2111", 600},   // 阻挡玩家活三
        {"211", 150},     // 阻挡玩家活二
        {"211111", 50000 }, // 阻挡玩家五连
        {"21111", 45000},  // 阻挡玩家冲四
    };

    int i = bestMove.size();
    if (step == 1) {
        bestMove[i - 1] = { "211111",65000 };
        bestMove[i - 2] = { "21111",60000 };
    }
    // 棋盘中心权重（初期布局优先中心）
    int center = BOARDSIZE / 2;
    int scoreBoard[BOARDSIZE][BOARDSIZE] = { 0 };
    for (int i = 0; i < BOARDSIZE; i++) {
        for (int j = 0; j < BOARDSIZE; j++) {
            scoreBoard[i][j] = BOARDSIZE - (abs(center - i) + abs(center - j));
        }
    }

    multimap<int, pair<int, int>> topTenScore; // 存储Top10高分落子点
    // 遍历所有空位，评估落子价值
    for (int y = 0; y < BOARDSIZE; y++) { // 注意：y 是行，x 是列
        for (int x = 0; x < BOARDSIZE; x++) {
            if (isValid(x, y)) {
                // 模拟 AI 落子 (注意是 tempboard[y][x])
                tempboard[y][x] = 2;
                vector<string> lines = getChessLines(x, y, tempboard);
                int totalScore = 0;
                bool foundPattern = false;

                // 计算当前落子位置的分数
                for (auto line : lines) {
                    for (auto pattern : bestMove) {
                        if (line.find(pattern.first) != string::npos) {
                            totalScore += pattern.second;
                            foundPattern = true;
                        }
                    }
                }
                if (!foundPattern) {
                    totalScore += scoreBoard[y][x]; // 注意是 scoreBoard[y][x]
                }

                // 将当前落子位置和分数加入分数容器
                if (topTenScore.size() < TOP_N) {
                    topTenScore.insert({ totalScore,{x,y} });
                }
                else if (totalScore > topTenScore.begin()->first) {
                    topTenScore.erase(topTenScore.begin());
                    topTenScore.insert({ totalScore,{x,y} });
                }

                // 撤销模拟落子 (注意是 board[y][x])
                tempboard[y][x] = 0;
            }
        }
    }

    return topTenScore;
}

//玩家落子评估函数
multimap<int, pair<int, int>> playerMove(int tempboard[][BOARDSIZE], int step) {
    // 评分体系：玩家视角，分数为负值，1和2交换
    vector<pair<string, int>> bestMove = {
        // 【中优先级】玩家自身进攻（优先级低于防守AI高危）
        {"111111", -200000}, // 玩家六连（必胜）
        {"11111", -90000},  // 玩家五连（下一步必胜）

        // 玩家的绝对优势棋形
        {"101111", -40000}, // 玩家活四
        {"111011", -40000},
        {"110111", -40000},
        {"111101", -40000},

        // 玩家的强进攻棋形
        {"1111",  -30000},   // 玩家四连
        {"10111", -20000},  // 玩家冲四
        {"11101", -20000},
        {"11011", -20000},

        // 玩家的中等进攻棋形
        {"111001", -10000}, // 玩家眠四
        {"110011", -10000},
        {"100111", -10000},

        // 玩家的弱进攻棋形
        {"111", -500},     // 玩家活三
        {"11", -100},      // 玩家活二

        // --- 【AI防守棋形】---
        // 【最高优先级】阻挡AI下一步必胜


        // 【次高优先级】阻挡AI形成绝对优势
        {"1222", -650},   // 阻挡AI活三
        {"122", -150},     // 阻挡AI活二
        { "122222", -50000 }, // 阻挡AI五连
        {"12222", -45000},  // 阻挡AI冲四
    };

    int i = bestMove.size();
    if (step == 3) {
        bestMove[i - 1] = { "122222",-65000 };
        bestMove[i - 2] = { "12222",-60000 };
    }

    // 棋盘中心权重（初期布局优先中心）
    int center = BOARDSIZE / 2;
    int scoreBoard[BOARDSIZE][BOARDSIZE] = { 0 };
    for (int i = 0; i < BOARDSIZE; i++) {
        for (int j = 0; j < BOARDSIZE; j++) {
            scoreBoard[i][j] = BOARDSIZE - (abs(center - i) + abs(center - j));
        }
    }

    multimap<int, pair<int, int>> topTenScore; // 存储Top10高分落子点（负分，越小越好）

    // 遍历所有空位，评估落子价值
    for (int y = 0; y < BOARDSIZE; y++) {
        for (int x = 0; x < BOARDSIZE; x++) {
            if (isValid(x, y)) {
                // 模拟玩家落子
                tempboard[y][x] = 1;
                vector<string> lines = getChessLines(x, y, tempboard);
                int totalScore = 0;
                bool foundPattern = false;

                // 计算当前落子位置的分数
                for (auto line : lines) {
                    for (auto pattern : bestMove) {
                        if (line.find(pattern.first) != string::npos) {
                            totalScore += pattern.second;
                            foundPattern = true;
                        }
                    }
                }
                if (!foundPattern) {
                    totalScore = -scoreBoard[y][x]; // 注意：这里是减，因为分数是负值
                }

                // 将当前落子位置和分数加入分数容器
                if (topTenScore.size() < TOP_N) {
                    topTenScore.insert({ totalScore, {x, y} });
                }
                else if (totalScore < topTenScore.rbegin()->first) { // 注意：负分，越小越好
                    topTenScore.erase(--topTenScore.end());
                    topTenScore.insert({ totalScore, {x, y} });
                }

                // 撤销模拟落子
                tempboard[y][x] = 0;
            }
        }
    }

    return topTenScore;
}

// scoreTopTen()递归搜索最有分数计算落子点
multimap<int, pair<int, int>> scoreTopTen(
    int boardtemp[BOARDSIZE][BOARDSIZE],
    int step,       // 0: AI第一步, 1: AI第二步, 2: 玩家第一步, 3: 玩家第二步
    int depth = 0     // 递归深度
) {

    // 创建临时棋盘用于模拟
    int simboard[BOARDSIZE][BOARDSIZE];

    copyBoard(boardtemp, simboard);

    multimap<int, pair<int, int>> currentMoves;
    vector<multimap<int, pair<int, int>>> candidates;
    //新建一个multimap<int, pair<int, int>> topNMoves;
    multimap<int, pair<int, int>> topNMoves;
    // 递归终止条件
    if (depth == 0) {
        if (step == 0 || step == 1) {  // AI落子评估
            return aiMove(simboard, step);
        }
        else {  // 玩家落子评估
            return playerMove(simboard, step);
        }
    }
    if (step == 0) {
        currentMoves = aiMove(simboard, step);
        int i = 0;
        int score = 0;
        pair<int, int> locate;//坐标
        for (auto it = currentMoves.rbegin(); it != currentMoves.rend(); it++) {//rbegin是最高分数,这一回合是ai回合需要加上最高分数
            simboard[it->second.second][it->second.first] = 2; // 模拟AI落子
            candidates.push_back(scoreTopTen(simboard, (step + 1) % 4, depth - 1));//递归调用，下一回合是ai回合，调用的结果存储在candidates中
            simboard[it->second.second][it->second.first] = 0;
            i++;
        }
        //获取完了candidates，然后进行处理
        i = 0;
        for (auto it = currentMoves.rbegin(); it != currentMoves.rend(); it++) {
            //获取最高分应该这样操作，candidates[i].rbegin()->first
            score = it->first + candidates[i++].rbegin()->first;//最高分数加递归最高分数
            locate = it->second;//坐标不变，表示在这个坐标能获得的期望值
            topNMoves.insert({ score,locate });
        }
    }
    if (step == 1) {
        //ai回合，但是下一步是玩家
        currentMoves = aiMove(simboard, step);
        int i = 0;
        int score = 0;
        pair<int, int> locate;//坐标
        for (auto it = currentMoves.rbegin(); it != currentMoves.rend(); it++) {//rbegin是最高分数,这一回合是玩家回合需要加上最低分数
            simboard[it->second.second][it->second.first] = 2; // 模拟AI落子
            candidates.push_back(scoreTopTen(simboard, (step + 1) % 4, depth - 1));//递归调用，下一回合是ai回合，调用的结果存储在candidates中
            simboard[it->second.second][it->second.first] = 0;
            i++;
        }
        i = 0;
        for (auto it = currentMoves.rbegin(); it != currentMoves.rend(); it++) {
            //获取最高分应该这样操作，candidates[i].begin()->first
            score = it->first + (candidates[i++].begin()->first) * 0.8;//最低分数加递归最低分数，
            locate = it->second;//坐标不变，表示在这个坐标能获得的期望值
            topNMoves.insert({ score,locate });
        }
    }
    if (step == 2) {
        //玩家回合，下一步还是玩家
        currentMoves = playerMove(simboard, step);
        int i = 0;
        int score = 0;
        pair<int, int> locate;//坐标
        for (auto it = currentMoves.begin(); it != currentMoves.end(); it++) {//begin是最低分数,这一回合是玩家回合需要加上最低分数
            simboard[it->second.second][it->second.first] = 1; // 模拟玩家落子
            candidates.push_back(scoreTopTen(simboard, (step + 1) % 4, depth - 1));//递归调用，下一回合是ai回合，调用的结果存储在candidates中
            //获取最高分应该这样操作，candidates[i].regin()->first
            simboard[it->second.second][it->second.first] = 0;
            i++;
        }
        i = 0;
        for (auto it = currentMoves.begin(); it != currentMoves.end(); it++) {
            score = it->first + candidates[i++].begin()->first;//最低分数加递归最低分数，
            locate = it->second;//坐标不变，表示在这个坐标能获得的期望值
            topNMoves.insert({ score,locate });
        }
    }
    if (step == 3) {
        //玩家回合，下一步是ai
        currentMoves = playerMove(simboard, step);
        int i = 0;
        int score = 0;
        pair<int, int> locate;//坐标
        for (auto it = currentMoves.begin(); it != currentMoves.end(); it++) {//begin是最低分数,这一回合是玩家回合需要加上最低分数
            simboard[it->second.second][it->second.first] = 1; // 模拟玩家落子
            candidates.push_back(scoreTopTen(simboard, (step + 1) % 4, depth - 1));//递归调用，下一回合是ai回合，调用的结果存储在candidates中
            //获取最高分应该这样操作，candidates[i].regin()->first
            simboard[it->second.second][it->second.first] = 0;
            i++;
        }
        i = 0;
        for (auto it = currentMoves.begin(); it != currentMoves.end(); it++) {
            score = it->first + (candidates[i++].rbegin()->first) * 0.8;//最低分数加递归最高分数，
            locate = it->second;//坐标不变，表示在这个坐标能获得的期望值
            topNMoves.insert({ score,locate });
        }
    }
    return topNMoves;
}

// AI落子函数
vector<pair<int, int>> aiput(int tempboard[BOARDSIZE][BOARDSIZE]) {
    vector<pair<int, int>> moves;

    // 第一步：基于原始棋盘计算
    auto firstMoves = scoreTopTen(tempboard, 0, dep);
    auto firstMove = firstMoves.rbegin()->second;
    moves.push_back(firstMove);

    // 在棋盘上模拟第一步，为计算第二步做准备
    tempboard[firstMove.second][firstMove.first] = 2;

    // 第二步：基于已经下了第一步的棋盘副本进行计算
    auto secondMoves = scoreTopTen(tempboard, 1, dep);
    //恢复模拟
    tempboard[firstMove.second][firstMove.first] = 0;
    auto secondMove = secondMoves.rbegin()->second;
    moves.push_back(secondMove);

    return moves;
}

vector<string> dealString(vector<string> Str) {
    vector<string> lines;
    for (auto s : Str) {
        lines.push_back(getPattern(s));
    }
    return lines;
}

// 返回值：true 表示再来一次，false 表示退回标题
bool GameEndButtons() {
    MOUSEMSG m;
    Button btnRestart(618, 350, 150, 40, L"重新开始", RGB(208, 208, 208), RGB(0, 0, 0));
    Button btnMenu(618, 400, 150, 40, L"返回菜单", RGB(208, 208, 208), RGB(0, 0, 0));
    btnRestart.setFont(L"隶书");
    btnMenu.setFont(L"隶书");
    btnRestart.setFontSize(30);
    btnMenu.setFontSize(30);
    while (true) {
        m = GetMouseMsg();
        if (m.uMsg == WM_LBUTTONDOWN) {
            if (btnRestart.IsMouseOnButton(m)) {
                return true;  // 重新开始
            }
            else if (btnMenu.IsMouseOnButton(m)) {
                return false; // 返回菜单
            }
        }
        BeginBatchDraw();
        btnRestart.DrawButton();
        btnMenu.DrawButton();
        EndBatchDraw();
    }
}

int main() {
    LoadImages();
    initgraph(808, 608);
    HWND hnd = GetHWnd(); // 获取窗口句柄
    SetWindowText(hnd, L"连六棋"); // 设置窗口标题
    while (true) {  // 主循环，控制整个程序流程
        int mode = MainMenu();  // 返回用户选择的模式
        if (mode == 1) {
            PVPWindow();  // 人人对战
        }
        else if (mode == 2) {
            PVCWindow();  // 人机对战
        }
        else if (mode == 3) {
            break;  // 退出游戏
        }
    }

    closegraph();
    return 0;
}

// 获取棋形线条
vector<string> getChessLines(int x, int y, int tempboard[BOARDSIZE][BOARDSIZE]) {
    vector<string> lines(8, "");
    int range = 7;
    //水平左
    for (int col = x; col >= 0 && col > x - range; col--) {
        lines[0] += to_string(tempboard[y][col]);
    }
    //水平右
    for (int col = x; col < BOARDSIZE && col < x + range; col++) {
        lines[1] += to_string(tempboard[y][col]);
    }
    //垂直上
    for (int row = y; row >= 0 && row > y - range; row--) {
        lines[2] += to_string(tempboard[row][x]);
    }
    //垂直下
    for (int row = y; row < BOARDSIZE && row < y + range; row++) {
        lines[3] += to_string(tempboard[row][x]);
    }
    //左斜上
    int startX = x, startY = y;
    for (int i = 0; startX >= 0 && startY >= 0 && i < range; i++) {
        lines[4] += to_string(tempboard[startY][startX]);
        startX--;
        startY--;
    }
    //左斜下
    startX = x, startY = y;
    for (int i = 0; startX < BOARDSIZE && startY < BOARDSIZE && i < range; i++) {
        lines[5] += to_string(tempboard[startY][startX]);
        startX++;
        startY++;
    }
    //右斜上
    startX = x, startY = y;
    for (int i = 0; startX < BOARDSIZE && startY >= 0 && i < range; i++) {
        lines[6] += to_string(tempboard[startY][startX]);
        startX++;
        startY--;
    }
    //右斜下
    startX = x, startY = y;
    for (int i = 0; startX >= 0 && startY < BOARDSIZE && i < range; i++) {
        lines[7] += to_string(tempboard[startY][startX]);
        startX--;
        startY++;
    }
    return dealString(lines);
}

// 人机对战窗口
void PVCWindow() {
    initgraph(808, 608);
    HWND hnd = GetHWnd(); // 获取窗口句柄
    SetWindowText(hnd, L"连六棋"); // 设置窗口标题
    MOUSEMSG m;
    while (true) {
        // --- 每局游戏开始时的初始化 ---
        memset(board, 0, sizeof(board));
        clearHistory();
        int result = 0;
        DrawBoard(); // 初始绘制空棋盘
        bool gameover = false;
        int playerSetTimes = 1;
        settextcolor(RGB(255, 0, 0));
        settextstyle(40, 0, L"隶书");
        outtextxy(200, 280, L"游戏开始！黑棋先行");
        //让文字显示一段时间后在刷新棋盘
        Sleep(1000);
        DrawBoard();

        // --- 游戏主循环---
        while (!gameover) {
            for (int i = 0; i < playerSetTimes; i++) {
                settextstyle(20, 0, L"微软雅黑");
                settextcolor(RGB(255, 0, 0));
                outtextxy(618, 10, L"你的回合，请落子");
                settextstyle(40, 0, L"微软雅黑");

                int col = -1, row = -1;

                //这是阻塞式等待循环，我们在这里插入按钮检测
                do {
                    m = GetMouseMsg(); // 阻塞，等待鼠标消息
                    // --- 检测是否点击了悔棋按钮 ---
                    if (m.uMsg == WM_LBUTTONDOWN && undoButton.IsMouseOnButton(m)) {
                        if (playerSetTimes - i == 2) {
                            if (moveHistory.size() > 2) {
                                if (moveHistory.size() < 4) { // 确保有棋可悔
                                    undoMove(1);
                                    break;
                                }
                                else {
                                    undoMove(1);
                                    playerSetTimes++;
                                    break;
                                }
                            }
                            break;
                        }
                        else {
                            // 玩家未完成回合，提示并忽略点击
                            settextstyle(20, 0, L"微软雅黑");
                            settextcolor(RGB(255, 0, 0));
                            outtextxy(620, 45, L"完成本回合后方可悔棋");
                        }

                    }

                    if (m.uMsg == WM_LBUTTONDOWN) {
                        col = m.x / 32;
                        row = m.y / 32;
                    }
                } while (!isValid(col, row)); // 循环条件保持不变

                if (!isValid(col, row)) {
                    // 这说明不是因为找到了有效落子点而跳出的循环
                    // 那么就是因为点击了悔棋按钮
                    // 重绘界面，因为悔棋已经修改了棋盘
                    DrawBoard();
                    continue; // 跳过本次 for 循环的剩余部分，也跳过后续的AI回合，
                    // 直接进入下一次 while (!gameover) 循环
                }
                setchess(col, row, 1);
                recordMove({ col, row });
                DrawBoard();

                result = checkWin(col, row);
                if (result == 1) {
                    settextstyle(40, 0, L"隶书");
                    settextcolor(RGB(255, 0, 0));
                    outtextxy(180, 280, L"黑棋胜利！");
                    gameover = true;
                    break; // 跳出 for 循环
                }
                else if (result == 3) {
                    settextstyle(40, 0, L"隶书");
                    settextcolor(RGB(255, 0, 0));
                    outtextxy(180, 280, L"和棋！");
                    gameover = true;
                    break; // 跳出 for 循环
                }
            }

            // 如果因为悔棋触发了 continue，那么下面的代码都不会执行
            if (gameover) break;

            settextstyle(20, 0, L"微软雅黑");
            settextcolor(RGB(255, 0, 0));
            outtextxy(618, 10, L"AI思考中...");
            
            vector<pair<int, int>> aiMoves = aiput(board);
            if (aiMoves.size() < 2) {
                cerr << "AI落子生成失败" << endl;
                gameover = true;
                break;
            }

            // AI第一步
            if (isValid(aiMoves[0].first, aiMoves[0].second)) {
                setchess(aiMoves[0].first, aiMoves[0].second, 2);
                recordMove({ aiMoves[0].first,aiMoves[0].second });
                DrawBoard();
                result = checkWin(aiMoves[0].first, aiMoves[0].second);
                if (result == 2) {
                    settextstyle(40, 0, L"隶书");
                    settextcolor(RGB(255, 0, 0));
                    outtextxy(180, 280, L"白棋胜利！");
                    gameover = true;
                    break;
                }
                else if (result == 3) {
                    settextstyle(40, 0, L"隶书");
                    settextcolor(RGB(255, 0, 0));
                    outtextxy(180, 280, L"和棋！");
                    gameover = true;
                    break;
                }
            }
            if (gameover) break;

            // AI第二步
            if (isValid(aiMoves[1].first, aiMoves[1].second)) {
                setchess(aiMoves[1].first, aiMoves[1].second, 2);
                recordMove({ aiMoves[1].first,aiMoves[1].second });
                DrawBoard();
                result = checkWin(aiMoves[1].first, aiMoves[1].second);
                if (result == 2) {
                    settextstyle(40, 0, L"隶书");
                    settextcolor(RGB(255, 0, 0));
                    outtextxy(180, 280, L"白棋胜利！");
                    gameover = true;
                    break;
                }
                else if (result == 3) {
                    settextstyle(40, 0, L"隶书");
                    settextcolor(RGB(255, 0, 0));
                    outtextxy(180, 280, L"和棋！");
                    gameover = true;
                    break;
                }
            }
            playerSetTimes = 2;

        }

        // 游戏结束，调用结尾按钮函数
        if (!GameEndButtons()) {
            return;  // 用户选择退回标题，直接返回，主循环会回到主菜单
        }
    }

    system("pause");
    closegraph();
}

//人人对战窗口
void PVPWindow() {
    initgraph(808, 608);
    HWND hnd = GetHWnd(); // 获取窗口句柄
    SetWindowText(hnd, L"连六棋"); // 设置窗口标题
    MOUSEMSG m;

    while (true) {
        memset(board, 0, sizeof(board));
        clearHistory();

        int result = 0;
        DrawBoard();
        bool gameover = false;
        int playerSetTimes = 1;
        int currentPlayer = 1; // 1 黑棋，2 白棋
        settextcolor(RGB(255, 0, 0));
        settextstyle(40, 0, L"隶书");
        outtextxy(200, 280, L"游戏开始！黑棋先行");
        Sleep(1000);
        DrawBoard();

        while (!gameover) {
            bool undoFlag = false;

            // 黑棋回合
            if (currentPlayer == 1) {
                for (int i = 0; i < playerSetTimes  && !undoFlag; i++) {
                    settextstyle(20, 0, L"微软雅黑");
                    settextcolor(RGB(255, 0, 0));
                    outtextxy(618, 10, L"黑棋回合，请落子");
                    int col = -1, row = -1;
                    do {
                        m = GetMouseMsg();

                        if (m.uMsg == WM_LBUTTONDOWN && undoButton.IsMouseOnButton(m)) {
                            if (playerSetTimes - i == 2) {
                                if (canUndo()) {
                                    undoMove(2);
                                    currentPlayer = 2;
                                    undoFlag = true;
                                    break;
                                }
                            }
                            else {
                                settextstyle(20, 0, L"微软雅黑");
                                settextcolor(RGB(255, 0, 0));
                                outtextxy(620, 45, L"完成本回合后方可悔棋");
                            }
                        }

                        if (m.uMsg == WM_LBUTTONDOWN) {
                            col = m.x / 32;
                            row = m.y / 32;
                        }
                    } while (!isValid(col, row));

                    if (!isValid(col, row)) {
                        DrawBoard();
                        continue;
                    }

                    setchess(col, row, 1);
                    recordMove({ col, row });
                    DrawBoard();

                    result = checkWin(col, row);
                    if (result == 1) {
                        settextstyle(40, 0, L"隶书");
                        settextcolor(RGB(255, 0, 0));
                        outtextxy(180, 280, L"黑棋胜利！");
                        gameover = true;
                        break;
                    }
                    else if (result == 3) {
                        settextstyle(40, 0, L"隶书");
                        settextcolor(RGB(255, 0, 0));
                        outtextxy(180, 280, L"和棋！");
                        gameover = true;
                        break;
                    }
                }
                if (gameover || undoFlag) continue;

                currentPlayer = 2; // 切换到白棋
                playerSetTimes = 2;
            }

            // 白棋回合
            if (currentPlayer == 2) {
                for (int i = 0; i < playerSetTimes  && !undoFlag; i++) {
                    settextstyle(20, 0, L"微软雅黑");
                    settextcolor(RGB(255, 0, 0));
                    outtextxy(618, 10, L"白棋回合，请落子");

                    int col = -1, row = -1;
                    do {
                        m = GetMouseMsg();
                        if (m.uMsg == WM_LBUTTONDOWN && undoButton.IsMouseOnButton(m)) {
                            if (playerSetTimes - i == 2) {
                                if (moveHistory.size() == 1) {
                                    playerSetTimes = 1;
                                    currentPlayer = 1;
                                    undoMove(2);
                                    undoFlag = true;
                                    break;
                                }
                                else if (canUndo()) {
                                    undoMove(2);
                                    currentPlayer = 1;
                                    undoFlag = true;
                                    break;
                                }
                            }
                            else {
                                // 玩家未完成回合，提示并忽略点击
                                settextstyle(20, 0, L"微软雅黑");
                                settextcolor(RGB(255, 0, 0));
                                outtextxy(620, 45, L"完成本回合后方可悔棋");
                            }
                        }

                        if (m.uMsg == WM_LBUTTONDOWN) {
                            col = m.x / 32;
                            row = m.y / 32;
                        }
                    } while (!isValid(col, row));

                    if (!isValid(col, row)) {
                        DrawBoard();
                        continue;
                    }

                    setchess(col, row, 2);
                    recordMove({ col, row });
                    DrawBoard();

                    result = checkWin(col, row);
                    if (result == 2) {
                        settextstyle(40, 0, L"隶书");
                        settextcolor(RGB(255, 0, 0));
                        outtextxy(180, 280, L"白棋胜利！");
                        gameover = true;
                        break;
                    }
                    else if (result == 3) {
                        settextstyle(40, 0, L"隶书");
                        settextcolor(RGB(255, 0, 0));
                        outtextxy(180, 280, L"和棋！");
                        gameover = true;
                        break;
                    }
                }
                if (gameover || undoFlag) continue;

                currentPlayer = 1; // 切换到黑棋
                playerSetTimes = 2;
            }
        }
        if (!GameEndButtons()) {
            return;  // 用户选择退回标题，直接返回，主循环会回到主菜单
        }
    }

    system("pause");
    closegraph();
}
