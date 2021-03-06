/* TEAM MEMBERS: HARITIMA MANCHANDA, BECKY REN
 * DESCRIPTION: PONG GAME
 * CISC-210(HONORS)
 * DESCRIPTION: It implements a simple pong game and if the ball reaches the end of acreen, it sends
 * the location of the ball to the other player and the other player picks up the game from ther.
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sense/sense.h>
#include <linux/input.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#define SPEED 1 //Speed with which the paddle moves
#define MAX 5  //Size of buffers in client or server

/*run: keeps account of whether the game is running
 * runJoyStick: Keeps account of whether joystick is pressed
 * startingPaddleIndex: starting Index of the Paddle
 * serverRunning: keeps account of whether server is running
 * clientRunning: keeps account of whether client is running
 */
int run = 1; 
int scorePlayer = 0;
int runJoyStick = 0;
int startingPaddleIndex = 0;
int serverRunning = 0;
int clientRunning = 0;


pi_framebuffer_t *fb;


int ballXVel;
int ballYVel;

//structure defines coordinates of the ball
typedef struct
{
	int ballx;
	int bally;
	int ballxprev;
	int ballyprev;
}gamestate_t;


int initializeGameSetUp(gamestate_t *state);
void initGame(gamestate_t *state);

// Exits the program on ctrl C
void handler(int sig)
{
	printf("\nEXITING...\n");
	run = 0;
}

//exits the program after displaying the error msg
void error(char *msg)
{
	perror(msg);
	exit(1);
}

//Creates socket for either client or server, whichever is called
int socketCreate()
{
	int server_socket;

	server_socket = socket(AF_INET, SOCK_STREAM, 0);
	return server_socket;
}

//Binds if the program works as server
int bindCreatedSocket(int server_socket, int portno)
{
	int n = -1;
	struct sockaddr_in serv_addr;

	bzero((char*)&serv_addr, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	serv_addr.sin_port = htons(portno);

	n = bind(server_socket, (struct sockaddr*)&serv_addr, sizeof(serv_addr));
	return n;
}

//Detects collision with paddle
int collision(int paddle_xpos, int ball_xpos)
{
 
	if(ball_xpos >= paddle_xpos && ball_xpos <= (paddle_xpos + 2))
	{
		scorePlayer++;
		return 1;
	}
	else
	{
		return 0;
	}
}

//Draws the paddle
void drawPaddle(sense_fb_bitmap_t *screen, int startingPaddleIndex, uint16_t color)
{
	clearBitmap(fb->bitmap, getColor(0,0,0));

	int i = startingPaddleIndex;
	int count = 0; // Number of paddle dots 

	while(i < 8 && i >= 0 && count < 3)
	{
		setPixel(screen, i, 0, color);
		i++;
		count++;
	}
}

//Function to generate random movement of the ball
int generate_random()
{
	return (rand() % 3) - 3;
}

//draws the ball
void drawBall(sense_fb_bitmap_t *screen, gamestate_t *state, uint16_t color)
{     
	setPixel(screen, state->ballxprev, state->ballyprev,0);
	setPixel(screen, state->ballx, state->bally, color);
}

//moves the ball
//returns the x coordinate of the ball when it reaches the end of screen, otherwise returns 1
int  moveBall(sense_fb_bitmap_t *screen, gamestate_t *state, int paddle_x)
{

	int x = state->ballx;
	int y = state->bally;

	state->ballxprev = state->ballx;
	state->ballyprev = state->bally;

	if(x >= 7)
	{
		ballXVel = -1;
	}

	if(x <= 0)
	{
		ballXVel = 1;
	}

	if(y >= 7 && ballYVel == 1)
	{
		drawPaddle(screen, paddle_x, getColor(0, 0, 255));
		return x;		    
	}

	else if(y >=7 && ballYVel == 0)
	{
		ballYVel = -1;
		state->ballx += generate_random();
	}

	else if(y <= 1 && collision(paddle_x,x))
	{
		ballYVel = 1;
		state->ballx += generate_random();
	}

	else if(y <= 1 && (collision(paddle_x,x) == 0))
	{
		run = 0; // If the player misses the ball, the game ends
	}

	state->ballx += ballXVel;
	state->bally += ballYVel;

	usleep(300000); // Used to reduce the speed of the ball

	return -1;
}

//Called if the program runs as server
int  runAsServer(sense_fb_bitmap_t *screen, int portno, gamestate_t *state, int paddle_x)
{
	int sockfd, newsockfd, clilen;
	char  buffer[MAX];

	struct sockaddr_in serv_addr, cli_addr;
	int n;

	sockfd = socketCreate();
	if(sockfd < 0)
	{
		error("Error in Creating Socket");
	}

	if(bindCreatedSocket(sockfd,portno) < 0)
	{
		error("Error Bind Failed");
	}
	
	
	listen(sockfd, 10);
	clilen = sizeof(cli_addr);

	newsockfd = accept(sockfd, (struct sockaddr*)&cli_addr, &clilen);

		if(newsockfd < 0)
		{
			error("Error in Accepting");
		}


	while(run)
	{
		
		bzero(buffer, MAX);

		while(run)
		{

			n = recv(newsockfd, buffer, MAX, MSG_DONTWAIT);	

			if(n ==  0)
			{
				printf("CLIENT DIED. ENDING\n");
				return 1;
			}
			if(n > 0)
			{
				break;
			}
	
		}
		printf("SCORE PLAYER 2:%d\n ", buffer[2]);//prints the score received from the other side
		state->ballx = (int)(buffer[0]);
		state->bally = (int)(buffer[1]);
	
		ballYVel = 0;
		int data = initializeGameSetUp(state);
		
		if(data > -1)
		{
			buffer[0] = data;	
			buffer[1] = 7;		
			buffer[2] = scorePlayer;

			n = send(newsockfd, buffer,3, 0);
			if(n < 0)
			{
				error("Error writing to socket");
			}

		}
	}

	close(newsockfd);
	close(sockfd);
	
	return 1;
}

//runs if the program works as client
int runAsClient(sense_fb_bitmap_t *screen, int portno, char* IP_addr, gamestate_t *state, int paddle_x)
{
	int sockfd, n;
	struct sockaddr_in serv_addr;
	struct hostent *server;

	char buffer[MAX];
	char newBuffer[MAX];

	sockfd = socketCreate();

	if(sockfd<0)
	{
		error("Error opening socket");
	}

	server = gethostbyname(IP_addr);
	
	if(server == NULL)
	{
		fprintf(stderr, "ERROR, no such host\n");
		exit(0);
	}

	bzero((char*)&serv_addr,sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;

	bcopy((char*)server->h_addr,(char*)&serv_addr.sin_addr.s_addr,server->h_length);
	serv_addr.sin_port = htons(portno);

	if(connect(sockfd,(struct sockaddr *)&serv_addr,sizeof(serv_addr))<0)
	{
		error("ERROR connecting");
	}

	initGame(state);
	
	int data = initializeGameSetUp(state);

	while(run){
	if(data > -1)
	{
	
		buffer[0] = data;	
		buffer[1] = 7; 		
		buffer[2] = scorePlayer;			

		n = send(sockfd,buffer,strlen(buffer),0);
		if(n<0)
		{
			error("ERROR writing to socket");
		}

		bzero(buffer,MAX);

		while(run)
		{
			n = recv(sockfd, newBuffer ,sizeof(newBuffer),MSG_DONTWAIT);	
			if(n > 0 )
			{

				break;
			}
		}
		printf("SCORE PLAYER 1: %d\n", newBuffer[2]);//Prints the score recieved
		state->ballx = (int)newBuffer[0];
		state->bally = (int)newBuffer[1];
		
		ballYVel = 0;
		data = initializeGameSetUp(state);	
	
	}

	}

	close(sockfd);

	return 1;
}

//Function called when joystick is pressed. Updates runJoyStick
void callbackFn(unsigned int code)
{
	switch(code)
	{
		case KEY_RIGHT:
			runJoyStick = 1;
			break;
		case KEY_LEFT:
			runJoyStick = -1;
			break;
		default:
			runJoyStick = 0;
			break;
	}
}

//Sets up Initial Position of the ball
void initGame(gamestate_t *game)
{
	game->ballx = game->bally = game->ballxprev = game->ballyprev = 1;
}


//moves the paddle
int  movePaddle(sense_fb_bitmap_t *screen, int direction)
{
	int paddleX = 0;
	if(direction == 1)
	{
		paddleX += 3 * SPEED * direction;	
	}
	else if(direction == -1)
	{
		paddleX += 3 * SPEED * direction;
	}
	return paddleX;
}


int main(int argc, char* argv[])
{
	int portno, i;

	gamestate_t game;
	fb = getFBDevice();
	if(!fb)
	{
		return 0;
	}
	
	if(argc == 2)
	{
		printf("\n Initializing as Server...");
		portno = atoi(argv[1]);
		serverRunning = runAsServer(fb->bitmap, portno, &game,startingPaddleIndex);
	}
	
	else if(argc == 3)
	{
		printf("\n Initializing as client...");
		portno = atoi(argv[1]);
		clientRunning = runAsClient(fb->bitmap,portno, argv[2], &game, startingPaddleIndex);
	}
	else
	{
		error("\nWrong number of arguments provided...");
	}

	return 0;
}

int initializeGameSetUp(gamestate_t *state)
{
	int paddleSpeed = 0;

	

	pi_i2c_t *device;

	signal(SIGINT, handler);

	pi_joystick_t* joystick = getJoystickDevice();

	clearBitmap(fb->bitmap, 0);

	drawPaddle(fb->bitmap,startingPaddleIndex, getColor(0,0,255));
	drawBall(fb->bitmap, state, getColor(255,0,0));

	device = geti2cDevice();

	if(device)
	{
		while(run)
		{
			usleep(2000);

			while(run)
			{
				drawBall(fb->bitmap,state, getColor(255,0,0));

				int data = moveBall(fb->bitmap, state, startingPaddleIndex);
				if(data > -1)
				{
					return data;
				}

				pollJoystick(joystick, callbackFn, 0);
				if(runJoyStick == 1 || runJoyStick == -1)
				{
					startingPaddleIndex +=  movePaddle(fb->bitmap, runJoyStick);//If Joystick is pressed paddle is moved
					drawPaddle(fb->bitmap, startingPaddleIndex, getColor(0,0,255));
					runJoyStick = 0;
				}

			}
		}
		freei2cDevice(device);
	}
	
	clearBitmap(fb->bitmap, 0);
	freeFrameBuffer(fb);
	freeJoystick(joystick);
	return 0;

}
