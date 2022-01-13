// compile with: clang++ -std=c++20 -Wall -Werror -Wextra -Wpedantic -g3 -o team35-terminalinvaders team35-terminalinvaders.cpp

// Works best in Visual Studio Code if you set:
//   Settings -> Features -> Terminal -> Local Echo Latency Threshold = -1

//-----------------TEAM 35 TERMINAL INVADERS GAME-------------------
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

using namespace std;

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-const-variable"

// Constants
const char NULL_CHAR{'z'};
const char UP_CHAR{'w'};
const char DOWN_CHAR{'s'};
const char LEFT_CHAR{'a'};
const char RIGHT_CHAR{'d'};
const char QUIT_CHAR{'q'};
const char SHOOT_CHAR{' '};

// Replay Constants
const char YES_CHAR{'y'};
const char NO_CHAR{'n'};

// Colour Constants
const string ANSI_START{"\033["};
const string START_COLOUR_PREFIX{"1;"};
const string START_COLOUR_SUFFIX{"m"};
const string STOP_COLOUR{"\033[0m"};
// the decimal code for escape character is 27
const char esc_char = 27;

const unsigned int COLOUR_IGNORE{0};
const unsigned int COLOUR_BLACK{30};
const unsigned int COLOUR_RED{31};
const unsigned int COLOUR_GREEN{32};
const unsigned int COLOUR_YELLOW{33};
const unsigned int COLOUR_BLUE{34};
const unsigned int COLOUR_MAGENTA{35};
const unsigned int COLOUR_CYAN{36};
const unsigned int COLOUR_WHITE{37};

// Sprite Constants
const char PlayerSprite{'A'};
const char EnemySprite{'M'};
const char PlayerLaserSprite{'^'};
const char EnemyLaserSprite{'U'};
const char DeathExplosion{'X'};

// Game Space Constants
const char Border{'|'};
const unsigned int PlayerSpaceLength{18};
const unsigned int PlayerSpaceWidth{38};

#pragma clang diagnostic pop

//Store position coordinates of objects using a structure
struct position
{
    int row;
    int col;
};

// All objects moving onscreen (invaders, lasers and player) is classified as a game object
// The structure stores all of the important characteristics of an object
struct GameObject
{
    position position;
    char sprite;
    bool display = true;
    bool died = false;
};

// Initialize player object
GameObject Player{{18, 30}, PlayerSprite};

// Setting up vectors for different data types
typedef vector<GameObject> GameObjectVector;
typedef vector<int> IntVector;
typedef vector<string> StringVector;

// Vectors to store multiples of a single object
GameObjectVector EnemiesVector;
GameObjectVector PlayerLasersVector;
GameObjectVector EnemyLasersVector;

// Flags
bool GameOver{false};
bool EndGame{false};
bool Lose{false};
bool MovingLeft{false};
bool MoveDownArrow{false};

string Difficulty{""};
int elapsedTimePerTick{100};

// GameSpace sets up the game boundaries: 20 Tall and 70 wide
StringVector GameSpace{
    "||                                                                     ||",
    "||                                                                     ||",
    "||                                                                     ||",
    "||                                                                     ||",
    "||                                                                     ||",
    "||                                                                     ||",
    "||                                                                     ||",
    "||                                                                     ||",
    "||                                                                     ||",
    "||                                                                     ||",
    "||                                                                     ||",
    "||                                                                     ||",
    "||                                                                     ||",
    "||                                                                     ||",
    "||                                                                     ||",
    "||                                                                     ||",
    "||                                                                     ||",
    "||                                                                     ||",
    "||                                                                     ||",
    "||                                                                     ||",
};

// Global Variables
struct termios initialTerm;
int TotalScore = 0;
int HighScore = 0;

// Utilty Functions For Screen (Insipired from Professor Foster's Code)
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
    // Reset STD I/O to its original settings
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
}

//ANSI escape code commands
auto ClearScreen() -> void { cout << ANSI_START << "2J" << flush; }
auto MoveTo(unsigned int x, unsigned int y) -> void { cout << ANSI_START << x << ";" << y << "H" << flush; }
auto HideCursor() -> void { cout << ANSI_START << "?25l" << flush; }
auto ShowCursor() -> void { cout << ANSI_START << "?25h" << flush; }

auto GetTerminalSize() -> position
{
    MoveTo(999, 999);
    cout << ANSI_START << "6n" << flush;
    string responseString;
    char currentChar{static_cast<char>(getchar())};
    while (currentChar != 'R')
    {
        responseString += currentChar;
        currentChar = getchar();
    }
    responseString.erase(0, 2);

    auto semicolonLocation = responseString.find(";");
    auto rowsString{responseString.substr(0, semicolonLocation)};
    auto colsString{responseString.substr((semicolonLocation + 1), responseString.size())};
    cout << "test" << flush;
    auto rows = stoul(rowsString);
    auto cols = stoul(colsString);
    position returnSize{static_cast<int>(rows), static_cast<int>(cols)};
    return returnSize;
}

// GAME CODE BEGINS HERE

// Start screen
auto StartGame() -> void
{
    int flag;
    cout << "\n"
         << endl;
    cout << ANSI_START << START_COLOUR_PREFIX << COLOUR_GREEN << START_COLOUR_SUFFIX << esc_char << "[1m";
    cout << " ████████╗███████╗██████╗ ███╗   ███╗██╗███╗   ██╗ █████╗ ██╗         ██╗███╗   ██╗██╗   ██╗ █████╗ ██████╗ ███████╗██████╗ ███████╗" << endl;
    cout << " ╚══██╔══╝██╔════╝██╔══██╗████╗ ████║██║████╗  ██║██╔══██╗██║         ██║████╗  ██║██║   ██║██╔══██╗██╔══██╗██╔════╝██╔══██╗██╔════╝" << endl;
    cout << "    ██║   █████╗  ██████╔╝██╔████╔██║██║██╔██╗ ██║███████║██║         ██║██╔██╗ ██║██║   ██║███████║██║  ██║█████╗  ██████╔╝███████╗" << endl;
    cout << "    ██║   ██╔══╝  ██╔══██╗██║╚██╔╝██║██║██║╚██╗██║██╔══██║██║         ██║██║╚██╗██║╚██╗ ██╔╝██╔══██║██║  ██║██╔══╝  ██╔══██╗╚════██║" << endl;
    cout << "    ██║   ███████╗██║  ██║██║ ╚═╝ ██║██║██║ ╚████║██║  ██║███████╗    ██║██║ ╚████║ ╚████╔╝ ██║  ██║██████╔╝███████╗██║  ██║███████║" << esc_char << "[0m" << STOP_COLOUR << endl;
    cout << "\n\n\t\t\t\t\t\t\t Welcome Captain! \n \n \n \n"
         << endl;
    this_thread::sleep_for(chrono::milliseconds(2000)); // Pause for 2 seconds before displaying each text
    cout << "\t\t\t\t\t Brave the TERMINAL INVADERS and come back a hero. \n \n \n \n"
         << endl;
    this_thread::sleep_for(chrono::milliseconds(2000));
    cout << "\t\t\t\t\t   Your operating system is depending upon you. \n \n \n \n"
         << endl;
    this_thread::sleep_for(chrono::milliseconds(2000));
    cout << "\t\t\t\t\t\t   Good luck in your journey... \n \n \n \n"
         << endl;
    this_thread::sleep_for(chrono::milliseconds(2000));

    cout << ANSI_START << START_COLOUR_PREFIX << COLOUR_GREEN << START_COLOUR_SUFFIX << esc_char << "[1m";
    cout << " ===================================================================================================================================" << STOP_COLOUR << endl;
    cout << ANSI_START << START_COLOUR_PREFIX << COLOUR_BLUE << START_COLOUR_SUFFIX << esc_char << "[1m"
         << "\n\n\n\t\t\t\t\t\t\t  DIFFICULTY" << esc_char << "[0m" << STOP_COLOUR << endl;
    cout << "\n\t\t\t\t\t       ENTER NUMBER FOR GAME DIFFICULTY" << endl;
    cout << "\n\t\t\t\t\t\t\t    EASY: 1" << endl;
    cout << "\n\t\t\t\t\t\t\t   MEDIUM: 2" << endl;
    cout << "\n\t\t\t\t\t\t\t    HARD: 3 \n \n \n \n"
         << endl;

    SetNonblockingReadState(false);
    TeardownScreenAndInput();
    ShowCursor();

    MoveTo(41, 64);
    while (Difficulty != "1" and Difficulty != "2" and Difficulty != "3")
    {
        string str;
        getline(cin, str);
        Difficulty = str;
    }
    // Unblock read state for user key press
    SetNonblockingReadState(true);

    cout << ANSI_START << START_COLOUR_PREFIX << COLOUR_GREEN << START_COLOUR_SUFFIX << esc_char << "[1m";
    cout << " \n\n\n===================================================================================================================================" << STOP_COLOUR << endl;
    cout << ANSI_START << START_COLOUR_PREFIX << COLOUR_BLUE << START_COLOUR_SUFFIX << esc_char << "[1m"
         << "\n\n\n\t\t\t\t\t\t\t  INSTRUCTIONS" << esc_char << "[0m" << STOP_COLOUR << endl;

    cout << "\n\t\t\t\t\t    SHOOT THE INVADERS WITH YOUR LASERS WHILE" << endl;
    cout << "\n\t\t\t\t     AVOIDING THEIR ATTACKS BEFORE THEY REACH THE BOTTOM." << endl;
    cout << "\n\n\t\t\t\t\t\t\t  MOVE RIGHT: D" << endl;
    cout << "\n\t\t\t\t\t\t\t  MOVE LEFT: A" << endl;

    cout << "\n\t\t\t\t\t\t\t  SHOOT LASER: SPACE BAR" << endl;
    cout << "\n\t\t\t\t\t\t\t  QUIT GAME: Q" << endl;

    cout << ANSI_START << START_COLOUR_PREFIX << COLOUR_GREEN << START_COLOUR_SUFFIX << esc_char << "[1m";
    cout << "\n\n\n\n\n\t\t\t\t\t\t     PRESS ANY KEY TO START. \n \n \n \n"
         << esc_char << "[0m" << STOP_COLOUR << endl;

    SetupScreenAndInput();
    SetNonblockingReadState(false);

    flag = getc(stdin);

    SetNonblockingReadState(true);
}

// Checks whether player has won by using the score global variable
auto WinGame() -> void
{
    if (TotalScore == 1800) // 1800 is the maximum number of points
    {
        GameOver = true; // Set state variable to true
    }
}

// Create vector of player lasers
auto CreatePlayerLaser(GameObjectVector &PlayerLasersVector, char currentChar) -> void
{
    if (currentChar == SHOOT_CHAR) // When user presses shoot command
    {
        GameObject NewPlayerLaser{{Player.position.row - 1, Player.position.col}, PlayerLaserSprite};
        PlayerLasersVector.push_back(NewPlayerLaser); // Adds new laser arrow to current vector
    }
}

// Create vector of enemy lasers
auto CreateEnemyLaser(GameObjectVector &EnemyLasersVector, GameObjectVector EnemiesVector) -> void
{

    IntVector UsedColumns = {};

    //Iterates backwards because the enemies are displaying top to down
    //Therefore checking for used column must be done in reverse order
    for (string::size_type EnemyIndex = EnemiesVector.size() - 1; EnemyIndex > 0; EnemyIndex--)
    {

        auto Enemy = EnemiesVector[EnemyIndex];
        int RandomInt = (rand() % 100) + 1; // Random variable used to randomize where the laser comes from
        bool IsColumnUsed = false;

        // Ensures that only the enemy in the front of the row can shoot
        for (int ColNumber : UsedColumns)
        {
            if (ColNumber == Enemy.position.col)
            {
                IsColumnUsed = true;
            }
        }

        UsedColumns.push_back(Enemy.position.col);

        if ((RandomInt >= 70) and (Player.position.col == Enemy.position.col) and (!IsColumnUsed))
        {

            GameObject NewEnemyLaser{{Enemy.position.row + 1, Enemy.position.col}, EnemyLaserSprite};
            EnemyLasersVector.push_back(NewEnemyLaser);
        }
    }
}

// Function to handle all collisions
auto Collision(GameObjectVector &EnemiesVector, GameObjectVector &PlayerLasersVector, GameObjectVector &EnemyLasersVector) -> void
{

    //Check for enemy collision with player lasers
    for (GameObject &enemy : EnemiesVector)
    {
        for (GameObject &laser : PlayerLasersVector)
        {

            // If player laser arrives at an enemy location
            if ((enemy.position.row == laser.position.row) and (enemy.position.col == laser.position.col) and (laser.display))
            {

                if (enemy.display) // Set the enemy displays to false
                {
                    enemy.display = false;
                    laser.display = false;
                    enemy.died = true;
                    TotalScore = TotalScore + 50; // Add to score once an enemy is defeated
                    if (TotalScore > HighScore)
                    {
                        HighScore = TotalScore; // If total score surpasses high score from previous game iterations, then replace it
                    }
                }
            }
        }
    }

    //Delete player lasers from vector if they go off the screen or if they are no longer displayed from collisions
    for (auto LaserIter = PlayerLasersVector.begin(); LaserIter != PlayerLasersVector.end();)
    {
        // Use arrow operator with LaserIter pointer to access display element in structs
        if ((LaserIter->display == false) or (LaserIter->position.row == 1))
        {
            LaserIter = PlayerLasersVector.erase(LaserIter);
        }
        else
        {
            LaserIter += 1; // If conditions are not met, move the laser one row up
        }
    }

    //Delete enemy lasers if they go off screen or end the game if they hit the player
    for (auto LaserIter = EnemyLasersVector.begin(); LaserIter != EnemyLasersVector.end();)
    {
        if (LaserIter->position.row == PlayerSpaceLength + 1)
        {
            LaserIter = EnemyLasersVector.erase(LaserIter);
        }
        // When enemy laser is at the coordinates of the player
        else if ((Player.position.row == LaserIter->position.row) and (Player.position.col == LaserIter->position.col))
        {
            GameOver = true;
            Lose = true;
            break;
        }
        else
        {
            LaserIter += 1;
        }
    }

    //Delete enemies if they go off screen or have their display as false due to being hit by player's laser
    for (auto EnemyIter = EnemiesVector.begin(); EnemyIter != EnemiesVector.end();)
    {
        if ((EnemyIter->display == false) or (EnemyIter->position.row == 20))
        {
            if (EnemiesVector.size() == 0)
            {
                break;
            }
            EnemyIter = EnemiesVector.erase(EnemyIter);
        }
        else
        {
            EnemyIter += 1;
        }
    }
}

// Create the vector of enemies

auto CreateEnemy(GameObjectVector &EnemiesVector, int y, int x) -> void
{
    // Link a new enemy sprite and the coordinates passed as a enemy object
    GameObject NewEnemy{{x, y}, EnemySprite};

    EnemiesVector.push_back(NewEnemy); // Push the enemy into the rest of the enemies vector
}

// Display the enemies
auto CreateInitialEnemyConfig(GameObjectVector &EnemiesVector) -> void
{

    for (int row = 18; row < 55; row += 1)
    {
        // Starting position of enemies must be between these rows and columns
        for (int col = 1; col <= 8; col += 1)
        {

            if ((row % 4 == 0) and (col % 2 == 0))
            // For every 4th row and 2nd column, create an enemy (to create matrix-like structure)
            {
                CreateEnemy(EnemiesVector, row, col);
            }
        }
    }
}

// Function updates the position of all moving objects in different cases
//Pass by reference is used to update the actual values in the vectors
auto UpdatePositions(char currentChar, GameObjectVector &PlayerLasersVector) -> void
{
    // Deal with movement commands
    int commandColChange = 0;

    // Player movements left and right (change column positions by 1)
    if (currentChar == LEFT_CHAR)
    {
        commandColChange -= 1;
    }
    if (currentChar == RIGHT_CHAR)
    {
        commandColChange += 1;
    }

    string::size_type VectorSize = EnemiesVector.size();

    // Conditionals for when the enemy group hits one of the wall boundaries
    // In both cases when the group hits the right or left wall, it will move down as well

    //If the last element of the vector (which will always be rightmost) hits the wall on the right, start moving left
    if (EnemiesVector[VectorSize - 1].position.col == 71)
    {
        MovingLeft = true;
        MoveDownArrow = true;
    }
    //If the first element of the vector (which will always be leftmost) hits the wall on the left, start moving right
    else if (EnemiesVector[0].position.col == 3)
    {
        MovingLeft = false;
        MoveDownArrow = true;
    }

    // Handle the coordinate changes
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

        if (MoveDownArrow)
        {
            Enemy.position.row = Enemy.position.row + 1;
        }
    }

    MoveDownArrow = false;

    int proposedCol;
    proposedCol = Player.position.col + commandColChange;

    // Player cannot go past the left and right borders by setting min and max coordinates
    Player.position.col = max(2, min(70, proposedCol));

    // Player lasers will always go up
    for (GameObject &laser : PlayerLasersVector)
    {
        laser.position.row = laser.position.row - 1;
    }

    // Enemy lasers will always go down
    for (GameObject &laser : EnemyLasersVector)
    {
        laser.position.row = laser.position.row + 1;
    }
}

// Display scores, including high score from previous rounds
auto DrawScore() -> void
{

    MoveTo(20, 33);
    cout << "Score: " << TotalScore << flush;

    MoveTo(21, 30);
    cout << "High Score: " << HighScore << flush;
}

// Display instructions
auto DrawInstructions() -> void
{

    MoveTo(6, 80);
    cout << ANSI_START << START_COLOUR_PREFIX << COLOUR_BLUE << START_COLOUR_SUFFIX << esc_char << "[1m"
         << "INSTRUCTIONS" << esc_char << "[0m" << STOP_COLOUR << endl;
    MoveTo(8, 80);
    cout << "MOVE RIGHT: D" << flush;
    MoveTo(10, 80);
    cout << "MOVE LEFT: A" << flush;

    MoveTo(12, 80);
    cout << "SHOOT LASER: SPACE BAR" << flush;

    MoveTo(14, 80);
    cout << "QUIT GAME: Q" << flush;
}

auto DrawEnemies(GameObjectVector &EnemiesVector) -> void
{
    // Go through each enemy in the vector of enemy objects
    for (auto &Enemy : EnemiesVector)
    {
        if (Enemy.display)
        {
            // Get the stored coordinates of each enemy and output them onto terminal screen

            MoveTo(Enemy.position.row, Enemy.position.col);
            cout << Enemy.sprite << flush;
        }
        else if (Enemy.died == true)
        {
            // Add explosion character when enemy dies
            Enemy.died = false;
            MoveTo(Enemy.position.row, Enemy.position.col);
            cout << DeathExplosion << flush;
        }
    }

    string::size_type VectorSize = EnemiesVector.size();

    // If an enemy reaches the bottom row where the player is, game is over
    if (EnemiesVector[VectorSize - 1].position.row == 17)
    {
        GameOver = true;
        Lose = true;
    }
}

//Draws every object other than the enemies and the score
auto DrawObjects() -> void
{

    MoveTo(1, 1);
    for (string::size_type GameSpaceRow = 0; GameSpaceRow < GameSpace.size(); GameSpaceRow += 1)
    {
        cout << GameSpace[GameSpaceRow] << flush;
        MoveTo(GameSpaceRow, 1);
    }

    MoveTo(Player.position.row, Player.position.col);
    //Display the colooured player sprite at its current location
    cout << ANSI_START << START_COLOUR_PREFIX << COLOUR_GREEN << START_COLOUR_SUFFIX << Player.sprite << STOP_COLOUR << flush;

    // Go through each laser in the vector of lasers from the player and enemies
    // Obtain their associated coordinates and display the laser arrows on the terminal

    for (GameObject laser : PlayerLasersVector)
    {
        if (laser.display)
        {
            MoveTo(laser.position.row, laser.position.col);

            //display the laser arrow from player
            cout << ANSI_START << START_COLOUR_PREFIX << COLOUR_GREEN << START_COLOUR_SUFFIX << laser.sprite << STOP_COLOUR << flush;
        }
    }

    for (GameObject laser : EnemyLasersVector)
    {

        if (laser.display)
        {
            MoveTo(laser.position.row, laser.position.col);
            //display the laser arrow from enemies
            cout << ANSI_START << START_COLOUR_PREFIX << COLOUR_RED << START_COLOUR_SUFFIX << laser.sprite << STOP_COLOUR << flush;
        }
    }
}

auto main() -> int
{
    // Set Up the system to receive input
    SetupScreenAndInput();

    const position TERMINAL_SIZE{GetTerminalSize()};

    CreateInitialEnemyConfig(EnemiesVector);

    // Check that the terminal size is large enough for game, if not exit
    if ((TERMINAL_SIZE.row < 30) or (TERMINAL_SIZE.col < 130))
    {
        ShowCursor();
        TeardownScreenAndInput();
        cout << endl
             << "Terminal window must be at least 30 by 130 to run this game" << endl;
        return EXIT_FAILURE;
    }

    // State Variables
    unsigned int ticks{0};

    char currentChar;
    string currentCommand;

    bool allowBackgroundProcessing{true};
    bool showCommandline{false};

    // Time variables
    auto startTimestamp{chrono::steady_clock::now()};
    auto endTimestamp{startTimestamp};
    int elapsedTimePerTick{100}; // For every 0.1s, check and update program

    // Set up game screen
    SetNonblockingReadState(allowBackgroundProcessing);
    ClearScreen();
    HideCursor();

    StartGame();

    if (Difficulty == "1")
    {
        elapsedTimePerTick = 120; // For every 0.1s, check and update program
    }
    else if (Difficulty == "2")
    {
        elapsedTimePerTick = 100;
    }
    else if (Difficulty == "3")
    {
        elapsedTimePerTick = 80;
    }

    // Game loop function --> controls the flow of program

    while (!EndGame)
    {
        // Game updates
        while ((currentChar != QUIT_CHAR) and (!GameOver))
        {
            endTimestamp = chrono::steady_clock::now();
            auto elapsed{chrono::duration_cast<chrono::milliseconds>(endTimestamp - startTimestamp).count()};

            // We want to process input and update the world when EITHER
            // (a) there is background processing and enough time has elapsed
            // (b) when we are not allowing background processing.
            if ((allowBackgroundProcessing and (elapsed >= elapsedTimePerTick)) or (not allowBackgroundProcessing))
            {
                ticks += 1;

                // Updates all game objects and check certain conditions
                UpdatePositions(currentChar, PlayerLasersVector);
                CreatePlayerLaser(PlayerLasersVector, currentChar);
                CreateEnemyLaser(EnemyLasersVector, EnemiesVector);

                Collision(EnemiesVector, PlayerLasersVector, EnemyLasersVector); // Check collision
                WinGame();                                                       // Check win condition

                ClearScreen();

                DrawScore();
                DrawInstructions();
                DrawObjects();
                DrawEnemies(EnemiesVector);

                // If user wants to see command line with the command char
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
                while (read(0, &currentChar, 1) == 1 and (currentChar != '\n'))
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

        // Lose Screen with option to replay the game
        if (Lose or currentChar == QUIT_CHAR)
        {
            MoveTo(1, 30);
            cout << ANSI_START << START_COLOUR_PREFIX << COLOUR_RED << START_COLOUR_SUFFIX << esc_char << "[1m"
                 << "\n\t\t\t     YOU LOST!" << esc_char << "[0m" << STOP_COLOUR << endl;

            cout << "\n\tValiant effort Captain, we'll get them next time. \n"
                 << endl;
            cout << "\n\t\t\tYour score is: " + to_string(TotalScore)
                 << endl;
            cout << "\n\n \t\t\t  RESTART? (y/n)" << endl;
        }
        else
        {
            MoveTo(1, 30);
            cout << ANSI_START << START_COLOUR_PREFIX << COLOUR_GREEN << START_COLOUR_SUFFIX << esc_char << "[1m"
                 << "\n\t\t\t       YOU WIN! \n\n"
                 << esc_char << "[0m" << STOP_COLOUR << endl;
            cout << "\t\t\t   PLAY AGAIN? (y/n)" << endl;
        }

        // Depending on the blocking mode, either read in one character or a string (character by character)
        // (Insipired by Professor Foster's code)
        if (showCommandline)
        {
            try
            {
                while (read(0, &currentChar, 1) == 1 and (currentChar != '\n'))
                {
                    cout << currentChar << flush; // the flush is important since we are in non-echoing mode
                    currentCommand += currentChar;
                }
            }
            catch (...)
            {

                cerr << "Received command [" << currentCommand << "]" << endl;
                currentChar = NULL_CHAR;
            }
        }
        else
        {
            read(0, &currentChar, 1);
        }

        // User response to game restart
        if (currentChar == YES_CHAR)
        {
            // If yes to restart, clear the screen, and then set up the initial parameters again
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

        this_thread::sleep_for(chrono::milliseconds(1500));
    }
    ClearScreen();

    // Tidy Up and Close Down
    ShowCursor();
    SetNonblockingReadState(false);
    TeardownScreenAndInput();
    cout << endl;
    return EXIT_SUCCESS;
}
