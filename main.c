// Platform checks
#define LINUX (defined(__linux__) && __linux__)


// Includes
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if LINUX
	#include <time.h>
	#include <sys/select.h>
	#include <unistd.h>
	#include <termios.h>
	extern void cfmakeraw (struct termios *);	// HACK
#else
	#include <conio.h>
#endif


// Defines
#define WIDTH 20
#define HEIGHT 5
#define PLAYER_Y 2

#define LEFT 1
#define RIGHT 2
#define FIRE 3
#define QUIT 4


// Types
typedef unsigned short Tile;
typedef unsigned char Type;
typedef unsigned char Colour;
typedef unsigned char MetaData;


// Enums
enum
{
	// First 2 bits for type
	kTypeMask = (1 << 2) - 1,
	kTypeShift = 0,
	kEmpty = 0,
	kBarrier = 1,
	kShip = 2,
	kBullet = 3,
	
	// Next 6 bits for colour, RGBRGB style
	kColourMask = (1 << 6) - 1,
	kColourShift = 2,
	kBlack = 0,
	kRedFront = 1 << 0,
	kGreenFront = 1 << 1,
	kBlueFront = 1 << 2,
	kRedBack = 1 << 3,
	kGreenBack = 1 << 4,
	kBlueBack = 1 << 5,
	
	// 8 bits for metadata
	kMetaMask = (1 << 8) - 1,
	kMetaShift = 8,
};


// Statics
Tile grid[HEIGHT][WIDTH];
unsigned char playerPos;

#if LINUX
struct termios orig_termios;	// taken from http://stackoverflow.com/a/448982
#endif


// Forward declares
Type GetType(Tile);
Colour GetColour(Tile);
MetaData GetMeta(Tile);
void PrintChar(char ch, Colour col);
void Init(void);
void Shutdown(void);
void Draw(void);
int Update(int*);
int GetKeyPressed(void);
void BeginListening();
void StopListening();


// Main entry point
int main()
{
	Init();
	
	// Game loop
	int finished = 0;
	Draw();
	do
	{
		int changedState = Update(&finished);
		
		if (changedState)
			Draw();
	}
	while(!finished);
	
	Shutdown();
	
	printf("\nThanks for playing!\n");
	
	return 0;
}


// Draw the grid
void Draw()
{
	// Drawing and listening for input breaks things
	StopListening();
	
	// Upside down
	for (int y=HEIGHT-1; y>=0; y--)
	{
		for (int x=0; x<WIDTH; x++)
		{
			Tile sq = grid[y][x];
			
			Colour col = GetColour(sq);
			MetaData meta = GetMeta(sq);
			
			// Find the character we want to print
			char c;
			switch (GetType(sq))
			{
				case kEmpty:
					c = ' ';
					break;
					
				case kBarrier:
					// TODO: damage
					c = '#';
					break;
					
				case kShip:
					// TODO: enemy vs player
					c = 'v';
					break;
					
				case kBullet:
					c = '.';
					break;
			}
			
			// Print it
			PrintChar(c, col);
		}
		
		// Newline shouldn't have a colour
		PrintChar('\n', kBlack);
	}
	
	// Start listening again
	BeginListening();
}


// Get the type of a tile
Type GetType(Tile sq)
{
	return (sq >> kTypeShift) & kTypeMask;
}


// Get the colour of a tile
Colour GetColour(Tile sq)
{
	return (sq >> kColourShift) & kColourMask;
}


// Get the metadata of a tile
MetaData GetMeta(Tile sq)
{
	return (sq >> kMetaShift) & kMetaMask;
}


// Print a character with a set colour
void PrintChar(char ch, Colour col)
{
	// Don't update more often than we need
	static Colour currentColour = (kRedFront | kGreenFront | kBlueFront) << kColourShift;	// default is white front, black back
	
	if (currentColour != col)
	{
#if LINUX
#else
ERROR - need to do this
#endif
		
		currentColour = col;
	}
	
	// Print it
	putchar(ch);
}


// Initialise everything
void Init()
{
	// Start listening for input
	BeginListening();
	
	// Add a basic border
	const Tile borderTile =
		(kBarrier << kTypeShift) /* barrier */ |
		((kRedFront | kGreenFront | kBlueFront) << kColourShift) /* white */ |
		(255 << kMetaShift) /* 255 health so it shouldn't get broken */
	;
	
	for (int x=0; x<WIDTH; x++)
	{
		grid[0][x] = borderTile;
		grid[HEIGHT-1][x] = borderTile;
	}
	for (int y=0; y<HEIGHT; y++)
	{
		grid[y][0] = borderTile;
		grid[y][WIDTH-1] = borderTile;
	}
	
	
	// Set the player position
	const Tile playerTile =
		(kShip << kTypeShift) /* barrier */ |
		((kRedFront | kGreenFront | kBlueFront) << kColourShift) /* white */ |
		(5 << kMetaShift) /* 5 health */
	;
	
	playerPos = WIDTH / 2;
	grid[PLAYER_Y][playerPos] = playerTile;
}


// Shutdown everything
void Shutdown()
{
	StopListening();
}


// Check for input and tick the game code
int Update(int* finished)
{
	int button = GetKeyPressed();
	int changedState = button != 0;
	
	switch (button)
	{
		case 0:
			// Nothing pressed
			break;
			
		case LEFT:
			if (playerPos > 1)
			{
				grid[PLAYER_Y][playerPos-1] = grid[PLAYER_Y][playerPos];
				grid[PLAYER_Y][playerPos] = 0;
				playerPos--;
			}
			else
			{
				changedState = 0;
			}
			break;
			
		case RIGHT:
			if (playerPos < WIDTH - 2)
			{
				grid[PLAYER_Y][playerPos+1] = grid[PLAYER_Y][playerPos];
				grid[PLAYER_Y][playerPos] = 0;
				playerPos++;
			}
			else
			{
				changedState = 0;
			}
			break;
			
		case QUIT:
			// We're done
			*finished = 1;
			changedState = 0;
			break;
	}
	
	// The state of the game changed
	return changedState;
}


// See if a key has been pressed
int GetKeyPressed()
{
	int pressed = 0;
	
#if LINUX
	// Check for stdin change
	struct timeval tv = { 0L, 0L };
	fd_set fds;
	FD_ZERO(&fds);
	FD_SET(0, &fds);
	
	if (select(1, &fds, NULL, NULL, &tv))
	{
		char c;
		read(0, &c, sizeof(c));
		pressed = c;
	}
	
#else
	if (kbhit())
		pressed = getch();
	
#endif
	
	// Translate from ascii to internals
	switch (pressed)
	{
		case 'a':	pressed = LEFT;		break;
		case 'd':	pressed = RIGHT;	break;
		case ' ':	pressed = FIRE;		break;
		case 'q':	pressed = QUIT;		break;
		default:	pressed = 0;		break;
	}
	
	return pressed;
}


// Setup input handling
void BeginListening()
{
#if LINUX
	struct termios new_termios;
	
	// Save old one
	tcgetattr(0, &orig_termios);
	memcpy(&new_termios, &orig_termios, sizeof(new_termios));
	
#if 1
	cfmakeraw(&new_termios);
#else
	new_termios.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP
							| INLCR | IGNCR | ICRNL | IXON);
	new_termios.c_oflag &= ~OPOST;
	new_termios.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
	new_termios.c_cflag &= ~(CSIZE | PARENB);
	new_termios.c_cflag |= CS8;
#endif
	tcsetattr(0, TCSANOW, &new_termios);
#endif
}


// Tear down input handling
void StopListening()
{
#if LINUX
	tcsetattr(0, TCSANOW, &orig_termios);
#endif
}


