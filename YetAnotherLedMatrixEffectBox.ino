/**************************************************************************

(yet another) LED Matrix Effect Box 

Created by Istvan Klezli, 2022-Feb-05

MIT License

Copyright (c) 2022 Istvan Klezli

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is furnished
to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

 **************************************************************************/

/*
HW components
- ESP8266 board, e.g. WEMOS D1 mini (clone)
- 1x 0,91" OLED I2C Display Module SSD1306 128x32
- 1x Rotary encoder
- 24x RGB LED WS2812 or compatible
- power connector, e.g. USB micro socket
Note: Current needed for the 24 LEDs (all R, G, B LEDs permanent ON!) 
+ ESP8266 is ca. 1200mA!

HW wiring (in case of WEMOS D1 mini board, ESP8266-based)
* A0 (ADC0)		- n.c.
* D0 (GPIO16)	- n.c.
* D5 (GPIO14)	- rotary encoder in1
* D6 (GPIO12)	- rotary encoder in2
* D7 (GPIO13)	- rotary encoder button
* D8 (GPIO15)	- n.c.
* TX (GPIO1)	- n.c.
* RX (GPIO3)	- n.c.
* D1 (GPIO5)	- OLED 128x32 I2C SCL
* D2 (GPIO4)	- OLED 128x32 I2C SDA
* D3 (GPIO0)	- WS2812 neopixel LED
* D4 (GPIO2)	- n.c.
Note: you can use more LEDs, other microcontroller or display
if you adapt the SW. Do not forget to consider the power consumption!

Used Software Libraries (hope listed all of them)
- Adafruit_SSD1306.h
- Adafruit_GFX.h
- Adafruit_NeoPixel.h
- RotaryEncoder by Matthias Hertel
- Wire
- ESP8266WiFi.h + ArduinoOTA.h (if enabled, see OTA_UPDATE_ACTIVE_TIME_IN_MINS)

Software:
...hmmm... improve/refactor/adapt/extend it if you have free time, thanks!
I wanted to provide just one file (*.ino), however, this source could be
easily slit up into small C++ modules/libraries.
Do not forget to add your router's name and password to wlan_settings.h
if you want to use OTA (over the air) update!

Maybe you would like to fork the code and implement a fire, matrix
(falling symbols) or a fancy rainbow effect? Or maybe implement a random
effect sequence mode? Let's go!
*/


// C++ stuff
#include <vector>
#include <tuple>
#include <string>

// Arduino libraries
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_NeoPixel.h>
#include <RotaryEncoder.h>

// IMPORTANT:
// OTA update is possible in the first 10 minutes after startup.
// 0 means OTA and also the wifi functions are disabled!
#define OTA_UPDATE_ACTIVE_TIME_IN_MINS	10

#if (OTA_UPDATE_ACTIVE_TIME_IN_MINS > 0)
#include <ESP8266WiFi.h>
#include <ArduinoOTA.h>
//{ WLAN CONSTANTS, VARIABLES
#include "wlan_settings.h" // THIS MUST BE CUSTOMIZED!!!
// The unit will habve the following name LEDBOARD6x4_<MAC-address>
String strUniqueDeviceName = PRODUCT_NAME + String(ESP.getChipId(), HEX);
#define MAC_ADDR_BYTES                6
uint8_t G_macAddress[MAC_ADDR_BYTES] = {0};
IPAddress G_IpAddr;
//}
#endif

//{ OLED Display related declarations
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 32 // OLED display height, in pixels
#define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C // See datasheet for Address
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
//}

//{ addressable RGB LED strip related declarations (needs to be customized?)
#define LED_PIN    D3
// How many NeoPixels/WS2812/etc. LEDs are attached to the Arduino?
const byte LED_ROWS = 4;
const byte LED_COLS = 6;
constexpr byte LED_COUNT = LED_ROWS*LED_COLS;
Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);
// Argument 1 = Number of pixels in NeoPixel strip
// Argument 2 = Arduino pin number
// Argument 3 = Pixel type flags, add together as needed:
//   NEO_KHZ800  800 KHz bitstream (most NeoPixel products w/WS2812 LEDs)
//   NEO_KHZ400  400 KHz (classic 'v1' (not v2) FLORA pixels, WS2811 drivers)
//   NEO_GRB     Pixels are wired for GRB bitstream (most NeoPixel products)
//   NEO_RGB     Pixels are wired for RGB bitstream (v1 FLORA pixels, not v2)
//   NEO_RGBW    Pixels are wired for RGBW bitstream (NeoPixel RGBW products)
const uint32_t MAX_HUE = 65535L;
//}

//{ Rotary encoder related declarations
// input signals from Rotary Encoder
#define PIN_IN1 D6
#define PIN_IN2 D5
#define BUTTON_PIN D7
// Setup the RotaryEncoder
RotaryEncoder encoder(PIN_IN1, PIN_IN2, RotaryEncoder::LatchMode::FOUR3);
//}

// forward declarations
void printMsg(String msg, bool bBigFont = false);
void printMenu(String strMenuText, String strParamName, String strParamValue);



/// --------------------------------------------------------------------------
/// Base class for handling a parameter of a light effect
class SingleParamBase
{
public:
	SingleParamBase(String name, String unit, int iInitialValue, bool bRotateThrough)
		: m_srtParamName{name},
		m_srtParamUnit{unit},
		m_iValue{iInitialValue},
		m_rotateThrough{bRotateThrough}
	{
	};

	virtual String GetParamName() const
	{
		return m_srtParamName;
	};
	
	virtual int GetVal() const
	{
		return m_iValue;
	};
	
	virtual String GetParamText()
	{
	}
	
	virtual void Next()
	{
	}
	
	virtual void Prev()
	{
	}
	
protected:
	String m_srtParamName;
	String m_srtParamUnit;
	int m_iValue;
	bool m_rotateThrough;
};


/// --------------------------------------------------------------------------
/// Class for handling a single integer parameter of a light effect
class SingleParamInt : public SingleParamBase
{
public:
	SingleParamInt(String name, String unit, int iInitialValue, bool bRotateThrough, std::tuple<int, int> limits)
		: SingleParamBase(name, unit, iInitialValue, bRotateThrough), m_Limits{limits}
	{
	};

	virtual String GetParamText() override
	{
		return String(m_iValue) + m_srtParamUnit;
	}

	virtual void Next() override
	{
		if (m_iValue >= std::get<1>(m_Limits))
		{
			if (m_rotateThrough)
				m_iValue = std::get<0>(m_Limits);
			return;
		}
		++m_iValue;
	};

virtual void Prev() override
{
	if (m_iValue <= std::get<0>(m_Limits))
	{
		if (m_rotateThrough)
			m_iValue = std::get<1>(m_Limits);
		return;
	}
	--m_iValue;
};

protected:
	std::tuple<int, int> m_Limits;
};


/// --------------------------------------------------------------------------
/// Class for handling a single option parameter of a light effect
class SingleParamNamedOpts : public SingleParamBase
{
public:
	// todo: beside the option name, store also a code (e.g. enum) to the option in order
	// to avoid comparing the option name against literals to device on caller's side which
	// option is set (string compare vs. integer compare)

	SingleParamNamedOpts(String name, String unit, int iInitialValue, bool bRotateThrough, const std::vector<String>& optNames)
		: SingleParamBase(name, unit, iInitialValue, bRotateThrough)
		, m_vecOptNames{optNames}, m_currOpt{m_vecOptNames.begin()}
	{
	};

	virtual String GetParamText() override
	{
		return *m_currOpt;
	};

	virtual void Next() override
	{
		if (m_currOpt == std::prev(m_vecOptNames.end()))
		{
			if (m_rotateThrough)
				m_currOpt = m_vecOptNames.begin();
			return;
		}
		++m_currOpt;
	};

	virtual void Prev() override
	{
		if (m_currOpt == m_vecOptNames.begin())
		{
			if (m_rotateThrough)
				m_currOpt = std::prev(m_vecOptNames.end());
			return;
	  }
	  --m_currOpt;
	};

protected:
	std::vector<String> m_vecOptNames;
	std::vector<String>::iterator m_currOpt;
};


enum LightProgramType
{
	PROG_WHITE,
	PROG_MONOCHROME,
	PROG_TRANSITION,
	PROG_GRADIENT,
	PROG_EFFECT,
};


class LightProg
{
public:
	static const byte COLOR_PARAM_MAX = 59;	// color 0 to 59, i.e. 60 diff. colors.
											// It is better when this number can be divided by
											// LED_COLS and LED_ROWS if Gradient/Rows or 
											// Gradient/Columns effects used (color1 is set to 0
											// and color2 is set to 3/4 or 5/6 of the whole range).
	static const byte SPEED_PARAM_MAX = 10;
	static const byte LEVEL_PARAM_MAX = 100;
	
	LightProg(LightProgramType progCode, String name, std::vector<SingleParamBase*>* params)
	: m_progCode{progCode}
	, m_strProgName{name}
	, m_vecParams{params}
	, m_currParam{m_vecParams->begin()}
	, m_bParamSettingMode{false}
	, m_bLedsAlreadyCalculated{false}
	{
	};

	virtual bool NextParam()
	{
		if (m_currParam == std::prev(m_vecParams->end()))
			return false;

		++m_currParam;
		return true;
	}

	virtual void GoToFirstParam()
	{
		m_currParam = m_vecParams->begin();
	}

	virtual void Next()
	{
		if (m_currParam == m_vecParams->end())
			return;
		(*m_currParam)->Next();
	};

	virtual void Prev()
	{
		if (m_currParam == m_vecParams->end())
			return;
		(*m_currParam)->Prev();
	};

	virtual LightProgramType GetProgCode() const
	{
		return m_progCode;
	}

	virtual void Print() const
	{
		String strParamName =
			(m_bParamSettingMode && m_currParam != m_vecParams->end()
			? (*m_currParam)->GetParamName()
			: "");

		String strParamText =
			(m_bParamSettingMode && m_currParam != m_vecParams->end()
			? (*m_currParam)->GetParamText()
			: "");

	printMenu(m_strProgName, strParamName, strParamText);
	}

	virtual void SetParamSettingMode(bool bParamSettingMode)
	{
		m_bParamSettingMode = bParamSettingMode;
	}

	virtual void Init()
	{
		m_currParam = m_vecParams->begin();
		m_bParamSettingMode = false;
		Off();
	}

	virtual void Start()
	{
		m_bLedsAlreadyCalculated = false;
	}

	virtual byte GetSpeed()
	{
	}
	
	virtual void Tick()
	{
		if (IsTimeToUpdate())
			Update();
	}

	virtual void Off()
	{
		strip.clear();
		strip.show();
	}
	
	virtual void setColor(uint32_t color)
	{
		for(int i=0; i<LED_COUNT; i++)
			strip.setPixelColor(i, color);
		strip.show();
	}
	
	virtual void Update()
	{
	}
	
	virtual bool IsTimeToUpdate()
	{
		static uint32_t lastUpdate = millis();

		byte speed = GetSpeed();
		// speed 0 means no update at all, however, 1st time the scene must be calculated
		if (speed == 0 && m_bLedsAlreadyCalculated)
			return false;

		// speed max. means updates as fast as possible
		if (SPEED_PARAM_MAX == speed)
			return true;

		// otherwise calculate the time delay and invoke update (in Tick()) if the
		// time elapsed since the last update is bigger than the time delay calculated
		// from the speed
		uint32_t timeDelay = (1L << (SPEED_PARAM_MAX - speed)); // 10..0 -> 1024..1
		uint32_t now = millis();
		if (now > lastUpdate + timeDelay)
		{
			lastUpdate = now;
			return true;
		}
		return false;
	}

protected:
	String m_strProgName;
	LightProgramType m_progCode;
	std::vector<SingleParamBase*>* m_vecParams;
	std::vector<SingleParamBase*>::iterator m_currParam;
	bool m_bParamSettingMode;
	uint32_t m_currColor;
	bool m_bLedsAlreadyCalculated;
};

class LightProgWhite : public LightProg
{
public:
	LightProgWhite(LightProgramType progCode, String name, std::vector<SingleParamBase*>* params)
	: LightProg(progCode, name, params)
	{
	};

	virtual bool IsTimeToUpdate() override
	{
		return true;
	}

	virtual void Update() override
	{
		static byte levelParam = 0;
		if (levelParam != m_vecParams->at(0)->GetVal())
		{
			levelParam = m_vecParams->at(0)->GetVal();
			byte level = map(levelParam, 0, LEVEL_PARAM_MAX, 0, 255);
			m_currColor = strip.Color(level, level, level);
			setColor(m_currColor);
			m_bLedsAlreadyCalculated = true;
		}
	}
};

class LightProgMonochrome : public LightProg
{
public:
	LightProgMonochrome(LightProgramType progCode, String name, std::vector<SingleParamBase*>* params)
	: LightProg(progCode, name, params)
	{
	};

	virtual bool IsTimeToUpdate() override
	{
		return true;
	}

	virtual void Update() override
	{
		static byte colorParam = 0;
		static byte levelParam = 0;
		if (colorParam != m_vecParams->at(0)->GetVal()
			|| levelParam != m_vecParams->at(1)->GetVal())
		{
			colorParam = m_vecParams->at(0)->GetVal();
			levelParam = m_vecParams->at(1)->GetVal();
			uint32_t color = map(colorParam, 0, COLOR_PARAM_MAX, 0, MAX_HUE);
			byte saturation = 255;
			byte level = map(levelParam, 0, LEVEL_PARAM_MAX, 0, 255);
			m_currColor = strip.ColorHSV(color, 255, level);
			setColor(m_currColor);
			m_bLedsAlreadyCalculated = true;
		}
	}

};

class LightProgGradient : public LightProg
{
public:

	enum GradientType
	{
		GRADIENT_ROWS,
		GRADIENT_COLS,
		GRADIENT_COLS2,
		GRADIENT_PIXELS,
		GRADIENT_RANDOM,
	};

	LightProgGradient(LightProgramType progCode, String name, std::vector<SingleParamBase*>* params)
	: LightProg(progCode, name, params)
	{
	};

	virtual void Start() override
	{
		m_bLedsAlreadyCalculated = false;
		m_vecRandomColors.clear();
		m_color1 = map(m_vecParams->at(1)->GetVal(), 0, COLOR_PARAM_MAX, 0, MAX_HUE);
		m_color2 = map(m_vecParams->at(2)->GetVal(), 0, COLOR_PARAM_MAX, 0, MAX_HUE);

		if (m_vecParams->at(3)->GetParamText() == "Rows") // oh, get rid of this, see todo in SingleParamNamedOpts
			m_gradType = GRADIENT_ROWS;
		else if (m_vecParams->at(3)->GetParamText() == "Columns")
			m_gradType = GRADIENT_COLS;
		else if (m_vecParams->at(3)->GetParamText() == "Columns 2")
			m_gradType = GRADIENT_COLS2;
		else if (m_vecParams->at(3)->GetParamText() == "Pixels")
			m_gradType = GRADIENT_PIXELS;
		else if (m_vecParams->at(3)->GetParamText() == "Random")
		{
			m_gradType = GRADIENT_RANDOM;
			std::vector<byte> vecColorOrder;
			for (byte pos = 0; pos<LED_COUNT; ++pos)
				vecColorOrder.push_back(pos);

			byte vecSize = 0;
			while ((vecSize = vecColorOrder.size()) > 0)
			{
				byte ledToConfigure = random(vecSize);
				byte order = vecColorOrder.at(ledToConfigure);
				uint32_t color = map(order, 0, LED_COUNT, m_color1, m_color2);
				m_vecRandomColors.push_back(color);
				vecColorOrder.erase(vecColorOrder.begin() + ledToConfigure);
			}
		}
	}

	virtual byte GetSpeed() override
	{
		return m_vecParams->at(0)->GetVal();
	}

	virtual void Update() override
	{
		const uint32_t HUE_INCREMENT = 128;
		
		static uint32_t hueOffset = 0;
		for (int i=0; i<LED_COUNT; ++i)
		{
			switch (m_gradType)
			{
			case GRADIENT_PIXELS:
				m_currColor = map(i, 0, LED_COUNT-1, m_color1, m_color2);
				break;

			case GRADIENT_ROWS:
				m_currColor = map(i / LED_COLS, 0, LED_ROWS-1, m_color1, m_color2);
				break;

			case GRADIENT_COLS:
				m_currColor = map(
					( (i / LED_COLS) % 2 > 0) ? (i % LED_COLS) : LED_COLS - (i % LED_COLS) - 1,
					0, LED_COLS - 1, m_color1, m_color2);
				break;

			case GRADIENT_COLS2:
				m_currColor = map(i % LED_COLS, 0, LED_COLS-1, m_color1, m_color2);
				break;

			case GRADIENT_RANDOM:
				m_currColor = m_vecRandomColors.at(i);
				break;
			}
			strip.setPixelColor(i, strip.gamma32(strip.ColorHSV(m_currColor + hueOffset, 255, 255)));
		}
		m_bLedsAlreadyCalculated = true;
		strip.show();
		hueOffset += HUE_INCREMENT;
	}


protected:
	GradientType m_gradType = GRADIENT_ROWS;
	std::vector<uint32_t> m_vecRandomColors;
	uint32_t m_color1;
	uint32_t m_color2;
};


class LightProgEffects : public LightProg
{
public:

	enum GradientType
	{
		EFFECTS_RAINDROPS,
		EFFECTS_MATRIX,
		EFFECTS_STROBO,
		EFFECTS_FIRE,
	};

	LightProgEffects(LightProgramType progCode, String name, std::vector<SingleParamBase*>* params)
	: LightProg(progCode, name, params)
	{
	};

	virtual void Start() override
	{
		m_bLedsAlreadyCalculated = false;
		m_vecRandomColors.clear();
		m_color1 = map(m_vecParams->at(1)->GetVal(), 0, COLOR_PARAM_MAX, 0, MAX_HUE);
		m_color2 = map(m_vecParams->at(2)->GetVal(), 0, COLOR_PARAM_MAX, 0, MAX_HUE);

		if (m_vecParams->at(3)->GetParamText() == "Raindrops")  // oh, get rid of this, see todo in SingleParamNamedOpts
			m_gradType = EFFECTS_RAINDROPS;
		else if (m_vecParams->at(3)->GetParamText() == "Matrix")
			m_gradType = EFFECTS_MATRIX;
		else if (m_vecParams->at(3)->GetParamText() == "Strobo")
			m_gradType = EFFECTS_STROBO;
		else if (m_vecParams->at(3)->GetParamText() == "Fire")
			m_gradType = EFFECTS_FIRE;
	}

	virtual byte GetSpeed() override
	{
		return m_vecParams->at(0)->GetVal();
	}

	virtual void Update() override
	{
		const byte HUE_INCREMENT = 1;
		const byte RANDOM_LED_SELECTION_PERIOD_MIN = 1;
		const byte RANDOM_LED_SELECTION_PERIOD_MAX = 100;
		
		static uint8_t iRandomLedIdx = 0;
		static uint32_t iCounter = RANDOM_LED_SELECTION_PERIOD_MAX;
		static uint32_t colorBuff[LED_COUNT][2];
		const byte COLOR_BUFF_HUE_IDX = 0;
		const byte COLOR_BUFF_LEVEL_IDX = 1;
		
		static byte hueOffset = 0;
		bool bDecreaseBrigtness = false;
		for (int i=0; i<LED_COUNT; ++i)
		{
			switch (m_gradType)
			{
			case EFFECTS_RAINDROPS:
				if (--iCounter == 0)
				{
					iCounter = random(RANDOM_LED_SELECTION_PERIOD_MIN, RANDOM_LED_SELECTION_PERIOD_MAX);
					iRandomLedIdx = random (LED_COUNT);
					colorBuff[iRandomLedIdx][COLOR_BUFF_LEVEL_IDX] = 255;
					colorBuff[iRandomLedIdx][COLOR_BUFF_HUE_IDX] = map(hueOffset, 0, 255, m_color1, m_color2);
				}
				colorBuff[i][COLOR_BUFF_LEVEL_IDX] = (15L * colorBuff[i][COLOR_BUFF_LEVEL_IDX]) >> 4; // ca. 6% lower brightness
				strip.setPixelColor(i, strip.gamma32(strip.ColorHSV(
					colorBuff[i][COLOR_BUFF_HUE_IDX],
					255,
					colorBuff[i][COLOR_BUFF_LEVEL_IDX])));
				break;

			case EFFECTS_MATRIX:
			case EFFECTS_STROBO:
			case EFFECTS_FIRE:
				printMsg("Please   implement", true);
				break;
			}
		}
		m_bLedsAlreadyCalculated = true;
		strip.show();
		hueOffset += HUE_INCREMENT;
	}


protected:
	GradientType m_gradType = EFFECTS_RAINDROPS;
	std::vector<uint32_t> m_vecRandomColors;
	uint32_t m_color1;
	uint32_t m_color2;
};

class LightProgTransition : public LightProg
{
public:

	enum TransientType
	{
		REPEAT,
		REBOUND,
	};

	LightProgTransition(LightProgramType progCode, String name, std::vector<SingleParamBase*>* params)
	: LightProg(progCode, name, params)
	{
	};

	virtual void Start() override
	{
		m_bLedsAlreadyCalculated = false;
		if (m_vecParams->at(3)->GetParamText() == "Repeat")
			m_transType = REPEAT;
		else
			m_transType = REBOUND;
	}

	virtual byte GetSpeed() override
	{
		return m_vecParams->at(0)->GetVal();
	}

	virtual void Update() override
	{
		const uint32_t MAX_STEPS = 256;
		static uint32_t step = 0;
		static byte color1Param = 0;
		static byte color2Param = 0;
		static uint32_t color1 = 0;
		static uint32_t color2 = 0;

		if (color1Param != m_vecParams->at(1)->GetVal()
			|| color2Param != m_vecParams->at(2)->GetVal())
		{
			color1Param = m_vecParams->at(1)->GetVal();
			color2Param = m_vecParams->at(2)->GetVal();
			color1 = map(color1Param, 0, COLOR_PARAM_MAX, 0, MAX_HUE);
			color2 = map(color2Param, 0, COLOR_PARAM_MAX, 0, MAX_HUE);
		}

		uint32_t currColor = map(step, 0, MAX_STEPS-1, color1, color2);
		m_currColor = strip.ColorHSV(currColor, 255, 255);
		setColor(m_currColor);
		m_bLedsAlreadyCalculated = true;

		step += m_increasing ? 1 : -1;
		if (step == 0 || step == MAX_STEPS)
		{
			if (m_transType == REBOUND)
				m_increasing = !m_increasing;
			else
				step = m_increasing ? 0 : MAX_STEPS-1;
		}

	}


protected:
	TransientType m_transType = REPEAT;
	bool m_increasing = true;
};



class ProgHandler
{
public:
	typedef std::vector<LightProg*> LightProgSet;
	
	enum State
	{
		STATE_INIT,
		STATE_PROGSELECT,
		STATE_PARAMSETTING,
		STATE_WAIT_FOR_START,
		STATE_RUNNING,
	};

	ProgHandler(LightProgSet* pProgSet) 
	: m_pProgSet{pProgSet}
	, m_pCurrLightProg{m_pProgSet->begin()}
	, m_state{STATE_INIT}
	{
		(*m_pCurrLightProg)->SetParamSettingMode(false);
	};

	void Tick()
	{
		if (m_state == STATE_RUNNING)
			(*m_pCurrLightProg)->Tick();
	}

	void Click()
	{
		switch (m_state)
		{
		case STATE_INIT:
		case STATE_RUNNING:
			(*m_pCurrLightProg)->Init();
			m_state = STATE_PROGSELECT;
			Print();
			break;
	
		case STATE_PROGSELECT:
			m_state = STATE_PARAMSETTING;
			(*m_pCurrLightProg)->SetParamSettingMode(true);
			Print();
		break;
	
		case STATE_PARAMSETTING:
			if (!(*m_pCurrLightProg)->NextParam())
			{
				m_state = STATE_WAIT_FOR_START;
				printMsg("Click to   START!", true);
			}
			else
				Print();
			break;
	
		case STATE_WAIT_FOR_START:
			m_state = STATE_RUNNING;
			printMsg("Click to   STOP!", true);
			(*m_pCurrLightProg)->GoToFirstParam();
			(*m_pCurrLightProg)->Start();
			break;
		}
	};

	void Next()
	{
		switch (m_state)
		{
		case STATE_PROGSELECT:
		++m_pCurrLightProg;
		if (m_pCurrLightProg == m_pProgSet->end())
			m_pCurrLightProg = m_pProgSet->begin();
		break;
		case STATE_PARAMSETTING:
		case STATE_RUNNING:
		(*m_pCurrLightProg)->Next();
		break;
		default:
			return;
		}
		Print();
	};

	void Prev()
	{
		switch (m_state)
		{
		case STATE_PROGSELECT:
			if (m_pCurrLightProg == m_pProgSet->begin())
				m_pCurrLightProg = std::prev(m_pProgSet->end());
			else
				--m_pCurrLightProg;
			break;
		case STATE_PARAMSETTING:
		case STATE_RUNNING:
			(*m_pCurrLightProg)->Prev();
			break;
		default:
			return;
		}
		Print();
	};

	LightProgramType GetCurrProg() const
	{
		return (*m_pCurrLightProg)->GetProgCode();
	}
	bool IsToBeConfirmed() const
	{
		return m_state == STATE_WAIT_FOR_START;
	}
	void Print() const
	{
		(*m_pCurrLightProg)->Print();
	};
private:
	LightProgSet* m_pProgSet;
	std::vector<LightProg*>::iterator m_pCurrLightProg;
	State m_state;

};


/// --------------------------------------------------------------------------
/// LIGHT PROGRAMS
/// concerning "new", feel free to change it :)
LightProgWhite programWhite
{
	PROG_WHITE,
	"White",
	new std::vector<SingleParamBase*> {
	new SingleParamInt{"Level", "%", 100, true, std::tuple<int, int>{0,LightProg::LEVEL_PARAM_MAX}},
	}
};

LightProgMonochrome programMonochrome
{
	PROG_MONOCHROME,
	"Monochrom",
	new std::vector<SingleParamBase*> {
	new SingleParamInt{"Color ", "", 0, true, std::tuple<int, int>{0,LightProg::COLOR_PARAM_MAX}},
	new SingleParamInt{"Level", "%", 100, true, std::tuple<int, int>{0,LightProg::LEVEL_PARAM_MAX}},
	}
};

LightProgTransition programTransition
{
	PROG_TRANSITION,
	"Transitn.",
	new std::vector<SingleParamBase*> {
	new SingleParamInt{"Speed", "", 0, false, std::tuple<int, int>{0,LightProg::SPEED_PARAM_MAX}},
	new SingleParamInt{"Color1", "", 0, true, std::tuple<int, int>{0,LightProg::COLOR_PARAM_MAX}},
	new SingleParamInt{"Color2", "", LightProg::COLOR_PARAM_MAX, true, std::tuple<int, int>{0,LightProg::COLOR_PARAM_MAX}},
	new SingleParamNamedOpts{"", "", 0, true, std::vector<String>{"Repeat", "Rebound"}},
	}
};

LightProgGradient programGradient
{
	PROG_GRADIENT,
	"Gradient",
	new std::vector<SingleParamBase*> {
	new SingleParamInt{"Speed", "", 0, false, std::tuple<int, int>{0,LightProg::SPEED_PARAM_MAX}},
	new SingleParamInt{"Color1", "", 0, true, std::tuple<int, int>{0,LightProg::COLOR_PARAM_MAX}},
	new SingleParamInt{"Color2", "", LightProg::COLOR_PARAM_MAX, true, std::tuple<int, int>{0,LightProg::COLOR_PARAM_MAX}},
	new SingleParamNamedOpts{"", "", 0, true, std::vector<String>{"Rows", "Columns", "Columns 2", "Pixels", "Random"}},
	}
};

LightProgEffects programEffects
{
	PROG_EFFECT,
	"Effects",
	new std::vector<SingleParamBase*> {
	new SingleParamInt{"Speed", "", 0, false, std::tuple<int, int>{0,LightProg::SPEED_PARAM_MAX}},
	new SingleParamInt{"Color1", "", 0, true, std::tuple<int, int>{0,LightProg::COLOR_PARAM_MAX}},
	new SingleParamInt{"Color2", "", LightProg::COLOR_PARAM_MAX, true, std::tuple<int, int>{0,LightProg::COLOR_PARAM_MAX}},
	new SingleParamNamedOpts{"", "", 0, true, std::vector<String>{"Raindrops", "Matrix", "Strobo", "Fire"}},
	}
};

ProgHandler::LightProgSet programSet
{
	{
		&programWhite,
		&programMonochrome,
		&programTransition,
		&programGradient,
		&programEffects,
	}
};

ProgHandler PROGRAMHANDLER{&programSet};


/// --------------------------------------------------------------------------
/// Some global scope functions...
 
/// Debounce algorithm for the button part of the
/// rotary encoder. (func. found in internet, originally from Jack G. Ganssle?)
bool debounce()
{
	static uint32_t state = 0;
	state = (state<<1) | digitalRead(BUTTON_PIN) | 0xe0000000;
	return (state == 0xf0000000);
}

/// Displays a text on the OLED display (font size 1 or 2)
void printMsg(String msg, bool bBigFont)
{
	display.clearDisplay();
	display.setTextSize(bBigFont ? 2 : 1);
	display.setTextColor(SSD1306_WHITE, SSD1306_BLACK);
	display.setCursor(0, 0);
	display.println(msg);
	display.display();
}

/// Shows a "structured" text on the OLED display.
/// (a) Either a (menu) text alone will be shown
/// (b) or a (menu) text and a paramter + value will be displayed.
///          (a)                      (b)
/// 1st row: (<menu text>)            <menu text>
/// 2nd row: <empty>                  <param> (<value>)
/// Note: <menu text> (case a) and <value> (case b) are highlighted,
/// i.e. written in inverse mode: black font on top of a white
/// rounded box in order to represent the text in focus.
/// 1st and 2nd line should be max. 9-char long.
/// Optimized for display with 128x32 resolution!
void printMenu(String strMenuText, String strParamName, String strParamValue)
{
	const byte CHAR_X_SPACING = 12;
	const byte CHAR_Y_SPACING = 16;
	byte iMenuTextLen = strMenuText.length();
	byte iParamNameLen = strParamName.length();
	byte iParamValueLen = strParamValue.length();

	display.clearDisplay();
	display.setTextSize(2);

	// No parameter -> just print out the menu text.
	// This branch is used if the menu (e.g. command) can be set.
	if (iParamNameLen == 0 && iParamValueLen == 0)
	{
		// Big font, black text on white background.
		// Draw a white rounded rectangle and put the text above it!
		display.fillRoundRect(	0, 0, // top left corner
								CHAR_X_SPACING * iMenuTextLen + 1.5*CHAR_X_SPACING, 4+CHAR_Y_SPACING, // width, height
								8, // radius
								SSD1306_INVERSE); // color
		display.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
		display.setCursor(4, 3);
		display.println(strMenuText);
		display.display();
		return;
	}

	// Otherwise print out the menu text...
	display.setCursor(0, 0);
	display.setTextColor(SSD1306_WHITE, SSD1306_BLACK);
	display.println(strMenuText);

	// then the Parameter name
	display.setCursor(0, 1+CHAR_Y_SPACING);
	display.println(strParamName);

	// and the Parameter value:
	// Draw a white rounded rectangle and put the text above it!
	display.fillRoundRect(	CHAR_X_SPACING * (iParamNameLen), // top left corner X coord. //+ CHAR_X_SPACING/2,
							CHAR_Y_SPACING, // top left corner Y coord.
							CHAR_X_SPACING * (9 - iParamNameLen) + CHAR_X_SPACING, // width
							2+CHAR_Y_SPACING, // height
							8, // radius
							SSD1306_INVERSE); // color
	display.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
	display.setCursor(CHAR_X_SPACING * iParamNameLen + CHAR_X_SPACING/2, 1+CHAR_Y_SPACING);
	display.println(strParamValue);
	display.display();
}


/// --------------------------------------------------------------------------
/// Setup function of the Arduino environment
void setup()
{
	// initialize the serial port (for debug purposes)
	Serial.begin(115000);

	// initialize the button of the rotary encoder module
	pinMode(BUTTON_PIN, INPUT_PULLUP);

	// initialize the RGB strip
	strip.begin();
	strip.show();
	strip.setBrightness(255);

	// initialize the OLED display
	if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS))
	{
		Serial.println(F("SSD1306 allocation failed"));
		for(;;); // Don't proceed, loop forever
	}
	// Clear the buffer
	display.clearDisplay();

	// initialize the rotarty encoder
	encoder.setPosition(0);

	// This Wifi/OTA code is part of the source 
	// if OTA_UPDATE_ACTIVE_TIME_IN_MINS initialized accordingly
#if (OTA_UPDATE_ACTIVE_TIME_IN_MINS > 0)
		uint32_t iLoopCounter = 0;
		printMsg("Try stored WIFI settings...");
		while (WiFi.status() != WL_CONNECTED)
		{
			delay(500);
			if (++iLoopCounter >= MAX_NUM_OF_WIFI_SSID_TRIAL_TIMEOUT_IN_500MS)
				break;
			yield();
		}

		// try the stored settings
		iLoopCounter = 0;
		if (WiFi.status() != WL_CONNECTED)
		{
			WiFi.mode(WIFI_STA);
			WiFi.config(G_ownStaticIP, G_gatewayIP, G_maskIP);
			WiFi.begin();
			WiFi.persistent(true);
			WiFi.setAutoConnect(true);
			WiFi.setAutoReconnect(true);
			while (WiFi.status() != WL_CONNECTED)
			{
			  delay(500);
			  if (++iLoopCounter >= MAX_NUM_OF_WIFI_SSID_TRIAL_TIMEOUT_IN_500MS)
				break;
			  yield();
			}
		}

		printMsg("Try home network...");
		// try home net
		iLoopCounter = 0;
		if (WiFi.status() != WL_CONNECTED)
		{
			WiFi.begin(ROUTER_SSID, ROUTER_PASSWD);
			while (WiFi.status() != WL_CONNECTED)
			{
			delay(500);
			if (++iLoopCounter >= MAX_NUM_OF_WIFI_SSID_TRIAL_TIMEOUT_IN_500MS)
				break;
			yield();
			}
		}

		if (WiFi.status() != WL_CONNECTED)
		{
			// TRY again... Restart
			printMsg("Will be restared...");
			Serial.println("will be restarted...\n");
			for (uint8_t i = 0; i<6; ++i) // 3 seconds
			delay(500);
			ESP.restart();        // *** RESTART ***
		}

		// Finally, WiFi connection established

		G_IpAddr = WiFi.localIP();
		IPAddress ipGateway = WiFi.gatewayIP();
		String strConnection = "MAC:" + WiFi.macAddress() + "IP: "
			+ G_IpAddr.toString().c_str() + " / Gateway: " + ipGateway.toString().c_str();
		printMsg(strConnection);
		Serial.println("WiFi connected!");
		Serial.println(strConnection);


		// OTA
		ArduinoOTA.setHostname(strUniqueDeviceName.c_str());

		// Port defaults to 3232
		// ArduinoOTA.setPort(3232);

		// Authentication
		ArduinoOTA.setPassword(OTA_PASSWORD);

		ArduinoOTA.onStart([]()
		{
			String type;
			if (ArduinoOTA.getCommand() == U_FLASH)
			type = "sketch";
			else // U_SPIFFS
			type = "filesystem";

			// NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
			//Serial.println("Start updating " + type);
			printMsg("Start updating" + type);
		});

		ArduinoOTA.onEnd([]()
		{
			//Serial.println("\nEnd");
			printMsg("Firmware Update finished!");
		});

		ArduinoOTA.onProgress([](unsigned int progress, unsigned int total)
		{
			int percent = progress / (total / 100);
			if (percent % 5 == 0)
			{
			String msg = "Progress: " + String(percent) + "%";
			printMsg(msg);
			}
		});

		ArduinoOTA.onError([](ota_error_t error)
		{
			//Serial.printf("Error[%u]: ", error);
			printMsg("Error: ");
			if (error == OTA_AUTH_ERROR) printMsg("Auth Failed");
			else if (error == OTA_BEGIN_ERROR) printMsg("Begin Failed");
			else if (error == OTA_CONNECT_ERROR) printMsg("Connect Failed");
			else if (error == OTA_RECEIVE_ERROR) printMsg("Receive Failed");
			else if (error == OTA_END_ERROR) printMsg("End Failed");
			delay(2000);
		});

		ArduinoOTA.begin();
		Serial.println("[INFO] OTA started!");
#else
		printMsg("Select light effect!");
#endif
}


/// --------------------------------------------------------------------------
/// Loop funciton of the Arduino environment
void loop() {
	static uint32_t T_start = millis();
	static int encoderLastPos = 0;

	// update the light effect program
	PROGRAMHANDLER.Tick();

#if (OTA_UPDATE_ACTIVE_TIME_IN_MINS > 0)
	// OTA handling only in the first minutes after program start
	if ((millis() < T_start + OTA_UPDATE_ACTIVE_TIME_IN_MINS * 60 * 1000))
		ArduinoOTA.handle();
#endif

	// hanlde user interactions: click and dial movement
	if (debounce())
		PROGRAMHANDLER.Click();

	encoder.tick();
	int newPos = encoder.getPosition();
	if (encoderLastPos != newPos)
	{
		if (newPos > encoderLastPos)
			PROGRAMHANDLER.Next();
		else
			PROGRAMHANDLER.Prev();

		encoderLastPos = newPos;
	}
}
