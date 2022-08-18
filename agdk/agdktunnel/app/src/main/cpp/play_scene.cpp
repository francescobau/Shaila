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

#include <cstdio>

#include "ascii_to_geom.hpp"
#include "game_consts.hpp"
#include "our_shader.hpp"
#include "play_scene.hpp"
#include "texture_manager.hpp"
#include "util.hpp"
#include "ui_scene.hpp"

#include "data/ascii_art.inl"
#include "data/cube_geom.inl"
#include "data/strings.inl"
#include "data/tunnel_geom.inl"
#include "welcome_scene.hpp"

#define WALL_TEXTURE_SIZE 64

// colors for menus
static const float MENUITEM_SEL_COLOR[] = {1.0f, 1.0f, 0.0f};
static const float MENUITEM_COLOR[] = {1.0f, 1.0f, 1.0f};

// TODO: Rimuovere variabili e function di debug
static FILE* outFile;
static bool isOpen;
static int counter = 0,nDebug = 0;

static bool openDebugFile() {
    if(isOpen){
//        ShowSign("Already open",SIGN_DURATION/SIGN_DURATION);
        return true;
    }
//    ShowSign("Attempting to open...",SIGN_DURATION/SIGN_DURATION);
    outFile = fopen("output.txt","w+");
    if(!outFile){
//        ShowSign("FOPEN FAILED TO START.",SIGN_DURATION);
        return false;
    }
    nDebug = 10;
    return true;
}

// obstacle colors
static const float OBS_COLORS[] = {
        0.0f, 0.0f, 0.0f, // style 0 (not used)
        0.0f, 0.0f, 1.0f,
        0.0f, 1.0f, 0.0f,
        0.0f, 1.0f, 1.0f,
        1.0f, 0.0f, 0.0f,
        1.0f, 0.0f, 1.0f,
        1.0f, 1.0f, 0.0f
};

static const char *TONE_BONUS[] = {
        "d70 f150. f250. f350. f450.",
        "d70 f200. f300. f400. f500.",
        "d70 f250. f350. f450. f550.",
        "d70 f300. f400. f500. f600.",
        "d70 f350. f450. f550. f650.",
        "d70 f400. f500. f600. f700.",
        "d70 f450. f550. f650. f750.",
        "d70 f500. f600. f700. f800.",
        "d70 f550. f650. f750. f850."
};

PlayScene::PlayScene() : Scene() {
    mOurShader = NULL;
    mTrivialShader = NULL;
    mTextRenderer = NULL;
    mShapeRenderer = NULL;
    mShipSteerX = mShipSteerZ = 0.0f;
    mFilteredSteerX = mFilteredSteerZ = 0.0f;

    screenWidth = SceneManager::GetInstance()->GetScreenWidth();
    screenHeight = SceneManager::GetInstance()->GetScreenHeight();
    screenAspect = SceneManager::GetInstance()->GetScreenAspect();
    mPlayerPos = glm::vec3 (-3*screenWidth/10.0f, 0.0f, 0.0f); // player position, (0.0f, 0.0f, 0.0f) is center

    mPlayerDir = glm::vec3 (-1.0f, 0.0f, 0.0f); // right
    player = NewPlayer();
    playerIconPos = glm::vec2(0.174f * screenAspect, 0.35f);

    mDifficulty = 0;
    mUseCloudSave = false;

    mCubeGeom = NULL;
    mTunnelGeom = NULL;

    pointerDownTimer = 0;

    // it stores the timer when the player is on the obstacle.
    storedPointerDownTimer = 0;
    // it determines if the player is on the obstacle or not.
    isOnTop = false;
    // it counts how many times the player earned an extra life
    extraLifeCounter = 0;

    halfJumpTime = DEFAULT_JUMP_TIME / 2;
    jumpHeight = JUMP_HEIGHT;
    jumpSpeed = jumpHeight * (mDifficulty+1);

    mObstacleCount = 0;
    mFirstObstacle = 0;
    mFirstSection = 0;
    mSteering = STEERING_NONE;
    mPointerId = -1;
    mPointerAnchorX = mPointerAnchorY = 0.0f;

    for (int i = 0; i < MAX_WALL_TEXTURES; ++i) {
        mWallTextures[i] = NULL;
    }

    memset(mMenuItemText, 0, sizeof(mMenuItemText));
    mMenuItemText[MENUITEM_UNPAUSE] = S_UNPAUSE;
    mMenuItemText[MENUITEM_QUIT] = S_QUIT;
    mMenuItemText[MENUITEM_START_OVER] = S_START_OVER;
    mMenuItemText[MENUITEM_RESUME] = S_RESUME;

    memset(mMenuItems, 0, sizeof(mMenuItems));
    mMenuItemCount = 0;

    mMenu = MENU_NONE;
    mMenuSel = 0;

    mSignText = NULL;
    mSignTimeLeft = 0.0f;

    mShowedHowto = false;
    mLifeGeom = NULL;

    mLives = PLAYER_LIVES;

    mRollAngle = 0.0f;

    mPlayerSpeed = 0.0f;
    mBlinkingHeart = false;
    mGameStartTime = Clock();

    mBonusInARow = 0;
    mLastCrashSection = -1;

    mFrameClock.SetMaxDelta(MAX_DELTA_T);
    mLastAmbientBeepEmitted = 0;
    mMenuTouchActive = false;

    mCheckpointSignPending = false;

    mActiveWallTextureCount = 0;

    SetScore(0);

    // TODO: Rimuovere istruzioni di debug.
    isOpen = openDebugFile();

    /*
     * where do I put the program???
     */
    const char *savePath = "/mnt/sdcard/com.google.example.games.tunnel.fix";
    int len = strlen(savePath) + strlen(SAVE_FILE_NAME) + 3;
    mSaveFileName = new char[len];
    strcpy(mSaveFileName, savePath);
    strcat(mSaveFileName, "/");
    strcat(mSaveFileName, SAVE_FILE_NAME);
    ALOGI("Save file name: %s", mSaveFileName);
    LoadProgress();

    if (mSavedCheckpoint) {
        // start with the menu that asks whether or not to start from the saved level
        // or start over from scratch
        ShowMenu(MENU_LEVEL);
    }
}

void PlayScene::LoadProgress() {
    // try to load save file
    mSavedCheckpoint = 0;

    ALOGI("Attempting to load: %s", mSaveFileName);
    FILE *f = fopen(mSaveFileName, "r");
    bool hasLocalFile = false;
    if (f) {
        hasLocalFile = true;
        ALOGI("File found. Loading data.");
        if (1 != fscanf(f, "v1 %d", &mSavedCheckpoint)) {
            ALOGE("Error parsing save file.");
            mSavedCheckpoint = 0;
        } else {
            ALOGI("Loaded. Level = %d", mSavedCheckpoint);
            mSavedCheckpoint = (mSavedCheckpoint / LEVELS_PER_CHECKPOINT) * LEVELS_PER_CHECKPOINT;
            ALOGI("Normalized check-point: level %d", mSavedCheckpoint);
        }
        fclose(f);
    } else {
        ALOGI("Save file not present.");
    }

    // check cloud save.
    ALOGI("Checking cloud save data.");
    if (true) {
        ALOGI("No cloud save available because we are not signed in.");
        mUseCloudSave = false;
    }

    if (mUseCloudSave && hasLocalFile) {
        // since we're using cloud save, we can delete the local progress file
        ALOGI("Since we're using cloud save, deleting local progress file %s", mSaveFileName);
        if (0 != remove(mSaveFileName)) {
            ALOGW("WARNING: failed to remove local progress file.");
        }
    }

    ALOGI("Final decision on starting level: %d", mSavedCheckpoint);
    ALOGI("Final decision on whether to use cloud: %s", mUseCloudSave ? "USE CLOUD" :
                                                        "DO NOT USE CLOUD (failed)");
}

void PlayScene::WriteSaveFile(int level) {
    ALOGI("Saving progress (level %d) to file: %s", level, mSaveFileName);
    FILE *f = fopen(mSaveFileName, "w");
    if (!f) {
        ALOGE("Error writing to save game file.");
        return;
    }
    fprintf(f, "v1 %d", level);
    fclose(f);
    ALOGI("Save file written.");
}

void PlayScene::SaveProgress() {
    if (mDifficulty <= mSavedCheckpoint) {
        // nothing to do
        ALOGI("No need to save level, current = %d, saved = %d", mDifficulty, mSavedCheckpoint);
        return;
    } else if (!IsCheckpointLevel()) {
        ALOGI("Current level %d is not a checkpoint level. Nothing to save.", mDifficulty);
        return;
    }

    mSavedCheckpoint = mDifficulty;

    // Save state locally or to the cloud, depending on configuration:
    if (mUseCloudSave) {
        ALOGI("Saving progress to the cloud: level %d", mDifficulty);
        /*
         * No where to save
         */
    } else {
        ALOGI("Saving progress to LOCAL FILE: level %d", mDifficulty);
        WriteSaveFile(mDifficulty);
    }

    // Show a "checkpoint saved" sign when possible. We don't show it right away
    // because will already be showing the "Level N" sign, so we just set this flag
    // to remind us to show it right after.
    mCheckpointSignPending = true;
}

static unsigned char *_gen_wall_texture() {
    static unsigned char pixel_data[WALL_TEXTURE_SIZE * WALL_TEXTURE_SIZE * 3];
    unsigned char *p;
    int x, y;
    for (y = 0, p = pixel_data; y < WALL_TEXTURE_SIZE; y++) {
        for (x = 0; x < WALL_TEXTURE_SIZE; x++, p += 3) {
            p[0] = p[1] = p[2] = 128 + ((x > 2 && y > 2) ? Random(128) : 0);
        }
    }
    return pixel_data;
}

void PlayScene::OnStartGraphics() {
    // build shaders
    mOurShader = new OurShader();
    mOurShader->Compile();
    mTrivialShader = new TrivialShader();
    mTrivialShader->Compile();
    player->StartGraphics();

    // TODO: Rimuovere blocco di debug.
//    isOpen = openDebugFile();
    if(!( (++counter)%10 ))
        ShowSign(&""[counter],SIGN_DURATION);

    // build projection matrix
    UpdateProjectionMatrix();

    // build tunnel geometry
    mTunnelGeom = new SimpleGeom(
            new VertexBuf(TUNNEL_GEOM, sizeof(TUNNEL_GEOM), TUNNEL_GEOM_STRIDE),
            new IndexBuf(TUNNEL_GEOM_INDICES, sizeof(TUNNEL_GEOM_INDICES)));
    mTunnelGeom->vbuf->SetColorsOffset(TUNNEL_GEOM_COLOR_OFFSET);
    mTunnelGeom->vbuf->SetTexCoordsOffset(TUNNEL_GEOM_TEXCOORD_OFFSET);

    // build cube geometry (to draw obstacles)
    mCubeGeom = new SimpleGeom(new VertexBuf(CUBE_GEOM, sizeof(CUBE_GEOM), CUBE_GEOM_STRIDE));
    mCubeGeom->vbuf->SetColorsOffset(CUBE_GEOM_COLOR_OFFSET);
    mCubeGeom->vbuf->SetTexCoordsOffset(CUBE_GEOM_TEXCOORD_OFFSET);

    // make the wall texture
    TextureManager *textureManager = NativeEngine::GetInstance()->GetTextureManager();
    char textureName[32];
    for (int wallIndex = 0; wallIndex < MAX_WALL_TEXTURES; ++wallIndex) {
        snprintf(textureName, 32, "textures/texture%d.ktx", wallIndex + 1);
        uint64_t textureReference = textureManager->GetTextureReference(textureName);
        textureManager->LoadTexture(textureName);
        if (textureReference != TextureManager::INVALID_TEXTURE_REF) {
            uint32_t mipCount = textureManager->GetTextureMipCount(textureName);
            mWallTextures[wallIndex] = new Texture(static_cast<GLuint>(textureReference), mipCount);
            ++mActiveWallTextureCount;
        }
    }
    // If no loaded wall textures, use a random noise texture
    if (mActiveWallTextureCount == 0) {
        mWallTextures[0] = new Texture();
        mWallTextures[0]->InitFromRawRGB(WALL_TEXTURE_SIZE, WALL_TEXTURE_SIZE, false,
                                         _gen_wall_texture());
        ++mActiveWallTextureCount;
    }

    // reset frame clock so the animation doesn't jump
    mFrameClock.Reset();

    // life icon geometry
    mLifeGeom = AsciiArtToGeom(ART_LIFE, LIFE_ICON_SCALE);

    // create text renderer and shape renderer
    mTextRenderer = new TextRenderer(mTrivialShader);
    mShapeRenderer = new ShapeRenderer(mTrivialShader);
}

void PlayScene::OnKillGraphics() {
    CleanUp(&mTextRenderer);
    CleanUp(&mShapeRenderer);
    CleanUp(&mOurShader);
    CleanUp(&mTrivialShader);
    CleanUp(&mTunnelGeom);
    CleanUp(&mCubeGeom);
    for (int i = 0; i < mActiveWallTextureCount; ++i) {
        CleanUp(&mWallTextures[i]);
    }
    mActiveWallTextureCount = 0;
    CleanUp(&mLifeGeom);
    player->KillGraphics();
}

void PlayScene::DoFrame() {
    float deltaT = mFrameClock.ReadDelta();
    float previousY = mPlayerPos.y;

    // clear screen
    glClearColor(0.0, 0.0, 0.0, 1.0);
    glEnable(GL_DEPTH_TEST);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // rotate the view matrix according to current roll angle
    glm::vec3 upVec = glm::vec3(-sin(0), 0, cos(0));

    //camera posizionata in modo da vedere solo un lato del tunnel
    glm::vec3 cameraPos = glm::vec3(- 0.37f * mPlayerPos.x, mPlayerPos.y + screenAspect*10 - 1.15f*screenHeight/100, 1.86f*screenHeight/1000);


    // set up view matrix according to player's ship position and direction
    mViewMat = glm::lookAt(cameraPos, cameraPos + mPlayerDir, upVec);

    // render tunnel walls
    RenderTunnel();

    // render obstacles
    RenderObstacles();

    //render player icon
    player->Render(mTrivialShader, mTextRenderer, mShapeRenderer, UiWidget::FOCUS_NOT_APPLICABLE, 0.0f);

    if (mMenu) {
        RenderMenu();
        // nothing more to do
        return;
    }

    // render HUD (lives, score, etc)
    RenderHUD();

    // deduct from the time remaining to remove a sign from the screen
    if (mSignText && mSignExpires) {
        mSignTimeLeft -= deltaT;
        if (mSignTimeLeft < 0.0f) {
            mSignText = NULL;
        }
    }

    // if a "saved checkpoint" sign pending? Can we show it right now?
    if (!mSignText && mCheckpointSignPending) {
        mCheckpointSignPending = false;
        ShowSign(S_CHECKPOINT_SAVED, SIGN_DURATION);
    }

    // did we already show the howto?
    if (!mShowedHowto && mDifficulty == 0) {
        mShowedHowto = true;
        ShowSign(S_HOWTO_WITHOUT_JOY, SIGN_DURATION);
    }

    // deduct from the time remaining on the blinking heart animation
    if (mBlinkingHeart && Clock() > mBlinkingHeartExpire) {
        mBlinkingHeart = false;
    }

    // update speed
    float targetSpeed = PLAYER_SPEED + PLAYER_SPEED_INC_PER_LEVEL * mDifficulty;
    float accel = mPlayerSpeed >= 0.0f ? PLAYER_ACCELERATION_POSITIVE_SPEED :
                  PLAYER_ACCELERATION_NEGATIVE_SPEED;

    if (mLives <= 0) {
        targetSpeed = 0.0f;
    }
    mPlayerSpeed = Approach(mPlayerSpeed, targetSpeed, deltaT * accel);

    // apply noise filter on steering
    mFilteredSteerX = (mFilteredSteerX * (NOISE_FILTER_SAMPLES - 1) + mShipSteerX)
                      / NOISE_FILTER_SAMPLES;
    mFilteredSteerZ = (mFilteredSteerZ * (NOISE_FILTER_SAMPLES - 1) + mShipSteerZ)
                      / NOISE_FILTER_SAMPLES;

    // move player
    if (mLives > 0) {

        //implemented jump movement and deleted joystick usability
        if (mSteering == STEERING_TOUCH) {
            // touch steering
            //mPlayerPos.z += 1/15.0; //jetpack joyride style

            if(!pointerDownTimer){
                //jump finished
                mSteering = STEERING_NONE;

                // TODO: Rimuovere blocco debug.
                if(canDebug()){
                    if(isOnTop)
                        fprintf(outFile,"ON THE ROOF\n");
                    else
                        fprintf(outFile,"ON THE FLOOR\n");
                    beginDebug();
                    nDebug--;
                }

                //jumpSpeed set to maximum of 5.0f
                if((jumpHeight * (mDifficulty+1)) > MAX_JUMP_SPEED)
                    jumpSpeed = MAX_JUMP_SPEED;
                else
                    jumpSpeed = jumpHeight * (mDifficulty+1);
                halfJumpTime = DEFAULT_JUMP_TIME / (2 * (mDifficulty+1));
            } else {
                //mPlayerPos.z = Approach(mPlayerPos.z, steerZ, PLAYER_MAX_LAT_SPEED * deltaT);

                if(pointerDownTimer > halfJumpTime) {
                    //first half of action, jump of dim 2
                    mPlayerPos.z += HEIGHT_DELTA;
                    playerIconPos.y += HEIGHT_DELTA / 10;
                    player->SetCenter(playerIconPos.x, playerIconPos.y);
                }
                else {

                    // TODO: Rimuovere blocco debug.
                    if(canDebug() && pointerDownTimer==halfJumpTime){
                        fprintf(outFile,"MAX PEAK");
                        beginDebug();
                    }

                    //second half of action, falling
                    mPlayerPos.z -= HEIGHT_DELTA;
                    playerIconPos.y -= HEIGHT_DELTA / 10;
                    player->SetCenter(playerIconPos.x, playerIconPos.y);
                }
                pointerDownTimer--;
            }
        }  //else mPlayerPos.z -= 1/15.0; //jetpack joyride style
    }
    mPlayerPos.y += deltaT * mPlayerSpeed;  // run

    // make sure player didn't leave tunnel
    mPlayerPos.x = Clamp(mPlayerPos.x, PLAYER_MIN_X, PLAYER_MAX_X);
    mPlayerPos.z = Clamp(mPlayerPos.z, PLAYER_MIN_Z, PLAYER_MAX_Z);

    // shift sections if needed
    ShiftIfNeeded();

    // generate more obstacles!
    GenObstacles();

    // detect collisions
    DetectCollisions(previousY);

    // update ship's roll speed according to level
    static float roll_speeds[] = ROLL_SPEEDS;
    int count = sizeof(roll_speeds) / sizeof(float);
    float speed = roll_speeds[mDifficulty % count];
    mRollAngle += deltaT * speed;
    while (mRollAngle < 0) {
        mRollAngle += 2 * M_PI;
    }
    while (mRollAngle > 2 * M_PI) {
        mRollAngle -= 2 * M_PI;
    }

    // did the game expire?
    if (mLives <= 0 && Clock() > mGameOverExpire) {
        SceneManager::GetInstance()->RequestNewScene(new PlayScene());

    }

    // produce the ambient sound
    int soundPoint = (int) floor(mPlayerPos.y / (TUNNEL_SECTION_LENGTH / 5));
    if (soundPoint % 5 != 0 && soundPoint > mLastAmbientBeepEmitted) {
        mLastAmbientBeepEmitted = soundPoint;
        switch(soundPoint % 3){
            case 0:
                SfxMan::GetInstance()->PlayTone(TONE_AMBIENT_1);
                break;
            case 1:
                SfxMan::GetInstance()->PlayTone(TONE_AMBIENT_2);
                break;
            case 2:
                SfxMan::GetInstance()->PlayTone(TONE_AMBIENT_7);
                break;
        }
    }
}

static float GetSectionCenterY(int i) {
    return (float) i * TUNNEL_SECTION_LENGTH;
}

static float GetSectionEndY(int i) {
    return GetSectionCenterY(i) + 0.5f * TUNNEL_SECTION_LENGTH;
}

static void _get_obs_color(int style, float *r, float *g, float *b) {
    style = Clamp(style, 1, 6);
    *r = OBS_COLORS[style * 3];
    *g = OBS_COLORS[style * 3 + 1];
    *b = OBS_COLORS[style * 3 + 2];
}

void PlayScene::RenderTunnel() {
    glm::mat4 modelMat;
    glm::mat4 mvpMat;
    int i, oi;

    mOurShader->BeginRender(mTunnelGeom->vbuf);
    mOurShader->SetTexture(mWallTextures[0]);
    for (i = mFirstSection, oi = 0; i <= mFirstSection + RENDER_TUNNEL_SECTION_COUNT; ++i, ++oi) {
        float segCenterY = GetSectionCenterY(i);
        modelMat = glm::translate(glm::mat4(1.0), glm::vec3(0.0, segCenterY, 0.0));
        mvpMat = mProjMat * mViewMat * modelMat;

        Obstacle *o = oi >= mObstacleCount ? NULL : GetObstacleAt(oi);

        // the point light is given in model coordinates, which is 0,0,0 is ok (center of
        // tunnel section)
        if (o) {
            float red, green, blue;
            _get_obs_color(o->style, &red, &green, &blue);
            mOurShader->EnablePointLight(glm::vec3(0.0, 0.0f, 0.0f), red, green, blue);
        } else {
            mOurShader->DisablePointLight();
        }

        // render tunnel section
        mOurShader->Render(mTunnelGeom->ibuf, &mvpMat);
    }
    mOurShader->EndRender();
}

void PlayScene::RenderObstacles() {
    int i;
    int r, c;
    float red, green, blue;
    glm::mat4 modelMat;
    glm::mat4 mvpMat;

    mOurShader->BeginRender(mCubeGeom->vbuf);
    mOurShader->SetTexture(mWallTextures[0]);

    for (i = 0; i < mObstacleCount; i++) {
        Obstacle *o = GetObstacleAt(i);
        float posY = GetSectionCenterY(mFirstSection + i);

        if (o->style == Obstacle::STYLE_NULL) {
            // don't render null obstacles
            continue;
        }

        for (r = 0; r < OBS_GRID_SIZE; r++) {
            for (c = 0; c < OBS_GRID_SIZE; c++) {
                bool isBonus = r == o->bonusRow && c == o->bonusCol;
                if (o->grid[c][r]) {
                    // set up matrices
                    modelMat = glm::translate(glm::mat4(1.0f), o->GetBoxCenter(c, r, posY));
                    modelMat = glm::scale(modelMat, o->GetBoxSize(c, r));
                    mvpMat = mProjMat * mViewMat * modelMat;

                    // set up color
                    _get_obs_color(o->style, &red, &green, &blue);
                    mOurShader->SetTintColor(red, green, blue);

                    // render box
                    mOurShader->Render(&mvpMat);
                } else if (isBonus) {
                    modelMat = glm::translate(glm::mat4(1.0f), o->GetBoxCenter(c, r, posY));
                    modelMat = glm::scale(modelMat, glm::vec3(OBS_BONUS_SIZE, OBS_BONUS_SIZE,
                                                              OBS_BONUS_SIZE));
                    modelMat = glm::rotate(modelMat, Clock() * 90.0f, glm::vec3(0.0f, 0.0f, 1.0f));
                    mvpMat = mProjMat * mViewMat * modelMat;
                    mOurShader->SetTintColor(SineWave(0.8f, 1.0f, 0.5f, 0.0f),
                                             SineWave(0.8f, 1.0f, 0.5f, 0.0f),
                                             SineWave(0.8f, 1.0f, 0.5f, 0.0f)); // shimmering color
                    mOurShader->Render(&mvpMat); // render
                }
            }
        }
    }
    mOurShader->EndRender();
}

void PlayScene::GenObstacles() {
    while (mObstacleCount < MAX_OBS) {
        // generate a new obstacle
        int index = (mFirstObstacle + mObstacleCount) % MAX_OBS;

        int section = mFirstSection + mObstacleCount;
        if (section < OBS_START_SECTION) {
            // generate an empty obstacle
            mObstacleCircBuf[index].Reset();
            mObstacleCircBuf[index].style = Obstacle::STYLE_NULL;
        } else {
            // generate a normal obstacle
            mObstacleGen.Generate(&mObstacleCircBuf[index]);
        }
        mObstacleCount++;
    }
}

void PlayScene::ShiftIfNeeded() {
    // is it time to discard a section and shift forward?
    while (mPlayerPos.y > GetSectionEndY(mFirstSection) + SHIFT_THRESH) {
        // shift to the next turnnel section
        mFirstSection++;

        // discard obstacle corresponding to the deleted section
        if (mObstacleCount > 0) {
            // discarding first object (shifting) is easy because it's a circular buffer!
            mFirstObstacle = (mFirstObstacle + 1) % MAX_OBS;
            --mObstacleCount;
        }
    }
}

void PlayScene::UpdateMenuSelFromTouch(float x, float y) {
    float sh = SceneManager::GetInstance()->GetScreenHeight();
    int item = (int) floor((y / sh) * (mMenuItemCount));
    mMenuSel = Clamp(item, 0, mMenuItemCount - 1);
}

// Converted OnPointerDown and OnPointerUp to sort of clickListener
// The player is now jumping
void PlayScene::OnPointerDown(int pointerId, const struct PointerCoords *coords) {

    // TODO: Rimuovere blocco di debug.
    if(canDebug()){
        fprintf(outFile,"START JUMP");
        beginDebug();
    }

    float x = coords->x, y = coords->y;
    if (mMenu) {
        if (coords->isScreen) {
            UpdateMenuSelFromTouch(x, y);
            mMenuTouchActive = true;
        }
    } else if(!pointerDownTimer) {
        mPointerId = pointerId;
        mPointerAnchorX = x;
        mPointerAnchorY = y;
        //mPlayerPos.z = mPlayerPos.z + 1;

        // manages timers, checking if the player is on the floor or not.
        if(restoreTimer())
            halfJumpTime += pointerDownTimer;
        else halfJumpTime = DEFAULT_JUMP_TIME / (2 * (mDifficulty+1));
        pointerDownTimer += DEFAULT_JUMP_TIME / (mDifficulty+1);

        //mShipAnchorX = mPlayerPos.x;
        //mShipAnchorZ = mPlayerPos.z;
        mSteering = STEERING_TOUCH;
    }
}


void PlayScene::OnPointerUp(int pointerId, const struct PointerCoords *coords) {
    if (mMenu && mMenuTouchActive) {
        if (coords->isScreen) {
            mMenuTouchActive = false;
            HandleMenu(mMenuItems[mMenuSel]);
        }
    } else if (pointerId == mPointerId) {
        //mPlayerPos.z = mPlayerPos.z - 1;
        //mSteering = STEERING_NONE;
    }
}

void PlayScene::OnPointerMove(int pointerId, const struct PointerCoords *coords) {
    float rangeY = coords->isScreen ? SceneManager::GetInstance()->GetScreenHeight() :
                   (coords->maxY - coords->minY);
    float x = coords->x, y = coords->y;

    if (mMenu && mMenuTouchActive) {
        UpdateMenuSelFromTouch(x, y);
    } else if (mSteering == STEERING_TOUCH && pointerId == mPointerId) {
        float deltaX = (x - mPointerAnchorX) * TOUCH_CONTROL_SENSIVITY / rangeY;
        float deltaY = -(y - mPointerAnchorY) * TOUCH_CONTROL_SENSIVITY / rangeY;
        float rotatedDx = cos(mRollAngle) * deltaX - sin(mRollAngle) * deltaY;
        float rotatedDy = sin(mRollAngle) * deltaX + cos(mRollAngle) * deltaY;

        mShipSteerX = mShipAnchorX + rotatedDx;
        mShipSteerZ = mShipAnchorZ + rotatedDy;
    }
}

void PlayScene::RenderHUD() {
    float aspect = SceneManager::GetInstance()->GetScreenAspect();
    glm::mat4 orthoMat = glm::ortho(0.0f, aspect, 0.0f, 1.0f);
    glm::mat4 modelMat;
    glm::mat4 mat;

    glDisable(GL_DEPTH_TEST);

    // render score digits
    int i, unit;
    static char score_str[6];
    int score = GetScore();
    for (i = 0, unit = 10000; i < 5; i++, unit /= 10) {
        score_str[i] = '0' + (score / unit) % 10;
    }
    score_str[i] = '\0';

    mTextRenderer->SetFontScale(SCORE_FONT_SCALE);
    mTextRenderer->RenderText(score_str, SCORE_POS_X, SCORE_POS_Y);

    // render current sign
    if (mSignText) {
        modelMat = glm::mat4(1.0f);
        float t = Clock() - mSignStartTime;
        if (t < SIGN_ANIM_DUR) {
            float scale = t / SIGN_ANIM_DUR;
            modelMat = glm::scale(modelMat, glm::vec3(1.0f, scale, 1.0f));
        } else if (mSignTimeLeft < SIGN_ANIM_DUR) {
            float scale = mSignTimeLeft / SIGN_ANIM_DUR;
            modelMat = glm::scale(modelMat, glm::vec3(1.0f, scale, 1.0f));
        }

        mTextRenderer->SetMatrix(modelMat);
        mTextRenderer->SetFontScale(SIGN_FONT_SCALE);
        mTextRenderer->RenderText(mSignText, aspect * 0.5f, 0.5f);
        mTextRenderer->ResetMatrix();
    }

    // render life icons
    glLineWidth(LIFE_LINE_WIDTH);
    float lifeX = LIFE_POS_X < 0.0f ? aspect + LIFE_POS_X : LIFE_POS_X;
    modelMat = glm::translate(glm::mat4(1.0), glm::vec3(lifeX, LIFE_POS_Y, 0.0f));
    modelMat = glm::scale(modelMat, glm::vec3(1.0f, LIFE_SCALE_Y, 1.0f));
    int ubound = (mBlinkingHeart && BlinkFunc(0.2f)) ? mLives + 1 : mLives;
    for (int idx = 0; idx < ubound; idx++) {
        mat = orthoMat * modelMat;
        mTrivialShader->RenderSimpleGeom(&mat, mLifeGeom);
        modelMat = glm::translate(modelMat, glm::vec3(LIFE_SPACING_X, 0.0f, 0.0f));
    }

    glEnable(GL_DEPTH_TEST);
}

void PlayScene::RenderMenu() {
    float aspect = SceneManager::GetInstance()->GetScreenAspect();
    // NCT_COMMENT glm::mat4 modelMat;
    // NCT_COMMENT glm::mat4 mat;

    glDisable(GL_DEPTH_TEST);

    //RenderBackgroundAnimation(mShapeRenderer);

    float scaleFactor = SineWave(1.0f, MENUITEM_PULSE_AMOUNT, MENUITEM_PULSE_PERIOD, 0.0f);

    int i;
    for (i = 0; i < mMenuItemCount; i++) {
        float thisFactor = (mMenuSel == i) ? scaleFactor : 1.0f;
        float y = 1.0f - (i + 1) / ((float) mMenuItemCount + 1);
        float x = aspect * 0.5f;
        mTextRenderer->SetFontScale(thisFactor * MENUITEM_FONT_SCALE);
        mTextRenderer->SetColor(mMenuSel == i ? MENUITEM_SEL_COLOR : MENUITEM_COLOR);
        mTextRenderer->RenderText(mMenuItemText[mMenuItems[i]], x, y);
    }
    mTextRenderer->ResetColor();

    glEnable(GL_DEPTH_TEST);
}

void PlayScene::DetectCollisions(float previousY) {
    Obstacle *o = GetObstacleAt(0);
    float obsCenter = GetSectionCenterY(mFirstSection);
    float obsMin = obsCenter - OBS_BOX_SIZE;
    float curY = mPlayerPos.y;

    if (!o || !(previousY < obsMin && curY >= obsMin)) {

        // if it's not on the obstacle anymore, the player is free to move.
        if(!isOnTop && restoreTimer()){
            mSteering = STEERING_TOUCH;
        }
        isOnTop = false;

        // no collision
        return;
    }

    // corrects the player's position if it's at the top of the obstacle.
    else if( pointerDownTimer+1 >= halfJumpTime/2 && pointerDownTimer+1 < halfJumpTime){

        // TODO: Rimuovere blocco di debug.
        if(canDebug()){
            fprintf(outFile,"ON THE TOP.\n");
            beginDebug();
        }

        isOnTop = true;
        // mantiene la posizione se tocca l'ostacolo da sopra.
        mPlayerPos.z += HEIGHT_DELTA;
        playerIconPos.y += HEIGHT_DELTA / 10;
        player->SetCenter(playerIconPos.x, playerIconPos.y);
        storedPointerDownTimer = pointerDownTimer+1;
        pointerDownTimer = 0;
        mSteering = STEERING_NONE;
    }
    // if it's not at the top of the obstacle, the player is free to move.
    else{
        isOnTop = false;

        // TODO: Rimuovere blocco di debug.
        if(canDebug()){
            fprintf(outFile,"FALLING FROM ROOF.\n");
            beginDebug();
        }

        mSteering = STEERING_TOUCH;
    }

    // what row/column is the player on?
    int col = o->GetColAt(mPlayerPos.x);
    int row = o->GetRowAt(mPlayerPos.z);

    // TODO: Rimuovere blocco di debug.
    if(canDebug()){
        fprintf(outFile,"[col  row]: [ %d  %d ]\n",col,row);
        beginDebug();
    }

    if (o->grid[col][row] && !isOnTop) {

        // TODO: Rimuovere blocco di debug.
        if(canDebug()){
            fprintf(outFile,"CRASHED BANDICOOT.\n");
            beginDebug();
        }

        // crashed against obstacle
        // TODO: Rimuovere modalità GOD MODE
//        mLives--;

        if (mLives > 0) {
            ShowSign(S_OUCH, SIGN_DURATION);
            SfxMan::GetInstance()->PlayTone(TONE_CRASHED);
        } else {
            // say "Game Over"
            ShowSign(S_GAME_OVER, SIGN_DURATION_GAME_OVER);
            SfxMan::GetInstance()->PlayTone(TONE_GAME_OVER);
            mGameOverExpire = Clock() + GAME_OVER_EXPIRE;
        }
        mPlayerPos.y = obsMin - PLAYER_RECEDE_AFTER_COLLISION;
        mPlayerSpeed = PLAYER_SPEED_AFTER_COLLISION;
        mBlinkingHeart = true;
        mBlinkingHeartExpire = Clock() + BLINKING_HEART_DURATION;

        mLastCrashSection = mFirstSection;


        // if the column is the one with a bonus, and the bonus is taken directly or by staying at the top of the obstacle,
        // then the bonus is obtained.
    } else if ( col == o->bonusCol && (isOnTop || row == o->bonusRow) ) {
        //ShowSign(S_GOT_BONUS, SIGN_DURATION_BONUS);

        // TODO: Rimuovere blocco debug.
        if(canDebug()){
            fprintf(outFile,"BONUS ACHIEVED.\n");
            beginDebug();
        }

        o->DeleteBonus();
        AddScore(BONUS_POINTS);
        mBonusInARow++;

        addScoreSign(true);

        if (mBonusInARow >= 10) {
            mBonusInARow = 0;
        }

        // update difficulty level, if applicable
        //Max level = 3, mDifficulty = 2
        int score = GetScore();

        if (mDifficulty < score / SCORE_PER_LEVEL && mDifficulty < MAX_DIFFICULTY) {
            mDifficulty = score / SCORE_PER_LEVEL;
            ShowLevelSign();
            mObstacleGen.SetDifficulty(mDifficulty);
            SfxMan::GetInstance()->PlayTone(TONE_LEVEL_UP);

            // save progress, if needed
            SaveProgress();
        } else {
            int tone = (score % SCORE_PER_LEVEL) / BONUS_POINTS - 1;
            tone = tone < 0 ? 0 :
                   tone >= static_cast<int>(sizeof(TONE_BONUS) / sizeof(char *)) ?
                   static_cast<int>(sizeof(TONE_BONUS) / sizeof(char *) - 1) : tone;
            SfxMan::GetInstance()->PlayTone(TONE_BONUS[tone]);
        }

    } else if (o->HasBonus()) {
        // player missed bonus!
        mBonusInARow = 0;
    }

    // was it a close call?
    if (!o->grid[col][row]) {
        bool isCloseCall = false;
        for (int i = -1; i <= 1 && !isCloseCall; i++) {
            for (int j = -1; j <= 1; j++) {
                int other_row = o->GetColAt(mPlayerPos.x + i * CLOSE_CALL_CALC_DELTA);
                int other_col = o->GetRowAt(mPlayerPos.z + j * CLOSE_CALL_CALC_DELTA);
                if (o->grid[other_col][other_row]) {
                    isCloseCall = true;
                    break;
                }
            }
        }

    }
}

bool PlayScene::OnBackKeyPressed() {
    if (mMenu) {
        // reset frame clock so that the animation doesn't jump:
        mFrameClock.Reset();

        // leave menu
        ShowMenu(MENU_NONE);
    } else {
        // enter pause menu
        ShowMenu(MENU_PAUSE);
    }
    return true;
}


void PlayScene::OnJoy(float joyX, float joyY) {
    if (!mSteering || mSteering == STEERING_JOY) {
        float deltaX = joyX * JOYSTICK_CONTROL_SENSIVITY;
        float deltaY = joyY * JOYSTICK_CONTROL_SENSIVITY;
        float rotatedDx = cos(-mRollAngle) * deltaX - sin(-mRollAngle) * deltaY;
        float rotatedDy = sin(-mRollAngle) * deltaX + cos(-mRollAngle) * deltaY;
        mShipSteerX = rotatedDx;
        mShipSteerZ = -rotatedDy;
        mSteering = STEERING_JOY;

        // If player is going faster than the reference speed, PLAYER_SPEED, adjust it.
        // This makes the steering react faster as the ship accelerates in more difficult
        // levels.
        if (mPlayerSpeed > PLAYER_SPEED) {
            mShipSteerX *= mPlayerSpeed / PLAYER_SPEED;
            mShipSteerZ *= mPlayerSpeed / PLAYER_SPEED;
        }
    }
}

void PlayScene::OnKeyDown(int keyCode) {
    if (mMenu) {
        if (keyCode == OURKEY_UP) {
            mMenuSel = mMenuSel > 0 ? mMenuSel - 1 : mMenuSel;
        } else if (keyCode == OURKEY_DOWN) {
            mMenuSel = mMenuSel + 1 < mMenuItemCount ? mMenuSel + 1 : mMenuSel;
        } else if (keyCode == OURKEY_ENTER) {
            HandleMenu(mMenuItems[mMenuSel]);
        }
    }
}

void PlayScene::ShowMenu(int menu) {
    mMenu = menu;
    mMenuSel = 0;
    switch (menu) {
        case MENU_PAUSE:
            mMenuItems[0] = MENUITEM_UNPAUSE;
            mMenuItems[1] = MENUITEM_QUIT;
            mMenuItemCount = 2;
            break;
        case MENU_LEVEL:
            mMenuItems[0] = MENUITEM_RESUME;
            mMenuItems[1] = MENUITEM_START_OVER;
            mMenuItemCount = 2;
            break;
        default:
            // since we're leaving the menu, reset the frame clock to avoid a skip
            // in the animation
            mFrameClock.Reset();
    }
}

void PlayScene::HandleMenu(int menuItem) {
    switch (menuItem) {
        case MENUITEM_QUIT:
            SceneManager::GetInstance()->RequestNewScene(new WelcomeScene());
            break;
        case MENUITEM_UNPAUSE:
            ShowMenu(MENU_NONE);
            break;
        case MENUITEM_RESUME:
            // resume from saved level
            //mDifficulty = (mSavedCheckpoint / LEVELS_PER_CHECKPOINT) * LEVELS_PER_CHECKPOINT;

            if(mSavedCheckpoint >= MAX_DIFFICULTY)
                mDifficulty = MAX_DIFFICULTY;
            else
                mDifficulty = 0;
            SetScore(SCORE_PER_LEVEL * mDifficulty);
            mObstacleGen.SetDifficulty(mDifficulty);
            ShowLevelSign();
            ShowMenu(MENU_NONE);
            break;
        case MENUITEM_START_OVER:
            // start over from scratch
            ShowMenu(MENU_NONE);
            break;
    }
}

void PlayScene::ShowLevelSign() {
    static char level_str[] = "LEVEL XX";
    int level = mDifficulty + 1;
    level_str[6] = '0' + ((level > 9) ? (level / 10) % 10 : level % 10);
    level_str[7] = (level > 9) ? ('0' + level % 10) : '\0';
    level_str[8] = '\0';
    ShowSign(level_str, SIGN_DURATION);
}

void PlayScene::OnPause() {
    if (mMenu == MENU_NONE) {
        ShowMenu(MENU_PAUSE);
    }
}

void PlayScene::OnScreenResized(int width, int height) {
    UpdateProjectionMatrix();
}

void PlayScene::UpdateProjectionMatrix() {
    SceneManager *mgr = SceneManager::GetInstance();
    mProjMat = glm::perspective(RENDER_FOV, mgr->GetScreenAspect(), RENDER_NEAR_CLIP,
                                RENDER_FAR_CLIP);
}

//new method for player representation
UiWidget* PlayScene::NewPlayer() {
    UiWidget* widget = new UiWidget(0);
    widget->SetCenter(0.174f * screenAspect, 0.35f);
    widget->SetBackColor(1.0f, 0.4f, 0.6f);
    widget->SetIsButton(false);
    widget->SetSize(0.1f, 0.1f);
    widget->SetText(" ");
    widget->SetTextColor(0.7f, 0.3f, 0.9f);
    widget->SetHasBorder(true);
    return widget;
}

bool PlayScene::restoreTimer(){
    if(storedPointerDownTimer > 0){
        pointerDownTimer += storedPointerDownTimer;
        storedPointerDownTimer = 0;
        return true;
    }
    else return false;
}

void PlayScene::addScoreSign(bool hasBonus) {
    if(checkExtraLife() && hasBonus){
        addExtralife();
        ShowSign(S_GOT_BONUS "\n" S_EXTRA_LIFE, SIGN_DURATION_BONUS);
        return;
    }
    if(hasBonus){
        ShowSign(S_GOT_BONUS, SIGN_DURATION_BONUS);
        return;
    }
    if(checkExtraLife()){
        addExtralife();
        ShowSign(S_EXTRA_LIFE,SIGN_DURATION);
        return;
    }
}

bool PlayScene::checkExtraLife() {
    return GetScore() >= (POINTS_FOR_EXTRA_LIFE * (extraLifeCounter+1));
}

void PlayScene::addExtralife() {
    // Aggiunto suono quando c'e' una vita extra.
    SfxMan::GetInstance()->PlayTone(TONE_LEVEL_UP);
    mLives++;
    extraLifeCounter++;
}

void PlayScene::beginDebug(){
    if(!canDebug()) return;
    fprintf(outFile,DEBUG_STRING,
            mPlayerPos.x,mPlayerPos.y,mPlayerPos.z,playerIconPos.x,playerIconPos.y);
}

bool PlayScene::canDebug() {
    if(!outFile){
        ShowSign("FILE ERROR.",SIGN_DURATION);
        return false;
    }
    if(!nDebug){
        ShowSign("END DEBUG",SIGN_DURATION);
        fclose(outFile);
        return false;
    }
    return true;
}