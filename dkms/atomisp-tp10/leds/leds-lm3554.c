// SPDX-License-Identifier: GPL-2.0-only
/*
 * Texas Instruments LM3554 flash LED driver
 *
 * The LM3554 drives a flash LED through two current sources (LED1, LED2),
 * which are used together, and an indicator LED (LEDI). It is used e.g. for
 * the rear camera flash of the Lenovo ThinkPad 10 (20C1/20C3), where the
 * firmware describes it as ACPI device INTCF1C.
 *
 * Copyright (C) 2026 Volodymyr Chapman <volodymyrchapman@gmail.com>
 */

#include <linux/acpi.h>
#include <linux/bits.h>
#include <linux/bitfield.h>
#include <linux/delay.h>
#include <linux/gpio/consumer.h>
#include <linux/i2c.h>
#include <linux/led-class-flash.h>
#include <linux/leds.h>
#include <linux/mod_devicetable.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/regmap.h>

#define LM3554_REG_TORCH		0xa0
#define   LM3554_TORCH_CURRENT		GENMASK(5, 3)
#define LM3554_REG_FLASH		0xb0
#define   LM3554_FLASH_CURRENT		GENMASK(6, 3)
/* Bits [2:0] of the torch and flash registers are shared */
#define   LM3554_MODE			GENMASK(2, 0)
#define   LM3554_MODE_SHUTDOWN		0
#define   LM3554_MODE_TORCH		2
#define   LM3554_MODE_FLASH		3
#define LM3554_REG_DURATION		0xc0
#define   LM3554_TIMEOUT		GENMASK(4, 0)
#define LM3554_REG_FLAGS		0xd0
#define   LM3554_FLAG_TIMEOUT		BIT(0)
#define   LM3554_FLAG_THERMAL_SHUTDOWN	BIT(1)
#define   LM3554_FLAG_LED_FAULT		BIT(2)
#define   LM3554_FLAG_LED_THERMAL	BIT(5)
#define   LM3554_FLAG_VIN_MONITOR	BIT(7)
#define LM3554_REG_CONFIG1		0xe0
#define LM3554_REG_CONFIG2		0xf0

#define LM3554_TIMEOUT_STEP_US		32000
#define LM3554_TIMEOUT_MIN_US		32000
/* Power-on default, the chip allows up to 1024 ms */
#define LM3554_TIMEOUT_MAX_US		512000

/*
 * 8 torch current settings, LED1 + LED2 together: 34, 71, 108, 146, 180, 218,
 * 256 and 295 mA.
 */
#define LM3554_TORCH_LEVELS		8

/* Total of LED1 + LED2, for the 16 flash current settings */
static const u32 lm3554_flash_ua[] = {
	71000, 146000, 218000, 295000, 365000, 441000, 518000, 596000,
	652000, 729000, 805000, 881000, 960000, 1037000, 1113000, 1191000,
};

/* Power-on default (13), the chip allows up to 1191 mA */
#define LM3554_FLASH_MAX_UA		1037000

struct lm3554 {
	struct led_classdev_flash fled_cdev;
	struct regmap *regmap;
	struct gpio_desc *enable_gpio;
	/* Serialises register access between the LED and flash callbacks */
	struct mutex lock;
};

static struct lm3554 *fled_cdev_to_lm3554(struct led_classdev_flash *fled_cdev)
{
	return container_of(fled_cdev, struct lm3554, fled_cdev);
}

static bool lm3554_volatile_reg(struct device *dev, unsigned int reg)
{
	/* The mode bits clear themselves when a flash times out */
	return reg == LM3554_REG_TORCH || reg == LM3554_REG_FLASH ||
	       reg == LM3554_REG_FLAGS;
}

static const struct regmap_config lm3554_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
	.max_register = LM3554_REG_CONFIG2,
	.volatile_reg = lm3554_volatile_reg,
};

static int lm3554_set_mode(struct lm3554 *lm3554, unsigned int mode)
{
	return regmap_update_bits(lm3554->regmap, LM3554_REG_TORCH,
				  LM3554_MODE, mode);
}

static int lm3554_brightness_set(struct led_classdev *led_cdev,
				 enum led_brightness brightness)
{
	struct led_classdev_flash *fled_cdev = lcdev_to_flcdev(led_cdev);
	struct lm3554 *lm3554 = fled_cdev_to_lm3554(fled_cdev);
	int ret;

	guard(mutex)(&lm3554->lock);

	if (brightness == LED_OFF)
		return lm3554_set_mode(lm3554, LM3554_MODE_SHUTDOWN);

	ret = regmap_update_bits(lm3554->regmap, LM3554_REG_TORCH,
				 LM3554_TORCH_CURRENT,
				 FIELD_PREP(LM3554_TORCH_CURRENT, brightness - 1));
	if (ret)
		return ret;

	return lm3554_set_mode(lm3554, LM3554_MODE_TORCH);
}

static int lm3554_flash_brightness_set(struct led_classdev_flash *fled_cdev,
				       u32 brightness)
{
	struct lm3554 *lm3554 = fled_cdev_to_lm3554(fled_cdev);
	unsigned int i;

	/* Use the highest setting which does not exceed the requested current */
	for (i = ARRAY_SIZE(lm3554_flash_ua) - 1; i > 0; i--)
		if (lm3554_flash_ua[i] <= brightness)
			break;

	guard(mutex)(&lm3554->lock);

	return regmap_update_bits(lm3554->regmap, LM3554_REG_FLASH,
				  LM3554_FLASH_CURRENT,
				  FIELD_PREP(LM3554_FLASH_CURRENT, i));
}

static int lm3554_timeout_set(struct led_classdev_flash *fled_cdev, u32 timeout)
{
	struct lm3554 *lm3554 = fled_cdev_to_lm3554(fled_cdev);

	guard(mutex)(&lm3554->lock);

	return regmap_update_bits(lm3554->regmap, LM3554_REG_DURATION,
				  LM3554_TIMEOUT,
				  FIELD_PREP(LM3554_TIMEOUT,
					     timeout / LM3554_TIMEOUT_STEP_US - 1));
}

static int lm3554_strobe_set(struct led_classdev_flash *fled_cdev, bool state)
{
	struct lm3554 *lm3554 = fled_cdev_to_lm3554(fled_cdev);
	unsigned int flags;
	int ret;

	guard(mutex)(&lm3554->lock);

	if (!state)
		return lm3554_set_mode(lm3554, LM3554_MODE_SHUTDOWN);

	/* Clear old flags, the timeout flag would otherwise stay set */
	ret = regmap_read(lm3554->regmap, LM3554_REG_FLAGS, &flags);
	if (ret)
		return ret;

	return lm3554_set_mode(lm3554, LM3554_MODE_FLASH);
}

static int lm3554_strobe_get(struct led_classdev_flash *fled_cdev, bool *state)
{
	struct lm3554 *lm3554 = fled_cdev_to_lm3554(fled_cdev);
	unsigned int val;
	int ret;

	guard(mutex)(&lm3554->lock);

	ret = regmap_read(lm3554->regmap, LM3554_REG_TORCH, &val);
	if (ret)
		return ret;

	*state = FIELD_GET(LM3554_MODE, val) == LM3554_MODE_FLASH;

	return 0;
}

static int lm3554_fault_get(struct led_classdev_flash *fled_cdev, u32 *fault)
{
	struct lm3554 *lm3554 = fled_cdev_to_lm3554(fled_cdev);
	unsigned int flags;
	int ret;

	guard(mutex)(&lm3554->lock);

	/* Reading the flags register clears it */
	ret = regmap_read(lm3554->regmap, LM3554_REG_FLAGS, &flags);
	if (ret)
		return ret;

	*fault = 0;
	if (flags & LM3554_FLAG_TIMEOUT)
		*fault |= LED_FAULT_TIMEOUT;
	if (flags & LM3554_FLAG_THERMAL_SHUTDOWN)
		*fault |= LED_FAULT_OVER_TEMPERATURE;
	if (flags & LM3554_FLAG_LED_FAULT)
		*fault |= LED_FAULT_SHORT_CIRCUIT;
	if (flags & LM3554_FLAG_LED_THERMAL)
		*fault |= LED_FAULT_LED_OVER_TEMPERATURE;
	if (flags & LM3554_FLAG_VIN_MONITOR)
		*fault |= LED_FAULT_INPUT_VOLTAGE;

	return 0;
}

static const struct led_flash_ops lm3554_flash_ops = {
	.flash_brightness_set	= lm3554_flash_brightness_set,
	.strobe_set		= lm3554_strobe_set,
	.strobe_get		= lm3554_strobe_get,
	.timeout_set		= lm3554_timeout_set,
	.fault_get		= lm3554_fault_get,
};

/*
 * The ACPI resources list the STROBE, TORCH and HWEN (enable) GPIOs in this
 * order, without names.
 */
static const struct acpi_gpio_params lm3554_strobe_gpio = { 0, 0, false };
static const struct acpi_gpio_params lm3554_torch_gpio = { 1, 0, false };
static const struct acpi_gpio_params lm3554_enable_gpio = { 2, 0, false };

static const struct acpi_gpio_mapping lm3554_acpi_gpios[] = {
	{ "strobe-gpios", &lm3554_strobe_gpio, 1 },
	{ "torch-gpios", &lm3554_torch_gpio, 1 },
	{ "enable-gpios", &lm3554_enable_gpio, 1 },
	{ }
};

static void lm3554_power_off(void *data)
{
	struct lm3554 *lm3554 = data;

	lm3554_set_mode(lm3554, LM3554_MODE_SHUTDOWN);
	gpiod_set_value_cansleep(lm3554->enable_gpio, 0);
}

static int lm3554_probe(struct i2c_client *client)
{
	struct device *dev = &client->dev;
	struct led_flash_setting *setting;
	struct led_classdev *led_cdev;
	struct gpio_desc *gpio;
	struct lm3554 *lm3554;
	unsigned int val;
	int ret;

	lm3554 = devm_kzalloc(dev, sizeof(*lm3554), GFP_KERNEL);
	if (!lm3554)
		return -ENOMEM;

	ret = devm_mutex_init(dev, &lm3554->lock);
	if (ret)
		return ret;

	ret = devm_acpi_dev_add_driver_gpios(dev, lm3554_acpi_gpios);
	if (ret)
		return ret;

	/*
	 * This driver only controls the LED over I2C. Keep the STROBE and
	 * TORCH inputs low so that they do not turn on the LED.
	 */
	gpio = devm_gpiod_get_optional(dev, "strobe", GPIOD_OUT_LOW);
	if (IS_ERR(gpio))
		return dev_err_probe(dev, PTR_ERR(gpio), "getting strobe GPIO\n");

	gpio = devm_gpiod_get_optional(dev, "torch", GPIOD_OUT_LOW);
	if (IS_ERR(gpio))
		return dev_err_probe(dev, PTR_ERR(gpio), "getting torch GPIO\n");

	lm3554->enable_gpio = devm_gpiod_get(dev, "enable", GPIOD_OUT_HIGH);
	if (IS_ERR(lm3554->enable_gpio))
		return dev_err_probe(dev, PTR_ERR(lm3554->enable_gpio),
				     "getting enable GPIO\n");

	lm3554->regmap = devm_regmap_init_i2c(client, &lm3554_regmap_config);
	if (IS_ERR(lm3554->regmap))
		return PTR_ERR(lm3554->regmap);

	ret = devm_add_action_or_reset(dev, lm3554_power_off, lm3554);
	if (ret)
		return ret;

	/* Check that the chip is there, this also clears its flags */
	ret = regmap_read(lm3554->regmap, LM3554_REG_FLAGS, &val);
	if (ret)
		return dev_err_probe(dev, ret, "reading flags register\n");

	lm3554->fled_cdev.ops = &lm3554_flash_ops;

	setting = &lm3554->fled_cdev.timeout;
	setting->min = LM3554_TIMEOUT_MIN_US;
	setting->max = LM3554_TIMEOUT_MAX_US;
	setting->step = LM3554_TIMEOUT_STEP_US;
	setting->val = LM3554_TIMEOUT_MAX_US;

	setting = &lm3554->fled_cdev.brightness;
	setting->min = lm3554_flash_ua[0];
	setting->max = LM3554_FLASH_MAX_UA;
	setting->step = 1;
	setting->val = LM3554_FLASH_MAX_UA;

	led_cdev = &lm3554->fled_cdev.led_cdev;
	led_cdev->name = "white:" LED_FUNCTION_FLASH;
	led_cdev->max_brightness = LM3554_TORCH_LEVELS;
	led_cdev->brightness_set_blocking = lm3554_brightness_set;
	led_cdev->flags |= LED_DEV_CAP_FLASH | LED_CORE_SUSPENDRESUME;

	return devm_led_classdev_flash_register(dev, &lm3554->fled_cdev);
}

static const struct acpi_device_id lm3554_acpi_match[] = {
	{ "INTCF1C" },
	{ }
};
MODULE_DEVICE_TABLE(acpi, lm3554_acpi_match);

static struct i2c_driver lm3554_driver = {
	.driver = {
		.name = "leds-lm3554",
		.acpi_match_table = lm3554_acpi_match,
	},
	.probe = lm3554_probe,
};
module_i2c_driver(lm3554_driver);

MODULE_DESCRIPTION("Texas Instruments LM3554 flash LED driver");
MODULE_AUTHOR("Volodymyr Chapman <volodymyrchapman@gmail.com>");
MODULE_LICENSE("GPL");
