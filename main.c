// Platform checks
#define LINUX (defined(__linux__) && __linux__)


// Includes
#define _POSIX_C_SOURCE 199309L	// HACK
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#if LINUX
	#include <sys/select.h>
	#include <unistd.h>
	#include <termios.h>
	extern void cfmakeraw (struct termios *);	// HACK
#else
	#include <conio.h>
#endif


// Defines
#define WIDTH 20
#define HEIGHT 15
#define PLAYER_Y 2

#define LEFT 1
#define RIGHT 2
#define FIRE 3
#define QUIT 4
#define DIED 5


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
	
	kWhiteFront = (kRedFront | kGreenFront | kBlueFront),
	kBlackFront = kBlack,
	kWhiteBack = (kRedBack | kGreenBack | kBlueBack),
	kBlackBack = kBlack,
	kColourDefault = kWhiteFront | kBlackBack,
	
	// 8 bits for metadata
	kMetaMask = (1 << 8) - 1,
	kMetaShift = 8,
};


// Statics
Tile grid[HEIGHT][WIDTH];
unsigned char playerPos;
long baseTick;
int currentTick;

#if LINUX
struct termios orig_termios;	// taken from http://stackoverflow.com/a/448982
#endif


// Forward declares
Type GetType(Tile);
Colour GetColour(Tile);
MetaData GetMeta(Tile);
Tile CreateTile(Type, Colour, MetaData);
void PrintChar(char ch, Colour col);
void Init(void);
void Shutdown(void);
void Draw(void);
int Update(int*);
int GetKeyPressed(void);
void BeginListening(void);
void StopListening(void);
int AdvanceTick(void);


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
	static Colour currentColour = kColourDefault << kColourShift;
	
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
	const Tile borderTile = CreateTile(kBarrier, kColourDefault, 255);	// 255 health so it shouldn't get broken
	
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
	playerPos = WIDTH / 2;
	grid[PLAYER_Y][playerPos] = CreateTile(kShip, kColourDefault, 5);
	
	// Setup the base time
#if LINUX
	struct timespec spec;
	clock_gettime(CLOCK_MONOTONIC, &spec);
	baseTick = (spec.tv_nsec / 1000000ULL) + (spec.tv_sec * 1000);
#endif
	
	// Advance a tick to setup the time keeping
	AdvanceTick();
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
	
	// Handle button state
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
			
		case FIRE:
			// TODO: check we haven't just fired
			grid[PLAYER_Y+1][playerPos] = CreateTile(kBullet, kColourDefault, 0);
			break;
			
		case QUIT:
			// We're done
			*finished = 1;
			changedState = 0;
			break;
	}
	
	// Update the objects
	int delta = AdvanceTick();
	if (delta && !*finished)
	{
		// TODO: not this!
		for (int y=HEIGHT-2; y>PLAYER_Y; y--)
		{
			for (int x=1; x<WIDTH-1; x++)
			{
				Tile sq = grid[y][x];
				switch (GetType(sq))
				{
					case kEmpty:
					case kBarrier:
						// Nothing to do
						break;
						
					case kShip:
						// TODO: move it down
						break;
						
					case kBullet:
					{
						// Move the bullet up every 20 ticks
						MetaData ticks = GetMeta(sq) + delta;
						if (ticks >= 20)
						{
							Tile above = grid[y+1][x];
							switch (GetType(above))
							{
								case kEmpty:
									grid[y+1][x] = CreateTile(kBullet, GetColour(sq), ticks % 20);
									grid[y][x] = 0;
									break;
									
								case kBarrier:
									// Ignore the ceiling
									if (y == HEIGHT-2)
									{
										grid[y][x] = 0;
										break;
									}
									// fallthrough
								case kShip:
									// TODO: damage
									break;
									
								case kBullet:
									// This shouldn't happen, so just ignore it and let it queue?
									break;
							}
							
							// The state changed
							changedState = 1;
						}
						else
						{
							// Update the ticks
							grid[y][x] = CreateTile(kBullet, GetColour(sq), ticks);
						}
					}
					break;
				}
			}
		}
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
	
	// TODO: just save this?
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


// Helper to create a tile
Tile CreateTile(Type ty, Colour col, MetaData meta)
{
	return (ty << kTypeShift) | (col << kColourShift) | (meta << kMetaShift);
}


// Advance the current tick
int AdvanceTick()
{
	int oldTime = currentTick;
	
#if LINUX
	struct timespec spec;
	clock_gettime(CLOCK_MONOTONIC, &spec);
	currentTick = (spec.tv_nsec / 1000000ULL) + (spec.tv_sec * 1000) - baseTick;
	
#else
ERROR TODO
#endif
	
	// We only deal with 0.01s per tick
	currentTick /= 10;
	
	return currentTick - oldTime;
}



