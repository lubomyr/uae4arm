#include "sysconfig.h"
#include "sysdeps.h"
#include "config.h"
#include "options.h"
#include "uae.h"
#include "include/memory-uae.h"
#include "newcpu.h"
#include "custom.h"
#include "xwin.h"
#include "drawing.h"
#include "inputdevice.h"
#include "keyboard.h"
#include "keybuf.h"
#include "gui.h"
#include <SDL.h>

char keyboard_type = 0;

static int capslock_key = -1;


#ifndef PANDORA

static struct uae_input_device_kbr_default keytrans_amiga_x11[256];

static int kb_np_x11[] = { SDLK_KP4, -1, SDLK_KP6, -1, SDLK_KP8, -1, SDLK_KP2, -1, SDLK_KP0, SDLK_KP5, -1, SDLK_KP_PERIOD, -1, SDLK_KP_ENTER, -1, -1 };
static int kb_ck_x11[] = { SDLK_LEFT, -1, SDLK_RIGHT, -1, SDLK_UP, -1, SDLK_DOWN, -1, SDLK_RMETA, -1, SDLK_RSHIFT, -1, -1 };
static int kb_se_x11[] = { SDLK_a, -1, SDLK_d, -1, SDLK_w, -1, SDLK_s, -1, SDLK_LMETA, -1, SDLK_LSHIFT, -1, -1 };
static int kb_np3_x11[] = { SDLK_KP4, -1, SDLK_KP6, -1, SDLK_KP8, -1, SDLK_KP2, -1, SDLK_KP0, SDLK_KP5, -1, SDLK_KP_PERIOD, -1, SDLK_KP_ENTER, -1, -1 };
static int kb_ck3_x11[] = { SDLK_LEFT, -1, SDLK_RIGHT, -1, SDLK_UP, -1, SDLK_DOWN, -1, SDLK_RCTRL, -1, SDLK_RSHIFT, -1, SDLK_RMETA, -1, -1 };
static int kb_se3_x11[] = { SDLK_a, -1, SDLK_d, -1, SDLK_w, -1, SDLK_s, -1, SDLK_LMETA, -1, SDLK_LSHIFT, -1, SDLK_LCTRL, -1, -1 };

static int kb_cd32_np_x11[] = { SDLK_KP4, -1, SDLK_KP6, -1, SDLK_KP8, -1, SDLK_KP2, -1, SDLK_KP0, SDLK_KP5, -1, SDLK_KP_PERIOD, -1, SDLK_KP7, -1, SDLK_KP9, -1, SDLK_KP_DIVIDE, -1, SDLK_KP_MINUS, -1, SDLK_KP_MULTIPLY, -1, -1 };
static int kb_cd32_ck_x11[] = { SDLK_LEFT, -1, SDLK_RIGHT, -1, SDLK_UP, -1, SDLK_DOWN, -1, SDLK_RCTRL, -1, SDLK_RMETA, -1, SDLK_KP7, -1, SDLK_KP9, -1, SDLK_KP_DIVIDE, -1, SDLK_KP_MINUS, -1, SDLK_KP_MULTIPLY, -1, -1 };
static int kb_cd32_se_x11[] = { SDLK_a, -1, SDLK_d, -1, SDLK_w, -1, SDLK_s, -1, SDLK_LMETA, -1, SDLK_LSHIFT, -1, SDLK_KP7, -1, SDLK_KP9, -1, SDLK_KP_DIVIDE, -1, SDLK_KP_MINUS, -1, SDLK_KP_MULTIPLY, -1, -1 };

static int *kbmaps_x11[] = {
	kb_np_x11, kb_ck_x11, kb_se_x11, kb_np3_x11, kb_ck3_x11, kb_se3_x11,
	kb_cd32_np_x11, kb_cd32_ck_x11, kb_cd32_se_x11
};


static struct uae_input_device_kbr_default keytrans_amiga_fbcon[] = {

	{ 9  - 8, INPUTEVENT_KEY_ESC },
	{ 67 - 8, INPUTEVENT_KEY_F1 },
	{ 68 - 8, INPUTEVENT_KEY_F2 },
	{ 69 - 8, INPUTEVENT_KEY_F3 },
	{ 70 - 8, INPUTEVENT_KEY_F4 },
	{ 71 - 8, INPUTEVENT_KEY_F5 },
	{ 72 - 8, INPUTEVENT_KEY_F6 },
	{ 73 - 8, INPUTEVENT_KEY_F7 },
	{ 74 - 8, INPUTEVENT_KEY_F8 },
	{ 75 - 8, INPUTEVENT_KEY_F9 },
	{ 76 - 8, INPUTEVENT_KEY_F10 },
    // { 95 -8 ,   INPUTEVENT_KEY_F11},
    // { 96 -8 ,   INPUTEVENT_KEY_F12},

	{ 49 - 8, INPUTEVENT_KEY_BACKQUOTE },

	{ 10 - 8, INPUTEVENT_KEY_1 },
	{ 11 - 8, INPUTEVENT_KEY_2 },
	{ 12 - 8, INPUTEVENT_KEY_3 },
	{ 13 - 8, INPUTEVENT_KEY_4 },
	{ 14 - 8, INPUTEVENT_KEY_5 },
	{ 15 - 8, INPUTEVENT_KEY_6 },
	{ 16 - 8, INPUTEVENT_KEY_7 },
	{ 17 - 8, INPUTEVENT_KEY_8 },
	{ 18 - 8, INPUTEVENT_KEY_9 },
	{ 19 - 8, INPUTEVENT_KEY_0 },
	{ 20 - 8, INPUTEVENT_KEY_SUB },
	{ 21 - 8, INPUTEVENT_KEY_EQUALS },
	{ 22 - 8, INPUTEVENT_KEY_BACKSPACE },

	{ 23 - 8, INPUTEVENT_KEY_TAB },
	{ 24 - 8, INPUTEVENT_KEY_Q },
	{ 25 - 8, INPUTEVENT_KEY_W },
	{ 26 - 8, INPUTEVENT_KEY_E },
	{ 27 - 8, INPUTEVENT_KEY_R },
	{ 28 - 8, INPUTEVENT_KEY_T },
	{ 29 - 8, INPUTEVENT_KEY_Y },
	{ 30 - 8, INPUTEVENT_KEY_U },
	{ 31 - 8, INPUTEVENT_KEY_I },
	{ 32 - 8, INPUTEVENT_KEY_O },
	{ 33 - 8, INPUTEVENT_KEY_P },
	{ 34 - 8, INPUTEVENT_KEY_LEFTBRACKET },
	{ 35 - 8, INPUTEVENT_KEY_RIGHTBRACKET },
	{ 36 - 8, INPUTEVENT_KEY_RETURN },

	{ 66 - 8, INPUTEVENT_KEY_CAPS_LOCK },
	{ 38 - 8, INPUTEVENT_KEY_A },
	{ 39 - 8, INPUTEVENT_KEY_S },
	{ 40 - 8, INPUTEVENT_KEY_D },
	{ 41 - 8, INPUTEVENT_KEY_F },
	{ 42 - 8, INPUTEVENT_KEY_G },
	{ 43 - 8, INPUTEVENT_KEY_H },
	{ 44 - 8, INPUTEVENT_KEY_J },
	{ 45 - 8, INPUTEVENT_KEY_K },
	{ 46 - 8, INPUTEVENT_KEY_L },
	{ 47 - 8, INPUTEVENT_KEY_SEMICOLON },
	{ 48 - 8, INPUTEVENT_KEY_SINGLEQUOTE },
	{ 51 - 8, INPUTEVENT_KEY_BACKSLASH },

	{ 50 - 8, INPUTEVENT_KEY_SHIFT_LEFT },
	{ 94 - 8, INPUTEVENT_KEY_LTGT },
	{ 52 - 8, INPUTEVENT_KEY_Z },
	{ 53 - 8, INPUTEVENT_KEY_X },
	{ 54 - 8, INPUTEVENT_KEY_C },
	{ 55 - 8, INPUTEVENT_KEY_V },
	{ 56 - 8, INPUTEVENT_KEY_B },
	{ 57 - 8, INPUTEVENT_KEY_N },
	{ 58 - 8, INPUTEVENT_KEY_M },
	{ 59 - 8, INPUTEVENT_KEY_COMMA },
	{ 60 - 8, INPUTEVENT_KEY_PERIOD },
	{ 61 - 8, INPUTEVENT_KEY_DIV },
	{ 62 - 8, INPUTEVENT_KEY_SHIFT_RIGHT },

	{ 37 - 8, INPUTEVENT_KEY_CTRL },
	{ 64 - 8, INPUTEVENT_KEY_ALT_LEFT },
	{ 65 - 8, INPUTEVENT_KEY_SPACE },

	{ 108 - 8, INPUTEVENT_KEY_ALT_RIGHT },

    //{ 78 -8 ,  INPUTEVENT_KEY_SCROLLOCK},

    //{ 77 -8 ,  INPUTEVENT_KEY_NUMLOCK},
	{ 106 - 8, INPUTEVENT_KEY_NP_DIV },
	{ 63 - 8, INPUTEVENT_KEY_NP_MUL },
	{ 82 - 8, INPUTEVENT_KEY_NP_SUB },

	{ 79 - 8, INPUTEVENT_KEY_NP_7 },
	{ 80 - 8, INPUTEVENT_KEY_NP_8 },
	{ 81 - 8, INPUTEVENT_KEY_NP_9 },
	{ 86 - 8, INPUTEVENT_KEY_NP_ADD },

	{ 83 - 8, INPUTEVENT_KEY_NP_4 },
	{ 84 - 8, INPUTEVENT_KEY_NP_5 },
	{ 85 - 8, INPUTEVENT_KEY_NP_6 },

	{ 87 - 8, INPUTEVENT_KEY_NP_1 },
	{ 88 - 8, INPUTEVENT_KEY_NP_2 },
	{ 89 - 8, INPUTEVENT_KEY_NP_3 },
	{ 104 - 8, INPUTEVENT_KEY_ENTER },         // The ENT from keypad..

	{ 90 - 8, INPUTEVENT_KEY_NP_0 },
	{ 91 - 8, INPUTEVENT_KEY_PERIOD },

	{ 111 - 8, INPUTEVENT_KEY_CURSOR_UP },
	{ 113 - 8, INPUTEVENT_KEY_CURSOR_LEFT },
	{ 116 - 8, INPUTEVENT_KEY_CURSOR_DOWN },
	{ 114 - 8, INPUTEVENT_KEY_CURSOR_RIGHT },


	{ 110 - 8, INPUTEVENT_KEY_NP_LPAREN },     // Map home   to left  parent (as fsuae)
	{ 112 - 8, INPUTEVENT_KEY_NP_RPAREN },     // Map pageup to right parent (as fsuae)

	{ 115 - 8, INPUTEVENT_KEY_HELP },          // Help mapped to End key (as fsuae)

	{ 119 - 8, INPUTEVENT_KEY_DEL },

	{ 133 - 8, INPUTEVENT_KEY_AMIGA_LEFT },   // Left amiga mapped to left Windows
	{ 134 - 8, INPUTEVENT_KEY_AMIGA_RIGHT },  // Right amiga mapped to right windows key.
	{ 135 - 8, INPUTEVENT_KEY_AMIGA_RIGHT },  // Right amiga mapped to Menu key.
	{ -1, 0 }
};

#endif // PANDORA

static struct uae_input_device_kbr_default keytrans_amiga[] = {

	{ SDLK_F1, INPUTEVENT_KEY_F1 },
	{ SDLK_F2, INPUTEVENT_KEY_F2 },
	{ SDLK_F3, INPUTEVENT_KEY_F3 },
	{ SDLK_F4, INPUTEVENT_KEY_F4 },
	{ SDLK_F5, INPUTEVENT_KEY_F5 },
	{ SDLK_F6, INPUTEVENT_KEY_F6 },
	{ SDLK_F7, INPUTEVENT_KEY_F7 },
	{ SDLK_F8, INPUTEVENT_KEY_F8 },
	{ SDLK_F9, INPUTEVENT_KEY_F9 },
	{ SDLK_F10, INPUTEVENT_KEY_F10 },

	{ SDLK_a, INPUTEVENT_KEY_A },
	{ SDLK_b, INPUTEVENT_KEY_B },
	{ SDLK_c, INPUTEVENT_KEY_C },
	{ SDLK_d, INPUTEVENT_KEY_D },
	{ SDLK_e, INPUTEVENT_KEY_E },
	{ SDLK_f, INPUTEVENT_KEY_F },
	{ SDLK_g, INPUTEVENT_KEY_G },
	{ SDLK_h, INPUTEVENT_KEY_H },
	{ SDLK_i, INPUTEVENT_KEY_I },
	{ SDLK_j, INPUTEVENT_KEY_J },
	{ SDLK_k, INPUTEVENT_KEY_K },
	{ SDLK_l, INPUTEVENT_KEY_L },
	{ SDLK_m, INPUTEVENT_KEY_M },
	{ SDLK_n, INPUTEVENT_KEY_N },
	{ SDLK_o, INPUTEVENT_KEY_O },
	{ SDLK_p, INPUTEVENT_KEY_P },
	{ SDLK_q, INPUTEVENT_KEY_Q },
	{ SDLK_r, INPUTEVENT_KEY_R },
	{ SDLK_s, INPUTEVENT_KEY_S },
	{ SDLK_t, INPUTEVENT_KEY_T },
	{ SDLK_u, INPUTEVENT_KEY_U },
	{ SDLK_v, INPUTEVENT_KEY_V },
	{ SDLK_w, INPUTEVENT_KEY_W },
	{ SDLK_x, INPUTEVENT_KEY_X },
	{ SDLK_y, INPUTEVENT_KEY_Y },
	{ SDLK_z, INPUTEVENT_KEY_Z },

	{ SDLK_0, INPUTEVENT_KEY_0 },
	{ SDLK_1, INPUTEVENT_KEY_1 },
	{ SDLK_2, INPUTEVENT_KEY_2 },
	{ SDLK_3, INPUTEVENT_KEY_3 },
	{ SDLK_4, INPUTEVENT_KEY_4 },
	{ SDLK_5, INPUTEVENT_KEY_5 },
	{ SDLK_6, INPUTEVENT_KEY_6 },
	{ SDLK_7, INPUTEVENT_KEY_7 },
	{ SDLK_8, INPUTEVENT_KEY_8 },
	{ SDLK_9, INPUTEVENT_KEY_9 },

  { SDLK_BACKSPACE, INPUTEVENT_KEY_BACKSPACE },
	{ SDLK_TAB, INPUTEVENT_KEY_TAB },
	{ SDLK_RETURN, INPUTEVENT_KEY_RETURN },
	{ SDLK_ESCAPE, INPUTEVENT_KEY_ESC },
	{ SDLK_SPACE, INPUTEVENT_KEY_SPACE },
	{ SDLK_QUOTE, INPUTEVENT_KEY_SINGLEQUOTE },
	{ SDLK_COMMA, INPUTEVENT_KEY_COMMA },
	{ SDLK_MINUS, INPUTEVENT_KEY_SUB },
	{ SDLK_PERIOD, INPUTEVENT_KEY_PERIOD },
	{ SDLK_SLASH, INPUTEVENT_KEY_DIV },

	{ SDLK_SEMICOLON, INPUTEVENT_KEY_SEMICOLON },
  { SDLK_EQUALS, INPUTEVENT_KEY_EQUALS },
	{ SDLK_LEFTBRACKET, INPUTEVENT_KEY_LEFTBRACKET },
	{ SDLK_BACKSLASH, INPUTEVENT_KEY_BACKSLASH },
	{ SDLK_RIGHTBRACKET, INPUTEVENT_KEY_RIGHTBRACKET },
  { SDLK_BACKQUOTE, INPUTEVENT_KEY_BACKQUOTE },
  { SDLK_DELETE, INPUTEVENT_KEY_DEL },

  { SDLK_LEFT, INPUTEVENT_KEY_CURSOR_LEFT },
  { SDLK_RIGHT, INPUTEVENT_KEY_CURSOR_RIGHT },
  { SDLK_UP, INPUTEVENT_KEY_CURSOR_UP },
  { SDLK_DOWN, INPUTEVENT_KEY_CURSOR_DOWN },

  { -1, 0 }
};

static struct uae_input_device_kbr_default *keytrans[] = {
	keytrans_amiga,
	keytrans_amiga,
	keytrans_amiga
};

static int kb_np[] = { SDLK_KP4, -1, SDLK_KP6, -1, SDLK_KP8, -1, SDLK_KP2, -1, SDLK_KP0, SDLK_KP5, -1, SDLK_KP_PERIOD, -1, SDLK_KP_ENTER, -1, -1 };
static int kb_ck[] = { SDLK_LEFT, -1, SDLK_RIGHT, -1, SDLK_UP, -1, SDLK_DOWN, -1, SDLK_RCTRL, -1, SDLK_RSHIFT, -1, -1 };
static int kb_se[] = { SDLK_a, -1, SDLK_d, -1, SDLK_w, -1, SDLK_s, -1, SDLK_LMETA, -1, SDLK_LSHIFT, -1, -1 };
static int kb_np3[] = { SDLK_KP4, -1, SDLK_KP6, -1, SDLK_KP8, -1, SDLK_KP2, -1, SDLK_KP0, SDLK_KP5, -1, SDLK_KP_PERIOD, -1, SDLK_KP_ENTER, -1, -1 };
static int kb_ck3[] = { SDLK_LEFT, -1, SDLK_RIGHT, -1, SDLK_UP, -1, SDLK_DOWN, -1, SDLK_RCTRL, -1, SDLK_RSHIFT, -1, SDLK_RMETA, -1, -1 };
static int kb_se3[] = { SDLK_a, -1, SDLK_d, -1, SDLK_w, -1, SDLK_s, -1, SDLK_LMETA, -1, SDLK_LSHIFT, -1, SDLK_LCTRL, -1, -1 };

static int kb_cd32_np[] = { SDLK_KP4, -1, SDLK_KP6, -1, SDLK_KP8, -1, SDLK_KP2, -1, SDLK_KP0, SDLK_KP5, SDLK_KP1, -1, SDLK_KP_PERIOD, SDLK_KP3, -1, SDLK_KP7, -1, SDLK_KP9, -1, SDLK_KP_DIVIDE, -1, SDLK_KP_MINUS, -1, SDLK_KP_MULTIPLY, -1, -1 };
static int kb_cd32_ck[] = { SDLK_LEFT, -1, SDLK_RIGHT, -1, SDLK_UP, -1, SDLK_DOWN, -1, SDLK_RCTRL, -1, SDLK_RMETA, -1, SDLK_KP7, -1, SDLK_KP9, -1, SDLK_KP_DIVIDE, -1, SDLK_KP_MINUS, -1, SDLK_KP_MULTIPLY, -1, -1 };
static int kb_cd32_se[] = { SDLK_a, -1, SDLK_d, -1, SDLK_w, -1, SDLK_s, -1, SDLK_LMETA, -1, SDLK_LSHIFT, -1, SDLK_KP7, -1, SDLK_KP9, -1, SDLK_KP_DIVIDE, -1, SDLK_KP_MINUS, -1, SDLK_KP_MULTIPLY, -1, -1 };

static int *kbmaps[] = {
	kb_np, kb_ck, kb_se, kb_np3, kb_ck3, kb_se3,
	kb_cd32_np, kb_cd32_ck, kb_cd32_se
};


#ifndef PANDORA

static struct uae_input_device_kbr_default *keytrans_x11[] = {
	keytrans_amiga_x11,
	keytrans_amiga_x11,
	keytrans_amiga_x11
};

static struct uae_input_device_kbr_default *keytrans_fbcon[] = {
	keytrans_amiga_fbcon,
	keytrans_amiga_fbcon,
	keytrans_amiga_fbcon
};


#include <X11/Xlib.h>

struct map_x11_to_event {
    char keysym[20];
    struct uae_input_device_default_node node[MAX_INPUT_SUB_EVENT];
};
static struct map_x11_to_event map_x11_to_event[] {
  { "f1",         INPUTEVENT_KEY_F1 },
  { "f2",         INPUTEVENT_KEY_F2 },
  { "f3",         INPUTEVENT_KEY_F3 },
  { "f4",         INPUTEVENT_KEY_F4 },
  { "f5",         INPUTEVENT_KEY_F5 },
  { "f6",         INPUTEVENT_KEY_F6 },
  { "f7",         INPUTEVENT_KEY_F7 },
  { "f8",         INPUTEVENT_KEY_F8 },
  { "f9",         INPUTEVENT_KEY_F9 },
  { "f10",        INPUTEVENT_KEY_F10 },
  { "escape",     INPUTEVENT_KEY_ESC },
  { "tab",        INPUTEVENT_KEY_TAB },
  { "control_l",  INPUTEVENT_KEY_CTRL, 0, INPUTEVENT_SPC_QUALIFIER_CONTROL },
  { "caps_lock",  INPUTEVENT_KEY_CAPS_LOCK, 0 },
  { "shift_l",    INPUTEVENT_KEY_SHIFT_LEFT, 0, INPUTEVENT_SPC_QUALIFIER_SHIFT },
  { "less",       INPUTEVENT_KEY_LTGT },
  { "alt_l",      INPUTEVENT_KEY_ALT_LEFT, 0, INPUTEVENT_SPC_QUALIFIER_ALT },
  { "super_l",    INPUTEVENT_KEY_AMIGA_LEFT, 0, INPUTEVENT_SPC_QUALIFIER_WIN },
  { "super_r",    INPUTEVENT_KEY_AMIGA_RIGHT, 0, INPUTEVENT_SPC_QUALIFIER_WIN },
  { "menu",       INPUTEVENT_KEY_AMIGA_RIGHT, 0, INPUTEVENT_SPC_QUALIFIER_WIN },
  { "alt_r",      INPUTEVENT_KEY_ALT_RIGHT, 0, INPUTEVENT_SPC_QUALIFIER_ALT },
  { "iso_level3_shift", INPUTEVENT_KEY_ALT_RIGHT, 0, INPUTEVENT_SPC_QUALIFIER_ALT },
  { "shift_r",    INPUTEVENT_KEY_SHIFT_RIGHT, 0, INPUTEVENT_SPC_QUALIFIER_SHIFT },
  { "space",      INPUTEVENT_KEY_SPACE },
	{ "up",         INPUTEVENT_KEY_CURSOR_UP },
	{ "down",       INPUTEVENT_KEY_CURSOR_DOWN },
	{ "left",       INPUTEVENT_KEY_CURSOR_LEFT },
	{ "right",      INPUTEVENT_KEY_CURSOR_RIGHT },
  { "help",       INPUTEVENT_KEY_HELP } ,
  { "next",       INPUTEVENT_KEY_HELP },
  { "delete",     INPUTEVENT_KEY_DEL },
  { "backspace",  INPUTEVENT_KEY_BACKSPACE },
  { "return",     INPUTEVENT_KEY_RETURN },
  { "a",          INPUTEVENT_KEY_A },
  { "b",          INPUTEVENT_KEY_B },
  { "c",          INPUTEVENT_KEY_C },
  { "d",          INPUTEVENT_KEY_D },
  { "e",          INPUTEVENT_KEY_E },
  { "f",          INPUTEVENT_KEY_F },
  { "g",          INPUTEVENT_KEY_G },
  { "h",          INPUTEVENT_KEY_H },
  { "i",          INPUTEVENT_KEY_I },
  { "j",          INPUTEVENT_KEY_J },
  { "k",          INPUTEVENT_KEY_K },
  { "l",          INPUTEVENT_KEY_L },
  { "m",          INPUTEVENT_KEY_M },
  { "n",          INPUTEVENT_KEY_N },
  { "o",          INPUTEVENT_KEY_O },
  { "p",          INPUTEVENT_KEY_P },
  { "q",          INPUTEVENT_KEY_Q },
  { "r",          INPUTEVENT_KEY_R },
  { "s",          INPUTEVENT_KEY_S },
  { "t",          INPUTEVENT_KEY_T },
  { "u",          INPUTEVENT_KEY_U },
  { "v",          INPUTEVENT_KEY_V },
  { "w",          INPUTEVENT_KEY_W },
  { "x",          INPUTEVENT_KEY_X },
  { "y",          INPUTEVENT_KEY_Y },
  { "z",          INPUTEVENT_KEY_Z },
  { "kp_enter",   INPUTEVENT_KEY_ENTER },
  { "kp_insert",  INPUTEVENT_KEY_NP_0 },
  { "kp_end",     INPUTEVENT_KEY_NP_1 },
  { "kp_down",    INPUTEVENT_KEY_NP_2 },
  { "kp_next",    INPUTEVENT_KEY_NP_3 },
  { "kp_left",    INPUTEVENT_KEY_NP_4 },
  { "kp_begin",   INPUTEVENT_KEY_NP_5 },
  { "kp_right",   INPUTEVENT_KEY_NP_6 },
  { "kp_home",    INPUTEVENT_KEY_NP_7 },
  { "kp_up",      INPUTEVENT_KEY_NP_8 },
  { "kp_prior",   INPUTEVENT_KEY_NP_9 },
  { "kp_delete",  INPUTEVENT_KEY_NP_PERIOD },
  { "kp_decimal", INPUTEVENT_KEY_NP_PERIOD },
  { "kp_add",     INPUTEVENT_KEY_NP_ADD },
  { "kp_subtract",INPUTEVENT_KEY_NP_SUB },
  { "kp_multiply",INPUTEVENT_KEY_NP_MUL },
  { "kp_divide",  INPUTEVENT_KEY_NP_DIV },
  { "parenleft",  INPUTEVENT_KEY_NP_LPAREN }, 
  { "home",       INPUTEVENT_KEY_NP_LPAREN }, 
  { "parenright", INPUTEVENT_KEY_NP_RPAREN },
  { "end",        INPUTEVENT_KEY_NP_RPAREN },
  { "backquote",  INPUTEVENT_KEY_BACKQUOTE },
  { "dead_circumflex", INPUTEVENT_KEY_BACKQUOTE },
  { "1",          INPUTEVENT_KEY_1 },
  { "2",          INPUTEVENT_KEY_2 },
  { "3",          INPUTEVENT_KEY_3 },
  { "4",          INPUTEVENT_KEY_4 },
  { "5",          INPUTEVENT_KEY_5 },
  { "6",          INPUTEVENT_KEY_6 },
  { "7",          INPUTEVENT_KEY_7 },
  { "8",          INPUTEVENT_KEY_8 },
  { "9",          INPUTEVENT_KEY_9 },
  { "0",          INPUTEVENT_KEY_0 },
  { "minus",      INPUTEVENT_KEY_SUB },
  { "dead_acute", INPUTEVENT_KEY_EQUALS },
  { "ssharp",     INPUTEVENT_KEY_BACKSLASH },
  { "udiaeresis", INPUTEVENT_KEY_LEFTBRACKET },
  { "plus",       INPUTEVENT_KEY_RIGHTBRACKET },
  { "odiaeresis", INPUTEVENT_KEY_SEMICOLON },
  { "adiaeresis", INPUTEVENT_KEY_SINGLEQUOTE },
  { "comma",      INPUTEVENT_KEY_COMMA },
  { "period",     INPUTEVENT_KEY_PERIOD },
  { "numbersign", INPUTEVENT_KEY_NUMBERSIGN },
    
	{ "f12",        INPUTEVENT_SPC_ENTERGUI },
  { "prior",      INPUTEVENT_SPC_FREEZEBUTTON },
  
  { "" }
};

extern TCHAR rawkeyboardlabels[256][20];

static void ParseX11Keymap(void)
{
  int keyIdx = 0;
  
  //printf("Parse X11 keymap\n");
  memset(rawkeyboardlabels, 0, sizeof(rawkeyboardlabels));
  
  Display *dpy = XOpenDisplay(NULL);
  if(dpy != NULL) {
    int min_keycode, max_keycode, keysyms_per_keycode;
    KeySym *origkeymap;
    
    XDisplayKeycodes (dpy, &min_keycode, &max_keycode);
    origkeymap = XGetKeyboardMapping (dpy, min_keycode, (max_keycode - min_keycode + 1), &keysyms_per_keycode);
    if (origkeymap != NULL) {
  	  //printf ("    KeyCode\tKey\n");
  	  
  	  KeySym *keymap = origkeymap;
  	  for (int i = min_keycode; i <= max_keycode; i++) {
      	int max;

        max = keysyms_per_keycode - 1;
      	while ((max >= 0) && (keymap[max] == NoSymbol))
	        max--;

        if(max > 0 && keymap[0] != NoSymbol) {
          const char *sym = XKeysymToString (keymap[0]);
       	  if(sym != NULL) {
            //printf("    %3d    \t%s\t", i, sym);
         	  
         	  strncpy(rawkeyboardlabels[i], sym, 19);
         	  
         	  char tmp[20];
       	    strncpy(tmp, sym, 19);
         	  for(int c = 0; c < strlen(tmp); ++c)
         	    tmp[c] = tolower(tmp[c]);
         	  int evt;
         	  for(evt = 0; map_x11_to_event[evt].keysym[0] != 0; ++evt){
         	    if(!strcmp(tmp, map_x11_to_event[evt].keysym)) {
                keytrans_amiga_x11[keyIdx].scancode = i;
                memcpy(keytrans_amiga_x11[keyIdx].node, map_x11_to_event[evt].node, sizeof(map_x11_to_event[evt].node));

           	    //printf("evt %d\n", keytrans_amiga_x11[keyIdx].node[0].evt);

                switch(keytrans_amiga_x11[keyIdx].node[0].evt) {
                  case INPUTEVENT_KEY_NP_4: kb_np_x11[0] = kb_np3_x11[0] = kb_cd32_np_x11[0] = i; break;
                  case INPUTEVENT_KEY_NP_6: kb_np_x11[2] = kb_np3_x11[2] = kb_cd32_np_x11[2] = i; break;
                  case INPUTEVENT_KEY_NP_8: kb_np_x11[4] = kb_np3_x11[4] = kb_cd32_np_x11[4] = i; break;
                  case INPUTEVENT_KEY_NP_2: kb_np_x11[6] = kb_np3_x11[6] = kb_cd32_np_x11[6] = i; break;
                  case INPUTEVENT_KEY_NP_0: kb_np_x11[8] = kb_np3_x11[8] = kb_cd32_np_x11[8] = i; break;
                  case INPUTEVENT_KEY_NP_5: kb_np_x11[9] = kb_np3_x11[9] = kb_cd32_np_x11[9] = i; break;
                  case INPUTEVENT_KEY_NP_PERIOD: kb_np_x11[11] = kb_np3_x11[11] = kb_cd32_np_x11[11] = i; break;
                  case INPUTEVENT_KEY_ENTER: kb_np_x11[13] = kb_np3_x11[13] = i; break;

                  case INPUTEVENT_KEY_CURSOR_LEFT: kb_ck_x11[0] = kb_ck3_x11[0] = kb_cd32_ck_x11[0] = i; break;
                  case INPUTEVENT_KEY_CURSOR_RIGHT: kb_ck_x11[2] = kb_ck3_x11[2] = kb_cd32_ck_x11[2] = i; break;
                  case INPUTEVENT_KEY_CURSOR_UP: kb_ck_x11[4] = kb_ck3_x11[4] = kb_cd32_ck_x11[4] = i; break;
                  case INPUTEVENT_KEY_CURSOR_DOWN: kb_ck_x11[6] = kb_ck3_x11[6] = kb_cd32_ck_x11[6] = i; break;
                  case INPUTEVENT_KEY_RETURN: kb_ck_x11[8] = kb_ck3_x11[8] = kb_cd32_ck_x11[8] = i; break;
                  case INPUTEVENT_KEY_SHIFT_RIGHT: kb_ck_x11[10] = kb_ck3_x11[10] = kb_cd32_ck_x11[10] = i; break;
                  case INPUTEVENT_KEY_AMIGA_RIGHT: kb_ck3_x11[12] = i; break;
                  
                  case INPUTEVENT_KEY_A: kb_se_x11[0] = kb_se3_x11[0] = kb_cd32_se_x11[0] = i; break;
                  case INPUTEVENT_KEY_D: kb_se_x11[2] = kb_se3_x11[2] = kb_cd32_se_x11[2] = i; break;
                  case INPUTEVENT_KEY_W: kb_se_x11[4] = kb_se3_x11[4] = kb_cd32_se_x11[4] = i; break;
                  case INPUTEVENT_KEY_S: kb_se_x11[6] = kb_se3_x11[6] = kb_cd32_se_x11[6] = i; break;
                  case INPUTEVENT_KEY_ALT_LEFT: kb_se_x11[8] = kb_se3_x11[8] = kb_cd32_se_x11[8] = i; break;
                  case INPUTEVENT_KEY_SHIFT_LEFT: kb_se_x11[10] = kb_se3_x11[10] = kb_cd32_se_x11[10] = i; break;
                  case INPUTEVENT_KEY_X: kb_se3_x11[12] = kb_cd32_se_x11[12] = i; break;

                  case INPUTEVENT_KEY_NP_7: kb_cd32_np_x11[13] = kb_cd32_ck_x11[12] = kb_cd32_se_x11[12] = i; break;
                  case INPUTEVENT_KEY_NP_9: kb_cd32_np_x11[15] = kb_cd32_ck_x11[14] = kb_cd32_se_x11[14] = i; break;
                  case INPUTEVENT_KEY_NP_DIV: kb_cd32_np_x11[17] = kb_cd32_ck_x11[16] = kb_cd32_se_x11[16] = i; break;
                  case INPUTEVENT_KEY_NP_SUB: kb_cd32_np_x11[19] = kb_cd32_ck_x11[18] = kb_cd32_se_x11[18] = i; break;
                  case INPUTEVENT_KEY_NP_MUL: kb_cd32_np_x11[21] = kb_cd32_ck_x11[20] = kb_cd32_se_x11[20] = i; break;
                    
                  case INPUTEVENT_KEY_CAPS_LOCK:
                    capslock_key = i;
                    break;
                }
                
           	    ++keyIdx;
         	      break;
         	    }
         	  }
         	  //if(map_x11_to_event[evt].keysym[0] == 0) {
         	  //  printf("---\n");
         	  //}
       	  }
        }
	      keymap += keysyms_per_keycode;
      }
      XFree ((char *) origkeymap);
    }
    
    XCloseDisplay(dpy);
  }
  
  keytrans_amiga_x11[keyIdx].scancode = -1;
  keytrans_amiga_x11[keyIdx].node[0].evt = 0;
}

#endif // PANDORA


int getcapslockstate (void)
{
  Uint8 *keystate = SDL_GetKeyState(NULL);
  return keystate[SDLK_CAPSLOCK] ? 1 : 0;
}
void setcapslockstate (int state)
{
  Uint8 *keystate = SDL_GetKeyState(NULL);
  int currstate = keystate[SDLK_CAPSLOCK] ? 1 : 0;
  if(currstate != state)
  {
    if(currstate)
      inputdevice_translatekeycode (0, capslock_key, 1, false);
    else
      inputdevice_translatekeycode (0, capslock_key, 0, true);
  }
}


void getcapslock (void)
{
  Uint8 *keystate = SDL_GetKeyState(NULL);
  if(keystate[SDLK_CAPSLOCK])
    inputdevice_translatekeycode (0, capslock_key, 1, false);
  else
    inputdevice_translatekeycode (0, capslock_key, 0, true);
}


void keyboard_settrans (void)
{
#if !defined(USE_SDL2) && !defined(PANDORA)
	char vid_drv_name[32];
	// get display type...
	SDL_VideoDriverName(vid_drv_name, sizeof(vid_drv_name));
	if (strcmp(vid_drv_name, "x11") == 0)	{
	  ParseX11Keymap();
		keyboard_type = KEYCODE_X11;
		inputdevice_setkeytranslation(keytrans_x11, kbmaps_x11);
	}	else  if (strcmp(vid_drv_name, "fbcon") == 0) {
		keyboard_type = KEYCODE_FBCON;
		capslock_key = 66 - 8;
		inputdevice_setkeytranslation(keytrans_fbcon, kbmaps);
	}	else 
#endif
	{
		keyboard_type = KEYCODE_UNK;
		capslock_key = SDLK_CAPSLOCK;
		inputdevice_setkeytranslation(keytrans, kbmaps);
	}
}
