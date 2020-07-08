#ifndef UAE_GAYLE_H
#define UAE_GAYLE_H

#include "uae/types.h"

void gayle_add_ide_unit (int ch, struct uaedev_config_info *ci, struct romconfig *rc);
bool gayle_ide_init(struct autoconfig_info*);
void gayle_free_units (void);
bool gayle_init_pcmcia(struct autoconfig_info *aci);

extern int gary_toenb; // non-existing memory access = bus error.

#define PCMCIA_COMMON_START 0x600000
#define PCMCIA_COMMON_SIZE 0x400000

#endif /* UAE_GAYLE_H */
