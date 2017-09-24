#include <funkeymonkey/funkeymonkeymodule.h>
#include <funkeymonkey/uinputdevice.h>

#include <iostream>
#include <thread>
#include <chrono>
#include <mutex>
#include <condition_variable>
#include <regex>
#include <fstream>
#include <algorithm>
#include <cctype>
#include <unordered_map>
#include <functional>
#include <array>
#include <stdlib.h> 

enum Role { ROLE_LEFT_NUB, ROLE_RIGHT_NUB, ROLE_KEYBOARD, ROLE_GPIO };

struct LeftRight {
	bool left	= false;
	bool right	= false;
	bool pressed	= false;
};

struct Scripts {
	std::string normal	= "";
	std::string Fn		= "";
	std::string Shift	= "";
	std::string FnShift	= "";
	std::string Alt		= "";
	std::string Ctrl	= "";
	std::string AltCtrl	= "";
	std::string FnAlt	= "";
	std::string FnCtrl	= "";
	std::string ShiftAlt	= "";
	std::string ShiftCtrl	= "";
	std::string FnShiftAlt	= "";
	std::string FnShiftCtrl	= "";
};

static constexpr unsigned int FIRST_KEY = KEY_RESERVED;
static constexpr unsigned int LAST_KEY = KEY_UNKNOWN;

template<int FIRST_KEY, int LAST_KEY> class KeyBehaviors {
public:
	void passthrough(unsigned int code);
	void map(unsigned int code, unsigned int result);
	void gphat(unsigned int code, unsigned int gamepad);
	void gpmap(unsigned int code, unsigned int gamepad);
	void gpmap2(unsigned int code, unsigned int gamepad, bool* self, LeftRight* pair);
	void altmap(unsigned int code, bool* flag, unsigned int regular, unsigned int alternative);
	void complex(unsigned int code, std::function<void(int)> function);
	void script(unsigned int code, Scripts *s);
	void handle(unsigned int code, int value);

private:
	struct KeyBehavior {
		KeyBehavior() : type(PASSTHROUGH), mapping(0), function() {}
		enum Type  { PASSTHROUGH, MAPPED, ALTMAPPED, COMPLEX, GPMAPPED,GPMAP2, GPHAT, SCRIPT };
		Type type;
		int mapping;
		int alternative;
		bool* flag;
		LeftRight* lr;
		Scripts *scripts;
		std::function<void(int)> function;
	};

	static constexpr unsigned int NUM_KEYS = LAST_KEY - FIRST_KEY + 1;
	std::array<KeyBehavior, NUM_KEYS> behaviors;
};

struct Mouse {
	// Any thread using device must hold mutex
	UinputDevice device;
	
	// May be changed from any thread at any time
	int dx;
	int dy;
	int dwx;
	int dwy;
	
	// Used to signal changes in above values, uses mutex
	std::condition_variable signal;
	
	// Mouse device mutex
	std::mutex mutex;
};


struct Settings {
	enum NubAxisMode {
		UNKNOWN_NUB_AXIS_MODE,
		MOUSE_X, MOUSE_Y, MOUSE_BTN,
		SCROLL_X, SCROLL_Y
	};
	enum NubClickMode {
		UNKNOWN_NUB_CLICK_MODE,
		NUB_CLICK_LEFT, NUB_CLICK_RIGHT,
		MOUSE_LEFT, MOUSE_RIGHT
	};
	
	NubAxisMode leftNubModeX = MOUSE_X;
	NubAxisMode leftNubModeY = MOUSE_Y;
	NubAxisMode rightNubModeX = MOUSE_BTN;
	NubAxisMode rightNubModeY = SCROLL_Y;
	NubClickMode leftNubClickMode = MOUSE_LEFT;
	NubClickMode rightNubClickMode = MOUSE_RIGHT;
	
	// May be changed from any thread at any time
	int joyDeadzone = 10;
	int mouseDeadzone = 20;
	int mouseSensitivity = 40;
	int mouseWheelDeadzone = 100;
	int mouseClickDeadzone = 100;
	
	bool exportGamepad = true;
	bool exportMouse   = true;
	bool exportKeypad  = true;

	std::string configFile;
	Scripts brightness;
};

using NubAxisModeMap = std::unordered_map<std::string, Settings::NubAxisMode>;
NubAxisModeMap const NUB_AXIS_MODES = {
	{ "mouse_x", Settings::MOUSE_X },
	{ "mouse_y", Settings::MOUSE_Y },
	{ "mouse_btn", Settings::MOUSE_BTN },
	{ "scroll_x", Settings::SCROLL_X },
	{ "scroll_y", Settings::SCROLL_Y }
};

using NubClickModeMap = std::unordered_map<std::string, Settings::NubClickMode>;
NubClickModeMap const NUB_CLICK_MODES = {
	{ "nub_click_left", Settings::NUB_CLICK_LEFT },
	{ "nub_click_right", Settings::NUB_CLICK_RIGHT },
	{ "mouse_left", Settings::MOUSE_LEFT },
	{ "mouse_right", Settings::MOUSE_RIGHT }
};

void handleArgs(char const** argv, unsigned int argc, Settings& settings);
void loadConfig(std::string const& filename, Settings& settings);
Settings::NubAxisMode parseNubAxisMode(std::string const& str);
Settings::NubClickMode parseNubClickMode(std::string const& str);
void handleNubAxis(Settings::NubAxisMode mode, int value, Mouse* mouse, UinputDevice* gamepad, Settings const& settings);
void handleNubClick(Settings::NubClickMode mode, int value, Mouse* mouse, UinputDevice* gamepad, Settings const& settings);

// Mouse movement/scroll thread handler
void handleMouse(Mouse* mouse, Settings* settings, bool* stop);

struct {
	bool stop = false;
	UinputDevice* gamepad = nullptr;
	UinputDevice* keyboard = nullptr;
	KeyBehaviors<FIRST_KEY, LAST_KEY>* behaviors = nullptr;
	Mouse* mouse = nullptr;
	std::thread mouseThread;
	Settings settings;
	LeftRight Fn;
	LeftRight Alt;
	LeftRight Shift;
	LeftRight Ctrl;
	int hatx = 0;
	int haty = 0;
	int mouseBtn = 0;
} global;


void init(char const** argv, unsigned int argc) {
	std::vector<unsigned int> keycodes;
	for(unsigned int i = FIRST_KEY; i <= LAST_KEY; ++i) {
		keycodes.push_back(i);
	}
	
	global.keyboard = new UinputDevice("/dev/uinput", BUS_USB, "pyraInput keyboard", 1, 1, 1, {
		{ EV_KEY, keycodes }
	});

	global.behaviors = new KeyBehaviors<FIRST_KEY, LAST_KEY>();
	global.behaviors->gphat(KEY_UP,		BTN_DPAD_UP);
	global.behaviors->gphat(KEY_DOWN,	BTN_DPAD_DOWN);
	global.behaviors->gphat(KEY_LEFT,	BTN_DPAD_LEFT);
	global.behaviors->gphat(KEY_RIGHT,	BTN_DPAD_RIGHT);
	global.behaviors->gpmap2(KEY_LEFTALT,	BTN_START, &global.Alt.left, &global.Alt);
	global.behaviors->gpmap2(KEY_LEFTCTRL,	BTN_SELECT, &global.Ctrl.left, &global.Ctrl);
	global.behaviors->gpmap(KEY_HOME,	BTN_A);
	global.behaviors->gpmap(KEY_END,	BTN_B);
	global.behaviors->gpmap(KEY_PAGEDOWN,	BTN_X);
	global.behaviors->gpmap(KEY_PAGEUP,	BTN_Y);
	global.behaviors->gpmap2(KEY_RIGHTSHIFT,BTN_TL, &global.Shift.right, &global.Shift);
	global.behaviors->gpmap2(KEY_RIGHTCTRL,	BTN_TR, &global.Ctrl.right, &global.Ctrl);
	global.behaviors->gpmap2(KEY_RIGHTALT,	BTN_TR2, &global.Alt.right, &global.Alt);
	global.behaviors->gpmap(KEY_INSERT,	BTN_C);  //(I)
	global.behaviors->gpmap(KEY_DELETE,	BTN_Z);  //(II)
	
	global.behaviors->complex(KEY_LEFTSHIFT, [](int value) {
		global.Shift.left = (value==1);
		global.Shift.pressed = global.Shift.left || global.Shift.right;
		global.keyboard->send(EV_KEY, KEY_LEFTSHIFT, value);
	});
	global.behaviors->complex(KEY_RIGHTMETA, [](int value) {
		global.Fn.right = (value==1);
		global.Fn.pressed = global.Fn.left || global.Fn.right;
		if (global.settings.exportGamepad) {
			global.gamepad->send(EV_KEY, BTN_TL2, value);
			global.gamepad->send(EV_SYN, 0, 0);
		}
	});
	global.behaviors->complex(KEY_LEFTMETA, [](int value) {
		global.Fn.left = (value==1);
		global.Fn.pressed = global.Fn.left || global.Fn.right;
	});

	//TODO: make the alt mapping configurable
	global.behaviors->altmap(KEY_ESC,	&global.Fn.pressed, KEY_ESC,	KEY_SYSRQ);
	global.behaviors->altmap(KEY_PAUSE,	&global.Fn.pressed, KEY_PAUSE,	KEY_SCALE);		global.behaviors->script(KEY_BRIGHTNESSUP, &global.settings.brightness);
	//global.behaviors->altmap(KEY_BRIGHTNESSUP,	&global.Fn.pressed, KEY_BRIGHTNESSUP,	KEY_BRIGHTNESSDOWN);
	global.behaviors->altmap(KEY_F11,	&global.Fn.pressed, KEY_F11,	KEY_F12);
	global.behaviors->altmap(KEY_1,		&global.Fn.pressed, KEY_1,	KEY_F1);
	global.behaviors->altmap(KEY_2,		&global.Fn.pressed, KEY_2,	KEY_F2);
	global.behaviors->altmap(KEY_3,		&global.Fn.pressed, KEY_3,	KEY_F3);
	global.behaviors->altmap(KEY_4,		&global.Fn.pressed, KEY_4,	KEY_F4);
	global.behaviors->altmap(KEY_5,		&global.Fn.pressed, KEY_5,	KEY_F5);
	global.behaviors->altmap(KEY_6,		&global.Fn.pressed, KEY_6,	KEY_F6);
	global.behaviors->altmap(KEY_7,		&global.Fn.pressed, KEY_7,	KEY_F7);
	global.behaviors->altmap(KEY_8,		&global.Fn.pressed, KEY_8,	KEY_F8);
	global.behaviors->altmap(KEY_9,		&global.Fn.pressed, KEY_9,	KEY_F9);
	global.behaviors->altmap(KEY_0,		&global.Fn.pressed, KEY_0,	KEY_F10);
	global.behaviors->altmap(KEY_TAB,	&global.Fn.pressed, KEY_TAB,	KEY_CAPSLOCK);
	global.behaviors->altmap(KEY_Q,		&global.Fn.pressed, KEY_Q,	KEY_MACRO);			// I120 // ok
	global.behaviors->altmap(KEY_W,		&global.Fn.pressed, KEY_W,	KEY_KPCOMMA);			// I129 // ok
	global.behaviors->altmap(KEY_E,		&global.Fn.pressed, KEY_E,	KEY_SETUP);			// I149 // ok
	global.behaviors->altmap(KEY_R,		&global.Fn.pressed, KEY_R,	KEY_DELETEFILE);		// I154 // ok
	global.behaviors->altmap(KEY_T,		&global.Fn.pressed, KEY_T,	KEY_CLOSECD);			// I168 // ok
	global.behaviors->altmap(KEY_Y,		&global.Fn.pressed, KEY_Y,	KEY_ISO);			// I178 // ok
	global.behaviors->altmap(KEY_U,		&global.Fn.pressed, KEY_U,	KEY_MOVE);			// I183 // ok
	global.behaviors->altmap(KEY_I,		&global.Fn.pressed, KEY_I,	KEY_EDIT);			// I184 // ok
	global.behaviors->altmap(KEY_O,		&global.Fn.pressed, KEY_O,	KEY_ALTERASE);			// I230 // ok
	global.behaviors->altmap(KEY_P,		&global.Fn.pressed, KEY_P,	KEY_BASSBOOST);			// I217 // ok
	global.behaviors->altmap(KEY_APOSTROPHE,	&global.Fn.pressed, KEY_APOSTROPHE,	KEY_UWB);	// I247 // ok
	global.behaviors->altmap(KEY_A,		&global.Fn.pressed, KEY_A,	KEY_QUESTION);			// I222 // ok
	global.behaviors->altmap(KEY_S,		&global.Fn.pressed, KEY_S,	KEY_UNKNOWN);			// I248 // ok
	global.behaviors->altmap(KEY_D,		&global.Fn.pressed, KEY_D,	KEY_SOUND);			// I221 // ok
	global.behaviors->altmap(KEY_F,		&global.Fn.pressed, KEY_F,	KEY_HP);			// I219 // ok
	global.behaviors->altmap(KEY_G,		&global.Fn.pressed, KEY_G,	KEY_RO);			// I249- AB11 (89)
	global.behaviors->altmap(KEY_H,		&global.Fn.pressed, KEY_H,	KEY_KPJPCOMMA);			// I250- JPCM (95)
	global.behaviors->altmap(KEY_J,		&global.Fn.pressed, KEY_J,	KEY_YEN);			// I251- AE13 (124)
	global.behaviors->altmap(KEY_K,		&global.Fn.pressed, KEY_K,	KEY_F19);			// I252- FK19 (189)
	global.behaviors->altmap(KEY_L,		&global.Fn.pressed, KEY_L,	KEY_F24);			// I253- FK24 (194)
	global.behaviors->altmap(KEY_COMMA,	&global.Fn.pressed, KEY_COMMA,	KEY_SEMICOLON);
	global.behaviors->altmap(KEY_DOT,	&global.Fn.pressed, KEY_DOT,	KEY_SLASH);
	global.behaviors->altmap(KEY_Z,		&global.Fn.pressed, KEY_Z,	KEY_EQUAL);
	global.behaviors->altmap(KEY_X,		&global.Fn.pressed, KEY_X,	KEY_MINUS);
	global.behaviors->altmap(KEY_C,		&global.Fn.pressed, KEY_C,	KEY_LEFTBRACE);
	global.behaviors->altmap(KEY_V,		&global.Fn.pressed, KEY_V,	KEY_RIGHTBRACE);
	global.behaviors->altmap(KEY_B,		&global.Fn.pressed, KEY_B,	KEY_BACKSLASH);
	global.behaviors->altmap(KEY_N,		&global.Fn.pressed, KEY_N,	KEY_GRAVE);
	global.behaviors->altmap(KEY_M,		&global.Fn.pressed, KEY_M,	195);			// 228- MDSW (195)
	global.behaviors->altmap(KEY_SPACE,	&global.Fn.pressed, KEY_SPACE,	KEY_COMPOSE);
	

	global.gamepad = new UinputDevice("/dev/uinput", BUS_USB, "pyraInput Gamepad", 1, 1, 1, {
		{ EV_KEY, {
			BTN_A, BTN_B, BTN_X, BTN_Y, 
			BTN_TL, BTN_TR, BTN_TL2, BTN_TR2,
			BTN_SELECT, BTN_START, BTN_C, BTN_Z,
			BTN_THUMBL, BTN_THUMBR
		} },
		{ EV_ABS, { ABS_HAT0X, ABS_HAT0Y, ABS_X, ABS_Y, ABS_RX, ABS_RY } }
	});
	global.mouse = new Mouse {
		UinputDevice("/dev/uinput", BUS_USB, "pyraInput Mouse", 1, 1, 1, {
			{ EV_KEY, { BTN_LEFT, BTN_RIGHT } },
	       { EV_REL, { REL_X, REL_Y, REL_HWHEEL, REL_WHEEL } }
		}), 0, 0, 0, 0, {}, {}
	};
	global.mouseThread = std::move(std::thread(handleMouse, global.mouse, &global.settings, &global.stop));

	handleArgs(argv, argc, global.settings);

	if(!global.settings.configFile.empty()) {
		loadConfig(global.settings.configFile, global.settings);
	}
}

void handle(input_event const& e, unsigned int role) {
	switch(e.type) {
	case EV_ABS:
		if (role == ROLE_LEFT_NUB) {
			switch(e.code) {
			case ABS_X:
				if (global.settings.exportGamepad && (e.value>global.settings.joyDeadzone||e.value<-global.settings.joyDeadzone)) {
					global.gamepad->send(EV_ABS, ABS_X, e.value);
					global.gamepad->send(EV_SYN, 0, 0);
				}
				handleNubAxis(global.settings.leftNubModeX, e.value, global.mouse, global.gamepad, global.settings);
				break;
			case ABS_Y:
				if (global.settings.exportGamepad && (e.value>global.settings.joyDeadzone||e.value<-global.settings.joyDeadzone)) {
					global.gamepad->send(EV_ABS, ABS_Y, e.value);
					global.gamepad->send(EV_SYN, 0, 0);
				}
				handleNubAxis(global.settings.leftNubModeY, e.value, global.mouse, global.gamepad, global.settings);
				break;
			default: break;
			};
		} else if (role == ROLE_RIGHT_NUB) {
			switch(e.code) {
			case ABS_X:
				if (global.settings.exportGamepad && (e.value>global.settings.joyDeadzone||e.value<-global.settings.joyDeadzone)) {
					global.gamepad->send(EV_ABS, ABS_RX, e.value);
					global.gamepad->send(EV_SYN, 0, 0);
				}
				handleNubAxis(global.settings.rightNubModeX, e.value, global.mouse, global.gamepad, global.settings);
				break;
			case ABS_Y:
				if (global.settings.exportGamepad && (e.value>global.settings.joyDeadzone||e.value<-global.settings.joyDeadzone)) {
					global.gamepad->send(EV_ABS, ABS_RY, e.value);
					global.gamepad->send(EV_SYN, 0, 0);
				}
				handleNubAxis(global.settings.rightNubModeY, e.value, global.mouse, global.gamepad, global.settings);
				break;
			default: break;
		};
		}
		break;
	case EV_KEY:
		switch(e.code) {
		case BTN_LEFT: // mouse click
		case BTN_RIGHT:
		case BTN_MIDDLE:
			// TODO : configure this
			if (role == ROLE_LEFT_NUB && global.settings.exportMouse)
				global.mouse->device.send(EV_KEY, BTN_LEFT, e.value);
			else if (role == ROLE_RIGHT_NUB && global.settings.exportMouse)
				global.mouse->device.send(EV_KEY, BTN_RIGHT, e.value);
			/*else
				global.mouse->device.send(EV_KEY, e.code, e.value);*/
			global.mouse->device.send(EV_SYN, 0, 0);
			break;
		case BTN_THUMBL:
		case BTN_THUMBR:
			if (role == ROLE_LEFT_NUB && global.settings.exportMouse) {
				std::cout << "left nub click\n";
				handleNubClick(global.settings.leftNubClickMode, e.value, global.mouse, global.gamepad, global.settings);
				if (global.settings.exportGamepad) {
					global.gamepad->send(EV_KEY, BTN_THUMBL, e.value);
					global.gamepad->send(EV_SYN, 0, 0);
				}
			} else if (role == ROLE_RIGHT_NUB && global.settings.exportMouse) {
				std::cout << "right nub click\n";
				handleNubClick(global.settings.rightNubClickMode, e.value, global.mouse, global.gamepad, global.settings);
				if (global.settings.exportGamepad) {
					global.gamepad->send(EV_KEY, BTN_THUMBR, e.value);
					global.gamepad->send(EV_SYN, 0, 0);
				}
			}
			break;
		default: 
			global.behaviors->handle(e.code, e.value);
			global.keyboard->send(EV_SYN, 0, 0);
			break;
		}
		break;
	case EV_REL:
		/* 
		 * One of the nub is in mouse mode
		 * TODO: Should forward the event to the mouse
		global.mouse->device.send(EV_REL, e.code, e.value);
		global.mouse->signal.notify_all();
		printf("REL: type=%d, code=%d, value=%d\n", e.type, e.code, e.value);
		 */
		break;
	}
}

void destroy() {
	global.stop = true;
	global.mouse->signal.notify_all();

	if(global.gamepad) {
		delete global.gamepad;
	}
	if(global.keyboard) {
		delete global.keyboard;
	}
	if(global.behaviors) {
		delete global.behaviors;
	}

	global.mouseThread.join();
}

void user1() {
	loadConfig(global.settings.configFile, global.settings);
}
void user2() {
}

void handleArgs(char const** argv, unsigned int argc, Settings& settings) {
	std::string configFile;
	std::regex configRegex("config=(.*)");
	
	for(unsigned int i = 0; i < argc; ++i) {
		std::string arg(argv[i]);
		std::smatch match;
		if(std::regex_match(arg, match, configRegex)) {
			settings.configFile = match[1];
		}
	}
}

using SettingHandler = std::function<void(std::string const&,Settings&)>;
using SettingHandlerMap = std::unordered_map<std::string, SettingHandler>;
SettingHandlerMap const SETTING_HANDLERS = {
	{ "scripts.brightness.normal", [](std::string const& value, Settings& settings){
		settings.brightness.normal = value;
	} },
	{ "scripts.brightness.fn", [](std::string const& value, Settings& settings){
		settings.brightness.Fn = value;
	} },
	{ "scripts.brightness.shift", [](std::string const& value, Settings& settings){
		settings.brightness.Shift = value;
	} },
	{ "scripts.brightness.fnshift", [](std::string const& value, Settings& settings){
		settings.brightness.FnShift = value;
	} },
	{ "scripts.brightness.alt", [](std::string const& value, Settings& settings){
		settings.brightness.Alt = value;
	} },
	{ "scripts.brightness.ctrl", [](std::string const& value, Settings& settings){
		settings.brightness.Ctrl = value;
	} },
	{ "scripts.brightness.altctrl", [](std::string const& value, Settings& settings){
		settings.brightness.AltCtrl = value;
	} },
	{ "scripts.brightness.fnalt", [](std::string const& value, Settings& settings){
		settings.brightness.FnAlt = value;
	} },
	{ "scripts.brightness.fnctrl", [](std::string const& value, Settings& settings){
		settings.brightness.FnCtrl = value;
	} },
	{ "scripts.brightness.shiftalt", [](std::string const& value, Settings& settings){
		settings.brightness.ShiftAlt = value;
	} },
	{ "scripts.brightness.shiftctrl", [](std::string const& value, Settings& settings){
		settings.brightness.ShiftCtrl = value;
	} },
	{ "scripts.brightness.fnshiftalt", [](std::string const& value, Settings& settings){
		settings.brightness.FnShiftAlt = value;
	} },
	{ "scripts.brightness.fnshiftctrl", [](std::string const& value, Settings& settings){
		settings.brightness.FnShiftCtrl = value;
	} },
	{ "gamepad.export", [](std::string const& value, Settings& settings){
		settings.exportGamepad = (value != "0");
	} },
	{ "keypad.export", [](std::string const& value, Settings& settings){
		settings.exportKeypad = (value != "0");
	} },
	{ "mouse.export", [](std::string const& value, Settings& settings){
		settings.exportMouse = (value != "0");
	} },
	{ "mouse.sensitivity", [](std::string const& value, Settings& settings){
		settings.mouseSensitivity = std::stoi(value);
	} },
	{ "mouse.deadzone", [](std::string const& value, Settings& settings) {
		settings.mouseDeadzone = std::stoi(value);
	} },
	{ "mouse.wheel.deadzone", [](std::string const& value, Settings& settings) {
		settings.mouseWheelDeadzone = std::stoi(value);
	} },
	{ "mouse.click.deadzone", [](std::string const& value, Settings& settings) {
		settings.mouseClickDeadzone = std::stoi(value);
	} },
	{ "nubs.deadzone", [](std::string const& value, Settings& settings) {
		settings.joyDeadzone = std::stoi(value);
	} },
	{ "nubs.left.x", [](std::string const& value, Settings& settings) {
		settings.leftNubModeX = parseNubAxisMode(value);
	} },
	{ "nubs.left.y", [](std::string const& value, Settings& settings) {
		settings.leftNubModeY = parseNubAxisMode(value);
	} },
	{ "nubs.right.x", [](std::string const& value, Settings& settings) {
		settings.rightNubModeX = parseNubAxisMode(value);
	} },
	{ "nubs.right.y", [](std::string const& value, Settings& settings) {
		settings.rightNubModeY = parseNubAxisMode(value);
	} },
	{ "nubs.left.click", [](std::string const& value, Settings& settings) {
		settings.leftNubClickMode = parseNubClickMode(value);
	} },
	{ "nubs.right.click", [](std::string const& value, Settings& settings) {
		settings.rightNubClickMode = parseNubClickMode(value);
	} }
};

void loadConfig(std::string const& filename, Settings& settings) {
	std::regex re("^([\\w.]+)\\s*=\\s*(.*)$");
	std::regex emptyRe("^\\s*$");
	std::ifstream configFile(filename);
	
	if(!configFile) {
		std::cerr << "ERROR: Could not open config file " << filename << std::endl;
		return;
	}
	std::string line;
	while(std::getline(configFile, line)) {
		if(line.empty() || line.at(0) == '#' || std::regex_match(line, emptyRe))
			continue;

		std::smatch match;
		if(std::regex_match(line, match, re)) {
			std::string key = match[1];
			std::string value = match[2];
			std::transform(key.begin(), key.end(), key.begin(), tolower);
			auto iter = SETTING_HANDLERS.find(key);
			if(iter == SETTING_HANDLERS.end()) {
				std::cout << "WARNING: Unknown setting in config file: " 
				<< key << std::endl;
			} else {
				iter->second(value, settings);
			}
		} else {
			std::cerr << "Invalid line in config file: " << line << std::endl;
		}
	}
}

Settings::NubAxisMode parseNubAxisMode(std::string const& str) {
	std::string s(str);
	std::transform(s.begin(), s.end(), s.begin(), tolower);
	auto iter = NUB_AXIS_MODES.find(s);
	if(iter == NUB_AXIS_MODES.end()) {
		return Settings::UNKNOWN_NUB_AXIS_MODE;
	} else {
		return iter->second;
	}
}

Settings::NubClickMode parseNubClickMode(std::string const& str) {
	std::string s(str);
	std::transform(s.begin(), s.end(), s.begin(), tolower);
	auto iter = NUB_CLICK_MODES.find(s);
	if(iter == NUB_CLICK_MODES.end()) {
		return Settings::UNKNOWN_NUB_CLICK_MODE;
	} else {
		return iter->second;
	}
}

void handleNubAxis(Settings::NubAxisMode mode, int value, Mouse* mouse, UinputDevice* gamepad, Settings const& settings) {
	switch(mode) {
	case Settings::MOUSE_X:
		mouse->dx = value;
		if(mouse->dx > settings.mouseDeadzone || mouse->dx < -settings.mouseDeadzone)
			mouse->signal.notify_all();
		break;
	case Settings::MOUSE_Y:
		mouse->dy = value;
		if(mouse->dy > settings.mouseDeadzone || mouse->dy < -settings.mouseDeadzone)
			mouse->signal.notify_all();
		break;
	case Settings::SCROLL_X:
		mouse->dwx = value;
		if(mouse->dwx > settings.mouseDeadzone || mouse->dwx < -settings.mouseDeadzone)
			mouse->signal.notify_all();
		break;
	case Settings::SCROLL_Y:
		mouse->dwy = value;
		if(mouse->dwy > settings.mouseDeadzone  || mouse->dwy < -settings.mouseDeadzone)
			mouse->signal.notify_all();
		break;
	case Settings::MOUSE_BTN: {
		int new_val = 0;
		if (value < -settings.mouseClickDeadzone) 
			new_val = -1;
		else if (value > settings.mouseClickDeadzone) 
			new_val = 1;
		if (global.mouseBtn!=new_val && global.settings.exportMouse) {
			if (global.mouseBtn == -1) 
				mouse->device.send(EV_KEY, BTN_LEFT, 0);
			else if (global.mouseBtn == 1) 
				mouse->device.send(EV_KEY, BTN_RIGHT, 0);
			if (new_val == -1 )
				mouse->device.send(EV_KEY, BTN_LEFT, 1);
			else if (new_val == 1 )
				mouse->device.send(EV_KEY, BTN_RIGHT, 1);
			mouse->device.send(EV_SYN, 0, 0);
			global.mouseBtn=new_val;
		}
		}
		break;
	case Settings::UNKNOWN_NUB_AXIS_MODE:
		break;
	}
}

void handleNubClick(Settings::NubClickMode mode, int value, Mouse* mouse, UinputDevice* gamepad, Settings const& settings) {
	switch(mode) {
	case Settings::MOUSE_LEFT: {
		if (global.settings.exportMouse) {
			std::lock_guard<std::mutex> lk(mouse->mutex);
			mouse->device.send(EV_KEY, BTN_LEFT, value);
			mouse->device.send(EV_SYN, 0, 0);
		}
		break;
	}
	case Settings::MOUSE_RIGHT: {
		if (global.settings.exportMouse) {
			std::lock_guard<std::mutex> lk(mouse->mutex);
			mouse->device.send(EV_KEY, BTN_RIGHT, value);
			mouse->device.send(EV_SYN, 0, 0);
		}
		break;
	}
	case Settings::NUB_CLICK_LEFT:
		if (global.settings.exportGamepad) {
			gamepad->send(EV_KEY, BTN_THUMBL, value);
			gamepad->send(EV_SYN, 0, 0);
		}
		break;
	case Settings::NUB_CLICK_RIGHT:
		if (global.settings.exportGamepad) {
			gamepad->send(EV_KEY, BTN_THUMBR, value);
			gamepad->send(EV_SYN, 0, 0);
		}
		break;
	case Settings::UNKNOWN_NUB_CLICK_MODE:
		break;
	}
}

void handleMouse(Mouse* mouse, Settings* settings, bool* stop) {
	while(!*stop) {
		if((mouse->dx > settings->mouseDeadzone || mouse->dx < -settings->mouseDeadzone || mouse->dy > settings->mouseDeadzone || mouse->dy < -settings->mouseDeadzone || mouse->dwx > settings->mouseClickDeadzone || mouse->dwx < -settings->mouseClickDeadzone || mouse->dwy > settings->mouseWheelDeadzone || mouse->dwy < -settings->mouseWheelDeadzone)&&global.settings.exportMouse) {
			std::lock_guard<std::mutex> lk(mouse->mutex);

			if(mouse->dx > settings->mouseDeadzone) {
				mouse->device.send(EV_REL, REL_X, (mouse->dx - settings->mouseDeadzone) * settings->mouseSensitivity / 1000);
			} else if(mouse->dx < -settings->mouseDeadzone) {
				mouse->device.send(EV_REL, REL_X, (mouse->dx + settings->mouseDeadzone) * settings->mouseSensitivity / 1000);
			}

			if(mouse->dy > settings->mouseDeadzone) {
				mouse->device.send(EV_REL, REL_Y, (mouse->dy - settings->mouseDeadzone) * settings->mouseSensitivity / 1000);
			} else if(mouse->dy < -settings->mouseDeadzone) {
				mouse->device.send(EV_REL, REL_Y, (mouse->dy + settings->mouseDeadzone) * settings->mouseSensitivity / 1000);
			}

			if(mouse->dwx > settings->mouseClickDeadzone) {
				mouse->device.send(EV_REL, REL_HWHEEL, 1);
			} else if(mouse->dwx < -settings->mouseClickDeadzone) {
				mouse->device.send(EV_REL, REL_HWHEEL, -1);
			}

			if(mouse->dwy > settings->mouseWheelDeadzone) {
				mouse->device.send(EV_REL, REL_WHEEL, -1);
			} else if(mouse->dwy < -settings->mouseWheelDeadzone) {
				mouse->device.send(EV_REL, REL_WHEEL, 1);
			}

			mouse->device.send(EV_SYN, 0, 0);
		} else {
			std::unique_lock<std::mutex> lk(mouse->mutex);
			mouse->signal.wait(lk);
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(16));
	}
	delete mouse;
}

template<int FIRST_KEY, int LAST_KEY> void KeyBehaviors<FIRST_KEY, LAST_KEY>::passthrough(unsigned int code) {
	behaviors.at(code - FIRST_KEY).type = KeyBehavior::PASSTHROUGH;
}

template<int FIRST_KEY, int LAST_KEY> void KeyBehaviors<FIRST_KEY, LAST_KEY>::map(unsigned int code, unsigned int result) {
	auto& b = behaviors.at(code - FIRST_KEY);
	b.type = KeyBehavior::MAPPED;
	b.mapping = result;
}

template<int FIRST_KEY, int LAST_KEY> void KeyBehaviors<FIRST_KEY, LAST_KEY>::altmap(unsigned int code, bool* flag, unsigned int regular, unsigned int alternative) {
	auto& b = behaviors.at(code - FIRST_KEY);
	b.type = KeyBehavior::ALTMAPPED;
	b.mapping = regular;
	b.alternative = alternative;
	b.flag = flag;
}

template<int FIRST_KEY, int LAST_KEY> void KeyBehaviors<FIRST_KEY, LAST_KEY>::complex(unsigned int code, std::function<void(int)> function) {
	auto& b = behaviors.at(code - FIRST_KEY);
	b.type = KeyBehavior::COMPLEX;
	b.function = function;
}

template<int FIRST_KEY, int LAST_KEY> void KeyBehaviors<FIRST_KEY, LAST_KEY>::gpmap(unsigned int code, unsigned int gamepad) {
	auto& b = behaviors.at(code - FIRST_KEY);
	b.type = KeyBehavior::GPMAPPED;
	b.alternative = gamepad;
}
template<int FIRST_KEY, int LAST_KEY> void KeyBehaviors<FIRST_KEY, LAST_KEY>::gpmap2(unsigned int code, unsigned int gamepad, bool* self, LeftRight* pair) {
	auto& b = behaviors.at(code - FIRST_KEY);
	b.type = KeyBehavior::GPMAP2;
	b.alternative = gamepad;
	b.flag  = self;
	b.lr	= pair;
}

template<int FIRST_KEY, int LAST_KEY> void KeyBehaviors<FIRST_KEY, LAST_KEY>::gphat(unsigned int code, unsigned int gamepad) {
	auto& b = behaviors.at(code - FIRST_KEY);
	b.type = KeyBehavior::GPHAT;
	b.alternative = gamepad;
}
template<int FIRST_KEY, int LAST_KEY> void KeyBehaviors<FIRST_KEY, LAST_KEY>::script(unsigned int code, Scripts *s) {
	auto& b = behaviors.at(code - FIRST_KEY);
	b.type = KeyBehavior::SCRIPT;
	b.scripts = s;
}

template<int FIRST_KEY, int LAST_KEY> void KeyBehaviors<FIRST_KEY, LAST_KEY>::handle(unsigned int code, int value) {
	if (code <FIRST_KEY || code >LAST_KEY) return;
	KeyBehavior const& kb = behaviors.at(code - FIRST_KEY);
	switch(kb.type) {
	case KeyBehavior::PASSTHROUGH:
		global.keyboard->send(EV_KEY, code, value);
		break;
	case KeyBehavior::MAPPED:
		global.keyboard->send(EV_KEY, kb.mapping, value);
		break;
	case KeyBehavior::ALTMAPPED:
		global.keyboard->send(EV_KEY, *kb.flag ? kb.alternative : kb.mapping, value);
		break; 
	case KeyBehavior::COMPLEX:
		kb.function(value);
		break;
	case KeyBehavior::GPMAPPED:
		if (global.settings.exportKeypad)
			global.keyboard->send(EV_KEY, code, value);
		if (global.settings.exportGamepad) {
			global.gamepad->send(EV_KEY, kb.alternative, value);
			global.gamepad->send(EV_SYN, 0, 0);
		}
		break;
	case KeyBehavior::GPMAP2:
		*kb.flag = (value==1);
		kb.lr->pressed = kb.lr->left || kb.lr->right;
		if (global.settings.exportKeypad)
			global.keyboard->send(EV_KEY, code, value);
		if (global.settings.exportGamepad) {
			global.gamepad->send(EV_KEY, kb.alternative, value);
			global.gamepad->send(EV_SYN, 0, 0);
		}
		break;
	case KeyBehavior::GPHAT:
		global.keyboard->send(EV_KEY, code, value);
		switch (kb.alternative) {
		case BTN_DPAD_UP:
			global.haty = -value;
			break;
		case BTN_DPAD_DOWN:
			global.haty = value;
			break;
		case BTN_DPAD_LEFT:
			global.hatx = -value;
			break;
		case BTN_DPAD_RIGHT:
			global.hatx = value;
			break;
		};
		if (global.settings.exportGamepad) {
			global.gamepad->send(EV_ABS, ABS_HAT0X, global.hatx*65535);
			global.gamepad->send(EV_ABS, ABS_HAT0Y, global.haty*65535);
			global.gamepad->send(EV_SYN, 0, 0);
		}
		break;
	case KeyBehavior::SCRIPT:
		if (value!=1) return;
		int ret = 0;
		if (global.Shift.pressed && global.Ctrl.pressed && global.Fn.pressed) {
			if (kb.scripts->FnShiftCtrl!="")
				ret=system(kb.scripts->FnShiftCtrl.c_str());
		} else if (global.Shift.pressed && global.Alt.pressed && global.Fn.pressed) {
			if (kb.scripts->FnShiftAlt!="")
				ret=system(kb.scripts->FnShiftAlt.c_str());
		} else if (global.Shift.pressed && global.Alt.pressed) {
			if (kb.scripts->ShiftAlt!="")
				ret=system(kb.scripts->ShiftAlt.c_str());
		} else if (global.Shift.pressed && global.Ctrl.pressed) {
			if (kb.scripts->ShiftCtrl!="")
				ret=system(kb.scripts->ShiftCtrl.c_str());
		} else if (global.Fn.pressed && global.Alt.pressed) {
			if (kb.scripts->FnAlt!="")
				ret=system(kb.scripts->FnAlt.c_str());
		} else if (global.Fn.pressed && global.Ctrl.pressed) {
			if (kb.scripts->FnCtrl!="")
				ret=system(kb.scripts->FnCtrl.c_str());
		} else if (global.Alt.pressed && global.Ctrl.pressed) {
			if (kb.scripts->AltCtrl!="")
				ret=system(kb.scripts->AltCtrl.c_str());
		} else if (global.Fn.pressed && global.Shift.pressed) {
			if (kb.scripts->FnShift!="")
				ret=system(kb.scripts->FnShift.c_str());
		} else if (global.Alt.pressed) {
			if (kb.scripts->Alt!="")
				ret=system(kb.scripts->Alt.c_str());
		} else if (global.Alt.pressed) {
			if (kb.scripts->Alt!="")
				ret=system(kb.scripts->Alt.c_str());
		} else if (global.Shift.pressed) {
			if (kb.scripts->Shift!="")
				ret=system(kb.scripts->Shift.c_str());
		} else if (global.Fn.pressed) {
			if (kb.scripts->Fn!="")
				ret=system(kb.scripts->Fn.c_str());
		} else {
			if (kb.scripts->normal!="")
				ret=system(kb.scripts->normal.c_str());
		}
		break;
	};
}
