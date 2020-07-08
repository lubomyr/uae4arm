#ifndef UAE_DEVICES_H
#define UAE_DEVICES_H

void devices_reset(int hardreset);
void devices_reset_ext(int hardreset);
void devices_vsync_pre(void);
void devices_hsync(void);
void devices_rethink(void);
void update_sound (double clk);
STATIC_INLINE void devices_update_sound(double clk)
{
  update_sound (clk);
}

void devices_update_sync(double svpos, double syncadjust);
void do_leave_program(void);
void virtualdevice_init(void);
void devices_restore_start(void);
void device_check_config(void);

typedef void (*DEVICE_INT)(int hardreset);
typedef void (*DEVICE_VOID)(void);

void device_add_hsync(DEVICE_VOID p);
void device_add_rethink(DEVICE_VOID p);
void device_add_reset(DEVICE_INT p);
void device_add_reset_imm(DEVICE_INT p);
void device_add_exit(DEVICE_VOID p);

#endif /* UAE_DEVICES_H */
