#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include <SDL2/SDL_scancode.h>

#include "FreeRTOS.h"
#include "queue.h"
#include "semphr.h"
#include "task.h"
#include "timers.h"

#include "TUM_Ball.h"
#include "TUM_Draw.h"
#include "TUM_Font.h"
#include "TUM_Event.h"
#include "TUM_Sound.h"
#include "TUM_Utils.h"
#include "TUM_FreeRTOS_Utils.h"
#include "TUM_Print.h"

#include "AsyncIO.h"

#define mainGENERIC_PRIORITY (tskIDLE_PRIORITY)
#define mainGENERIC_STACK_SIZE ((unsigned short)2560)

#define STATE_QUEUE_LENGTH 1

#define STATE_COUNT 3

#define STATE_ONE 0
#define STATE_TWO 1
#define STATE_Three 2
#define NEXT_TASK_1 0
#define NEXT_TASK_2 1 
#define PREV_TASK   2

#define STARTING_STATE STATE_ONE

#define STATE_DEBOUNCE_DELAY 300

#define KEYCODE(CHAR) SDL_SCANCODE_##CHAR
#define CAVE_SIZE_X SCREEN_WIDTH / 2
#define CAVE_SIZE_Y SCREEN_HEIGHT / 2
#define RADIOS 50
#define CAVE_X CAVE_SIZE_X / 2
#define CAVE_Y CAVE_SIZE_Y / 2
#define CAVE_THICKNESS 25
#define LOGO_FILENAME "freertos.jpg"
#define UDP_BUFFER_SIZE 2000
#define UDP_TEST_PORT_1 1234
#define UDP_TEST_PORT_2 4321
#define UDP_TEST_PORT_3 5678
#define MSG_QUEUE_BUFFER_SIZE 1000
#define MSG_QUEUE_MAX_MSG_COUNT 10
#define TCP_BUFFER_SIZE 2000
#define TCP_TEST_PORT 2222
// TimerHandle_t myTimer=NULL ;
TaskHandle_t HandleCircleOne = NULL;
TaskHandle_t HandleCircleTwo = NULL;
TaskHandle_t Time=NULL;
SDL_Window *window;
StaticTask_t xTaskBuffer;
StackType_t xStack[mainGENERIC_STACK_SIZE];

QueueHandle_t FlagQueueR = NULL;
QueueHandle_t FlagQueueL = NULL;

TaskHandle_t ButtonK =NULL;
TaskHandle_t ButtonS =NULL;

static QueueHandle_t StateQueue = NULL;
static SemaphoreHandle_t DrawSignal = NULL;
static SemaphoreHandle_t ScreenLock = NULL;

static SemaphoreHandle_t mySyncSignal = NULL;
static SemaphoreHandle_t mySyncSignalTask = NULL;

#ifdef TRACE_FUNCTIONS
#include "tracer.h"
#endif
//pointer of triagle
static coord_t arr[3]={ 
			            {300,200},
                        {350,300},
                        {250,300}
                            };

static char *mq_one_name = "FreeRTOS_MQ_one_1";
static char *mq_two_name = "FreeRTOS_MQ_two_1";
static char *mq_three_name = "FreeRTOS_MQ_three_1";
aIO_handle_t mq_one = NULL;
aIO_handle_t mq_two = NULL;
aIO_handle_t mq_three = NULL;
aIO_handle_t udp_soc_one = NULL;
aIO_handle_t udp_soc_two = NULL;
aIO_handle_t udp_soc_three = NULL;
aIO_handle_t tcp_soc = NULL;

const unsigned char next_state_signal_1 = NEXT_TASK_1;
const unsigned char next_state_signal_2 = NEXT_TASK_2;
const unsigned char prev_state_signal = PREV_TASK;

static TaskHandle_t StateMachine = NULL;
static TaskHandle_t BufferSwap = NULL;
static TaskHandle_t DemoTask1 = NULL;
static TaskHandle_t DemoTask2 = NULL;
static TaskHandle_t DemoTask3 = NULL;
static TaskHandle_t UDPDemoTask = NULL;
static TaskHandle_t TCPDemoTask = NULL;
static TaskHandle_t MQDemoTask = NULL;
static TaskHandle_t DemoSendTask = NULL;

static image_handle_t logo_image = NULL;

typedef struct buttons_buffer {
    unsigned char buttons[SDL_NUM_SCANCODES];
    SemaphoreHandle_t lock;
} buttons_buffer_t;

static buttons_buffer_t buttons = { 0 };

void checkDraw(unsigned char status, const char *msg)
{
    if (status) {
        if (msg)
            fprints(stderr, "[ERROR] %s, %s\n", msg,
                    tumGetErrorMessage());
        else {
            fprints(stderr, "[ERROR] %s\n", tumGetErrorMessage());
        }
    }
}

/*
 * Changes the state, either forwards of backwards
 */
void changeState(volatile unsigned char *state, unsigned char forwards)
{
    switch (forwards) {
        case NEXT_TASK_1:
            if (*state == STATE_COUNT - 1) {
                *state = 0;
            }
            else {
                (*state)++;
            }
            break;
        case NEXT_TASK_2:
            if (*state == STATE_COUNT - 2) {
                *state = 1;
            }
            else {
                (*state)++;
            }
            break;

        case PREV_TASK:
            if (*state == 0) {
                *state = STATE_COUNT - 1;
            }
            else {
                (*state)--;
            }
            break;
        default:
            break;
    }
}

/*
 * Example basic state machine with sequential states
 */
void basicSequentialStateMachine(void *pvParameters)
{
    unsigned char current_state = STARTING_STATE; // Default state
    unsigned char state_changed =
        1; // Only re-evaluate state if it has changed
    unsigned char input = 0;

    const int state_change_period = STATE_DEBOUNCE_DELAY;

    TickType_t last_change = xTaskGetTickCount();

    while (1) {
        if (state_changed) {
            goto initial_state;
        }

        // Handle state machine input
        if (StateQueue)
            if (xQueueReceive(StateQueue, &input, portMAX_DELAY) ==
                pdTRUE)
                if (xTaskGetTickCount() - last_change >
                    state_change_period) {
                    changeState(&current_state, input);
                    state_changed = 1;
                    last_change = xTaskGetTickCount();
                }

initial_state:
        // Handle current state
        if (state_changed) {
            switch (current_state) {
                case STATE_ONE:
                    if (DemoTask2) {
                        vTaskSuspend(DemoTask2);
                    }
                    if (DemoTask3) {
                        vTaskSuspend(DemoTask3);
                    }
                    if (DemoTask1) {
                        vTaskResume(DemoTask1);
                    }
                    
                    break;
                case STATE_TWO:
                    if (DemoTask3) {
                        vTaskSuspend(DemoTask3);
                    }
                    if (DemoTask1) {
                        vTaskSuspend(DemoTask1);

                    if (DemoTask2) {
                        vTaskResume(DemoTask2);
                    }
                    break;
                    
                    }
                case STATE_Three:
                    if (DemoTask1) {
                        vTaskSuspend(DemoTask1);
                    } 
                    if (DemoTask2) {
                        vTaskSuspend(DemoTask2);
                    }
                    if (DemoTask3) {
                        vTaskResume(DemoTask3);
                    }
                    break;

                   
                default:
                    break;
            }
            state_changed = 0;
        }
    }
}

void vSwapBuffers(void *pvParameters)
{
    TickType_t xLastWakeTime;
    xLastWakeTime = xTaskGetTickCount();
    const TickType_t frameratePeriod = 20;

    tumDrawBindThread(); // Setup Rendering handle with correct GL context

    while (1) {
        if (xSemaphoreTake(ScreenLock, portMAX_DELAY) == pdTRUE) {
            tumDrawUpdateScreen();
            tumEventFetchEvents(FETCH_EVENT_BLOCK);
            xSemaphoreGive(ScreenLock);
            xSemaphoreGive(DrawSignal);
            vTaskDelayUntil(&xLastWakeTime,
                            pdMS_TO_TICKS(frameratePeriod));
        }
    }
}

void xGetButtonInput(void)
{
   if (xSemaphoreTake(buttons.lock, 0) == pdTRUE) {
        xQueueReceive(buttonInputQueue, &buttons.buttons, 0);
        xSemaphoreGive(buttons.lock);
    }
}
//Display
void vDrawTriangle(void)
{ 
	//display triangle
	 
        coord_t*ptr= arr;

        checkDraw(tumDrawTriangle(ptr,
				                Red),
		            __FUNCTION__);       
            

}

void vDrawMoveCirele(int x,int y)
{   checkDraw(tumDrawCircle(  x,
                              y,
                              RADIOS,
                              TUMBlue),
                 __FUNCTION__);
}

void vDrawMoveCirelSquare(int x,int y){
    checkDraw(tumDrawFilledBox(x,
                              y,  
                            CAVE_SIZE_X/4, 
                            CAVE_SIZE_X/4,
                               Aqua),
               __FUNCTION__);

}

void vDrawCireleBlink(int x,int y,int count)
{  if(count%2!=0)
     checkDraw(tumDrawCircle(  x,
                              y,
                              RADIOS/2,
                              TUMBlue),
                 __FUNCTION__);
    else 
    checkDraw(tumDrawCircle(  x,
                              y,
                              RADIOS/2,
                              White),
                 __FUNCTION__);
   
}
int CircleBlink1=0;
void vDrawCircleBlink1Hz(void * pvParameters)
{
      int positionX=100, positionY=100,Frequence=500;
 
    
        for (;;)
        {
        checkDraw(tumDrawCircle( positionX,
                              positionY,
                              RADIOS,
                              Black),
                 __FUNCTION__);
         

        vTaskDelay((TickType_t)Frequence);
        
        checkDraw(tumDrawCircle( positionX,
                              positionY,
                              RADIOS,
                              White),
                 __FUNCTION__);
        vTaskDelay((TickType_t)Frequence);
    
    }
    //vTaskDelete(HandleCircleOne) ;
}

int CircleBlink2=0;
void vDrawCircleBlink2Hz(void * pvParameters)
{
    
    int positionX=250, positionY=100,Frequence=250;
    
    
       for(;;){
        checkDraw(tumDrawCircle( positionX,
                              positionY,
                              RADIOS,
                              Black),
                 __FUNCTION__);
               

        vTaskDelay((TickType_t)Frequence );

          
        checkDraw(tumDrawCircle( positionX,
                              positionY,
                              RADIOS,
                              White),
                 __FUNCTION__);
        vTaskDelay((TickType_t)Frequence);
       } 
     //  vTaskDelete(HandleCircleTwo);
    

}

//count store
char btnA = 0, btnB = 0, btnC = 0, btnD = 0;

void ButtonCountRest(unsigned char Reset)
{
     static char str[100] = { 0 };
    

    if (Reset){
     btnA = 0; btnB = 0; btnC = 0; btnD = 0;
     sprintf(str, "A: %d | B: %d | C: %d | D: %d",
                        btnA ,btnB,btnC,btnD);
        
        checkDraw(tumDrawText(str, 10, DEFAULT_FONT_SIZE * 5, Black),
                  __FUNCTION__);
    }
    
}

void vDrawHelpText(void)
{
    static char str1[100] = { 0 };
  
    static int text_width;
    ssize_t prev_font_size = tumFontGetCurFontSize();

    tumFontSetSize((ssize_t)30);

    sprintf(str1, "Right");
   

    if (!tumGetTextSize((char *)str1, &text_width, NULL))
        checkDraw(tumDrawText((char* )str1, (620-text_width),
                                400,
                             Black),
                  __FUNCTION__);
    
      tumFontSetSize(prev_font_size);
}

void vDrawHelpTextMove(int i)
{
    static char str1[100] = { 0 };
    
    static int text_width;
    ssize_t prev_font_size = tumFontGetCurFontSize();

    tumFontSetSize((ssize_t)30);

    sprintf(str1, "Left");

    if (!tumGetTextSize((char *)str1, &text_width, NULL))
           
    checkDraw(tumDrawText((char *)str1,
                            60+i,
                            DEFAULT_FONT_SIZE * 0.5, 
                            Black),
                  __FUNCTION__);
    tumFontSetSize(prev_font_size);
}
#define FPS_AVERAGE_COUNT 50
#define FPS_FONT "IBMPlexSans-Bold.ttf"

void vDrawFPS(void)
{
    static unsigned int periods[FPS_AVERAGE_COUNT] = { 0 };
    static unsigned int periods_total = 0;
    static unsigned int index = 0;
    static unsigned int average_count = 0;
    static TickType_t xLastWakeTime = 0, prevWakeTime = 0;
    static char str[10] = { 0 };
    static int text_width;
    int fps = 0;
    font_handle_t cur_font = tumFontGetCurFontHandle();

    if (average_count < FPS_AVERAGE_COUNT) {
        average_count++;
    }
    else {
        periods_total -= periods[index];
    }

    xLastWakeTime = xTaskGetTickCount();

    if (prevWakeTime != xLastWakeTime) {
        periods[index] =
            configTICK_RATE_HZ / (xLastWakeTime - prevWakeTime);
        prevWakeTime = xLastWakeTime;
    }
    else {
        periods[index] = 0;
    }

    periods_total += periods[index];

    if (index == (FPS_AVERAGE_COUNT - 1)) {
        index = 0;
    }
    else {
        index++;
    }

    fps = periods_total / average_count;

    tumFontSelectFontFromName(FPS_FONT);
    
    sprintf(str, "FPS: %2d", fps);

    if (!tumGetTextSize((char *)str, &text_width, NULL))
        checkDraw(tumDrawText(str, SCREEN_WIDTH - text_width - 10,
                              SCREEN_HEIGHT - DEFAULT_FONT_SIZE*3,
                              TUMBlue),
                  __FUNCTION__);
tumFontSetSize((ssize_t)18);
    tumFontSelectFontFromHandle(cur_font);
    tumFontPutFontHandle(cur_font);
}

void vDrawStaticItems(void)
{
    vDrawHelpText();
    vDrawTriangle();
    
     // vDrawLogo();
}



void vDrawButtonText(void)
{
    static char str[100] = { 0 };
   
      tumFontSetSize((ssize_t)18);
    //Mouse value
    sprintf(str, "Axis 1: %5d | Axis 2: %5d", tumEventGetMouseX(),
           tumEventGetMouseY());

checkDraw(tumDrawText(str, 10, DEFAULT_FONT_SIZE * 7, Black),
              __FUNCTION__);

    //Button count
    if (xSemaphoreTake(buttons.lock, 0) == pdTRUE) {
        if (buttons.buttons[KEYCODE(A)]) {
            buttons.buttons[KEYCODE(A)] = 0;
            btnA+=1;}
        else if (buttons.buttons[KEYCODE(B)]) {
            buttons.buttons[KEYCODE(B)] = 0;
            btnB+=1;}

        else if (buttons.buttons[KEYCODE(C)]) {
            buttons.buttons[KEYCODE(C)] = 0;
            btnC+=1;}

        else if (buttons.buttons[KEYCODE(D)]) {
            buttons.buttons[KEYCODE(D)] = 0;
            btnD+=1;}
    
        sprintf(str, "A: %d | B: %d | C: %d | D: %d",
                        btnA ,btnB,btnC,btnD);
        xSemaphoreGive(buttons.lock);
        checkDraw(tumDrawText(str, 10, DEFAULT_FONT_SIZE * 5, Black),
                  __FUNCTION__);
    }

    
}


static int vCheckStateInput(void)
{
    if (xSemaphoreTake(buttons.lock, 0) == pdTRUE) {
        if (buttons.buttons[KEYCODE(E)]) {
           buttons.buttons[KEYCODE(E)] = 0;
            if (StateQueue) {
                xSemaphoreGive(buttons.lock);
                xQueueSend(StateQueue, &next_state_signal_1, 0);
                return 0;
            }
            return -1;
        }
        xSemaphoreGive(buttons.lock);
    }

    return 0;
}
int Status=0;
int kt=0;
int st=0;
static void vCheckStateInput_P(void)
{       
    if (xSemaphoreTake(buttons.lock, 0) == pdTRUE) {
        if (buttons.buttons[KEYCODE(P)]) {
           buttons.buttons[KEYCODE(P)] = 0;
            if (Status) {
                vTaskResume(Time);
                Status=0;
            }
            else
            {
                vTaskSuspend(Time);
                Status=1;
            }
        }
        
        if (buttons.buttons[KEYCODE(K)]) 
        {
            kt=1;
         buttons.buttons[KEYCODE(K)]=0;
                    
         xSemaphoreGive(buttons.lock);
         xTaskNotifyGive(ButtonK);
                    }
        if (buttons.buttons[KEYCODE(S)])
        {
         buttons.buttons[KEYCODE(S)]=0;
                st=1; 
        xSemaphoreGive(buttons.lock);
         xSemaphoreGive(mySyncSignal);
                }
        xSemaphoreGive(buttons.lock);
    }
     if (mySyncSignal==NULL)
                {
                  printf(" sync signal null");
     
                }
                
                xSemaphoreGive(buttons.lock);
                xSemaphoreGive(ScreenLock);

                vCheckStateInput();
   
    }



int time=0;
void Timer()
{
const TickType_t xDelay = 1000/portTICK_PERIOD_MS;
     
     while(1)
     {
        time+=1;
        vTaskDelay(xDelay);
     }


}

void TimeText()
{
     static char str1[100] = { 0 };
     static char str2[100]={0};

    
        tumFontSetSize((ssize_t)18);

    sprintf(str1, "TIME: %d", time);
    sprintf(str2,"Press P to Stop");
          
    checkDraw(tumDrawText((char *)str1,
                            10,
                            CAVE_SIZE_Y+210, 
                            Black),
                  __FUNCTION__);
    checkDraw(tumDrawText((char *)str2,
                            10,
                            CAVE_SIZE_Y+190, 
                            Black),
                  __FUNCTION__);              
   
}
void UDPHandlerOne(size_t read_size, char *buffer, void *args)
{
    prints("UDP Recv in first handler: %s\n", buffer);
}

void UDPHandlerTwo(size_t read_size, char *buffer, void *args)
{
    prints("UDP Recv in second handler: %s\n", buffer);
}

void UDPHandlerThree(size_t read_size, char *buffer, void *args)
{
    prints("UDP Recv in third handler: %s\n", buffer);
}

void vUDPDemoTask(void *pvParameters)
{
    char *addr = NULL; // Loopback
    in_port_t port = UDP_TEST_PORT_1;

    udp_soc_one = aIOOpenUDPSocket(addr, port, UDP_BUFFER_SIZE,
                                   UDPHandlerOne, NULL);

    prints("UDP socket opened on port %d\n", port);
    prints("Demo UDP Socket can be tested using\n");
    prints("*** netcat -vv localhost %d -u ***\n", port);

    port = UDP_TEST_PORT_2;

    udp_soc_two = aIOOpenUDPSocket(addr, port, UDP_BUFFER_SIZE,
                                   UDPHandlerTwo, NULL);

    prints("UDP socket opened on port %d\n", port);
    prints("Demo UDP Socket can be tested using\n");
    prints("*** netcat -vv localhost %d -u ***\n", port);

      port = UDP_TEST_PORT_3;

    udp_soc_three = aIOOpenUDPSocket(addr, port, UDP_BUFFER_SIZE,
                                   UDPHandlerThree, NULL);

    prints("UDP socket opened on port %d\n", port);
    prints("Demo UDP Socket can be tested using\n");
    prints("*** netcat -vv localhost %d -u ***\n", port);


    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void MQHandlerOne(size_t read_size, char *buffer, void *args)
{
    prints("MQ Recv in first handler: %s\n", buffer);
}

void MQHanderTwo(size_t read_size, char *buffer, void *args)
{
    prints("MQ Recv in second handler: %s\n", buffer);
}


void MQHanderThree(size_t read_size, char *buffer, void *args)
{
    prints("MQ Recv in third handler: %s\n", buffer);
}

void vDemoSendTask(void *pvParameters)
{
    static char *test_str_1 = "UDP test 1";
    static char *test_str_2 = "UDP test 2";
    static char *test_str_3 = "UDP test 2";
    static char *test_str_4 = "TCP test";

    while (1) {
        prints("*****TICK******\n");
        if (mq_one) {
            aIOMessageQueuePut(mq_one_name, "Hello MQ one");
        }
        if (mq_two) {
            aIOMessageQueuePut(mq_two_name, "Hello MQ two");
        }

        if (mq_three) {
            aIOMessageQueuePut(mq_three_name, "Hello MQ three");
        }
        if (udp_soc_one)
            aIOSocketPut(UDP, NULL, UDP_TEST_PORT_1, test_str_1,
                         strlen(test_str_1));
        if (udp_soc_two)
            aIOSocketPut(UDP, NULL, UDP_TEST_PORT_2, test_str_2,
                         strlen(test_str_2));
        if (udp_soc_three)
            aIOSocketPut(UDP, NULL, UDP_TEST_PORT_3, test_str_3,
                         strlen(test_str_3));

        if (tcp_soc)
            aIOSocketPut(TCP, NULL, TCP_TEST_PORT, test_str_4,
                         strlen(test_str_4));

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void vMQDemoTask(void *pvParameters)
{
    mq_one = aIOOpenMessageQueue(mq_one_name, MSG_QUEUE_MAX_MSG_COUNT,
                                 MSG_QUEUE_BUFFER_SIZE, MQHandlerOne, NULL);
    mq_two = aIOOpenMessageQueue(mq_two_name, MSG_QUEUE_MAX_MSG_COUNT,
                                 MSG_QUEUE_BUFFER_SIZE, MQHanderTwo, NULL);
    mq_three=aIOOpenMessageQueue(mq_three_name, MSG_QUEUE_MAX_MSG_COUNT,
                                 MSG_QUEUE_BUFFER_SIZE, MQHanderThree, NULL);                                

    while (1)

    {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void TCPHandler(size_t read_size, char *buffer, void *args)
{
    prints("TCP Recv: %s\n", buffer);
}

void vTCPDemoTask(void *pvParameters)
{
    char *addr = NULL; // Loopback
    in_port_t port = TCP_TEST_PORT;

    tcp_soc =
        aIOOpenTCPSocket(addr, port, TCP_BUFFER_SIZE, TCPHandler, NULL);

    prints("TCP socket opened on port %d\n", port);
    prints("Demo TCP socket can be tested using\n");
    prints("*** netcat -vv localhost %d ***\n", port);

    while (1) {
        vTaskDelay(10);
    }
}
int k;
int s;
void Button_K(void *pvParameters)
{
    int task_notification;

    while(1){
        if((task_notification = ulTaskNotifyTake(pdTRUE,portMAX_DELAY)))
         {   
            k+=1;
         }    
    }
}


void Button_S(void *pvParameters)
{   
    
    while (1) {
                if (mySyncSignal)
                   if (xSemaphoreTake(mySyncSignal, STATE_DEBOUNCE_DELAY) ==
                  pdTRUE) 
                  {
                            
                   s+=1;
               
               
                }
         }
    }

void DrawButtonText()
{
 static char str1[100] = { 0 };
static char str2[100] = { 0 };
    if(kt==1)
    {
                sprintf(str1, "K: %d", k);  
               checkDraw(tumDrawText((char *)str1,
                            10,
                            CAVE_SIZE_Y+150, 
                            Black),
                  __FUNCTION__);   
    }
    if(st==1)
    {
                sprintf(str2, "S: %d", s);  
               checkDraw(tumDrawText((char *)str2,
                            10,
                            CAVE_SIZE_Y+130, 
                            Black),
                  __FUNCTION__);   

    }
        tumFontSetSize((ssize_t)15);
}

void vDemoTask1(void *pvParameters)
{
    int count=0;
    float phiSquare = acos(-1.0);
    float phiCircle = 0;
    int circleX = 125;
	int circleY = 250;
    int squareX = CAVE_X+250;
    int squareY = CAVE_Y+75;
    CircleBlink1=0;
    CircleBlink2=0;
    int ScreenX=CAVE_SIZE_X ;
    int ScreenY=CAVE_SIZE_Y;
    while (1) {
        if (DrawSignal)
            if (xSemaphoreTake(DrawSignal, portMAX_DELAY) ==
                pdTRUE) {
                tumEventFetchEvents(FETCH_EVENT_BLOCK |
                                    FETCH_EVENT_NO_GL_CHECK);
                xGetButtonInput(); // Update global input

                xSemaphoreTake(ScreenLock, portMAX_DELAY);

                // Clear screen
                checkDraw(tumDrawClear(White), __FUNCTION__);
                vDrawStaticItems();
                ScreenX=tumEventGetMouseX()/2+CAVE_X;
                ScreenY=tumEventGetMouseY()/2+CAVE_Y;

                SDL_SetWindowPosition(window ,ScreenX,ScreenY);
                vDrawMoveCirele(circleX,circleY);
                vDrawMoveCirelSquare(squareX,squareY);
                 //parameter für circle
                 phiCircle += 0.1;
		        circleX = 140 * cos(phiCircle) + 300;
		        circleY = 140 * sin(phiCircle) + 260;
                //parameter für square
                phiSquare += 0.1;
		        squareX = 140 * cos(phiSquare) +250;
		        squareY = 140 * sin(phiSquare) +220;
                 
                count+=10;
                if (count>490) count=490;
                vDrawHelpTextMove(count);
                ButtonCountRest(tumEventGetMouseLeft());
                vDrawButtonText();
              

                xSemaphoreGive(ScreenLock);

                // Get input and check for state change
                vCheckStateInput();
            }
    }
}
void vDemoTask2(void *pvParameters)
{ 
    Status=0;
       //Exercise 3.2.2
    xTaskCreate(vDrawCircleBlink1Hz, "CircleTaskS", mainGENERIC_STACK_SIZE, (void*)1, mainGENERIC_PRIORITY, &HandleCircleTwo );
    xTaskCreate(vDrawCircleBlink2Hz, "CircleTaskD", mainGENERIC_STACK_SIZE, (void*)1, mainGENERIC_PRIORITY , &HandleCircleOne );
    vTaskResume(HandleCircleTwo);
    vTaskResume(HandleCircleOne);
   
    xTaskCreate(Button_K,"K",mainGENERIC_STACK_SIZE,NULL,mainGENERIC_PRIORITY,&ButtonK);
    xTaskCreate(Button_S,"S",mainGENERIC_STACK_SIZE,NULL,mainGENERIC_PRIORITY,&ButtonS);

   
    xTaskCreate(Timer,"Timer",mainGENERIC_STACK_SIZE,(void*) 1,mainGENERIC_PRIORITY,&Time);
    while (1) {
       if (DrawSignal)
            if (xSemaphoreTake(DrawSignal, portMAX_DELAY) ==
                pdTRUE) {
                tumEventFetchEvents(FETCH_EVENT_BLOCK |
                                    FETCH_EVENT_NO_GL_CHECK);
    
            xGetButtonInput(); // Update global button data
                xSemaphoreTake(ScreenLock, portMAX_DELAY);
                // Clear screen
                checkDraw(tumDrawClear(White), __FUNCTION__);
                
               
               // Draw FPS in lower right corner
            vDrawFPS();
            TimeText();
             
                xSemaphoreGive(buttons.lock);
                xSemaphoreGive(ScreenLock);
               vCheckStateInput_P();
               DrawButtonText();
                vCheckStateInput();
            }
          
            
        }
   
}
#define PRINT_TASK_ERROR(task) PRINT_ERROR("Failed to print task ##task");

void vDemoTask3(void *pvParameters)
{
    vTaskDelete(HandleCircleOne);
    vTaskDelete(HandleCircleTwo);
   

    prints("Task 1 init'd\n");

    while (1) {
        if (DrawSignal)
            if (xSemaphoreTake(DrawSignal, portMAX_DELAY) ==
                pdTRUE) {
               
                xGetButtonInput(); // Update global button data

                xSemaphoreTake(ScreenLock, portMAX_DELAY);
                // Clear screen
                checkDraw(tumDrawClear(White), __FUNCTION__);

                

                // Draw the walls
                checkDraw(tumDrawFilledBox(100,100,100,100,Red),
                              
                          __FUNCTION__);
                
                xSemaphoreGive(ScreenLock);
                // Check for state change
                vCheckStateInput();

            }
    }
}



#define PRINT_TASK_ERROR(task) PRINT_ERROR("Failed to print task ##task");

int main(int argc, char *argv[])
{   
    char *bin_folder_path = tumUtilGetBinFolderPath(argv[0]);

    prints("Initializing: ");

    if (tumDrawInit(bin_folder_path)) {
        PRINT_ERROR("Failed to intialize drawing");
        goto err_init_drawing;
    }
    else {
        prints("drawing");
    }

    if (tumEventInit()) {
        PRINT_ERROR("Failed to initialize events");
        goto err_init_events;
    }
    else {
        prints(", events");
    }

    if (tumSoundInit(bin_folder_path)) {
        PRINT_ERROR("Failed to initialize audio");
        goto err_init_audio;
    }
    else {
        prints(", and audio\n");
    }

    if (safePrintInit()) {
        PRINT_ERROR("Failed to init safe print");
        goto err_init_safe_print;
    }

    logo_image = tumDrawLoadImage(LOGO_FILENAME);

    atexit(aIODeinit);

    //Load a second font for fun
    tumFontLoadFont(FPS_FONT, DEFAULT_FONT_SIZE);

    buttons.lock = xSemaphoreCreateMutex(); // Locking mechanism
    if (!buttons.lock) {
        PRINT_ERROR("Failed to create buttons lock");
        goto err_buttons_lock;
    }

    DrawSignal = xSemaphoreCreateBinary(); // Screen buffer locking
    if (!DrawSignal) {
        PRINT_ERROR("Failed to create draw signal");
        goto err_draw_signal;
    }
    ScreenLock = xSemaphoreCreateMutex();
    if (!ScreenLock) {
        PRINT_ERROR("Failed to create screen lock");
        goto err_screen_lock;
    }
        mySyncSignal= xSemaphoreCreateBinary();
    if (!mySyncSignal)
    {
        PRINT_ERROR("Failed to create sync signal");
        goto err_sync;
    }

    mySyncSignalTask= xSemaphoreCreateBinary();
    if (!mySyncSignalTask)
    {
        PRINT_ERROR("Failed to create sync signal");
        goto err_sync;
    }


    // Message sending
    StateQueue = xQueueCreate(STATE_QUEUE_LENGTH, sizeof(unsigned char));
    if (!StateQueue) {
        PRINT_ERROR("Could not open state queue");
        goto err_state_queue;
    }

    if (xTaskCreate(basicSequentialStateMachine, "StateMachine",
                    mainGENERIC_STACK_SIZE * 2, NULL,
                    configMAX_PRIORITIES - 1, &StateMachine) != pdPASS) {
        PRINT_TASK_ERROR("StateMachine");
        goto err_statemachine;
    }
    if (xTaskCreate(vSwapBuffers, "BufferSwapTask",
                    mainGENERIC_STACK_SIZE * 2, NULL, configMAX_PRIORITIES,
                    &BufferSwap) != pdPASS) {
        PRINT_TASK_ERROR("BufferSwapTask");
        goto err_bufferswap;
    }
    /** Demo Tasks */
    if (xTaskCreate(vDemoTask1, "DemoTask1", mainGENERIC_STACK_SIZE * 2,
                    NULL, mainGENERIC_PRIORITY, &DemoTask1) != pdPASS) {
        PRINT_TASK_ERROR("DemoTask1");
        goto err_demotask1;
         }
    if (xTaskCreate(vDemoTask2, "DemoTask2", mainGENERIC_STACK_SIZE * 2,
                    NULL, mainGENERIC_PRIORITY, &DemoTask2) != pdPASS) {
       PRINT_TASK_ERROR("DemoTask2");
        goto err_demotask2;
    }
     if (xTaskCreate(vDemoTask3, "DemoTask3", mainGENERIC_STACK_SIZE * 2,
                    NULL, mainGENERIC_PRIORITY, &DemoTask3) != pdPASS) {
       PRINT_TASK_ERROR("DemoTask3");
        goto err_demotask3;
    }

    /** SOCKETS */
    xTaskCreate(vUDPDemoTask, "UDPTask", mainGENERIC_STACK_SIZE * 2, NULL,
                configMAX_PRIORITIES - 1, &UDPDemoTask);
    xTaskCreate(vTCPDemoTask, "TCPTask", mainGENERIC_STACK_SIZE, NULL,
                configMAX_PRIORITIES - 1, &TCPDemoTask);

    /** POSIX MESSAGE QUEUES */
    xTaskCreate(vMQDemoTask, "MQTask", mainGENERIC_STACK_SIZE * 2, NULL,
                configMAX_PRIORITIES - 1, &MQDemoTask);
    xTaskCreate(vDemoSendTask, "SendTask", mainGENERIC_STACK_SIZE * 2, NULL,
                configMAX_PRIORITIES - 1, &DemoSendTask);

   
    
    vTaskSuspend(DemoTask1);
    vTaskSuspend(DemoTask2);
    vTaskSuspend(DemoTask3);
   
    
    tumFUtilPrintTaskStateList();

    vTaskStartScheduler();

    return EXIT_SUCCESS;

err_demotask3:
    vTaskDelete(DemoTask2);
    
err_demotask2:
    vTaskDelete(DemoTask1); 

err_demotask1:
    vTaskDelete(BufferSwap);
    
err_bufferswap:
    vTaskDelete(StateMachine);
err_statemachine:
    vQueueDelete(StateQueue);
err_state_queue:
    vSemaphoreDelete(ScreenLock);
err_screen_lock:
    vSemaphoreDelete(DrawSignal);
err_draw_signal:
    vSemaphoreDelete(buttons.lock);
err_sync:
    vSemaphoreDelete(mySyncSignal);
err_buttons_lock:
    tumSoundExit();
err_init_audio:
    tumEventExit();
err_init_events:
    tumDrawExit();
err_init_drawing:
    safePrintExit();
err_init_safe_print:
    return EXIT_FAILURE;
}

// cppcheck-suppress unusedFunction
__attribute__((unused)) void vMainQueueSendPassed(void)
{
    /* This is just an example implementation of the "queue send" trace hook. */
}

// cppcheck-suppress unusedFunction
__attribute__((unused)) void vApplicationIdleHook(void)
{
#ifdef __GCC_POSIX__
    struct timespec xTimeToSleep, xTimeSlept;
    /* Makes the process more agreeable when using the Posix simulator. */
    xTimeToSleep.tv_sec = 1;
    xTimeToSleep.tv_nsec = 0;
    nanosleep(&xTimeToSleep, &xTimeSlept);
#endif
}
void vApplicationGetIdleTaskMemory( StaticTask_t **ppxIdleTaskTCBBuffer,
                                    StackType_t **ppxIdleTaskStackBuffer,
                                    uint32_t *pulIdleTaskStackSize )
{

/* If the buffers to be provided to the Idle task are declared inside this
function then they must be declared static – otherwise they will be allocated on
the stack and so not exists after this function exits. */
static StaticTask_t xIdleTaskTCB;
static StackType_t uxIdleTaskStack[ configMINIMAL_STACK_SIZE ];

    /* Pass out a pointer to the StaticTask_t structure in which the Idle task’s
    state will be stored. */
    *ppxIdleTaskTCBBuffer = &xIdleTaskTCB;

    /* Pass out the array that will be used as the Idle task’s stack. */
    *ppxIdleTaskStackBuffer = uxIdleTaskStack;

    /* Pass out the size of the array pointed to by *ppxIdleTaskStackBuffer.
    Note that, as the array is necessarily of type StackType_t,
    configMINIMAL_STACK_SIZE is specified in words, not bytes. */
    *pulIdleTaskStackSize = configMINIMAL_STACK_SIZE;
}
#define configTIMER_TASK_STACK_DEPTH            configMINIMAL_STACK_SIZE
void vApplicationGetTimerTaskMemory( StaticTask_t **ppxTimerTaskTCBBuffer, StackType_t **ppxTimerTaskStackBuffer, uint32_t *pulTimerTaskStackSize)
{
    static StaticTask_t xTimerTaskTCB;
    static StackType_t uxTimerTaskStack[configTIMER_TASK_STACK_DEPTH];
    *ppxTimerTaskTCBBuffer = &xTimerTaskTCB;
    *ppxTimerTaskStackBuffer = uxTimerTaskStack;
    *pulTimerTaskStackSize =configTIMER_TASK_STACK_DEPTH;
}
