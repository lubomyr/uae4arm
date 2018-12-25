#include "sysconfig.h"
#include "sysdeps.h"
#include "config.h"
#include "uae.h"
#include "options.h"
#include "include/memory-uae.h"
#include "inputdevice.h"
#include "keyboard.h"
#include "rtgmodes.h"
#include "savestate.h"
#include "gfxboard.h"


void target_default_options (struct uae_prefs *p, int type)
{
  p->gfx_monitor.gfx_size.y = OFFSET_Y_ADJUST;
  
	p->picasso96_modeflags = RGBFF_CLUT | RGBFF_R5G6B5 | RGBFF_R8G8B8A8;
	
	p->cr[0].index = 0;
	p->cr[0].horiz = -1;
	p->cr[0].vert = -1;
	p->cr[0].lace = -1;
	p->cr[0].resolution = 0;
	p->cr[0].rate = 60.0;
	p->cr[0].ntsc = 1;
	p->cr[0].rtg = true;
	p->cr[0].inuse = true;
	_tcscpy (p->cr[0].label, _T("RTG"));

#ifdef USE_SDL2
	p->gfx_correct_aspect = 0;
	p->gfx_fullscreen_ratio = 100;
#else
	p->gfx_correct_aspect = 1;
	p->gfx_fullscreen_ratio = 100;
#endif
	p->kbd_led_num = -1; // No status on numlock
	p->kbd_led_scr = -1; // No status on scrollock
	p->kbd_led_cap = -1; // No status on capslock
}


void target_fixup_options (struct uae_prefs *p)
{
	p->rtgboards[0].rtgmem_type = GFXBOARD_UAE_Z3;

	if(z3base_adr == Z3BASE_REAL) {
  	// map Z3 memory at real address (0x40000000)
  	p->z3_mapping_mode = Z3MAPPING_REAL;
    p->z3autoconfig_start = z3base_adr;
	} else {
	  // map Z3 memory at UAE address (0x10000000)
  	p->z3_mapping_mode = Z3MAPPING_UAE;
    p->z3autoconfig_start = z3base_adr;
	}

  if(p->cs_cd32cd && p->cs_cd32nvram && (p->cs_compatible == CP_GENERIC || p->cs_compatible == 0)) {
    // Old config without cs_compatible, but other cd32-flags
    p->cs_compatible = CP_CD32;
    built_in_chipset_prefs(p);
  }

  if(p->cs_cd32cd && p->cartfile[0]) {
    p->cs_cd32fmv = 1;
  }
  
	p->picasso96_modeflags = RGBFF_CLUT | RGBFF_R5G6B5 | RGBFF_R8G8B8A8;
  p->gfx_resolution = p->gfx_monitor.gfx_size.width > 600 ? 1 : 0;
  
  if(p->cachesize > 0)
    p->fpu_no_unimplemented = 0;
  else
    p->fpu_no_unimplemented = 1;
  
#ifdef USE_SDL2
	p->gfx_correct_aspect = 0;
	p->gfx_fullscreen_ratio = 100;
#endif

  fix_apmodes(p);
}


void target_save_options (struct zfile *f, struct uae_prefs *p)
{
	cfgfile_write(f, _T("gfx_correct_aspect"), _T("%d"), p->gfx_correct_aspect);
	cfgfile_write(f, _T("gfx_fullscreen_ratio"), _T("%d"), p->gfx_fullscreen_ratio);
	cfgfile_write(f, _T("kbd_led_num"), _T("%d"), p->kbd_led_num);
	cfgfile_write(f, _T("kbd_led_scr"), _T("%d"), p->kbd_led_scr);
	cfgfile_write(f, _T("kbd_led_cap"), _T("%d"), p->kbd_led_cap);
}


int target_parse_option (struct uae_prefs *p, const char *option, const char *value)
{
  int result = (cfgfile_intval (option, value, "gfx_correct_aspect", &p->gfx_correct_aspect, 1)
    || cfgfile_intval (option, value, "gfx_fullscreen_ratio", &p->gfx_fullscreen_ratio, 1)
    || cfgfile_intval (option, value, "kbd_led_num", &p->kbd_led_num, 1)
    || cfgfile_intval (option, value, "kbd_led_scr", &p->kbd_led_scr, 1)
    || cfgfile_intval (option, value, "kbd_led_cap", &p->kbd_led_cap, 1)
    );
  if(!result) {
    result = cfgfile_intval (option, value, "move_y", &p->gfx_monitor.gfx_size.y, 1); // for compatibility only
    if(result)
      p->gfx_monitor.gfx_size.y += OFFSET_Y_ADJUST;
  }

  return result;
}


int handle_msgpump (void)
{
	int got = 0;
  SDL_Event rEvent;
  
	while (SDL_PollEvent(&rEvent)) {
		got = 1;
#ifndef USE_SDL2
		Uint8 *keystate = SDL_GetKeyState(NULL);
#endif
		
		switch (rEvent.type)
		{
  		case SDL_QUIT:
  			uae_quit();
  			break;
  			
  		case SDL_KEYDOWN:
#ifndef USE_SDL2
			  // Strangely in FBCON left window is seen as left alt ??
			  if (keyboard_type == 2) // KEYCODE_FBCON
			  {
				  if (keystate[SDLK_LCTRL] && (keystate[SDLK_LSUPER] || keystate[SDLK_LALT]) && (keystate[SDLK_RSUPER] || keystate[SDLK_MENU]))
				  {
					  uae_reset(0, 1);
					  break;
				  }
			  }
			  else
			  {
				  if (keystate[SDLK_LCTRL] && keystate[SDLK_LSUPER] && (keystate[SDLK_RSUPER] || keystate[SDLK_MENU]))
				  {
					  uae_reset(0, 1);
					  break;
				  }
			  }
#endif
			
			  // fix Caps Lock keypress shown as SDLK_UNKNOWN (scancode = 58)
			  if (rEvent.key.keysym.scancode == 58 && rEvent.key.keysym.sym == SDLK_UNKNOWN)
				  rEvent.key.keysym.sym = SDLK_CAPSLOCK;

  		  switch(rEvent.key.keysym.sym)
  		  {
  		    case SDLK_LCTRL: // Select key
  		      inputdevice_add_inputcode (AKS_ENTERGUI, 1, NULL);
  		      break;

				  case SDLK_LSHIFT: // Shift key
            inputdevice_do_keyboard(AK_LSH, 1);
            break;
            
  				default:
			      if (keyboard_type == KEYCODE_UNK)
			        inputdevice_translatekeycode(0, rEvent.key.keysym.sym, 1, false);
			      else
				      inputdevice_translatekeycode(0, rEvent.key.keysym.scancode, 1, false);
            				    				  
  				  break;
				}
        break;
        
  	  case SDL_KEYUP:
			  // fix Caps Lock keypress shown as SDLK_UNKNOWN (scancode = 58)
			  if (rEvent.key.keysym.scancode == 58 && rEvent.key.keysym.sym == SDLK_UNKNOWN)
				  rEvent.key.keysym.sym = SDLK_CAPSLOCK;

  	    switch(rEvent.key.keysym.sym)
  	    {
  		    case SDLK_LCTRL: // Select key
  		      break;

				  case SDLK_LSHIFT: // Shift key
            inputdevice_do_keyboard(AK_LSH, 0);
            break;
            
  				default:
  					if (keyboard_type == KEYCODE_UNK)
	  		      inputdevice_translatekeycode(0, rEvent.key.keysym.sym, 0, true);
	    			else
	    				inputdevice_translatekeycode(0, rEvent.key.keysym.scancode, 0, true);
  				  break;
  	    }
  	    break;
  	    
  	  case SDL_MOUSEBUTTONDOWN:
        if(currprefs.jports[0].id == JSEM_MICE || currprefs.jports[1].id == JSEM_MICE) {
    	    if(rEvent.button.button == SDL_BUTTON_LEFT) {
     	      setmousebuttonstate (0, 0, 1);
    	    }
    	    else if(rEvent.button.button == SDL_BUTTON_RIGHT)
    	      setmousebuttonstate (0, 1, 1);
        }
  	    break;

  	  case SDL_MOUSEBUTTONUP:
        if(currprefs.jports[0].id == JSEM_MICE || currprefs.jports[1].id == JSEM_MICE) {
    	    if(rEvent.button.button == SDL_BUTTON_LEFT) {
  	        setmousebuttonstate (0, 0, 0);
          }
    	    else if(rEvent.button.button == SDL_BUTTON_RIGHT)
    	      setmousebuttonstate (0, 1, 0);
        }
  	    break;
  	    
  		case SDL_MOUSEMOTION:
  		  if(currprefs.input_tablet == TABLET_OFF) {
          if(currprefs.jports[0].id == JSEM_MICE || currprefs.jports[1].id == JSEM_MICE) {
  			    int x, y;
    		    int mouseScale = currprefs.input_joymouse_multiplier / 2;
            x = rEvent.motion.xrel;
    				y = rEvent.motion.yrel;
  				  setmousestate(0, 0, x * mouseScale, 0);
        	  setmousestate(0, 1, y * mouseScale, 0);
          }
        }
        break;
		}
	}
	return got;
}


int main (int argc, char *argv[])
{
	printf("UAE4ARM %d.%d.%d, by TomB\n", UAEMAJOR, UAEMINOR, UAESUBREV);

  generic_main(argc, argv);
}
