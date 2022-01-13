// compile with: clang++ -std=c++20 -Wall -Werror -Wextra -Wpedantic -g3 -o fishies fishies.cpp
// run with: ./fishies 2> /dev/null
// run with: ./fishies 2> debugoutput.txt
//  "2>" redirect standard error (STDERR; cerr)
//  /dev/null is a "virtual file" which discard contents

// Works best in Visual Studio Code if you set:
//   Settings -> Features -> Terminal -> Local Echo Latency Threshold = -1

// https://en.wikipedia.org/wiki/ANSI_escape_code#3-bit_and_4-bit

#include <iostream>
#include <vector>
#include <string>
#include <random>
#include <chrono>    // for dealing with time intervals
#include <cmath>     // for max() and min()
#include <termios.h> // to control terminal modess
#include <unistd.h>  // for read()
#include <fcntl.h>   // to enable / disable non-blocking read()

// Because we are only using #includes from the standard, names shouldn't conflict
using namespace std;

// Constants

// Disable JUST this warning (in case students choose not to use some of these constants)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-const-variable"

const char NULL_CHAR{'z'}; //?????
const char UP_CHAR{'w'};
const char DOWN_CHAR{'s'};
const char LEFT_CHAR{'a'};
const char RIGHT_CHAR{'d'};
const char QUIT_CHAR{'q'};
const char FREEZE_CHAR{'f'};
const char CREATE_CHAR{'c'};   //creates new fish
const char BLOCKING_CHAR{'b'}; //stops the fish
const char COMMAND_CHAR{'o'};  //shows command line --> CANT RETURN OUT OF IT THO

const string ANSI_START{"\033["};
const string START_COLOUR_PREFIX{"1;"};
const string START_COLOUR_SUFFIX{"m"};
const string STOP_COLOUR{"\033[0m"};

const unsigned int COLOUR_IGNORE{0}; // this is a little dangerous but should work out OK
const unsigned int COLOUR_BLACK{30};
const unsigned int COLOUR_RED{31};
const unsigned int COLOUR_GREEN{32};
const unsigned int COLOUR_YELLOW{33};
const unsigned int COLOUR_BLUE{34};
const unsigned int COLOUR_MAGENTA{35};
const unsigned int COLOUR_CYAN{36};
const unsigned int COLOUR_WHITE{37};

const unsigned short MOVING_NOWHERE{0};
const unsigned short MOVING_LEFT{1};
const unsigned short MOVING_RIGHT{2};
const unsigned short MOVING_UP{3};
const unsigned short MOVING_DOWN{4};

#pragma clang diagnostic pop

// Types

// Using signed and not unsigned to avoid having to check for ( 0 - 1 ) being very large
struct position
{
    int row;
    int col;
};

struct fishie
{
    position position{1, 1};
    bool swimming = true;
    unsigned int colour = COLOUR_BLUE;
    float speed = 1.0;
};

typedef vector<fishie> fishvector;

// Globals

struct termios initialTerm;
default_random_engine generator;
uniform_int_distribution<int> movement(-1, 1);
uniform_int_distribution<unsigned int> fishcolour(COLOUR_RED, COLOUR_WHITE);

// Utilty Functions

// These two functions are taken from StackExchange and are
// all of the "magic" in this code.
auto SetupScreenAndInput() -> void
{
    struct termios newTerm;
    // Load the current terminal attributes for STDIN and store them in a global
    tcgetattr(fileno(stdin), &initialTerm);
    newTerm = initialTerm;
    // Mask out terminal echo and enable "noncanonical mode"
    // " ... input is available immediately (without the user having to type
    // a line-delimiter character), no input processing is performed ..."
    newTerm.c_lflag &= ~ICANON;
    newTerm.c_lflag &= ~ECHO;
    newTerm.c_cc[VMIN] = 1;

    // Set the terminal attributes for STDIN immediately
    auto result{tcsetattr(fileno(stdin), TCSANOW, &newTerm)};
    if (result < 0)
    {
        cerr << "Error setting terminal attributes [" << result << "]" << endl;
    }
}
auto TeardownScreenAndInput() -> void
{
    // Reset STDIO to its original settings
    tcsetattr(fileno(stdin), TCSANOW, &initialTerm);
}

//
auto SetNonblockingReadState(bool desiredState = true) -> void
{
    auto currentFlags{fcntl(0, F_GETFL)};
    if (desiredState)
    {
        fcntl(0, F_SETFL, (currentFlags | O_NONBLOCK));
    }
    else
    {
        fcntl(0, F_SETFL, (currentFlags & (~O_NONBLOCK)));
    }
    cerr << "SetNonblockingReadState [" << desiredState << "]" << endl;
}

// Everything from here on is based on ANSI codes
// Note the use of "flush" after every write to ensure the screen updates
auto ClearScreen() -> void { cout << ANSI_START << "2J" << flush; }
auto MoveTo(unsigned int x, unsigned int y) -> void { cout << ANSI_START << x << ";" << y << "H" << flush; }
auto HideCursor() -> void { cout << ANSI_START << "?25l" << flush; }
auto ShowCursor() -> void { cout << ANSI_START << "?25h" << flush; }
auto GetTerminalSize() -> position
{
    // This feels sketchy but is actually about the only way to make this work
    MoveTo(999, 999);
    cout << ANSI_START << "6n" << flush;
    string responseString;
    char currentChar{static_cast<char>(getchar())};
    while (currentChar != 'R')
    {
        responseString += currentChar;
        currentChar = getchar();
    }
    // format is ESC[nnn;mmm ... so remove the first 2 characters + split on ; + convert to unsigned int
    // cerr << responseString << endl;
    responseString.erase(0, 2);
    // cerr << responseString << endl;
    auto semicolonLocation = responseString.find(";");
    // cerr << "[" << semicolonLocation << "]" << endl;
    auto rowsString{responseString.substr(0, semicolonLocation)};
    auto colsString{responseString.substr((semicolonLocation + 1), responseString.size())};
    // cerr << "[" << rowsString << "][" << colsString << "]" << endl;
    auto rows = stoul(rowsString);
    auto cols = stoul(colsString);
    position returnSize{static_cast<int>(rows), static_cast<int>(cols)};
    // cerr << "[" << returnSize.row << "," << returnSize.col << "]" << endl;
    return returnSize;
}
auto MakeColour(string inputString,
                const unsigned int foregroundColour = COLOUR_WHITE,
                const unsigned int backgroundColour = COLOUR_IGNORE) -> string
{
    string outputString;
    outputString += ANSI_START;
    outputString += START_COLOUR_PREFIX;
    outputString += to_string(foregroundColour);
    if (backgroundColour)
    {
        outputString += ";";
        outputString += to_string((backgroundColour + 10)); // Tacky but works
    }
    outputString += START_COLOUR_SUFFIX;
    outputString += inputString;
    outputString += STOP_COLOUR;
    return outputString;
}

// Fish Logic

auto UpdateFishPositions(fishvector &fishies, char currentChar) -> void
{
    // Deal with movement commands
    int commandRowChange = 0;
    int commandColChange = 0;

    if (currentChar == UP_CHAR)
    {
        commandRowChange -= 1;
    }
    if (currentChar == DOWN_CHAR)
    {
        commandRowChange += 1;
    }
    if (currentChar == LEFT_CHAR)
    {
        commandColChange -= 1;
    }
    if (currentChar == RIGHT_CHAR)
    {
        commandColChange += 1;
    }

    // Update the position of each fish
    // Use a reference so that the actual position updates
    for (auto &currentFish : fishies)
    {
        auto currentRowChange{commandRowChange};
        auto currentColChange{commandColChange};
        if (currentFish.swimming)
        {
            currentRowChange += movement(generator);
            currentColChange += movement(generator);
        }
        auto proposedRow{currentFish.position.row + currentRowChange};
        auto proposedCol{currentFish.position.col + currentColChange};
        currentFish.position.row = max(1, min(20, proposedRow));
        currentFish.position.col = max(1, min(40, proposedCol));
    }
}
auto ToggleFrozenFish(fishvector &fishies) -> void
{
    for (auto &currentFish : fishies)
    {
        currentFish.swimming = not currentFish.swimming;
    }
}
auto CreateFishie(fishvector &fishies) -> void
{
    cerr << "Creating Fishie" << endl;
    fishie newFish{
        .position = {.row = 1, .col = 1},
        .swimming = true,
        .colour = fishcolour(generator),
        .speed = 1.0};
    fishies.push_back(newFish);
}

auto DrawFishies(fishvector fishies) -> void
{
    for (auto currentFish : fishies)
    {
        MoveTo(currentFish.position.row, currentFish.position.col);
        cout << "          O" << flush;
        MoveTo(currentFish.position.row + 1, currentFish.position.col);
        cout << "         o" << flush;
        MoveTo(currentFish.position.row + 2, currentFish.position.col);
        cout << MakeColour("><((('>", currentFish.colour) << flush;
    }
}

auto main() -> int
{
    // Set Up the system to receive input
    SetupScreenAndInput();

    // Check that the terminal size is large enough for our fishies
    const position TERMINAL_SIZE{GetTerminalSize()};
    if ((TERMINAL_SIZE.row < 30) or (TERMINAL_SIZE.col < 50))
    {
        ShowCursor();
        TeardownScreenAndInput();
        cout << endl
             << "Terminal window must be at least 30 by 50 to run this game" << endl;
        return EXIT_FAILURE;
    }

    // State Variables
    fishvector fishies;
    unsigned int ticks{0};

    char currentChar{CREATE_CHAR}; // the first act will be to create a fish
    string currentCommand;

    bool allowBackgroundProcessing{true};
    bool showCommandline{false};

    auto startTimestamp{chrono::steady_clock::now()};
    auto endTimestamp{startTimestamp};
    int elapsedTimePerTick{100}; // Every 0.1s check on things

    SetNonblockingReadState(allowBackgroundProcessing);
    ClearScreen();
    HideCursor();

    while (currentChar != QUIT_CHAR)
    {
        endTimestamp = chrono::steady_clock::now();
        auto elapsed{chrono::duration_cast<chrono::milliseconds>(endTimestamp - startTimestamp).count()};
        // We want to process input and update the world when EITHER
        // (a) there is background processing and enough time has elapsed
        // (b) when we are not allowing background processing.
        if ((allowBackgroundProcessing and (elapsed >= elapsedTimePerTick)) or (not allowBackgroundProcessing))
        {
            ticks++;
            cerr << "Ticks [" << ticks << "] allowBackgroundProcessing [" << allowBackgroundProcessing << "] elapsed [" << elapsed << "] currentChar [" << currentChar << "] currentCommand [" << currentCommand << "]" << endl;
            if (currentChar == BLOCKING_CHAR) // Toggle background processing
            {
                allowBackgroundProcessing = not allowBackgroundProcessing;
                SetNonblockingReadState(allowBackgroundProcessing);
            }
            if (currentChar == COMMAND_CHAR) // Switch into command line mode
            {
                allowBackgroundProcessing = false;
                SetNonblockingReadState(allowBackgroundProcessing);
                showCommandline = true;
            }
            if (currentCommand.compare("resume") == 0)
            {
                cerr << "Turning off command line" << endl;
                showCommandline = false;
            }
            if ((currentChar == FREEZE_CHAR) or (currentCommand.compare("freeze") == 0))
            {
                ToggleFrozenFish(fishies);
            }
            if ((currentChar == CREATE_CHAR) or (currentCommand.compare("create") == 0))
            {
                CreateFishie(fishies);
            }

            UpdateFishPositions(fishies, currentChar);
            ClearScreen();
            DrawFishies(fishies);

            if (showCommandline)
            {
                cerr << "Showing Command Line" << endl;
                MoveTo(21, 1);
                ShowCursor();
                cout << "Command:" << flush;
            }
            else
            {
                HideCursor();
            }

            // Clear inputs in preparation for the next iteration
            startTimestamp = endTimestamp;
            currentChar = NULL_CHAR;
            currentCommand.clear();
        }
        // Depending on the blocking mode, either read in one character or a string (character by character)
        if (showCommandline)
        {
            while (read(0, &currentChar, 1) == 1 && (currentChar != '\n'))
            {
                cout << currentChar << flush; // the flush is important since we are in non-echoing mode
                currentCommand += currentChar;
            }
            cerr << "Received command [" << currentCommand << "]" << endl;
            currentChar = NULL_CHAR;
        }
        else
        {
            read(0, &currentChar, 1);
        }
    }
    // Tidy Up and Close Down
    ShowCursor();
    SetNonblockingReadState(false);
    TeardownScreenAndInput();
    cout << endl; // be nice to the next command
    return EXIT_SUCCESS;
}
