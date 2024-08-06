/*
 * emac.h
 *
 *  Created on: 10-Jun-2024
 *      Author: Aartech
 */

#ifndef EMAC_H_
#define EMAC_H_

#include "HL_emac.h"
#include "HL_emac_phyConfig.h"
#include "HL_hw_emac.h"
#include "HL_hw_emac_ctrl.h"
#include "HL_hw_reg_access.h"
#include "HL_mdio.h"
#include "HL_sys_common.h"

extern void EMACInstConfig(hdkif_t *hdkif);
extern uint32 EMACLinkSetup(hdkif_t *hdkif);
uint32 EMACHWInit(uint8_t macaddr[6U]);
extern void emacrun();

#endif /* EMAC_H_ */
