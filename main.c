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
	#include <Windows.h>
	#include <conio.h>
#endif


// Defines
#define WIDTH 22
#define HEIGHT 16
#define PLAYER_Y 1

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
	
	// 7 bits for metadata
	kMetaMask = (1 << 7) - 1,
	kMetaShift = 8,
	
	// Final bit indicates player ownership
	kOwnedShift = 15,
	kOwned = 1,
};


// Statics
static Tile grid[HEIGHT][WIDTH];
static unsigned char playerPos;
static long baseTick;
static int currentTick;
static Colour currentColour = kColourDefault << kColourShift;

#if LINUX
static struct termios orig_termios;	// taken from http://stackoverflow.com/a/448982
#else
static HANDLE consoleHandle;
#endif


// Forward declares
static Type GetType(Tile);
static Colour GetColour(Tile);
static MetaData GetMeta(Tile);
static MetaData GetOwned(Tile);
static Tile CreateTile(Type, Colour, MetaData, MetaData);
static void PrintChar(char ch, Colour col);
static void Init(void);
static void Shutdown(void);
static void Draw(void);
static int Update(int*);
static int GetKeyPressed(void);
static void BeginListening(void);
static void StopListening(void);
static int AdvanceTick(void);


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
					c = (y > PLAYER_Y) ? 'v' : '^';
					break;
					
				case kBullet:
					// Make it look like it's moving up/down
					if ((meta >= 10) ^ GetOwned(sq))
						c = '.';
					else
						c = '\'';
					break;
			}
			
			// Print it
			PrintChar(c, col);
		}
		
		// Newline shouldn't have a colour
		PrintChar('\n', kColourDefault);
	}
	
	// Start listening again
	BeginListening();
}


// Get the type of a tile
static Type GetType(Tile sq)
{
	return (sq >> kTypeShift) & kTypeMask;
}


// Get the colour of a tile
static Colour GetColour(Tile sq)
{
	return (sq >> kColourShift) & kColourMask;
}


// Get the metadata of a tile
static MetaData GetMeta(Tile sq)
{
	return (sq >> kMetaShift) & kMetaMask;
}


// See if the tile is owned by the player
static MetaData GetOwned(Tile sq)
{
	return (sq >> kOwnedShift) & kOwned;
}


// Print a character with a set colour
static void PrintChar(char ch, Colour col)
{
	// Don't update more often than we need
	if (currentColour != col)
	{
#if LINUX
		// These map nicely on Linux
		
		Colour newFront = col & kWhiteFront;
		Colour oldFront = currentColour & kWhiteFront;
		if (newFront != oldFront)
		{
			printf("\e[38;5;%im", (int)newFront);
		}
		
		Colour newBack = col & kWhiteBack;
		Colour oldBack = currentColour & kWhiteBack;
		if (newBack != oldBack)
		{
			printf("\e[48;5;%im", (int)(newBack>>3));
		}
		
#else
		WORD colour = 0;
		
		if (col & kRedFront)	colour |= FOREGROUND_RED;
		if (col & kGreenFront)	colour |= FOREGROUND_GREEN;
		if (col & kBlueFront)	colour |= FOREGROUND_BLUE;
		if (col & kRedBack)		colour |= BACKGROUND_RED;
		if (col & kGreenBack)	colour |= BACKGROUND_GREEN;
		if (col & kBlueBack)	colour |= BACKGROUND_BLUE;
		
		SetConsoleTextAttribute(consoleHandle, colour);
#endif
		
		currentColour = col;
	}
	
	// Print it
	putchar(ch);
}


// Initialise everything
static void Init()
{
	// Start listening for input
	BeginListening();
	
	// Add a basic border
	const Tile borderTile = CreateTile(kBarrier, kWhiteFront | kWhiteBack, 255, 0);	// 255 health so it shouldn't get broken
	
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
	grid[PLAYER_Y][playerPos] = CreateTile(kShip, kGreenFront, 0, kOwned);
	
	// Populate the rest of the grid
	for (int y=HEIGHT-3; y>=HEIGHT/2; y-=2)
	{
		for (int x=3; x<WIDTH-3; x+=3)
		{
			grid[y][x] = CreateTile(kShip, kBlueFront, 0, 0);
		}
	}
	
	// Setup the base time
#if LINUX
	struct timespec spec;
	clock_gettime(CLOCK_MONOTONIC, &spec);
	baseTick = (spec.tv_nsec / 1000000ULL) + (spec.tv_sec * 1000);
	
#else
	SYSTEMTIME st;
	GetSystemTime(&st);
	currentTick = st.wMilliseconds + st.wSecond - baseTick;
	
#endif
	
	// Grab the handle to the console on windows
#if !LINUX
	consoleHandle = GetStdHandle(STD_OUTPUT_HANDLE);
#endif
	
	// Advance a tick to setup the time keeping
	AdvanceTick();
}


// Shutdown everything
static void Shutdown()
{
	StopListening();
}


// Check for input and tick the game code
static int Update(int* finished)
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
			if (GetType(grid[PLAYER_Y+1][playerPos]) != kBullet)
			{
				grid[PLAYER_Y+1][playerPos] = CreateTile(kBullet, kRedFront, 0, kOwned);
			}
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
				MetaData owned = GetOwned(sq);
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
							int nextY = owned ? (y+1) : (y-1);
							Tile next = grid[nextY][x];
							switch (GetType(next))
							{
								case kEmpty:
									grid[nextY][x] = CreateTile(kBullet, GetColour(sq), ticks % 20, owned);
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
								{
									MetaData health = GetMeta(next);
									grid[nextY][x] = (health == 0) ? 0 : CreateTile(GetType(next), GetColour(next), health-1, owned);
									grid[y][x] = 0;
								}
								break;
									
								case kBullet:
									if (owned != GetOwned(next))
									{
										// Bullets cancel out
										grid[y][x] = 0;
										grid[nextY][x] = 0;
									}
									else
									{
										// This shouldn't happen, so just ignore it and let it queue?
									}
									break;
							}
							
							// The state changed
							changedState = 1;
						}
						else
						{
							// Update the ticks
							grid[y][x] = CreateTile(kBullet, GetColour(sq), ticks, owned);
							
							// See if we need to redraw
							if (ticks >= 10 && ticks - delta < 10)
								changedState = 1;
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
static int GetKeyPressed()
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
static void BeginListening()
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
static void StopListening()
{
#if LINUX
	tcsetattr(0, TCSANOW, &orig_termios);
#endif
}


// Helper to create a tile
static Tile CreateTile(Type ty, Colour col, MetaData meta, MetaData owned)
{
	return (ty << kTypeShift) | (col << kColourShift) | (meta << kMetaShift) | (owned << kOwnedShift);
}


// Advance the current tick
static int AdvanceTick()
{
	int oldTime = currentTick;
	
#if LINUX
	struct timespec spec;
	clock_gettime(CLOCK_MONOTONIC, &spec);
	currentTick = (spec.tv_nsec / 1000000ULL) + (spec.tv_sec * 1000) - baseTick;
	
#else
	SYSTEMTIME st;
	GetSystemTime(&st);
	currentTick = st.wMilliseconds + st.wSecond - baseTick;
	
#endif
	
	// We only deal with 0.01s per tick
	currentTick /= 10;
	
	return currentTick - oldTime;
}



