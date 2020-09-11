#include <EnergySaving.h>

// PINS
const int scanDetectPin		= 0;		// detects access controller valid scan output
const int ignDetectPin		= 1;		// detects ignition state

const int fobEnablePin		= 2;		// powers up the fob when high
const int fobUnlockPin		= 3;		// pulse to "press" the unlock button
const int fobLockPin		= 4;		// pulse to "press" the lock button


const int fobButtonDelay	= 100;		// number of milliseconds to "hold" the fob button down for simulated presses
const int powerDelay		= 200;		// number of milliseconds to wait for the fob to power up or before power down.

// LEDs
#define LED_OFF				HIGH		// board LEDs in active low configuration
#define LED_ON				LOW			// here to avoid confusion

const int scanLED			= 11;		// blue towards the top - solid when access control signal is present
const int countdownLED		= 12;		// blue next to green LED - When solid, in countdown state
const int fobpowerLED		= 13;		// orange - On solid when ignition signal is present.  Indicates locking/unlocking button presses when ignition is not on



// CONFIG OPTIONS
const bool optUnlock					= true;		// true will unlock when scanned
const bool optLockOnExitScan			= true;		// true will lock if  you scan during shutdown timer
const bool optLockOnTimeout				= true;		// true will lock if the shutdown timer expires without an exit scan
const bool optDoubleLock				= false;	// true will press the lock button twice when locking

const unsigned long optTimeoutMillis 	= 30000;	// Milliseconds until countdown timer expires


// STATES (MAIN)

#define STATE_IDLE				0
#define STATE_COUNTDOWN			1
#define STATE_IGNITION_ON		2

int state = STATE_IDLE;			// stores current main state


// STATES (DEBOUNCE)

#define DB_IDLE					0
#define DB_DEBOUNCING			1
#define DB_DEBOUNCED			2
#define DB_WAIT					3

int dbState  = DB_IDLE;							// stores current debounce state

unsigned long dbStartTime;						// store the millis when the debounce timer starts
const unsigned long dbTimeMillis 	= 100;		// debounce for 100ms


bool scanState;					// stores current scan status

unsigned long shudownStartTime;					// store the millis when the countdown timer starts

EnergySaving lowPower;			// Library for sleep mode.

void setup()
{
	// PIN I/O SETUP
	// INPUTS	
	pinMode(scanDetectPin,	INPUT);
	pinMode(ignDetectPin,	INPUT);

	// LEDS
	pinMode(scanLED,		OUTPUT);
	pinMode(countdownLED,	OUTPUT);
	pinMode(fobpowerLED,	OUTPUT);

	// FOB CONTROL
	pinMode(fobEnablePin,	OUTPUT);
	pinMode(fobLockPin,		OUTPUT);
	pinMode(fobUnlockPin,	OUTPUT);

	// DEFAULT PIN STATES
	digitalWrite(scanLED,			LED_OFF);
	digitalWrite(countdownLED,		LED_OFF);
	digitalWrite(fobpowerLED,		LED_OFF);
	
	digitalWrite(fobEnablePin,		LOW);
	digitalWrite(fobLockPin,		LOW);
	digitalWrite(fobUnlockPin,		LOW);


	// SETUP SLEEP INTERRUPT
	lowPower.begin(WAKE_EXT_INTERRUPT, scanDetectPin, dummy);  
}



void loop()
{
	scanState = debounceScan();	// check to see if we have a valid tag scan
	
	switch (state)
	{
	
		case STATE_IDLE:
		{
			if (scanState)
			{   // we are idle and we got a new scan!
			
				fobEnable(true);		// turn on the fob
				
				delay(powerDelay);		// wait before we do anything else
				
				if (optUnlock)
					fobUnlock();		// unlock if option enabled
				
				// start countdown timer
				shudownStartTime = millis();
				state = STATE_COUNTDOWN;
			
			}
			else
			{ // in IDLE state without a scan, power down
				
				digitalWrite(countdownLED, LED_OFF);
				fobEnable(false);	// turn off the fob

				// bedtime.  Reduces current from 14ma waiting for a scan to 8ma.  
				// Of course the access cotnroller draws way more that that but every bit helps.
				// sits here until an interrupt on the acccess controller input pin starts the main loop back up
				lowPower.standby();  	
			}
		}
		break;
		
		
		case STATE_COUNTDOWN:
		{   // waitng for ignition, a scan, or timer expire
			digitalWrite(countdownLED, LED_ON);
			
			if (millis() - shudownStartTime > optTimeoutMillis)
			{   // optTimeoutMillis has elapsed, no one started the car 
				// and there was no scan, so lock the doors and shut back down
				
				// lock the doors if option is set
				if (optLockOnTimeout)
					fobLock();
				
				// go to idle
				state = STATE_IDLE;
				delay(powerDelay);
				
			}
			else
			{ // still in countdown
				
				if (digitalRead(ignDetectPin))
				{   // ignition has been detected while in countdown, halt countdown and wait for ignition drop
				
					state = STATE_IGNITION_ON;
					digitalWrite(fobpowerLED, LED_ON);					
				}
				else
				{   // no ignition
					if (scanState)
					{   // we have a scan!
						// a scan during a shutdown period is a signal to lock and shut the fob down
						
						if (optLockOnExitScan)
							fobLock();
						
						// go to idle
						state = STATE_IDLE;
						delay(powerDelay);		
					
					}
				}
			}
		}
		break;
		
		case STATE_IGNITION_ON:
		{
			// ignition is on
			// wait for it to turn off.

			digitalWrite(countdownLED, LED_OFF);
			bool ign = digitalRead(ignDetectPin);
			if (ign == false)
			{   // ignition has been turned off

				digitalWrite(fobpowerLED, LED_OFF);
				
				// start shutdown timer
				shudownStartTime = millis();
				state = STATE_COUNTDOWN;
				
			
			}
		}
		break;
	}
}


void fobLock()
{
	// press the fob's lock button
	digitalWrite(fobpowerLED, LED_ON);
	
	digitalWrite(fobLockPin, HIGH);
	delay(fobButtonDelay);
	digitalWrite(fobLockPin, LOW);

	if(optDoubleLock)
	{
		delay(fobButtonDelay);
		
		digitalWrite(fobLockPin, HIGH);
		delay(fobButtonDelay);
		digitalWrite(fobLockPin, LOW);
	}
		
	digitalWrite(fobpowerLED, LED_OFF);
}

void fobUnlock()
{
	// press the fob's unlock button
	digitalWrite(fobpowerLED, LED_ON);
	
	digitalWrite(fobUnlockPin, HIGH);
	delay(fobButtonDelay);
	digitalWrite(fobUnlockPin, LOW);
	
	digitalWrite(fobpowerLED, LED_OFF);
}

void fobEnable(bool p)
{
	if (p)
	{
	//	digitalWrite(fobpowerLED, LED_ON);
		digitalWrite(fobEnablePin, HIGH);
	}
	else
	{
		digitalWrite(fobEnablePin, LOW);
	//	digitalWrite(fobpowerLED, LED_OFF);
	}

}

bool debounceScan()
{   // debounces input from access controller and returns debounced pin state. Will not return true again until the input drops after a debounce.

	if (digitalRead(scanDetectPin))
	{   // input is high

		digitalWrite(scanLED, LED_ON);
	
		switch (dbState)
		{
			case DB_IDLE:
			{   // high and idle, start debouncing timer
				dbState = DB_DEBOUNCING;
				
				dbStartTime = millis();         // record ticks
			}
			break;
			
			case DB_DEBOUNCING:
			{   // check the deboucning timer
			
				if (millis() - dbStartTime > dbTimeMillis)
				{   // dbTimeMillis has elapsed without a state change
					dbState = DB_DEBOUNCED;
				}
			}
			break;
			
			case DB_DEBOUNCED:
			{   // already debounced. notify the calling function (below)
				
			}
			break;
			
			case DB_WAIT:
			{   // already debounced and notified calling function, just waiting for the signal to drop.
				// nothing to do.
			}
			break;
		
		}
		
	}
	else
	{   // input low. no longer debounced
		dbState = DB_IDLE;
		digitalWrite(scanLED, LED_OFF);
	}

	if (dbState == DB_DEBOUNCED)
	{ 	// we have debounced the input, notify the calling function
		// changing to DB_WAIT state mean there will only be exaclty one call to this function 
		// that returns true until the input drops and we go back to idle.	
		// So even if someone holds their tag on the reader, it will only count exactly 1 read.
		
		dbState = DB_WAIT;  
		return true;
	}
	else
		return false;
}


void dummy(void)  //interrupt routine (isn't necessary to execute any tasks in this routine
{

}
