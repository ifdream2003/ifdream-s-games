#include "core/Rules.h"

#include <string>
#include <vector>

using namespace std;

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
int checkWin(int x, int y, int tempboard[BOARDSIZE][BOARDSIZE]) {
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

vector<string> dealString(vector<string> Str) {
    vector<string> lines;
    for (auto s : Str) {
        lines.push_back(getPattern(s));
    }
    return lines;
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

