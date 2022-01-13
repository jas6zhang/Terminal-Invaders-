// clang++ -std=c++17 -Wall -Werror -Wextra -Wpedantic -g3 -o team35-project team35-project.cpp

// Works best in Visual Studio Code if you set:
//   Settings -> Features -> Terminal -> Local Echo Latency Threshold = -1

#include <iostream>
#include <termios.h>
#include <vector>
#include <string>
#include <cmath>

using namespace std;

//CREATE ALIEN SPRITES
//CREATE PLAYER SPRITES
//CREATE UPWARDS BULLET SPRITES
//CREATE DOWNWARDS BULLET SPRITES
//CREATE HEALTH INDICATOR
//CREATE GAMEOVER SPRITE
//CREATE INSTRUCTION SPRITE
//Use Kronos to add pause for instructions before the game starts?

// Constants

// Disable JUST this warning (in case students choose not to use some of these constants)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-const-variable"

const char UP_CHAR{'w'};
const char DOWN_CHAR{'s'};
const char LEFT_CHAR{'a'};
const char RIGHT_CHAR{'d'};
const char QUIT_CHAR{'q'};
const char SHOOT_CHAR{' '};

// https://en.wikipedia.org/wiki/ANSI_escape_code#3-bit_and_4-bit
const string ANSI_START{"\033["};
const string START_COLOUR_PREFIX{"1;"};
const string START_COLOUR_SUFFIX{"m"};
const string STOP_COLOUR{"\033[0m"};
const unsigned int COLOUR_IGNORE{0}; // this is a little dangerous but should work out OK
const unsigned int COLOUR_WHITE{37};
const unsigned int COLOUR_RED{31};
const unsigned int COLOUR_BLUE{34};
const unsigned int COLOUR_BLACK{30};
const unsigned int COLOUR_BRIGHTGREEN{92};

// Types

//Create struct with row and column variables
//Define a type to declare the struct with everytime
struct position
{
    unsigned int row;
    unsigned int col;
};
struct bullet
{
    position position;
    string direction;
    unsigned int colour = COLOUR_RED;
};

typedef struct position positionstruct;
typedef vector<string> stringvector;
typedef vector<bullet> bulletvector;

bulletvector bullets;

#pragma clang diagnostic pop

//player sprite

//CHANGE DrawSPrite to draw just strings later
const stringvector PLAYER_SPRITE{{"^"}};
//alien sprite
const string ALIEN_SPRITE{"H"};
//bullet sprites
const string BULLET_SPRITE{"|"};
//health indicator
const string HEALTH_POINT{"❤"};
//gameover screen

//Instruction sprite

//Alien sprite

// Globals

struct termios initialTerm; // declaring variable of type "struct termios" named initialTerm

// Utilty Functions

// These two functions are taken from Stack Exchange and are
// all of the "magic" in this code.
auto SetupScreenAndInput() -> void
{
    struct termios newTerm;
    // Load the current terminal attributes for STDIN and store them in a global
    tcgetattr(fileno(stdin), &initialTerm);
    newTerm = initialTerm;
    // Mask out terminal echo (remove outputting the strings that are passed to it as arguments) and enable "noncanonical mode ( characters are not grouped into lines)"
    // " ... input is available immediately (without the user having to type
    // a line-delimiter character), no input processing is performed ..."
    newTerm.c_lflag &= ~ICANON;
    newTerm.c_lflag &= ~ECHO;
    //Bitwise assignments
    //~ means the opposite of the flag

    //fileno() -> takes the input stream actively
    //tcgetattr -> returns termios structure
    // Set the terminal attributes for STDIN immediately
    tcsetattr(fileno(stdin), TCSANOW, &newTerm);
    //TCSANOW: means change the attributes immediately
    //set terminal attributes: https://www.ibm.com/docs/en/zos/2.1.0?topic=functions-tcsetattr-set-attributes-terminal#rttcsa
}

//Reset the terminal attributes to the original settings
auto TeardownScreenAndInput() -> void
{
    // Reset STDIO to its original settings
    tcsetattr(fileno(stdin), TCSANOW, &initialTerm);
}

// Everything from here on is based on ANSI codes
//ANSI escape codes: https://gist.github.com/fnky/458719343aabd01cfb17a3a4f7296797
auto ClearScreen() -> void { cout << ANSI_START << "2J"; }                                          //2J clears the entire screen
auto MoveTo(unsigned int x, unsigned int y) -> void { cout << ANSI_START << x << ";" << y << "H"; } //ESC[{line};{column}H: moves cursor to line # and column #
auto HideCursor() -> void { cout << ANSI_START << "?25l"; }                                         //ESC[?25l : makes cursor invisible
auto ShowCursor() -> void { cout << ANSI_START << "?25h"; }                                         //ESC[?25h : make cursor visible
auto GetTerminalSize() -> position                                                                  //
{
    // This feels sketchy but is actually about the only way to make this work
    MoveTo(999, 999);
    cout << ANSI_START << "6n"; // request cursor position (reports as ESC[#;#R)
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
    position returnSize{static_cast<unsigned int>(rows), static_cast<unsigned int>(cols)};
    // cerr << "[" << returnSize.row << "," << returnSize.col << "]" << endl;
    return returnSize;
}

// This is pretty sketchy since it's not handling the graphical state very well or flexibly
auto MakeColour(string inputString,
                const unsigned int foregroundColour = COLOUR_WHITE,
                const unsigned int backgroundColour = COLOUR_IGNORE) -> string
{
    string outputString;
    outputString += ANSI_START;
    outputString += START_COLOUR_PREFIX;
    outputString += to_string(foregroundColour);
    outputString += ";";
    if (backgroundColour)
    {
        outputString += to_string((backgroundColour + 10));
    } // Tacky but works
    outputString += START_COLOUR_SUFFIX;
    outputString += inputString;
    outputString += STOP_COLOUR;
    return outputString;
}

// This is super sketchy since it doesn't do (e.g.) background removal
// or allow individual colour control of the output elements.
auto DrawSprite(position targetPosition,
                stringvector sprite,
                const unsigned int foregroundColour = COLOUR_WHITE,
                const unsigned int backgroundColour = COLOUR_IGNORE)
{
    MoveTo(targetPosition.row, targetPosition.col);

    //Painting downward, row by row
    for (auto currentSpriteRow = 0;
         currentSpriteRow < static_cast<int>(sprite.size());
         currentSpriteRow++)
    {
        cout << MakeColour(sprite[currentSpriteRow], foregroundColour, backgroundColour);
        //Move down to print the next row
        MoveTo((targetPosition.row + (currentSpriteRow + 1)), targetPosition.col);
    };
}

auto CreateBullet(bulletvector &bullets, unsigned int row, unsigned int column, string direction) -> void
{
    bullet newBullet{{row + 1, column}, direction};
    bullets.push_back(newBullet);
}

auto main() -> int
{
    // 0. Set Up the system and get the size of the screen

    SetupScreenAndInput();
    const position TERMINAL_SIZE{GetTerminalSize()};
    if ((TERMINAL_SIZE.row < 30) or (TERMINAL_SIZE.col < 50))
    {
        ShowCursor();
        TeardownScreenAndInput();
        cout << endl
             << "Terminal window must be at least 30 by 50 to run this game" << endl;
        return EXIT_FAILURE;
        //Exit failure: https://www.cplusplus.com/reference/cstdlib/EXIT_FAILURE/
    }

    // 1. Initialize State
    position currentPosition{1, 1};

    // GameLoop
    char currentChar{'z'}; // I would rather use a different approach, but this is quick

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
        // 2. Update State
        //the max and min are to make sure we don't go over the boundaries
        if (currentChar == UP_CHAR)
        {
            currentPosition.row = max(1U, (currentPosition.row - 1));
        }
        if (currentChar == DOWN_CHAR)
        {
            currentPosition.row = min(20U, (currentPosition.row + 1));
        }
        if (currentChar == LEFT_CHAR)
        {
            currentPosition.col = max(1U, (currentPosition.col - 1));
        }
        if (currentChar == RIGHT_CHAR)
        {
            currentPosition.col = min(40U, (currentPosition.col + 1));
        }
        if (currentChar == SHOOT_CHAR)
        {

            CreateBullet(bullets, currentPosition.row, currentPosition.col, "Up")

                BulletMove()

                    auto endTimestamp = chrono::steady_clock::now();
            auto elapsed{chrono::duration_cast<chrono::milliseconds>(endTimestamp - startTimestamp).count()};

            ;
        }
        //U means unsigned
        // 3. Update Screen

        // 3.A Prepare Screen
        ClearScreen();
        HideCursor(); // sometimes the Visual Studio Code terminal seems to forget

        // 3.B Draw based on state
        MoveTo(currentPosition.row, currentPosition.col);
        //Draw the sprites here
        DrawSprite({currentPosition.row, currentPosition.col}, PLAYER_SPRITE);
        for (bullet bullet : bullets)
        {
            if (bullet.direction == "Up")
            {
                MoveTo((bullet.position.row - 2), bullet.position.col);
                cout << "|";
            }
            else if (bullet.direction == "Down")
            {
                MoveTo((bullet.position.row + 2), bullet.position.col);
                cout << "|";
            }
        }
        MoveTo(21, 1);
        cout << "[" << currentPosition.row << "," << currentPosition.col << "]" << endl;

        // 4. Prepare for the next pass
        currentChar = getchar();
    }
    // N. Tidy Up and Close Down
    ShowCursor();
    TeardownScreenAndInput();
    cout << endl; // be nice to the next command
    return EXIT_SUCCESS;
}

//Game Outline:
//Set up terminal: get rid of terminal line, run commands automatically, set flag to not press return, check if it is the right size
//Set starting position
//Game loop:
//If we want aliens to move:
//Make, for instance, 6 alien spaces. The aliens move in a random direction (u,d,l,r) every loop,
//The aliens can move anywhere inside those boxes (if their random step takes them outside the box, take the negative version of that step)
//Check for player movement and move, move the aliens randomly, check for gun shots,
//+1 y value of any existing player gunshots, -1 y value of any alien gunshots,
//check if an alien position is the same as a player bullet (delete the alien if it is)
//check if a player position is the same as an alien bullet (-1 player health if yes)
//Check if there is an empty alien box -> if yes, move an alien into that position -> make a path to that position
//
//Game end:
//Check if player health is 0 -> if yes, return a gameover screen and the number of aliens killed
