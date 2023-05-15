#include <mbed.h>
//this file has all the functions for interacting
//with the screen
#include "stm32f4xx_hal.h"
#include "drivers/LCD_DISCO_F429ZI.h"
#define BACKGROUND 1
#define FOREGROUND 0
#define GRAPH_PADDING 5
LCD_DISCO_F429ZI lcd;
Timer t;
int btnflag=0;
SPI spi(PF_9, PF_8, PF_7,PC_1,use_gpio_ssel); // mosi, miso, sclk, cs

//address of first register with gyro data
#define OUT_X_L 0x28

//register fields(bits): data_rate(2),Bandwidth(2),Power_down(1),Zen(1),Yen(1),Xen(1)
#define CTRL_REG1 0x20

//configuration: 200Hz ODR,50Hz cutoff, Power on, Z on, Y on, X on
#define CTRL_REG1_CONFIG 0b01'10'1'1'1'1

//register fields(bits): reserved(1), endian-ness(1),Full scale sel(2), reserved(1),self-test(2), SPI mode(1)
#define CTRL_REG4 0x23

//configuration: reserved,little endian,500 dps,reserved,disabled,4-wire mode
#define CTRL_REG4_CONFIG 0b0'0'01'0'00'0

#define SPI_FLAG 1

uint8_t write_buf[32]; 
uint8_t read_buf[32];
EventFlags flags;
//The spi.transfer() function requires that the callback
//provided to it takes an int parameter
void spi_cb(int event){
  flags.set(SPI_FLAG);
}
// function is for filtering the data from spi
float* movingAverageFilter(float *x, int n,  int window_size) {
float y[n];
float sum = 0.0;
// Loop through the input array x.
for (int i = 0; i < n; i++) {
    // Add the current value of x[i] to the sum.
    sum += x[i];

    // If we have processed at least window_size values, we can start to output results.
    if (i >= window_size - 1) {
        // Calculate the average of the last window_size values.
        y[i - window_size + 1] = sum / window_size;

        // Subtract the oldest value from the sum.
        sum -= x[i - window_size + 1];
    }
}
    return y;
}


//
void toglle()
{if(t.read_ms()>=10)//prevent button bouncing
{if (btnflag==8)// attempt more than 3 times, exit 
   { lcd.Clear(LCD_COLOR_BLACK);
   lcd.SetTextColor(LCD_COLOR_DARKRED);
   lcd.SetFont(&Font24);
    lcd.DisplayStringAt(0,LINE(2),(uint8_t*)"NO",CENTER_MODE);
     lcd.DisplayStringAt(0,LINE(6),(uint8_t*)"MORE",CENTER_MODE);
     lcd.DisplayStringAt(0,LINE(10),(uint8_t*)"CHANCE !!!",CENTER_MODE);
    exit;
       
   }

  if (btnflag==7)//the eighth time to press the button which means the third recogition end
   { lcd.Clear(LCD_COLOR_BLACK);
    
       btnflag++;
   }
    if (btnflag==6)//the seventh time to press the button which means the third recognition start
   { lcd.Clear(LCD_COLOR_BLACK);
    lcd.SetTextColor(LCD_COLOR_LIGHTRED);
  lcd.DisplayStringAt(0,LINE(5),(uint8_t*)"GESTURE",CENTER_MODE);
     lcd.DisplayStringAt(0,LINE(10),(uint8_t*)"RECOGNITION",CENTER_MODE);
     lcd.DisplayStringAt(0,LINE(15),(uint8_t*)"START",CENTER_MODE);
       btnflag++;
   }
   if (btnflag==5)//the sixth time to press the button which means the third recognition end 
   { lcd.Clear(LCD_COLOR_BLACK);
       btnflag++;
   }
  if (btnflag==4)//the fifth time to press the button which means the second recognition start 
   { lcd.Clear(LCD_COLOR_BLACK);
    lcd.SetTextColor(LCD_COLOR_LIGHTRED);
   lcd.DisplayStringAt(0,LINE(5),(uint8_t*)"GESTURE",CENTER_MODE);
     lcd.DisplayStringAt(0,LINE(10),(uint8_t*)"RECOGNITION",CENTER_MODE);
     lcd.DisplayStringAt(0,LINE(15),(uint8_t*)"START",CENTER_MODE);
       btnflag++;
   }
  if (btnflag==3)//the forth time to press the button which means the first recongnition end
   { lcd.Clear(LCD_COLOR_BLACK);
   
       btnflag=4;
   }
   if (btnflag==2)//the third time to press the button which means the first recognition start 
   { lcd.Clear(LCD_COLOR_BLACK);
    lcd.SetTextColor(LCD_COLOR_LIGHTRED);
  lcd.DisplayStringAt(0,LINE(5),(uint8_t*)"GESTURE",CENTER_MODE);
     lcd.DisplayStringAt(0,LINE(10),(uint8_t*)"RECOGNITION",CENTER_MODE);
     lcd.DisplayStringAt(0,LINE(15),(uint8_t*)"START",CENTER_MODE);
       btnflag=3;
   }
   if (btnflag==1)//the second time to press the button which means recording finished
   { lcd.Clear(LCD_COLOR_BLACK);
   lcd.DisplayStringAt(0,LINE(8),(uint8_t*)"RECORDING FINISHED",CENTER_MODE);
    lcd.SetTextColor(LCD_COLOR_LIGHTRED);
     lcd.DisplayStringAt(0,LINE(12),(uint8_t*)"LOCKED",CENTER_MODE);
       btnflag=2;
   }
   if (btnflag==0)//first time to press the button which means recording start
   {lcd.Clear(LCD_COLOR_BLACK);
     lcd.DisplayStringAt(0,LINE(5),(uint8_t*)"RECORDING ",CENTER_MODE);
     lcd.DisplayStringAt(0,LINE(10),(uint8_t*)"START",CENTER_MODE);
     lcd.DisplayStringAt(0,LINE(15),(uint8_t*)"RECORDING PROGRESS",CENTER_MODE);
         btnflag=1;
   }
    t.reset();
}
}


//buffer for holding displayed text strings
char display_buf[2][60];
uint32_t graph_width=lcd.GetXSize()-2*GRAPH_PADDING;
uint32_t graph_height=graph_width;

//sets the background layer 
//to be visible, transparent, and
//resets its colors to all black
void setup_background_layer(){
  lcd.SelectLayer(BACKGROUND);
  lcd.Clear(LCD_COLOR_BLACK);
  lcd.SetBackColor(LCD_COLOR_BLACK);
  lcd.SetTextColor(LCD_COLOR_GREEN);
  lcd.SetLayerVisible(BACKGROUND,ENABLE);
  lcd.SetTransparency(BACKGROUND,0x7Fu);
}

//resets the foreground layer to
//all black
void setup_foreground_layer(){
    lcd.SelectLayer(FOREGROUND);
    lcd.Clear(LCD_COLOR_BLACK);
    lcd.SetBackColor(LCD_COLOR_BLACK);
    lcd.SetTextColor(LCD_COLOR_LIGHTGREEN);
}

//draws a rectangle with horizontal tick marks
//on the background layer. The spacing between tick
//marks in pixels is taken as a parameter
void draw_graph_window(uint32_t horiz_tick_spacing){
  lcd.SelectLayer(BACKGROUND);
  
  lcd.DrawRect(GRAPH_PADDING,GRAPH_PADDING,graph_width,graph_width);
  //draw the x-axis tick marks
  for (int32_t i = 0 ; i < graph_width;i+=horiz_tick_spacing){
    lcd.DrawVLine(GRAPH_PADDING+i,graph_height,GRAPH_PADDING);
  }
}

//maps inputY in the range minVal to maxVal, to a y-axis value pixel in the range
//minPixelY to MaxPixelY
uint16_t mapPixelY(float inputY,float minVal, float maxVal, int32_t minPixelY, int32_t maxPixelY){
  const float mapped_pixel_y=(float)maxPixelY-(inputY)/(maxVal-minVal)*((float)maxPixelY-(float)minPixelY);
  return mapped_pixel_y;
}

//DON'T COMMENT OUT THE ABOVE CODE
int*  digitalProcessing(float * x, int SHORT_ARRAY_SIZE, int segments){
   float x1[SHORT_ARRAY_SIZE];

// choose the short array
  for (int j = 0; j < SHORT_ARRAY_SIZE; j++) {
    x1[j] = x[j];
   
  }

   int segment_size = SHORT_ARRAY_SIZE / segments;  // calculate segment size
   int xp[segments];  // store the final processed value
    // loop through each segment and calculate its average value
    for (int i = 0; i < segments; i++) {
        int start_index = i * segment_size;
        int end_index = start_index + segment_size - 1;
        int sumx = 0;

        // loop through the current segment and calculate the sum of its values
        for (int j = start_index; j <= end_index; j++) {
            sumx += x1[j];
            
        }

        int averagex = static_cast<float>(sumx) / segment_size;  // calculate the average value
      

       for (int j = 0; j <= segments; j++) {
        xp[j] =  averagex;
  
       }
    }


    return xp ;

}
//this function is used to unlocked resource by comparing the key recording sequence with the unlocking sequence 
float similarity(float* a, float* b, int size){
       float sum1 = 0;
       float c ;
       for (int j = 0; j < 6; j++) {
        sum1 += (a[j] - b[j])*(a[j] - b[j]);  // calculate squared difference
        // to monitor the result.
        c = a[j] - b[j];
        printf("a = %4.5f\n",a[j]);
        printf("b = %4.5f\n",b[j]);

    }
  
    return sum1;

   
}

  
  
float x_test0[500]={0};// to store the x data from gyroscope of the recording sequence
float x_test1[500]={0};//to store the x data from gyroscope of the unlocking sequence
float y_test0[500]={0};// to store the y data from gyroscope of the recording sequence
float y_test1[500]={0};//to store the y data from gyroscope of the unlocking sequence
float z_test0[500]={0};// to store the z data from gyroscope of the recording sequence
float z_test1[500]={0};//to store the z data from gyroscope of the unlocking sequence
float saveDatax[500] = {0};// to store the x data 
float saveDatay[500] = {0};// to store the y data 
float saveDataz[500] = {0};// to store the z data 
 int size;
///*//START OF EXAMPLE 1---------------------------------------------
int main() 
{
  //initialize the lcd
  setup_background_layer();

  setup_foreground_layer();
  //creates c-strings in the display buffers, in preparation
  //for displaying them on the screen
  snprintf(display_buf[0],60,"width: %d pixels",lcd.GetXSize());
  snprintf(display_buf[1],60,"height: %d pixels",lcd.GetYSize());
  lcd.SelectLayer(FOREGROUND);
  //display the buffered string on the screen
  spi.format(8,3);
    spi.frequency(1'000'000);

    write_buf[0]=CTRL_REG1;
    write_buf[1]=CTRL_REG1_CONFIG;
    // initialize the interrupt
    InterruptIn btn(USER_BUTTON,PullDown);
    t.start();
    //interrupt service routine (ISR) function
    btn.rise(&toglle);
// set up the spi
    spi.transfer(write_buf,2,read_buf,2,spi_cb,SPI_EVENT_COMPLETE );
    flags.wait_all(SPI_FLAG);
    write_buf[0]=CTRL_REG4;
    write_buf[1]=CTRL_REG4_CONFIG;
    spi.transfer(write_buf,2,read_buf,2,spi_cb,SPI_EVENT_COMPLETE );
    flags.wait_all(SPI_FLAG); 


int i = 0;  // set counter for the saving list
    // Setup the spi for 8 bit data, high steady state clock,
    // second edge capture, with a 1MHz clock rate
    spi.format(8,3);
    spi.frequency(1'000'000);
    write_buf[0]=CTRL_REG1;
    write_buf[1]=CTRL_REG1_CONFIG;
    spi.transfer(write_buf,2,read_buf,2,spi_cb,SPI_EVENT_COMPLETE );
    flags.wait_all(SPI_FLAG);
    write_buf[0]=CTRL_REG4;
    write_buf[1]=CTRL_REG4_CONFIG;
    spi.transfer(write_buf,2,read_buf,2,spi_cb,SPI_EVENT_COMPLETE );
    flags.wait_all(SPI_FLAG); 
  lcd.SelectLayer(FOREGROUND); 
  //When flag = 0, it indicates standby mode, when flag = 1, it indicates running mode
  int flag = 0;
//LCD display the imformation of our project
 lcd.SetTextColor(LCD_COLOR_LIGHTRED);
    lcd.DisplayStringAt(0,LINE(5),(uint8_t*)"EMBEDED SENTRY",CENTER_MODE);
    lcd.DisplayStringAt(0,LINE(10),(uint8_t*)"GROUP 44",CENTER_MODE);
    lcd.DisplayStringAt(0,LINE(15),(uint8_t*)"NO RECORD",CENTER_MODE);
  
  while(1){    
     int16_t raw_gx,raw_gy,raw_gz;// setup 3 array to save data from spi 
   float gx, gy, gz;// setup 3 array to save raw_gx,raw_ gy and raw_gz

    while(btnflag%2==1)// it means the microchip is in recording mode
    {
      //prepare the write buffer to trigger a sequential read
      write_buf[0]=OUT_X_L|0x80|0x40;
      //start sequential sample reading
      spi.transfer(write_buf,7,read_buf,7,spi_cb,SPI_EVENT_COMPLETE );
      flags.wait_all(SPI_FLAG);
      //read_buf after transfer: garbage byte, gx_low,gx_high,gy_low,gy_high,gz_low,gz_high
      //Put the high and low bytes in the correct order lowB,HighB -> HighB,LowB
      raw_gx=( ( (uint16_t)read_buf[2] ) <<8 ) | ( (uint16_t)read_buf[1] );
      raw_gy=( ( (uint16_t)read_buf[4] ) <<8 ) | ( (uint16_t)read_buf[3] );
      raw_gz=( ( (uint16_t)read_buf[6] ) <<8 ) | ( (uint16_t)read_buf[5] );

      //printf("RAW|\tgx: %d \t gy: %d \t gz: %d\t",raw_gx,raw_gy,raw_gz);

      gx=((float)raw_gx)*(17.5f*0.017453292519943295769236907684886f / 1000.0f);
      gy=((float)raw_gy)*(17.5f*0.017453292519943295769236907684886f / 1000.0f);
      gz=((float)raw_gz)*(17.5f*0.017453292519943295769236907684886f / 1000.0f);
      printf("\n cha: %f", gy);
      printf("*********");
      //printf("Actual|\tgx: %4.5f \t gy: %4.5f \t gz: %4.5f\n",gx,gy,gz);
      
        if (i<=499){
      saveDatax[i] = gx; 
      saveDatay[i] = gy; 
      saveDataz[i] = gz; 
      }
      //save data
printf("\n i: %d", i);
i++;// data size plus 1
      thread_sleep_for(100);       //set up the sampling time
   
      flag = 1;

  }
//when butnflag is even, and the recording finished
if(flag == 1)//to check whether it is in the standby mode or not 
{
 size = i;// save the value of size 
 i =0;
printf("\n t: %d", size);

  if(btnflag==2)
  {
  
 //move the recording sequence data  
    for (int v=0; v< size; v++){
      x_test0[v] = saveDatax[v];  
  y_test0[v] = saveDatay[v];
    z_test0[v] = saveDataz[v];
    }    

  }
 else
{ char str[16];
 //move the unlocking sequence data  
    for (int j=0; j< size; j++){
          x_test1[j] = saveDatax[j];  
             y_test1[j] = saveDatay[j]; 
                z_test1[j] = saveDataz[j]; 
     }   
//monitor
      for (int j = 0; j < size; j++) {
        printf("\n");
   printf("myArray0[%d] = %4.5f\n", j, x_test0[j]);
   printf("myArray1[%d] = %4.5f\n", j, x_test1[j]);
   printf("\n*****************");
      }
   float AVERAGEx=similarity(x_test0,x_test1,size);//calculate the similarity of x data of the recording data and unlocking data
   float AVERAGEy=similarity(y_test0,y_test1,size);
   float AVERAGEz=similarity(z_test0,z_test1,size);
float AVERAGE=(AVERAGEz+AVERAGEx+AVERAGEy)/3;//calculate the final result
  // Display the string on the LCD screen 
   printf("Average = %4.5f\n", AVERAGEx);
   printf("+++++++++++");
  if(btnflag!=0)
{ 
  if(AVERAGE<2)// to check whether the similarity pass the threshold
  {lcd.SetTextColor(LCD_COLOR_LIGHTGREEN);
  lcd.DisplayStringAt(0,LINE(8),(uint8_t*)"UNLOCKED",CENTER_MODE);//show success
  }
   else
   {lcd.SetTextColor(LCD_COLOR_DARKRED);
     lcd.DisplayStringAt(0,LINE(8),(uint8_t*)"FAIL",CENTER_MODE
   );}

   
 sprintf(str, "AVERAGE: %f", AVERAGE);
  lcd.DisplayStringAt(0,LINE(12), (uint8_t*) str,CENTER_MODE);//show the result of samilarity
} 
   }
  
  flag = 0;

}

  }
  }
