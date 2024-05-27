#include "bmsParser.h"

#include <iostream>

int BmsParser::gcd(int a, int b) {
	return b ? gcd(b, a % b) : a;
}

int BmsParser::lcm(int a, int b) {
	return a * b ? a * b / gcd(a, b) : a;
}

int BmsParser::convert36baseToInt(std::string wavNum) {
	int returnNum = 0;
	if ('0' <= wavNum[0] && wavNum[0] <= '9') {
		returnNum += (wavNum[0] - '0') * 36;
	}
	else if ('A' <= wavNum[0] && wavNum[0] <= 'Z') {
		returnNum += (wavNum[0] - 'A' + 10) * 36;
	}
	if ('0' <= wavNum[1] && wavNum[1] <= '9') {
		return returnNum + (wavNum[1] - '0');
	}
	else if ('A' <= wavNum[1] && wavNum[1] <= 'Z') {
		return returnNum + (wavNum[1] - 'A' + 10);
	}
	else {
		return 0;
	}
}

int BmsParser::convertHexToInt(std::string wavNum) {
	int returnNum = 0;
	if ('0' <= wavNum[0] && wavNum[0] <= '9') {
		returnNum += (wavNum[0] - '0') * 16;
	}
	else if ('A' <= wavNum[0] && wavNum[0] <= 'F') {
		returnNum += (wavNum[0] - 'A' + 10) * 16;
	}
	if ('0' <= wavNum[1] && wavNum[1] <= '9') {
		return returnNum + (wavNum[1] - '0');
	}
	else if ('A' <= wavNum[1] && wavNum[1] <= 'F') {
		return returnNum + (wavNum[1] - 'A' + 10);
	}
	else {
		return 0;
	}
}

std::vector<int> BmsParser::messageToNotes(std::string message, wavType wavType) {
	std::vector<int> notes;
	for (int index = 0; index < message.length(); index += 2) {
		int wav = convert36baseToInt(message.substr(index, 2));
		notes.push_back(wav);
		if (wavType == isBgm) {
			wavInBgm[wav]++;
		}
		else if (wavType == isNote) {
			wavInNote[wav]++;
		}
	}
	return notes;
}

std::vector<int> BmsParser::messageToLongNotes(std::string message, int key) {
	std::vector<int> notes;
	for (int index = 0; index < message.length(); index += 2) {
		int wav = convert36baseToInt(message.substr(index, 2));
		if (wav) {
			if (!longNoteCancel[key]) {
				notes.push_back(wav);
				wavInNote[wav]++;
				longNoteCancel[key] = true;
			}
			else {
				notes.push_back(0);
				longNoteCancel[key] = false;
			}
		}
		else {
			notes.push_back(0);
		}
	}
	return notes;
}

std::vector<int> BmsParser::messageToBpms(std::string message) {
	std::vector<int> bpms;
	for (int index = 0; index < message.length(); index += 2) {
		bpms.push_back(convertHexToInt(message.substr(index, 2)));
	}
	return bpms;
}

std::vector<int> BmsParser::notesMerge(std::vector<int> notes1, std::vector<int> notes2) {
	std::vector<int> notes;
	int LCM = lcm((int)notes1.size(), (int)notes2.size());
	int notes1Dist = notes1.size() ? LCM / (int)notes1.size() : 0;
	int notes2Dist = notes2.size() ? LCM / (int)notes2.size() : 0;
	for (int index = 0; index < LCM; index++) {
		if (notes1Dist && !(index % notes1Dist)) {
			notes.push_back(notes1[index / notes1Dist]);
		}
		else if (notes2Dist && !(index % notes2Dist)) {
			notes.push_back(notes2[index / notes2Dist]);
			std::cout << notes2[index / notes2Dist] << std::endl;
		}
		else {
			notes.push_back(0);
		}
	}
	return notes;
}

void BmsParser::parseFile(const char* file) {
	std::regex regWav("#WAV([[:upper:]|[:digit:]]{2}) (.+)");
	std::regex regBPM("#BPM([[:upper:]|[:digit:]]{2}) (.+)");
	std::regex regHeader("#([[:upper:]]+) (.+)");
	std::regex regData("#([[:digit:]]{3})([[:digit:]]{2}):([[:upper:]|[:digit:]|\.]+)");
	std::ifstream fstream(file);
	std::string buffer;
	std::smatch match;
	while (std::getline(fstream, buffer)) {
		if (std::regex_search(buffer, match, regWav)) {
			wav[convert36baseToInt(match[1])] = match[2];
		}
		else if (std::regex_search(buffer, match, regBPM)) {
			bpmFloat[convert36baseToInt(match[1])] = std::stof(match[2]);
		}
		else if (std::regex_search(buffer, match, regHeader)) {
			if (match[1] == std::string("PLAYER")) {
				player = std::stoi(match[2]);
			}
			else if (match[1] == std::string("BPM")) {
				bpm = std::stof(match[2]);
			}
		}
		else if (std::regex_search(buffer, match, regData)) {
			int barNum = std::stoi(match[1]);
			barMax = barNum > barMax ? barNum : barMax;
			switch (std::stoi(match[2])) {
			case 1: // BGM
				bgm[barNum].push_back(messageToNotes(match[3], isBgm));
				break;
			case 2: // Time signature
				timeSignature[barNum] = std::stof(match[3]);
				break;
			case 3: // BPM
				bpmInt[barNum] = messageToBpms(match[3]);
				break;
			case 8: // BPM float
				break;
			case 9: // Pause
				fprintf(stderr, "Stop Sequence detected!\n");
				break;
			case 11:
				note[barNum][0] = messageToNotes(match[3], isNote);
				break;
			case 12:
				note[barNum][1] = messageToNotes(match[3], isNote);
				break;
			case 13:
				note[barNum][2] = messageToNotes(match[3], isNote);
				break;
			case 14:
				note[barNum][3] = messageToNotes(match[3], isNote);
				break;
			/*case 15:
			case 18:
			case 19:
			case 16:
			case 51:
			case 52:
			case 53:
			case 54:
			case 55:
			case 58:
			case 59:
			case 56:
				bgm[barNum].push_back(messageToNotes(match[3], isBgm));
				break;*/
			}
		}
	}
}

void BmsParser::clear() {
	player = 0;
	bpm = 0;
	barMax = 0;
	bpmInt->clear();
	for (int bar = 0; bar < maxBarNum; bar++) {
		std::vector<std::vector<int>>().swap(bgm[bar]);
		for (int key = 0; key < maxKeyNum; key++) {
			note[bar][key].clear();
		}
		timeSignature[bar] = 0;
	}
	for (int idx = 0; idx < maxIndex; idx++) {
		bpmInt[idx].clear();
		wavInBgm[idx] = 0;
		wavInNote[idx] = 0;
	}
}
