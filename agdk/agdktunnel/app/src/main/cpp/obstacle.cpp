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

#include "obstacle.hpp"

#define BONUS_PROBABILITY 0.7f

void Obstacle::PutRandomBonus() {
    if (Random(100) * 0.01f > BONUS_PROBABILITY) {
        return;
    }

    int r, c;

    // (This is the most deeply indented code I have written in my life)
    // It goes through the grid and marks all the squares that are adjacent to
    // a solid square as candidates for the bonus.
    for (r = 0; r < OBS_GRID_SIZE; r++) {
        for (c = 0; c < OBS_GRID_SIZE; c++) {
            if (grid[c][r]) {
                bonusCol = c;
                bonusRow = r+1;
            }
        }
    }
}
