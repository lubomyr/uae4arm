#include "sysconfig.h"
#include "sysdeps.h"
#include "config.h"
#include "options.h"
#include "keyboard.h"
#include "inputdevice.h"
#include "xwin.h"
#include <SDL.h>


#define MAX_AXIS 4
#define MAX_BUTTONS 256

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

  char IsPS3Controller;
  SDL_Joystick* sdlHandle;
};

static struct pidata pi_mouse[MAX_INPUT_DEVICES];
static struct pidata pi_keyboard[MAX_INPUT_DEVICES];
static struct pidata pi_joystick[MAX_INPUT_DEVICES];
static int num_mouse, num_keyboard, num_joystick;
static int sdlmouse, sdlmousenumber;
static int sdlkbd, sdlkbdnumber;

TCHAR rawkeyboardlabels[256][20];


static void setkblabels (struct pidata *pid)
{
	for (int k = 0; k < 254; k++) {
		TCHAR tmp[100];
		tmp[0] = 0;
		if (rawkeyboardlabels[k] != NULL && rawkeyboardlabels[k][0])
			_tcscpy (tmp, rawkeyboardlabels[k]);
		if (!tmp[0])
			_stprintf (tmp, _T("KEY_%02X"), k + 1);
		pid->buttonname[k] = my_strdup (tmp);
		pid->button_keycode[k] = k + 1;
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
  if(!pi_initialized)
    return;
  
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

static bool input_initialize_alldevices (void)
{
  if(pi_initialized)
    return true;
  
  struct pidata *pid;
  TCHAR tmp[80];
  memset(pi_mouse, 0, sizeof(pi_mouse));
  memset(pi_joystick, 0, sizeof(pi_joystick));
  memset(pi_keyboard, 0, sizeof(pi_keyboard));

  input_closeall();
  
  pid = pi_mouse;

  int mouse = open("/dev/input/mouse0", O_RDONLY);
  if(mouse != -1) {
    pid->type = PID_MOUSE;
    pid->name = my_strdup (_T("Mouse"));
    pid->configname = my_strdup (_T("MOUSE0"));
    pid->eventinput = 1;
    init_mouse_widgets(pid, 2, 2);
	  pid++;
	  num_mouse++;
    close(mouse);
  }
  
  pid = pi_joystick;

	int nrjoys = SDL_NumJoysticks();
	if (nrjoys > MAX_INPUT_DEVICES)
		nrjoys = MAX_INPUT_DEVICES;
	for (int cpt = 0; cpt < nrjoys; cpt++) {
    pid->type = PID_JOYSTICK;
		pid->sdlHandle = SDL_JoystickOpen(cpt);
		if(pid->sdlHandle != NULL) {
#ifdef USE_SDL2
		  if(SDL_JoystickNameForIndex(cpt) != NULL) {
    		pid->name = my_strdup(SDL_JoystickNameForIndex(cpt));
#else
		  if(SDL_JoystickName(cpt) != NULL) {
    		pid->name = my_strdup(SDL_JoystickName(cpt));
#endif
       my_trim(pid->name);
     } else {
        _stprintf(tmp, _T("Joystick%d"), cpt);
    		pid->name = my_strdup(tmp);
      }
  		if (_tcscmp(pid->name, "Sony PLAYSTATION(R)3 Controller") == 0 || _tcscmp(pid->name, "PLAYSTATION(R)3 Controller") == 0)
  			pid->IsPS3Controller = 1;
  		else
  			pid->IsPS3Controller = 0;

      _stprintf(tmp, _T("JOY%d"), cpt);
      pid->configname = my_strdup (tmp);

      pid->eventinput = 0;
      int axes = SDL_JoystickNumAxes(pid->sdlHandle);
      int buttons = SDL_JoystickNumButtons(pid->sdlHandle);
      init_joy_widgets(pid, axes, buttons);
	    pid++;
	    num_joystick++;
    }
	}
  
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
  input_initialize_alldevices();

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
  if(currprefs.input_tablet > TABLET_OFF) {
    // Mousehack active
    int x, y;
    SDL_GetMouseState(&x, &y);
	  setmousestate(0, 0, x, 1);
#ifdef USE_SDL2
	  setmousestate(0, 1, y, 1);
#else
		if(!adisplays.picasso_on)
		  y -= OFFSET_Y_ADJUST;
	  setmousestate(0, 1, y, 1);
#endif
		return;
  }

  // All mice for Raspi are with flag eventinput set.
  // input handling for this type in handle_msgpump()
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
		//*code = pi_keyboard[kb].buttonmappings[num];
		*code = num;
	}
	return IDEV_WIDGET_KEY;
}

static int init_kb (void)
{
  input_initialize_alldevices();
  
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
  // All keyboards for Raspi are with flag eventinput set.
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
  switch (type) {
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

static int ps3ControllerMap[] = { 14, 13, 15, 12, 10, 11, 3, 8, 9 , -1 };

static void read_joystick (void)
{
  int i, j;

	for (i = 0; i < num_joystick; i++) {
		struct pidata *pid = &pi_joystick[i];
		if (!pid->acquired)
			continue;

		int hat = SDL_JoystickGetHat(pid->sdlHandle, 0);
		int val = SDL_JoystickGetAxis(pid->sdlHandle, 0);
		if (hat & SDL_HAT_RIGHT)
			setjoystickstate(i, 0, 32767, 32767);
		else if (hat & SDL_HAT_LEFT)
			setjoystickstate(i, 0, -32767, 32767);
		else
			setjoystickstate(i, 0, val, 32767);

		val = SDL_JoystickGetAxis(pid->sdlHandle, 1);
		if (hat & SDL_HAT_UP)
			setjoystickstate(i, 1, -32767, 32767);
		else if (hat & SDL_HAT_DOWN) 
			setjoystickstate(i, 1, 32767, 32767);
		else
			setjoystickstate(i, 1, val, 32767);

    if (pid->IsPS3Controller) {
      for (j = 0; j < pid->buttons && ps3ControllerMap[j] >= 0; ++j)
        setjoybuttonstate (i, j, (SDL_JoystickGetButton(pid->sdlHandle, ps3ControllerMap[j]) & 1));

			// Simulate a top with button 4
			if (SDL_JoystickGetButton(pid->sdlHandle, 4))
				setjoystickstate(i, 1, -32767, 32767);
			// Simulate a right with button 5
			if (SDL_JoystickGetButton(pid->sdlHandle, 5))
				setjoystickstate(i, 0, 32767, 32767);
			// Simulate a bottom with button 6
			if (SDL_JoystickGetButton(pid->sdlHandle, 6))
				setjoystickstate(i, 1, 32767, 32767);
			// Simulate a left with button 7
			if (SDL_JoystickGetButton(pid->sdlHandle, 7))
				setjoystickstate(i, 0, -32767, 32767);
    } else {
      for (j = 0; j < pid->buttons; ++j)
        setjoybuttonstate (i, j, (SDL_JoystickGetButton(pid->sdlHandle, j) & 1));
    }
  }
}

static int init_joystick (void)
{
  input_initialize_alldevices();

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
      setid (uid, i, ID_BUTTON_OFFSET + 6, 0, port, port ? INPUTEVENT_JOY2_CD32_PLAY : INPUTEVENT_JOY1_CD32_PLAY, gp);
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
