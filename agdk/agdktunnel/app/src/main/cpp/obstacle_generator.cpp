/*
 * Copyright 2021 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     https://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "game_consts.hpp"
#include "obstacle_generator.hpp"

void ObstacleGenerator::Generate(Obstacle *result) {
    static const int PROB_TABLE[] = {
            // EASY   MED  INT  HARD
            100, 0, 0, 0,  // difficulty 0
            75, 25, 0, 0,  // difficulty 1
            50, 50, 0, 0,  // difficulty 2
            25, 75, 0, 0,  // difficulty 3
            0, 100, 0, 0,  // difficulty 4
            0, 75, 25, 0,  // difficulty 5
            0, 50, 50, 0,  // difficulty 6
            0, 25, 75, 0,  // difficulty 7
            0, 0, 100, 0,  // difficulty 8
            0, 0, 75, 25,  // difficulty 9
            0, 0, 50, 50,  // difficulty 10
            0, 0, 25, 75,  // difficulty 11
            0, 0, 0, 100   // difficulty 12+
    };
    result->Reset();
    result->style = 1 + Random(7);

    int d = Clamp(mDifficulty, 0, 2);
    int easyProb = PROB_TABLE[0];
    int medProb = PROB_TABLE[d * 4 + 1];
    int intermediateProb = PROB_TABLE[d * 4 + 2];
    int roll = Random(100);
    if (roll <= easyProb) {
        GenEasy(result);
    } else if (roll <= easyProb + medProb) {
        GenMedium(result);
    } else if (roll <= easyProb + medProb + intermediateProb) {
        GenIntermediate(result);
    } else {
        GenHard(result);
    }
    result->PutRandomBonus();
}

void ObstacleGenerator::GenEasy(Obstacle *result) {
    int n = Random(4);
    switch (n) {
        case 0:
            result->grid[0][4] = true;
            break;
        case 1:
            break;
        case 2:
            break;
        default:
            result->grid[0][4] = true;
            break;
    }
}

void ObstacleGenerator::GenMedium(Obstacle *result) {
    result->grid[0][4] = true;
}

void ObstacleGenerator::GenIntermediate(Obstacle *result) {
    result->grid[0][4] = true;
}

void ObstacleGenerator::GenHard(Obstacle *result) {
    result->grid[0][4] = true;
}
