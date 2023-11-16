#define LOG_TAG "LCM"

#ifndef BUILD_LK
#include <linux/string.h>
#include <linux/kernel.h>
#endif

#include "lcm_drv.h"

#ifdef BUILD_LK
#include <platform/upmu_common.h>
#include <platform/mt_gpio.h>
#include <platform/mt_i2c.h>
#include <platform/mt_pmic.h>
#include <string.h>
#elif defined(BUILD_UBOOT)
#include <asm/arch/mt_gpio.h>
#else
#endif

#ifdef BUILD_LK
#define LCM_LOGI(string, args...)  dprintf(0, "[LK/"LOG_TAG"]"string, ##args)
#define LCM_LOGD(string, args...)  dprintf(1, "[LK/"LOG_TAG"]"string, ##args)
#else
#define LCM_LOGI(fmt, args...)  pr_debug("[KERNEL/"LOG_TAG"]"fmt, ##args)
#define LCM_LOGD(fmt, args...)  pr_debug("[KERNEL/"LOG_TAG"]"fmt, ##args)
#endif

//prize-add tp enter low power mode-pengzhipeng-20190109-start
//extern void fts_enter_low_power(void);
//int fts_reset_proc(int hdelayms);
//prize-add tp enter low power mode-pengzhipeng-20190109-end

static struct LCM_UTIL_FUNCS lcm_util;

#define SET_RESET_PIN(v)			(lcm_util.set_reset_pin((v)))
#define MDELAY(n)					(lcm_util.mdelay(n))

/* --------------------------------------------------------------------------- */
/* Local Functions */
/* --------------------------------------------------------------------------- */
#define dsi_set_cmdq_V2(cmd, count, ppara, force_update) \
	lcm_util.dsi_set_cmdq_V2(cmd, count, ppara, force_update)
#define dsi_set_cmdq(pdata, queue_size, force_update) \
	lcm_util.dsi_set_cmdq(pdata, queue_size, force_update)
#define wrtie_cmd(cmd) \
	lcm_util.dsi_write_cmd(cmd)
#define write_regs(addr, pdata, byte_nums) \
	lcm_util.dsi_write_regs(addr, pdata, byte_nums)
#define read_reg(cmd) \
	lcm_util.dsi_dcs_read_lcm_reg(cmd)
#define read_reg_v2(cmd, buffer, buffer_size) \
	lcm_util.dsi_dcs_read_lcm_reg_v2(cmd, buffer, buffer_size)
#define set_gpio_lcd_enp(cmd) \
		lcm_util.set_gpio_lcd_enp_bias(cmd)
		

static const unsigned char LCD_MODULE_ID = 0x01;
/* --------------------------------------------------------------------------- */
/* Local Constants */
/* --------------------------------------------------------------------------- */
#define LCM_DSI_CMD_MODE	0
#define FRAME_WIDTH  										 (1080)
#define FRAME_HEIGHT 										 (2400)

#define REGFLAG_DELAY             							 0xFFFA
#define REGFLAG_UDELAY             							 0xFFFB
#define REGFLAG_PORT_SWAP									 0xFFFC
#define REGFLAG_END_OF_TABLE      							 0xFFFD   // END OF REGISTERS MARKER

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

//extern void ilitek_set_rst_pin(int value);
/* --------------------------------------------------------------------------- */
/* Local Variables */
/* --------------------------------------------------------------------------- */

struct LCM_setting_table
{
    unsigned int cmd;
    unsigned char count;
    unsigned char para_list[64];
};

static struct LCM_setting_table lcm_suspend_setting[] =
{
      {0x28, 1, {0x00}},
    {REGFLAG_DELAY, 20, {}},
    {0x10, 1, {0x00}},
    {REGFLAG_DELAY, 120, {} },
    {REGFLAG_END_OF_TABLE, 0x00, {}}
    //prize-add tp enter low power mode-pengzhipeng-20190109-start

};

static struct LCM_setting_table lcm_initialization_setting[] =
{
{0xFF,03,{0x78,0x07,0x06}},
{0x1F,01,{0x0C}},
{0xFF,03,{0x78,0x07,0x00}},
{0x01,0,{}},            //SW RESET
{REGFLAG_DELAY,5, {}},				//此延时必须保留	
{0x01,0,{}},               //SW RESET
{REGFLAG_DELAY,5, {}},				//此延时必须保留
{0xFF,03,{0x78,0x07,0x0B}},
{0x80,01,{0x01}},         //Barcode EN
{0xFF,03,{0x78,0x07,0x00}},                 
{0x01,0,{}},              //SW RESET
{REGFLAG_DELAY,5, {}},
{0xFF,03,{0x78,0x07,0x01}},		//page1
{0x00,01,{0x62}},     //
{0x01,01,{0x51}},     //
{0x02,01,{0x00}},     //FTI_A_RISE
{0x03,01,{0x6A}},     //FTI_A_FALL
{0x04,01,{0x00}},
{0x05,01,{0x00}},
{0x06,01,{0x00}},
{0x07,01,{0x00}},
{0x08,01,{0xA9}},     //CLK RISE 
{0x09,01,{0x0A}},     //DUMMY CKV CLK FALL
{0x0a,01,{0x30}},
{0x0B,01,{0x00}},     //
{0x0C,01,{0x03}},     //CLW_A1_RISE
{0x0E,01,{0x04}},     //CLW_A1_FALL 
{0x31,01,{0x3D}},     //GOUTR01 CTSW_VCOM
{0x32,01,{0x3D}},     //GOUTR02 CTSW_VCOM
{0x33,01,{0x02}},     //GOUTR03 CTSW
{0x34,01,{0x3C}},     //GOUTR04 EN_Touch
{0x35,01,{0x0C}},     //GOUTR05 VGL_G
{0x36,01,{0x0C}},     //GOUTR06 VGL_G
{0x37,01,{0x30}},     //GOUTR07 MUXB2
{0x38,01,{0x33}},     //GOUTR08 MUXB1
{0x39,01,{0x2F}},     //GOUTR09 MUXG2
{0x3A,01,{0x32}},     //GOUTR10 MUXG1
{0x3B,01,{0x2E}},     //GOUTR11 MUXR2
{0x3C,01,{0x31}},     //GOUTR12 MUXR1
{0x3D,01,{0x07}},     //GOUTR13 
{0x3E,01,{0x07}},     //GOUTR14 
{0x3F,01,{0x07}},     //GOUTR15 
{0x40,01,{0x07}},     //GOUTR16 
{0x41,01,{0x10}},     //GOUTR17 CKBL
{0x42,01,{0x12}},     //GOUTR18 CKL
{0x43,01,{0x2C}},     //GOUTR19 RESET
{0x44,01,{0x29}},     //GOUTR20 VGH_G_L
{0x45,01,{0x29}},     //GOUTR21 VGH_G_L
{0x46,01,{0x01}},     //GOUTR22 CN
{0x47,01,{0x00}},     //GOUTR23 CNB
{0x48,01,{0x08}},     //GOUTR24 STVL
{0x49,01,{0x3D}},     //GOUTL01 CTSW_VCOM 
{0x4A,01,{0x3D}},     //GOUTL02 CTSW_VCOM 
{0x4B,01,{0x02}},     //GOUTL03 CTSW 
{0x4C,01,{0x3C}},     //GOUTL04 EN_Touch 
{0x4D,01,{0x0C}},     //GOUTL05 VGL_G 
{0x4E,01,{0x0C}},     //GOUTL06 VGL_G 
{0x4F,01,{0x30}},     //GOUTL07 MUXB2 
{0x50,01,{0x33}},     //GOUTL08 MUXB1 
{0x51,01,{0x2F}},     //GOUTL09 MUXG2 
{0x52,01,{0x32}},     //GOUTL10 MUXG1 
{0x53,01,{0x2E}},     //GOUTL11 MUXR2 
{0x54,01,{0x31}},     //GOUTL12 MUXR1 
{0x55,01,{0x07}},     //GOUTL13  
{0x56,01,{0x07}},     //GOUTL14  
{0x57,01,{0x07}},     //GOUTL15  
{0x58,01,{0x07}},     //GOUTL16  
{0x59,01,{0x11}},     //GOUTL17 CKBR 
{0x5A,01,{0x13}},     //GOUTL18 CKR 
{0x5B,01,{0x2C}},     //GOUTL19 RESET 
{0x5C,01,{0x29}},     //GOUTL20 VGH_G_R 
{0x5D,01,{0x29}},     //GOUTL21 VGH_G_R 
{0x5E,01,{0x01}},     //GOUTL22 CN 
{0x5F,01,{0x00}},     //GOUTL23 CNB 
{0x60,01,{0x09}},     //GOUTL24 STVR 
{0x61,01,{0x3D}},     //GOUTR01 CTSW_VCOM
{0x62,01,{0x3D}},     //GOUTR02 CTSW_VCOM
{0x63,01,{0x02}},     //GOUTR03 CTSW
{0x64,01,{0x3C}},     //GOUTR04 EN_Touch
{0x65,01,{0x0C}},     //GOUTR05 VGL_G
{0x66,01,{0x0C}},     //GOUTR06 VGL_G
{0x67,01,{0x30}},     //GOUTR07 MUXB2
{0x68,01,{0x33}},     //GOUTR08 MUXB1
{0x69,01,{0x2F}},     //GOUTR09 MUXG2
{0x6A,01,{0x32}},     //GOUTR10 MUXG1
{0x6B,01,{0x2E}},     //GOUTR11 MUXR2
{0x6C,01,{0x31}},     //GOUTR12 MUXR1
{0x6D,01,{0x07}},     //GOUTR13 
{0x6E,01,{0x07}},     //GOUTR14 
{0x6F,01,{0x07}},     //GOUTR15 
{0x70,01,{0x07}},     //GOUTR16 
{0x71,01,{0x13}},     //GOUTR17 CKBL
{0x72,01,{0x11}},     //GOUTR18 CKL
{0x73,01,{0x2C}},     //GOUTR19 RESET
{0x74,01,{0x29}},     //GOUTR20 VGH_G_L
{0x75,01,{0x29}},     //GOUTR21 VGH_G_L
{0x76,01,{0x01}},     //GOUTR22 CN
{0x77,01,{0x00}},     //GOUTR23 CNB
{0x78,01,{0x09}},     //GOUTR24 STVL
{0x79,01,{0x3D}},     //GOUTL01 CTSW_VCOM 
{0x7A,01,{0x3D}},     //GOUTL02 CTSW_VCOM 
{0x7B,01,{0x02}},     //GOUTL03 CTSW 
{0x7C,01,{0x3C}},     //GOUTL04 EN_Touch 
{0x7D,01,{0x0C}},     //GOUTL05 VGL_G 
{0x7E,01,{0x0C}},     //GOUTL06 VGL_G 
{0x7F,01,{0x30}},     //GOUTL07 MUXB2 
{0x80,01,{0x33}},     //GOUTL08 MUXB1 
{0x81,01,{0x2F}},     //GOUTL09 MUXG2 
{0x82,01,{0x32}},     //GOUTL10 MUXG1 
{0x83,01,{0x2E}},     //GOUTL11 MUXR2 
{0x84,01,{0x31}},     //GOUTL12 MUXR1 
{0x85,01,{0x07}},     //GOUTL13  
{0x86,01,{0x07}},     //GOUTL14  
{0x87,01,{0x07}},     //GOUTL15  
{0x88,01,{0x07}},     //GOUTL16  
{0x89,01,{0x12}},     //GOUTL17 CKBR 
{0x8A,01,{0x10}},     //GOUTL18 CKR 
{0x8B,01,{0x2C}},     //GOUTL19 RESET 
{0x8C,01,{0x29}},     //GOUTL20 VGH_G_R 
{0x8D,01,{0x29}},     //GOUTL21 VGH_G_R 
{0x8E,01,{0x01}},     //GOUTL22 CN 
{0x8F,01,{0x00}},     //GOUTL23 CNB 
{0x90,01,{0x08}},     //GOUTL24 STVR 
{0xA0,01,{0x4B}},     //For CGOUT RESET
{0xA1,01,{0x4A}},     //For CGOUT RESET
{0xA7,01,{0x10}},     //For CGOUT RESET
{0xB2,01,{0x02}},
{0xB3,01,{0x10}},
{0xC6,01,{0x60}},
{0xCA,01,{0x00}},
{0xD1,01,{0x11}},
{0xD2,01,{0x10}},
{0xD3,01,{0x41}},
{0xD5,01,{0x12}},
{0xD8,01,{0x20}},
{0xD9,01,{0x01}},
{0xDA,01,{0x0A}},
{0xDB,01,{0x02}},
{0xDC,01,{0x86}},
{0xDD,01,{0x98}},
{0xDF,01,{0x46}},
{0xE0,01,{0x16}},
{0xE1,01,{0xA8}},
{0xE3,01,{0x13}},
{0xE4,01,{0x72}},
{0xE5,01,{0x0E}},
{0xE6,01,{0x23}},
{0xED,01,{0x35}},
{0xF4,01,{0x54}},    
{0xFF,03,{0x78,0x07,0x05}},
{0x30,01,{0x45}},	//STCH_WID_ON_SEQ
{0xFF,03,{0x78,0x07,0x02}},
{0x06,01,{0x6E}},  //BIST FR
{0x07,01,{0x6E}},  //BIST FR
{0x0E,01,{0x08}},  //VBP
{0x0F,01,{0x26}},  //VFP
{0x40,01,{0x0E}},    //CKH T8
{0x42,01,{0x07}},    //CKH GAP
{0x43,01,{0x16}},    //CKH WIDTH
{0x46,01,{0x22}},    //CKH DUMMY
{0x53,01,{0x05}},    //SRC SDT
//****************V line****************************

{0x4E,01,{0x04}}, //CKH_3_to_1_connect d[2]
{0x47,01,{0x00}}, //Frane&line CKH 123456 =>654321
{0xFF,03,{0x78,0x07,0x05}},
{0x3C,01,{0x00}},		//follow abnormal seq 01:TM slp in seq
{0x02,01,{0x00}},       //VCOM
{0x03,01,{0x7E}},       //VCOM 
{0x52,01,{0x56}},       //VGH 9V
{0x56,01,{0x51}},       //VGHO 8V
{0x5A,01,{0x51}},       //VGLO -8V
{0x54,01,{0x56}},       //VGL -9V
{0x5B,01,{0x83}},       //GVDDP +5.1V
{0x5C,01,{0x83}},       //GVDDP -5.1V
{0x2D,01,{0x56}},
{0x2E,01,{0x76}},
{0x2B,01,{0x08}},
{0xB5,01,{0x56}},
{0xB7,01,{0x76}},
{0xFF,03,{0x78,0x07,0x06}}, // switch to page 6
{0x0A,01,{0x0C}},		//NL_SEL = 1 NL[10:8] = 4  2+(2NL) = 2400
{0x0B,01,{0xAF}}, 		//NL[7:0]  = 91
{0xFF,03,{0x78,0x07,0x07}}, //
{0x03,01,{0x10}},       //GAMMA127 OFF
//************Long H timing setting********************
{0xFF,03,{0x78,0x07,0x0E}},
{0x00,01,{0xA3}}, //LH mode
{0x02,01,{0x0C}}, //VBP
{0x40,01,{0x07}}, //8 Unit
{0x49,01,{0x2C}},  //Unit line = 300
{0x47,01,{0x90}},  //Unit line = 300
{0x45,01,{0x0A}}, 	//TP term2_unit 0 = 170us 
{0x46,01,{0xFD}}, 	//TP term2_unit 0 = 170us 
{0x4D,01,{0xBF}},  //RTN = 6.00us
{0x51,01,{0x00}},  //RTN = 6.00us
{0xB0,01,{0x21}}, //term1 number 
{0xB1,01,{0x60}},
{0xC0,01,{0x12}}, //TP3_DUM_NUM 2019/12/11 03=>63 rescan 1 line
{0xC8,01,{0x60}},
{0xC9,01,{0x60}},
{0xE0,01,{0x0E}},    //TP CKH T8
{0xE2,01,{0x07}},    //TP CKH GAP
{0xE3,01,{0x16}},    //TP CKH WIDTH		 
{0xE4,01,{0x04}},
{0xE5,01,{0x04}},
{0xE6,01,{0x00}},
{0xE7,01,{0x05}},
{0xE8,01,{0x00}},
{0xE9,01,{0x02}},
{0xEA,01,{0x07}},

//***********TP modulation  ***************************
{0xFF,03,{0x78,0x07,0x0E}},
{0x4B,01,{0x09}},
{0x07,01,{0x20}},
{0xFF,03,{0x78,0x07,0x0C}},
{0x00,01,{0x12}},
{0x01,01,{0x22}},
{0x02,01,{0x12}},
{0x03,01,{0x27}},
{0x04,01,{0x12}},
{0x05,01,{0x23}},
{0x06,01,{0x12}},
{0x07,01,{0x26}},
{0x08,01,{0x11}},
{0x09,01,{0x18}},
{0x0A,01,{0x11}},
{0x0B,01,{0x1C}},
{0x0C,01,{0x11}},
{0x0D,01,{0x1F}},
{0x0E,01,{0x11}},
{0x0F,01,{0x14}},
{0x10,01,{0x11}},
{0x11,01,{0x19}},
{0x12,01,{0x11}},
{0x13,01,{0x1A}},
{0x14,01,{0x11}},
{0x15,01,{0x20}},
{0x16,01,{0x11}},
{0x17,01,{0x1D}},
{0x18,01,{0x11}},
{0x19,01,{0x15}},
{0x1A,01,{0x11}},
{0x1B,01,{0x1B}},
{0x1C,01,{0x12}},
{0x1D,01,{0x21}},
{0x1E,01,{0x12}},
{0x1F,01,{0x28}},
{0x20,01,{0x11}},
{0x21,01,{0x16}},
{0x22,01,{0x12}},
{0x23,01,{0x25}},
{0x24,01,{0x12}},
{0x25,01,{0x24}},
{0x26,01,{0x11}},
{0x27,01,{0x17}},
{0x28,01,{0x11}},
{0x29,01,{0x1E}},
//****************GAMMA*****************************
{0xFF,03,{0x78,0x07,0x08}},											
{0xE0,40,{0x00,0x00,0x16,0x3C,0x00,0x5B,0x76,0x8B,0x00,0xA0,0xB1,0xC1,0x14,0xF4,0x1D,0x5B,0x25,0x87,0xD0,0x0F,0x2A,0x10,0x43,0x85,0x3A,0xAF,0xE7,0x09,0x3F,0x3C,0x48,0x55,0x3F,0x68,0x7B,0x91,0x3F,0xAF,0xD3,0xD9}},									
{0xE1,40,{0x00,0x00,0x16,0x3C,0x00,0x5B,0x76,0x8B,0x00,0xA0,0xB1,0xC1,0x14,0xF4,0x1D,0x5B,0x25,0x87,0xD0,0x0F,0x2A,0x10,0x43,0x85,0x3A,0xAF,0xE7,0x09,0x3F,0x3C,0x48,0x55,0x3F,0x68,0x7B,0x91,0x3F,0xAF,0xD3,0xD9}},

//************Auto trim********************
//OSC auto trim
{0xFF,03,{0x78,0x07,0x0B}},
{0x94,01,{0x88}},
{0x95,01,{0x22}},
{0x96,01,{0x06}},         //ub/lb 0.25%
{0x97,01,{0x06}},
{0x98,01,{0xCB}},         //keep trim code range max/min 10%
{0x99,01,{0xCB}},
{0x9A,01,{0x06}},
{0x9B,01,{0xCD}},
{0x9C,01,{0x05}},         //ub/lb 0.25%
{0x9D,01,{0x05}},
{0x9E,01,{0xAA}},	  //keep trim code range max/min 10%
{0x9F,01,{0xAA}},
{0xAB,01,{0xF0}},
//{0xFF,03,{0x78,0x07,0x00}},	//Page0
{0xFF,03,{0x78,0x07,0x00}},	//Page0
{0x36,01,{0x02}},
{0x11, 1,{0x00}},
{REGFLAG_DELAY, 120, {0}},
{0x29, 1,{0x00}},
{REGFLAG_DELAY, 20, {0}},
{0x35,1,{0x00}},
{REGFLAG_END_OF_TABLE, 0x00, {}}
};

static void push_table(struct LCM_setting_table *table, unsigned int count, unsigned char force_update)
{
    unsigned int i;

    for (i = 0; i < count; i++)
    {
        unsigned cmd;
        cmd = table[i].cmd;

        switch (cmd)
        {
        case REGFLAG_DELAY:
            if (table[i].count <= 10)
                MDELAY(table[i].count);
            else
                MDELAY(table[i].count);
            break;

        case REGFLAG_END_OF_TABLE:
            break;

        default:
            dsi_set_cmdq_V2(cmd, table[i].count, table[i].para_list, force_update);
        }
    }
}

/* --------------------------------------------------------------------------- */
/* LCM Driver Implementations */
/* --------------------------------------------------------------------------- */

static void lcm_set_util_funcs(const struct LCM_UTIL_FUNCS *util)
{
	memcpy(&lcm_util, util, sizeof(struct LCM_UTIL_FUNCS));
}

static void lcm_get_params(struct LCM_PARAMS *params)
{
	memset(params, 0, sizeof(struct LCM_PARAMS));

	params->type                         = LCM_TYPE_DSI;
	params->width                        = FRAME_WIDTH;
	params->height                       = FRAME_HEIGHT;

	#ifndef BUILD_LK
	params->physical_width               = 68;
	params->physical_height              = 151;
	params->physical_width_um            = 67930;
	params->physical_height_um           = 150960;
	params->density                      = 320;
	#endif

	// enable tearing-free
	params->dbi.te_mode                  = LCM_DBI_TE_MODE_VSYNC_ONLY;
	params->dbi.te_edge_polarity         = LCM_POLARITY_RISING;

	#if (LCM_DSI_CMD_MODE)
	params->dsi.mode                     = CMD_MODE;
	params->dsi.switch_mode              = SYNC_PULSE_VDO_MODE;
	#else
	params->dsi.mode                     = SYNC_PULSE_VDO_MODE;//SYNC_EVENT_VDO_MODE;//BURST_VDO_MODE;////
	#endif

	// DSI
	/* Command mode setting */
	//1 Three lane or Four lane
	params->dsi.LANE_NUM                 = LCM_FOUR_LANE;
	//The following defined the fomat for data coming from LCD engine.
	params->dsi.data_format.color_order  = LCM_COLOR_ORDER_RGB;
	params->dsi.data_format.trans_seq    = LCM_DSI_TRANS_SEQ_MSB_FIRST;
	params->dsi.data_format.padding      = LCM_DSI_PADDING_ON_LSB;
	params->dsi.data_format.format       = LCM_DSI_FORMAT_RGB888;

	params->dsi.PS                       = LCM_PACKED_PS_24BIT_RGB888;

	#if (LCM_DSI_CMD_MODE)
	params->dsi.intermediat_buffer_num   = 0;//because DSI/DPI HW design change, this parameters should be 0 when video mode in MT658X; or memory leakage
	params->dsi.word_count               = FRAME_WIDTH * 3; //DSI CMD mode need set these two bellow params, different to 6577
	#else
	params->dsi.intermediat_buffer_num   = 2;	//because DSI/DPI HW design change, this parameters should be 0 when video mode in MT658X; or memory leakage
	#endif

	// Video mode setting
	params->dsi.packet_size              = 256;

	params->dsi.vertical_sync_active = 10;//
	params->dsi.vertical_backporch = 60;//
	params->dsi.vertical_frontporch = 36;//
	params->dsi.vertical_active_line     = FRAME_HEIGHT;

	params->dsi.horizontal_sync_active = 16;
	params->dsi.horizontal_backporch =130;
	params->dsi.horizontal_frontporch = 50;
	params->dsi.horizontal_active_pixel  = FRAME_WIDTH;
	/* params->dsi.ssc_disable = 1; */

	params->dsi.PLL_CLOCK                = 620;
	params->dsi.ssc_disable = 1;
	params->dsi.esd_check_enable = 1;
	params->dsi.customization_esd_check_enable = 1;
	params->dsi.lcm_esd_check_table[0].cmd          = 0x0a;
	params->dsi.lcm_esd_check_table[0].count        = 1;
	params->dsi.lcm_esd_check_table[0].para_list[0] = 0x9c;

}
static void lcm_set_bias(int enable)
{
	
	if (enable){
		display_bias_vpos_set(5800);
		display_bias_vneg_set(5800);
		display_bias_enable();
	}else{
		display_bias_disable();
		
	}
}

static unsigned int lcm_compare_id(void)
{
   return 1; 
}

#ifndef BUILD_LK
extern atomic_t ESDCheck_byCPU;
#endif
static unsigned int lcm_ata_check(unsigned char *bufferr)
{
	//prize-Solve ATA testing-pengzhipeng-20181127-start
	unsigned char buffer0[2]={0};
	unsigned char buffer1[2]={0};
	unsigned char buffer2[2]={0};
	
	unsigned int data_array[6]; 
	 
	data_array[0]= 0x00023902;//LS packet 
	data_array[1]= 0x00005002; 
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0]= 0x0002390a;//HS packet 
	data_array[1]= 0x0000fe27; 
	dsi_set_cmdq(data_array, 2, 1);
	

	data_array[0]= 0x0002390a;//HS packet 
	data_array[1]= 0x00005658; 
	dsi_set_cmdq(data_array, 2, 1);
	data_array[0] = 0x00033700;// read id return two byte,version and id
	dsi_set_cmdq(data_array, 1, 1);
	atomic_set(&ESDCheck_byCPU, 1);
	
	read_reg_v2(0x02, buffer0, 1);
	read_reg_v2(0x27, buffer1, 1);
	read_reg_v2(0x58, buffer2, 1);
	atomic_set(&ESDCheck_byCPU, 0);
	
	printk("%s, Kernel TDDI id = 0x%04x buffer1 0x52= 0x%04x buffer2= 0x%04x\n", __func__, buffer0[0], buffer1[0], buffer2[0]);
	return ((0x50 == buffer0[0])&&(0xfe == buffer1[0])&&(0x56 == buffer2[0]))?1:0; 
	//prize-Solve ATA testing-pengzhipeng-20181127-end
}

static void lcm_init(void)
{
     SET_RESET_PIN(0);
	lcm_set_bias(1);
	MDELAY(20);
	SET_RESET_PIN(1);
	MDELAY(10);
	SET_RESET_PIN(0);
	MDELAY(10);  
	SET_RESET_PIN(1);

    MDELAY(120);

	
	push_table(lcm_initialization_setting, sizeof(lcm_initialization_setting)/sizeof(struct LCM_setting_table), 1);
}
static void lcm_suspend(void)
{
  push_table(lcm_suspend_setting, sizeof(lcm_suspend_setting)/sizeof(struct LCM_setting_table), 1);
	MDELAY(150);//100
	lcm_set_bias(0);
	//ilitek_set_rst_pin(0);
	SET_RESET_PIN(0);	//prize-wyq 20190315 keep reset pin high to fix suspend current leakage
	
	//display_bias_vpos_enable(0);
	MDELAY(10);//100
    display_ldo18_enable(0);
    MDELAY(2);
}
static void lcm_resume(void)
{
	lcm_init();
}

struct LCM_DRIVER ili7807q_fhdp_dsi_vdo_incell_lcm_drv =
{
    .name			= "ili7807q_fhdp_dsi_vdo_incell",
#if defined(CONFIG_PRIZE_HARDWARE_INFO) && !defined (BUILD_LK)
	.lcm_info = {
		.chip	= "ili7807q",
		.vendor	= "focaltech",
		.id		= "0x82",
		.more	= "1080*2244",
	},
#endif
    .set_util_funcs = lcm_set_util_funcs,
    .get_params     = lcm_get_params,
    .init           = lcm_init,
    .suspend        = lcm_suspend,
    .resume         = lcm_resume,
    .compare_id     = lcm_compare_id,
	.ata_check 		= lcm_ata_check,
};
