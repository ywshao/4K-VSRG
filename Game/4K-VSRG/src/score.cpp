#include <cmath>

#include "score.h"

void ErrorMeter::setLifeTime(int time) {
	lifeTime = time;
}

int ErrorMeter::getLifeTime() {
	return lifeTime;
}

void ErrorMeter::setScale(int factor) {
	scale = factor;
}
float ErrorMeter::getScale() {
	return scale;
}

void ErrorMeter::add(Uint64 time, int judge, int error) {
	errorList.push_back({ time, judge, error });
}

void ErrorMeter::update(Uint64 currentTime) {
	for (std::list<JudgeErrorTime>::iterator iter = errorList.begin(); iter != errorList.end();) {
		if (currentTime >= iter->time + lifeTime) {
			iter = errorList.erase(iter);
		}
		else {
			iter++;
		}
	}
}

std::list<JudgeErrorTime>::iterator ErrorMeter::begin() {
	return errorList.begin();
}

std::list<JudgeErrorTime>::iterator ErrorMeter::end() {
	return errorList.end();
}

int Score::judgeScoreV1(double judgeDifficulty, Uint64 errorMs) {
	if (errorMs <= 22.5 / judgeDifficulty) {
		return 5;
	}
	else if (errorMs <= 45 / judgeDifficulty) {
		return 4;
	}
	else if (errorMs <= 90 / judgeDifficulty) {
		return 3;
	}
	else if (errorMs <= 135 / judgeDifficulty) {
		return 2;
	}
	else if (errorMs <= 180) {
		return 1;
	}
	else {
		return 0;
	}
}

// Etterna Wife v3
double Score::judgeScoreV2(double judgeDifficulty, Uint64 errorMs) {
	const static double missPoints = -2.75;
	const static double powerFactor = 0.75;
	const double judgeWindowFactor = 1 / judgeDifficulty;
	const double deviationFactor = 22.7 * std::pow(judgeWindowFactor, powerFactor);
	// Two important MS constant
	const double zeroPointMs = 65 * std::pow(judgeWindowFactor, powerFactor);
	const double minPointMs = 180 * judgeWindowFactor;
	if (errorMs <= 5 * judgeWindowFactor) {
		return 1;
	}
	else if (errorMs <= zeroPointMs) {
		double x = errorMs / 100;
		return std::erf((zeroPointMs - errorMs) / deviationFactor);
	}
	else if (errorMs <= minPointMs) {
		return (errorMs - zeroPointMs) * missPoints / (minPointMs - zeroPointMs);
	}
	else {
		return missPoints;
	}
}

int Score::judge(double judgeDifficulty, Uint64 errorMs) {
	if (errorMs <= 22.5 / judgeDifficulty) {
		return 0;
	}
	else if (errorMs <= 45 / judgeDifficulty) {
		return 1;
	}
	else if (errorMs <= 90 / judgeDifficulty) {
		return 2;
	}
	else if (errorMs <= 135 / judgeDifficulty) {
		return 3;
	}
	else if (errorMs <= 180) {
		return 4;
	}
	else if (errorMs <= earlyMissMs) {
		return 5;
	}
	else {
		return 6;
	}
}

JudgeKeySound Score::judger(Uint64 currentTime, double judgeDifficulty, int key, ChartVisible& chartVisible, JudgeVisible& judgeNoteVisible, ErrorMeter& errorMeter, Uint64 chartOffset) {
	static JudgeKeySound judgeKeySound[4]; // Preserve the last note sound for audio purpose
	static int judgeResult = 6; // Preserve the last judge for visual purpose

	// If no notes, just return key sound and last judge
	if (!chartVisible.count(key)) {
		judgeKeySound[key].judge = judgeResult;
		return judgeKeySound[key];
	}

	// Next and previous is based on the current time
	std::list<KeySound>::iterator nextNote = std::lower_bound(chartVisible.begin(key), chartVisible.end(key), currentTime - chartOffset,
		[](const KeySound& a, const Uint64& b) -> bool {return a.time < b; });
	// If there are no previous notes, then previous note = next note to reduce branches
	std::list<KeySound>::iterator previousNote = nextNote;
	if (previousNote != chartVisible.begin(key)) {
		std::advance(previousNote, -1);
	}
	if (nextNote == chartVisible.end(key)) {
		nextNote = previousNote;
	}
	int errorMsNextNote = std::abs(static_cast<long long>(currentTime - (nextNote->time + chartOffset)));
	int errorMsPreviousNote = std::abs(static_cast<long long>(currentTime - (previousNote->time + chartOffset)));

	// Judge the closest note

	if (errorMsPreviousNote <= errorMsNextNote) {
		bool early = currentTime < previousNote->time + chartOffset;
		judgeResult = judge(judgeDifficulty, errorMsPreviousNote);
		judgeKeySound[key] = { *previousNote,  judgeResult };
		if (judgeResult == 6) { // Not in any judge window
			return judgeKeySound[key];
		}
		else if (judgeResult == 5) { // Early miss (early poor)
			earlyMiss();
		}
		else {
			updateScore(judgeDifficulty, early, errorMsPreviousNote, judgeResult);
			updateHp(judgeResult);
		}
		judgeNoteVisible.add(key, judgeKeySound[key]);
		if (judgeResult <= 4) {
			chartVisible.remove(key, previousNote);
		}
		errorMeter.add(currentTime, judgeResult, early ? -static_cast<int>(errorMsPreviousNote) : static_cast<int>(errorMsPreviousNote));
		judgeKeySound[key].time = currentTime - chartOffset;
	}
	else {
		bool early = currentTime < nextNote->time + chartOffset;
		judgeResult = judge(judgeDifficulty, errorMsNextNote);
		judgeKeySound[key] = { *nextNote,  judgeResult };
		if (judgeResult == 6) { // Not in any judge window
			return judgeKeySound[key];
		}
		else if (judgeResult == 5) { // Early miss (early poor)
			earlyMiss();
		}
		else {
			updateScore(judgeDifficulty, early, errorMsNextNote, judgeResult);
			updateHp(judgeResult);
		}
		judgeNoteVisible.add(key, judgeKeySound[key]);
		if (judgeResult <= 4) {
			chartVisible.remove(key, nextNote);
		}
		errorMeter.add(currentTime, judgeResult, early ? -static_cast<int>(errorMsNextNote) : static_cast<int>(errorMsNextNote));
		judgeKeySound[key].time = currentTime - chartOffset;
	}
	return judgeKeySound[key];
}

bool Score::missJudger(Uint64 currentTime, double difficulty, ChartVisible& chartVisible, JudgeVisible& judgeNoteVisible, Uint64 chartOffset) {
	bool flag = false;
	for (int key = 0; key < 4; key++) {
		for (std::list<KeySound>::iterator iter = chartVisible.begin(key); iter != chartVisible.end(key);) {
			if (currentTime >= iter->time + chartOffset + lateMissMs) {
				miss();
				judgeNoteVisible.add(key, { *iter,  5 });
				iter = chartVisible.remove(key, iter);
				flag = true;
			}
			else {
				++iter;
			}
		}
	}
	return flag;
}

void Score::clearSeg() {
	for (int judge = 0; judge <= 6; judge++) {
		judgeCounterSeg[judge] = 0;
	}
	scoreV1Seg = 0;
	scoreV2Seg = 0;
	judgedNoteCountSeg = 0;
	notMissCountSeg = 0;
	comboSeg = 0;
	meanSeg = 0;
	M2Seg = 0;
}

void Score::clearDan() {
	for (int judge = 0; judge <= 6; judge++) {
		judgeCounter[judge] = 0;
	}
	scoreV1 = 0;
	scoreV2 = 0;
	judgedNoteCount = 0;
	notMissCount = 0;
	combo = 0;
	mean = 0;
	M2 = 0;
}

void Score::updateScore(double judgeDifficulty, bool early, Uint64 errorMs, int judgeResult) {
	scoreV1 += judgeScoreV1(judgeDifficulty, errorMs);
	scoreV1Seg += judgeScoreV1(judgeDifficulty, errorMs);
	scoreV2 += judgeScoreV2(judgeDifficulty, errorMs);
	scoreV2Seg += judgeScoreV2(judgeDifficulty, errorMs);
	judgedNoteCount++;
	judgedNoteCountSeg++;
	judgeCounter[judgeResult]++;
	judgeCounterSeg[judgeResult]++;
	notMissCount++;
	notMissCountSeg++;
	judgeResult <= 2 ? combo++ : combo = 0;
	judgeResult <= 2 ? comboSeg++ : comboSeg = 0;

	// Welford's online algorithm
	int errorMsSigned = early ? -(int)errorMs : errorMs;
	double delta = errorMsSigned - mean;
	mean += delta / notMissCount;
	double delta2 = errorMsSigned - mean;
	M2 += delta * delta2;

	double deltaSeg = errorMsSigned - meanSeg;
	meanSeg += deltaSeg / notMissCountSeg;
	double delta2Seg = errorMsSigned - meanSeg;
	M2Seg += deltaSeg * delta2Seg;
}

void Score::updateHp(int judgeResult) {
	switch (judgeResult) {
	case 0:
		hp += 8;
		break;
	case 1:
		hp += 8;
		break;
	case 2:
		hp += 4;
		break;
	case 3:
		break;
	case 4:
		hp -= 40;
		break;
	case 5:
		hp -= 80;
		break;
	}
	hp = std::min(hp, 1000);
}

void Score::earlyMiss() {
	judgeCounter[6]++;
	judgeCounterSeg[6]++;
	hp -= 10;
}

void Score::miss() {
	scoreV2 -= 2.75;
	scoreV2Seg -= 2.75;
	judgedNoteCount++;
	judgedNoteCountSeg++;
	judgeCounter[5]++;
	judgeCounterSeg[5]++;
	combo = 0;
	comboSeg = 0;
	hp -= 80;
}

double Score::getScoreV1() {
	return judgedNoteCount ? (double)20 * scoreV1 / judgedNoteCount : 0;
}

double Score::getScoreV2() {
	return judgedNoteCount ? (double)100 * scoreV2 / judgedNoteCount : 0;
}

double Score::getScoreV1Seg() {
	return judgedNoteCountSeg ? (double)20 * scoreV1Seg / judgedNoteCountSeg : 0;
}

double Score::getScoreV2Seg() {
	return judgedNoteCountSeg ? (double)100 * scoreV2Seg / judgedNoteCountSeg : 0;
}

double Score::getAvgError() {
	return mean;
}

double Score::getAvgErrorSeg() {
	return meanSeg;
}

double Score::getVariance() {
	return notMissCount ? M2 / notMissCount : 0;
}

double Score::getVarianceSeg() {
	return notMissCountSeg ? M2Seg / notMissCountSeg : 0;
}

double Score::getSD() {
	return sqrt(getVariance());
}

double Score::getSDSeg() {
	return sqrt(getVarianceSeg());
}

int Score::getCombo() {
	return combo;
}
