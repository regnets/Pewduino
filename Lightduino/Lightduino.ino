#include <SPI.h>
#include <Adafruit_WS2801.h>
#include <IRLib.h>
#include <U8glib.h>

#define PIN_IR_RECEIVER      2
#define PIN_IR_TRANSMITTER   3 //actually you can't set this pin, it is per default pin three
#define PIN_RF_RECEIVER      4
#define PIN_RF_TRANSMITTER   5
#define PIN_WS2801_DATA      6
#define PIN_WS2801_CLOCK     7
#define PIN_LASER_POINTER    8

#define BUTTON_COUNT         4
#define BUTTON_DELAY        50

#define PIN_TRIGGER                    A0
#define PIN_RELOAD                     A1
#define PIN_CHANGE_MARKER_OR_TEAM      A2
#define PIN_ACTIVATE_SHIELD_OR_RESPAWN A3

#define TRIGGER              0
#define RELOAD               1
#define MARKER_OR_TEAM       2 //while Button Trigger is pressed also change team
#define SHIELD_OR_RESPAWN    3 //while Button Trigger is pressed also respawn

#define LIGHT_STRIKE_DATA_LENGTH    32
#define LIGHT_STRIKE_RAW_LENGTH     66
#define LIGHT_STRIKE_HEADER_MARK  6750
#define LIGHT_STRIKE_HEADER_SPACE    0
#define LIGHT_STRIKE_MARK_ONE      900
#define LIGHT_STRIKE_MARK_ZERO     900
#define LIGHT_STRIKE_SPACE_ONE    3700
#define LIGHT_STRIKE_SPACE_ZERO    900
#define LIGHT_STRIKE_KHZ            38
#define LIGHT_STRIKE_USE_STOP     true
#define LIGHT_STRIKE_MAX_EXTENT      0

#define MARKER_COUNT   9
#define START_MARKER   0
#define TEAM_COUNT     4
#define START_TEAM     1
#define LIGHT_UP_LASERPOINTER 1
#define LIGHT_UP_LED 1
#define LIGHT_UP_LED_HIT 500

#define ACTION_MARKER_CHANGE_WAIT_TIME 2000
#define ACTION_TEAM_CHANGE_WAIT_TIME 2000

#define MAX_ENERGY   120

#define WHITE  0xFFFFFF
#define BLACK  0x000000

#define BLUE   0x000077
#define RED    0x770000
#define YELLOW 0x777700
#define GREEN  0x007700

//as we are using the internal pull up resistors, a key press is a low signal and a release key is a high signal, i will define these compiler variables to make the code easier to read
#define KEY_PRESSED LOW
#define KEY_RELEASED HIGH

IRrecv receiver(PIN_IR_RECEIVER);
IRsend transmitter;
IRdecodeBase decoder;
U8GLIB_SH1106_128X64 u8g(U8G_I2C_OPT_NONE);  
Adafruit_WS2801 strip = Adafruit_WS2801(5, PIN_WS2801_DATA, PIN_WS2801_CLOCK);


struct Buttons {
	boolean currentReading;
	boolean lastReading;
	unsigned long timeLastReadingChanged;
	
	unsigned int delayButtonDown;
	unsigned int delayButtonPressed;

	boolean buttonDown;
	boolean buttonPressed;
};

Buttons buttons[] = {
	{ KEY_RELEASED, KEY_RELEASED, 0, 0, 0, false, false}, //Trigger 
	{ KEY_RELEASED, KEY_RELEASED, 0, 0, 0, false, false}, //Reload
	{ KEY_RELEASED, KEY_RELEASED, 0, 0, 0, false, false}, //Marker
	{ KEY_RELEASED, KEY_RELEASED, 0, 0, 0, false, false}  //Shield
};

struct Teams {
	unsigned int code;
	String name;
	uint32_t color;
};

const Teams teams[] = {
	{ 0x0700, "Blue", BLUE },
	{ 0x0400, "Red", RED },
	{ 0x0500, "Yellow", YELLOW },
	{ 0x0600, "Green", GREEN }
};

struct Markers {
	unsigned int code;
	char type;
	boolean enabled;
	String name;
	int damage;
	int charges;
	int bounceDelay;
	int continuesDelay;
	int reloadTime;
};

const Markers markers[] = {
  
  { 0x0102, 'P', true,  "Laserstrike",    -10, 12,  175,  250, 1750 }, 
  { 0x0202, 'P', true,  "Stealthstrike",  -10, 12,  250,  250, 1750 }, 
  { 0x0303, 'P', true,  "Pulsestrike",    -15, 10,  500,  500, 2000 },
  { 0x0406, 'P', true,  "Sonicstrike",    -40,  8, 1000, 1000, 2500 },
  
  { 0x0502, 'R', true,  "Laserstrike",    -10, 24,  200,  250, 1750 },
  { 0x0602, 'R', true,  "Stealthstrike",  -10, 24,  200,  250, 1750 },
  { 0x0703, 'R', true,  "Pulsestrike",    -15, 16,  200,  500, 2500 },
  { 0x0806, 'R', true,  "Railstrike",     -30,  8, 1000, 1000, 4000 },
  { 0x0908, 'R', true,  "Sonicstrike",    -40,  6, 1250, 1250, 5500 },
/*
  { 0x0E18, 'S', false, "Bomb",          -120,  1, 5000, 5000, 30000 },
  { 0x0A0C, 'S', false, "Optic",          -60,  2, 5000, 5000, 30000 },
  { 0x0F08, 'S', false, "Sentry",         -40,  3, 5000, 5000, 30000 },
  { 0x0B12, 'S', false, "Refractor",      -60,  2, 5000, 5000, 30000 },
  { 0x0C03, 'S', false, "Auto Strike",    -15,  8, 5000, 5000, 30000 },
  { 0x0D06, 'S', false, "Launcher",       -30,  4, 5000, 5000, 30000 },
  { 0x0800, 'S', false, "Medic",          +40,  3, 5000, 5000, 30000 }
*/
};

unsigned int currentTeam = START_TEAM;
unsigned int currentMarker = START_MARKER;
unsigned int currentEnergy = MAX_ENERGY;
unsigned int currentCharge = markers[START_MARKER].charges;

boolean updateDisplay = true;

unsigned long lastHit = 0;
unsigned long msSinceLastHit = 0;
unsigned long msSinceLastShot = 0;
unsigned long msSinceLastTick = 0;
unsigned long msSinceLastAction = 0;

unsigned int waitTime = 0;

unsigned int hitByCode = 0x0000;
String hitByName = "";
uint32_t hitByColor = 0xFFFFFF;

void setup() {
	receiver.enableIRIn();
	Serial.begin(9600);
	strip.begin();
	strip.show();
	setupButtons();
	start();
}

void setupButtons() {
	pinMode(PIN_TRIGGER, INPUT_PULLUP);
	pinMode(PIN_RELOAD, INPUT_PULLUP);
	pinMode(PIN_CHANGE_MARKER_OR_TEAM, INPUT_PULLUP);
	pinMode(PIN_ACTIVATE_SHIELD_OR_RESPAWN, INPUT_PULLUP);
	
	for (byte i = 0; i < BUTTON_COUNT; i++) {
		buttons[i].delayButtonDown = BUTTON_DELAY;
		buttons[i].delayButtonPressed = BUTTON_DELAY * 2;
	}	
}

void evaluateButtons() {
	for (byte i = 0; i < BUTTON_COUNT; i++) {
		buttons[i].lastReading = buttons[i].currentReading;
	}
	
	buttons[0].currentReading = digitalRead(PIN_TRIGGER);
	buttons[1].currentReading = digitalRead(PIN_RELOAD);
	buttons[2].currentReading = digitalRead(PIN_CHANGE_MARKER_OR_TEAM);
	buttons[3].currentReading = digitalRead(PIN_ACTIVATE_SHIELD_OR_RESPAWN);

	for (byte i = 0; i < BUTTON_COUNT; i++) {
		if (buttons[i].lastReading != buttons[i].currentReading) {
			buttons[i].timeLastReadingChanged = millis();
		} else if (millis() > (buttons[i].timeLastReadingChanged + buttons[i].delayButtonDown)) {
			if (buttons[i].currentReading == KEY_PRESSED) {
				buttons[i].buttonDown = true;
			} else {
				buttons[i].buttonDown = false;
			}
		} 
		if (millis() > (buttons[i].timeLastReadingChanged + buttons[i].delayButtonPressed)) {
			if (buttons[i].currentReading == KEY_PRESSED) {
				buttons[i].buttonDown = false;
				buttons[i].buttonPressed = true;
			} else {
				//Probably not really neccessary
				buttons[i].buttonDown = false;
				buttons[i].buttonPressed = false;
			}
		}
	}
}

void setLEDColor(uint32_t c) {
	for (unsigned int i = 0; i < strip.numPixels(); i++) {
		strip.setPixelColor(i, c);
	}
	strip.show();
}

void start() {
	setTeam(START_TEAM);
	setMarker(START_MARKER);
	refreshDisplayValues();
}

void refreshDisplayValues() {
	if (updateDisplay == true) {
		updateDisplay = false;

		u8g.firstPage();
		do {
			u8g.setFont(u8g_font_6x10);
			u8g.setPrintPos(0, 10);
			u8g.print(teams[currentTeam].name);
			
			String temp = markers[currentMarker].name;
			u8g.setFont(u8g_font_5x7);
			u8g.drawStr(6, 25, F("Charges"));
			u8g.drawStr(78, 25, F("Energy"));
			
			u8g.setPrintPos((128 - ((temp.length() * 6) + 1)), 10);
			u8g.print(temp);
			u8g.drawStr(34, 62, F("Hit by "));
			u8g.setPrintPos(69, 62);
			u8g.print(hitByName);
			u8g.setFont(u8g_font_9x15);
		
			if (markers[currentMarker].type == 'P') {
				u8g.setPrintPos(14, 38);
				u8g.print("o");
				u8g.setPrintPos(21, 38);
				u8g.print("o");
			} else {
				u8g.setPrintPos(0, 38);
				u8g.print(currentCharge);
				u8g.drawStr(18, 38, "/");
				u8g.setPrintPos(27, 38);
				u8g.print(markers[currentMarker].charges);  
			}

			u8g.setPrintPos(60, 38);
			u8g.print(currentEnergy);
			u8g.setPrintPos(87, 38);
			u8g.print("/120");
		} while (u8g.nextPage());
	}
}


void loop() {
	//Serial.println(millis()-msSinceLastTick);
	msSinceLastTick = millis();
	evaluateButtons();

	if (currentEnergy > 0) {
		if (receiver.GetResults(&decoder)) {
			decoder.decodeGeneric(LIGHT_STRIKE_RAW_LENGTH, 
			 					  LIGHT_STRIKE_HEADER_MARK, 
							      LIGHT_STRIKE_HEADER_SPACE,
								  //For use with IRLib-Light 
								  //LIGHT_STRIKE_MARK_ZERO, 
								  //For use with Standard IRLib
								  0, 
								  LIGHT_STRIKE_MARK_ZERO, 
								  LIGHT_STRIKE_SPACE_ONE, 
								  LIGHT_STRIKE_SPACE_ZERO);
			lastHit = decoder.value;
			receiver.resume();		
				
			hitByCode = getTeamCodeFromHit(lastHit);
			hitByColor = getTeamColorByCode(hitByCode);
			hitByName = getTeamNameByCode(hitByCode);
		
		
			if (hitByCode != teams[currentTeam].code) {
				setLEDColor(hitByColor);
				currentEnergy = currentEnergy + getMarkerDamageByCode(getMarkerCodeFromHit(lastHit));
				lastHit = 0;
				msSinceLastHit = millis();
				updateDisplay = true;
			}
		}
		/* Only let the user do anything, if the last action has ended */
		if (millis() > (msSinceLastAction + waitTime)) {		  
			/* Trigger Action */
			if (buttons[TRIGGER].buttonDown == true) {
				if (millis() > (msSinceLastShot + markers[currentMarker].bounceDelay)) {
					Serial.println("Bounce");
					waitTime = markers[currentMarker].bounceDelay;
					shot(teams[currentTeam].code, markers[currentMarker].code);
				}
			} else if (buttons[TRIGGER].buttonPressed == true) {
				if (millis() > (msSinceLastShot + markers[currentMarker].continuesDelay)) {
					Serial.println("Continues");
					shot(teams[currentTeam].code, markers[currentMarker].code);
				}
			}
			
			/* Reload Action */
			if (buttons[RELOAD].buttonDown == true) {
				reload();
			}
			
			/* Change Marker Action */
			if (buttons[MARKER_OR_TEAM].buttonDown == true) {
				changeMarker();
			}
			
			/* Change Team Action */
			if (buttons[TRIGGER].buttonPressed == true && buttons[TRIGGER].timeLastReadingChanged > 2000 && buttons[MARKER_OR_TEAM].buttonPressed == true && buttons[MARKER_OR_TEAM].timeLastReadingChanged > 2000) {
				changeTeam();
			}
		}
	} else {
		if ((buttons[MARKER_OR_TEAM].buttonDown == true || buttons[MARKER_OR_TEAM].buttonPressed == true) && (buttons[SHIELD_OR_RESPAWN].buttonDown == true || buttons[SHIELD_OR_RESPAWN].buttonPressed == true)) {
			respawn();
		}
	}
	refreshDisplayValues();
	refreshLights();
}

void refreshLights() {
	if (millis() > (msSinceLastHit + LIGHT_UP_LED_HIT)) {
		setLEDColor(teams[currentTeam].color);
		if (millis() > (msSinceLastShot + LIGHT_UP_LED)) {
			setLEDColor(teams[currentTeam].color);
		}
	}
	if (millis() > (msSinceLastShot + LIGHT_UP_LASERPOINTER)) {
		digitalWrite(PIN_LASER_POINTER, LOW);
	}
}

void shot(long teamCode, int markerCode) {
	if (currentCharge > 0) {
		transmitter.sendGeneric(teamCode + markerCode,
								LIGHT_STRIKE_DATA_LENGTH,
								LIGHT_STRIKE_HEADER_MARK,
								LIGHT_STRIKE_HEADER_SPACE,
								LIGHT_STRIKE_MARK_ONE,
								LIGHT_STRIKE_MARK_ZERO,
								LIGHT_STRIKE_SPACE_ONE,
								LIGHT_STRIKE_SPACE_ZERO,
								LIGHT_STRIKE_KHZ,
								LIGHT_STRIKE_USE_STOP,
								LIGHT_STRIKE_MAX_EXTENT);
		receiver.enableIRIn();
		
		msSinceLastShot = millis();
		
		currentCharge--;
		
		if (currentCharge == 0 && markers[currentMarker].type == 'P') {
			reload();
		} 
		
		updateDisplay = true;
		digitalWrite(PIN_LASER_POINTER, HIGH);
		setLEDColor(WHITE);
	}
}

void action() {
	waitTime = markers[currentMarker].reloadTime;
	msSinceLastAction = millis();
	updateDisplay = true;
}

void reload() {
	currentCharge = markers[currentMarker].charges;
	action();  
}

void changeTeam() {
	if (currentTeam + 1 == TEAM_COUNT) {
		setTeam(0);
	} else {
		setTeam(currentTeam + 1);
	}
	action();
}

void changeMarker() {
  if (currentMarker + 1 == MARKER_COUNT) {
    setMarker(0);
  } else {
    setMarker(currentMarker + 1);
  }
  action();
}

void respawn() {
	currentEnergy = MAX_ENERGY;
	start();
}

void setTeam(int team) {
	currentTeam = team;
	setLEDColor(teams[team].color);
}

void setMarker(int marker) {
	currentMarker = marker;
}

String getTeamNameByCode(long code) {
	for (byte i = 0; i < TEAM_COUNT; i++) {
		if (code == teams[i].code) {
			return teams[i].name;
		}
	}
	return "";
}

uint32_t getTeamColorByCode(long code) {
	for (byte i = 0; i < TEAM_COUNT; i++) {
		if (code == teams[i].code) {
			return teams[i].color;
		}
	}
	return 0x0000;
}


int getMarkerDamageByCode(unsigned int code) {
	for (int i = 0; i < MARKER_COUNT; i++) {
		if (code == markers[i].code) {
			return markers[i].damage;
		}
	}
	return 0;
}

String getMarkerNameByCode(unsigned int code) {
	for (int i = 0; i < MARKER_COUNT; i++) {
		if (code == markers[i].code) {
			return markers[i].name;
		}
	}
	return "";
}

unsigned int getTeamCodeFromHit(long shotValue) {
	return shotValue >> 16;
}

unsigned int getMarkerCodeFromHit(long shotValue) {
	return shotValue;
}
