// compile with: clang++ -std=c++20 -Wall -Werror -Wextra -Wpedantic -g3 -o gameloop gameloop.cpp
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
#include <thread>

// Because we are only using #includes from the standard, names shouldn't conflict
using namespace std;

// Constants

// Disable JUST this warning (in case students choose not to use some of these constants)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-const-variable"

//REMEMBER TERMINAL starts at top left corner

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
const char SHOOT_CHAR{' '};

const char YES_CHAR{'y'};
const char NO_CHAR{'n'};

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

const char PlayerSprite{'A'};
const char EnemySprite{'M'};
const char PlayerLaserSprite{'^'};
const char EnemyLaserSprite{'U'};
const char DeathExplosion{'X'};
const char Border{'|'};
const unsigned int PlayerSpaceLength{18};
const unsigned int PlayerSpaceWidth{38};

#pragma clang diagnostic pop

// Types

// Using signed and not unsigned to avoid having to check for ( 0 - 1 ) being very large
struct position
{
    int row;
    int col;
};

struct GameObject
{
    position position;

    //can't be const because of Collision;
    char sprite;

    //display is so that I don't ever have to alter the number of enemies in a vector
    bool display = true;
    bool died = false;
};

GameObject Player{{18, 20}, PlayerSprite};

vector<GameObject> EnemiesVector;
vector<GameObject> PlayerLasersVector;
vector<GameObject> EnemyLasersVector;

// flags
bool GameOver{false};
bool EndGame{false};
bool Lose{false};

bool MovingLeft{false};
bool MoveDownARow{false};
int TotalScore = 0;
int HighScore = 0;

//20 Tall and 40 wide
vector<string> GameSpace{
    "|                                                                     |",
    "|                                                                     |",
    "|                                                                     |",
    "|                                                                     |",
    "|                                                                     |",
    "|                                                                     |",
    "|                                                                     |",
    "|                                                                     |",
    "|                                                                     |",
    "|                                                                     |",
    "|                                                                     |",
    "|                                                                     |",
    "|                                                                     |",
    "|                                                                     |",
    "|                                                                     |",
    "|                                                                     |",
    "|                                                                     |",
    "|                                                                     |",
    "|                                                                     |",
    "|                                                                     |",
};

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
    cout << "test" << flush;
    auto rows = stoul(rowsString);
    auto cols = stoul(colsString);
    position returnSize{static_cast<int>(rows), static_cast<int>(cols)};
    // cerr << "[" << returnSize.row << "," << returnSize.col << "]" << endl;
    return returnSize;
}

//Create player laser
auto CreatePlayerLaser(vector<GameObject> &PlayerLasersVector, char currentChar) -> void
{
    if (currentChar == ' ')
    {
        GameObject NewPlayerLaser{{Player.position.row - 1, Player.position.col}, PlayerLaserSprite};
        PlayerLasersVector.push_back(NewPlayerLaser);
    }
}

auto StartGame() -> void
{
    //Start Screen
    char esc_char = 27; // the decimal code for escape character is 27
    cout << esc_char << "TERMINAL INVADERS[1m" << esc_char << "[0m" << endl;
    cout << "\n \n               Welcome Captain! \n \n \n \n"
         << endl;
    this_thread::sleep_for(std::chrono::milliseconds(2000));
    cout << "  Brave the TERMINAL INVADERS and come back a hero. \n \n \n \n"
         << endl;
    ;
    this_thread::sleep_for(std::chrono::milliseconds(2000));
    cout << "    Your operating system is depending upon you. \n \n \n \n"
         << endl;
    this_thread::sleep_for(std::chrono::milliseconds(2000));
    cout << "          Good luck in your journey... \n \n \n \n"
         << endl;
    this_thread::sleep_for(std::chrono::milliseconds(2000));
    cout << "           PRESS ANY KEY TO START. \n \n \n \n"
         << endl;
    getchar();
}

//Fix shooting
auto CreateEnemyLaser(vector<GameObject> &EnemyLasersVector, vector<GameObject> EnemiesVector) -> void
{

    vector<int> UsedColumns = {};

    for (string::size_type EnemyIndex = EnemiesVector.size() - 1; EnemyIndex > 0; EnemyIndex--)
    {

        auto Enemy = EnemiesVector[EnemyIndex];
        int RandomInt = (rand() % 100) + 1;
        bool IsColumnUsed = false;

        for (int ColNumber : UsedColumns)
        {
            if (ColNumber == Enemy.position.col)
            {
                IsColumnUsed = true;
            }
        }

        UsedColumns.push_back(Enemy.position.col);

        if ((RandomInt >= 70) && (Player.position.col == Enemy.position.col) && (!IsColumnUsed))
        {

            GameObject NewEnemyLaser{{Enemy.position.row + 1, Enemy.position.col}, EnemyLaserSprite};
            EnemyLasersVector.push_back(NewEnemyLaser);
        }
    }
    //I need to clear out the vector
}

auto Collision(vector<GameObject> &EnemiesVector, vector<GameObject> &PlayerLasersVector, vector<GameObject> &EnemyLasersVector) -> void
{

    //Check for enemy collision with player lasers
    for (GameObject &enemy : EnemiesVector)
    {

        for (GameObject &laser : PlayerLasersVector)
        {

            if ((enemy.position.row == laser.position.row) && (enemy.position.col == laser.position.col) && (laser.display))
            {

                if (enemy.display)
                {

                    enemy.display = false;
                    laser.display = false;
                    enemy.died = true;
                    TotalScore = TotalScore + 50;
                    if (TotalScore > HighScore)
                    {
                        HighScore = TotalScore;
                    }
                }
            }
        }
    }

    //Delete player lasers from vector if they go off the screen or if they are no longer displayed
    for (auto LaserIter = PlayerLasersVector.begin(); LaserIter != PlayerLasersVector.end();)
    {
        if ((LaserIter->display == false) || (LaserIter->position.row == 1))
        {
            LaserIter = PlayerLasersVector.erase(LaserIter);
        }
        else
        {
            ++LaserIter;
        }
    }

    //Delete enemy lasers if they go off screen or end the game if they hit the player
    for (auto LaserIter = EnemyLasersVector.begin(); LaserIter != EnemyLasersVector.end();)
    {
        if (LaserIter->position.row == PlayerSpaceLength + 1)
        {
            LaserIter = EnemyLasersVector.erase(LaserIter);
        }
        else if ((Player.position.row == LaserIter->position.row) && (Player.position.col == LaserIter->position.col))
        {
            GameOver = true;
            Lose = true;
            break;
        }
        else
        {
            ++LaserIter;
        }
    }

    //Delete enemies after they are no longer displayed
    for (auto EnemyIter = EnemiesVector.begin(); EnemyIter != EnemiesVector.end();)
    {
        if ((EnemyIter->display == false) || (EnemyIter->position.row == 20))
        {
            if (EnemiesVector.size() == 0)
            {
                break;
            }
            EnemyIter = EnemiesVector.erase(EnemyIter);
        }
        else
        {
            ++EnemyIter;
        }
    }
}

auto CreateEnemy(vector<GameObject> &EnemiesVector, int y, int x) -> void
{
    GameObject NewEnemy{{x, y}, EnemySprite};
    EnemiesVector.push_back(NewEnemy);
}

auto CreateInitialEnemyConfig(vector<GameObject> &EnemiesVector) -> void
{

    for (int row = 18; row < 55; row++)
    {
        for (int col = 1; col <= 8; col++)
        {
            if ((row % 4 == 0) && (col % 2 == 0))
            {
                CreateEnemy(EnemiesVector, row, col);
            }
        }
    }
}

// Fish Logic
auto UpdatePositions(char currentChar, vector<GameObject> &PlayerLasersVector) -> void
{
    // Deal with movement commands
    int commandColChange = 0;

    if (currentChar == LEFT_CHAR)
    {
        commandColChange -= 1;
    }
    if (currentChar == RIGHT_CHAR)
    {
        commandColChange += 1;
    }

    string::size_type VectorSize = EnemiesVector.size();

    if (EnemiesVector[VectorSize - 1].position.col == 70)
    {
        MovingLeft = true;
        MoveDownARow = true;
    }
    else if (EnemiesVector[0].position.col == 2)
    {
        MovingLeft = false;
        MoveDownARow = true;
    }

    for (GameObject &Enemy : EnemiesVector)
    {
        if (MovingLeft)
        {
            Enemy.position.col = Enemy.position.col - 1;
        }
        else
        {
            Enemy.position.col = Enemy.position.col + 1;
        }

        if (MoveDownARow)
        {
            Enemy.position.row = Enemy.position.row + 1;
        }
    }

    MoveDownARow = false;

    // Update the position of each fish
    // Use a reference so that the actual position updates

    //int proposedRow;
    int proposedCol;
    proposedCol = Player.position.col + commandColChange;

    //Borders
    Player.position.col = max(2, min(70, proposedCol));

    for (GameObject &laser : PlayerLasersVector)
    {
        laser.position.row = laser.position.row - 1;
    }

    for (GameObject &laser : EnemyLasersVector)
    {
        laser.position.row = laser.position.row + 1;
    }
}

auto DrawScore() -> void
{

    MoveTo(20, 30);
    cout << "Score: " << TotalScore << flush;

    MoveTo(21, 30);
    cout << "High Score: " << HighScore << flush;
}

auto DrawEnemies(vector<GameObject> &EnemiesVector) -> void
{
    for (auto &Enemy : EnemiesVector)
    {
        if (Enemy.display)
        {
            MoveTo(Enemy.position.row, Enemy.position.col);
            cout << Enemy.sprite << flush;
        }
        else if (Enemy.died == true)
        {
            Enemy.died = false;
            MoveTo(Enemy.position.row, Enemy.position.col);
            cout << DeathExplosion << flush;
        }
    }

    string::size_type VectorSize = EnemiesVector.size();

    if (EnemiesVector[VectorSize - 1].position.row == 17)
    {
        GameOver = true;
        Lose = true;
    }
}

auto DrawObjects() -> void
{

    MoveTo(1, 1);
    for (string::size_type GameSpaceRow = 0; GameSpaceRow < GameSpace.size(); GameSpaceRow++)
    {
        cout << GameSpace[GameSpaceRow] << flush;
        MoveTo(GameSpaceRow, 1);
    }

    MoveTo(Player.position.row, Player.position.col);
    cout << Player.sprite << flush;

    for (GameObject laser : PlayerLasersVector)
    {
        if (laser.display)
        {
            MoveTo(laser.position.row, laser.position.col);
            cout << laser.sprite << flush;
        }
    }

    for (GameObject laser : EnemyLasersVector)
    {

        if (laser.display)
        {
            MoveTo(laser.position.row, laser.position.col);
            cout << laser.sprite << flush;
        }
    }
}

auto main() -> int
{
    // Set Up the system to receive input
    SetupScreenAndInput();

    // Check that the terminal size is large enough for our fishies
    const position TERMINAL_SIZE{GetTerminalSize()};
    CreateInitialEnemyConfig(EnemiesVector);

    if ((TERMINAL_SIZE.row < 30) or (TERMINAL_SIZE.col < 50))
    {
        ShowCursor();
        TeardownScreenAndInput();
        cout << endl
             << "Terminal window must be at least 30 by 50 to run this game" << endl;
        return EXIT_FAILURE;
    }

    // State Variables
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

    //StartGame();

    while (!EndGame)
    {
        while ((currentChar != QUIT_CHAR) && (!GameOver))
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

                UpdatePositions(currentChar, PlayerLasersVector);
                CreatePlayerLaser(PlayerLasersVector, currentChar);
                CreateEnemyLaser(EnemyLasersVector, EnemiesVector);
                Collision(EnemiesVector, PlayerLasersVector, EnemyLasersVector);

                ClearScreen();

                DrawScore();
                DrawObjects();
                DrawEnemies(EnemiesVector);

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

        ClearScreen();

        if (Lose)
        {

            cout << "YOU LOST!" << endl;
            cout << "\n Valiant effort Captain, we'll get them next time. \n\n"
                 << endl;
            cout << "RESTART? (y/n)" << endl;
        }
        else
        {
            cout << "YOU WIN!" << endl;
            cout << "PLAY AGAIN? (y/n)" << endl;
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

        if (currentChar == YES_CHAR)
        {
            PlayerLasersVector.clear();
            EnemyLasersVector.clear();
            EnemiesVector.clear();
            Lose = false;
            TotalScore = 0;
            CreateInitialEnemyConfig(EnemiesVector);
            GameOver = false;
        }
        else if (currentChar == NO_CHAR)
        {
            EndGame = true;
        }

        this_thread::sleep_for(std::chrono::milliseconds(2000));

    } // big while loop

    ClearScreen();

    // Tidy Up and Close Down
    ShowCursor();
    SetNonblockingReadState(false);
    TeardownScreenAndInput();
    cout << endl; // be nice to the next command
    return EXIT_SUCCESS;
}
