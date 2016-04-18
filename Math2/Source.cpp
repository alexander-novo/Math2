#include <fstream>
#include <iostream>
#include <regex>
#include <sys/stat.h> //To check if file exists
#include <time.h>

#ifdef _WIN32
#include <conio.h>
#include <Windows.h>
#pragma comment( lib, "winmm" )
#elif defined __linux__ || defined __APPLE__
#include <ncurses.h>
#else
#error Platform not supported
#endif

#include "resource1.h"

#include "Math.pb.h"
#include "MathOperation.h"

using namespace std; 

//Menu functions
void getStartInfo(char*, char*, MathHelper::Log&, unsigned int&, unsigned int&);
MathOperation* doMainMenu(char*, char*, MathHelper::Log&, int);
void drawMainMenu(char*, MathHelper::Log&, unsigned int, unsigned int);
void doQuestion(MathOperation*, MathHelper::Log::Session*, int);
void drawQuestionMenu(int[], string[], int, int, int, int, int, MathOperation*, int);

//Main menu functions
void help(MathHelper::Log&);
void options(MathHelper::Log&);
void quit(MathHelper::Log&);

//Generic functions
string getCurrTime();
int getDegreeFromInput(int, int, int);
unsigned int getNumByPlace(unsigned int, unsigned int = 0);
void getStrFromInput(std::regex, char*);
bool doesFileExist(const char*);

#ifdef _WIN32
void playSound(int);
#endif

typedef void(*menuFunction)(MathHelper::Log&);
//Global constants in case we ever want to change the parameters of the program
const map<unsigned char, pair<char*, menuFunction>> menuFunctions = {
	{ 'h', { "Help", help } },
	{ 'o', { "Options", options } },
	{ 'q', { "Quit the Program", quit } }
};
const char * PREVIOUS_FILE = ".previous";
const unsigned int NUM_ANSWERS = 4; //Reperesents how many answers should be generated and displayed besides NONE OF THE ABOVE
const unsigned int MAX_TRIES = 3; //How many tries we should give the user before marking the question incorrect

//An object which keeps track of our log and writes it to the file when the program exits
class WriteOnShutdown {
	private:
		const char* filename;
		MathHelper::Log& message;
	public:
		WriteOnShutdown(char* a, MathHelper::Log& b) : filename(a), message(b) {}
		~WriteOnShutdown() {
			//If a session exists, set the last session's end time to now
			if(message.session_size()) message.mutable_session() -> rbegin() -> set_endtime(getCurrTime());

			//Write to the main file
			fstream output(filename, fstream::out | fstream::trunc | fstream::binary);
			if(!message.SerializeToOstream(&output)) {
				cout << "Error writing output file." << endl;
				return;
			}
			output.close();

			//Then write the username to the PREVIOUS_FILE
			output.open(PREVIOUS_FILE, fstream::out | fstream::trunc);
			output << message.name();
			output.close();

			//Then write human-readable output to username.txt
			char txtFilename[104];
			strcpy(txtFilename, filename);
			strcat(txtFilename, ".txt");

			output.open(txtFilename, fstream::out | fstream::trunc);
			output << message.DebugString();

			cout << endl;
		}
};

int main() {
	//Stuff we need to keep track of
	char name[100];
	char* filename = new char[100];
	unsigned int difficulty = 1;
	unsigned int seed = time(NULL);

	MathHelper::Log& log = *new MathHelper::Log();
	MathHelper::Log::Session* sesh;

#ifdef __linux__
	initscr();
#endif

	//Let's get the info we need to start the program (name, difficulty, etc.)
	getStartInfo(name, filename, log, difficulty, seed);

	//Start a new sesh
	sesh = log.add_session();
	sesh->set_starttime(getCurrTime());
	sesh->set_seed(seed);
	sesh->set_difficulty(difficulty);
	srand(seed);

	//When we exit the program, write our log to the output files
	static WriteOnShutdown write(filename, log);

	//Now enter a loop of the user selecting problems and solving them
	MathOperation* operation = doMainMenu(name, filename, log, difficulty);
	while(operation != NULL) {
		doQuestion(operation, sesh, difficulty);
		operation = doMainMenu(name, filename, log, difficulty);
	}
}

//MENU FUNCTIONS

void getStartInfo(char* name, char* filename, MathHelper::Log& log, unsigned int& difficulty, unsigned int& seed) {
	system("cls");

	cout << "  Enter your name: ";

	//If we left off somewhere, let's resume
	if (doesFileExist(PREVIOUS_FILE)) { 
		fstream in(PREVIOUS_FILE, fstream::in);
		in.getline(name, 100);
	}
	getStrFromInput(regex("^[[:alpha:]]* ?[[:alpha:]]*$"), name);
	strcpy(filename, name);
	replace(filename, filename + strlen(filename), ' ', '_');

	cout << endl;

	//Now check if the user has a log file, then load it for defaults
	if (doesFileExist(filename)) {
		fstream in(filename, fstream::in | fstream::binary);
		log.ParseFromIstream(&in);
	} else log.set_name(name);

	//Get the difficulty we last used, then use it as the default choice
	if (log.session_size() > 0) {
		difficulty = log.session().rbegin() -> difficulty();
	}
	cout << "  Difficulty: ";
	difficulty = getDegreeFromInput(1, 5, difficulty);

	cout << endl;
	
	//Now get the seed
	cout << "  Seed: ";
	string _seed = to_string(seed);
	getStrFromInput(regex("^[[:digit:]]*$"), &_seed.front());
	seed = stoi(_seed);
}

MathOperation* doMainMenu(char* name, char* filename, MathHelper::Log& log, int difficulty) {
	unsigned char chosen, maxOption = 'a' + MathOperation::getOperations().size() - 1;
	unsigned int index = 0, longestOption = 0;
	vector<MathOperation*> operations = MathOperation::getOperations();

	
	//If the user has previously answered a question this session, default to what category they just chose
	//Otherwise, if the user hasn't seen the help menu before, we'll default to help
	if(log.session_size() && log.session().rbegin()->question_size()) {
		int i = 0;
		for(vector<MathOperation*>::const_iterator it = operations.cbegin(); it != operations.end(); it++) {
			if(!strcmp((*it)->name, log.session().rbegin()->question().rbegin()->operation().c_str())) {
				break;
			}
			i++;
		}
		index = i;
	} else if(!log.hasseenhelp()) {
		index = operations.size();
	}

	//Calculate which option's name is longest
	for(vector<MathOperation*>::const_iterator it = operations.cbegin(); it != operations.end(); it++) {
		int length = strlen((*it)->name);
		if(length > longestOption) longestOption = length;
	}
	for(map<unsigned char, pair<char*, menuFunction>>::const_iterator it = menuFunctions.cbegin(); it != menuFunctions.end(); it++) {
		int length = strlen(it->second.first);
		if(length > longestOption) longestOption = length;
	}

	do {
		drawMainMenu(name, log, index, longestOption);
		chosen = getch();
		switch(chosen) {
			case 3: //CTL-C
				return NULL;
				break;
			case 0:
			case 224: //ARROW-KEYS
				//Move our selector up or down
				switch(getch()) {
					case 72: //UP
						index--;
						if(index >= operations.size() + menuFunctions.size()) index = operations.size() + menuFunctions.size() - 1;
						break;
					case 80: //DOWN
						index++;
						if(index >= operations.size() + menuFunctions.size()) index = 0;
						break;
				}
				break;
			case 13:
				//Select whatever our cursor is currently on
				if(index < operations.size()) return operations[index];
				else {
					map<unsigned char, pair<char*, menuFunction>>::const_iterator it = menuFunctions.cbegin();
					advance(it, index - operations.size());
					it->second.second(log);
				}
				break;
			default:
				if(menuFunctions.find(chosen) != menuFunctions.end()) {
					menuFunctions.find(chosen)->second.second(log);
				}
				break;
		}
	} while(chosen < 'a' || chosen > maxOption);

	return operations[chosen - 'a'];
}

void drawMainMenu(char* name, MathHelper::Log& log, unsigned int index, unsigned int longestOption) {
	system("cls");

	cout << "                  ARITHMETIC PRACTICE PROGRAM\n\n            Hello "
		<< name
		<< ", and welcome\n            to the Math Skills Practice Program.\n\n            This program allows you to practice your\n            math skills.\n\n            Choose what you want to practice in the\n            menu shown below.\n";
	//Keep track of accuracy
	if (log.mutable_session()->rbegin()->question_size()) cout << "            Current Accuracy: "
		<< log.mutable_session()->rbegin()->mutable_question()->rbegin()->correctpercent() * 100
		<< '%';
	cout << "\n        -----------------------------------------------\n                  ARITHMETIC PRACTICE PROGRAM\n                          MAIN  MENU\n        -----------------------------------------------\n\n";

	//Write all of our math options to the main menu
	vector<MathOperation*> operations = MathOperation::getOperations();
	char option = 'a';
	unsigned int _index = 0;
	for (vector<MathOperation*>::const_iterator it = operations.begin(); it != operations.end(); it++) {
		cout << "                   "
			<< (_index == index ? "[ " : "  ")
			<< option
			<< ".   "
			<< (*it)->name;
		if(_index++ == index) {
			for(int i = strlen((*it)->name); i < longestOption; i++) {
				cout << " ";
			}
			cout << " ]";
		}
		cout << endl;
		option++;
	}
	cout << "                   ------\n";
	//Now write all of our extraneous options
	for (map<unsigned char, pair<char*, menuFunction>>::const_iterator it = menuFunctions.cbegin(); it != menuFunctions.cend(); it++) {
		cout << "                   "
			<< (_index == index ? "[ " : "  ")
			<< it->first
			<< ".   "
			<< it->second.first;
		if(_index++ == index) {
			for(int i = strlen(it->second.first); i < longestOption; i++) {
				cout << " ";
			}
			cout << " ]";
		}
		cout << endl;
	}
	cout << "\n                  " << (char)24 << (char)25 << " - navigate   Enter - Select";
}

void doQuestion(MathOperation* operation, MathHelper::Log::Session* sesh, int difficulty) {
	unsigned int op2Dig, op2Max, op1, op2, answer, answerDig;
	int answered[NUM_ANSWERS + 1] = {0};
	string answers[NUM_ANSWERS + 1];
	
	//The first operand is easy - any number with the number of digits equal to the difficulty
	op1 = getNumByPlace(difficulty);
	//The second operand, however, needs to be decided based on the operation being performed
	//Anything with the TWO_DIG_MULT flag (division and multiplication) will only have 1 or 2 digits, depending on the difficulty
	//Anything with the SECOND_LTHAN_FIRST flag (division and subtraction) will have to be less than (or equal to) the first
	op2Dig = (operation->flag & MathOperation::TWO_DIG_MULT) ? difficulty > 3 ? 2 : 1 : difficulty;
	op2Max = (operation->flag & MathOperation::SECOND_LTHAN_FIRST) ? op1 : 0;
	op2 = getNumByPlace(op2Dig, op2Max);

	//Now we choose which of the answers are correct
	answer = rand() % (NUM_ANSWERS + 1);

	//Calculate how many digits the answer will have, so we can make similar dummy answers
	answerDig = (operation->op(op1, op2) ? log10(operation->op(op1, op2)) + 1 : 1);

	//Fill our answers with the correct answer and dummy answers
	for(int i = 0; i <= NUM_ANSWERS; i++) {
		if(i == answer) {
			answers[i] = to_string(operation->op(op1, op2));
			if(operation->flag & MathOperation::REM_OUT) {
				answers[i] += " r";
				answers[i] += to_string(op1 % op2);
			}
		} else {
			answers[i] = to_string(getNumByPlace(answerDig));
			if(operation->flag & MathOperation::REM_OUT) {
				answers[i] += " r";
				answers[i] += to_string(rand() % op2);
			}
		}
		for(int j = 0; j < i; j++) {
			if(answers[j] == answers[i]) {
				i = j - 1;
				break;
			}
		}
	}

	//Now, enter a loop which keeps track of the number of tries and whether or not the person has gotten the correct answer
	//Every loop, draw the menu and ask user for input
	int tries = 0;
	for(; tries < MAX_TRIES && !(answered[answer]); tries++) {
		drawQuestionMenu(answered, answers, answerDig, difficulty, op1, op2, op2Dig, operation, tries);
		if(tries) playSound(IDR_WAVE2);

		char selection;
		do {
			selection = getch();
			if(selection == 3) exit(0); //CTRL-c
			if(selection < 'A' + NUM_ANSWERS) selection += 32;
		} while(selection < 'a' || selection > 'a' + NUM_ANSWERS || answered[selection - 'a']);

		answered[selection - 'a'] = tries + 1;
	}

	//Now we're out of the loop either because the user has exceeded th enumber of tries or they got the answer correct
	//Congratulate them if they got it correct
	//And show them the correct answer otherwise
	if(answered[answer]) {
		cout << char('a' + answer) << " Correct!";
		playSound(IDR_WAVE1);
	} else {
		drawQuestionMenu(answered, answers, answerDig, difficulty, op1, op2, op2Dig, operation, tries);
		playSound(IDR_WAVE3);
		for(int i = 0; i < NUM_ANSWERS + 1; i++) {
			if(answered[i] == MAX_TRIES) {
				cout << (char)('a' + i) << " Correct answer: " << (char)('a' + answer);
			}
		}
	}

	//Then, log our results
	MathHelper::Log::Session::Question* quest = sesh->add_question();
	quest->set_time(getCurrTime());
	quest->set_x(op1);
	quest->set_y(op2);
	quest->set_operation(operation->name);

	for(int i = 0; i < NUM_ANSWERS; i++) {
		quest->add_option(answers[i]);
	}

	quest->set_answer(answers[answer]);

	for(int i = 0; i < tries; i++) {
		for(int j = 0; j <= NUM_ANSWERS; j++) {
			if(answered[j] == i + 1) {
				quest->add_attempt(j);
				break;
			}
		}
	}

	quest->set_correct(answered[answer] != 0);

	//Calculate the percentage correct
	double correctSum = 0;
	for(int i = 0; i < sesh->question_size(); i++) {
		MathHelper::Log::Session::Question* _quest = sesh->mutable_question(i);
		if(_quest->correct()) correctSum++;
	}

	quest->set_correctpercent(correctSum / (double) (sesh->question_size()));

	cout << endl << "        Press anything to continue...";
	getch();
}

void drawQuestionMenu(int answered[], string answers[], int answerDig, int difficulty, int op1, int op2, int op2Dig, MathOperation* operation, int tries) {
	system("cls");
	/*
		Display problem like so:

			  123
			X 456
			-----
			?????
	*/
	cout << "        Solve:\n\n";
	for(int i = 0; i < 18 - difficulty; i++) cout << " ";
	cout << op1 << endl;
	for(int i = 0; i < 16 - op2Dig; i++) cout << " ";
	cout << operation->_operator << " " << op2 << endl;
	for(int i = 0; i < 16 - op2Dig; i++) cout << " ";
	for(int i = 0; i < op2Dig + 2; i++) cout << "-";
	cout << endl;
	for(int i = 0; i < 16 - op2Dig; i++) cout << " ";
	for(int i = 0; i < op2Dig + 2; i++) cout << "?";
	cout << "\n\n\n";

	for(int j = 0; j <= NUM_ANSWERS; j++) {
		cout << "        (" << (char) ('a' + j) << ")  ";
		if(answered[j]) {
			for(int k = 0; k < answerDig; k++) {
				cout << 'X';
			}
			if(answered[j] == tries) cout << " Incorrect!";
		} else if(j == NUM_ANSWERS) {
			cout << "NONE OF THE ABOVE";
		} else cout << answers[j];
		cout << endl;
	}

	cout << "        Please enter your answer [" << MAX_TRIES - tries << '/' << MAX_TRIES << "]: ";
}

//MAIN MENU FUNCTIONS

void help(MathHelper::Log& log) {
	//TODO: DO THIS
}

void options(MathHelper::Log& log) {
	//TODO: DO THIS
}

void quit(MathHelper::Log& log) {
	exit(0);
}

string getCurrTime() {
	char timeStr[100];
	time_t currTime = time(NULL);

	strftime(timeStr, 100, "%c", localtime(&currTime));
	return timeStr;
}

int getDegreeFromInput(int min, int max, int def) {
	unsigned char button;
	unsigned int index = def - min;
	bool written = false;

	const int charas = (max - min + 1) * 3 + 2;

	while (true) {
		//Do this every time except the first
		if (written) {
			for (int i = 0; i < charas; i++) {
				cout << '\b';
			}
		}
		written = true;

		cout << "<";
		for (int i = min; i <= max; i++) {
			cout << (i == index + 1 ? "[" : " ") << i << (i == index + 1 ? "]" : " ");
		}
		cout << ">";

		button = getch();
		switch (button) {
			case 3: //ctrl-c
				cout << endl;
				exit(1);
			case 13: //ENTER
				return index + min;
			case 0:
			case 224: //ARROW KEYS
				switch(getch()) {
					case 75: //LEFT
						if(index) index--;
						else index = max - min;
						break;
					case 77: //RIGHT
						if(index < (max - min)) index++;
						else index = 0;
						break;
				}
				break;
			default:
				if (button - '0' >= min && button - '0' <= max) {
					index = button - '0' - min;
				}
				break;
		}
	}
}

unsigned int getNumByPlace(unsigned int place, unsigned int _max) {
	int max = RAND_MAX;
	int num = rand();

	int digMin = pow(10, place - 1) - 1; //9,999
	int digMax = pow(10, place) - 1; //99,999

	if (_max >= digMin && _max < digMax && _max != 0) digMax = _max;

	while(digMax > max) {
		int num2 = rand();
		num = num2 * (max + 1) + num;
		max = RAND_MAX * (max + 1);
	}

	double ranD = ((double) num) / max; //Random number between 0 and 1
	return ranD * (digMax - digMin - 1) + digMin + 1;
	//     1    * (99,999 - 9,999  - 1) + 9,999  + 1 = 99,999
	//     0    * (99,999 - 9,999  - 1) + 9,999  + 1 = 10,000
}

void getStrFromInput(regex reg, char* buf) {
	char button;
	unsigned int index = 0;
	for(char* it = buf; *it; ++it) {
		if(it == NULL || *it == '\0' || *it == -52) break;
		index++;
	}

	if (index) {
		cout << buf;
	}

	while(true) {
		button = getch();
		unsigned int _index = index; //Store where we just were for use later, in case it changes
		switch(button) {
			case 3: //ctrl-c
				cout << endl;
				exit(1);
			case '\b': //BACKSPACE
				if(index) buf[--index] = '\0';
				break;
			case 13: //ENTER
				if(index) return;
				break;
			default:
				buf[index++] = button;
				buf[index] = '\0';
				if(!regex_match(buf, reg)) {
					buf[--index] = '\0';
					continue;
				}
		}

		//CLEAR LINE
		//First, we clear the number of characters that we were displaying previously
		//But this doesn't actually clear them from the console - you have to overwrite them with something
		//This would be fine if our number were growing in digits, but if you had '567' and typed backspace, '567' would still show up, even if you only printed '56'
		//So, we overwrite everything with whitespace, then back up to the beginning again.
		for(unsigned int i = 0; i < _index; i++) {
			cout << '\b';
		}
		for(unsigned int i = 0; i < _index; i++) {
			cout << ' ';
		}
		for(unsigned int i = 0; i < _index; i++) {
			cout << '\b';
		}

		//END CLEAR LINE

		cout << buf;
	}
}

bool doesFileExist(const char* fileName) {
	struct stat buf;
	return (stat(fileName, &buf) == 0);
}

#ifdef _WIN32
void playSound(int res) {
	HANDLE found, sound;
	LPVOID data;

	found = FindResource(NULL, MAKEINTRESOURCE(res), L"WAVE");
	sound = LoadResource(NULL, (HRSRC) found);
	data = LockResource(sound);
	sndPlaySound((LPCWSTR) data, SND_ASYNC | SND_MEMORY);
	UnlockResource(sound);
	FreeResource(found);
}
#endif