#include "chessTool.h"

using namespace std;
int board[19][19];
set<pair<int, int>> moves;

unordered_map<string, int> patternScores = {
	{"1", 1},//活一
	{"101",5},//眠二
	{"1001",4},
	{"11",8},//活二
	{"1101",15},//眠三
	{"1011",15},
	{"10011",10},
	{"11001",10},
	{"10101",10},
	{"111",30},//活三
	{"10111",40},//眠四
	{"11011",40},
	{"11101",40},
	{"100111",35},
	{"110011",35},
	{"111001",35},
	{"101011",35},
	{"101101",35},
	{"110101",35},
	{"1111",50 },//活四
	{"101111",60},//眠五
	{"110111",60},
	{"111011",60},
	{"111101",60},
	{"11111",100 },//活五
	{"111111",99999},//胜利情形
};
bool isEmpty(int row, int col) {
    if (board[row][col] == 0)
        return true;
    return false;
}
void updateMoves(int board[19][19], int row, int col, bool refresh) {
    if (refresh) {
        // 外扩 8 点：距离 3 的八个方向
        if (row - 3 >= 0) {
            if (isEmpty(row - 3, col))
                moves.insert({ row - 3, col });
            if (col - 3 >= 0 && isEmpty(row - 3, col - 3))
                moves.insert({ row - 3, col - 3 });
            if (col + 3 < 19 && isEmpty(row - 3, col + 3))
                moves.insert({ row - 3, col + 3 });
        }
        if (row + 3 < 19) {
            if (isEmpty(row + 3, col))
                moves.insert({ row + 3, col });
            if (col + 3 < 19 && isEmpty(row + 3, col + 3))
                moves.insert({ row + 3, col + 3 });
            if (col - 3 >= 0 && isEmpty(row + 3, col - 3))
                moves.insert({ row + 3, col - 3 });
        }
        if (col - 3 >= 0 && isEmpty(row, col - 3))
            moves.insert({ row, col - 3 });
        if (col + 3 < 19 && isEmpty(row, col + 3))
            moves.insert({ row, col + 3 });

        // 5×5 邻域（24 格）
        for (int i = row - 2; i < row + 3; i++) {
            for (int j = col - 2; j < col + 3; j++) {
                if (i >= 0 && i < 19 && j >= 0 && j < 19 && isEmpty(i, j))
                    moves.insert({ i, j });
            }
        }
    }

}
vector<pair<pair<int, int>, pair<int, int>>> makePairs() {
    vector<pair<pair<int, int>, pair<int, int>>> pairs;
    vector<pair<int, int>> points(moves.begin(), moves.end());

    for (size_t i = 0; i < points.size(); i++) {
        for (size_t j = i + 1; j < points.size(); j++) {
            pairs.push_back({ points[i], points[j] });
        }
    }
    return pairs;
}
void placeStone(int row, int col, int board[19][19], int piece, bool refresh) {
    if (board[row][col] != 0) return;
    board[row][col] = piece;
    updateMoves(board, row, col, refresh);
}

vector<string> makeCase(int board[19][19]) {
	vector<string> result;

	auto addLine = [&](int startRow, int startCol, int rowStep, int colStep) {
		string line;
		int row = startRow;
		int col = startCol;

		while (row >= 0 && row < 19 && col >= 0 && col < 19) {
			line += to_string(board[row][col]);
			row += rowStep;
			col += colStep;
		}

		if (!line.empty()) {
			result.push_back(line);
		}
		};

	// 横方向
	for (int row = 0; row < 19; ++row) {
		addLine(row, 0, 0, 1);
	}

	// 竖方向
	for (int col = 0; col < 19; ++col) {
		addLine(0, col, 1, 0);
	}

	// 反斜方向：\

	for (int col = 0; col < 19; ++col) {
		addLine(0, col, 1, 1);
	}

	for (int row = 1; row < 19; ++row) {
		addLine(row, 0, 1, 1);
	}

	// 斜方向：/
	for (int col = 0; col < 19; ++col) {
		addLine(0, col, 1, -1);
	}

	for (int row = 1; row < 19; ++row) {
		addLine(row, 18, 1, -1);
	}
	return result;
}