#include "sysconfig.h"
#include "sysdeps.h"
#include "config.h"
#include "options.h"
#include "keyboard.h"
#include "inputdevice.h"
#include "xwin.h"
#include <SDL.h>


#define MAX_AXIS 4
#define MAX_BUTTONS 128

#define PID_MOUSE 1
#define PID_JOYSTICK 2
#define PID_KEYBOARD 3


struct pidata {
	int type;
	int acquired;
	TCHAR *name;
	TCHAR *configname;

	int eventinput;
	uae_s16 axles;
	uae_s16 buttons, buttons_real;

	TCHAR *axisname[MAX_AXIS];
	TCHAR *buttonname[MAX_BUTTONS];
  uae_s16 button_keycode[MAX_BUTTONS];
    
	bool analogstick;
};

static struct pidata pi_mouse[MAX_INPUT_DEVICES];
static struct pidata pi_keyboard[MAX_INPUT_DEVICES];
static struct pidata pi_joystick[MAX_INPUT_DEVICES];
static int num_mouse, num_keyboard, num_joystick;
static int sdlmouse, sdlmousenumber;
static int sdlkbd, sdlkbdnumber;

static const int sdlkeyboard_keys[MAX_BUTTONS] = {
	SDLK_0, SDLK_1,	SDLK_2,	SDLK_3,	SDLK_4,	SDLK_5,	SDLK_6,	SDLK_7,	SDLK_8,	SDLK_9, SDLK_BACKSPACE,         // 10
 	SDLK_F1, SDLK_F2, SDLK_F3, SDLK_F4, SDLK_F5, SDLK_F6, SDLK_F7, SDLK_F8, SDLK_F9, SDLK_F10, SDLK_INSERT, // 21
  SDLK_q, SDLK_w, SDLK_e, SDLK_r, SDLK_t, SDLK_y, SDLK_u, SDLK_i, SDLK_o, SDLK_p,                         // 31
	SDLK_LSHIFT, SDLK_a, SDLK_s, SDLK_d, SDLK_f, SDLK_g, SDLK_h, SDLK_j, SDLK_k, SDLK_l, SDLK_RETURN,       // 42
 	SDLK_COMMA, SDLK_PERIOD, SDLK_z, SDLK_x, SDLK_c, SDLK_v, SDLK_b, SDLK_n, SDLK_m, SDLK_SPACE,            // 52
  SDLK_ESCAPE, SDLK_AT, SDLK_LEFTBRACKET, SDLK_RIGHTBRACKET, SDLK_EXCLAIM, SDLK_UNDERSCORE, SDLK_F11, SDLK_F12, // 60
  SDLK_CAPSLOCK, SDLK_QUOTE, SDLK_QUOTEDBL, SDLK_MINUS, SDLK_PLUS, SDLK_EQUALS, SDLK_BACKQUOTE,                 // 67
  SDLK_SEMICOLON, SDLK_COLON, SDLK_SLASH, SDLK_QUESTION, SDLK_BACKSLASH, SDLK_HASH, SDLK_DOLLAR, SDLK_TAB,      // 75
  SDLK_LALT, SDLK_HOME, SDLK_END, SDLK_PAGEDOWN, SDLK_PAGEUP, SDLK_RSHIFT, SDLK_RCTRL,                          // 82
  -1
};

static const TCHAR *sdlkeyboard_labels[MAX_BUTTONS] = {
	_T("0"), _T("1"), _T("2"), _T("3"), _T("4"), _T("5"), _T("6"), _T("7"), _T("8"), _T("9"), _T("BACK"),
	_T("F1"), _T("F2"), _T("F3"), _T("F4"), _T("F5"), _T("F6"), _T("F7"), _T("F8"), _T("F9"), _T("F10"), _T("INSERT"),
	_T("Q"), _T("W"), _T("E"), _T("R"), _T("T"), _T("Y"), _T("U"), _T("I"), _T("O"), _T("P"),
  _T("LSHIFT"), _T("A"), _T("S"), _T("D"), _T("F"), _T("G"), _T("H"), _T("J"), _T("K"), _T("L"), _T("RETURN"),
  _T(","), _T("."), _T("Z"), _T("X"), _T("C"), _T("V"), _T("B"), _T("N"), _T("M"), _T("SPACE"),
	_T("ESCAPE"), _T("@"), _T("("), _T(")"), _T("!"), _T("_"), _T("F11"), _T("F12"),
	_T("CAPSLOCK"), _T("'"), _T("\""), _T("-"), _T("+"), _T("="), _T("BACKQUOTE"),
  _T(";"), _T(":"), _T("/"), _T("?"), _T("\\"), _T("#"), _T("$"), _T("TAB"),
  _T("START"), _T("BUTTON A"), _T("BUTTON B"), _T("BUTTON X"), _T("BUTTON Y"), _T("LSHOULDER"), _T("RSHOULDER"),
  NULL
};

int sdlkeyboard_keys_inv[SDLK_LAST];

static void build_inv_keytab(void)
{
  memset(sdlkeyboard_keys_inv, 0, sizeof(sdlkeyboard_keys_inv));
  for (int i = 0; i < MAX_BUTTONS && sdlkeyboard_keys[i] >= 0; ++i)
    sdlkeyboard_keys_inv[sdlkeyboard_keys[i]] = i;
}

static void setkblabels (struct pidata *pid)
{
	for (int k = 0; sdlkeyboard_keys[k] >= 0; k++) {
		TCHAR tmp[100];
		tmp[0] = 0;
		if (sdlkeyboard_labels[k] != NULL && sdlkeyboard_labels[k][0])
			_tcscpy (tmp, sdlkeyboard_labels[k]);
		if (!tmp[0])
			_stprintf (tmp, _T("KEY_%02X"), sdlkeyboard_keys[k]);
		pid->buttonname[k] = my_strdup (tmp);
		pid->button_keycode[k] = sdlkeyboard_keys[k];
		pid->buttons++;
	}
}

int get_sdlmouse (void)
{
	if (sdlmouse)
		return sdlmousenumber;
	return -1;
}

int get_sdlkbd (void)
{
	if (sdlkbd)
		return sdlkbdnumber;
	return -1;
}

static int isrealbutton (struct pidata *pid, int num)
{
	if (num >= pid->buttons)
		return 0;
	return 1;
}

static void init_mouse_widgets(struct pidata *pid, int numaxles, int numbuttons)
{
  int j;
 	TCHAR tmp[100];

  pid->buttons_real = pid->buttons = numbuttons;
  pid->analogstick = false;
	for (j = 0; j < pid->buttons; j++) {
		_stprintf (tmp, _T("Button %d"), j + 1);
		pid->buttonname[j] = my_strdup (tmp);
	}
  pid->axles = numaxles;
  for (j = 0; j < pid->axles; j++) {
  	if (j == 0)
  	  pid->axisname[j] = my_strdup (_T("X Axis"));
  	else if (j == 1)
    	pid->axisname[j] = my_strdup (_T("Y Axis"));
    else if (j == 2)
      pid->axisname[j] = my_strdup (_T("Wheel"));
    else
      pid->axisname[j] = my_strdup (_T("Unknown"));
  }
}

static void init_joy_widgets(struct pidata *pid, int numaxles, int numbuttons)
{
  int j, code;
 	TCHAR tmp[100];

  pid->buttons_real = pid->buttons = numbuttons;
  pid->analogstick = false;
	for (j = 0; j < pid->buttons; j++) {
	  switch(j) {
	    case 0:
  		  _tcscpy (tmp, _T("Button X"));
  		  code = SDLK_PAGEDOWN;
  		  break;
  		case 1:
  		  _tcscpy (tmp, _T("Button B"));
  		  code = SDLK_END;
  		  break;
  		case 2:
  		  _tcscpy (tmp, _T("Button A"));
  		  code = SDLK_HOME;
  		  break;
  		case 3:
  		  _tcscpy (tmp, _T("Button Y"));
  		  code = SDLK_PAGEUP;
  		  break;
  		case 4:
  		  _tcscpy (tmp, _T("R shoulder"));
  		  code = SDLK_RSHIFT;
  		  break;
  		case 5:
  		  _tcscpy (tmp, _T("L shoulder"));
  		  code = SDLK_RCTRL;
  		  break;
  		case 6:
  		  _tcscpy (tmp, _T("Start"));
  		  code = SDLK_LALT;
  		  break;
  		default:
  		  _stprintf (tmp, _T("Button %d"), j + 1);
  		  code = -1;
  		  break;
		}
		pid->buttonname[j] = my_strdup (tmp);
		pid->button_keycode[j] = code;
	}
  pid->axles = numaxles;
  for (j = 0; j < pid->axles; j++) {
  	if (j == 0)
  	  pid->axisname[j] = my_strdup (_T("X Axis"));
  	else if (j == 1)
    	pid->axisname[j] = my_strdup (_T("Y Axis"));
    else
      pid->axisname[j] = my_strdup (_T("Unknown"));
  }
}

static void pi_dev_free (struct pidata *pid)
{
	for (int i = 0; i < MAX_AXIS; i++) {
	  if(pid->axisname[i])
	    xfree(pid->axisname[i]);
  }
	for (int i = 0; i < MAX_BUTTONS; i++) {
	  if(pid->buttonname[i])
	    xfree(pid->buttonname[i]);
  }
	if(pid->name)
  	xfree (pid->name);
	if(pid->configname)
	  xfree (pid->configname);
	memset (pid, 0, sizeof (*pid));
}

static bool pi_initialized = false;

void input_closeall(void)
{
	for (int i = 0; i < MAX_INPUT_DEVICES; i++) {
		pi_dev_free (&pi_joystick[i]);
		pi_dev_free (&pi_mouse[i]);
		pi_dev_free (&pi_keyboard[i]);
	}
	num_mouse = 0;
	num_keyboard = 0;
	num_joystick = 0;
  sdlmouse = 0;
  sdlmousenumber = 0;
  sdlkbd = 0;
  sdlkbdnumber = 0;

  pi_initialized = false;
}

bool input_initialize_alldevices (void)
{
  struct pidata *pid;

  memset(pi_mouse, 0, sizeof(pi_mouse));
  memset(pi_joystick, 0, sizeof(pi_joystick));
  memset(pi_keyboard, 0, sizeof(pi_keyboard));

  input_closeall();
  
  build_inv_keytab();
  
  pid = pi_mouse;
  
  pid->type = PID_MOUSE;
  pid->name = my_strdup (_T("Nubs as mouse"));
  pid->configname = my_strdup (_T("MOUSE0"));
  pid->eventinput = 1;
  init_mouse_widgets(pid, 2, 2);
	pid++;
	num_mouse++;

  pid->type = PID_MOUSE;
  pid->name = my_strdup (_T("dPad as mouse"));
  pid->configname = my_strdup (_T("MOUSE1"));
  pid->eventinput = 0;
  init_mouse_widgets(pid, 2, 2);
	pid++;
	num_mouse++;

  pid = pi_joystick;
  pid->type = PID_JOYSTICK;
  pid->name = my_strdup(_T("dPad as joystick"));
  pid->configname = my_strdup (_T("JOY0"));
  pid->eventinput = 0;
  init_joy_widgets(pid, 2, 7);
	pid++;
	num_joystick++;
  
  pid = pi_keyboard;
  pid->type = PID_KEYBOARD;
  pid->name = my_strdup(_T("Keyboard"));
  pid->configname = my_strdup (_T("KBD0"));
  pid->eventinput = 1;
  pid->buttons = 0;
  pid->axles = 0;
  setkblabels(pid);
  pid->buttons_real = pid->buttons;
	pid++;
	num_keyboard++;

  pi_initialized = true;

	return true;
}

static int get_mouse_num (void)
{
	return num_mouse;
}

static TCHAR *get_mouse_friendlyname (int mouse)
{
	return pi_mouse[mouse].name;
}

static TCHAR *get_mouse_uniquename (int mouse)
{
	return pi_mouse[mouse].configname;
}

static int get_mouse_widget_num (int mouse)
{
	return pi_mouse[mouse].axles + pi_mouse[mouse].buttons;
}

static int get_mouse_widget_first (int mouse, int type)
{
  switch (type) {
  	case IDEV_WIDGET_BUTTON:
		  return pi_mouse[mouse].axles;
	  case IDEV_WIDGET_AXIS:
		  return 0;
  	case IDEV_WIDGET_BUTTONAXIS:
		  return pi_mouse[mouse].axles + pi_mouse[mouse].buttons_real;
  }
  return -1;
}

static int get_mouse_widget_type (int mouse, int num, TCHAR *name, uae_u32 *code)
{
	struct pidata *pid = &pi_mouse[mouse];

	int axles = pid->axles;
	int buttons = pid->buttons;
	int realbuttons = pid->buttons_real;
	if (num >= axles + realbuttons && num < axles + buttons) {
		if (name)
			_tcscpy (name, pid->buttonname[num - axles]);
		return IDEV_WIDGET_BUTTONAXIS;
	} else if (num >= axles && num < axles + realbuttons) {
  	if (name)
			_tcscpy (name, pid->buttonname[num - axles]);
	  return IDEV_WIDGET_BUTTON;
	} else if (num < axles) {
		if (name)
			_tcscpy (name, pid->axisname[num]);
	  return IDEV_WIDGET_AXIS;
  }
  return IDEV_WIDGET_NONE;
}

static int init_mouse (void) 
{
	for (int i = 0; i < num_mouse; i++) {
		struct pidata *pid = &pi_mouse[i];
    pid->acquired = 0;
	}
  sdlmouse = 0;
  sdlmousenumber = 0;
	
  return 1;
}

static void close_mouse (void) 
{
	for (int i = 0; i < num_mouse; i++) {
		struct pidata *pid = &pi_mouse[i];
    pid->acquired = 0;
	}
  sdlmouse = 0;
  sdlmousenumber = 0;
}

static int acquire_mouse (int num, int flags) 
{
	if (num < 0)
    return 1;

	struct pidata *pid = &pi_mouse[num];

	pid->acquired = 1;

	if (pid->eventinput) {
		sdlmouse++;
		sdlmousenumber = num;
	}
	return pid->acquired > 0 ? 1 : 0;
}

static void unacquire_mouse (int num) 
{
	if (num < 0)
		return;

	struct pidata *pid = &pi_mouse[num];

	if (pid->acquired > 0) {
		if (pid->eventinput) {
			sdlmouse--;
		}
		pid->acquired = 0;
	}
}

static void read_mouse (void)
{
  int i;
  Uint8 *keystate = NULL;
  int mouseScale = currprefs.input_joymouse_multiplier / 4;

  if(currprefs.input_tablet > TABLET_OFF) {
    // Mousehack active
    int x, y;
    SDL_GetMouseState(&x, &y);
	  setmousestate(0, 0, x, 1);
		if(!adisplays.picasso_on)
		  y -= OFFSET_Y_ADJUST;
	  setmousestate(0, 1, y, 1);
		return;
  }
  
	for (i = 0; i < num_mouse; i++) {
		struct pidata *pid = &pi_mouse[i];
		if (!pid->acquired)
			continue;
		if (pid->eventinput)
		  continue; // input handling for this type in handle_msgpump() 

		if(keystate == NULL)
  	  keystate = SDL_GetKeyState(NULL);

	  // x axis
    if(keystate[VK_LEFT])
      setmousestate(i, 0, -mouseScale, 0);
    if(keystate[VK_RIGHT])
      setmousestate(i, 0, mouseScale, 0);
    // y axis
    if(keystate[VK_UP])
      setmousestate(i, 1, -mouseScale, 0);
    if(keystate[VK_DOWN])
      setmousestate(i, 1, mouseScale, 0);

    setmousebuttonstate (i, 0, keystate[VK_A]);
    setmousebuttonstate (i, 1, keystate[VK_B]);
  }
}

static int get_mouse_flags (int num) 
{
  if(num >= num_mouse)
    return 1;
  if(pi_mouse[num].eventinput)
    return 1;
  return 0;
}

struct inputdevice_functions inputdevicefunc_mouse = {
  init_mouse, close_mouse, acquire_mouse, unacquire_mouse, read_mouse,
  get_mouse_num, get_mouse_friendlyname, get_mouse_uniquename,
  get_mouse_widget_num, get_mouse_widget_type,
  get_mouse_widget_first,
  get_mouse_flags
};


static int get_kb_num (void)
{
	return num_keyboard;
}

static TCHAR *get_kb_friendlyname (int kb)
{
	return pi_keyboard[kb].name;
}

static TCHAR *get_kb_uniquename (int kb)
{
	return pi_keyboard[kb].configname;
}

static int get_kb_widget_num (int kb)
{
	return pi_keyboard[kb].buttons;
}

static int get_kb_widget_first (int kb, int type)
{
	return 0;
}

static int get_kb_widget_type (int kb, int num, TCHAR *name, uae_u32 *code)
{
	if (name) {
		if (pi_keyboard[kb].buttonname[num])
			_tcscpy (name, pi_keyboard[kb].buttonname[num]);
		else
			name[0] = 0;
	}
	if (code) {
		*code = pi_keyboard[kb].button_keycode[num];
	}
	return IDEV_WIDGET_KEY;
}

static int init_kb (void)
{
	for (int i = 0; i < num_keyboard; i++) {
		struct pidata *pid = &pi_keyboard[i];
    pid->acquired = 0;
	}
  sdlkbd = 0;
  sdlkbdnumber = 0;

  return 1;
}

static void close_kb (void)
{
	for (int i = 0; i < num_keyboard; i++) {
		struct pidata *pid = &pi_keyboard[i];
    pid->acquired = 0;
	}
  sdlkbd = 0;
  sdlkbdnumber = 0;
}

static int acquire_kb (int num, int flags)
{
	if (num < 0)
    return 1;

	struct pidata *pid = &pi_keyboard[num];

	pid->acquired = 1;

	if (pid->eventinput) {
		sdlkbd++;
		sdlkbdnumber = num;
	}

	return pid->acquired > 0 ? 1 : 0;
}

static void unacquire_kb (int num)
{
	if (num < 0) {
		return;
	}

	struct pidata *pid = &pi_keyboard[num];

  if (pid->acquired) {
  	if (pid->eventinput) {
  		sdlkbd--;
  	}
  	pid->acquired = 0;
  }
}

static void read_kb (void)
{
  // All keyboards for Pandora are with flag eventinput set.
  // input handling for this type in handle_msgpump() 
}

static int get_kb_flags (int num)
{
  if(num >= num_keyboard)
    return 1;
  if(pi_keyboard[num].eventinput)
    return 1;
  return 0;
}

struct inputdevice_functions inputdevicefunc_keyboard = {
	init_kb, close_kb, acquire_kb, unacquire_kb, read_kb,
	get_kb_num, get_kb_friendlyname, get_kb_uniquename,
	get_kb_widget_num, get_kb_widget_type,
	get_kb_widget_first,
	get_kb_flags
};


static int get_joystick_num (void)
{
	return num_joystick;
}

static int get_joystick_widget_num (int joy)
{
	return pi_joystick[joy].axles + pi_joystick[joy].buttons;
}

static int get_joystick_widget_type (int joy, int num, TCHAR *name, uae_u32 *code)
{
	struct pidata *pid = &pi_joystick[joy];
	if (num >= pid->axles + pid->buttons_real && num < pid->axles + pid->buttons) {
		if (name)
			_tcscpy (name, pid->buttonname[num - pid->axles]);
		return IDEV_WIDGET_BUTTONAXIS;
	} else if (num >= pid->axles && num < pid->axles + pid->buttons_real) {
		if (name)
			_tcscpy (name, pid->buttonname[num - pid->axles]);
		return IDEV_WIDGET_BUTTON;
	} else if (num < pid->axles) {
		if (name)
			_tcscpy (name, pid->axisname[num]);
		return IDEV_WIDGET_AXIS;
	}
	return IDEV_WIDGET_NONE;
}

static int get_joystick_widget_first (int joy, int type)
{
	switch (type)	{
  	case IDEV_WIDGET_BUTTON:
  		return pi_joystick[joy].axles;
  	case IDEV_WIDGET_AXIS:
  		return 0;
  	case IDEV_WIDGET_BUTTONAXIS:
  		return pi_joystick[joy].axles + pi_joystick[joy].buttons_real;
	}
	return -1;
}

static TCHAR *get_joystick_friendlyname (int joy)
{
	return pi_joystick[joy].name;
}

static TCHAR *get_joystick_uniquename (int joy)
{
	return pi_joystick[joy].configname;
}

static void read_joystick (void)
{
  int i, j;
  Uint8 *keystate = NULL;

	for (i = 0; i < num_joystick; i++) {
		struct pidata *pid = &pi_joystick[i];
		if (!pid->acquired)
			continue;

		if(keystate == NULL)
  	  keystate = SDL_GetKeyState(NULL);

    // dPad is joystick only without right shoulder
    if(!keystate[SDLK_RCTRL]) {
      int axis = (keystate[SDLK_LEFT] ? -32767 : (keystate[SDLK_RIGHT] ? 32767 : 0));
      setjoystickstate (i, 0, axis, 32767);
      axis = (keystate[SDLK_UP] ? -32767 : (keystate[SDLK_DOWN] ? 32767 : 0));
      setjoystickstate (i, 1, axis, 32767);
    }
    for (j = 0; j < pid->buttons; ++j)
      setjoybuttonstate (i, j, keystate[pid->button_keycode[j]]);
	}
}

static int init_joystick (void)
{
	for (int i = 0; i < num_joystick; i++) {
		struct pidata *pid = &pi_joystick[i];
    pid->acquired = 0;
	}

	return 1;
}

static void close_joystick (void)
{
	for (int i = 0; i < num_joystick; i++) {
		struct pidata *pid = &pi_joystick[i];
    pid->acquired = 0;
	}
}

static int acquire_joystick (int num, int flags)
{
	if (num < 0)
    return 1;

	struct pidata *pid = &pi_joystick[num];

	pid->acquired = 1;

	return pid->acquired > 0 ? 1 : 0;
}

static void unacquire_joystick (int num)
{
	if (num < 0) {
		return;
	}

	struct pidata *pid = &pi_joystick[num];

	pid->acquired = 0;
}

static int get_joystick_flags (int num)
{
	return 0;
}

struct inputdevice_functions inputdevicefunc_joystick = {
	init_joystick, close_joystick, acquire_joystick, unacquire_joystick,
	read_joystick, get_joystick_num, get_joystick_friendlyname, get_joystick_uniquename,
	get_joystick_widget_num, get_joystick_widget_type,
	get_joystick_widget_first,
	get_joystick_flags
};


static void setid (struct uae_input_device *uid, int i, int slot, int sub, int port, int evt, bool gp)
{
	if (gp)
		inputdevice_sparecopy (&uid[i], slot, 0);
  uid[i].eventid[slot][sub] = evt;
  uid[i].port[slot][sub] = port + 1;
}

static void setid (struct uae_input_device *uid, int i, int slot, int sub, int port, int evt, int af, bool gp)
{
	setid (uid, i, slot, sub, port, evt, gp);
	uid[i].flags[slot][sub] &= ~ID_FLAG_AUTOFIRE_MASK;
	if (af >= JPORT_AF_NORMAL)
		uid[i].flags[slot][sub] |= ID_FLAG_AUTOFIRE;
	if (af == JPORT_AF_TOGGLE)
		uid[i].flags[slot][sub] |= ID_FLAG_TOGGLE;
	if (af == JPORT_AF_ALWAYS)
		uid[i].flags[slot][sub] |= ID_FLAG_INVERTTOGGLE;
}

int input_get_default_mouse (struct uae_input_device *uid, int i, int port, int af, bool gp, bool wheel, bool joymouseswap)
{
	struct pidata *pid = NULL;

	if (!joymouseswap) {
		if (i >= num_mouse)
			return 0;
		pid = &pi_mouse[i];
	} else {
		if (i >= num_joystick)
			return 0;
		pid = &pi_joystick[i];
	}

	setid (uid, i, ID_AXIS_OFFSET + 0, 0, port, port ? INPUTEVENT_MOUSE2_HORIZ : INPUTEVENT_MOUSE1_HORIZ, gp);
	setid (uid, i, ID_AXIS_OFFSET + 1, 0, port, port ? INPUTEVENT_MOUSE2_VERT : INPUTEVENT_MOUSE1_VERT, gp);
	if (wheel)
		setid (uid, i, ID_AXIS_OFFSET + 2, 0, port, port ? 0 : INPUTEVENT_MOUSE1_WHEEL, gp);
	setid (uid, i, ID_BUTTON_OFFSET + 0, 0, port, port ? INPUTEVENT_JOY2_FIRE_BUTTON : INPUTEVENT_JOY1_FIRE_BUTTON, af, gp);
	setid (uid, i, ID_BUTTON_OFFSET + 1, 0, port, port ? INPUTEVENT_JOY2_2ND_BUTTON : INPUTEVENT_JOY1_2ND_BUTTON, gp);
	setid (uid, i, ID_BUTTON_OFFSET + 2, 0, port, port ? INPUTEVENT_JOY2_3RD_BUTTON : INPUTEVENT_JOY1_3RD_BUTTON, gp);
	if (i == 0)
		return 1;
	return 0;
}

int input_get_default_joystick (struct uae_input_device *uid, int i, int port, int af, int mode, bool gp, bool joymouseswap)
{
 	struct pidata *pid = NULL;
  int h, v;

	if (joymouseswap) {
		if (i >= num_mouse)
			return 0;
		pid = &pi_mouse[i];
	} else {
		if (i >= num_joystick)
			return 0;
		pid = &pi_joystick[i];
	}

	if (port >= 2) {
		h = port == 3 ? INPUTEVENT_PAR_JOY2_HORIZ : INPUTEVENT_PAR_JOY1_HORIZ;
		v = port == 3 ? INPUTEVENT_PAR_JOY2_VERT : INPUTEVENT_PAR_JOY1_VERT;
	} else {
    h = port ? INPUTEVENT_JOY2_HORIZ : INPUTEVENT_JOY1_HORIZ;
    v = port ? INPUTEVENT_JOY2_VERT : INPUTEVENT_JOY1_VERT;
  }

  setid (uid, i, ID_AXIS_OFFSET + 0, 0, port, h, gp);
  setid (uid, i, ID_AXIS_OFFSET + 1, 0, port, v, gp);

	if (port >= 2) {
		setid (uid, i, ID_BUTTON_OFFSET + 0, 0, port, port == 3 ? INPUTEVENT_PAR_JOY2_FIRE_BUTTON : INPUTEVENT_PAR_JOY1_FIRE_BUTTON, af, gp);
	} else {
		setid (uid, i, ID_BUTTON_OFFSET + 0, 0, port, port ? INPUTEVENT_JOY2_FIRE_BUTTON : INPUTEVENT_JOY1_FIRE_BUTTON, af, gp);
		if (isrealbutton (pid, 1))
			setid (uid, i, ID_BUTTON_OFFSET + 1, 0, port, port ? INPUTEVENT_JOY2_2ND_BUTTON : INPUTEVENT_JOY1_2ND_BUTTON, gp);
		if (mode != JSEM_MODE_JOYSTICK) {
			if (isrealbutton (pid, 2))
				setid (uid, i, ID_BUTTON_OFFSET + 2, 0, port, port ? INPUTEVENT_JOY2_3RD_BUTTON : INPUTEVENT_JOY1_3RD_BUTTON, gp);
		}
	}

	if (mode == JSEM_MODE_JOYSTICK_CD32) {
		setid (uid, i, ID_BUTTON_OFFSET + 0, 0, port, port ? INPUTEVENT_JOY2_CD32_RED : INPUTEVENT_JOY1_CD32_RED, af, gp);
		if (isrealbutton (pid, 1)) {
			setid (uid, i, ID_BUTTON_OFFSET + 1, 0, port, port ? INPUTEVENT_JOY2_CD32_BLUE : INPUTEVENT_JOY1_CD32_BLUE, gp);
		}
		if (isrealbutton (pid, 2))
			setid (uid, i, ID_BUTTON_OFFSET + 2, 0, port, port ? INPUTEVENT_JOY2_CD32_GREEN : INPUTEVENT_JOY1_CD32_GREEN, gp);
		if (isrealbutton (pid, 3))
			setid (uid, i, ID_BUTTON_OFFSET + 3, 0, port, port ? INPUTEVENT_JOY2_CD32_YELLOW : INPUTEVENT_JOY1_CD32_YELLOW, gp);
		if (isrealbutton (pid, 4))
			setid (uid, i, ID_BUTTON_OFFSET + 4, 0, port, port ? INPUTEVENT_JOY2_CD32_RWD : INPUTEVENT_JOY1_CD32_RWD, gp);
		if (isrealbutton (pid, 5))
			setid (uid, i, ID_BUTTON_OFFSET + 5, 0, port, port ? INPUTEVENT_JOY2_CD32_FFW : INPUTEVENT_JOY1_CD32_FFW, gp);
		if (isrealbutton (pid, 6))
			setid (uid, i, ID_BUTTON_OFFSET + 6, 0, port, port ? INPUTEVENT_JOY2_CD32_PLAY :  INPUTEVENT_JOY1_CD32_PLAY, gp);
	}

  if (i == 0) {
    return 1;
  }
  return 0;
}

int input_get_default_keyboard (int i)
{
	if (sdlkbd) {
		if (i < 0)
			return 0;
		if (i >= num_keyboard)
			return 0;
		return 1;
	} else {
		if (i < 0)
			return 0;
		if (i == 0)
			return 1;
	}
	return 0;
}

int input_get_default_joystick_analog (struct uae_input_device *uid, int i, int port, int af, bool gp, bool joymouseswap)
{
	struct pidata *pid;

	if (joymouseswap) {
		if (i >= num_mouse)
			return 0;
		pid = &pi_mouse[i];
	} else {
		if (i >= num_joystick)
			return 0;
		pid = &pi_joystick[i];
	}
	setid (uid, i, ID_AXIS_OFFSET + 0, 0, port, port ? INPUTEVENT_JOY2_HORIZ_POT : INPUTEVENT_JOY1_HORIZ_POT, gp);
	setid (uid, i, ID_AXIS_OFFSET + 1, 0, port, port ? INPUTEVENT_JOY2_VERT_POT : INPUTEVENT_JOY1_VERT_POT, gp);
	setid (uid, i, ID_BUTTON_OFFSET + 0, 0, port, port ? INPUTEVENT_JOY2_LEFT : INPUTEVENT_JOY1_LEFT, af, gp);
	if (isrealbutton (pid, 1))
		setid (uid, i, ID_BUTTON_OFFSET + 1, 0, port, port ? INPUTEVENT_JOY2_RIGHT : INPUTEVENT_JOY1_RIGHT, gp);
	if (isrealbutton (pid, 2))
		setid (uid, i, ID_BUTTON_OFFSET + 2, 0, port, port ? INPUTEVENT_JOY2_UP : INPUTEVENT_JOY1_UP, gp);
	if (isrealbutton (pid, 3))
		setid (uid, i, ID_BUTTON_OFFSET + 3, 0, port, port ? INPUTEVENT_JOY2_DOWN : INPUTEVENT_JOY1_DOWN, gp);

	if (i == 0)
		return 1;
	return 0;
}
