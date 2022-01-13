//clang++ -std=c++17 -Wall -Werror -Wextra -Wpedantic -g3 -o team35-project team35-project.cpp

//Game Start + Game Over
#include <iostream>
#include <vector>
#include <string>
#include <chrono>
#include <thread>

#include <termios.h>

using namespace std;

typedef vector<char> charvector;
typedef vector<charvector> charmatrix;

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-const-variable"

// Globals

struct termios initialTerm; // declaring variable of type "struct termios" named initialTerm

// Utilty Functions

// These two functions are taken from Stack Exchange and are
// all of the "magic" in this code.
auto SetupScreenAndInput() -> void //Foster Code
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

    tcsetattr(fileno(stdin), TCSANOW, &newTerm);
}

//Reset the terminal attributes to the original settings
auto TeardownScreenAndInput() -> void
{
    // Reset STDIO to its original settings
    tcsetattr(fileno(stdin), TCSANOW, &initialTerm);
}

const string ANSI_START{"\033["};
auto ClearScreen() -> void { cout << ANSI_START << "2J"; } //2J clears the engire screen

//Game stops moving every two turns, then moves next turn for every player movement
auto main() -> int
{
    SetupScreenAndInput();

    // ---------------------
    char enemyLaser = 'U';
    char explosion = 'X';
    int score = 0;
    int enemyReady = 0;
    // ---------------------

    char playerLaser = '^';
    int laserReady = 1;
    int victory = 1;
    char player = 'A';
    char enemy = 'M';
    char enemyShielded = 'O';
    int sizex = 40;
    int sizey = 23;

    int x, y, yi;

    srand(time(NULL));

    charmatrix world;
    charvector worldRow;

    // for reading user input
    char currentchar;
    //const char UP_CHAR    { 'w' };
    //const char DOWN_CHAR  { 's' };
    const char LEFT_CHAR{'a'};
    const char RIGHT_CHAR{'d'};
    const char LASER_CHAR{' '};

    /*welcome screen*/
    // printf("\n \n     Welcome soldier! \n \n \n \n");
    // Sleep(1000);
    // printf("  Brave the COMMAND PROMPT INVADERS and come back a hero. \n \n \n \n");
    // Sleep(2500);
    // printf("  Your operating system is depending upon you. \n \n \n \n");
    // Sleep(2500);
    // printf("               Good luck.");
    // Sleep(1000);
    // printf("\n \n \n \n Press any key to start.");
    // getchar();

    // ************** INITIALIZE MATRIX *********************
    for (int i = 0; i < sizey; i++)
    {
        for (int j = 0; j < sizex; j++)
        {
            worldRow.push_back('h');
            //cout << world[i][j] << " ";
        }
        world.push_back(worldRow);
        worldRow.clear();
        //cout << endl;
    }
    // *******************************************************

    // ************** INITIALIZE WORLD *********************
    int totalEnemies = 0;
    for (x = 0; x < sizex; x++)
    {
        for (y = 0; y < sizey; y++)
        {
            if ((y + 1) % 2 == 0 && y < 7 && x > 4 && x < sizex - 5 && x % 2 == 0)
            {
                world[y][x] = enemy;
                totalEnemies++;
            }
            else if ((y + 1) % 2 == 0 && y >= 7 && y < 9 && x > 4 && x < sizex - 5 && x % 2 == 0)
            {
                world[y][x] = enemyShielded;
                totalEnemies = totalEnemies + 2;
            }
            else
            {
                world[y][x] = ' ';
            }
        }
    }
    // *******************************************************

    world[sizey - 1][sizex / 2] = player; // set player location

    int i = 1;
    char direction = 'l';
    int currentEnemies = totalEnemies;

    // *********************** GAME LOOP BEGINS ***************************
    while (currentEnemies > 0 && victory)
    {
        int drop = 0;
        int enemySpeed = 1 + 10 * currentEnemies / totalEnemies;
        laserReady++;

        //ClearScreen();
        cout << "\033[2J\033[1;1H"; // clear the screen

        // ************** DISPLAY WORLD *********************
        for (charvector item : world)
        {
            cout << "|" << endl;
            for (char c : item)
            {
                string temp = "";
                temp.push_back(c);
                cout << temp << " " << flush;
            }
            cout << "|" << flush;
            cout << endl;
        }
        // *******************************************************

        // ----------------- CORE LOGIC ---------------------------
        for (x = 0; x < sizex; x++)
        {
            for (y = sizey - 1; y >= 0; y--)
            {
                if (i % 2 == 0 && world[y][x] == enemyLaser && (world[y + 1][x] != enemy && world[y + 1][x] != enemyShielded))
                {
                    world[y + 1][x] = enemyLaser;
                    world[y][x] = ' ';
                }
                else if (i % 2 == 0 && world[y][x] == enemyLaser && (world[y + 1][x] == enemy || world[y + 1][x] == enemyShielded))
                {
                    world[y][x] = ' ';
                }
            }
        }
        for (x = 0; x < sizex; x++)
        {
            for (y = 0; y < sizey; y++)
            {
                if ((i % 5) == 0 && (world[y][x] == enemyShielded || world[y][x] == enemy) && (rand() % 15) > 13 && world[y + 1][x] != playerLaser)
                {
                    for (yi = y + 1; yi < sizey; yi++)
                    {
                        if (world[yi][x] == enemy || world[yi][x] == enemyShielded)
                        {
                            enemyReady = 0;
                            break;
                        }
                        enemyReady = 1;
                    }
                    if (enemyReady)
                    {
                        world[y + 1][x] = enemyLaser;
                    }
                }
                if (world[y][x] == playerLaser && world[y - 1][x] == enemy)
                {
                    world[y][x] = ' ';
                    world[y - 1][x] = explosion;
                    currentEnemies--;
                    score = score + 50;
                }
                else if (world[y][x] == playerLaser && world[y - 1][x] == enemyShielded)
                {
                    world[y][x] = ' ';
                    world[y - 1][x] = enemy;
                    currentEnemies--;
                    score = score + 50;
                }
                else if (world[y][x] == playerLaser && world[y - 1][x] == enemyLaser)
                {
                    world[y][x] = ' ';
                }
                else if (world[y][x] == explosion)
                {
                    world[y][x] = ' ';
                }
                else if ((i + 1) % 2 == 0 && world[y][x] == enemyLaser && world[y + 1][x] == player)
                {
                    world[y + 1][x] = explosion;
                    world[y][x] = ' ';
                    victory = 0;
                }
                else if (world[y][x] == playerLaser && world[y - 1][x] != enemyLaser)
                {
                    world[y][x] = ' ';
                    world[y - 1][x] = playerLaser;
                }
            }
        }

        /*update enemy direction*/
        for (y = 0; y < sizey; y++)
        {
            if (world[y][0] == enemy)
            {
                direction = 'r';
                drop = 1;
                break;
            }
            if (world[y][sizex - 1] == enemy)
            {
                direction = 'l';
                drop = 1;
                break;
            }
        }

        /*update board*/
        if (i % enemySpeed == 0)
        {
            if (direction == 'l')
            {
                for (x = 0; x < sizex - 1; x++)
                {
                    for (y = 0; y < sizey; y++)
                    {
                        if (drop && (world[y - 1][x + 1] == enemy || world[y - 1][x + 1] == enemyShielded))
                        {
                            world[y][x] = world[y - 1][x + 1];
                            world[y - 1][x + 1] = ' ';
                        }
                        else if (!drop && (world[y][x + 1] == enemy || world[y][x + 1] == enemyShielded))
                        {
                            world[y][x] = world[y][x + 1];
                            world[y][x + 1] = ' ';
                        }
                    }
                }
            }
            else
            {
                for (x = sizex; x > 0; x--)
                {
                    for (y = 0; y < sizey; y++)
                    {
                        if (drop && (world[y - 1][x - 1] == enemy || world[y - 1][x - 1] == enemyShielded))
                        {
                            world[y][x] = world[y - 1][x - 1];
                            world[y - 1][x - 1] = ' ';
                        }
                        else if (!drop && (world[y][x - 1] == enemy || world[y][x - 1] == enemyShielded))
                        {
                            world[y][x] = world[y][x - 1];
                            world[y][x - 1] = ' ';
                        }
                    }
                }
            }
            for (x = 0; x < sizex; x++)
            {
                if (world[sizey - 1][x] == enemy) //enemy reaches user
                {
                    victory = 0;
                }
            }
        }

        // --------------------------------------------------------

        // ***************** MOVE PLAYER *************************

        currentchar = getchar();

        if (currentchar == LEFT_CHAR)
        {
            for (x = 0; x < sizex; x = x + 1)
            {
                if (world[sizey - 1][x + 1] == player) // if this is the location of the player most player left,
                //replaces hole in matrix with " ";
                {
                    world[sizey - 1][x] = player;
                    world[sizey - 1][x + 1] = ' ';
                }
            }
        }

        if (currentchar == RIGHT_CHAR)
        {
            for (x = sizex - 1; x > 0; x = x - 1)
            {
                if (world[sizey - 1][x - 1] == player)
                {
                    world[sizey - 1][x] = player;
                    world[sizey - 1][x - 1] = ' ';
                }
            }
        }

        if (currentchar == LASER_CHAR)
        {
            for (x = 0; x < sizex; x = x + 1)
            {
                if (world[sizey - 1][x] == player)
                {
                    world[sizey - 2][x] = playerLaser;
                    laserReady = 0;
                }
            }
        }
        // *******************************************************

        i++; // VERY IMPORTANT DO NOT DELETE THIS

        // sleep --> https://stackoverflow.com/questions/1658386/sleep-function-in-c
        chrono::milliseconds timespan(50);
        this_thread::sleep_for(timespan); //sleeps for timespan millisecconds
    }

    ClearScreen();
    //cout << "\033[2J\033[1;1H"; // clear the screen

    // ************** DISPLAY WORLD *********************
    for (charvector item : world)
    {
        cout << "|";
        for (char c : item)
        {
            string temp = "";
            temp.push_back(c);
            cout << temp << " ";
        }
        cout << "|";
        cout << endl;
    }
    // *******************************************************

    /*int totalEnemies; 

    for (int x = 0 ; x < sizex ; x++){
        for (int y = 0 ; y < sizey ; y++){
            if ((y+1) % 2 == 0 && y < 7 && x > 4 && x < sizex - 5 && x % 2 == 0){

            }
        }
    }*/

    return 0;
}