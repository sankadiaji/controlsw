/*
 * emac.c
 *
 *  Created on: 10-Jun-2024
 *      Author: Aartech
 */
#include "emac.h"

#include "HL_sys_vim.h"

#include "dp83869.h"

hdkif_t hdkif_data[MAX_EMAC_INSTANCE];

pbuf_t pack[5];
static uint8 data[5][100];
uint32 size_x = 470, size_y = 94;
uint8 emacAddress[6U] = {0x00U, 0x08U, 0xEEU, 0x03U, 0xA6U, 0x6CU};
uint32 emacPhyAddress = 1U;

void EMACInstConfig_1(hdkif_t *hdkif) {
    hdkif->emac_base      = EMAC_0_BASE;
    hdkif->emac_ctrl_base = EMAC_CTRL_0_BASE;
    hdkif->emac_ctrl_ram  = EMAC_CTRL_RAM_0_BASE;
    hdkif->mdio_base      = MDIO_BASE;
    hdkif->phy_addr       = 1U;
    /* (MISRA-C:2004 10.1/R) MISRA error reported with Code Composer Studio MISRA checker.  */
    hdkif->phy_autoneg        = &dp83869_AutoNegotiate;
    hdkif->phy_partnerability = &dp83869_PartnerAbilityGet;
}
uint32 EMACLinkSetup_1(hdkif_t *hdkif) {
    uint32 linkstat       = EMAC_ERR_CONNECT;
    uint16 partnr_ablty   = 0U;
    uint32 phyduplex      = EMAC_DUPLEX_HALF;
    volatile uint32 delay = 0xFFFFFU;

    if (dp83869_AutoNegotiate(
            (uint32)hdkif->mdio_base,
            (uint32)hdkif->phy_addr,
            (uint16)((uint16)ANAR_100HD | (uint16)ANAR_100FD | (uint16)ANAR_10HD | (uint16)ANAR_10FD)) == TRUE) {
        linkstat = EMAC_ERR_OK;
        /* (MISRA-C:2004 10.1/R) MISRA error reported with Code Composer Studio MISRA checker (due to use of & ?)  */
        (void)dp83869_PartnerAbilityGet(hdkif->mdio_base, hdkif->phy_addr, &partnr_ablty);

        /* Check for 100 Mbps and duplex capability */
        if ((partnr_ablty & ANAR_100FD) != 0U) {
            phyduplex = EMAC_DUPLEX_FULL;
        }
    } else {
        linkstat = EMAC_ERR_CONNECT;
    }

    /* Set the EMAC with the negotiation results if it is successful */
    if (linkstat == EMAC_ERR_OK) {
        EMACDuplexSet(hdkif->emac_base, phyduplex);
    }

    /* Wait for the MII to settle down */
    /*SAFETYMCUSW 134 S MR:12.2 <APPROVED> "LDRA Tool issue" */
    while (delay != 0U) {
        delay--;
    }

    return linkstat;
}

uint32 EMACHWInit_1_1(uint8_t macaddr[6U]) {
    uint32_t temp, channel;
    volatile uint32 phyID          = 0U;
    volatile uint32 delay          = 0xFFFU;
    uint32 phyIdReadCount          = 0xFFFFU;
    volatile uint32 phyLinkRetries = 0xFFFFU;
    hdkif_t *hdkif;
    rxch_t *rxch;
    uint32_t retVal   = EMAC_ERR_OK;
    uint32_t emacBase = 0U;
#if (EMAC_MII_ENABLE == 0U)
    uint16 partnr_spd;
#endif

    hdkif = &hdkif_data[0U];
    EMACInstConfig_1(hdkif);
    /* set MAC hardware address */
    for (temp = 0U; temp < EMAC_HWADDR_LEN; temp++) {
        hdkif->mac_addr[temp] = macaddr[(EMAC_HWADDR_LEN - 1U) - temp];
    }
    /*Initialize the EMAC, EMAC Control and MDIO modules. */
    EMACInit(hdkif->emac_ctrl_base, hdkif->emac_base);
    MDIOInit(hdkif->mdio_base, MDIO_FREQ_INPUT, MDIO_FREQ_OUTPUT);

    /*SAFETYMCUSW 134 S MR:12.2 <APPROVED> "LDRA Tool issue" */
    while (delay != 0U) {
        /*SAFETYMCUSW 134 S MR:12.2 <APPROVED> "LDRA Tool issue" */
        delay--;
    }

    /* Set the MAC Addresses in EMAC hardware */
    emacBase = hdkif->emac_base; /* MISRA Code Fix (12.2) */
    EMACMACSrcAddrSet(emacBase, hdkif->mac_addr);
    for (channel = 0U; channel < 8U; channel++) {
        emacBase = hdkif->emac_base;
        EMACMACAddrSet(emacBase, channel, hdkif->mac_addr, EMAC_MACADDR_MATCH);
    }

    /*SAFETYMCUSW 134 S MR:12.2 <APPROVED> "LDRA Tool issue" */
    while ((phyID == 0U) && (phyIdReadCount > 0U)) {
        phyID = dp83869_getid(hdkif->mdio_base, hdkif->phy_addr);
        phyIdReadCount--;
    }

    if (0U == phyID) {
        retVal = EMAC_ERR_CONNECT;
    } else {
    }

    if ((uint32)0U == ((MDIOPhyAliveStatusGet(hdkif->mdio_base) >> hdkif->phy_addr) & (uint32)0x01U)) {
        retVal = EMAC_ERR_CONNECT;
    } else {
    }

#if (EMAC_MII_ENABLE == 0U)
    PhyPartnerSpdGet(hdkif->mdio_base, hdkif->phy_addr, &partnr_spd);
    if ((partnr_spd & 2U) == 0U) {
        EMACRMIISpeedSet(hdkif->emac_base, EMAC_MACCONTROL_RMIISPEED);
    }
#endif

    if (!dp83869_LinkStatusGet(hdkif->mdio_base, (uint32)EMAC_PHYADDRESS, (uint32)phyLinkRetries)) {
        retVal = EMAC_ERR_CONNECT;
    } else {
    }

    if (EMACLinkSetup_1(hdkif) != EMAC_ERR_OK) {
        retVal = EMAC_ERR_CONNECT;
    } else {
    }

    /* The transmit and receive buffer descriptors are initialized here.
   * Also, packet buffers are allocated to the receive buffer descriptors.
   */

    EMACDMAInit(hdkif);

    /* Acknowledge receive and transmit interrupts for proper interrupt pulsing*/
    EMACCoreIntAck(hdkif->emac_base, (uint32)EMAC_INT_CORE0_RX);
    EMACCoreIntAck(hdkif->emac_base, (uint32)EMAC_INT_CORE0_TX);

    /* Enable GMII bit in the MACCONTROL Rgister*/
    /*SAFETYMCUSW 139 S MR:13.7 <APPROVED> "Parameter is taken as input from GUI." */
    EMACMIIEnable(hdkif->emac_base);

    /* Enable Broadcast if enabled in the GUI. */
    /*SAFETYMCUSW 139 S MR:13.7 <APPROVED> "Parameter is taken as input from GUI." */
#if (EMAC_BROADCAST_ENABLE)
    EMACRxBroadCastEnable(hdkif->emac_base, (uint32)EMAC_CHANNELNUMBER);
#else
    /*SAFETYMCUSW 1 J MR:14.1 <APPROVED> "If condition parameter is taken as input from GUI." */
    /*SAFETYMCUSW 1 J MR:14.1 <APPROVED> "If condition parameter is taken as input from GUI." */
    EMACRxBroadCastDisable(hdkif->emac_base, (uint32)EMAC_CHANNELNUMBER);
#endif

    /* Enable Broadcast if enabled in the GUI. */
    /*SAFETYMCUSW 139 S MR:13.7 <APPROVED> "Parameter is taken as input from GUI." */
#if (EMAC_UNICAST_ENABLE)
    EMACRxUnicastSet(hdkif->emac_base, (uint32)EMAC_CHANNELNUMBER);
#else
    /*SAFETYMCUSW 1 J MR:14.1 <APPROVED> "If condition parameter is taken as input from GUI." */
    EMACRxUnicastClear(hdkif->emac_base, (uint32)EMAC_CHANNELNUMBER);
#endif

    /*Enable Full Duplex or Half-Duplex mode based on GUI Input. */
    /*SAFETYMCUSW 139 S MR:13.7 <APPROVED> "Parameter is taken as input from GUI." */
#if (EMAC_FULL_DUPLEX_ENABLE)
    EMACDuplexSet(EMAC_0_BASE, (uint32)EMAC_DUPLEX_FULL);
#else
    /*SAFETYMCUSW 1 J MR:14.1 <APPROVED> "If condition arameter is taken as input from GUI." */
    EMACDuplexSet(EMAC_0_BASE, (uint32)EMAC_DUPLEX_HALF);
#endif

    /* Enable Loopback based on GUI Input */
    /*SAFETYMCUSW 139 S MR:13.7 <APPROVED> "Parameter is taken as input from GUI." */
#if (EMAC_LOOPBACK_ENABLE)
    EMACEnableLoopback(hdkif->emac_base);
#else
    /*SAFETYMCUSW 1 J MR:14.1 <APPROVED> "If condition parameter is taken as input from GUI." */
    EMACDisableLoopback(hdkif->emac_base);
#endif

    /* Enable Transmit and Transmit Interrupt */
    /*SAFETYMCUSW 139 S MR:13.7 <APPROVED> "Parameter is taken as input from GUI." */
#if (EMAC_TX_ENABLE)
    EMACTxEnable(hdkif->emac_base);
    EMACTxIntPulseEnable(
        hdkif->emac_base, hdkif->emac_ctrl_base, (uint32)EMAC_CHANNELNUMBER, (uint32)EMAC_CHANNELNUMBER);
#else
    /*SAFETYMCUSW 1 J MR:14.1 <APPROVED> "If condition parameter is taken as input from GUI." */
    /*SAFETYMCUSW 1 J MR:14.1 <APPROVED> "If condition parameter is taken as input from GUI." */
    EMACTxDisable(hdkif->emac_base);
    EMACTxIntPulseDisable(
        hdkif->emac_base, hdkif->emac_ctrl_base, (uint32)EMAC_CHANNELNUMBER, (uint32)EMAC_CHANNELNUMBER);
#endif

    /* Enable Receive and Receive Interrupt. Then start receiving by writing to the HDP register. */
    /*SAFETYMCUSW 139 S MR:13.7 <APPROVED> "Parameter is taken as input from GUI." */
#if (EMAC_RX_ENABLE)
    EMACNumFreeBufSet(hdkif->emac_base, (uint32)EMAC_CHANNELNUMBER, (uint32)MAX_RX_PBUF_ALLOC);
    EMACRxEnable(hdkif->emac_base);
    EMACRxIntPulseEnable(
        hdkif->emac_base, hdkif->emac_ctrl_base, (uint32)EMAC_CHANNELNUMBER, (uint32)EMAC_CHANNELNUMBER);
    rxch = &(hdkif->rxchptr);
    /* Write to the RX HDP for channel 0 */
    /*SAFETYMCUSW 45 D MR:21.1 <APPROVED> "Valid non NULL input parameters are assigned in this driver" */
    EMACRxHdrDescPtrWrite(hdkif->emac_base, (uint32)rxch->active_head, (uint32)EMAC_CHANNELNUMBER);
#else
    /*SAFETYMCUSW 1 J MR:14.1 <APPROVED> "If condition parameter is taken as input from GUI." */
    /*SAFETYMCUSW 1 J MR:14.1 <APPROVED> "If condition parameter is taken as input from GUI." */
    EMACRxDisable(hdkif->emac_base);
    EMACRxIntPulseDisable(
        hdkif->emac_base, hdkif->emac_ctrl_base, (uint32)EMAC_CHANNELNUMBER, (uint32)EMAC_CHANNELNUMBER);
#endif

    return retVal;
}

/* USER CODE BEGIN (2) */

void create_packet() {
    int i, j;

    for (i = 0; i < 5; i++) {
        pack[i].tot_len = size_y;
        pack[i].len     = size_y;
        for (j = 0; j < 6; j++) {
            data[i][j] = 0xffu;
        }
        for (j = 0; j < 6; j++) {
            data[i][j + 6] = emacAddress[j];
        }

        data[i][12] = 0;
        data[i][13] = 80;

        for (j = 0; j < 80; j++) {
            data[i][j + 14] = i + 5;
        }

        pack[i].payload = &data[i][0];
        if (i != 4)
            pack[i].next = &pack[i + 1];
    }
    pack[4].next = NULL;
}
/* USER CODE END */

/* USER CODE BEGIN (3) */
extern void emacrun() {
    _enable_IRQ();
    EMACHWInit_1_1(emacAddress);
    create_packet();
    EMACTransmit(&hdkif_data[0], &pack[0]);
    size_y = 600;
    size_y = 120;
    create_packet();
    EMACTransmit(&hdkif_data[0], &pack[0]);
}
