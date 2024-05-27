#include "game.h"

void Game::bmsInit(bool dan) {
	audio.portAudioExit();
	audio.portAudioInit(audioDeviceIndex);
	static std::filesystem::path previousFilePath = {};
	std::filesystem::path dirPath = dan ? bmsPath / danDirPath : bmsPath / bmsDir[bmsSelect];
	std::filesystem::path filePath = dan ? dirPath / danFilePath : dirPath / bmsFileDir[bmsSelect][bmsFileSelect];
	if (previousFilePath != filePath) {
		previousFilePath = filePath;
		bmsParser.clear();
		bmsParser.parseFile(filePath.string().c_str());
		for (int wav = 0; wav < 1536; wav++) {
			audio.offloadSound(wav);
			if (!bmsParser.wav[wav].empty()) {
				audio.loadSound(wav, (dirPath / std::filesystem::path(bmsParser.wav[wav])).string().c_str());
			}
			else {
				audio.loadSound(wav, "0");
			}
		}
	}
	else {
		bmsParser.clear();
		bmsParser.parseFile(filePath.string().c_str());
	}
	if (rate != 100) {
		for (int wav = 0; wav < 1536; wav++) {
			audio.offloadNewSound(wav);
			audio.changeRate(wsola, rate, wav);
		}
	}
	double offset = SDL_GetTicks64();
	double currentBpm = bmsParser.bpm * rate / 100;
	for (int bar = 0; bar <= bmsParser.barMax; bar++) {
		double barLength;
		if (!bmsParser.bpmInt[bar].empty()) { // Test
			currentBpm = bmsParser.bpmInt[bar][0] * rate / 100;
		}
		barLength = (double)4 / currentBpm * 60000;
		barLength = bmsParser.timeSignature[bar] ? barLength * bmsParser.timeSignature[bar] : barLength;
		for (int key = 0; key < 4; key++) {
			int sizeNote = bmsParser.note[bar][key].size();
			double keyLength = barLength / sizeNote;
			double keyOffset = offset;
			for (int note = 0; note < sizeNote; note++) {
				if (bmsParser.note[bar][key][note]) {
					chart.add(key, { (Uint64)keyOffset, bmsParser.note[bar][key][note] });
				}
				keyOffset += keyLength;
			}
		}
		int sizeIdx = bmsParser.bgm[bar].size();
		for (int idx = 0; idx < sizeIdx; idx++) {
			int sizeBgm = bmsParser.bgm[bar][idx].size();
			double keyLength = barLength / sizeBgm;
			double keyOffset = offset;
			for (int bgm = 0; bgm < sizeBgm; bgm++) {
				if (bmsParser.bgm[bar][idx][bgm]) {
					chart.add(4, { (Uint64)keyOffset, bmsParser.bgm[bar][idx][bgm] });
				}
				keyOffset += keyLength;
			}
		}
		offset += barLength;
	}
	chartVisible.init(&chart);
}

void Game::rateWrite(std::string iniPath) {
	mINI::INIFile iniFile(iniPath);
	mINI::INIStructure ini;
	ini["PatternParameter"]["rate"] = std::to_string(rate);
	//iniFile.write(ini);
	iniFile.generate(ini);
	
}

void Game::init() {
	// Config init
	mINI::INIFile iniFileGlobal("config.ini");
	mINI::INIStructure iniGlobal;
	iniFileGlobal.read(iniGlobal);
	std::string textPath(std::string("texture/") + iniGlobal["Texture"]["directory"] + std::string("/"));
	mINI::INIFile iniFileTexture(textPath + std::string("config.ini"));
	mINI::INIStructure iniTexture;
	iniFileTexture.read(iniTexture);
	// Graphic init
	graphic.init(textPath, iniTexture);
	// Audio init
	audioDeviceIndex = std::stoi(iniGlobal["Audio"]["device"]);
	// Key map init
	keyMap[0] = SDL_SCANCODE_X;
	keyMap[1] = SDL_SCANCODE_C;
	keyMap[2] = SDL_SCANCODE_COMMA;
	keyMap[3] = SDL_SCANCODE_PERIOD;
	keyMap[4] = SDL_SCANCODE_ESCAPE;
	keyMap[5] = SDL_SCANCODE_RETURN;
	keyMap[6] = SDL_SCANCODE_KP_ENTER;
	keyMap[7] = SDL_SCANCODE_UP;
	keyMap[8] = SDL_SCANCODE_LEFT;
	keyMap[9] = SDL_SCANCODE_DOWN;
	keyMap[10] = SDL_SCANCODE_RIGHT;
	// Fps init
	fps.init();
	// Error meter
	errorMeter.setLifeTime(stoi(iniGlobal["ErrorMeter"]["liveTime"]));
	errorMeter.setScale(stof(iniGlobal["ErrorMeter"]["scale"]));
	scrollSpeed = stof(iniGlobal["ScrollSpeed"]["speed"]);
	//
	bmsPath = iniGlobal["BMSPath"]["directory"];
	int counter = 0;
	for (auto const& dir_entry : std::filesystem::directory_iterator{ bmsPath }) {
		if (dir_entry.is_directory()) {
			if (dir_entry.path().filename().string() == std::string("dan")) {
				danFile = counter;
				for (auto const& file_entry : std::filesystem::directory_iterator{ dir_entry }) {
					if (file_entry.is_regular_file() && file_entry.path().filename().extension().string() == std::string(".ini")) {
						danDir.push_back(file_entry.path().filename());
					}
				}
			}
			bmsDir.push_back(dir_entry.path().filename());
			std::vector<std::filesystem::path> bmsVarient;
			for (auto const& file_entry : std::filesystem::directory_iterator{ dir_entry }) {
				if (file_entry.is_regular_file() &&
					(file_entry.path().filename().extension().string() == std::string(".bms") ||
					file_entry.path().filename().extension().string() == std::string(".bme") ||
					file_entry.path().filename().extension().string() == std::string(".bml") ||
					file_entry.path().filename().extension().string() == std::string(".bm4"))) {
					bmsVarient.push_back(file_entry.path().filename());
				}
			}
			bmsFileDir.push_back(bmsVarient);
			counter++;
		}
	}
	wsola = stoi(iniGlobal["WSOLA"]["enable"]);
	debug = stoi(iniGlobal["Debug"]["enable"]);
	chartOffset = 3000;
	gameState = GameState::SelectGame;
	rate = 100;
}

void Game::exit() {
	graphic.exit();
	audio.portAudioExit();
	std::exit(0);
}

void Game::update() {
	graphic.clear();
	// Esc
	const Uint8* keyboardState = SDL_GetKeyboardState(NULL);
	bool keyTrigger = false; // For esc, enter, and arrow keys
	if (keyboardState[keyMap[4]]) {
		if (!keyPressed[4]) {
			keyPressed[4] = true;
			keyTrigger = true;
		}
	}
	else {
		keyPressed[4] = false;
	}
	// Enter and arrow keys
	for (int key = 5; key < 11; key++) {
		if (keyboardState[keyMap[key]]) {
			if (SDL_GetTicks64() >= keyDelay[key]) {
				if (!keyPressed[key]) {
					keyDelay[key] = SDL_GetTicks64() + 300;
					keyPressed[key] = true;
				}
				else {
					keyDelay[key] = SDL_GetTicks64() + 20;
				}
				keyTrigger = true;
			}
		}
		else {
			keyDelay[key] = 0;
			keyPressed[key] = false;
		}
	}
	switch (gameState)
	{
	case GameState::SelectGame:
	{
		const Uint8* keyboardState = SDL_GetKeyboardState(NULL);
		if (keyTrigger) {
			if (keyPressed[4]) { // Esc
			}
			else if (keyPressed[5] || keyPressed[6]) { // Enter
				if (gameSelect == GameType::Train) {
					gameState = GameState::SelectBms;
				}
				else if (gameSelect == GameType::Dan) {
					gameState = GameState::SelectDan;
				}
				else if (gameSelect == GameType::Exit) {
					exit();
				}
			}
			else if (keyPressed[7]) { // Up
				gameSelect = (3 + gameSelect - 1) % 3;
			}
			else if (keyPressed[8]) { // Left
			}
			else if (keyPressed[9]) { // Down
				gameSelect = (3 + gameSelect + 1) % 3;
			}
			else if (keyPressed[10]) { // Right
			}
		}
		graphic.drawGameType(gameSelect);
		break;
	}
	case GameState::SelectDan:
	{
		const Uint8* keyboardState = SDL_GetKeyboardState(NULL);
		if (keyTrigger) {
			if (keyPressed[4]) { // Esc
				gameState = GameState::SelectGame;
			}
			else if (keyPressed[5] || keyPressed[6]) { // Enter
				gameState = GameState::PlayDan;
				mINI::INIFile iniFile(bmsPath.string() + std::string("/dan/") + danDir[danSelect].string());
				mINI::INIStructure ini;
				if (iniFile.read(ini)) {
					danDirPath = ini["PatternParameter1"]["dirPath"];
					danFilePath = ini["PatternParameter1"]["filePath"];
					rate = stoi(ini["PatternParameter1"]["rate"]);
				}
				//printf("%s\n", danDirPath.string().c_str());
				//printf("%s\n", danFilePath.string().c_str());
				//printf("BMSInit: Select Dan\n");
				bmsInit(true);
				score.hp = 500;
				danSeg = 0;
				score.clearSeg();
				score.clearDan();
			}
			else if (keyPressed[7]) { // Up
				danSelect = (danDir.size() + danSelect - 1) % danDir.size();
			}
			else if (keyPressed[8]) { // Left
			}
			else if (keyPressed[9]) { // Down
				danSelect = (danDir.size() + danSelect + 1) % danDir.size();
			}
			else if (keyPressed[10]) { // Right
			}
		}
		graphic.drawDan(danDir, danFile, danSelect);
		break;
	}
	case GameState::SelectBms:
	{
		const Uint8* keyboardState = SDL_GetKeyboardState(NULL);
		if (keyTrigger) {
			if (keyPressed[4]) { // Esc
				gameState = GameState::SelectGame;
			}
			else if (keyPressed[5] || keyPressed[6]) { // Enter
				if (!bmsFileDir[bmsSelect].empty()) {
					gameState = GameState::SelectFile;
				}
			}
			else if (keyPressed[7]) { // Up
				bmsSelect = (bmsDir.size() + bmsSelect - 1) % bmsDir.size();
			}
			else if (keyPressed[8]) { // Left
			}
			else if (keyPressed[9]) { // Down
				bmsSelect = (bmsDir.size() + bmsSelect + 1) % bmsDir.size();
			}
			else if (keyPressed[10]) { // Right
			}
		}
		graphic.drawBms(bmsDir, bmsSelect);
		break;
	}
	case GameState::SelectFile:
	{
		const Uint8* keyboardState = SDL_GetKeyboardState(NULL);
		if (keyTrigger) {
			if (keyPressed[4]) { // Esc
				gameState = GameState::SelectBms;
				bmsFileSelect = 0;
			}
			else if (keyPressed[5] || keyPressed[6]) { // Enter
				gameState = GameState::SelectDifficulty;
				std::filesystem::path iniPath = bmsPath / bmsDir[bmsSelect] / bmsFileDir[bmsSelect][bmsFileSelect];
				iniPath += std::string(".ini");
				mINI::INIFile iniFile(iniPath.string());
				mINI::INIStructure ini;
				if (iniFile.read(ini)) {
					rate = stoi(ini["PatternParameter"]["rate"]);
				}
			}
			else if (keyPressed[7]) { // Up
				bmsFileSelect = (bmsFileDir[bmsSelect].size() + bmsFileSelect - 1) % bmsFileDir[bmsSelect].size();
			}
			else if (keyPressed[8]) { // Left
			}
			else if (keyPressed[9]) { // Down
				bmsFileSelect = (bmsFileDir[bmsSelect].size() + bmsFileSelect + 1) % bmsFileDir[bmsSelect].size();
			}
			else if (keyPressed[10]) { // Right
			}
		}
		graphic.drawBmsFile(bmsFileDir, bmsSelect, bmsFileSelect);
		break;
	}
	case GameState::SelectDifficulty:
	{
		const Uint8* keyboardState = SDL_GetKeyboardState(NULL);
		if (keyTrigger) {
			if (keyPressed[4]) { // Esc
				gameState = GameState::SelectFile;
				//judgeKey.clear();
				std::filesystem::path iniPath = bmsPath / bmsDir[bmsSelect] / bmsFileDir[bmsSelect][bmsFileSelect];
				iniPath += std::string(".ini");
				rateWrite(iniPath.string());
			}
			else if (keyPressed[5] || keyPressed[6]) { // Enter
				gameState = GameState::Play;
				std::filesystem::path iniPath = bmsPath / bmsDir[bmsSelect] / bmsFileDir[bmsSelect][bmsFileSelect];
				iniPath += std::string(".ini");
				rateWrite(iniPath.string());
				//printf("BMSInit: Select Difficulty\n");
				bmsInit(false);
				score.hp = 500;
				score.clearSeg();
				score.clearDan();
			}
			else if (keyPressed[7]) { // Up
				int idx = difficultySelect;
				rate = std::min(rate + 1, 200);
			}
			else if (keyPressed[8]) { // Left
				//difficultySelect = difficultySelect ? difficultySelect - 1 : 7;
			}
			else if (keyPressed[9]) { // Down
				int idx = difficultySelect;
				rate = std::max(rate - 1, 50);
			}
			else if (keyPressed[10]) { // Right
				//difficultySelect = difficultySelect < 7 ? difficultySelect + 1 : 0;
			}
		}
		graphic.drawDifficultySelect(rate, difficultySelect);
		break;
	}
	case GameState::Play:
	case GameState::PlayDan:
	{
		// Key
		static JudgeKeySound judgeKeySound({ 0, 0 }, 6);
		if (score.missJudger(1/*Test*/, chartVisible, judgeNoteVisible, chartOffset)) {
			judgeKeySound.judge = 5;
		}
		const Uint8* keyboardState = SDL_GetKeyboardState(NULL);
		for (int key = 0; key < 4; key++) {
			if (keyboardState[keyMap[key]]) {
				if (!keyPressed[key]) {
					judgeKeySound = score.judger(1/*Test*/, key, chartVisible, judgeNoteVisible, errorMeter, chartOffset);
					if (judgeKeySound.sound) {
						audio.playSound(rate != 100, judgeKeySound.sound);
					}
					judgeKeyVisible.add(key, judgeKeySound);
					judgeKey.add(key, judgeKeySound);
					keyPressed[key] = true;
				}
			}
			else {
				keyPressed[key] = false;
			}
		}
		if (keyTrigger) {
			if (keyPressed[4]) { // Esc
				if (gameState == GameState::Play) {
					gameState = GameState::SelectDifficulty;
				}
				else {
					gameState = GameState::SelectDan;
				}
				chart.clear();
				chartVisible.clear();
				judgeKey.clear();
				audio.portAudioExit();
				audio.stopSound();
				break;
			}
		}
		judgeNoteVisible.update(chartOffset);
		judgeKeyVisible.update(chartOffset);
		chartVisible.update(&audio, &chart, chartOffset, rate != 100);
		errorMeter.update();
		// Test
		score.hp = 100;
		//

		if ((!chartVisible.count() && chartVisible.finished()) || score.hp <= 0) { // Chart end or hp == 0
			if (gameState == GameState::Play) {
				gameState = GameState::Result;
			}
			else {
				gameState = GameState::ResultDan;
			}
		}

		graphic.drawReceiptor();
		graphic.drawKeyPressed(keyPressed);
		graphic.drawNote(chartVisible, scrollSpeed, chartOffset);
		graphic.drawJudgeNote(judgeNoteVisible, chartOffset);
		graphic.drawJudgeKey(judgeKeyVisible, chartOffset);
		graphic.drawJudge(judgeKeySound.judge);
		graphic.drawErrorMeter(errorMeter.begin(), errorMeter.end(), errorMeter.getLifeTime(), errorMeter.getScale());
		char combo[32];
		sprintf(combo, "%d", score.getCombo());
		graphic.drawCombo(combo);
		//graphic.drawText();
		graphic.drawHp(score.hp);
		if (debug) {
			// Debug info
			char fpsBuffer[32] = {};
			sprintf(fpsBuffer, "FPS: %d", fps.get());
			char sizeBuffer[32] = {};
			sprintf(sizeBuffer, "Sound num: %d", audio.getCurrentSoundNum());
			char keyCountBufffer[32] = {};
			sprintf(keyCountBufffer, "Key pressed: %d", judgeKey.count());
			char chartVisibleBufffer[32] = {};
			sprintf(chartVisibleBufffer, "Visible note: %d", chartVisible.count());
			char chartBufffer[32] = {};
			sprintf(chartBufffer, "Chart note: %d", chart.count());
			char scoreV1Bufffer[32] = {};
			sprintf(scoreV1Bufffer, "ScoreV1: %.4f", score.getScoreV1());
			char scoreV2Bufffer[32] = {};
			sprintf(scoreV2Bufffer, "ScoreV2: %.4f", score.getScoreV2());
			char avgErrorBufffer[32] = {};
			sprintf(avgErrorBufffer, "AvgError: %.4f", score.getAvgError());
			char varianceBufffer[32] = {};
			sprintf(varianceBufffer, "Variance: %.4f", score.getVariance());
			char SDBufffer[32] = {};
			sprintf(SDBufffer, "SD: %.4f", score.getSD());
			char KPSBuffer[32] = {};
			sprintf(KPSBuffer, "KPS: %.1f", (float)judgeKeyVisible.count() / 2);
			char* debugText[12] = {
				fpsBuffer,
				sizeBuffer,
				keyCountBufffer,
				chartVisibleBufffer,
				chartBufffer,
				scoreV1Bufffer,
				scoreV2Bufffer,
				avgErrorBufffer,
				varianceBufffer,
				SDBufffer,
				KPSBuffer
			};
			graphic.drawDebug(debugText);
		}
		else {
			char fpsBuffer[32] = {};
			sprintf(fpsBuffer, "FPS: %d", fps.get());
			char scoreV1Bufffer[32] = {};
			sprintf(scoreV1Bufffer, "ScoreV1: %.4f", score.getScoreV1());
			char scoreV2Bufffer[32] = {};
			sprintf(scoreV2Bufffer, "ScoreV2: %.4f", score.getScoreV2());
			char avgErrorBufffer[32] = {};
			sprintf(avgErrorBufffer, "AvgErr: %.4f", score.getAvgErrorSeg());
			char varianceBufffer[32] = {};
			sprintf(varianceBufffer, "Var: %.4f", score.getVarianceSeg());
			char SDBufffer[32] = {};
			sprintf(SDBufffer, "SD: %.4f", score.getSDSeg());
			char KPSBuffer[32] = {};
			sprintf(KPSBuffer, "KPS: %.1f", (float)judgeKeyVisible.count() / 2);
			char* debugText[8] = {
				fpsBuffer,
				scoreV1Bufffer,
				scoreV2Bufffer,
				avgErrorBufffer,
				varianceBufffer,
				SDBufffer,
				KPSBuffer
			};
			graphic.drawDebug(debugText);
		}
		break;
	}
	case GameState::Result:
	case GameState::ResultDan:
	{
		if (keyTrigger) {
			if (keyPressed[4] || keyPressed[5] || keyPressed[6]) { // Esc or enter
				chart.clear();
				chartVisible.clear();
				judgeKey.clear();
				audio.portAudioExit();
				audio.stopSound();
				score.clearSeg();
				if (gameState == GameState::Result) {
					gameState = GameState::SelectFile;
				}
				else {
					if (++danSeg < 4 && score.hp > 0) {
						gameState = GameState::PlayDan;
						mINI::INIFile iniFile(bmsPath.string() + std::string("/dan/") + danDir[danSelect].string());
						mINI::INIStructure ini;
						char PatternParameterName[32];
						if (danSeg == 1) {
							strcpy(PatternParameterName, "PatternParameter2");
						}
						else if (danSeg == 2) {
							strcpy(PatternParameterName, "PatternParameter3");
						}
						else if (danSeg == 3) {
							strcpy(PatternParameterName, "PatternParameter4");
						}
						if (iniFile.read(ini)) {
							danDirPath = ini[PatternParameterName]["dirPath"];
							danFilePath = ini[PatternParameterName]["filePath"];
							rate = stoi(ini[PatternParameterName]["rate"]);
						}
						bmsInit(true);
					}
					else {
						gameState = GameState::SelectDan;
						score.clearDan();
					}
				}
			}
		}
		graphic.drawResult(score);
		if (gameState == GameState::ResultDan) {
			graphic.drawDanResult(score, danSeg);
		}
		break;
	}
	}
	graphic.draw();
	if (fps.check()) {
		fps.update();
	}
}

void Fps::init() {
	tick = SDL_GetTicks64();
	frameCounter = 0;
	fps = 0;
}

bool Fps::check() {
	frameCounter++;
	return SDL_GetTicks64() - tick >= 1000;
}

void Fps::update() {
	fps = frameCounter;
	frameCounter = 0;
	tick = SDL_GetTicks64();
}

int Fps::get() {
	return fps;
}
