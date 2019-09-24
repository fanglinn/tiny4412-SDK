/*
 *  njpt4412-wm8960.c
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 */

#include "../codecs/wm8960.h"
#include "./i2s.h"
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <sound/pcm.h>


#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/clk/clk-conf.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/spinlock.h>
#include <linux/err.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/device.h>
#include <linux/init.h>
#include <linux/pm_runtime.h>
#include <linux/sched.h>
#include <linux/clkdev.h>
#include <linux/stringify.h>

#define ITOP4412_WM8960 1
#define DEBUG	1

#ifdef	DEBUG
#define	dprintk( argc, argv... )		printk( argc, ##argv )
#else
#define	dprintk( argc, argv... )		
#endif


#define SAMSUNG_I2S_DIV_PRESCALER 2


/*------------2018-11-16 add by zhuchengzhi--------------------*/
#include <linux/platform_device.h>

#include <asm/io.h>
#include <asm/uaccess.h>
#include <asm/div64.h>

#include <asm/mach/map.h>
#include <linux/fb.h>
#include <asm/types.h>


#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/gpio.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/fs.h>
#include <asm/uaccess.h>
#include <linux/uaccess.h>

struct clk *base_clk_iis0;
struct clk *base_clk_iis0_opclk0;
struct clk *base_clk_iis0_opclk1;
struct clk *base_clk_iis0_cdclk0;
static struct resource *res1;
static volatile void __iomem *iis0_regs_base;

/*-------------------------------------------------------------*/


 /*
  * Default CFG switch settings to use this driver:
  *	SMDKV310: CFG5-1000, CFG7-111111
  */

 /*
  * Configure audio route as :-
  * $ amixer sset 'DAC1' on,on
  * $ amixer sset 'Right Headphone Mux' 'DAC'
  * $ amixer sset 'Left Headphone Mux' 'DAC'
  * $ amixer sset 'DAC1R Mixer AIF1.1' on
  * $ amixer sset 'DAC1L Mixer AIF1.1' on
  * $ amixer sset 'IN2L' on
  * $ amixer sset 'IN2L PGA IN2LN' on
  * $ amixer sset 'MIXINL IN2L' on
  * $ amixer sset 'AIF1ADC1L Mixer ADC/DMIC' on
  * $ amixer sset 'IN2R' on
  * $ amixer sset 'IN2R PGA IN2RN' on
  * $ amixer sset 'MIXINR IN2R' on
  * $ amixer sset 'AIF1ADC1R Mixer ADC/DMIC' on
  */

/* SMDK has a 16.934MHZ crystal attached to WM8960 */
//#define SMDK_WM8960_FREQ 16934000
#define SMDK_WM8960_FREQ 112896000

struct smdk_wm8960_data {
	int mclk1_rate;
};

/* Default SMDKs */
static struct smdk_wm8960_data smdk_board_data = {
	.mclk1_rate = SMDK_WM8960_FREQ,
};


#if 0
static int smdk_hw_params(struct snd_pcm_substream *substream,struct snd_pcm_hw_params *params)
{

	printk("%d : %s ..........[2018-11-16]\n",__LINE__,__func__);
	return 0;
}
#else 

//设置cpu_dai（i2s控制器）和codec_dai（wm8960）的频率
static int smdk_hw_params(struct snd_pcm_substream *substream,struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct snd_soc_card *soc_card = rtd->card;
	unsigned int pll_out;
	int ret;
	ret = snd_soc_dai_set_fmt(codec_dai,  SND_SOC_DAIFMT_I2S
										| SND_SOC_DAIFMT_NB_NF
										| SND_SOC_DAIFMT_CBS_CFS);
	if (ret < 0)
		return ret;

	ret = snd_soc_dai_set_fmt(cpu_dai,    SND_SOC_DAIFMT_I2S
										| SND_SOC_DAIFMT_NB_NF
										| SND_SOC_DAIFMT_CBS_CFS);
	if (ret < 0)
		return ret;

	/* set the CPU system clock */
	ret = snd_soc_dai_set_sysclk(cpu_dai, 1, 256, SND_SOC_CLOCK_OUT);
	if (ret < 0){
		printk( "%s: AP I2SCLK RCLK setting error, %d\n", cpu_dai->name, ret );
		return ret;
	}

	ret = snd_soc_dai_set_sysclk(cpu_dai, 2, 256, SND_SOC_CLOCK_OUT);
	if (ret < 0){
		printk( "%s: AP I2SCLK RCLK setting error, %d\n", cpu_dai->name, ret );
		return ret;
	}

	ret = snd_soc_dai_set_clkdiv(codec_dai,1, 16);
	if( ret < 0 ){
		printk( "%s: Codec DACDIV setting error, %d\n", codec_dai->name, ret );
		return ret;
	}

	ret = snd_soc_dai_set_clkdiv(codec_dai, WM8960_DACDIV, 1<<3);
	if( ret < 0 ){
		printk( "%s: Codec DACDIV setting error, %d\n", codec_dai->name, ret );
		return ret;
	}

	return 0;
}
#endif


/*
 * SMDK WM8960 DAI operations.
 */
static struct snd_soc_ops smdk_ops = {
	.hw_params = smdk_hw_params,
};

static const struct snd_soc_dapm_widget smdk4x12_dapm_capture_widgets[] = {
	SND_SOC_DAPM_MIC("Mic Jack",NULL ),
	SND_SOC_DAPM_LINE("Line Input 3 (FM)",NULL ),
};

static const struct snd_soc_dapm_widget smdk4x12_dapm_playback_widgets[] = {
	SND_SOC_DAPM_HP("Headphone Jack", NULL),
	SND_SOC_DAPM_SPK("Speaker_L", NULL),
	SND_SOC_DAPM_SPK("Speaker_R", NULL),
};

static const struct snd_soc_dapm_route smdk4x12_audio_map[] = {
	{"Headphone Jack", NULL, "HP_L"},
	{"Headphone Jack", NULL, "HP_R"},
	{"Speaker_L", NULL, "SPK_LP"},
	{"Speaker_L", NULL, "SPK_LN"},
	{"Speaker_R", NULL, "SPK_RP"},
	{"Speaker_R", NULL, "SPK_RN"},
	{"LINPUT1", NULL, "MICB"},
	{"MICB", NULL, "Mic Jack"},
};


static int smdk_wm8960_init_paiftx(struct snd_soc_pcm_runtime *rtd)
{
	/* Other pins NC */
#if 1
	struct snd_soc_dapm_context *dapm = &rtd->card->dapm;
	int err;  
	//struct snd_soc_codec *codec = rtd->codec;  
	struct clk *fout_epll;
	struct clk *mout_audss;
	fout_epll = __clk_lookup("fout_epll");
	mout_audss = __clk_lookup("mout_audss");
//	clk_set_rate(fout_epll,67737602);
//	clk_set_rate(fout_epll,112896000); 
	clk_set_rate(fout_epll,1411000); 
	clk_set_parent(mout_audss,fout_epll);

	snd_soc_dapm_new_controls( dapm, smdk4x12_dapm_playback_widgets, ARRAY_SIZE( smdk4x12_dapm_playback_widgets ) );
	snd_soc_dapm_add_routes( dapm, smdk4x12_audio_map, ARRAY_SIZE( smdk4x12_audio_map ) );
	snd_soc_dapm_enable_pin(dapm, "Headphone Jack");
	snd_soc_dapm_sync( dapm );
#endif
	return 0;
}

static struct snd_soc_dai_link smdk_dai[] = {

	{ /* Primary DAI i/f */
		.name		= "wm8960",
		.stream_name	= "Playback",
		.codec_dai_name	= "wm8960-hifi",
//		.cpu_dai_name = "samsung-i2s",
//		.platform_name = "samsung-audio",
//		.codec_name = "wm8960",
		.init = smdk_wm8960_init_paiftx,
		.dai_fmt = SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_NB_NF | SND_SOC_DAIFMT_CBM_CFM,
		.ops = &smdk_ops,
	},
};

static struct snd_soc_card smdk = {
	.name = "WM-8960A",
	.owner = THIS_MODULE,
	.dai_link = smdk_dai,
	.num_links = ARRAY_SIZE(smdk_dai),
};

static const struct of_device_id samsung_wm8960_of_match[] = {
	{ .compatible = "tiny4412,wm8960-machine", .data = &smdk_board_data },
	{},
};
MODULE_DEVICE_TABLE(of, samsung_wm8960_of_match);


static int smdk_audio_probe(struct platform_device *pdev)
{
	int ret;
	struct device_node *np = pdev->dev.of_node;
	struct snd_soc_card *card = &smdk;
	struct smdk_wm8960_data *board;
	const struct of_device_id *id;

	card->dev = &pdev->dev;

	board = devm_kzalloc(&pdev->dev, sizeof(*board), GFP_KERNEL);
	if (!board)
		return -ENOMEM;

	if (np) {
		smdk_dai[0].cpu_dai_name = NULL;
		smdk_dai[0].cpu_of_node = of_parse_phandle(np,"samsung,i2s-controller", 0);
		if (!smdk_dai[0].cpu_of_node) {
			dev_err(&pdev->dev,
			   "Property 'samsung,i2s-controller' missing or invalid\n");
			ret = -EINVAL;
		}

		smdk_dai[0].platform_name = NULL;
		smdk_dai[0].platform_of_node = smdk_dai[0].cpu_of_node;
		smdk_dai[0].codec_of_node = of_parse_phandle(np,"samsung,audio-codec",0) ;

		if (!smdk_dai[0].codec_of_node) {
			dev_err(&pdev->dev,
			   "Property 'samsung,audio-codec' missing or invalid\n");
			ret = -EINVAL;
		}


		printk("%d : %s \n",__LINE__,__func__);
	}

	id = of_match_device(of_match_ptr(samsung_wm8960_of_match), &pdev->dev);
	if (id){
		*board = *((struct smdk_wm8960_data *)id->data);
		printk("%d : %s \n",__LINE__,__func__);
	}

	printk("%d : %s \n",__LINE__,__func__);


	platform_set_drvdata(pdev, board);

	ret = devm_snd_soc_register_card(&pdev->dev, card);

	if (ret)
		dev_err(&pdev->dev, "snd_soc_register_card() failed:%d\n", ret);

	return ret;
}

static struct platform_driver smdk_audio_driver = {
	.driver		= {
		.name	= "wm8960",
		.of_match_table = of_match_ptr(samsung_wm8960_of_match),
		.pm	= &snd_soc_pm_ops,
	},
	.probe		= smdk_audio_probe,
};

module_platform_driver(smdk_audio_driver);

MODULE_DESCRIPTION("ALSA SoC SMDK WM8960");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:njpt4412-audio-wm8960");

