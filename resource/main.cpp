#include<iostream>
#include<random>
#include"chessTool.h"
using namespace std;
void show() {
	for (int i = 0; i < 19; i++) {
		for (int j = 0; j < 19; j++) {
			cout << board[i][j] << " ";
		}
		cout << endl;
	}
}

int main() {
	Trie scorer(patternScores);
	int score;
	return 0;
}