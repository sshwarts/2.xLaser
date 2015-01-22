// Laser Controller

#include <Adafruit_GFX.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_ILI9341.h>
#include <Adafruit_STMPE610.h>
#include <SD.h>
#include <SimpleTimer.h>

// Touch Screen defines
// This is calibration data for the raw touch data to the screen coordinates
#define TS_MINX 150
#define TS_MINY 130
#define TS_MAXX 3800
#define TS_MAXY 4000

#define STMPE_CS 8
#define TFT_CS 10
#define TFT_DC 9
// Touchscreen variables
Adafruit_STMPE610 ts = Adafruit_STMPE610(STMPE_CS);
// tft is the display
Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC);


// useful defines
#define OFF false;
#define ON true;

// IO
#define SPKR 6
#define PUMP 2
#define BLOWER 3
#define AIR 7
#define AUX 5

#define WATER_FLOW A1
#define COVER A0
#define FIRING A2
#define ENABLE_LASER A3

#define SD_CS 4

#define NUMBUTTONS 5

// Rectangles of our graphic objects (x, y, h, w)
static const int16_t Banner[2] = {74,3}; 
static const int16_t Pump_Btn[4] = {12, 24, 50, 100};
static const int16_t Blower_Btn[4] = {12, 78, 50, 100};
static const int16_t Air_Btn[4] = {12, 132, 50, 100};
static const int16_t Aux_Btn[4] = {12, 186, 50, 100};
static const int16_t Enable_Btn[4] = {12, 240, 50, 100};

static const int16_t Flow_Indicator[2] = {124, 40};
static const int16_t Cover_Indicator[2] = {164, 42};
static const int16_t Firing_Label[2] = {180, 30};
static const int16_t Firing_Indicator[2] = {180, 40};

static const int16_t Job_Time[2] = {218, 132};
static const int16_t Job_Time_Label[2] = {140, 132};
static const int16_t Total_Time_Label[2] = {140, 156};
static const int16_t Total_Time[2] = {218, 156};

// Global Variables
boolean Pump = OFF;
boolean Blower = OFF;
boolean Air = OFF;
boolean Aux = OFF;
boolean Firing = false;
boolean Enabled = false;
boolean Cover = true;	// Let's assume open
boolean Water_Flowing = false;
boolean showFireTime = false;
byte lastPressed = 0;
unsigned long pressedTime;
unsigned long fireTime = 0;
unsigned long accFireTime = 0;
unsigned long totalFireTime = 0;
byte lifeTime[4] = {0,0,0,0};
byte tickCount = 0;

SimpleTimer timer;
File dataFile;    // SD Card


void setup(void)
{  
  Serial.begin(9600);
  
  randomSeed(analogRead(A4));
  // Setup Pins
   pinMode(SPKR, OUTPUT);  // Beeper
   pinMode(PUMP, OUTPUT);
   pinMode(BLOWER, OUTPUT);
   pinMode(AIR, OUTPUT);
   pinMode(AUX, OUTPUT);
   
   
   pinMode(WATER_FLOW, INPUT);
   pinMode(COVER, INPUT);
   pinMode(FIRING, INPUT);
   pinMode(ENABLE_LASER, OUTPUT);
         
   // Set them all to 1 which is off
   digitalWrite(PUMP, HIGH);
   digitalWrite(BLOWER, HIGH);
   digitalWrite(AIR, HIGH);
   digitalWrite(AUX, HIGH);
   digitalWrite(ENABLE_LASER, HIGH);
   
   // Enable the pull-ups on the Analog inputs
   digitalWrite(WATER_FLOW, HIGH);
   digitalWrite(COVER, HIGH);
   digitalWrite(FIRING, HIGH);

  
  // Setup Display
  tft.begin();  // Start up the display
  if (!ts.begin()) { 
    Serial.println("Unable to start touchscreen.");
  } 
  else { 
    Serial.println("Touchscreen started."); 
  }

  tft.setRotation(1); 
 
  Serial.print("Initializing SD card...");
  if (!SD.begin(SD_CS)) {
    Serial.println("failed!");
  }
  
  // Read the card for total laser time

  // Display a random picture
  if ((random(1,3) == 1))
    bmpDraw("1.bmp", 0, 0);
  else
    bmpDraw("2.bmp", 0, 0);
 
    delay(4000);
    
  // Open the file for storing laser running time
  dataFile = SD.open("log.dat", FILE_WRITE);
  dataFile.seek(0);  // reset to beginning
  // Read the total run time
  byte index = 0;
  while (dataFile.available()) {
    lifeTime[index] = dataFile.read();
    Serial.println( lifeTime[index]);
    index++;
  }
  
  totalFireTime = ( ((unsigned long)lifeTime[0] << 24) 
                   + ((unsigned long)lifeTime[1] << 16) 
                   + ((unsigned long)lifeTime[2] << 8) 
                   + ((unsigned long)lifeTime[3] ) ) ;
                   
  //Serial.println(totalFireTime);
  
  tft.fillScreen(ILI9341_BLUE);
    
  // Draw Buttons
  drawMainButtons();
  
  // Draw initial Pump Status
  drawIndicators();
  
  // Draw Labels
  drawLabels();

  // Set up the timer so we check for buttons and draw states
   timer.setInterval(100, process);	// Ever 100 milliseconds
   
   // Set up another one to update time display
   timer.setInterval(1000, updateTime); // Every second


}

void loop()
{
  
  timer.run();
  
  

}

void process() {
  byte index = 0;
  boolean  inputValue;
  boolean	aChange = false;  
  unsigned long  time;

          // Process the other inputs
	  // Check Water Flow
	  inputValue = digitalRead(WATER_FLOW);	// false means flowing
	  if (inputValue != Water_Flowing) {
	  	Water_Flowing = inputValue;
	  	aChange = true;
	  }
	  
	  // Check Cover
	  inputValue = digitalRead(COVER);	// true means open
	  if (inputValue != Cover) {
	  	Cover = inputValue;
	  	aChange = true;
	  }

	// Check if laser is firing so we can accumulate run time
        Firing = digitalRead(FIRING);
        //Serial.println(Firing);
        // If water isn't flowing then laser is not really firing
        if (Water_Flowing && Firing) {
          time = millis();

          if (fireTime > 0) {
             accFireTime += time - fireTime;
             fireTime = time;

          } else
            fireTime = time;
           
        } else
          fireTime = 0;
          
	if (aChange)
		drawIndicators();
	  
      // See if there's any  touch data for us
		if (!ts.bufferEmpty()) {   
		// Retrieve a point  
		TS_Point p = ts.getPoint(); 
		// Scale using the calibration #'s
		// and rotate coordinate system
		 p.x = map(p.x, TS_MINY, TS_MAXY, 0, tft.height());
                 p.y = map(p.y, TS_MINX, TS_MAXX, 0, tft.width());
		
                int y = tft.height() - p.x;
                int x = p.y;
		
		// Check each button. Start with X
		if (x > Pump_Btn[0] && x < (Pump_Btn[0] + Pump_Btn[3])) {
			// It's in the first row of buttons. Now let's see which one.
			if (y > Pump_Btn[1] && y < (Pump_Btn[1] + Pump_Btn[2])) {
				index = 1;
			} else if (y > Blower_Btn[1] && y < (Blower_Btn[1] + Blower_Btn[2])) {
				index = 2;
			} else if (y > Air_Btn[1] && y < (Air_Btn[1] + Air_Btn[2])) {
				index = 3;
			} else if  (y > Aux_Btn[1] && y < (Aux_Btn[1] + Aux_Btn[2])) {
				index = 4;
			}
		} else {
			if (x > Enable_Btn[0] && x < (Enable_Btn[0] + Enable_Btn[3]) && y > Enable_Btn[1] && y < (Enable_Btn[1] + Enable_Btn[2])) {
				// We know it's the enable button
				index = 5;
			}
		}
		 // Process a unique press (if any)
		  if (index > 0 && index != lastPressed) {
			lastPressed = index;
			// Log the press
			pressedTime = millis();
		
			// Here's where we control all the outputs
			// First Draw all the states
			//drawMainButtons();
			switch (index) {
				case 1:
					if (Pump) {
						digitalWrite(PUMP, HIGH);
						Pump = OFF;
					} else {
						digitalWrite(PUMP, LOW);
						Pump = ON;
					}	
					  drawBtn(Pump_Btn[0], Pump_Btn[1], Pump_Btn[2], Pump_Btn[3], "Pump", Pump);
					break;
				case 2:
					if (Blower) {
						digitalWrite(BLOWER, HIGH);
						Blower = OFF;
					} else {
						digitalWrite(BLOWER, LOW);
						Blower = ON;
					}
					  drawBtn(Blower_Btn[0], Blower_Btn[1], Blower_Btn[2], Blower_Btn[3], "Blower", Blower);
					break;

				case 3:
					  if (Air) {
						digitalWrite(AIR, HIGH);
						Air = OFF;
					} else {
						digitalWrite(AIR, LOW);
						Air = ON;
					}
					drawBtn(Air_Btn[0], Air_Btn[1], Air_Btn[2], Air_Btn[3], "Air", Air);
					  break;

				case 4:
					 if (Aux) {
						digitalWrite(AUX, HIGH);
						Aux = OFF;
					} else {
						digitalWrite(AUX, LOW);
						Aux = ON;
					}
					 drawBtn(Aux_Btn[0], Aux_Btn[1], Aux_Btn[2], Aux_Btn[3], "Aux", Aux);
					break;

				case 5:		// Enable button
					break;

		
		
			}
		
	  
		  }
		  if (millis() > pressedTime + 600)	// We're allowed to press twice as long as enough time went by
			lastPressed = 0;
			
		    ts.writeRegister8(STMPE_INT_STA, 0xFF); // reset all ints

	  }
	  
	 
	  
}

void drawBtn(int16_t x, int16_t y, int16_t height, int16_t width, char* label, boolean state)
{ 
    tft.drawRoundRect(x+1, y+1,width, height, 16, ILI9341_BLACK);
if (state)
  	tft.fillRoundRect(x, y, width, height, 16, ILI9341_GREEN);
  else
  	tft.fillRoundRect(x, y, width, height, 16, ILI9341_RED);

  tft.setCursor(x + 9 , y + (height/2)-4);
  tft.setTextColor(ILI9341_WHITE);
  tft.setTextSize(2);
  tft.println(label);
}


void beep(unsigned char freq){
   
  // for (int i=0; i<20; i++) {  // generate a 1KHz tone for 1/2 second
   // digitalWrite(SPKR, HIGH);
   // delayMicroseconds(500);
   // digitalWrite(SPKR, LOW);
   // delayMicroseconds(500);
  //}
} 

void drawIndicator(int16_t x, int16_t y, boolean state) {
	uint16_t	color;
	if (state) 
		color = ILI9341_GREEN;
	else 
		color = ILI9341_RED;
		

	tft.drawRoundRect(x-1, y-1, 24, 24, 12, ILI9341_BLACK);
 	tft.fillRoundRect(x, y, 22, 22, 10, color);

}

void drawLabels() {
  
  tft.setTextSize(2);
  
  // Show Banner
  tft.setTextColor(ILI9341_BLACK);
  tft.setCursor(Banner[0], Banner[1]);
  tft.println("Big Scary Laser");
  
  tft.setTextColor(ILI9341_WHITE);

  tft.setCursor(Job_Time_Label[0], Job_Time_Label[1]);
  tft.println("Job:");
  
   tft.setCursor(Total_Time_Label[0], Total_Time_Label[1]);
  tft.println("Total:");
  
  // Show total for first time
  tft.setCursor(Total_Time[0] , Total_Time[1]);
  tft.print(totalFireTime); tft.println(" m");
  
}


void drawMainButtons() {
	// Draw Main Control Buttons
  drawBtn(Pump_Btn[0], Pump_Btn[1], Pump_Btn[2], Pump_Btn[3], "Pump", Pump);
  drawBtn(Blower_Btn[0], Blower_Btn[1], Blower_Btn[2], Blower_Btn[3], "Blower", Blower);
  drawBtn(Air_Btn[0], Air_Btn[1], Air_Btn[2], Air_Btn[3], "Air", Air);
  drawBtn(Aux_Btn[0], Aux_Btn[1], Aux_Btn[2], Aux_Btn[3], "Aux", Aux);
  //drawBtn(Enable_Btn[0], Enable_Btn[1], Enable_Btn[2], Enable_Btn[3], "Enable", Enabled);

}

void drawIndicators() {
  drawIndicator(Flow_Indicator[0], Flow_Indicator[1], Water_Flowing);
  
  tft.setCursor(Cover_Indicator[0] , Cover_Indicator[1]);
  if (Cover)
  	tft.setTextColor(ILI9341_RED);
  else
  	tft.setTextColor(ILI9341_BLUE);
  tft.setTextSize(2);
  tft.println("Cover");
 
}

// Update our timer display
void updateTime() {
   if (accFireTime != 0 && fireTime != 0) {  
     // Erase the old time
     tft.fillRect(Job_Time[0], Job_Time[1], 80, 22, ILI9341_BLUE);
     
     tft.setCursor(Job_Time[0] , Job_Time[1]);
     tft.setTextColor(ILI9341_WHITE);
     tft.setTextSize(2);
     tft.print(accFireTime/1000); tft.println(" s");
     
     tickCount++;
     
   }
   
   // If the tickcount is more than 30 seconds, update the total and write to the card
   if (tickCount > 30) {
     // Add the accumulated fire time to our lifetime
     //totalFireTime = round(totalFireTime + (accFireTime/1000)/60);
     //Serial.print(round(totalFireTime + (accFireTime/1000)/60)); Serial.println(" total");
    
     // Update total time
     tft.fillRect(Total_Time[0], Total_Time[1], 80, 22, ILI9341_BLUE);
     
     tft.setCursor(Total_Time[0] , Total_Time[1]);
    // tft.setTextColor(ILI9341_WHITE);
     //tft.setTextSize(2);
     tft.print(round(totalFireTime + (accFireTime/1000)/60)); tft.println(" m");
    
     tickCount = 0;
     
     // Update the card
     // convert from an unsigned long int to a 4-byte array
    unsigned long accTotalFire = round(totalFireTime + (accFireTime/1000)/60);
    
    lifeTime[0] = (int)((accTotalFire >> 24) & 0xFF) ;
    lifeTime[1] = (int)((accTotalFire >> 16) & 0xFF) ;
    lifeTime[2] = (int)((accTotalFire >> 8) & 0XFF);
    lifeTime[3] = (int)((accTotalFire & 0XFF));
    
    Serial.println(lifeTime[0]);
    Serial.println(lifeTime[1]);
    Serial.println(lifeTime[2]);
    Serial.println(lifeTime[3]);
    
    // Reset position
    dataFile.seek(0);
    dataFile.write(&lifeTime[0], 4);
    dataFile.flush();
   }
  
  
}

// This function opens a Windows Bitmap (BMP) file and
// displays it at the given coordinates.  It's sped up
// by reading many pixels worth of data at a time
// (rather than pixel by pixel).  Increasing the buffer
// size takes more of the Arduino's precious RAM but
// makes loading a little faster.  20 pixels seems a
// good balance.

#define BUFFPIXEL 20

void bmpDraw(char *filename, uint8_t x, uint16_t y) {

  File     bmpFile;
  int      bmpWidth, bmpHeight;   // W+H in pixels
  uint8_t  bmpDepth;              // Bit depth (currently must be 24)
  uint32_t bmpImageoffset;        // Start of image data in file
  uint32_t rowSize;               // Not always = bmpWidth; may have padding
  uint8_t  sdbuffer[3*BUFFPIXEL]; // pixel buffer (R+G+B per pixel)
  uint8_t  buffidx = sizeof(sdbuffer); // Current position in sdbuffer
  boolean  goodBmp = false;       // Set to true on valid header parse
  boolean  flip    = true;        // BMP is stored bottom-to-top
  int      w, h, row, col;
  uint8_t  r, g, b;
  uint32_t pos = 0; //startTime = millis();

  if((x >= tft.width()) || (y >= tft.height())) return;

  //Serial.println();
 // Serial.print(F("Loading image '"));
  //Serial.print(filename);
  //Serial.println('\'');

  // Open requested file on SD card
  if ((bmpFile = SD.open(filename)) == NULL) {
    Serial.print(F("File not found"));
    return;
  }

  // Parse BMP header
  if(read16(bmpFile) == 0x4D42) { // BMP signature
   read32(bmpFile);
    (void)read32(bmpFile); // Read & ignore creator bytes
    bmpImageoffset = read32(bmpFile); // Start of image data
    Serial.print(F("Image Offset: ")); Serial.println(bmpImageoffset, DEC);
    // Read DIB header
    read32(bmpFile);
    bmpWidth  = read32(bmpFile);
    bmpHeight = read32(bmpFile);
    if(read16(bmpFile) == 1) { // # planes -- must be '1'
      bmpDepth = read16(bmpFile); // bits per pixel
     //Serial.print(F("Bit Depth: ")); Serial.println(bmpDepth);
      if((bmpDepth == 24) && (read32(bmpFile) == 0)) { // 0 = uncompressed

        goodBmp = true; // Supported BMP format -- proceed!
        //Serial.print(F("Image size: "));
       // Serial.print(bmpWidth);
       // Serial.print('x');
       // Serial.println(bmpHeight);

        // BMP rows are padded (if needed) to 4-byte boundary
        rowSize = (bmpWidth * 3 + 3) & ~3;

        // If bmpHeight is negative, image is in top-down order.
        // This is not canon but has been observed in the wild.
        if(bmpHeight < 0) {
          bmpHeight = -bmpHeight;
          flip      = false;
        }

        // Crop area to be loaded
        w = bmpWidth;
        h = bmpHeight;
        if((x+w-1) >= tft.width())  w = tft.width()  - x;
        if((y+h-1) >= tft.height()) h = tft.height() - y;

        // Set TFT address window to clipped image bounds
        tft.setAddrWindow(x, y, x+w-1, y+h-1);

        for (row=0; row<h; row++) { // For each scanline...

          // Seek to start of scan line.  It might seem labor-
          // intensive to be doing this on every line, but this
          // method covers a lot of gritty details like cropping
          // and scanline padding.  Also, the seek only takes
          // place if the file position actually needs to change
          // (avoids a lot of cluster math in SD library).
          if(flip) // Bitmap is stored bottom-to-top order (normal BMP)
            pos = bmpImageoffset + (bmpHeight - 1 - row) * rowSize;
          else     // Bitmap is stored top-to-bottom
            pos = bmpImageoffset + row * rowSize;
          if(bmpFile.position() != pos) { // Need seek?
            bmpFile.seek(pos);
            buffidx = sizeof(sdbuffer); // Force buffer reload
          }

          for (col=0; col<w; col++) { // For each pixel...
            // Time to read more pixel data?
            if (buffidx >= sizeof(sdbuffer)) { // Indeed
              bmpFile.read(sdbuffer, sizeof(sdbuffer));
              buffidx = 0; // Set index to beginning
            }

            // Convert pixel from BMP to TFT format, push to display
            b = sdbuffer[buffidx++];
            g = sdbuffer[buffidx++];
            r = sdbuffer[buffidx++];
            tft.pushColor(tft.color565(r,g,b));
          } // end pixel
        } // end scanline
        //Serial.print(F("Loaded in "));
       // Serial.print(millis() - startTime);
        //Serial.println(" ms");
      } // end goodBmp
    }
  }

  bmpFile.close();
  //if(!goodBmp) Serial.println(F("BMP format not recognized."));
}

// These read 16- and 32-bit types from the SD card file.
// BMP data is stored little-endian, Arduino is little-endian too.
// May need to reverse subscript order if porting elsewhere.

uint16_t read16(File &f) {
  uint16_t result;
  ((uint8_t *)&result)[0] = f.read(); // LSB
  ((uint8_t *)&result)[1] = f.read(); // MSB
  return result;
}

uint32_t read32(File &f) {
  uint32_t result;
  ((uint8_t *)&result)[0] = f.read(); // LSB
  ((uint8_t *)&result)[1] = f.read();
  ((uint8_t *)&result)[2] = f.read();
  ((uint8_t *)&result)[3] = f.read(); // MSB
  return result;
}



