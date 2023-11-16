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
/*#include <mach/mt_pm_ldo.h>*/
#ifdef CONFIG_MTK_LEGACY
#include <mach/mt_gpio.h>
#endif
#endif

#ifdef BUILD_LK
#define LCM_LOGI(fmt, args...)  printk(KERN_INFO  " LCM file=%s: %s: line=%d: "fmt"\n", __FILE__,__func__,  __LINE__,##args)
#define LCM_LOGD(fmt, args...)  printk(KERN_DEBUG " LCM file=%s: %s: line=%d: "fmt"\n", __FILE__,__func__,  __LINE__,##args)
#else
#define LCM_LOGI(fmt, args...)  printk(KERN_INFO " LCM :"fmt"\n", ##args)
#define LCM_LOGD(fmt, args...)  printk(KERN_DEBUG " LCM :"fmt"\n", ##args)
#endif

#define I2C_I2C_LCD_BIAS_CHANNEL 0
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

static const unsigned char LCD_MODULE_ID = 0x01;
/* --------------------------------------------------------------------------- */
/* Local Constants */
/* --------------------------------------------------------------------------- */
#define LCM_DSI_CMD_MODE	0
#define FRAME_WIDTH  										1080
#define FRAME_HEIGHT 										2340
//prize-penggy modify LCD size-20190328-start
#define LCM_PHYSICAL_WIDTH                  				(69500)
#define LCM_PHYSICAL_HEIGHT                  				(150580)
//prize-penggy modify LCD size-20190328-end

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

/* --------------------------------------------------------------------------- */
/* Local Variables */
/* --------------------------------------------------------------------------- */

struct LCM_setting_table {
	unsigned int cmd;
	unsigned char count;
	unsigned char para_list[64];
};

static struct LCM_setting_table lcm_initialization_setting[] = {
	//CMD2 ENABLE
	{0x00, 1, {0x00}},
	{0xFF, 3, {0x87,0x56,0x01}},
	{0x00, 1, {0x80}},
	{0xFF, 2, {0x87,0x56}},
	
	//Panel resulotion 1080x2340
	{0x00, 1, {0xA1}},
	{0xB3, 6, {0x04,0x38,0x09,0x24,0x00,0xFC}},   
		
	//TCON Setting
	{0x00, 1, {0x80}},
	{0xC0, 6, {0x00 ,0x8D,0x00,0x05,0x00,0x1E}},
	{0x00, 1, {0x90}},
	{0xC0, 6, {0x00 ,0x8D,0x00,0x05,0x00,0x1E}},
	{0x00, 1, {0xA0}},
	{0xC0, 6, {0x01 ,0x1C,0x00,0x05,0x00,0x1E}},
	{0x00, 1, {0xB0}},
	{0xC0, 5, {0x00 ,0x8D,0x00,0x05,0x1E}},
	{0x00, 1, {0xC1}},
	{0xC0, 8, {0x00 ,0xD2,0x00,0xA3,0x00 ,0x8C ,0x00 ,0xF4}},
	{0x00, 1, {0xD7}},
	{0xC0, 6, {0x00 ,0x8C,0x00,0x05,0x00 ,0x1E}},
	{0x00, 1, {0xA3}},
	{0xC1, 6, {0x00, 0x2A, 0x00 ,0x2A ,0x00 ,0x02}},
	{0x00, 1, {0x80}},
	{0xCE, 16,{0x01, 0x81 ,0x09 ,0x13 ,0x00 ,0xD8 ,0x00 ,0xD8 ,0x00 ,0x90 ,0x00 ,0x90 ,0x00 ,0x74 ,0x00,0x74}},
	{0x00, 1, {0x90}},
	{0xCE, 15,{0x00 ,0x8E ,0x0E ,0xB6 ,0x00 ,0x8E ,0x80 ,0x09 ,0x13 ,0x00 ,0x04 ,0x00 ,0x19 ,0x19 ,0x11}},
	{0x00, 1, {0xA0}},
	{0xCE, 3, {0x00 ,0x00 ,0x00}},
	{0x00, 1, {0xB0}},
	{0xCE, 3, {0x22 ,0x00 ,0x00}},
	{0x00, 1, {0xD1}},
	{0xCE, 7, {0x00 ,0x00 ,0x01 ,0x00 ,0x00 ,0x00 ,0x00}},
	{0x00, 1, {0xE1}},
	{0xCE, 11,{0x08 ,0x02 ,0x4D ,0x02 ,0x4D ,0x02 ,0x4D ,0x00 ,0x00 ,0x00 ,0x00}},
	{0x00, 1, {0xF1}},
	{0xCE, 9, {0x15 ,0x0A ,0x0E ,0x01 ,0x35 ,0x01 ,0xDA ,0x01 ,0x50}},
	{0x00, 1, {0xB0}},
	{0xCF, 4, {0x00 ,0x00 ,0x6C ,0x70}},
	{0x00, 1, {0xB5}},
	{0xCF, 4, {0x04 ,0x04 ,0xA4 ,0xA8}},
	{0x00, 1, {0xC0}},
	{0xCF, 4, {0x08 ,0x08 ,0xCA ,0xCE}},
	{0x00, 1, {0xC5}},
	{0xCF, 4, {0x00 ,0x00 ,0x08 ,0x0C}},
	
	//Panel scan mode
	{0x00, 1, {0xE8}},
	{0xC0, 1, {0x40}},
	
	//VST1&VST2
	{0x00, 1, {0x80}},
	{0xc2, 8, {0x84,0x00,0x00,0x8D,0x83,0x00,0x00,0x8D}},
	
	//CKV1-3
	{0x00, 1, {0xa0}},
	{0xc2, 15,{0x82,0x04,0x00,0x00,0x8C,0x81,0x04,0x00,0x00,0x8C,0x00,0x04,0x00,0x00,0x8C}}, 
			   
	//CKV4-6
	{0x00, 1, {0xb0}},
	{0xc2, 15,{0x01,0x04,0x00,0x00,0x8C,0x02,0x04,0x00,0x00,0x8C,0x03,0x04,0x00,0x00,0x8C}},
	
	//CKV7-8
	{0x00, 1, {0xc0}},
	{0xc2, 10,{0x04,0x04,0x00,0x00,0x8C,0x05,0x04,0x00,0x00,0x8C}},
	
	//CKV Width
	{0x00, 1, {0xe0}},
	{0xc2, 4, {0x77,0x77,0x77,0x77}},
	
	{0x00, 1, {0xc0}},
	{0xc3, 4, {0x99,0x99,0x99,0x99}},
	
	//Panel Interface Setting
	{0x00, 1, {0x80}},
	{0xcb, 16,{0x00,0xC5,0x00,0x00,0x05,0x05,0x00,0x05,0x06,0x05,0xc5,0x00,0x05,0x05,0x00,0xc4}},
	{0x00, 1, {0x90}},
	{0xcb, 16,{0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}},
	{0x00, 1, {0xa0}},
	{0xcb, 4, {0x00,0x00,0x00,0x00}},
	{0x00, 1, {0xB0}},
	{0xCB, 4, {0x10,0x51,0x94,0x50}},
	{0x00, 1, {0xC0}},
	{0xCB, 4, {0x10,0x51,0x94,0x50}},
	
	//CGOUT
	{0x00, 1, {0x80}},
	{0xcc, 16,{0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x07,0x09,0x0B,0x0D,0x25,0x25,0x03,0x16,0x17}},
	{0x00, 1, {0x90}},
	{0xcc, 8, {0x18,0x22,0x26,0x29,0x19,0x1A,0x1B,0x24}},
	{0x00, 1, {0x80}},
	{0xcd, 16,{0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x06,0x08,0x0A,0x0C,0x25,0x00,0x02,0x16,0x17}},
	{0x00, 1, {0x90}},
	{0xcd, 8, {0x18,0x22,0x26,0x29,0x19,0x1A,0x1B,0x24}},
	{0x00, 1, {0xa0}},
	{0xcc, 16,{0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x0C,0x0A,0x08,0x06,0x25,0x25,0x02,0x16,0x17}},
	{0x00, 1, {0xb0}},
	{0xcc, 8, {0x18,0x26,0x23,0x29,0x19,0x1A,0x1B,0x24}},
	{0x00, 1, {0xa0}},
	{0xcd, 16,{0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x0D,0x0B,0x09,0x07,0x25,0x00,0x03,0x16,0x17}},
	{0x00, 1, {0xb0}},
	{0xcd, 8, {0x18,0x26,0x23,0x29,0x19,0x1A,0x1B,0x24}},
		
	//Rotate[4:0]
	{0x00, 1, {0x80}},
	{0xa7, 1, {0x13}},
	{0x00, 1, {0xA0}},
	{0xC3, 16,{0x35,0x02,0x41,0x35,0x53,0x14,0x20,0x00,0x00,0x00,0x13,0x50,0x24,0x42,0x05,0x31}},
	
	//CKH Dummy
	{0x00, 1, {0x82}},
	{0xa7, 2, {0x22,0x02}},
	
	//Reduce Source ring for COB
	{0x00, 1, {0x85}},
	{0xC4, 1, {0x1C}},
	{0x00, 1, {0x97}},
	{0xC4, 1, {0x01}},
	
	//Source remapping
	{0x00, 1, {0xA0}},
	{0xC4, 3, {0x8D,0xD8,0x8D}},
	
	//VGH N_pump x2(VGH=11V)
	{0x00, 1, {0x93}},
	{0xC5, 1, {0x37}},	//c593{7}=1  3±¶Ñ¹	0£º2±¶Ñ¹ 20190108
	
	//VGH I_pump x2 (VGH=11V)  
	{0x00, 1, {0x97}},
	{0xC5, 1, {0x37}}, //c597{7}=1	3±¶Ñ¹  0£º2±¶Ñ¹ 20190108
	
	//VGHO1/9V
	{0x00, 1, {0xB6}},
	{0xC5, 2, {0x23,0x23}},
	{0x00, 1, {0x93}},
	{0xF5, 2, {0x00,0x00}},
	{0x00, 1, {0x9D}},
	{0xF5, 1, {0x49}},
	{0x00, 1, {0x82}},
	{0xF5, 2, {0x00,0x00}},
	{0x00, 1, {0x8C}},
	{0xC3, 3, {0x00,0x00,0x00}},
	{0x00, 1, {0x84}},
	{0xC5, 2, {0x28,0x28}},
	
	//Gamma 32
	{0x00, 1, {0xA4}},
	{0xD7, 1, {0x00}},
	
	//ahung:hvreg_en_vglo1s,hvreg_en_vglo2s
	{0x00, 1, {0x80}},
	{0xF5, 2, {0x59,0x59}},
	
	//hvreg_en_vgho1s,hvreg_en_vgho2s,pump_en_vgh_s
	{0x00, 1, {0x84}},
	{0xF5, 3, {0x59,0x59,0x59}},
	
	//pump_en_vgl_s
	{0x00, 1, {0x96}},
	{0xF5, 1, {0x59}},
	
	//vclreg_en_s
	{0x00, 1, {0xA6}},
	{0xF5, 1, {0x59}},
	
	//pwron 3->5
	{0x00, 1, {0xCA}},
	{0xC0, 1, {0x80}},
	
	//ahung:vcom_en_vcom
	{0x00, 1, {0xB1}},
	{0xF5, 1, {0x1f}},
	
	//GVDDP/N=+/-5V 
	{0x00, 1, {0x00}},
	{0xD8, 2, {0x2B,0x2B}},
	
	//VCOM/-0.15V
	//{0x00, 1, {0x00}},
	//{0xD9, 2, {0x20,0x20}},
	
	//MUX CKH
	{0x00, 1, {0x86}},//MUX CKH
	{0xC0, 6, {0x01,0x01,0x01,0x01,0x11,0x03}},
	{0x00, 1, {0x96}},
	{0xC0, 6, {0x01,0x01,0x01,0x01,0x12,0x03}},
	{0x00, 1, {0xA6}},
	{0xC0, 6, {0x01,0x01,0x01,0x06,0x17,0x03}},
	{0x00, 1, {0xE9}},
	{0xC0, 6, {0x01,0x01,0x01,0x01,0x11,0x03}},
	{0x00, 1, {0xA3}},
	{0xCE, 6, {0x01,0x01,0x01,0x01,0x11,0x03}},
	{0x00, 1, {0xb3}},
	{0xCE, 6, {0x01,0x01,0x01,0x01,0x11,0x03}},
	
	{0x00, 1, {0xCC}},
	{0xC0, 1, {0x13}},
	{0x00, 1, {0x82}},
	{0xC5, 1, {0x3a}},
	{0x00, 1, {0x00}},
	{0xFF, 3, {0x00,0x00,0x00}},
	
	{0x35, 1, {0x00}},

	{0x11, 0, {}},
	{REGFLAG_DELAY, 120,{}},

	{0x29, 0, {}},
	{REGFLAG_DELAY, 10, {}},

	{REGFLAG_END_OF_TABLE, 0x00, {}}              
};

static struct LCM_setting_table lcm_suspend_setting[] = {
	{0x28, 0, {}},
	{REGFLAG_DELAY, 30, {} },
	{0x10, 0, {}},
	{REGFLAG_DELAY, 120, {} },
	{0x00, 1, {0x00}},
	{0xF7, 4, {0x5A,0xA5,0x95,0x27}},

	{REGFLAG_END_OF_TABLE, 0x00, {}}  
};

static void push_table(struct LCM_setting_table *table, unsigned int count,
		       unsigned char force_update)
{
	unsigned int i;
    LCM_LOGI("nt35695----tps6132-lcm_init   push_table++++++++++++++===============================devin----\n");
	for (i = 0; i < count; i++) {
		unsigned cmd;

		cmd = table[i].cmd;

		switch (cmd) {

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
	
	params->type = LCM_TYPE_DSI;
	
	params->width = FRAME_WIDTH;
	params->height = FRAME_HEIGHT;
	
	// enable tearing-free
	params->dbi.te_mode 				= LCM_DBI_TE_MODE_VSYNC_ONLY;
	params->dbi.te_edge_polarity		= LCM_POLARITY_RISING;
	#if (LCM_DSI_CMD_MODE)
	params->dsi.mode   = CMD_MODE;
	params->dsi.switch_mode = SYNC_PULSE_VDO_MODE;
	#else
	params->dsi.mode   = SYNC_PULSE_VDO_MODE;//SYNC_EVENT_VDO_MODE;//BURST_VDO_MODE;////
	#endif
		
	// DSI
	/* Command mode setting */
	//1 Three lane or Four lane
	params->dsi.LANE_NUM				= LCM_FOUR_LANE;
	//The following defined the fomat for data coming from LCD engine.
	params->dsi.data_format.color_order = LCM_COLOR_ORDER_RGB;
	params->dsi.data_format.trans_seq	= LCM_DSI_TRANS_SEQ_MSB_FIRST;
	params->dsi.data_format.padding 	= LCM_DSI_PADDING_ON_LSB;
	params->dsi.data_format.format		= LCM_DSI_FORMAT_RGB888;
		
		
	params->dsi.PS=LCM_PACKED_PS_24BIT_RGB888;
		
	#if (LCM_DSI_CMD_MODE)
	params->dsi.intermediat_buffer_num = 0;//because DSI/DPI HW design change, this parameters should be 0 when video mode in MT658X; or memory leakage
	params->dsi.word_count=FRAME_WIDTH*3;	//DSI CMD mode need set these two bellow params, different to 6577
	#else
	params->dsi.intermediat_buffer_num = 0; //because DSI/DPI HW design change, this parameters should be 0 when video mode in MT658X; or memory leakage
	#endif
	
	// Video mode setting
	params->dsi.packet_size=256;
	
	params->dsi.vertical_sync_active				= 10;
	params->dsi.vertical_backporch					= 21;//16 25 30 35 12 8
	params->dsi.vertical_frontporch					= 10;
	params->dsi.vertical_active_line				= FRAME_HEIGHT; 

	params->dsi.horizontal_sync_active = 6;
	params->dsi.horizontal_backporch = 16;//32
	params->dsi.horizontal_frontporch = 32;//78
	params->dsi.horizontal_active_pixel = FRAME_WIDTH;
	/* params->dsi.ssc_disable                                                       = 1; */

	params->dsi.PLL_CLOCK = 499;//244;
	params->dsi.ssc_disable = 0;
	params->dsi.ssc_range = 1;
	params->dsi.cont_clock = 0;
	params->dsi.clk_lp_per_line_enable = 0;
	//prize-penggy modify LCD size-20190328-start
	params->physical_width = LCM_PHYSICAL_WIDTH/1000;
	params->physical_height = LCM_PHYSICAL_HEIGHT/1000;
	//prize-penggy modify LCD size-20190328-end
	params->physical_width_um = LCM_PHYSICAL_WIDTH;
	params->physical_height_um = LCM_PHYSICAL_HEIGHT;
	/*prize penggy add-for LCD ESD-20190219-start*/
	#if 1
	params->dsi.ssc_disable = 1;
	params->dsi.lcm_ext_te_monitor = FALSE;
		
	params->dsi.esd_check_enable = 1;
	params->dsi.customization_esd_check_enable = 1;
	params->dsi.lcm_esd_check_table[0].cmd			= 0x0a;
	params->dsi.lcm_esd_check_table[0].count		= 1;
	params->dsi.lcm_esd_check_table[0].para_list[0] = 0x9c;
	#endif
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
	unsigned char buffer1[2]={0};
	unsigned char buffer2[2]={0};
	
	unsigned int data_array[6]; 
	 
	data_array[0]= 0x00023902;//LS packet 
	data_array[1]= 0x000050b8; 
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00013700;// read id return two byte,version and id
	dsi_set_cmdq(data_array, 1, 1);
	atomic_set(&ESDCheck_byCPU, 1);
	
	read_reg_v2(0xb8, buffer1, 1);
	atomic_set(&ESDCheck_byCPU, 0);
	
	
	data_array[0]= 0x0002390a;//HS packet 
	data_array[1]= 0x000031b8; 
	dsi_set_cmdq(data_array, 2, 1);
	
	data_array[0] = 0x00013700;// read id return two byte,version and id
	dsi_set_cmdq(data_array, 1, 1);
	atomic_set(&ESDCheck_byCPU, 1);
	
	read_reg_v2(0xb8, buffer2, 1);
	atomic_set(&ESDCheck_byCPU, 0);
	
	
	LCM_LOGI("%s, Kernel TDDI id buffer1= 0x%04x buffer2= 0x%04x\n", __func__, buffer1[0],buffer2[0]);
	return ((0x50 == buffer1[0])&&(0x31 == buffer2[0]))?1:0; 
	//prize-Solve ATA testing-pengzhipeng-20181127-end
}

static void lcm_init(void)
{
	display_bias_enable();
	MDELAY(10);

	SET_RESET_PIN(1);
	MDELAY(10);
	SET_RESET_PIN(0);
	MDELAY(10);  

	SET_RESET_PIN(1);
	MDELAY(10);//250
	
	push_table(lcm_initialization_setting, sizeof(lcm_initialization_setting)/sizeof(struct LCM_setting_table), 1);
}

static void lcm_suspend(void)
{
    push_table(lcm_suspend_setting, sizeof(lcm_suspend_setting)/sizeof(struct LCM_setting_table), 1);
	     
	display_bias_disable();
	MDELAY(10);//100
	
	//SET_RESET_PIN(0);	//prize-wyq 20190315 keep reset pin high to fix suspend current leakage
	//MDELAY(10);//100
}

static void lcm_resume(void)
{
	lcm_init();
}

static void lcm_init_power(void)
{
	display_bias_enable();
}

static void lcm_suspend_power(void)
{
	display_bias_disable();
}

static void lcm_resume_power(void)
{
	SET_RESET_PIN(0); //prize-wyq 20190315 keep reset pin high to fix suspend current leakage
	display_bias_enable();
}

struct LCM_DRIVER ft8756_fhdp_dsi_vdo_tcl_drv = 
{
    .name			= "ft8756_fhdp_dsi_vdo_tcl",
	#if defined(CONFIG_PRIZE_HARDWARE_INFO) && !defined (BUILD_LK)
	.lcm_info = {
		.chip	= "ft8756",
		.vendor	= "focaltech",
		.id		= "0x80",
		.more	= "1080*2340",
	},
	#endif
	.set_util_funcs = lcm_set_util_funcs,
	.get_params     = lcm_get_params,
	.init           = lcm_init,
	.suspend        = lcm_suspend,
	.init_power = lcm_init_power,
	.resume_power = lcm_resume_power,
	.suspend_power = lcm_suspend_power,
	.resume         = lcm_resume,
	.compare_id     = lcm_compare_id,
	.ata_check 		= lcm_ata_check,
};
