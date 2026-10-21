#include "ai/Search.h"

#include <map>
#include <utility>
#include <vector>

#include "ai/Evaluator.h"
#include "core/Rules.h"

using namespace std;

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

