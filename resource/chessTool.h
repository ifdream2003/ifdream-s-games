#pragma once
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>
#include<set>
extern std::unordered_map<std::string, int> patternScores;
extern int board[19][19];
extern std::set<std::pair<int, int>> moves;

class Trie {
public:
	// ---------- 节点定义 ----------
	class Node {
	public:
		char ch;
		std::vector<Node*> children;
		bool terminal = false;
		int score = 0;

		Node(char value) : ch(value) {}
		~Node() {                        // 递归释放，防内存泄漏
			for (Node* child : children)
				delete child;
		}
	};

	Node* rootNode = new Node('\0');

	// ---------- 构建字典树 ----------
	Trie(const std::unordered_map<std::string, int>& scores) {
		for (auto& [pattern, score] : scores) {
			insert(pattern, score);
		}
	}

	~Trie() { delete rootNode; }

	// ---------- 插入一个完整单词 ----------
	void insert(const std::string& pattern, int score) {
		Node* current = rootNode;
		for (char ch : pattern) {
			Node* next = find(current, ch);
			if (next == nullptr) {       // 子节点不存在才创建
				next = new Node(ch);
				current->children.push_back(next);
			}
			current = next;
		}
		current->terminal = true;
		current->score = score;
	}

	// ---------- 在某节点的 children 中查找指定字符 ----------
	Node* find(Node* node, char ch) const {
		for (Node* child : node->children) {
			if (child->ch == ch)
				return child;
		}
		return nullptr;
	}

	// ---------- 最长前缀匹配 ----------
// 从 text[start] 开始做最长前缀匹配，返回最长词尾长度，val 带出分值
	int matchLongest(const std::string& text, int start,
		std::string& matchedPattern, int& score) const {
		Node* current = rootNode;
		int len = 0, lastLen = 0;
		score = 0;
		matchedPattern.clear();

		for (int i = start; i < (int)text.size(); ++i) {
			Node* next = find(current, text[i]);
			if (next == nullptr)
				break;

			current = next;
			++len;

			if (current->terminal) {
				lastLen = len;
				score = current->score;
				matchedPattern = text.substr(start, len);
			}
		}
		return lastLen;
	}

	// 找到匹配后，检查它前后的两个 '2'，距离 >5 才计分，返回全串最高分
	bool findBest(const std::string& text, int& bestScore) const {
		bestScore = 0;
		bool found = false;
		std::string padded = "2" + text + "2";

		for (int start = 0; start < (int)padded.size(); ++start) {
			std::string pattern;
			int score = 0;
			int len = matchLongest(padded, start, pattern, score);
			if (len == 0)
				continue;               // 这个位置没匹配到，换下一个起点

			// 匹配区间是 [matchBegin, matchEnd)
			int matchStart = start;
			int matchEnd = start + len - 1;

			// 向左找匹配串前一个 '2'
			int left = matchStart - 1;
			while (left >= 0 && padded[left] != '2')
				--left;

			// 向右找匹配串后一个 '2'
			int right = matchEnd+1;
			while (right < (int)padded.size() && padded[right] != '2')
				++right;

			// 两个 '2' 之间的距离 >5 才算有效棋型
			if (right - left > 6) {
				if (score > bestScore) {
					bestScore = score;
					found = true;
				}
			}

		}
		return found;
	}

};

bool isEmpty(int row, int col);
void updateMoves(int board[19][19], int row, int col, bool refresh = false);
void placeStone(int row, int col, int board[19][19], int piece, bool refresh = false);
std::vector<std::pair<std::pair<int, int>, std::pair<int, int>>>makePairs();
std::vector<std::string> makeCase(int board[19][19]);