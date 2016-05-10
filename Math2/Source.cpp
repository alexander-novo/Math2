#include <fstream>
#include <iostream>
#include <random>
#include <regex>
#include <sys/stat.h> //To check if file exists
#include <time.h>

#ifdef _WIN32
#define NOMINMAX //For windows.h and protobuf to play nicely
#include <conio.h>
#include <Windows.h>
#include "resource1.h"
#pragma comment( lib, "winmm" )
//#elif defined __linux__ || defined __APPLE__
#else
#error Platform not supported
#endif

#include <google/protobuf/util/message_differencer.h> //To compare messages

#include "openssl/ssl.h"
#include "openssl/bio.h"
#include "openssl/err.h"

#include "Math.pb.h"
#include "MathOperation.h"

using namespace std; 

//Menu functions
void getStartInfo(char*, char*, MathHelper::Log&, unsigned int&, unsigned int&);
MathOperation* doMainMenu(char*, char*, MathHelper::Log&, int);
void drawMainMenu(char*, MathHelper::Log&, unsigned int, unsigned int);
void doQuestion(MathOperation*, MathHelper::Log::Session*, int, MathHelper::Log::Options&, mt19937_64&);
void drawQuestionMenu(int[], string[], int, int, int, int, int, MathOperation*, int, int, int);

//Main menu functions
void help(MathHelper::Log&);
void options(MathHelper::Log&);
void drawOptionsMenu(int, MathHelper::Log::Options&);
void quit(MathHelper::Log&);

//Generic functions
string getCurrTime();
int getDegreeFromInput(int, int, int);
unsigned long long getNumByPlace(mt19937_64&, unsigned int, unsigned long long = 0);
void getStrFromInput(std::regex, char*);
void getExePath(char*, int);
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
const unsigned int NUM_OPTIONS = 4; //Number of options in the option menu

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

			MathHelper::Log prev;

			//Check if we've got a previous log for this user
			//Load it into prev if we do
			bool init = false;
			if (doesFileExist(filename)) {
				fstream in(filename, fstream::in | fstream::binary);
				init = prev.ParseFromIstream(&in);
				in.close();
			}

			//If it was loaded successfully, change the options to the most recent options and add our last session
			fstream output(filename, fstream::out | fstream::trunc | fstream::binary);
			if (init) {
				*prev.mutable_options() = *message.mutable_options();
				*prev.add_session() = *message.session().rbegin();
				message = prev;
			}

			//Write to the main file
			if (!message.IsInitialized() || !message.SerializeToOstream(&output)) {
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
	BIO * bio;
	SSL * ssl;
	SSL_CTX * ctx;

	char blah[MAX_PATH];
	getExePath(blah, MAX_PATH);
	cout << blah;

	int p;

	char * request = "GET / HTTP/1.1\x0D\x0AHost: www.verisign.com\x0D\x0A\x43onnection: Close\x0D\x0A\x0D\x0A";
	char r[1024];

	/* Set up the library */

	ERR_load_BIO_strings();
	SSL_load_error_strings();
	OpenSSL_add_all_algorithms();

	/* Set up the SSL context */

	ctx = SSL_CTX_new(SSLv23_client_method());

	/* Load the trust store */

	if (!SSL_CTX_load_verify_locations(ctx, "TrustStore.pem", NULL))
	{
		fprintf(stderr, "Error loading trust store\n");
		ERR_print_errors_fp(stderr);
		SSL_CTX_free(ctx);
		return 0;
	}

	/* Setup the connection */

	bio = BIO_new_ssl_connect(ctx);

	/* Set the SSL_MODE_AUTO_RETRY flag */

	BIO_get_ssl(bio, &ssl);
	SSL_set_mode(ssl, SSL_MODE_AUTO_RETRY);

	/* Create and setup the connection */

	BIO_set_conn_hostname(bio, "www.verisign.com:https");

	if (BIO_do_connect(bio) <= 0)
	{
		fprintf(stderr, "Error attempting to connect\n");
		ERR_print_errors_fp(stderr);
		BIO_free_all(bio);
		SSL_CTX_free(ctx);
		return 0;
	}

	/* Check the certificate */

	if (SSL_get_verify_result(ssl) != X509_V_OK)
	{
		fprintf(stderr, "Certificate verification error: %i\n", SSL_get_verify_result(ssl));
		BIO_free_all(bio);
		SSL_CTX_free(ctx);
		return 0;
	}

	/* Send the request */

	BIO_write(bio, request, strlen(request));

	/* Read in the response */

	for (;;)
	{
		p = BIO_read(bio, r, 1023);
		if (p <= 0) break;
		r[p] = 0;
		printf("%s", r);
	}

	/* Close the connection and free the context */

	BIO_free_all(bio);
	SSL_CTX_free(ctx);
	return 0;
	
	/*
	//Stuff we need to keep track of
	char name[100];
	char* filename = new char[100];
	unsigned int difficulty = 1;

	//We determine the seed using a random device
	//Random device's implementation varies by platform and may not actually be random
	//So if entropy is 0, then we should use the time instead
	random_device rd;
	unsigned int seed = rd.entropy() ? rd() : time(NULL);
	mt19937_64 rng(seed);

	google::protobuf::SetLogHandler(NULL);
	MathHelper::Log& log = *new MathHelper::Log();
	MathHelper::Log::Session* sesh;


	//Let's get the info we need to start the program (name, difficulty, etc.)
	getStartInfo(name, filename, log, difficulty, seed);

	//Start a new sesh
	sesh = log.add_session();
	sesh->set_starttime(getCurrTime());
	sesh->set_seed(seed);
	sesh->set_difficulty(difficulty);

	//When we exit the program, write our log to the output files
	static WriteOnShutdown write(filename, log);

	//Now enter a loop of the user selecting problems and solving them
	MathOperation* operation = doMainMenu(name, filename, log, difficulty);
	while(operation != NULL) {
		doQuestion(operation, sesh, difficulty, *log.mutable_options(), rng);
		operation = doMainMenu(name, filename, log, difficulty);
	}*/
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
	bool init = false;
	if (doesFileExist(filename)) {
		fstream in(filename, fstream::in | fstream::binary);
		init = log.ParseFromIstream(&in);
	}
	if (!init) {
		log.set_name(name);
		*log.mutable_options() = MathHelper::Log::Options();
	}

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
	seed = stoul(_seed);
}

//TODO: Switch from conio to curses
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
			case 13: //ENTER
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
	cout << "\n                  " << (char)24 << (char)25 << " - Navigate   Enter - Select";
}

void doQuestion(MathOperation* operation, MathHelper::Log::Session* sesh, int difficulty, MathHelper::Log::Options& options, mt19937_64& rng) {
	unsigned int op2Dig, op2Max, answerDig, numAnswers = options.numanswers(), maxTries = options.maxtries();
	long long op1, op2, answer;
	int* answered = new int[numAnswers + 1]();
	string* answers = new string[numAnswers + 1];
	
	//The first operand is easy - any number with the number of digits equal to the difficulty
	op1 = getNumByPlace(rng, difficulty);
	//The second operand, however, needs to be decided based on the operation being performed
	//Anything with the TWO_DIG_MULT flag (division and multiplication) will only have 1 or 2 digits, depending on the difficulty
	//Anything with the SECOND_LTHAN_FIRST flag (division and subtraction) will have to be less than (or equal to) the first
	op2Dig = (operation->flag & MathOperation::TWO_DIG_MULT && options.easymult()) ? difficulty > 3 ? 2 : 1 : difficulty;
	op2Max = (operation->flag & MathOperation::SECOND_LTHAN_FIRST) ? op1 : 0;
	op2 = getNumByPlace(rng, op2Dig, op2Max);

	//Now we choose which of the answers are correct
	uniform_int_distribution<unsigned int> answerDist(0, numAnswers);
	answer = answerDist(rng);

	//Calculate how many digits the answer will have, so we can make similar dummy answers
	answerDig = (operation->op(op1, op2) ? log10(operation->op(op1, op2)) + 1 : 1);

	//Fill our answers with the correct answer and dummy answers
	for (int i = 0; i <= numAnswers; i++) {
		if(i == answer) {
			answers[i] = to_string(operation->op(op1, op2));
			if(operation->flag & MathOperation::REM_OUT) {
				answers[i] += (options.remainform() ? " Remainder = " : " r");
				answers[i] += to_string(op1 % op2);
			}
		} else {
			answers[i] = to_string(getNumByPlace(rng, answerDig));
			if(operation->flag & MathOperation::REM_OUT) {
				answers[i] += (options.remainform() ? " Remainder = " : " r");

				uniform_int_distribution<unsigned int> remainDist(0, op2 - 1);
				answers[i] += to_string(remainDist(rng));
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
	for(; tries < maxTries && !(answered[answer]); tries++) {
		drawQuestionMenu(answered, answers, answerDig, difficulty, op1, op2, op2Dig, operation, tries, numAnswers, maxTries);
		if(tries) playSound(IDR_WAVE2);

		char selection;
		do {
			selection = getch();
			if(selection == 3) exit(0); //CTRL-C
			if (selection < 'A' + numAnswers) selection += 32;
		} while (selection < 'a' || selection > 'a' + numAnswers || answered[selection - 'a']);

		answered[selection - 'a'] = tries + 1;
	}

	//Now we're out of the loop either because the user has exceeded th enumber of tries or they got the answer correct
	//Congratulate them if they got it correct
	//And show them the correct answer otherwise
	if(answered[answer]) {
		cout << char('a' + answer) << " Correct!";
		playSound(IDR_WAVE1);
	} else {
		drawQuestionMenu(answered, answers, answerDig, difficulty, op1, op2, op2Dig, operation, tries, numAnswers, maxTries);
		playSound(IDR_WAVE3);
		for (int i = 0; i < numAnswers + 1; i++) {
			if (answered[i] == maxTries) {
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

	for (int i = 0; i < numAnswers; i++) {
		quest->add_option(answers[i]);
	}

	quest->set_answer(answers[answer]);

	for(int i = 0; i < tries; i++) {
		for (int j = 0; j <= numAnswers; j++) {
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

	delete[] answered;
	delete[] answers;
	answered = nullptr;
	answers = nullptr;

	cout << endl << "        Press anything to continue...";
	if(getch() == 3) exit(0);
}

void drawQuestionMenu(int answered[], string answers[], int answerDig, int difficulty, int op1, int op2, int op2Dig, MathOperation* operation, int tries, int numAnswers, int maxTries) {
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

	for (int j = 0; j <= numAnswers; j++) {
		cout << "        (" << (char) ('a' + j) << ")  ";
		if(answered[j]) {
			for(int k = 0; k < answerDig; k++) {
				cout << 'X';
			}
			if(answered[j] == tries) cout << " Incorrect!";
		} else if (j == numAnswers) {
			cout << "NONE OF THE ABOVE";
		} else cout << answers[j];
		cout << endl;
	}

	cout << "        Please enter your answer [" << maxTries - tries << '/' << maxTries << "]: ";
}

//MAIN MENU FUNCTIONS

void help(MathHelper::Log& log) {
	system("cls");
	cout << "\n        -----------------------------------------------"
		<< "\n                  ARITHMETIC PRACTICE PROGRAM"
		<< "\n                           Help Menu"
		<< "\n        -----------------------------------------------\n"
		<< "            Welcome to the help menu. Choose from any\n            of the "
		<< MathOperation::getOperations().size()
		<< " operations to be given a math\n            problem based on your current difficulty: "
		<< log.session().rbegin()->difficulty()
		<< ".\n            You will be given "
		<< log.options().numanswers() + 1
		<< " multiple choice\n            answers and "
		<< log.options().maxtries()
		<< " tries to answer it correctly.\n\n            You can change these parameters in the\n            options menu.\n\n            Press anything to continue...";
	log.set_hasseenhelp(true);
	if(getch() == 3) exit(0);
}

//TODO: switch from conio to curses
void options(MathHelper::Log& log) {
	system("cls");
	MathHelper::Log::Options op(log.options());

	int index = 0;
	while (true) {
		drawOptionsMenu(index, op);
		switch (getch()) {
			case 3: //CTRL-C
				cout << endl;
				exit(0);
				break;
			case 0:
			case 224: //ARROW KEYS
				switch (getch()) {
					case 72: //UP ARROW
						if (index == NUM_OPTIONS + 1) index = NUM_OPTIONS - 1;
						else if (--index < 0) index = NUM_OPTIONS + 1;
						break;
					case 80: //DOWN ARROW
						if (index == NUM_OPTIONS) index = 0;
						else if (++index > NUM_OPTIONS + 1) index = 0;
						break;
					case 75: //LEFT ARROW
						switch (index) {
							case 0:
								if (op.maxtries() > 1) op.set_maxtries(op.maxtries() - 1);
								break;
							case 1:
								if (op.numanswers() > 1 && op.numanswers() > op.maxtries() + 1) op.set_numanswers(op.numanswers() - 1);
								break;
							case 2:
								op.set_remainform(!op.remainform());
								break;
							case 3:
								op.set_easymult(!op.easymult());
								break;
							case NUM_OPTIONS:
								index = NUM_OPTIONS + 1;
								break;
							case NUM_OPTIONS + 1:
								index = NUM_OPTIONS;
								break;
						}
						break;
					case 77: //RIGHT ARROW
						switch (index) {
							case 0:
								if (op.maxtries() < 9 && op.numanswers() > op.maxtries() + 1) op.set_maxtries(op.maxtries() + 1);
								break;
							case 1:
								if(op.numanswers() < 9) op.set_numanswers(op.numanswers() + 1);
								break;
							case 2:
								op.set_remainform(!op.remainform());
								break;
							case 3:
								op.set_easymult(!op.easymult());
								break;
							case NUM_OPTIONS:
								index = NUM_OPTIONS + 1;
								break;
							case NUM_OPTIONS + 1:
								index = NUM_OPTIONS;
								break;
						}
						break;
				}
				break;
			case 13: //ENTER
				//If the user has Save and Exit selected, set the user's options to the new ones
				//Then exit if the user has either Save and Exit selected or Cancel selected
				if (index == NUM_OPTIONS) {
					*log.mutable_options() = op;
					return;
				} else if (index == NUM_OPTIONS + 1) return;
				break;
		}
	}
}

void drawOptionsMenu(int index, MathHelper::Log::Options& op) {
	system("cls");
	cout << "\n        -----------------------------------------------"
		<< "\n                  ARITHMETIC PRACTICE PROGRAM"
		<< "\n                         Options  Menu"
		<< "\n        -----------------------------------------------\n"
		<< "\n            Maximum tries                        "
		<< (index == 0 ? "< " : "  ") << op.maxtries() << (index == 0 ? " >" : "")
		<< "\n            Number of Answers                    "
		<< (index == 1 ? "< " : "  ") << op.numanswers() << (index == 1 ? " >" : "")
		<< "\n            Remainder format         " << (op.remainform() ? "" : "           ")
		<< (index == 2 ? "< " : "  ") << (op.remainform() ? "Remainder = 5" : "r5") << (index == 2 ? " >" : "")
		<< "\n            Easy multipliers                   " << (op.easymult() ? " " : "")
		<< (index == 3 ? "< " : "  ") << (op.easymult() ? "ON" : "OFF") << (index == 3 ? " >" : "")
		<< "\n\n            " << (index == 4 ? "[ " : "  ") << "Save & Exit" << (index == 4 ? " ]" : "  ")
		<< "    " << (index == 5 ? "[ " : "  ") << "Cancel" << (index == 5 ? " ]" : "  ")
		<< "\n\n            " << (char)24 << (char)25 << " - Navigate  " << (char)27 << (char)26 << " - Change";
}

void quit(MathHelper::Log& log) {
	exit(0);
}

//GENERIC FUNCTIONS

string getCurrTime() {
	char timeStr[100];
	time_t currTime = time(NULL);

	strftime(timeStr, 100, "%c", localtime(&currTime));
	return timeStr;
}

//TODO: switch from conio to curses
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

unsigned long long getNumByPlace(mt19937_64& rng, unsigned int place, unsigned long long _max) {
	//Determine our minimum and maximum numbers base on digits (examples shown are 5 digits)
	unsigned long long digMin = pow(10, place - 1); //10,000
	unsigned long long digMax = pow(10, place) - 1; //99,999

	if (_max >= digMin && _max < digMax && _max != 0) digMax = _max;

	uniform_int_distribution<unsigned long long> dist(digMin, digMax);
	return dist(rng);
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

string getExePath() {
#ifdef _WIN32
	char buffer[MAX_PATH];
	GetModuleFileNameA(NULL, buffer, MAX_PATH);
	string re(buffer);
	return re.substr(0, re.find_last_of("\\/"));
#endif
}

void playSound(int res) {
#ifdef _WIN32
	HANDLE found, sound;
	LPVOID data;

	found = FindResource(NULL, MAKEINTRESOURCE(res), L"WAVE");
	sound = LoadResource(NULL, (HRSRC) found);
	data = LockResource(sound);
	sndPlaySound((LPCWSTR) data, SND_ASYNC | SND_MEMORY);
	UnlockResource(sound);
	FreeResource(found);
#endif
}
